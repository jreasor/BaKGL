#pragma once

// ROADMAP 4.7 (Overhead Map) — terrain/structure classification for the 2D
// stylized top-down render. The terrain mapping replicates the per-face
// terrain logic in bak/worldFactory.cpp (name prefix + face colorIndex →
// Terrain enum) so the overhead map colors match the 3D world's terrain
// assignment. EntityType is NOT a reliable terrain/structure split (ground
// items carry EntityType::EXTERIOR for `t0*` tiles and EntityType::HILL for
// `zero*`/`one*`/`landscp*`), so terrain is classified by name+colors and
// EntityType is used only for the structure-square vs skip decision.

#include "bak/constants.hpp"
#include "bak/entityType.hpp"
#include "bak/worldFactory.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace BAK {

// Pure core of the terrain classifier: name prefix + the set of per-face
// color indices → a single Terrain enum, mirroring worldFactory's
// terrain-face branches (bak/worldFactory.cpp:438-538). Returns the
// highest-priority feature terrain present (Road > Path > River >
// Waterfall > Bank > Dirt), else Ground. Exposed (rather than file-local)
// so it can be unit-tested without constructing a full ZoneItem.
Terrain ClassifyTerrainByNameAndColors(
    const std::string& name,
    const std::vector<std::uint8_t>& colors);

// Classify one terrain item (a ZoneItem) to a single Terrain enum; thin
// wrapper over ClassifyTerrainByNameAndColors using the item's name/colors.
Terrain ClassifyTerrainItem(const ZoneItem& item);

// Aggregate the ground terrain of one tile from its item instances: the
// highest-priority terrain among the tile's terrain items (Road wins if any
// road face exists, etc.), else Ground. Structure/skip items are ignored.
Terrain ClassifyTileTerrain(const std::vector<WorldItemInstance>& items);

// EntityType → overhead-map category predicates.
// Terrain: drawn as a ground quad colored by ClassifyTerrainItem.
bool IsOverheadMapTerrain(EntityType et);
// Structure: drawn as a square marker (building/bridge/entrance/...).
bool IsOverheadMapStructure(EntityType et);

}