#include "gtest/gtest.h"

#include "bak/constants.hpp"
#include "bak/entityType.hpp"
#include "bak/overheadMapClassifier.hpp"

#include <cstdint>
#include <vector>

namespace BAK {

// Exercises the ported worldFactory terrain-face branches
// (bak/worldFactory.cpp:438-538) through the pure classifier core, so the
// overhead map's terrain colors are validated without loading a full Zone.
// Each branch is name-prefix + a present per-face color index → Terrain.
TEST(OverheadMapClassifier, TerrainBranches)
{
    const auto C = [](std::vector<std::uint8_t> v) { return v; };

    // t0*: road/path/river faces, else ground.
    EXPECT_EQ(ClassifyTerrainByNameAndColors("t010006", C({1})),    Terrain::Road);
    EXPECT_EQ(ClassifyTerrainByNameAndColors("t0foo",   C({2})),    Terrain::Path);
    EXPECT_EQ(ClassifyTerrainByNameAndColors("t0bar",   C({3})),    Terrain::River);
    EXPECT_EQ(ClassifyTerrainByNameAndColors("t0baz",   C({})),     Terrain::Ground);
    // Priority within t0 is by first match (1 > 2 > 3).
    EXPECT_EQ(ClassifyTerrainByNameAndColors("t0x",     C({3, 1})), Terrain::Road);

    // r0*: river/bank.
    EXPECT_EQ(ClassifyTerrainByNameAndColors("r0river", C({3})),    Terrain::River);
    EXPECT_EQ(ClassifyTerrainByNameAndColors("r0bank",  C({5})),    Terrain::Bank);
    EXPECT_EQ(ClassifyTerrainByNameAndColors("r0none",  C({})),     Terrain::Ground);

    // g0*: river only.
    EXPECT_EQ(ClassifyTerrainByNameAndColors("g0x",     C({5})),    Terrain::River);
    EXPECT_EQ(ClassifyTerrainByNameAndColors("g0y",     C({})),     Terrain::Ground);

    // field*: dirt/bank, default dirt.
    EXPECT_EQ(ClassifyTerrainByNameAndColors("field1",  C({1})),    Terrain::Dirt);
    EXPECT_EQ(ClassifyTerrainByNameAndColors("field2",  C({2})),    Terrain::Bank);
    EXPECT_EQ(ClassifyTerrainByNameAndColors("field3",  C({})),     Terrain::Dirt);

    // fall/spring: waterfall/river/bank.
    EXPECT_EQ(ClassifyTerrainByNameAndColors("fall1",   C({6})),    Terrain::Waterfall);
    EXPECT_EQ(ClassifyTerrainByNameAndColors("fall2",   C({3})),    Terrain::River);
    EXPECT_EQ(ClassifyTerrainByNameAndColors("spring1", C({5})),    Terrain::Bank);
    EXPECT_EQ(ClassifyTerrainByNameAndColors("spring2", C({})),     Terrain::Ground);

    // Plain ground items (ground/zero/one/landscp/...): no feature face.
    EXPECT_EQ(ClassifyTerrainByNameAndColors("ground",  C({1, 2})), Terrain::Ground);
    EXPECT_EQ(ClassifyTerrainByNameAndColors("zero00",  C({})),     Terrain::Ground);
    EXPECT_EQ(ClassifyTerrainByNameAndColors("landscp1",C({9})),    Terrain::Ground);
}

TEST(OverheadMapClassifier, EntityTypeCategories)
{
    // Terrain-layer EntityTypes are drawn as ground quads.
    EXPECT_TRUE(IsOverheadMapTerrain(EntityType::TERRAIN));
    EXPECT_TRUE(IsOverheadMapTerrain(EntityType::EXTERIOR));
    EXPECT_TRUE(IsOverheadMapTerrain(EntityType::HILL));
    EXPECT_TRUE(IsOverheadMapTerrain(EntityType::LANDSCAPE));
    EXPECT_FALSE(IsOverheadMapTerrain(EntityType::BUILDING));
    EXPECT_FALSE(IsOverheadMapTerrain(EntityType::BRIDGE));

    // Structure-layer EntityTypes are drawn as square markers.
    EXPECT_TRUE(IsOverheadMapStructure(EntityType::BRIDGE));
    EXPECT_TRUE(IsOverheadMapStructure(EntityType::BUILDING));
    EXPECT_TRUE(IsOverheadMapStructure(EntityType::ENTRANCE));
    EXPECT_TRUE(IsOverheadMapStructure(EntityType::GATE));
    EXPECT_TRUE(IsOverheadMapStructure(EntityType::DOOR));
    EXPECT_TRUE(IsOverheadMapStructure(EntityType::WELL));
    EXPECT_FALSE(IsOverheadMapStructure(EntityType::TERRAIN));
    EXPECT_FALSE(IsOverheadMapStructure(EntityType::HILL));
}

}