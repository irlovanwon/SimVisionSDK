/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Subscriber-driven simulation capture engine (FPS-paced via CV with timeout)
 * Date: 20260719
 * Modification:
 */
#pragma once

#include "sim_vision/common/Parameter.h"
#include "sim_vision/common/Types.h"
#include "sim_vision/data/ChannelManager.h"
#include "sim_vision/data/DataPipeline.h"
#include "sim_vision/datasource/SimDataSource.h"

#include <atomic>
#include <cstdint>
#include <thread>

namespace sim_vision {

class CaptureEngine {
public:
    CaptureEngine(SimDataSource& source, DataPipeline& pipeline,
                  ChannelManager& channels, ParameterManager& params);
    ~CaptureEngine();

    bool start();
    void stop();

    bool is_running() const { return running_.load(); }
    uint64_t frames_produced() const { return frames_produced_.load(); }

private:
    void run();
    ChannelFramePtr build_frame(DataGroup group, int64_t ts_sec, int64_t ts_nsec,
                                uint64_t frame_index, const std::string& pub_id);

    SimDataSource& source_;
    DataPipeline& pipeline_;
    ChannelManager& channels_;
    ParameterManager& params_;

    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> frames_produced_{0};
};

}  // namespace sim_vision
