/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: JSON configuration loader with validation at startup
 * Date: 20260719
 * Modification:
 */
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace sim_vision {

struct AdminServerConfig {
    std::string host = "0.0.0.0";
    int port = 8443;
    std::string cert_path = "certs/server.crt";
    std::string key_path = "certs/server.key";
    int worker_threads = 4;
    std::string server_id = "sim_vision_01";
};

struct CaptureConfig {
    int fps = 15;
    std::string data_source_dir = "DataSource";
    std::string stereo_image_dir = "DataSource/StereoImage";
    int image_width = 1280;
    int image_height = 720;
    int image_channels = 4;
};

struct ZmqConfig {
    std::string transport = "ipc";
    int hwm = 10;
    bool immediate = true;
    int linger_ms = 1000;
    std::string pub_id = "sim_vision_pub_01";
    std::unordered_map<std::string, std::string> endpoints;
};

struct SpscConfig {
    int queue_size = 8;
    std::string drop_policy = "newest";
    int cv_timeout_ms = 10;
};

struct Config {
    int version = 1;
    AdminServerConfig admin;
    CaptureConfig capture;
    ZmqConfig zmq;
    SpscConfig spsc;
    std::string log_level = "info";
};

class ConfigLoader {
public:
    static Config load(const std::string& config_dir, std::string& err);
};

}  // namespace sim_vision
