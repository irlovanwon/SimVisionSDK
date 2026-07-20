/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: ZMQ PUB publisher (3 grouped channels, zero-copy, drop-NEWEST at HWM)
 * Date: 20260719
 * Modification:
 */
#pragma once

#include <zmq.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>

#include "sim_vision/common/Types.h"
#include "sim_vision/data/DataPipeline.h"

namespace sim_vision {

struct PublisherStats {
    uint64_t sent_2d = 0;
    uint64_t sent_3d = 0;
    uint64_t sent_sensor = 0;
    uint64_t dropped_hwm = 0;
    uint64_t shutdown_sent = 0;
};

class DataPublisher {
public:
    DataPublisher(void* zmq_ctx, DataPipeline& pipeline, const struct ZmqConfig& cfg);
    ~DataPublisher();

    bool start();
    void stop();

    void publish_shutdown();
    void drain();
    PublisherStats stats() const;

private:
    void run();
    void send_group(const ChannelFramePtr& frame);
    bool send_multipart(void* socket, const ChannelFramePtr& frame);

    void* ctx_;
    DataPipeline& pipeline_;
    std::string pub_id_;
    int hwm_;
    bool immediate_;
    int linger_ms_;
    std::unordered_map<int, std::string> endpoints_;

    void* sock_2d_   = nullptr;
    void* sock_3d_   = nullptr;
    void* sock_sensor_ = nullptr;

    std::thread thread_;
    std::atomic<bool> running_{false};

    std::atomic<uint64_t> sent_2d_{0};
    std::atomic<uint64_t> sent_3d_{0};
    std::atomic<uint64_t> sent_sensor_{0};
    std::atomic<uint64_t> dropped_hwm_{0};
    std::atomic<uint64_t> shutdown_sent_{0};
};

using DataPublisherPtr = std::shared_ptr<DataPublisher>;

}  // namespace sim_vision
