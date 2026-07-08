#include "gui/overheadMap.hpp"

#include "gui/IGuiManager.hpp"
#include "gui/clickButton.hpp"
#include "gui/core/widget.hpp"

#include "com/logger.hpp"

#include <glm/glm.hpp>

namespace Gui {

OverheadMap::OverheadMap(
    IGuiManager& guiManager,
    const Font& font)
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
}

void OverheadMap::AddChildren()
{
    ClearChildren();
    AddChildBack(&mFullMapButton);
    AddChildBack(&mExitButton);
}

}