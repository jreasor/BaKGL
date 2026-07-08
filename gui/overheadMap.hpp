#pragma once

#include "gui/clickButton.hpp"
#include "gui/core/widget.hpp"

#include "bak/types.hpp"

#include <glm/glm.hpp>

#include <array>
#include <vector>

namespace Logging { class Logger; }

namespace BAK { class GameState; }

namespace Gui {

class IGuiManager;
class Backgrounds;
class Icons;

// ROADMAP 4.7 — the Betrayal at Krondor "Overhead Map" (local area view).
//
// The map image is a top-down orthographic 3D render of the current zone's
// live scene, drawn by the app render loop (app/main3d.cpp) into this screen's
// view-area rectangle — the same cut-out the main screen uses for its 3D scene
// (x13 y12 w294 h100 in 320×200 logical space; see screenCutout.hpp). The
// chrome mirrors the main screen: a FRAME.SCX background, the 3 active
// character portraits (left-click → inventory, right-click → character sheet,
// exactly as on MainView), and the 6-slot icon bar at the bottom-right — Full
// Map / Zoom Out / Zoom In / Camp / Main, with one slot left blank. The red
// party marker is drawn by the app on top of the 3D render at the view-area
// centre (the top-down camera is player-centred).
//
// mZoom is the vertical tile-count visible in the top-down view (player-
// centred); the app reads GetZoom() to size the orthographic projection box.
class OverheadMap : public Widget
{
public:
    // The view-area rectangle (320×200 logical, y-down) — the main screen's
    // 3D-scene cut-out. The app's top-down pass renders into this rect; shared
    // single source of truth with app/main3d.cpp.
    static constexpr float kViewX = 13.0f;
    static constexpr float kViewY = 12.0f;
    static constexpr float kViewW = 294.0f;
    static constexpr float kViewH = 100.0f;

    OverheadMap(
        IGuiManager& guiManager,
        const Backgrounds& backgrounds,
        const Icons& icons,
        BAK::GameState& gameState);

    // Called by GuiManager::ShowOverheadMap before the screen is pushed.
    void Enter();

    [[nodiscard]] bool OnKeyEvent(const KeyEvent& event) override;

    // Vertical tiles visible in the top-down view (player-centred). The app
    // render loop reads this to size the orthographic projection box.
    [[nodiscard]] float GetZoom() const { return mZoom; }

private:
    void AddChildren();
    // (Re)build the portrait widgets from the live party, then refresh children
    // (the zoom buttons hide when their limit is reached). Called from Enter
    // and StepZoom; the 3D ortho box re-reads GetZoom() next frame.
    void RebuildMap();
    void UpdatePartyMembers();
    // Apply a zoom step (negative = zoom in / fewer tiles visible).
    void StepZoom(int direction);
    [[nodiscard]] bool AtZoomInLimit() const;
    [[nodiscard]] bool AtZoomOutLimit() const;

    IGuiManager& mGuiManager;
    const Icons& mIcons;
    BAK::GameState& mGameState;

    // View span in tiles (player-centred). Tunable calibration constants.
    float mZoom{};

    static constexpr float kZoomDefault = 6.0f;    // tiles visible (vertical)
    static constexpr float kZoomMinTiles = 0.25f;  // closest zoom-in (quarter-tile)
    static constexpr float kZoomMaxTiles = 50.0f;  // farthest zoom out
    static constexpr float kZoomStep    = 0.5f;

    // REQ_MAIN.DAT button-bar geometry (320×200, y-down): 6 slots, 34×29 each,
    // in a 3×2 grid. Slot idx 4 (top-left) is intentionally left blank.
    static constexpr glm::vec2 sButtonDims{34, 29};
    static constexpr glm::vec2 sFullMapPos{200, 164};   // idx 5 — Full Map
    static constexpr glm::vec2 sZoomOutPos{237, 130};   // idx 6 — Zoom Out
    static constexpr glm::vec2 sZoomInPos{236, 164};    // idx 7 — Zoom In
    static constexpr glm::vec2 sCampPos{273, 130};      // idx 8 — Camp
    static constexpr glm::vec2 sMainPos{273, 164};      // idx 9 — Main (exit)

    // BICONS icon indices (BICONS1.BMX; pressed state is at idx + 67).
    static constexpr unsigned kIconFullMap = 6;
    static constexpr unsigned kIconZoomOut = 10;
    static constexpr unsigned kIconZoomIn  = 11;
    static constexpr unsigned kIconCamp    = 7;
    static constexpr unsigned kIconMain    = 9;

    // Portrait slots (320×200, y-down): h=44, y=141; indexed by ActiveCharIndex.
    static constexpr std::array<glm::vec2, 3> sPortraitPos{{
        glm::vec2{14, 141}, glm::vec2{73, 141}, glm::vec2{132, 141}}};
    static constexpr std::array<glm::vec2, 3> sPortraitDims{{
        glm::vec2{53, 44}, glm::vec2{52, 44}, glm::vec2{52, 44}}};

    ClickButtonImage mFullMapButton;
    ClickButtonImage mZoomOutButton;
    ClickButtonImage mZoomInButton;
    ClickButtonImage mCampButton;
    ClickButtonImage mMainButton;

    std::vector<ClickButtonImage> mCharacters;

    const Logging::Logger& mLogger;
};

}