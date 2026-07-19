/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Unit tests for ChannelManager subscriber tracking
 * Date: 20260719
 * Modification:
 */
#include "sim_vision/data/ChannelManager.h"

#include <gtest/gtest.h>

using namespace sim_vision;

TEST(ChannelManager, StartStopCapture) {
    ChannelManager cm;
    EXPECT_FALSE(cm.is_active(DataType::StereoImage));
    cm.start_capture(1, {DataType::StereoImage, DataType::DepthMap});
    EXPECT_TRUE(cm.is_active(DataType::StereoImage));
    EXPECT_TRUE(cm.is_active(DataType::DepthMap));
    EXPECT_EQ(cm.subscriber_count(DataType::StereoImage), 1);
    cm.stop_capture(1, {DataType::StereoImage});
    EXPECT_FALSE(cm.is_active(DataType::StereoImage));
    EXPECT_TRUE(cm.is_active(DataType::DepthMap));
}

TEST(ChannelManager, MultiClientRefcount) {
    ChannelManager cm;
    cm.start_capture(1, {DataType::StereoImage});
    cm.start_capture(2, {DataType::StereoImage});
    EXPECT_EQ(cm.subscriber_count(DataType::StereoImage), 2);
    cm.stop_capture(1, {DataType::StereoImage});
    EXPECT_TRUE(cm.is_active(DataType::StereoImage));
    EXPECT_EQ(cm.subscriber_count(DataType::StereoImage), 1);
    cm.stop_capture(2, {DataType::StereoImage});
    EXPECT_FALSE(cm.is_active(DataType::StereoImage));
}

TEST(ChannelManager, DisconnectClient) {
    ChannelManager cm;
    cm.start_capture(5, {DataType::IMU, DataType::Temperature});
    EXPECT_TRUE(cm.is_active(DataType::IMU));
    EXPECT_TRUE(cm.disconnect_client(5));
    EXPECT_FALSE(cm.is_active(DataType::IMU));
    EXPECT_FALSE(cm.is_active(DataType::Temperature));
}

TEST(ChannelManager, ForceActivate) {
    ChannelManager cm;
    EXPECT_FALSE(cm.is_active(DataType::PointCloud));
    cm.activate_channel(DataType::PointCloud);
    EXPECT_TRUE(cm.is_active(DataType::PointCloud));
    EXPECT_TRUE(cm.group_active(DataGroup::Visual3D));
    cm.deactivate_channel(DataType::PointCloud);
    EXPECT_FALSE(cm.is_active(DataType::PointCloud));
}

TEST(ChannelManager, GroupActive) {
    ChannelManager cm;
    EXPECT_FALSE(cm.group_active(DataGroup::SensorData));
    cm.start_capture(9, {DataType::Barometer});
    EXPECT_TRUE(cm.group_active(DataGroup::SensorData));
    EXPECT_TRUE(cm.last_subscriber_left(DataGroup::Visual2D));
}
