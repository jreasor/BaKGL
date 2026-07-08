#pragma once

#include "gui/clickButton.hpp"
#include "gui/core/widget.hpp"

#include "bak/types.hpp"

#include <glm/glm.hpp>

#include <memory>

namespace Logging { class Logger; }

namespace BAK { class GameState; }

namespace Gui {

class IGuiManager;
class Font;

// ROADMAP 4.7 — the Betrayal at Krondor "Overhead Map".
//
// The map image itself is a top-down orthographic 3D render of the current
// zone's live scene, drawn by the app render loop (app/main3d.cpp) into this
// screen's 320×170 map sub-viewport (over the opaque black background, which
// hides the main view underneath). This widget owns only the GUI chrome: the
// black background, the Full Map / Exit buttons, the zoom state, and the red
// party marker + cream heading nose drawn on top of the 3D render at the map
// centre (the top-down camera is player-centred, so the party sits at screen
// centre).
//
// mZoom is the vertical tile-count visible in the top-down view (player-
// centred); the app reads GetZoom() to size the orthographic projection box.
//
// The icon button bar (Follow Road / Full Map / Zoom In/Out / Encamp / Main)
// is a later checkpoint; the provisional text buttons (Full Map / Exit) are
// retained for the OverheadMap<->FullMap toggle.
class OverheadMap : public Widget
{
public:
    OverheadMap(
        IGuiManager& guiManager,
        const Font& font,
        BAK::GameState& gameState);

    // Called by GuiManager::ShowOverheadMap before the screen is pushed.
    void Enter();

    [[nodiscard]] bool OnKeyEvent(const KeyEvent& event) override;

    // Vertical tiles visible in the top-down view (player-centred). The app
    // render loop reads this to size the orthographic projection box.
    [[nodiscard]] float GetZoom() const { return static_cast<float>(mZoom); }
    // The 320×170 map sub-viewport; the app re-renders this (scissored to the
    // map rect) after the top-down 3D pass so the party marker draws on top.
    [[nodiscard]] Widget& GetMapClipRegion() { return mClipRegion; }

private:
    void AddChildren();
    // (Re)build the party marker quads (camera-centred → marker at the map
    // centre). Terrain/structures come from the top-down 3D pass.
    void RebuildMap();
    // Tile coords (0..49) → screen coords (player-centred, north-up).
    glm::vec2 ProjectTile(unsigned tx, unsigned ty) const;
    // Apply a zoom step (negative = zoom in / fewer tiles visible).
    void StepZoom(int direction);

    IGuiManager& mGuiManager;
    const Font& mFont;
    BAK::GameState& mGameState;

    // View span in tiles (player-centred). Tunable calibration constants.
    unsigned mZoom{};

    static constexpr unsigned kZoomDefault = 6;    // tiles visible (vertical)
    static constexpr unsigned kZoomMinTiles = 1;    // zoom in to a single tile (closer than the original)
    static constexpr unsigned kZoomMaxTiles = 50;   // zooming out farther than the original is allowed
    static constexpr unsigned kZoomStep = 2;
    static constexpr float kMapW = 320.0f;
    static constexpr float kMapH = 170.0f;
    static constexpr glm::vec2 sMapCenter{160.0f, 85.0f};

    Widget mClipRegion;
    std::unique_ptr<Widget> mPlayerBody;
    std::unique_ptr<Widget> mPlayerNose;

    ClickButton mFullMapButton;
    ClickButton mExitButton;

    const Logging::Logger& mLogger;
};

}