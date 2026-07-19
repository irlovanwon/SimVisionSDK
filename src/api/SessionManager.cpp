/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Session manager — issues client ids and tracks connected clients
 * Date: 20260719
 * Modification:
 */
#include "sim_vision/api/SessionManager.h"

namespace sim_vision {

int64_t SessionManager::connect() {
    std::lock_guard<std::mutex> lk(mtx_);
    int64_t id = next_id_++;
    sessions_[id] = true;
    return id;
}

bool SessionManager::disconnect(int64_t client_id) {
    std::lock_guard<std::mutex> lk(mtx_);
    return sessions_.erase(client_id) > 0;
}

bool SessionManager::is_connected(int64_t client_id) const {
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = sessions_.find(client_id);
    return it != sessions_.end() && it->second;
}

int SessionManager::session_count() const {
    std::lock_guard<std::mutex> lk(mtx_);
    int n = 0;
    for (const auto& kv : sessions_) {
        if (kv.second) ++n;
    }
    return n;
}

}  // namespace sim_vision
