/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Response envelope JSON serialization and HTTP status mapping
 * Date: 20260719
 * Modification:
 */
#include "sim_vision/common/Response.h"

namespace sim_vision {

std::string Response::to_json() const {
    nlohmann::json j;
    j["code"] = static_cast<int>(code);
    j["message"] = message;
    j["data"] = data;
    return j.dump();
}

Response Response::from_error(Code c, const std::string& message) {
    Response r;
    r.code = c;
    r.message = message;
    return r;
}

int Response::http_status(Code code) {
    switch (code) {
        case Code::Success:     return 200;
        case Code::AlreadyInit: return 200;
        case Code::Error:       return 500;
        case Code::NotReady:    return 400;
        case Code::InvalidParam:return 400;
        case Code::Unavailable: return 404;
    }
    return 500;
}

}  // namespace sim_vision
