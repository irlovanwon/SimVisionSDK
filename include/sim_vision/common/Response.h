/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Unified API response envelope {code, message, data} with HTTP status mapping
 * Date: 20260719
 * Modification:
 */
#pragma once

#include <nlohmann/json.hpp>
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
    nlohmann::json data = nlohmann::json::object();

    std::string to_json() const;
    static Response from_error(Code code, const std::string& message);
    static int http_status(Code code);
};

}  // namespace sim_vision
