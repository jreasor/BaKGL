#pragma once

#include "gui/clickButton.hpp"
#include "gui/core/widget.hpp"

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
// 2D top-down of the current zone (zoomable, party centered). Checkpoint B
// (this revision) renders the zone terrain + structures + party marker from
// the live zone's WorldTileStore (threaded through IGuiManager::GetWorldTileStore)
// with keyboard +/- zoom. The icon button bar (Follow Road / Full Map / Zoom
// In/Out / Encamp / Main) is Checkpoint C; the provisional text buttons
// (Full Map / Exit) are retained for the OverheadMap<->FullMap toggle.
class OverheadMap : public Widget
{
public:
    OverheadMap(
        IGuiManager& guiManager,
        const Font& font,
        BAK::GameState& gameState);

    // Called by GuiManager::ShowOverheadMap before the screen is pushed.
    // Loads the per-zone zoom bounds and (re)builds the map render.
    void Enter();

    [[nodiscard]] bool OnKeyEvent(const KeyEvent& event) override;

private:
    void AddChildren();
    // (Re)build the terrain/structure/player quads from the live zone tiles
    // at the current zoom. Off-screen tiles are culled.
    void RebuildMap();
    // BAK world coords → screen coords (player-centered, north-up).
    glm::vec2 Project(glm::uvec2 bakPos) const;
    // Apply a zoom step (negative = zoom in / smaller span).
    void StepZoom(int direction);

    IGuiManager& mGuiManager;
    const Font& mFont;
    BAK::GameState& mGameState;

    // Per-zone zoom bounds (raw ZxxDEF.DAT uint32 values, treated as a
    // BAK-unit view span; kZoomScale calibrates the span to tile count).
    std::uint32_t mZoomMin{};
    std::uint32_t mZoomMax{};
    std::uint32_t mZoomRate{};
    std::uint32_t mZoom{};

    static constexpr float kZoomScale = 1.0f;
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