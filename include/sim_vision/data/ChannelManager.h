/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Subscriber-driven channel activation — per-client refcount + force-active control
 * Date: 20260719
 * Modification:
 */
#pragma once

#include <nlohmann/json.hpp>

#include "sim_vision/common/Types.h"

#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace sim_vision {

class ChannelManager {
public:
    void start_capture(const std::string& client_id, const std::vector<DataType>& types);
    void stop_capture(const std::string& client_id, const std::vector<DataType>& types);
    bool disconnect_client(const std::string& client_id);
    bool activate_channel(DataType t);
    bool deactivate_channel(DataType t);

    bool is_active(DataType t) const;
    bool group_active(DataGroup g) const;
    std::vector<DataType> active_types() const;
    std::vector<DataType> forced_types() const;
    int subscriber_count(DataType t) const;
    int total_clients() const;
    bool any_subscriber() const;
    bool last_subscriber_left(DataGroup g) const;

    nlohmann::json status_json() const;

private:
    struct State {
        int subscriber_count = 0;
        bool force_active = false;
    };
    mutable std::mutex mtx_;
    std::map<int, State> states_;
    std::map<std::string, std::set<int>> clients_;
};

}  // namespace sim_vision
