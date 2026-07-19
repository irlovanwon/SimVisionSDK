/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Session manager — issues client ids, tracks connected clients
 * Date: 20260719
 * Modification:
 */
#pragma once

#include <cstdint>
#include <map>
#include <mutex>

namespace sim_vision {

class SessionManager {
public:
    int64_t connect();
    bool disconnect(int64_t client_id);
    bool is_connected(int64_t client_id) const;
    int session_count() const;

private:
    mutable std::mutex mtx_;
    int64_t next_id_ = 1;
    std::map<int64_t, bool> sessions_;
};

}  // namespace sim_vision
