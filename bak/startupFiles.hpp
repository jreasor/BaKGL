#pragma once

#include "bak/coordinates.hpp"
#include "bak/types.hpp"
#include "bak/worldClock.hpp"

#include <array>

namespace BAK
{

struct ChapterStartLocation
{
    MapLocation mMapLocation;
    Location mLocation;
    Time mTimeElapsed;
};

std::ostream& operator<<(std::ostream&, const ChapterStartLocation&);

ChapterStartLocation LoadChapterStartLocation(Chapter);
void LoadFilter();
void LoadDetect();
void LoadZoneDat(ZoneNumber);
void LoadZoneDefDat(ZoneNumber);

class FileBuffer;

// The subset of ZxxDEF.DAT fields the engine actually consumes. `LoadZoneDefDat`
// reads the whole record but historically only logged it; this struct exposes
// the fields ROADMAP 4.7 (Overhead Map) needs: the per-zone map-zoom bounds
// (minMapZoom/maxMapZoom/mapZoomRate) and the zone-type/horizon fields used to
// detect underground zones. Units of the zoom fields are undocumented in-repo;
// they are raw Uint32LE values (treated as BAK-unit view span by the Overhead
// Map, validated against real zone values).
struct ZoneDefInfo
{
    std::uint16_t mZoneType{};
    std::uint16_t mThreeDParam{};
    std::uint16_t mHorizonDisplayType{};
    std::uint8_t  mGroundType{};
    std::uint8_t  mGroundHeight{};
    std::uint32_t mMinMapZoom{};
    std::uint32_t mMaxMapZoom{};
    std::uint32_t mMapZoomRate{};
};

// Reads ZxxDEF.DAT and returns the consumed fields. The legacy void
// `LoadZoneDefDat(ZoneNumber)` overload delegates to this and logs (preserving
// its original debug-dump behavior for `display_startup_files`).
ZoneDefInfo LoadZoneDefDatInfo(ZoneNumber zone);

// Parses the ZxxDEF.DAT record from an already-loaded buffer (no disk I/O).
// `LoadZoneDefDatInfo` loads the file then delegates here; tests feed a
// synthetic buffer to verify the field layout round-trips.
ZoneDefInfo ParseZoneDefDat(FileBuffer& fb);

using ZoneMap = std::array<std::uint8_t, 0x190>;

// This is used to say whether there is a tile adjacent
// for the purposes of loading surrounding tiles
ZoneMap LoadZoneMap(ZoneNumber);
unsigned GetMapDatTileValue(
    unsigned x,
    unsigned y,
    const ZoneMap& mapDat);

void LoadStartDat();
}
