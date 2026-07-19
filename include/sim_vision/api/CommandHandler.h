/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Command dispatcher — implements all client workflow + query + lifecycle commands
 * Date: 20260719
 * Modification:
 */
#pragma once

#include <nlohmann/json.hpp>

#include "sim_vision/common/Response.h"
#include "sim_vision/common/Types.h"

#include <cstdint>
#include <string>

namespace sim_vision {

class SessionManager;
class ChannelManager;
class ParameterManager;
struct Config;
class DataPublisher;
class DataPipeline;
class SimDataSource;
class CaptureEngine;

class CommandHandler {
public:
    CommandHandler(SessionManager& sessions, ChannelManager& channels,
                   ParameterManager& params, const Config& config,
                   DataPublisher* publisher, DataPipeline* pipeline,
                   SimDataSource* source, CaptureEngine* capture);

    Response handle(const std::string& command, const nlohmann::json& body,
                    int64_t client_id);

    static std::string normalize_command(const std::string& path,
                                         const nlohmann::json& body);

private:
    Response cmd_connect(const nlohmann::json& body);
    Response cmd_disconnect(const nlohmann::json& body, int64_t client_id);
    Response cmd_start_capture(const nlohmann::json& body, int64_t client_id);
    Response cmd_stop_capture(const nlohmann::json& body, int64_t client_id);
    Response cmd_activate_channel(const nlohmann::json& body);
    Response cmd_deactivate_channel(const nlohmann::json& body);
    Response cmd_check_status() const;
    Response cmd_list_channels() const;
    Response cmd_list_parameters() const;
    Response cmd_get_parameter(const nlohmann::json& body) const;
    Response cmd_set_parameter(const nlohmann::json& body);

    SessionManager& sessions_;
    ChannelManager& channels_;
    ParameterManager& params_;
    const Config& config_;
    DataPublisher* publisher_;
    DataPipeline* pipeline_;
    SimDataSource* source_;
    CaptureEngine* capture_;
};

}  // namespace sim_vision
