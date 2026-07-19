/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Core data types, DataType enum, groups, timestamps, DataBundle, ChannelFrame
 * Date: 20260719
 * Modification:
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
    std::vector<uint8_t> data;
    int64_t ts_sec  = 0;
    int64_t ts_nsec = 0;
    std::string source;
};
using DataBundlePtr = std::shared_ptr<DataBundle>;

struct ChannelFrame {
    DataGroup group;
    int64_t ts_sec  = 0;
    int64_t ts_nsec = 0;
    uint64_t frame_index = 0;
    std::string pub_id;
    std::vector<DataBundlePtr> bundles;
};
using ChannelFramePtr = std::shared_ptr<ChannelFrame>;

}  // namespace sim_vision
