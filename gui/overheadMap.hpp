#pragma once

#include "gui/clickButton.hpp"
#include "gui/core/widget.hpp"

#include "bak/startupFiles.hpp"
#include "bak/types.hpp"

#include <glm/glm.hpp>

#include <memory>
#include <vector>

namespace Logging { class Logger; }

namespace BAK { class GameState; }

namespace Gui {

class IGuiManager;
class Font;

// ROADMAP 4.7 — the original Betrayal at Krondor "Overhead Map": a stylized
// 2D top-down of the current zone (zoomable, party centered).
//
// Base layer = the zone's authored ZxxMAP.DAT 50×50 coverage grid
// (BAK::LoadZoneMap): each on-map cell is drawn as a colored tile, off-map
// cells are background. This is the continuous map shape the original used.
// Per-cell terrain color comes from the sparse live WorldTileStore tiles
// (BAK::ClassifyTileTerrain); on-map cells with no 3D tile default to
// Ground. Structures (buildings/bridges/...) + the red party marker +
// heading nose are overlaid. Zoom is a tile-count view span (player
// centered, north-up) inside a 320×170 ClipRegion.
//
// The icon button bar (Follow Road / Full Map / Zoom In/Out / Encamp / Main)
// is Checkpoint C; the provisional text buttons (Full Map / Exit) are
// retained for the OverheadMap<->FullMap toggle.
class OverheadMap : public Widget
{
public:
    OverheadMap(
        IGuiManager& guiManager,
        const Font& font,
        BAK::GameState& gameState);

    // Called by GuiManager::ShowOverheadMap before the screen is pushed.
    // Loads the zone map grid + per-zone zoom bounds and (re)builds the render.
    void Enter();

    [[nodiscard]] bool OnKeyEvent(const KeyEvent& event) override;

private:
    void AddChildren();
    // (Re)build the coverage/terrain/structure/player quads from the zone
    // map grid + live tiles at the current zoom. Off-screen tiles are culled.
    void RebuildMap();
    // Tile coords (0..49) → screen coords (player-centered, north-up).
    glm::vec2 ProjectTile(unsigned tx, unsigned ty) const;
    // Apply a zoom step (negative = zoom in / fewer tiles visible).
    void StepZoom(int direction);

    IGuiManager& mGuiManager;
    const Font& mFont;
    BAK::GameState& mGameState;

    // The zone's 50×50 coverage grid (ZxxMAP.DAT), loaded in Enter().
    BAK::ZoneMap mZoneMap{};
    // Per-zone zoom bounds from ZxxDEF.DAT. Loaded for reference only —
    // BaKGL never renders this map, so the original zoom-unit semantics are
    // unknown; the actual view span uses mZoom (tile count) below.
    std::uint32_t mZoomMin{};
    std::uint32_t mZoomMax{};
    std::uint32_t mZoomRate{};
    // View span in tiles (player-centered). Tunable calibration constants.
    unsigned mZoom{};

    static constexpr unsigned kZoneDim = 50;       // ZxxMAP.DAT grid is 50×50
    static constexpr unsigned kZoomDefault = 16;   // tiles visible (vertical)
    static constexpr unsigned kZoomMinTiles = 6;
    static constexpr unsigned kZoomMaxTiles = 50;
    static constexpr unsigned kZoomStep = 4;
    static constexpr float kMapW = 320.0f;
    static constexpr float kMapH = 170.0f;
    static constexpr glm::vec2 sMapCenter{160.0f, 85.0f};

    Widget mClipRegion;
    std::vector<std::unique_ptr<Widget>> mTerrainQuads;
    std::vector<std::unique_ptr<Widget>> mStructureQuads;
    std::unique_ptr<Widget> mPlayerBody;
    std::unique_ptr<Widget> mPlayerNose;

    ClickButton mFullMapButton;
    ClickButton mExitButton;

    const Logging::Logger& mLogger;
};

}