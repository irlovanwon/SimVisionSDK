/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Simulation data source — loads stereo images to RAM (round-robin) + synthetic generators
 * Date: 20260719
 * Modification:
 */
#pragma once

#include "sim_vision/common/Types.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace sim_vision {

struct ImageInfo {
    std::string format = "raw_u8";   // "JPG"/"PNG"/"TIFF"/"raw_u8"/"raw_u12"
    bool is_encoded = false;         // true for JPG/PNG/TIFF
    int channels = 0;                // decoded channel count
    std::string code;                // color code "BGRA"/"RGB"/"GRAY"
    int width = 0;
    int height = 0;
};

struct StereoPair {
    std::vector<uint8_t> left;
    std::vector<uint8_t> right;
    ImageInfo left_info;
    ImageInfo right_info;
    size_t pair_index = 0;
};

class SimDataSource {
public:
    bool load(const std::string& stereo_dir, std::string& err);
    size_t pair_count() const;

    bool next_stereo(StereoPair& out);

    std::vector<uint8_t> generate_depth(int width, int height, uint64_t frame_index) const;
    std::vector<uint8_t> generate_disparity(int width, int height, uint64_t frame_index) const;
    std::vector<uint8_t> generate_confidence(int width, int height, uint64_t frame_index) const;
    std::vector<uint8_t> generate_pointcloud(const std::string& depth_mode,
                                             uint64_t frame_index) const;
    std::vector<uint8_t> generate_imu(uint64_t frame_index) const;
    std::vector<uint8_t> generate_temperature(uint64_t frame_index) const;
    std::vector<uint8_t> generate_magnetometer(uint64_t frame_index) const;
    std::vector<uint8_t> generate_barometer(uint64_t frame_index) const;

    int image_width() const { return width_; }
    int image_height() const { return height_; }
    int image_channels() const { return channels_; }
    void set_image_size(int w, int h);
    void set_image_channels(int c) { channels_ = c; }

    static const char* version_tag() { return "SimVisionSDK/1.0"; }

private:
    std::vector<StereoPair> pairs_;
    mutable std::mutex mtx_;
    std::atomic<size_t> cursor_{0};
    int width_ = 1280;
    int height_ = 720;
    int channels_ = 4;
};

}  // namespace sim_vision
