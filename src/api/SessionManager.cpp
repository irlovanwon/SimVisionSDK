/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Session manager — tracks connected clients by string client_id
 * Date: 20260719
 * Modification:
 */
#include "sim_vision/api/SessionManager.h"

namespace sim_vision {

bool SessionManager::connect(const std::string& client_id) {
    std::lock_guard<std::mutex> lk(mtx_);
    return sessions_.insert(client_id).second;
}

bool SessionManager::disconnect(const std::string& client_id) {
    std::lock_guard<std::mutex> lk(mtx_);
    return sessions_.erase(client_id) > 0;
}

bool SessionManager::is_connected(const std::string& client_id) const {
    std::lock_guard<std::mutex> lk(mtx_);
    return sessions_.find(client_id) != sessions_.end();
}

int SessionManager::session_count() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return static_cast<int>(sessions_.size());
}

}  // namespace sim_vision
