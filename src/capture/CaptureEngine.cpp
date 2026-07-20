/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Subscriber-driven capture loop — FPS-paced via CV wait_for (low-power)
 * Date: 20260719
 * Modification:
 */
#include "sim_vision/capture/CaptureEngine.h"

#include "sim_vision/common/Logger.h"

#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>

namespace sim_vision {

namespace {
constexpr auto kModule = "CaptureEngine";
}

class CapturePacer {
public:
    void sleep_for(std::chrono::milliseconds d) {
        std::unique_lock<std::mutex> lk(mtx_);
        cv_.wait_for(lk, d, [this] { return stop_.load(); });
    }
    void wake() {
        std::lock_guard<std::mutex> lk(mtx_);
        stop_.store(true);
        cv_.notify_all();
    }
    void reset() { stop_.store(false); }

private:
    std::mutex mtx_;
    std::condition_variable cv_;
    std::atomic<bool> stop_{false};
};

CaptureEngine::CaptureEngine(SimDataSource& source, DataPipeline& pipeline,
                             ChannelManager& channels, ParameterManager& params)
    : source_(source), pipeline_(pipeline), channels_(channels), params_(params) {}

CaptureEngine::~CaptureEngine() { stop(); }

bool CaptureEngine::start() {
    if (running_.exchange(true)) return false;
    thread_ = std::thread(&CaptureEngine::run, this);
    SIM_LOG(LogLevel::INFO, kModule, "started");
    return true;
}

void CaptureEngine::stop() {
    if (!running_.exchange(false)) return;
    if (thread_.joinable()) thread_.join();
    SIM_LOG(LogLevel::INFO, kModule, "stopped");
}

ChannelFramePtr CaptureEngine::build_frame(DataGroup group, int64_t ts_sec,
                                           int64_t ts_nsec, uint64_t frame_index,
                                           const std::string& pub_id) {
    auto f = std::make_shared<ChannelFrame>();
    f->group = group;
    f->ts_sec = ts_sec;
    f->ts_nsec = ts_nsec;
    f->frame_index = frame_index;
    f->pair_id = frame_index;
    f->pub_id = pub_id;

    std::string depth_mode = "NEURAL";
    Parameter p;
    if (params_.get("depth_mode", p) && std::holds_alternative<param_type::Enum>(p.value)) {
        depth_mode = std::get<param_type::Enum>(p.value);
    }
    const int w = source_.image_width();
    const int h = source_.image_height();

    auto make_bundle = [&](DataType t, const std::string& part_id) {
        auto b = std::make_shared<DataBundle>();
        b->type = t;
        b->part_id = part_id;
        b->ts_sec = ts_sec;
        b->ts_nsec = ts_nsec;
        b->source = SimDataSource::version_tag();
        return b;
    };

    for (DataType t : group_members(group)) {
        if (!channels_.is_active(t)) continue;
        switch (t) {
            case DataType::StereoImage: {
                StereoPair pair;
                if (source_.next_stereo(pair)) {
                    auto b = make_bundle(t, "stereo_image");
                    const uint32_t lsize = static_cast<uint32_t>(pair.left.size());
                    b->data.resize(sizeof(uint32_t) + pair.left.size() + pair.right.size());
                    b->data[0] = static_cast<uint8_t>(lsize & 0xFF);
                    b->data[1] = static_cast<uint8_t>((lsize >> 8) & 0xFF);
                    b->data[2] = static_cast<uint8_t>((lsize >> 16) & 0xFF);
                    b->data[3] = static_cast<uint8_t>((lsize >> 24) & 0xFF);
                    std::memcpy(b->data.data() + sizeof(uint32_t),
                                pair.left.data(), pair.left.size());
                    std::memcpy(b->data.data() + sizeof(uint32_t) + pair.left.size(),
                                pair.right.data(), pair.right.size());
                    b->format = pair.left_info.format;
                    b->is_encoded = pair.left_info.is_encoded;
                    b->channels = pair.left_info.channels;
                    b->code = pair.left_info.code;
                    b->width = pair.left_info.width;
                    b->height = pair.left_info.height;
                    f->bundles.push_back(std::move(b));
                }
                break;
            }
            case DataType::DepthMap: {
                auto b = make_bundle(t, "depth_map");
                b->data = source_.generate_depth(w, h, frame_index);
                b->format = "raw_f32";
                f->bundles.push_back(std::move(b));
                break;
            }
            case DataType::DisparityMap: {
                auto b = make_bundle(t, "disparity_map");
                b->data = source_.generate_disparity(w, h, frame_index);
                b->format = "raw_f32";
                f->bundles.push_back(std::move(b));
                break;
            }
            case DataType::ConfidenceMap: {
                auto b = make_bundle(t, "confidence_map");
                b->data = source_.generate_confidence(w, h, frame_index);
                b->format = "raw_f32";
                f->bundles.push_back(std::move(b));
                break;
            }
            case DataType::PointCloud: {
                auto b = make_bundle(t, "point_cloud");
                b->data = source_.generate_pointcloud(depth_mode, frame_index);
                b->format = "raw_f32x4";
                f->bundles.push_back(std::move(b));
                break;
            }
            case DataType::IMU: {
                auto b = make_bundle(t, "imu");
                b->data = source_.generate_imu(frame_index);
                b->format = "json";
                f->bundles.push_back(std::move(b));
                break;
            }
            case DataType::Temperature: {
                auto b = make_bundle(t, "temperature");
                b->data = source_.generate_temperature(frame_index);
                b->format = "json";
                f->bundles.push_back(std::move(b));
                break;
            }
            case DataType::Magnetometer: {
                auto b = make_bundle(t, "magnetometer");
                b->data = source_.generate_magnetometer(frame_index);
                b->format = "json";
                f->bundles.push_back(std::move(b));
                break;
            }
            case DataType::Barometer: {
                auto b = make_bundle(t, "barometer");
                b->data = source_.generate_barometer(frame_index);
                b->format = "json";
                f->bundles.push_back(std::move(b));
                break;
            }
        }
    }
    return f;
}

void CaptureEngine::run() {
    CapturePacer pacer;
    uint64_t frame_index = 0;
    while (running_.load()) {
        auto tick_start = std::chrono::steady_clock::now();

        const int64_t ts_sec = now_unix_sec();
        const int64_t ts_nsec = now_unix_nsec();

        for (DataGroup g : {DataGroup::Visual2D, DataGroup::Visual3D, DataGroup::SensorData}) {
            if (!channels_.group_active(g)) continue;
            ChannelFramePtr f = build_frame(g, ts_sec, ts_nsec, frame_index, "");
            if (!f->bundles.empty()) {
                pipeline_.push(g, f);
            }
        }

        ++frame_index;
        frames_produced_.fetch_add(1, std::memory_order_relaxed);

        int fps = 15;
        Parameter pf;
        if (params_.get("fps", pf) && std::holds_alternative<param_type::Integer>(pf.value)) {
            fps = static_cast<int>(std::get<param_type::Integer>(pf.value));
        }
        if (fps < 1) fps = 1;
        if (fps > 240) fps = 240;

        auto period = std::chrono::milliseconds(1000 / fps);
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - tick_start);
        auto remaining = period - elapsed;
        if (remaining.count() > 0) {
            pacer.reset();
            pacer.sleep_for(remaining);
        }
    }
}

}  // namespace sim_vision
