#pragma once

#include "bak/types.hpp"

#include "bak/encounter/teleport.hpp"

namespace BAK {

class WorldTileStore;

// Interface to load zone
class IZoneLoader
{
public:
    virtual ~IZoneLoader() = default;
    // Load zone based on zone info in DEF_ZONE.DAT
    virtual void DoTeleport(BAK::Encounter::Teleport) = 0;
    virtual void LoadGame(std::string, std::optional<Chapter>) = 0;
    // The currently-loaded zone's world tiles (ROADMAP 4.7 Overhead Map).
    // Valid only while a zone is loaded (i.e. in-game); implementations assert.
    virtual const BAK::WorldTileStore& GetWorldTileStore() const = 0;
};

}
