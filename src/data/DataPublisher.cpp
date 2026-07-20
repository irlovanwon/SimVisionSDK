/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: ZMQ PUB publisher — 3 grouped channels, zero-copy, drop-NEWEST at HWM
 * Date: 20260719
 * Modification:
 */
#include "sim_vision/data/DataPublisher.h"

#include "sim_vision/common/Config.h"
#include "sim_vision/common/Logger.h"
#include "sim_vision/datasource/SimDataSource.h"

#include <nlohmann/json.hpp>

#include <cerrno>
#include <cstring>
#include <new>

namespace sim_vision {

namespace {

void payload_free(void* /*data*/, void* hint) {
    auto* p = static_cast<std::shared_ptr<DataBundle>*>(hint);
    delete p;
}

int send_frame(void* sock, const void* data, size_t len, int flags,
               const std::shared_ptr<DataBundle>& keep) {
    zmq_msg_t msg;
    if (keep) {
        auto* hint = new (std::nothrow) std::shared_ptr<DataBundle>(keep);
        if (!hint) return -1;
        if (zmq_msg_init_data(&msg, const_cast<void*>(data), len, payload_free, hint) != 0) {
            delete hint;
            return -1;
        }
    } else {
        if (zmq_msg_init_size(&msg, len) != 0) return -1;
        if (len > 0) std::memcpy(zmq_msg_data(&msg), data, len);
    }
    int rc = zmq_msg_send(&msg, sock, flags);
    if (rc < 0) {
        zmq_msg_close(&msg);
    }
    return rc;
}

void configure_pub(void* sock, int hwm, bool immediate, int linger_ms) {
    int val = hwm;
    zmq_setsockopt(sock, ZMQ_SNDHWM, &val, sizeof(val));
    int imm = immediate ? 1 : 0;
    zmq_setsockopt(sock, ZMQ_IMMEDIATE, &imm, sizeof(imm));
    int linger = linger_ms;
    zmq_setsockopt(sock, ZMQ_LINGER, &linger, sizeof(linger));
}

}  // namespace

DataPublisher::DataPublisher(void* zmq_ctx, DataPipeline& pipeline, const ZmqConfig& cfg)
    : ctx_(zmq_ctx),
      pipeline_(pipeline),
      pub_id_(cfg.pub_id),
      hwm_(cfg.hwm),
      immediate_(cfg.immediate),
      linger_ms_(cfg.linger_ms) {
    endpoints_[static_cast<int>(DataGroup::Visual2D)]   =
        cfg.endpoints.count("visual_2d") ? cfg.endpoints.at("visual_2d") : "ipc:///tmp/sim_vision_visual_2d";
    endpoints_[static_cast<int>(DataGroup::Visual3D)]   =
        cfg.endpoints.count("visual_3d") ? cfg.endpoints.at("visual_3d") : "ipc:///tmp/sim_vision_visual_3d";
    endpoints_[static_cast<int>(DataGroup::SensorData)] =
        cfg.endpoints.count("sensor_data") ? cfg.endpoints.at("sensor_data") : "ipc:///tmp/sim_vision_sensor_data";
}

DataPublisher::~DataPublisher() {
    stop();
    if (sock_2d_)     zmq_close(sock_2d_);
    if (sock_3d_)     zmq_close(sock_3d_);
    if (sock_sensor_) zmq_close(sock_sensor_);
}

bool DataPublisher::start() {
    if (running_.load()) return false;

    sock_2d_     = zmq_socket(ctx_, ZMQ_PUB);
    sock_3d_     = zmq_socket(ctx_, ZMQ_PUB);
    sock_sensor_ = zmq_socket(ctx_, ZMQ_PUB);
    if (!sock_2d_ || !sock_3d_ || !sock_sensor_) {
        SIM_LOG(LogLevel::ERROR, "DataPublisher", "zmq_socket failed");
        return false;
    }
    configure_pub(sock_2d_, hwm_, immediate_, linger_ms_);
    configure_pub(sock_3d_, hwm_, immediate_, linger_ms_);
    configure_pub(sock_sensor_, hwm_, immediate_, linger_ms_);

    auto bind_one = [this](void* sock, DataGroup g) -> bool {
        const std::string& ep = endpoints_[static_cast<int>(g)];
        if (zmq_bind(sock, ep.c_str()) != 0) {
            SIM_LOG(LogLevel::ERROR, "DataPublisher",
                    "bind failed: " + ep + " | " + zmq_strerror(zmq_errno()));
            return false;
        }
        SIM_LOG(LogLevel::INFO, "DataPublisher", "PUB bound: " + ep);
        return true;
    };
    if (!bind_one(sock_2d_, DataGroup::Visual2D) ||
        !bind_one(sock_3d_, DataGroup::Visual3D) ||
        !bind_one(sock_sensor_, DataGroup::SensorData)) {
        return false;
    }

    running_.store(true);
    thread_ = std::thread(&DataPublisher::run, this);
    return true;
}

void DataPublisher::stop() {
    if (!running_.exchange(false)) return;
    if (thread_.joinable()) thread_.join();
}

void DataPublisher::run() {
    std::vector<ChannelFramePtr> frames;
    while (running_.load()) {
        bool got = pipeline_.pop_all(frames, 10);
        if (!got) continue;
        for (auto& f : frames) {
            if (f) send_group(f);
        }
    }
}

void DataPublisher::send_group(const ChannelFramePtr& frame) {
    void* sock = nullptr;
    switch (frame->group) {
        case DataGroup::Visual2D:   sock = sock_2d_;   break;
        case DataGroup::Visual3D:   sock = sock_3d_;   break;
        case DataGroup::SensorData: sock = sock_sensor_; break;
    }
    if (!sock) return;
    send_multipart(sock, frame);
}

bool DataPublisher::send_multipart(void* sock, const ChannelFramePtr& frame) {
    // Build JSON header (ZED-compatible): Frame 0 = header, Frames 1..N = payloads.
    nlohmann::json header;
    header["group"] = group_to_string(frame->group);
    header["ts_sec"] = frame->ts_sec;
    header["ts_nsec"] = frame->ts_nsec;
    header["pair_id"] = frame->pair_id;
    header["active"] = true;
    header["pub_id"] = frame->pub_id.empty() ? pub_id_ : frame->pub_id;
    header["frame_index"] = frame->frame_index;
    header["source"] = SimDataSource::version_tag();
    const int64_t pub_sec = now_unix_sec();
    const int64_t pub_nsec = now_unix_nsec();
    header["pub_ts_sec"] = pub_sec;
    header["pub_ts_nsec"] = pub_nsec;

    nlohmann::json parts = nlohmann::json::array();
    for (const auto& b : frame->bundles) {
        nlohmann::json p;
        p["id"] = b->part_id;
        p["size"] = b->data.size();
        p["format"] = b->format;
        if (b->width > 0) {  // image part: carry full image metadata
            p["is_encoded"] = b->is_encoded;
            p["channels"] = b->channels;
            p["channel_layout"] = b->code;
            p["width"] = b->width;
            p["height"] = b->height;
        }
        parts.push_back(std::move(p));
    }
    header["parts"] = parts;
    const std::string header_str = header.dump();

    const int dontwait = ZMQ_DONTWAIT;
    const size_t n = frame->bundles.size();

    // Frame 0 = JSON header.
    int rc = send_frame(sock, header_str.data(), header_str.size(),
                        dontwait | (n ? ZMQ_SNDMORE : 0), nullptr);
    if (rc < 0) {
        dropped_hwm_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    // Frames 1..N = binary payloads (one per part).
    for (size_t i = 0; i < n; ++i) {
        const auto& b = frame->bundles[i];
        int flags = dontwait | (i + 1 < n ? ZMQ_SNDMORE : 0);
        rc = send_frame(sock, b->data.data(), b->data.size(), flags, b);
        if (rc < 0) {
            dropped_hwm_.fetch_add(1, std::memory_order_relaxed);
            SIM_LOG(LogLevel::WARN, "DataPublisher",
                    std::string("HWM drop on ") + group_to_string(frame->group) +
                        " frame=" + std::to_string(frame->frame_index));
            return false;
        }
    }

    switch (frame->group) {
        case DataGroup::Visual2D:   sent_2d_.fetch_add(1, std::memory_order_relaxed); break;
        case DataGroup::Visual3D:   sent_3d_.fetch_add(1, std::memory_order_relaxed); break;
        case DataGroup::SensorData: sent_sensor_.fetch_add(1, std::memory_order_relaxed); break;
    }
    return true;
}

void DataPublisher::publish_shutdown() {
    auto send_ctrl = [this](void* sock) {
        if (!sock) return;
        nlohmann::json h;
        h["event"] = "server_shutdown";
        h["pub_id"] = pub_id_;
        h["ts_sec"] = now_unix_sec();
        h["ts_nsec"] = now_unix_nsec();
        const std::string s = h.dump();
        int rc = send_frame(sock, s.data(), s.size(), ZMQ_DONTWAIT, nullptr);
        if (rc >= 0) shutdown_sent_.fetch_add(1, std::memory_order_relaxed);
    };
    send_ctrl(sock_2d_);
    send_ctrl(sock_3d_);
    send_ctrl(sock_sensor_);
    SIM_LOG(LogLevel::INFO, "DataPublisher", "broadcast server_shutdown on all PUB channels");
}

void DataPublisher::drain() {
    std::vector<ChannelFramePtr> frames;
    pipeline_.drain();
    SIM_LOG(LogLevel::INFO, "DataPublisher", "drained SPSC pipeline + ZMQ relies on IMMEDIATE+HWM");
}

PublisherStats DataPublisher::stats() const {
    PublisherStats s;
    s.sent_2d = sent_2d_.load();
    s.sent_3d = sent_3d_.load();
    s.sent_sensor = sent_sensor_.load();
    s.dropped_hwm = dropped_hwm_.load();
    s.shutdown_sent = shutdown_sent_.load();
    return s;
}

}  // namespace sim_vision
