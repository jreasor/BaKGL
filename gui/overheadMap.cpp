#include "gui/overheadMap.hpp"

#include "gui/IGuiManager.hpp"
#include "gui/clickButton.hpp"
#include "gui/core/widget.hpp"

#include "bak/coordinates.hpp"
#include "bak/gameState.hpp"
#include "bak/types.hpp"

#include "com/logger.hpp"

#include <glm/glm.hpp>

#include <GLFW/glfw3.h>

#include <cmath>
#include <cstddef>
#include <memory>
#include <variant>

namespace Gui {

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
    mZoom = kZoomDefault;
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
    // The party marker (a red triangle) is now drawn by the app render loop's
    // top-down pass in clip space, so this widget owns no per-frame map
    // graphics. Retained as the zoom-change hook (called from Enter/StepZoom);
    // the 3D ortho box re-reads GetZoom() next frame.
    mClipRegion.ClearChildren();
}

}