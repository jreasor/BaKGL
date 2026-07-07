#include "graphics/vramTracker.hpp"

#include <gtest/gtest.h>

using namespace Graphics;

// GL-free VRAM accounting math (ROADMAP Task 3.3 increment A). The singleton
// accumulates across the whole test process, so every assertion is a delta
// against a captured baseline.

TEST(VramBudgetTest, NearestArrayIsBaseRgba8)
{
    // No mipchain: maxDim^2 * layers * 4 B/texel.
    EXPECT_EQ(ResidentBytes(2048, 1, false), 2048ull * 2048 * 4);
    EXPECT_EQ(ResidentBytes(512, 3, false), 512ull * 512 * 3 * 4);
}

TEST(VramBudgetTest, MipchainExactForPowerOfTwo)
{
    // 2048 full mipchain: 2048^2+1024^2+...+1^2 = 5592405 texels/layer * 4 B * 2 layers.
    constexpr unsigned long long kTexelsPerLayer = 5592405ull;
    EXPECT_EQ(ResidentBytes(2048, 2, true), kTexelsPerLayer * 4 * 2);
    // The exact sum is the truncated geometric series, so it is <= 4/3*base
    // (minus the series tail, a few bytes) and strictly greater than the base.
    const auto base = 2048ull * 2048 * 4 * 2;
    EXPECT_LE(ResidentBytes(2048, 2, true), base * 4 / 3);
    EXPECT_GT(ResidentBytes(2048, 2, true), base);
}

TEST(VramBudgetTest, MipchainBoundedByFourThirds)
{
    // Non-power-of-two: exact sum is a slight upper bound on the real allocation
    // but never exceeds 4/3 * base (within a texel of rounding).
    const auto exact = ResidentBytes(100, 1, true);
    const auto base = 100ull * 100 * 4;
    EXPECT_LE(exact, base * 4 / 3 + 4);
    EXPECT_GT(exact, base); // strictly more than the base level (has mips)
}

TEST(VramBudgetTest, EmptyStoreIsZero)
{
    EXPECT_EQ(ResidentBytes(0, 1, false), 0ull);
    EXPECT_EQ(ResidentBytes(0, 1, true), 0ull);
    EXPECT_EQ(ResidentBytes(2048, 0, true), 0ull);
}

TEST(VramBudgetTest, TrackerAccountsCalls)
{
    auto& t = VramTracker::Get();
    const auto bytesBefore = t.GetTotalBytes();
    const auto callsBefore = t.GetTotalCalls();
    const auto layersBefore = t.GetTotalLayers();

    t.Account(32, 2, false, "ut-a");
    t.AccountViewport(64, 64, 16, "ut-pick");

    EXPECT_EQ(t.GetTotalCalls() - callsBefore, 2u);
    EXPECT_EQ(t.GetTotalLayers() - layersBefore, 2ull);
    EXPECT_EQ(t.GetTotalBytes() - bytesBefore,
              ResidentBytes(32, 2, false) + 64ull * 64 * 16);
}