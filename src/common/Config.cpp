/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: JSON config loader with startup validation (fail-fast)
 * Date: 20260719
 * Modification:
 */
#include "sim_vision/common/Config.h"

#include "sim_vision/common/Logger.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>

namespace sim_vision {

namespace {
nlohmann::json load_json_file(const std::string& path, std::string& err) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        err = "cannot open config file: " + path;
        return {};
    }
    nlohmann::json j;
    try {
        ifs >> j;
    } catch (const std::exception& e) {
        err = std::string("config parse error: ") + e.what();
        return {};
    }
    return j;
}

bool check(const nlohmann::json& j, const char* key, std::string& err) {
    if (!j.contains(key)) {
        err = std::string("missing required config field: ") + key;
        return false;
    }
    return true;
}
}  // namespace

Config ConfigLoader::load(const std::string& config_dir, std::string& err) {
    Config cfg;

    nlohmann::json j = load_json_file(config_dir + "/config.json", err);
    if (!err.empty()) return cfg;

    try {
        if (j.contains("version")) cfg.version = j["version"].get<int>();

        if (j.contains("admin_server")) {
            const auto& a = j["admin_server"];
            if (a.contains("host")) cfg.admin.host = a["host"].get<std::string>();
            if (a.contains("port")) cfg.admin.port = a["port"].get<int>();
            if (a.contains("cert_path")) cfg.admin.cert_path = a["cert_path"].get<std::string>();
            if (a.contains("key_path")) cfg.admin.key_path = a["key_path"].get<std::string>();
            if (a.contains("worker_threads")) cfg.admin.worker_threads = a["worker_threads"].get<int>();
            if (a.contains("server_id")) cfg.admin.server_id = a["server_id"].get<std::string>();
        }
        if (!check(j, "admin_server", err) || cfg.admin.port <= 0) {
            if (err.empty()) err = "invalid admin_server port";
            return cfg;
        }

        if (j.contains("capture")) {
            const auto& c = j["capture"];
            if (c.contains("fps")) cfg.capture.fps = c["fps"].get<int>();
            if (c.contains("data_source_dir")) cfg.capture.data_source_dir = c["data_source_dir"].get<std::string>();
            if (c.contains("stereo_image_dir")) cfg.capture.stereo_image_dir = c["stereo_image_dir"].get<std::string>();
            if (c.contains("image_width")) cfg.capture.image_width = c["image_width"].get<int>();
            if (c.contains("image_height")) cfg.capture.image_height = c["image_height"].get<int>();
            if (c.contains("image_channels")) cfg.capture.image_channels = c["image_channels"].get<int>();
        }
        if (cfg.capture.fps <= 0 || cfg.capture.fps > 240) {
            err = "invalid capture.fps (must be 1..240)";
            return cfg;
        }

        if (j.contains("zmq")) {
            const auto& z = j["zmq"];
            if (z.contains("transport")) cfg.zmq.transport = z["transport"].get<std::string>();
            if (z.contains("hwm")) cfg.zmq.hwm = z["hwm"].get<int>();
            if (z.contains("immediate")) cfg.zmq.immediate = z["immediate"].get<bool>();
            if (z.contains("linger_ms")) cfg.zmq.linger_ms = z["linger_ms"].get<int>();
            if (z.contains("pub_id")) cfg.zmq.pub_id = z["pub_id"].get<std::string>();
            if (z.contains("endpoints")) {
                for (auto it = z["endpoints"].begin(); it != z["endpoints"].end(); ++it) {
                    cfg.zmq.endpoints[it.key()] = it.value().get<std::string>();
                }
            }
        }
        if (cfg.zmq.hwm < 1) cfg.zmq.hwm = 1;
        if (cfg.zmq.endpoints.size() < 3) {
            err = "zmq.endpoints must define visual_2d, visual_3d, sensor_data";
            return cfg;
        }

        if (j.contains("spsc")) {
            const auto& s = j["spsc"];
            if (s.contains("queue_size")) cfg.spsc.queue_size = s["queue_size"].get<int>();
            if (s.contains("drop_policy")) cfg.spsc.drop_policy = s["drop_policy"].get<std::string>();
            if (s.contains("cv_timeout_ms")) cfg.spsc.cv_timeout_ms = s["cv_timeout_ms"].get<int>();
        }
        if (cfg.spsc.queue_size < 1) cfg.spsc.queue_size = 1;

        if (j.contains("log_level")) cfg.log_level = j["log_level"].get<std::string>();
    } catch (const std::exception& e) {
        err = std::string("config field error: ") + e.what();
        return cfg;
    }

    SIM_LOG(LogLevel::INFO, "Config",
            "loaded config v" + std::to_string(cfg.version) +
                " | admin port=" + std::to_string(cfg.admin.port) +
                " | fps=" + std::to_string(cfg.capture.fps) +
                " | hwm=" + std::to_string(cfg.zmq.hwm));
    return cfg;
}

}  // namespace sim_vision
