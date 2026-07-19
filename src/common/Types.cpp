/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: DataType / group string conversions and timestamp helpers
 * Date: 20260719
 * Modification:
 */
#include "sim_vision/common/Types.h"

#include <chrono>
#include <cstdio>
#include <cstring>

namespace sim_vision {

namespace {
struct Entry {
    DataType t;
    const char* name;
    DataGroup g;
};

constexpr Entry kTable[] = {
    {DataType::StereoImage,   "stereo_image",   DataGroup::Visual2D},
    {DataType::DepthMap,      "depth_map",      DataGroup::Visual2D},
    {DataType::PointCloud,    "point_cloud",    DataGroup::Visual3D},
    {DataType::IMU,           "imu",            DataGroup::SensorData},
    {DataType::DisparityMap,  "disparity_map",  DataGroup::Visual3D},
    {DataType::ConfidenceMap, "confidence_map", DataGroup::Visual3D},
    {DataType::Temperature,   "temperature",    DataGroup::SensorData},
    {DataType::Magnetometer,  "magnetometer",   DataGroup::SensorData},
    {DataType::Barometer,     "barometer",      DataGroup::SensorData},
};
}  // namespace

const char* data_type_to_string(DataType t) {
    for (const auto& e : kTable) {
        if (e.t == t) return e.name;
    }
    return "unknown";
}

bool data_type_from_string(const std::string& s, DataType& out) {
    for (const auto& e : kTable) {
        if (s == e.name) {
            out = e.t;
            return true;
        }
    }
    return false;
}

DataGroup data_type_to_group(DataType t) {
    for (const auto& e : kTable) {
        if (e.t == t) return e.g;
    }
    return DataGroup::SensorData;
}

const char* group_to_string(DataGroup g) {
    switch (g) {
        case DataGroup::Visual2D:   return "visual_2d";
        case DataGroup::Visual3D:   return "visual_3d";
        case DataGroup::SensorData: return "sensor_data";
    }
    return "unknown";
}

const char* group_to_channel(DataGroup g) {
    switch (g) {
        case DataGroup::Visual2D:   return "visual_geometric_2d";
        case DataGroup::Visual3D:   return "visual_geometric_3d";
        case DataGroup::SensorData: return "sensor_tracking";
    }
    return "unknown";
}

std::vector<DataType> group_members(DataGroup g) {
    std::vector<DataType> v;
    for (const auto& e : kTable) {
        if (e.g == g) v.push_back(e.t);
    }
    return v;
}

std::vector<DataType> all_data_types() {
    std::vector<DataType> v;
    for (const auto& e : kTable) v.push_back(e.t);
    return v;
}

int64_t now_unix_sec() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

int64_t now_unix_nsec() {
    auto now = std::chrono::system_clock::now();
    auto secs = std::chrono::time_point_cast<std::chrono::seconds>(now);
    auto nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(now - secs);
    return static_cast<int64_t>(nsec.count());
}

std::string format_log_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto secs = std::chrono::time_point_cast<std::chrono::seconds>(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - secs);
    std::time_t t = std::chrono::system_clock::to_time_t(secs);
    std::tm tm{};
    localtime_r(&t, &tm);
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%04d%02d%02d-%02d%02d%02d-%03d",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec,
                  static_cast<int>(ms.count()));
    return buf;
}

}  // namespace sim_vision
