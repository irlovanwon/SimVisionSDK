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

uint16_t be16(const uint8_t* p) { return static_cast<uint16_t>((p[0] << 8) | p[1]); }
uint32_t be32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}
uint32_t le32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

// Probe an image buffer's real format + dimensions without decoding.
// Detects JPEG (SOF marker), PNG (IHDR), and the synthetic SIMRAW1 test format.
// Falls back to cfg defaults (def_w/def_h/def_ch) when probing fails.
ImageInfo probe_image(const std::vector<uint8_t>& buf, const std::string& ext,
                      int def_w, int def_h, int def_ch) {
    ImageInfo info;
    const size_t n = buf.size();

    // Synthetic SIMRAW1 test format: 16-byte magic + <w LE><h LE> + raw rows.
    if (n >= 24 && buf[0] == 'S' && buf[1] == 'I' && buf[2] == 'M' && buf[3] == 'R') {
        info.format = "raw_u8";
        info.is_encoded = false;
        info.width = static_cast<int>(le32(buf.data() + 16));
        info.height = static_cast<int>(le32(buf.data() + 20));
        info.channels = 3;
        info.code = "RGB";
        return info;
    }

    // JPEG: scan markers for SOF0/SOF2.
    if (n >= 4 && buf[0] == 0xFF && buf[1] == 0xD8) {
        info.format = "JPG";
        info.is_encoded = true;
        size_t i = 2;
        while (i + 3 < n) {
            if (buf[i] != 0xFF) { ++i; continue; }
            uint8_t marker = buf[i + 1];
            if (marker == 0xC0 || marker == 0xC2) {  // SOF0 / SOF2
                if (i + 9 < n) {
                    info.height = be16(buf.data() + i + 5);
                    info.width = be16(buf.data() + i + 7);
                    int comp = buf[i + 9];
                    info.channels = comp;
                    info.code = (comp == 1) ? "GRAY" : "RGB";
                    return info;
                }
            }
            // Skip this marker segment.
            if (i + 3 < n) {
                size_t seg = be16(buf.data() + i + 2);
                i += 2 + seg;
            } else {
                break;
            }
        }
        info.channels = def_ch > 0 ? def_ch : 3;
        info.code = (info.channels == 1) ? "GRAY" : "RGB";
        info.width = def_w;
        info.height = def_h;
        return info;
    }

    // PNG: signature + IHDR.
    static const uint8_t kPngSig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    if (n >= 24 && std::memcmp(buf.data(), kPngSig, 8) == 0) {
        info.format = "PNG";
        info.is_encoded = true;
        info.width = static_cast<int>(be32(buf.data() + 16));
        info.height = static_cast<int>(be32(buf.data() + 20));
        int color_type = buf.size() > 25 ? buf[25] : 6;
        switch (color_type) {
            case 0: info.channels = 1; info.code = "GRAY"; break;
            case 2: info.channels = 3; info.code = "RGB"; break;
            case 3: info.channels = 1; info.code = "GRAY"; break;  // palette
            case 4: info.channels = 2; info.code = "GRAYA"; break;
            case 6: info.channels = 4; info.code = "RGBA"; break;
            default: info.channels = 4; info.code = "RGBA"; break;
        }
        return info;
    }

    // TIFF.
    if (n >= 4 && ((buf[0] == 'I' && buf[1] == 'I') || (buf[0] == 'M' && buf[1] == 'M'))) {
        info.format = "TIFF";
        info.is_encoded = true;
        info.channels = def_ch > 0 ? def_ch : 3;
        info.code = "RGB";
        info.width = def_w;
        info.height = def_h;
        return info;
    }

    // Unknown — treat as raw_u8 with configured defaults.
    info.format = "raw_u8";
    info.is_encoded = false;
    info.channels = def_ch > 0 ? def_ch : 4;
    info.code = (info.channels == 4) ? "BGRA" : "RGB";
    info.width = def_w;
    info.height = def_h;
    (void)ext;
    return info;
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
        if (have_left && !have_right) {
            pair.right = pair.left;
        } else if (have_right && !have_left) {
            pair.left = pair.right;
        }
        pair.left_info = probe_image(pair.left, ext_of(kv.second.first),
                                     width_, height_, channels_);
        if (have_left && have_right) {
            pair.right_info = probe_image(pair.right, ext_of(kv.second.second),
                                          width_, height_, channels_);
        } else {
            pair.right_info = pair.left_info;  // mirrored side
        }
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
            row[x] = 0.3f + 0.7f * d;
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
