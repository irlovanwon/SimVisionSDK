/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: API response envelope matching ZEDVisionSDK contract {code, message, detail,
 *      server_id, client_id, recv_timestamp, send_timestamp} with HTTP status mapping
 * Date: 20260719
 * Modification:
 */
#pragma once

#include <nlohmann/json.hpp>
#include <cstdint>
#include <string>

namespace sim_vision {

enum class Code : int {
    Success        = 0,
    Error          = 1,
    NotReady       = 2,
    AlreadyInit    = 3,
    InvalidParam   = 4,
    Unavailable    = 5,
};

struct Response {
    Code code = Code::Success;
    std::string message;
    nlohmann::json detail = nlohmann::json::object();

    // Per-frame metadata (populated by AdminServer before serialization)
    std::string server_id;
    std::string client_id;
    int64_t recv_sec = 0;
    int64_t recv_nsec = 0;
    int64_t send_sec = 0;
    int64_t send_nsec = 0;

    std::string to_json() const;
    static Response from_error(Code code, const std::string& message);
    static int http_status(Code code);
};

}  // namespace sim_vision
