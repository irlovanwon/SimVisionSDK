/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Unit tests for ParameterManager
 * Date: 20260719
 * Modification:
 */
#include "sim_vision/common/Parameter.h"

#include <gtest/gtest.h>

using namespace sim_vision;

static Parameter make_int(const std::string& n, int64_t def, int64_t lo, int64_t hi) {
    Parameter p;
    p.name = n;
    p.value = def;
    p.min = lo;
    p.max = hi;
    p.default_value = def;
    return p;
}

TEST(Parameter, RegisterAndGet) {
    ParameterManager pm;
    pm.register_parameter(make_int("fps", 30, 1, 240));
    Parameter p;
    ASSERT_TRUE(pm.get("fps", p));
    EXPECT_EQ(std::get<param_type::Integer>(p.value), 30);
    EXPECT_TRUE(pm.has("fps"));
    EXPECT_FALSE(pm.has("nope"));
}

TEST(Parameter, SetInRange) {
    ParameterManager pm;
    pm.register_parameter(make_int("fps", 30, 1, 240));
    std::string err;
    EXPECT_TRUE(pm.set_value("fps", param_type::Integer{60}, err));
    Parameter p;
    pm.get("fps", p);
    EXPECT_EQ(std::get<param_type::Integer>(p.value), 60);
}

TEST(Parameter, SetOutOfRangeFails) {
    ParameterManager pm;
    pm.register_parameter(make_int("fps", 30, 1, 240));
    std::string err;
    EXPECT_FALSE(pm.set_value("fps", param_type::Integer{1000}, err));
    EXPECT_FALSE(err.empty());
}

TEST(Parameter, UnknownParamFails) {
    ParameterManager pm;
    std::string err;
    EXPECT_FALSE(pm.set_value("x", param_type::Integer{1}, err));
}

TEST(Parameter, EnumValidation) {
    Parameter p;
    p.name = "depth_mode";
    p.value = std::string("NEURAL");
    p.default_value = std::string("NEURAL");
    p.enum_values = {"NONE", "NEURAL", "ULTRA"};
    ParameterManager pm;
    pm.register_parameter(p);
    std::string err;
    EXPECT_TRUE(pm.set_value("depth_mode", std::string("ULTRA"), err));
    EXPECT_FALSE(pm.set_value("depth_mode", std::string("BAD"), err));
}

TEST(Parameter, TypeMismatchFails) {
    ParameterManager pm;
    pm.register_parameter(make_int("fps", 30, 1, 240));
    std::string err;
    EXPECT_FALSE(pm.set_value("fps", std::string("foo"), err));
}
