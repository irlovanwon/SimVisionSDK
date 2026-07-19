/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Lock-free SPSC ring buffer (monotonic head/tail, drop-NEWEST when full)
 * Date: 20260719
 * Modification:
 */
#include "sim_vision/data/SPSCQueue.h"

namespace sim_vision {

ChannelFrameQueue::ChannelFrameQueue(size_t capacity) {
    capacity_ = capacity < 1 ? 1 : capacity;
    buffer_.resize(capacity_);
}

bool ChannelFrameQueue::try_push(ChannelFramePtr frame) {
    const size_t h = head_.load(std::memory_order_relaxed);
    const size_t t = tail_.load(std::memory_order_acquire);
    if (h - t >= capacity_) {
        dropped_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    buffer_[h % capacity_] = std::move(frame);
    head_.store(h + 1, std::memory_order_release);
    pushed_.fetch_add(1, std::memory_order_relaxed);
    return true;
}

bool ChannelFrameQueue::try_pop(ChannelFramePtr& out) {
    const size_t t = tail_.load(std::memory_order_relaxed);
    const size_t h = head_.load(std::memory_order_acquire);
    if (h == t) return false;
    out = std::move(buffer_[t % capacity_]);
    buffer_[t % capacity_].reset();
    tail_.store(t + 1, std::memory_order_release);
    return true;
}

bool ChannelFrameQueue::empty() const {
    return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
}

bool ChannelFrameQueue::full() const {
    return (head_.load(std::memory_order_acquire) - tail_.load(std::memory_order_acquire)) >= capacity_;
}

size_t ChannelFrameQueue::capacity() const { return capacity_; }

size_t ChannelFrameQueue::size() const {
    const size_t h = head_.load(std::memory_order_acquire);
    const size_t t = tail_.load(std::memory_order_acquire);
    return h - t;
}

uint64_t ChannelFrameQueue::total_pushed() const {
    return pushed_.load(std::memory_order_relaxed);
}

uint64_t ChannelFrameQueue::total_dropped() const {
    return dropped_.load(std::memory_order_relaxed);
}

void ChannelFrameQueue::reset_stats() {
    pushed_.store(0, std::memory_order_relaxed);
    dropped_.store(0, std::memory_order_relaxed);
}

void ChannelFrameQueue::clear() {
    ChannelFramePtr p;
    while (try_pop(p)) {
    }
}

}  // namespace sim_vision
