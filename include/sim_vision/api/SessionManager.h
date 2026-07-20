/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Session manager — issues client ids, tracks connected clients
 * Date: 20260719
 * Modification:
 */
#pragma once

#include <mutex>
#include <set>
#include <string>

namespace sim_vision {

class SessionManager {
public:
    // Returns true if newly connected; false if already connected.
    bool connect(const std::string& client_id);
    bool disconnect(const std::string& client_id);
    bool is_connected(const std::string& client_id) const;
    int session_count() const;

private:
    mutable std::mutex mtx_;
    std::set<std::string> sessions_;
};

}  // namespace sim_vision
