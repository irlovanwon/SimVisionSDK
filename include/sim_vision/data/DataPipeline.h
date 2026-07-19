/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: 3-group SPSC pipeline with condition_variable signalling (low-power wait)
 * Date: 20260719
 * Modification:
 */
#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

#include "sim_vision/common/Types.h"
#include "sim_vision/data/SPSCQueue.h"

namespace sim_vision {

struct PipelineStats {
    uint64_t pushed_2d = 0;
    uint64_t pushed_3d = 0;
    uint64_t pushed_sensor = 0;
    uint64_t dropped_2d = 0;
    uint64_t dropped_3d = 0;
    uint64_t dropped_sensor = 0;
    size_t queue_2d = 0;
    size_t queue_3d = 0;
    size_t queue_sensor = 0;
};

class DataPipeline {
public:
    explicit DataPipeline(size_t queue_capacity);

    bool push(DataGroup group, ChannelFramePtr frame);
    bool pop_all(std::vector<ChannelFramePtr>& out, int timeout_ms);
    void drain();
    void drain_group(DataGroup group);

    PipelineStats stats() const;
    size_t capacity() const;

private:
    ChannelFrameQueue q2d_;
    ChannelFrameQueue q3d_;
    ChannelFrameQueue qsensor_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::atomic<bool> notified_{false};
};

}  // namespace sim_vision
