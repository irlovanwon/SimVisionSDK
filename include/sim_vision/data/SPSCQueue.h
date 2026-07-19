/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Lock-free Single-Producer Single-Consumer ring buffer (drop-NEWEST on full)
 * Date: 20260719
 * Modification:
 */
#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <vector>

#include "sim_vision/common/Types.h"

namespace sim_vision {

class ChannelFrameQueue {
public:
    explicit ChannelFrameQueue(size_t capacity);

    bool try_push(ChannelFramePtr frame);
    bool try_pop(ChannelFramePtr& out);

    bool empty() const;
    bool full() const;
    size_t capacity() const;
    size_t size() const;

    uint64_t total_pushed() const;
    uint64_t total_dropped() const;
    void reset_stats();

    void clear();

private:
    size_t capacity_;
    std::vector<ChannelFramePtr> buffer_;
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};
    std::atomic<uint64_t> pushed_{0};
    std::atomic<uint64_t> dropped_{0};
};

}  // namespace sim_vision
