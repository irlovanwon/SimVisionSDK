/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Command dispatcher — all client workflow + query + lifecycle commands
 * Date: 20260719
 * Modification:
 */
#include "sim_vision/api/CommandHandler.h"

#include "sim_vision/common/Logger.h"
#include "sim_vision/data/ChannelManager.h"
#include "sim_vision/data/DataPipeline.h"
#include "sim_vision/data/DataPublisher.h"
#include "sim_vision/datasource/SimDataSource.h"
#include "sim_vision/capture/CaptureEngine.h"
#include "sim_vision/api/SessionManager.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <map>

namespace sim_vision {

namespace {

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

const std::map<std::string, std::string>& alias_map() {
    static const std::map<std::string, std::string> m = {
        {"checkstatus", "check_status"},
        {"listchannels", "list_channels"},
        {"listparameters", "list_parameters"},
        {"getparameter", "get_parameter"},
        {"setparameter", "set_parameter"},
        {"activatechannel", "activate_channel"},
        {"deactivatechannel", "deactivate_channel"},
        {"startcapture", "start_capture"},
        {"stopcapture", "stop_capture"},
    };
    return m;
}

nlohmann::json value_to_json(const ParamValue& v) {
    if (std::holds_alternative<param_type::Integer>(v)) return std::get<param_type::Integer>(v);
    if (std::holds_alternative<param_type::Float>(v))   return std::get<param_type::Float>(v);
    return std::get<param_type::Enum>(v);
}

nlohmann::json param_to_json(const Parameter& p) {
    nlohmann::json j;
    j["name"] = p.name;
    j["type"] = param_variant_type_name(p.value);
    j["value"] = value_to_json(p.value);
    j["default"] = value_to_json(p.default_value);
    j["is_readonly"] = p.is_readonly;
    j["is_available"] = p.is_available;
    j["needs_reopen"] = p.needs_reopen;
    if (std::holds_alternative<param_type::Integer>(p.value) ||
        std::holds_alternative<param_type::Float>(p.value)) {
        j["min"] = value_to_json(p.min);
        j["max"] = value_to_json(p.max);
    }
    if (!p.enum_options.empty()) j["enum_options"] = p.enum_options;
    return j;
}

std::vector<DataType> expand_types(const nlohmann::json& body) {
    std::vector<DataType> out;
    if (!body.contains("data_types") || !body["data_types"].is_array()) {
        if (body.contains("data_type") && body["data_type"].is_string()) {
            DataType t;
            if (data_type_from_string(body["data_type"].get<std::string>(), t)) out.push_back(t);
        }
        return out;
    }
    for (const auto& item : body["data_types"]) {
        if (!item.is_string()) continue;
        std::string s = item.get<std::string>();
        std::string ls = to_lower(s);
        if (ls == "visual_2d" || ls == "visual_geometric_2d") {
            for (DataType t : group_members(DataGroup::Visual2D)) out.push_back(t);
        } else if (ls == "visual_3d" || ls == "visual_geometric_3d") {
            for (DataType t : group_members(DataGroup::Visual3D)) out.push_back(t);
        } else if (ls == "sensor_data" || ls == "sensor_tracking") {
            for (DataType t : group_members(DataGroup::SensorData)) out.push_back(t);
        } else {
            DataType t;
            if (data_type_from_string(ls, t)) out.push_back(t);
        }
    }
    return out;
}

}  // namespace

CommandHandler::CommandHandler(SessionManager& sessions, ChannelManager& channels,
                               ParameterManager& params, const Config& config,
                               DataPublisher* publisher, DataPipeline* pipeline,
                               SimDataSource* source, CaptureEngine* capture)
    : sessions_(sessions),
      channels_(channels),
      params_(params),
      config_(config),
      publisher_(publisher),
      pipeline_(pipeline),
      source_(source),
      capture_(capture) {}

std::string CommandHandler::normalize_command(const std::string& path,
                                              const nlohmann::json& body) {
    std::string cmd;
    std::string p = path;
    if (!p.empty() && p.front() == '/') p = p.substr(1);
    // StereoCamera routes camera-SDK calls under an /api/ prefix (e.g.
    // /api/get_parameter). Strip any leading "api/" segment and take the last
    // path segment as the command — matches ZEDVisionSDK routing.
    if (p.rfind("api/", 0) == 0) p = p.substr(4);
    if (!p.empty()) {
        size_t slash = p.find('/');
        if (slash != std::string::npos) p = p.substr(slash + 1);
        cmd = to_lower(p);
        const auto& am = alias_map();
        auto it = am.find(cmd);
        if (it != am.end()) cmd = it->second;
    }
    if (cmd.empty() && body.contains("command") && body["command"].is_string()) {
        cmd = to_lower(body["command"].get<std::string>());
        const auto& am = alias_map();
        auto it = am.find(cmd);
        if (it != am.end()) cmd = it->second;
    }
    return cmd;
}

Response CommandHandler::handle(const std::string& command, const nlohmann::json& body,
                                const std::string& client_id) {
    if (command == "connect") return cmd_connect(client_id);
    if (command == "disconnect") return cmd_disconnect(client_id);
    if (command == "start_capture") return cmd_start_capture(body, client_id);
    if (command == "stop_capture") return cmd_stop_capture(body, client_id);
    if (command == "activate_channel") return cmd_activate_channel(body);
    if (command == "deactivate_channel") return cmd_deactivate_channel(body);
    if (command == "check_status") return cmd_check_status();
    if (command == "list_channels") return cmd_list_channels();
    if (command == "list_parameters") return cmd_list_parameters();
    if (command == "get_parameter") return cmd_get_parameter(body);
    if (command == "set_parameter") return cmd_set_parameter(body);
    if (command == "init") return Response::from_error(Code::AlreadyInit, "Already initialized");
    if (command == "dispose") return Response::from_error(Code::Error, "Not permitted via API");
    return Response::from_error(Code::InvalidParam, "unknown command: " + command);
}

Response CommandHandler::cmd_connect(const std::string& client_id) {
    Response r;
    if (sessions_.is_connected(client_id)) {
        r.code = Code::AlreadyInit;
        r.message = "Already connected";
        return r;
    }
    sessions_.connect(client_id);
    r.code = Code::Success;
    r.message = "Connected";
    SIM_LOG(LogLevel::INFO, "CommandHandler", "connect client_id=" + client_id);
    return r;
}

Response CommandHandler::cmd_disconnect(const std::string& client_id) {
    if (!sessions_.is_connected(client_id)) {
        return Response::from_error(Code::NotReady, "Not connected");
    }
    channels_.disconnect_client(client_id);
    sessions_.disconnect(client_id);
    if (!channels_.any_subscriber()) {
        if (pipeline_) pipeline_->drain();
        if (publisher_) publisher_->drain();
        SIM_LOG(LogLevel::INFO, "CommandHandler", "last subscriber left — stale data flushed");
    }
    Response r;
    r.code = Code::Success;
    r.message = "Disconnected";
    return r;
}

Response CommandHandler::cmd_start_capture(const nlohmann::json& body, const std::string& client_id) {
    if (!sessions_.is_connected(client_id)) {
        return Response::from_error(Code::NotReady, "not connected — call connect first");
    }
    auto types = expand_types(body);
    if (types.empty()) {
        return Response::from_error(Code::InvalidParam, "no valid data_types");
    }
    channels_.start_capture(client_id, types);
    Response r;
    r.code = Code::Success;
    r.message = "Capture started";
    for (DataType t : types) r.detail["data_types"].push_back(data_type_to_string(t));
    return r;
}

Response CommandHandler::cmd_stop_capture(const nlohmann::json& body, const std::string& client_id) {
    auto types = expand_types(body);
    if (types.empty()) {
        return Response::from_error(Code::InvalidParam, "no valid data_types");
    }
    channels_.stop_capture(client_id, types);
    if (!channels_.any_subscriber()) {
        if (pipeline_) pipeline_->drain();
        if (publisher_) publisher_->drain();
    }
    Response r;
    r.code = Code::Success;
    r.message = "Capture stopped";
    for (DataType t : types) r.detail["data_types"].push_back(data_type_to_string(t));
    return r;
}

Response CommandHandler::cmd_activate_channel(const nlohmann::json& body) {
    DataType t;
    if (!body.contains("data_type") || !data_type_from_string(to_lower(body["data_type"].get<std::string>()), t)) {
        return Response::from_error(Code::InvalidParam, "invalid data_type");
    }
    bool changed = channels_.activate_channel(t);
    Response r;
    r.code = Code::Success;
    r.message = changed ? "Channel activated" : "Channel already active";
    r.detail["data_type"] = data_type_to_string(t);
    r.detail["restart_required"] = false;
    return r;
}

Response CommandHandler::cmd_deactivate_channel(const nlohmann::json& body) {
    DataType t;
    if (!body.contains("data_type") || !data_type_from_string(to_lower(body["data_type"].get<std::string>()), t)) {
        return Response::from_error(Code::InvalidParam, "invalid data_type");
    }
    channels_.deactivate_channel(t);
    if (!channels_.group_active(data_type_to_group(t))) {
        if (pipeline_) pipeline_->drain_group(data_type_to_group(t));
    }
    Response r;
    r.code = Code::Success;
    r.message = "Channel deactivated";
    r.detail["data_type"] = data_type_to_string(t);
    r.detail["restart_required"] = true;
    return r;
}

Response CommandHandler::cmd_check_status() const {
    Response r;
    r.code = Code::Success;
    r.message = "Status";
    r.detail["initialized"] = true;
    r.detail["sessions"] = sessions_.session_count();
    r.detail["simulator"] = SimDataSource::version_tag();
    if (source_) r.detail["stereo_pairs_loaded"] = source_->pair_count();
    if (capture_) r.detail["frames_produced"] = capture_->frames_produced();
    r.detail["channels"] = channels_.status_json();

    nlohmann::json pub = nlohmann::json::object();
    if (publisher_) {
        auto s = publisher_->stats();
        pub["sent_2d"] = s.sent_2d;
        pub["sent_3d"] = s.sent_3d;
        pub["sent_sensor"] = s.sent_sensor;
        pub["dropped_hwm"] = s.dropped_hwm;
        pub["shutdown_sent"] = s.shutdown_sent;
    }
    r.detail["publisher"] = pub;
    if (pipeline_) {
        auto s = pipeline_->stats();
        nlohmann::json pl;
        pl["pushed_2d"] = s.pushed_2d;
        pl["pushed_3d"] = s.pushed_3d;
        pl["pushed_sensor"] = s.pushed_sensor;
        pl["dropped_2d"] = s.dropped_2d;
        pl["dropped_3d"] = s.dropped_3d;
        pl["dropped_sensor"] = s.dropped_sensor;
        pl["queue_2d"] = s.queue_2d;
        pl["queue_3d"] = s.queue_3d;
        pl["queue_sensor"] = s.queue_sensor;
        r.detail["pipeline"] = pl;
    }
    return r;
}

Response CommandHandler::cmd_list_channels() const {
    Response r;
    r.code = Code::Success;
    r.message = "Channels";
    nlohmann::json arr = nlohmann::json::array();
    for (DataType t : all_data_types()) {
        nlohmann::json c;
        c["data_type"] = data_type_to_string(t);
        c["group"] = group_to_string(data_type_to_group(t));
        c["supported"] = true;
        c["activated"] = channels_.is_active(t);
        c["subscribers"] = channels_.subscriber_count(t);
        arr.push_back(std::move(c));
    }
    r.detail["channels"] = arr;
    return r;
}

Response CommandHandler::cmd_list_parameters() const {
    Response r;
    r.code = Code::Success;
    r.message = "Parameters";
    nlohmann::json arr = nlohmann::json::array();
    for (const Parameter& p : params_.all()) arr.push_back(param_to_json(p));
    r.detail["parameters"] = arr;
    return r;
}

Response CommandHandler::cmd_get_parameter(const nlohmann::json& body) const {
    if (!body.contains("name")) {
        return Response::from_error(Code::InvalidParam, "missing 'name'");
    }
    Parameter p;
    if (!params_.get(body["name"].get<std::string>(), p)) {
        return Response::from_error(Code::InvalidParam, "unknown parameter");
    }
    Response r;
    r.code = Code::Success;
    r.message = "Success";
    r.detail = param_to_json(p);
    return r;
}

Response CommandHandler::cmd_set_parameter(const nlohmann::json& body) {
    if (!body.contains("name") || !body.contains("value")) {
        return Response::from_error(Code::InvalidParam, "missing 'name' or 'value'");
    }
    const std::string name = body["name"].get<std::string>();
    Parameter p;
    if (!params_.get(name, p)) {
        return Response::from_error(Code::InvalidParam, "unknown parameter");
    }
    ParamValue v;
    if (std::holds_alternative<param_type::Integer>(p.value)) {
        v = static_cast<param_type::Integer>(body["value"].get<param_type::Integer>());
    } else if (std::holds_alternative<param_type::Float>(p.value)) {
        v = body["value"].get<param_type::Float>();
    } else {
        v = body["value"].get<std::string>();
    }
    std::string err;
    if (!params_.set_value(name, v, err)) {
        return Response::from_error(Code::InvalidParam, err);
    }
    Response r;
    r.code = Code::Success;
    r.message = "Parameter set";
    r.detail["name"] = name;
    r.detail["value"] = value_to_json(v);
    r.detail["needs_reopen"] = p.needs_reopen;
    return r;
}

}  // namespace sim_vision
