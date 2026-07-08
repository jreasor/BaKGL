#include "gui/overheadMap.hpp"

#include "gui/IGuiManager.hpp"
#include "gui/clickButton.hpp"
#include "gui/core/widget.hpp"

#include "bak/constants.hpp"
#include "bak/coordinates.hpp"
#include "bak/gameState.hpp"
#include "bak/overheadMapClassifier.hpp"
#include "bak/startupFiles.hpp"
#include "bak/types.hpp"
#include "bak/worldFactory.hpp"

#include "com/logger.hpp"

#include <glm/glm.hpp>

#include <GLFW/glfw3.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <variant>

namespace Gui {

namespace {

// Stylized top-down terrain colors (RGBA). Tunable calibration constants;
// the original 1993 overhead map uses a similar muted palette.
glm::vec4 TerrainColor(BAK::Terrain t)
{
    switch (t)
    {
    case BAK::Terrain::Road:      return glm::vec4{0.45f, 0.30f, 0.15f, 1.0f};
    case BAK::Terrain::Path:      return glm::vec4{0.30f, 0.55f, 0.25f, 1.0f};
    case BAK::Terrain::River:     return glm::vec4{0.20f, 0.35f, 0.65f, 1.0f};
    case BAK::Terrain::Waterfall: return glm::vec4{0.35f, 0.55f, 0.80f, 1.0f};
    case BAK::Terrain::Bank:      return glm::vec4{0.62f, 0.55f, 0.35f, 1.0f};
    case BAK::Terrain::Dirt:      return glm::vec4{0.50f, 0.40f, 0.25f, 1.0f};
    case BAK::Terrain::Sand:      return glm::vec4{0.70f, 0.62f, 0.40f, 1.0f};
    case BAK::Terrain::Ground:
    default:                      return glm::vec4{0.24f, 0.34f, 0.20f, 1.0f};
    }
}

constexpr glm::vec4 sStructureColor{0.18f, 0.18f, 0.18f, 1.0f};
constexpr glm::vec4 sPlayerColor{0.85f, 0.15f, 0.15f, 1.0f};
constexpr glm::vec4 sPlayerNoseColor{0.95f, 0.85f, 0.55f, 1.0f};

// BAK heading is a 0..255 circle (0 = north). North maps to screen-up (-y).
constexpr double kFullHeading = 256.0;

} // namespace

OverheadMap::OverheadMap(
    IGuiManager& guiManager,
    const Font& font,
    BAK::GameState& gameState)
:
    Widget{
        RectTag{},
        glm::vec2{0, 0},
        glm::vec2{320, 200},
        glm::vec4{0, 0, 0, 1},
        true
    },
    mGuiManager{guiManager},
    mFont{font},
    mGameState{gameState},
    mClipRegion{
        ClipRegionTag{},
        glm::vec2{0, 0},
        glm::vec2{kMapW, kMapH},
        true
    },
    mFullMapButton{
        glm::vec2{20, 180},
        glm::vec2{64, 14},
        mFont,
        "Full Map",
        [this]{ mGuiManager.ShowFullMap(); }
    },
    mExitButton{
        glm::vec2{250, 180},
        glm::vec2{48, 14},
        mFont,
        "Exit",
        [this]{
            mGuiManager.DoFade(.8, [this]{
                mGuiManager.ExitSimpleScreen();
            });
        }
    },
    mLogger{Logging::LogState::GetLogger("Gui::OverheadMap")}
{
    AddChildren();
}

void OverheadMap::Enter()
{
    mLogger.Info() << "OverheadMap::Enter\n";
    mZoneMap = BAK::LoadZoneMap(mGameState.GetZone());
    // Loaded for reference/future fidelity only — the view span uses mZoom.
    const auto def = BAK::LoadZoneDefDatInfo(mGameState.GetZone());
    mZoomMin  = def.mMinMapZoom;
    mZoomMax  = def.mMaxMapZoom;
    mZoomRate = def.mMapZoomRate;
    mZoom     = kZoomDefault;
    RebuildMap();
}

bool OverheadMap::OnKeyEvent(const KeyEvent& event)
{
    if (!std::holds_alternative<KeyPress>(event))
        return false;
    const auto key = std::get<KeyPress>(event).mValue;
    if (key == GLFW_KEY_EQUAL)
    {
        StepZoom(-1); // zoom in: fewer tiles visible
        return true;
    }
    if (key == GLFW_KEY_MINUS)
    {
        StepZoom(+1); // zoom out: more tiles visible
        return true;
    }
    return false;
}

void OverheadMap::AddChildren()
{
    ClearChildren();
    AddChildBack(&mClipRegion);
    AddChildBack(&mFullMapButton);
    AddChildBack(&mExitButton);
}

void OverheadMap::StepZoom(int direction)
{
    if (direction < 0)
        mZoom = (mZoom > kZoomMinTiles + kZoomStep) ? mZoom - kZoomStep : kZoomMinTiles;
    else
        mZoom = (mZoom + kZoomStep < kZoomMaxTiles) ? mZoom + kZoomStep : kZoomMaxTiles;
    RebuildMap();
}

glm::vec2 OverheadMap::ProjectTile(unsigned tx, unsigned ty) const
{
    const auto player = BAK::GetTile(mGameState.GetLocation().mPosition);
    const float cellPx = kMapH / static_cast<float>(mZoom);
    // Returns the cell CENTER. Player's tile is centered on the map; north up.
    return glm::vec2{
        sMapCenter.x + (static_cast<float>(tx) - static_cast<float>(player.x)) * cellPx,
        sMapCenter.y - (static_cast<float>(ty) - static_cast<float>(player.y)) * cellPx};
}

void OverheadMap::RebuildMap()
{
    // Drop the raw-ptr children before freeing the Widgets they point at.
    mClipRegion.ClearChildren();
    mTerrainQuads.clear();
    mStructureQuads.clear();
    mPlayerBody.reset();
    mPlayerNose.reset();

    // Per-cell terrain from the sparse live tiles; on-map cells with no 3D
    // tile default to Ground. (≤255 tiles, so this is cheap.)
    std::array<BAK::Terrain, kZoneDim * kZoneDim> terrainGrid{};
    terrainGrid.fill(BAK::Terrain::Ground);

    const auto& tiles = mGuiManager.GetWorldTileStore().GetTiles();
    for (const auto& world : tiles)
    {
        const auto tile = world.GetTile(); // glm::uvec2, 0..49
        if (tile.x >= kZoneDim || tile.y >= kZoneDim)
            continue;
        terrainGrid[tile.y * kZoneDim + tile.x] = BAK::ClassifyTileTerrain(world.GetItems());
    }

    const auto player = BAK::GetTile(mGameState.GetLocation().mPosition);
    const float cellPx = kMapH / static_cast<float>(mZoom);
    const int halfX = static_cast<int>(kMapW / cellPx * 0.5f) + 1;
    const int halfY = static_cast<int>(kMapH / cellPx * 0.5f) + 1;

    // Coverage base: draw every on-map cell in the visible window. Off-map
    // cells (GetMapDatTileValue == 0) are left as background → the map reads
    // as a continuous silhouette of the zone's walkable area, not gaps.
    for (int dy = -halfY; dy <= halfY; ++dy)
    {
        const int ty = static_cast<int>(player.y) + dy;
        if (ty < 0 || ty >= static_cast<int>(kZoneDim)) continue;
        for (int dx = -halfX; dx <= halfX; ++dx)
        {
            const int tx = static_cast<int>(player.x) + dx;
            if (tx < 0 || tx >= static_cast<int>(kZoneDim)) continue;
            if (BAK::GetMapDatTileValue(tx, ty, mZoneMap) == 0)
                continue;

            const auto center = ProjectTile(tx, ty);
            auto quad = std::make_unique<Widget>(
                RectTag{},
                glm::vec2{center.x - cellPx * 0.5f, center.y - cellPx * 0.5f},
                glm::vec2{cellPx, cellPx},
                TerrainColor(terrainGrid[ty * kZoneDim + tx]),
                false);
            mClipRegion.AddChildBack(quad.get());
            mTerrainQuads.push_back(std::move(quad));
        }
    }

    // Structure markers (buildings / bridges / entrances / ...). Placed at
    // their sub-tile BAK location within the tile cell.
    for (const auto& world : tiles)
    {
        const auto tile = world.GetTile();
        if (tile.x >= kZoneDim || tile.y >= kZoneDim) continue;
        for (const auto& item : world.GetItems())
        {
            if (!BAK::IsOverheadMapStructure(item.GetZoneItem().GetEntityType()))
                continue;
            const auto center = ProjectTile(tile.x, tile.y);
            const auto bakLoc = item.GetBakLocation();
            const glm::uvec2 tileOrigin{
                tile.x * static_cast<unsigned>(BAK::gTileSize),
                tile.y * static_cast<unsigned>(BAK::gTileSize)};
            const float subX = static_cast<float>(static_cast<long>(bakLoc.x) - static_cast<long>(tileOrigin.x)) / static_cast<float>(BAK::gTileSize);
            const float subY = static_cast<float>(static_cast<long>(bakLoc.y) - static_cast<long>(tileOrigin.y)) / static_cast<float>(BAK::gTileSize);
            const glm::vec2 loc{
                center.x + (subX - 0.5f) * cellPx,
                center.y - (subY - 0.5f) * cellPx};
            constexpr float ss = 4.0f;
            auto sq = std::make_unique<Widget>(
                RectTag{},
                glm::vec2{loc.x - ss * 0.5f, loc.y - ss * 0.5f},
                glm::vec2{ss, ss},
                sStructureColor,
                false);
            mClipRegion.AddChildBack(sq.get());
            mStructureQuads.push_back(std::move(sq));
        }
    }

    // Party marker: red square on the player tile + a cream "nose" offset in
    // the facing direction (heading 0 = north = screen-up).
    const auto screen = ProjectTile(player.x, player.y);
    constexpr float body = 6.0f;
    mPlayerBody = std::make_unique<Widget>(
        RectTag{},
        glm::vec2{screen.x - body * 0.5f, screen.y - body * 0.5f},
        glm::vec2{body, body},
        sPlayerColor,
        false);
    mClipRegion.AddChildBack(mPlayerBody.get());

    const auto heading = mGameState.GetLocation().mHeading; // 0..255, 0 = north
    const double angle = static_cast<double>(heading) / kFullHeading * 6.283185307179586;
    constexpr float noseR = 6.0f;
    constexpr float nose = 3.0f;
    const glm::vec2 noseCenter{
        screen.x + static_cast<float>(std::sin(angle)) * noseR,
        screen.y - static_cast<float>(std::cos(angle)) * noseR};
    mPlayerNose = std::make_unique<Widget>(
        RectTag{},
        glm::vec2{noseCenter.x - nose * 0.5f, noseCenter.y - nose * 0.5f},
        glm::vec2{nose, nose},
        sPlayerNoseColor,
        false);
    mClipRegion.AddChildBack(mPlayerNose.get());
}

}