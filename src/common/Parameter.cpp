/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Parameter storage with range validation and enum enforcement
 * Date: 20260719
 * Modification:
 */
#include "sim_vision/common/Parameter.h"

#include <algorithm>

namespace sim_vision {

const char* param_variant_type_name(const ParamValue& v) {
    if (std::holds_alternative<param_type::Integer>(v)) return "Integer";
    if (std::holds_alternative<param_type::Float>(v))   return "Float";
    return "Enum";
}

namespace {
bool same_type(const ParamValue& a, const ParamValue& b) {
    return a.index() == b.index();
}

template <typename T>
bool in_range(const T& v, const ParamValue& lo, const ParamValue& hi) {
    if (!std::holds_alternative<T>(lo) || !std::holds_alternative<T>(hi)) return true;
    return v >= std::get<T>(lo) && v <= std::get<T>(hi);
}

bool enum_allowed(const std::vector<std::string>& vals, const std::string& v) {
    return vals.empty() ||
           std::find(vals.begin(), vals.end(), v) != vals.end();
}
}  // namespace

void ParameterManager::register_parameter(const Parameter& p) {
    std::lock_guard<std::mutex> lk(mtx_);
    params_[p.name] = p;
}

bool ParameterManager::set_value(const std::string& name, const ParamValue& value,
                                 std::string& err) {
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = params_.find(name);
    if (it == params_.end()) {
        err = "unknown parameter: " + name;
        return false;
    }
    Parameter& p = it->second;
    if (p.is_readonly) {
        err = "parameter is read-only: " + name;
        return false;
    }
    if (!same_type(p.value, value)) {
        err = "type mismatch for parameter: " + name;
        return false;
    }
    if (std::holds_alternative<param_type::Integer>(value)) {
        if (!in_range<param_type::Integer>(std::get<param_type::Integer>(value), p.min, p.max)) {
            err = "value out of range for parameter: " + name;
            return false;
        }
    } else if (std::holds_alternative<param_type::Float>(value)) {
        if (!in_range<param_type::Float>(std::get<param_type::Float>(value), p.min, p.max)) {
            err = "value out of range for parameter: " + name;
            return false;
        }
    } else {
        const auto& ev = std::get<param_type::Enum>(value);
        if (!enum_allowed(p.enum_values, ev)) {
            err = "invalid enum value for parameter: " + name;
            return false;
        }
    }
    p.value = value;
    return true;
}

bool ParameterManager::get(const std::string& name, Parameter& out) const {
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = params_.find(name);
    if (it == params_.end()) return false;
    out = it->second;
    return true;
}

std::vector<Parameter> ParameterManager::all() const {
    std::lock_guard<std::mutex> lk(mtx_);
    std::vector<Parameter> v;
    v.reserve(params_.size());
    for (const auto& kv : params_) v.push_back(kv.second);
    return v;
}

std::vector<std::string> ParameterManager::names() const {
    std::lock_guard<std::mutex> lk(mtx_);
    std::vector<std::string> v;
    v.reserve(params_.size());
    for (const auto& kv : params_) v.push_back(kv.first);
    return v;
}

bool ParameterManager::has(const std::string& name) const {
    std::lock_guard<std::mutex> lk(mtx_);
    return params_.count(name) > 0;
}

}  // namespace sim_vision
