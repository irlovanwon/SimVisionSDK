/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Unit tests for lock-free SPSC queue (drop-NEWEST)
 * Date: 20260719
 * Modification:
 */
#include "sim_vision/data/SPSCQueue.h"

#include <gtest/gtest.h>
#include <thread>

using namespace sim_vision;

TEST(SPSCQueue, PushPopBasic) {
    ChannelFrameQueue q(4);
    auto f = std::make_shared<ChannelFrame>();
    f->frame_index = 7;
    EXPECT_TRUE(q.try_push(f));
    ChannelFramePtr out;
    EXPECT_TRUE(q.try_pop(out));
    ASSERT_TRUE(out != nullptr);
    EXPECT_EQ(out->frame_index, 7u);
    EXPECT_TRUE(q.empty());
}

TEST(SPSCQueue, DropNewestWhenFull) {
    ChannelFrameQueue q(3);
    for (size_t i = 0; i < 3; ++i) {
        auto f = std::make_shared<ChannelFrame>();
        f->frame_index = i;
        ASSERT_TRUE(q.try_push(f));
    }
    auto f = std::make_shared<ChannelFrame>();
    f->frame_index = 99;
    EXPECT_FALSE(q.try_push(f));
    EXPECT_EQ(q.total_dropped(), 1u);

    ChannelFramePtr out;
    ASSERT_TRUE(q.try_pop(out));
    EXPECT_EQ(out->frame_index, 0u);
}

TEST(SPSCQueue, ProducerConsumerStress) {
    ChannelFrameQueue q(64);
    constexpr int N = 5000;
    std::thread prod([&] {
        for (int i = 0; i < N; ++i) {
            auto f = std::make_shared<ChannelFrame>();
            f->frame_index = i;
            while (!q.try_push(f)) {
                std::this_thread::yield();
            }
        }
    });
    uint64_t last = 0;
    int count = 0;
    std::thread cons([&] {
        ChannelFramePtr out;
        while (count < N) {
            if (q.try_pop(out)) {
                EXPECT_GE(out->frame_index, last);
                last = out->frame_index;
                ++count;
            } else {
                std::this_thread::yield();
            }
        }
    });
    prod.join();
    cons.join();
    EXPECT_EQ(count, N);
}
