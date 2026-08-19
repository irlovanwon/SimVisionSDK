/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Core data types, DataType enum, groups, timestamps, DataBundle, ChannelFrame
 * Date: 20260719
 * Modification: 20260819 DataBundle: right_channels/right_code per-side raw-type flags; mono flag "Mono"
 */
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace sim_vision {

enum class DataType : int {
    StereoImage   = 0,
    DepthMap      = 1,
    PointCloud    = 2,
    IMU           = 3,
    DisparityMap  = 4,
    ConfidenceMap = 5,
    Temperature   = 6,
    Magnetometer  = 7,
    Barometer     = 8,
};

enum class DataGroup : int {
    Visual2D   = 0,
    Visual3D   = 1,
    SensorData = 2,
};

constexpr int kDataTypeCount = 9;

const char* data_type_to_string(DataType t);
bool data_type_from_string(const std::string& s, DataType& out);
DataGroup data_type_to_group(DataType t);
const char* group_to_string(DataGroup g);
const char* group_to_channel(DataGroup g);
std::vector<DataType> group_members(DataGroup g);
std::vector<DataType> all_data_types();

int64_t now_unix_sec();
int64_t now_unix_nsec();
std::string format_log_timestamp();

struct DataBundle {
    DataType type;
    std::string part_id;             // ZMQ part id ("left","right","depth_map",...)
    std::vector<uint8_t> data;
    int64_t ts_sec  = 0;
    int64_t ts_nsec = 0;
    std::string source;
    // Part header metadata (emitted in ZMQ parts[] entry)
    std::string format;              // "JPG"/"PNG"/"TIFF"/"raw_u8"/"raw_f32"/"raw_f32x4"/"json"
    bool is_encoded = false;         // image only: true for JPG/PNG/TIFF
    int channels = 0;                // image only
    std::string code;                // image only: raw-type flag "BGRA"/"RGB"/"BGR"/"RGBA"/"Mono"
    int right_channels = 0;          // stereo only: right image channel count (when different from left)
    std::string right_code;          // stereo only: right image raw-type flag (when different from left)
    int width = 0;                   // image only
    int height = 0;                  // image only
};
using DataBundlePtr = std::shared_ptr<DataBundle>;

struct ChannelFrame {
    DataGroup group;
    int64_t ts_sec  = 0;
    int64_t ts_nsec = 0;
    uint64_t pair_id = 0;            // unique per capture tick (shared across all 3 groups)
    uint64_t frame_index = 0;
    std::string pub_id;
    std::vector<DataBundlePtr> bundles;
};
using ChannelFramePtr = std::shared_ptr<ChannelFrame>;

}  // namespace sim_vision
