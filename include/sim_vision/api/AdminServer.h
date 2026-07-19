/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: HTTPS admin server (Hybrid: 1 accept + N workers) with per-frame metadata
 * Date: 20260719
 * Modification:
 */
#pragma once

#include "sim_vision/api/CommandHandler.h"
#include "sim_vision/api/SessionManager.h"
#include "sim_vision/common/Config.h"
#include "sim_vision/common/Response.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace sim_vision {

class AdminServer {
public:
    AdminServer(const AdminServerConfig& cfg, CommandHandler& handler);
    ~AdminServer();

    bool start();
    void stop();
    bool is_running() const { return running_.load(); }

private:
    struct Connection {
        int fd = -1;
        std::string peer_addr;
    };

    void accept_loop();
    void worker_loop();
    bool dequeue_connection(Connection& conn);
    void handle_connection(const Connection& conn);

    AdminServerConfig cfg_;
    CommandHandler& handler_;

    int listen_fd_ = -1;
    std::atomic<bool> running_{false};
    std::thread accept_thread_;
    std::vector<std::thread> workers_;

    std::mutex qmtx_;
    std::condition_variable qcv_;
    std::queue<Connection> queue_;
    std::atomic<bool> stopping_{false};

    void* ssl_ctx_ = nullptr;
};

}  // namespace sim_vision
