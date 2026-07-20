/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Subscriber-driven channel activation (per-client refcount + force-active)
 * Date: 20260719
 * Modification:
 */
#include "sim_vision/data/ChannelManager.h"

namespace sim_vision {

namespace {
int dt_key(DataType t) { return static_cast<int>(t); }
}

void ChannelManager::start_capture(const std::string& client_id, const std::vector<DataType>& types) {
    std::lock_guard<std::mutex> lk(mtx_);
    auto& subs = clients_[client_id];
    for (DataType t : types) {
        int k = dt_key(t);
        if (subs.insert(k).second) {
            states_[k].subscriber_count += 1;
        }
    }
}

void ChannelManager::stop_capture(const std::string& client_id, const std::vector<DataType>& types) {
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = clients_.find(client_id);
    if (it == clients_.end()) return;
    for (DataType t : types) {
        int k = dt_key(t);
        if (it->second.erase(k) > 0) {
            auto sit = states_.find(k);
            if (sit != states_.end() && sit->second.subscriber_count > 0) {
                sit->second.subscriber_count -= 1;
            }
        }
    }
}

bool ChannelManager::disconnect_client(const std::string& client_id) {
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = clients_.find(client_id);
    if (it == clients_.end()) return false;
    for (int k : it->second) {
        auto sit = states_.find(k);
        if (sit != states_.end() && sit->second.subscriber_count > 0) {
            sit->second.subscriber_count -= 1;
        }
    }
    clients_.erase(it);
    return true;
}

bool ChannelManager::activate_channel(DataType t) {
    std::lock_guard<std::mutex> lk(mtx_);
    bool was = states_[dt_key(t)].force_active;
    states_[dt_key(t)].force_active = true;
    return !was;
}

bool ChannelManager::deactivate_channel(DataType t) {
    std::lock_guard<std::mutex> lk(mtx_);
    bool was = states_[dt_key(t)].force_active;
    states_[dt_key(t)].force_active = false;
    return was;
}

bool ChannelManager::is_active(DataType t) const {
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = states_.find(dt_key(t));
    if (it == states_.end()) return false;
    return it->second.subscriber_count > 0 || it->second.force_active;
}

bool ChannelManager::group_active(DataGroup g) const {
    for (DataType t : group_members(g)) {
        if (is_active(t)) return true;
    }
    return false;
}

std::vector<DataType> ChannelManager::active_types() const {
    std::vector<DataType> v;
    for (DataType t : all_data_types()) {
        if (is_active(t)) v.push_back(t);
    }
    return v;
}

std::vector<DataType> ChannelManager::forced_types() const {
    std::vector<DataType> v;
    std::lock_guard<std::mutex> lk(mtx_);
    for (const auto& kv : states_) {
        if (kv.second.force_active) v.push_back(static_cast<DataType>(kv.first));
    }
    return v;
}

int ChannelManager::subscriber_count(DataType t) const {
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = states_.find(dt_key(t));
    return it == states_.end() ? 0 : it->second.subscriber_count;
}

int ChannelManager::total_clients() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return static_cast<int>(clients_.size());
}

bool ChannelManager::any_subscriber() const {
    std::lock_guard<std::mutex> lk(mtx_);
    for (const auto& kv : states_) {
        if (kv.second.subscriber_count > 0) return true;
    }
    return false;
}

bool ChannelManager::last_subscriber_left(DataGroup g) const {
    for (DataType t : group_members(g)) {
        if (is_active(t)) return false;
    }
    return true;
}

nlohmann::json ChannelManager::status_json() const {
    std::lock_guard<std::mutex> lk(mtx_);
    nlohmann::json channels = nlohmann::json::array();
    for (const auto& kv : states_) {
        auto t = static_cast<DataType>(kv.first);
        nlohmann::json c;
        c["data_type"] = data_type_to_string(t);
        c["group"] = group_to_string(data_type_to_group(t));
        c["subscribers"] = kv.second.subscriber_count;
        c["activated"] = (kv.second.subscriber_count > 0 || kv.second.force_active);
        c["force_active"] = kv.second.force_active;
        channels.push_back(std::move(c));
    }
    nlohmann::json j;
    j["total_clients"] = clients_.size();
    j["channels"] = channels;
    return j;
}

}  // namespace sim_vision
