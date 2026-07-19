/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: 3-group SPSC pipeline + condition_variable (low-power wait_for consumer)
 * Date: 20260719
 * Modification:
 */
#include "sim_vision/data/DataPipeline.h"

namespace sim_vision {

DataPipeline::DataPipeline(size_t queue_capacity)
    : q2d_(queue_capacity), q3d_(queue_capacity), qsensor_(queue_capacity) {}

bool DataPipeline::push(DataGroup group, ChannelFramePtr frame) {
    bool ok = false;
    switch (group) {
        case DataGroup::Visual2D:   ok = q2d_.try_push(std::move(frame)); break;
        case DataGroup::Visual3D:   ok = q3d_.try_push(std::move(frame)); break;
        case DataGroup::SensorData: ok = qsensor_.try_push(std::move(frame)); break;
    }
    {
        std::lock_guard<std::mutex> lk(mtx_);
        notified_.store(true, std::memory_order_release);
    }
    cv_.notify_one();
    return ok;
}

bool DataPipeline::pop_all(std::vector<ChannelFramePtr>& out, int timeout_ms) {
    out.clear();
    ChannelFramePtr f;
    bool any = false;
    while (q2d_.try_pop(f))     { out.push_back(std::move(f)); any = true; }
    while (q3d_.try_pop(f))     { out.push_back(std::move(f)); any = true; }
    while (qsensor_.try_pop(f)) { out.push_back(std::move(f)); any = true; }
    if (any) return true;

    std::unique_lock<std::mutex> lk(mtx_);
    cv_.wait_for(lk, std::chrono::milliseconds(timeout_ms > 0 ? timeout_ms : 0),
                 [this] { return notified_.load(std::memory_order_acquire); });
    notified_.store(false, std::memory_order_release);
    lk.unlock();

    while (q2d_.try_pop(f))     { out.push_back(std::move(f)); any = true; }
    while (q3d_.try_pop(f))     { out.push_back(std::move(f)); any = true; }
    while (qsensor_.try_pop(f)) { out.push_back(std::move(f)); any = true; }
    return any;
}

void DataPipeline::drain() {
    drain_group(DataGroup::Visual2D);
    drain_group(DataGroup::Visual3D);
    drain_group(DataGroup::SensorData);
}

void DataPipeline::drain_group(DataGroup group) {
    ChannelFramePtr f;
    switch (group) {
        case DataGroup::Visual2D:   while (q2d_.try_pop(f)) {} break;
        case DataGroup::Visual3D:   while (q3d_.try_pop(f)) {} break;
        case DataGroup::SensorData: while (qsensor_.try_pop(f)) {} break;
    }
}

PipelineStats DataPipeline::stats() const {
    PipelineStats s;
    s.pushed_2d = q2d_.total_pushed();
    s.pushed_3d = q3d_.total_pushed();
    s.pushed_sensor = qsensor_.total_pushed();
    s.dropped_2d = q2d_.total_dropped();
    s.dropped_3d = q3d_.total_dropped();
    s.dropped_sensor = qsensor_.total_dropped();
    s.queue_2d = q2d_.size();
    s.queue_3d = q3d_.size();
    s.queue_sensor = qsensor_.size();
    return s;
}

size_t DataPipeline::capacity() const { return q2d_.capacity(); }

}  // namespace sim_vision
