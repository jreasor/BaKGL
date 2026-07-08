#pragma once

#include "gui/clickButton.hpp"
#include "gui/core/widget.hpp"

#include <glm/glm.hpp>

namespace Logging { class Logger; }

namespace Gui {

class IGuiManager;
class Font;

// ROADMAP 4.7 — the original Betrayal at Krondor "Overhead Map": a stylized
// 2D top-down of the current zone (zoomable, party centered). This skeleton
// wires the screen into the GuiManager screen stack and the MainView Map
// button; the terrain render, zoom and full icon button bar arrive in later
// sub-increments. The provisional text buttons (Full Map / Exit) prove the
// OverheadMap<->FullMap toggle via the screen stack — FullMap's Exit pops back
// here, Exit pops to MainView.
class OverheadMap : public Widget
{
public:
    OverheadMap(
        IGuiManager& guiManager,
        const Font& font);

    // Called by GuiManager::ShowOverheadMap before the screen is pushed.
    void Enter();

private:
    void AddChildren();

    IGuiManager& mGuiManager;
    const Font& mFont;

    ClickButton mFullMapButton;
    ClickButton mExitButton;

    const Logging::Logger& mLogger;
};

}