/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Parameter model with value/min/max/default + readonly/available/restart flags
 * Date: 20260719
 * Modification:
 */
#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace sim_vision {

namespace param_type {
using Integer = int64_t;
using Float   = double;
using Enum    = std::string;
}  // namespace param_type

using ParamValue = std::variant<param_type::Integer, param_type::Float, param_type::Enum>;

const char* param_variant_type_name(const ParamValue& v);

struct Parameter {
    std::string name;
    ParamValue value;
    ParamValue min;
    ParamValue max;
    ParamValue default_value;
    bool is_readonly = false;
    bool is_available = true;
    bool needs_restart = false;
    std::vector<std::string> enum_values;
};

class ParameterManager {
public:
    void register_parameter(const Parameter& p);
    bool set_value(const std::string& name, const ParamValue& value, std::string& err);
    bool get(const std::string& name, Parameter& out) const;
    std::vector<Parameter> all() const;
    std::vector<std::string> names() const;
    bool has(const std::string& name) const;

private:
    mutable std::mutex mtx_;
    std::unordered_map<std::string, Parameter> params_;
};

}  // namespace sim_vision
