#include "gtest/gtest.h"

#include "bak/file/fileBuffer.hpp"
#include "bak/startupFiles.hpp"

namespace BAK {

// Builds a synthetic ZxxDEF.DAT record with distinctive, non-zero field
// values so any endian/offset/order regression in ParseZoneDefDat is caught.
// The 18-field layout mirrors LoadZoneDefDatInfo's read order exactly
// (bak/startupFiles.cpp); only 8 of the fields are exposed via ZoneDefInfo.
TEST(ZoneDefInfo, ParseRoundTrip)
{
    auto fb = FileBuffer{52};

    fb.PutUint16LE(0x1122);            // zoneType           (exposed)
    fb.PutUint16LE(0x3344);            // threeDParam        (exposed)
    fb.PutUint32LE(0xDEADBEEF);        // playerPos_fieldA   (skipped)
    fb.PutUint16LE(0x99AA);            // playerPos_fieldE   (skipped)
    fb.PutUint16LE(0x5566);            // horizonDisplayType (exposed)
    fb.PutUint8 (0x77);                // groundType         (exposed)
    fb.PutUint8 (0x88);                // groundHeight       (exposed)
    fb.PutUint32LE(0x11111111);        // minMapZoom         (exposed)
    fb.PutUint32LE(0xCAFEBABE);        // unknown8           (skipped)
    fb.PutUint32LE(0x22222222);        // maxMapZoom         (exposed)
    fb.PutUint32LE(0x33333333);        // mapZoomRate        (exposed)
    fb.PutUint16LE(0x0011);            // unknown11          (skipped)
    fb.PutUint16LE(0x0022);            // unknown12          (skipped)
    fb.PutUint32LE(0x0000000D);        // unknown13          (skipped)
    fb.PutUint32LE(0x0000000E);        // unknown14          (skipped)
    fb.PutUint16LE(0x000F);            // unknown15          (skipped)
    fb.PutUint32LE(0x00000010);        // unknown16          (skipped)
    fb.PutUint32LE(0x00000011);        // unknown17          (skipped)

    fb.Rewind();

    const auto info = ParseZoneDefDat(fb);

    EXPECT_EQ(info.mZoneType,           0x1122);
    EXPECT_EQ(info.mThreeDParam,        0x3344);
    EXPECT_EQ(info.mHorizonDisplayType, 0x5566);
    EXPECT_EQ(info.mGroundType,         0x77u);
    EXPECT_EQ(info.mGroundHeight,       0x88u);
    EXPECT_EQ(info.mMinMapZoom,         0x11111111);
    EXPECT_EQ(info.mMaxMapZoom,         0x22222222);
    EXPECT_EQ(info.mMapZoomRate,        0x33333333);

    // The reader must consume exactly the 52 bytes laid out above; a drift
    // in the parse layout (a field added/removed/resized) is a regression.
    EXPECT_EQ(fb.GetBytesLeft(), 0);
}

}