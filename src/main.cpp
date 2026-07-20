/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Application entry — config, wiring, signal handling, restart broadcast
 * Date: 20260719
 * Modification:
 */
#include "sim_vision/api/AdminServer.h"
#include "sim_vision/api/CommandHandler.h"
#include "sim_vision/api/SessionManager.h"
#include "sim_vision/capture/CaptureEngine.h"
#include "sim_vision/common/Config.h"
#include "sim_vision/common/Logger.h"
#include "sim_vision/common/Parameter.h"
#include "sim_vision/data/ChannelManager.h"
#include "sim_vision/data/DataPipeline.h"
#include "sim_vision/data/DataPublisher.h"
#include "sim_vision/datasource/SimDataSource.h"

#include <zmq.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

namespace {
std::atomic<sig_atomic_t> g_signal{0};
void on_signal(int sig) { g_signal.store(sig); }

void ignore_sigpipe() {
    struct sigaction sa{};
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, nullptr);
}

void register_parameters(sim_vision::ParameterManager& pm, const sim_vision::Config& cfg) {
    using namespace sim_vision;
    auto add_int = [&](const std::string& n, int64_t def, int64_t lo, int64_t hi, bool reopen) {
        Parameter p;
        p.name = n;
        p.value = def;
        p.min = lo;
        p.max = hi;
        p.default_value = def;
        p.needs_reopen = reopen;
        pm.register_parameter(p);
    };
    auto add_enum = [&](const std::string& n, const std::string& def,
                        std::vector<std::string> vals, bool reopen) {
        Parameter p;
        p.name = n;
        p.value = def;
        p.default_value = def;
        p.enum_options = std::move(vals);
        p.needs_reopen = reopen;
        pm.register_parameter(p);
    };

    // --- InitParameters (camera reopen simulated; takes effect next frame) ---
    add_int("fps", cfg.capture.fps, 1, 120, true);
    add_enum("resolution", "HD720",
             {"HD2K", "HD1080", "HD720", "HD1200", "VGA"}, true);
    add_enum("depth_mode", "NEURAL",
             {"NONE", "NEURAL_LIGHT", "NEURAL", "NEURAL_PLUS", "PERFORMANCE", "QUALITY", "ULTRA"},
             true);

    // --- RuntimeParameters (immediate effect) ---
    add_int("exposure_time", 0, 0, 33333, false);
    add_int("gain", 50, 0, 100, false);              // generic gain (non-ZED-X)
    add_int("analog_gain", 1000, 1000, 16000, false); // ZED X series
    add_int("digital_gain", 1, 1, 256, false);        // ZED X series
    add_enum("auto_exposure_gain", "On", {"On", "Off"}, false);
    add_enum("mem_type", "CPU", {"CPU", "GPU"}, false);

    // --- Application-level Timer Gate (publish-rate throttle) ---
    add_int("target_fps_2d", 15, 0, 240, false);
    add_int("target_fps_3d", 15, 0, 240, false);
    add_int("target_fps_sensor", 200, 0, 1000, false);
}
}  // namespace

int main(int argc, char** argv) {
    ignore_sigpipe();

    std::string config_dir = "config";
    if (argc > 1) config_dir = argv[1];

    std::string err;
    sim_vision::Config cfg = sim_vision::ConfigLoader::load(config_dir, err);
    if (!err.empty()) {
        std::cerr << "[SimVisionSDK] config error: " << err << "\n";
        return 1;
    }

    namespace fs = std::filesystem;
    fs::create_directories("log");
    fs::create_directories("status");
    sim_vision::log_init(cfg.log_level, "log/sim_vision_node.log");

    SIM_LOG(sim_vision::LogLevel::INFO, "Main",
            "SimVisionSDK starting | config_dir=" + config_dir);

    void* zmq_ctx = zmq_ctx_new();
    zmq_ctx_set(zmq_ctx, ZMQ_IO_THREADS, 2);

    sim_vision::SimDataSource source;
    source.set_image_size(cfg.capture.image_width, cfg.capture.image_height);
    source.set_image_channels(cfg.capture.image_channels);
    std::string ds_err;
    source.load(cfg.capture.stereo_image_dir, ds_err);

    sim_vision::ParameterManager params;
    register_parameters(params, cfg);

    sim_vision::ChannelManager channels;
    sim_vision::SessionManager sessions;
    sim_vision::DataPipeline pipeline(static_cast<size_t>(cfg.spsc.queue_size));
    sim_vision::DataPublisher publisher(zmq_ctx, pipeline, cfg.zmq);
    sim_vision::CaptureEngine engine(source, pipeline, channels, params);

    sim_vision::CommandHandler handler(sessions, channels, params, cfg,
                                       &publisher, &pipeline, &source, &engine);
    sim_vision::AdminServer server(cfg.admin, handler);

    if (!publisher.start()) {
        SIM_LOG(sim_vision::LogLevel::FATAL, "Main", "DataPublisher start failed");
        return 1;
    }
    if (!engine.start()) {
        SIM_LOG(sim_vision::LogLevel::FATAL, "Main", "CaptureEngine start failed");
        return 1;
    }
    if (!server.start()) {
        SIM_LOG(sim_vision::LogLevel::FATAL, "Main", "AdminServer start failed");
        return 1;
    }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    SIM_LOG(sim_vision::LogLevel::INFO, "Main", "SimVisionSDK ready");

    while (g_signal.load() == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    SIM_LOG(sim_vision::LogLevel::INFO, "Main",
            "shutdown signal received — broadcasting server_shutdown");

    publisher.publish_shutdown();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    server.stop();
    engine.stop();
    pipeline.drain();
    publisher.stop();

    zmq_ctx_destroy(zmq_ctx);
    SIM_LOG(sim_vision::LogLevel::INFO, "Main", "SimVisionSDK stopped cleanly");
    return 0;
}
