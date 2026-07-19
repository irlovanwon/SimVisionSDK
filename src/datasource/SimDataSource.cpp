/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Loads stereo images to RAM (round-robin) and generates synthetic sensor/3D data
 * Date: 20260719
 * Modification:
 */
#include "sim_vision/datasource/SimDataSource.h"

#include "sim_vision/common/Logger.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>

namespace sim_vision {

namespace {

std::string lower(const std::string& s) {
    std::string o = s;
    std::transform(o.begin(), o.end(), o.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return o;
}

std::string ext_of(const std::string& fn) {
    auto pos = fn.find_last_of('.');
    if (pos == std::string::npos) return "";
    return lower(fn.substr(pos + 1));
}

bool read_file(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return false;
    ifs.seekg(0, std::ios::end);
    auto sz = ifs.tellg();
    if (sz <= 0) return false;
    ifs.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(sz));
    ifs.read(reinterpret_cast<char*>(out.data()), sz);
    return ifs.good() || ifs.eof();
}

void append_floats(std::vector<uint8_t>& out, const float* vals, size_t n) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(vals);
    out.insert(out.end(), p, p + n * sizeof(float));
}

}  // namespace

bool SimDataSource::load(const std::string& stereo_dir, std::string& err) {
    namespace fs = std::filesystem;
    if (stereo_dir.empty()) {
        err = "stereo_image_dir is empty";
        return false;
    }
    if (!fs::exists(stereo_dir)) {
        SIM_LOG(LogLevel::WARN, "SimDataSource",
                "stereo dir not found, will synthesize frames: " + stereo_dir);
        return true;
    }

    std::map<size_t, std::pair<std::string, std::string>> by_index;  // index -> (L_path, R_path)
    for (const auto& entry : fs::directory_iterator(stereo_dir)) {
        if (!entry.is_regular_file()) continue;
        std::string fn = entry.path().filename().string();
        std::string ext = ext_of(fn);
        if (ext != "jpg" && ext != "jpeg" && ext != "png" && ext != "raw") continue;
        auto stem = entry.path().stem().string();  // filename without ext
        auto us = stem.find_last_of('_');
        if (us == std::string::npos) continue;
        std::string camid = stem.substr(0, us);
        std::string idxs = stem.substr(us + 1);
        if (idxs.empty()) continue;
        size_t idx = 0;
        try {
            idx = std::stoul(idxs);
        } catch (...) {
            continue;
        }
        char side = lower(camid).empty() ? ' ' : lower(camid)[0];
        auto& slot = by_index[idx];
        if (side == 'l') slot.first = entry.path().string();
        else if (side == 'r') slot.second = entry.path().string();
    }

    std::vector<StereoPair> loaded;
    for (const auto& kv : by_index) {
        StereoPair pair;
        pair.pair_index = kv.first;
        bool have_left = !kv.second.first.empty();
        bool have_right = !kv.second.second.empty();
        if (!have_left && !have_right) continue;
        if (have_left) read_file(kv.second.first, pair.left);
        if (have_right) read_file(kv.second.second, pair.right);
        if (have_left && !have_right) pair.right = pair.left;
        if (have_right && !have_left) pair.left = pair.right;
        pair.left_ext = ext_of(kv.second.first);
        pair.right_ext = ext_of(kv.second.second);
        loaded.push_back(std::move(pair));
    }

    std::sort(loaded.begin(), loaded.end(),
              [](const StereoPair& a, const StereoPair& b) { return a.pair_index < b.pair_index; });

    {
        std::lock_guard<std::mutex> lk(mtx_);
        pairs_ = std::move(loaded);
    }
    SIM_LOG(LogLevel::INFO, "SimDataSource",
            "loaded " + std::to_string(pairs_.size()) + " stereo pair(s) from " + stereo_dir);
    return true;
}

size_t SimDataSource::pair_count() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return pairs_.size();
}

bool SimDataSource::next_stereo(StereoPair& out) {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (pairs_.empty()) {
            return false;
        }
        size_t idx = cursor_.fetch_add(1) % pairs_.size();
        out = pairs_[idx];
    }
    return true;
}

void SimDataSource::set_image_size(int w, int h) {
    width_ = w;
    height_ = h;
}

std::vector<uint8_t> SimDataSource::generate_depth(int width, int height,
                                                    uint64_t frame_index) const {
    std::vector<uint8_t> out;
    out.reserve(static_cast<size_t>(width) * height * sizeof(float));
    std::vector<float> row(width);
    const double t = static_cast<double>(frame_index) * 0.05;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            double nx = static_cast<double>(x) / width;
            double ny = static_cast<double>(y) / height;
            float d = 0.5f + 0.5f * static_cast<float>(std::sin(nx * 6.0 + t) *
                                                        std::cos(ny * 4.0 + t));
            row[x] = d * 10.0f;
        }
        append_floats(out, row.data(), row.size());
    }
    return out;
}

std::vector<uint8_t> SimDataSource::generate_disparity(int width, int height,
                                                       uint64_t frame_index) const {
    std::vector<uint8_t> out;
    out.reserve(static_cast<size_t>(width) * height * sizeof(float));
    std::vector<float> row(width);
    const double t = static_cast<double>(frame_index) * 0.05;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            row[x] = static_cast<float>(32.0 + 16.0 * std::sin(x * 0.05 + t) -
                                        8.0 * std::cos(y * 0.07 + t));
        }
        append_floats(out, row.data(), row.size());
    }
    return out;
}

std::vector<uint8_t> SimDataSource::generate_confidence(int width, int height,
                                                        uint64_t frame_index) const {
    std::vector<uint8_t> out;
    out.reserve(static_cast<size_t>(width) * height * sizeof(float));
    std::vector<float> row(width);
    const double t = static_cast<double>(frame_index) * 0.03;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float c = 0.5f + 0.5f * static_cast<float>(std::sin(x * 0.02 + y * 0.02 + t));
            row[x] = std::min(1.0f, std::max(0.0f, c));
        }
        append_floats(out, row.data(), row.size());
    }
    return out;
}

std::vector<uint8_t> SimDataSource::generate_pointcloud(const std::string& depth_mode,
                                                        uint64_t frame_index) const {
    size_t points = 0;
    if (depth_mode == "NONE") points = 0;
    else if (depth_mode == "PERFORMANCE") points = 5000;
    else if (depth_mode == "NEURAL_LIGHT") points = 10000;
    else if (depth_mode == "QUALITY") points = 20000;
    else if (depth_mode == "NEURAL") points = 40000;
    else if (depth_mode == "ULTRA") points = 60000;
    else if (depth_mode == "NEURAL_PLUS") points = 80000;
    else points = 40000;

    std::vector<uint8_t> out;
    out.reserve(points * 4 * sizeof(float));
    const double t = static_cast<double>(frame_index) * 0.05;
    std::array<float, 4> pt{};
    for (size_t i = 0; i < points; ++i) {
        double a = static_cast<double>(i) / points * 6.2831853;
        double r = 1.0 + 0.5 * std::sin(a * 3.0 + t);
        pt[0] = static_cast<float>(r * std::cos(a + t));
        pt[1] = static_cast<float>(r * std::sin(a + t));
        pt[2] = static_cast<float>(std::cos(a * 2.0 + t) * 2.0);
        pt[3] = static_cast<float>(i) / static_cast<float>(points);
        append_floats(out, pt.data(), 4);
    }
    return out;
}

std::vector<uint8_t> SimDataSource::generate_imu(uint64_t frame_index) const {
    const double t = static_cast<double>(frame_index) * 0.05;
    nlohmann::json j;
    j["accel"] = {0.1 * std::sin(t), 0.1 * std::cos(t), 9.81};
    j["gyro"]  = {0.01 * std::sin(t), 0.01 * std::cos(t), 0.0};
    j["frame_index"] = frame_index;
    const std::string s = j.dump();
    return std::vector<uint8_t>(s.begin(), s.end());
}

std::vector<uint8_t> SimDataSource::generate_temperature(uint64_t frame_index) const {
    nlohmann::json j;
    j["celsius"] = 38.0 + 0.5 * std::sin(static_cast<double>(frame_index) * 0.01);
    j["frame_index"] = frame_index;
    const std::string s = j.dump();
    return std::vector<uint8_t>(s.begin(), s.end());
}

std::vector<uint8_t> SimDataSource::generate_magnetometer(uint64_t frame_index) const {
    const double t = static_cast<double>(frame_index) * 0.02;
    nlohmann::json j;
    j["field"] = {0.3 * std::sin(t), 0.3 * std::cos(t), -0.45};
    j["frame_index"] = frame_index;
    const std::string s = j.dump();
    return std::vector<uint8_t>(s.begin(), s.end());
}

std::vector<uint8_t> SimDataSource::generate_barometer(uint64_t frame_index) const {
    nlohmann::json j;
    j["pressure_hpa"] = 1013.25 + 0.2 * std::sin(static_cast<double>(frame_index) * 0.01);
    j["frame_index"] = frame_index;
    const std::string s = j.dump();
    return std::vector<uint8_t>(s.begin(), s.end());
}

}  // namespace sim_vision
