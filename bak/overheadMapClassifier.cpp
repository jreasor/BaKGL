#include "bak/overheadMapClassifier.hpp"

#include <algorithm>

namespace BAK {

namespace {

bool NameHas(const std::string& name, const char* prefix, std::size_t len)
{
    return name.size() >= len && name.compare(0, len, prefix) == 0;
}

bool HasColor(const std::vector<std::uint8_t>& colors, std::uint8_t idx)
{
    return std::find(colors.begin(), colors.end(), idx) != colors.end();
}

// Lower = higher priority (wins the per-tile aggregation). Ground is the
// fallback so a tile with only plain-ground items reads as Ground.
int TerrainPriority(Terrain t)
{
    switch (t)
    {
        case Terrain::Road:      return 0;
        case Terrain::Path:      return 1;
        case Terrain::River:     return 2;
        case Terrain::Waterfall: return 3;
        case Terrain::Bank:      return 4;
        case Terrain::Dirt:      return 5;
        case Terrain::Sand:      return 6;
        case Terrain::Ground:    return 7;
    }
    return 7;
}

} // namespace

Terrain ClassifyTerrainByNameAndColors(
    const std::string& name,
    const std::vector<std::uint8_t>& colors)
{
    // Mirrors bak/worldFactory.cpp:438-538 terrain-face branches.
    if (NameHas(name, "t0", 2))
    {
        if (HasColor(colors, 1)) return Terrain::Road;
        if (HasColor(colors, 2)) return Terrain::Path;
        if (HasColor(colors, 3)) return Terrain::River;
        return Terrain::Ground;
    }
    if (NameHas(name, "r0", 2))
    {
        if (HasColor(colors, 3)) return Terrain::River;
        if (HasColor(colors, 5)) return Terrain::Bank;
        return Terrain::Ground;
    }
    if (NameHas(name, "g0", 2))
    {
        if (HasColor(colors, 5)) return Terrain::River;
        return Terrain::Ground;
    }
    if (NameHas(name, "field", 5))
    {
        if (HasColor(colors, 1)) return Terrain::Dirt;
        if (HasColor(colors, 2)) return Terrain::Bank;
        return Terrain::Dirt;
    }
    if (NameHas(name, "fall", 4) || NameHas(name, "spring", 6))
    {
        if (HasColor(colors, 6)) return Terrain::Waterfall;
        if (HasColor(colors, 3)) return Terrain::River;
        if (HasColor(colors, 5)) return Terrain::Bank;
        return Terrain::Ground;
    }
    // ground/zero*/one*/landscp*/null and any other ground item: no feature
    // terrain face → plain ground.
    return Terrain::Ground;
}

Terrain ClassifyTerrainItem(const ZoneItem& item)
{
    return ClassifyTerrainByNameAndColors(item.GetName(), item.GetColors());
}

Terrain ClassifyTileTerrain(const std::vector<WorldItemInstance>& items)
{
    Terrain best = Terrain::Ground;
    bool any = false;
    for (const auto& item : items)
    {
        if (!IsOverheadMapTerrain(item.GetZoneItem().GetEntityType()))
            continue;
        const auto t = ClassifyTerrainItem(item.GetZoneItem());
        if (!any || TerrainPriority(t) < TerrainPriority(best))
        {
            best = t;
            any = true;
        }
    }
    return best;
}

bool IsOverheadMapTerrain(EntityType et)
{
    using enum EntityType;
    switch (et)
    {
        case TERRAIN:   [[fallthrough]];
        case EXTERIOR:  [[fallthrough]];
        case INTERIOR:  [[fallthrough]];
        case HILL:      [[fallthrough]];
        case LANDSCAPE:
            return true;
        default:
            return false;
    }
}

bool IsOverheadMapStructure(EntityType et)
{
    using enum EntityType;
    switch (et)
    {
        case BRIDGE:    [[fallthrough]];
        case BUILDING:  [[fallthrough]];
        case ENTRANCE:  [[fallthrough]];
        case TOMBSTONE: [[fallthrough]];
        case GATE:      [[fallthrough]];
        case DOOR:      [[fallthrough]];
        case WELL:
            return true;
        default:
            return false;
    }
}

}