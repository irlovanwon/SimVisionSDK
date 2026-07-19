/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: HTTPS admin server (Hybrid: 1 accept + N workers) with per-frame metadata
 * Date: 20260719
 * Modification:
 */
#include "sim_vision/api/AdminServer.h"

#include "sim_vision/common/Logger.h"

#include <nlohmann/json.hpp>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <chrono>
#include <string>

namespace sim_vision {

namespace {
constexpr auto kModule = "AdminServer";
constexpr int kMaxPending = 100;

int64_t unix_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

bool read_until_headers(SSL* ssl, std::string& buf) {
    char tmp[2048];
    while (true) {
        int n = SSL_read(ssl, tmp, sizeof(tmp));
        if (n > 0) {
            buf.append(tmp, n);
            if (buf.find("\r\n\r\n") != std::string::npos) return true;
            if (buf.size() > 64 * 1024) return false;
        } else {
            int err = SSL_get_error(ssl, n);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) continue;
            return false;
        }
    }
}

bool read_body(SSL* ssl, std::string& buf, size_t want) {
    char tmp[4096];
    while (buf.size() < want) {
        int n = SSL_read(ssl, tmp, std::min(sizeof(tmp), want - buf.size()));
        if (n > 0) {
            buf.append(tmp, n);
        } else {
            int err = SSL_get_error(ssl, n);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) continue;
            return false;
        }
    }
    return true;
}

void parse_request_line(const std::string& buf, std::string& method, std::string& path,
                        size_t& header_end, size_t& content_length) {
    content_length = 0;
    auto line_end = buf.find("\r\n");
    std::string line = (line_end == std::string::npos) ? buf : buf.substr(0, line_end);
    auto sp1 = line.find(' ');
    method = sp1 == std::string::npos ? "" : line.substr(0, sp1);
    auto sp2 = line.find(' ', sp1 + 1);
    path = (sp1 == std::string::npos || sp2 == std::string::npos)
               ? "/"
               : line.substr(sp1 + 1, sp2 - sp1 - 1);
    auto q = path.find('?');
    if (q != std::string::npos) path = path.substr(0, q);
    header_end = buf.find("\r\n\r\n");
    if (header_end == std::string::npos) header_end = buf.size();

    std::string lower = buf;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    auto pos = lower.find("content-length:");
    if (pos != std::string::npos) {
        auto eol = lower.find("\r\n", pos);
        std::string val = lower.substr(pos + 15, eol - (pos + 15));
        try {
            content_length = static_cast<size_t>(std::stoul(val));
        } catch (...) {
            content_length = 0;
        }
    }
}

bool send_all(SSL* ssl, const std::string& data) {
    size_t sent = 0;
    int spins = 0;
    while (sent < data.size()) {
        int n = SSL_write(ssl, data.data() + sent, static_cast<int>(data.size() - sent));
        if (n > 0) {
            sent += static_cast<size_t>(n);
            spins = 0;
        } else {
            int err = SSL_get_error(ssl, n);
            if (err == SSL_ERROR_WANT_WRITE) {
                if (++spins > 64) return false;
                continue;
            }
            return false;
        }
    }
    return true;
}

}  // namespace

AdminServer::AdminServer(const AdminServerConfig& cfg, CommandHandler& handler)
    : cfg_(cfg), handler_(handler) {}

AdminServer::~AdminServer() { stop(); }

bool AdminServer::start() {
    SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());
    ssl_ctx_ = ctx;
    if (!ctx) {
        SIM_LOG(LogLevel::ERROR, kModule, "SSL_CTX_new failed");
        return false;
    }
    if (SSL_CTX_use_certificate_file(ctx, cfg_.cert_path.c_str(), SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(ctx, cfg_.key_path.c_str(), SSL_FILETYPE_PEM) <= 0) {
        SIM_LOG(LogLevel::ERROR, kModule,
                "failed to load cert/key: " + cfg_.cert_path + " / " + cfg_.key_path);
        return false;
    }

    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        SIM_LOG(LogLevel::ERROR, kModule, "socket() failed");
        return false;
    }
    int yes = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    struct timeval tv{};
    tv.tv_sec = 2;
    setsockopt(listen_fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(cfg_.host.c_str());
    addr.sin_port = htons(static_cast<uint16_t>(cfg_.port));
    if (addr.sin_addr.s_addr == INADDR_NONE) addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        SIM_LOG(LogLevel::ERROR, kModule,
                "bind failed on " + cfg_.host + ":" + std::to_string(cfg_.port) +
                    " | " + std::strerror(errno));
        return false;
    }
    if (listen(listen_fd_, 16) < 0) {
        SIM_LOG(LogLevel::ERROR, kModule, "listen() failed");
        return false;
    }

    running_.store(true);
    stopping_.store(false);
    int workers = cfg_.worker_threads > 0 ? cfg_.worker_threads : 4;
    for (int i = 0; i < workers; ++i) {
        workers_.emplace_back(&AdminServer::worker_loop, this);
    }
    accept_thread_ = std::thread(&AdminServer::accept_loop, this);

    SIM_LOG(LogLevel::INFO, kModule,
            "HTTPS server listening on " + cfg_.host + ":" + std::to_string(cfg_.port) +
                " | workers=" + std::to_string(workers) + " | server_id=" + cfg_.server_id);
    return true;
}

void AdminServer::stop() {
    if (!running_.exchange(false)) return;
    stopping_.store(true);
    if (listen_fd_ >= 0) {
        shutdown(listen_fd_, SHUT_RDWR);
        close(listen_fd_);
        listen_fd_ = -1;
    }
    qcv_.notify_all();
    if (accept_thread_.joinable()) accept_thread_.join();
    for (auto& w : workers_) {
        if (w.joinable()) w.join();
    }
    if (ssl_ctx_) {
        SSL_CTX_free(static_cast<SSL_CTX*>(ssl_ctx_));
        ssl_ctx_ = nullptr;
    }
    SIM_LOG(LogLevel::INFO, kModule, "stopped");
}

void AdminServer::accept_loop() {
    while (running_.load()) {
        sockaddr_in caddr{};
        socklen_t clen = sizeof(caddr);
        int fd = accept(listen_fd_, reinterpret_cast<sockaddr*>(&caddr), &clen);
        if (fd < 0) {
            if (errno == EINTR) continue;
            if (!running_.load()) break;
            continue;
        }
        struct timeval rtv{};
        rtv.tv_sec = 10;
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &rtv, sizeof(rtv));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &rtv, sizeof(rtv));

        Connection conn;
        conn.fd = fd;
        char ip[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &caddr.sin_addr, ip, sizeof(ip));
        conn.peer_addr = ip;

        {
            std::lock_guard<std::mutex> lk(qmtx_);
            if (queue_.size() >= kMaxPending) {
                SIM_LOG(LogLevel::WARN, kModule, "pending queue full — rejecting connection");
                close(fd);
                continue;
            }
            queue_.push(conn);
        }
        qcv_.notify_one();
    }
}

bool AdminServer::dequeue_connection(Connection& conn) {
    std::unique_lock<std::mutex> lk(qmtx_);
    qcv_.wait(lk, [this] { return !queue_.empty() || stopping_.load(); });
    if (queue_.empty()) return false;
    conn = queue_.front();
    queue_.pop();
    return true;
}

void AdminServer::worker_loop() {
    while (true) {
        Connection conn;
        if (!dequeue_connection(conn)) {
            if (!running_.load()) break;
            continue;
        }
        handle_connection(conn);
    }
}

void AdminServer::handle_connection(const Connection& conn) {
    SSL* ssl = SSL_new(static_cast<SSL_CTX*>(ssl_ctx_));
    if (!ssl) {
        close(conn.fd);
        return;
    }
    SSL_set_fd(ssl, conn.fd);
    if (SSL_accept(ssl) <= 0) {
        SIM_LOG(LogLevel::DEBUG, kModule, "TLS handshake failed from " + conn.peer_addr);
        SSL_free(ssl);
        close(conn.fd);
        return;
    }

    std::string buf;
    if (!read_until_headers(ssl, buf)) {
        SSL_free(ssl);
        close(conn.fd);
        return;
    }

    std::string method, path;
    size_t header_end = 0, content_length = 0;
    parse_request_line(buf, method, path, header_end, content_length);

    std::string body_raw = (header_end + 4 <= buf.size()) ? buf.substr(header_end + 4) : "";
    if (content_length > body_raw.size()) {
        size_t want = content_length;
        read_body(ssl, body_raw, want);
        if (body_raw.size() > content_length) body_raw.resize(content_length);
    }

    const int64_t recv_ms = unix_ms();
    const std::string recv_ts = format_log_timestamp();

    nlohmann::json body = nlohmann::json::object();
    if (!body_raw.empty()) {
        try {
            body = nlohmann::json::parse(body_raw);
        } catch (...) {
            body = nlohmann::json::object();
        }
    }

    int64_t client_id = static_cast<int64_t>(conn.fd);
    if (body.contains("client_id") && body["client_id"].is_number()) {
        client_id = body["client_id"].get<int64_t>();
    }

    std::string command = CommandHandler::normalize_command(path, body);
    Response resp = handler_.handle(command, body, client_id);

    const int64_t send_ms = unix_ms();
    resp.data["_frame"]["client_id"] = client_id;
    resp.data["_frame"]["client_addr"] = conn.peer_addr;
    resp.data["_frame"]["server_id"] = cfg_.server_id;
    resp.data["_frame"]["command"] = command;
    resp.data["_frame"]["recv_ts"] = recv_ts;
    resp.data["_frame"]["recv_unix_ms"] = recv_ms;
    resp.data["_frame"]["send_ts"] = format_log_timestamp();
    resp.data["_frame"]["send_unix_ms"] = send_ms;

    const std::string body_json = resp.to_json();
    std::string http = "HTTP/1.1 " + std::to_string(Response::http_status(resp.code)) + " OK\r\n";
    http += "Content-Type: application/json\r\n";
    http += "Content-Length: " + std::to_string(body_json.size()) + "\r\n";
    http += "Connection: close\r\n\r\n";
    http += body_json;

    send_all(ssl, http);

    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(conn.fd);
}

}  // namespace sim_vision
