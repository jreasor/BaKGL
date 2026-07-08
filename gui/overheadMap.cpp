#include "gui/overheadMap.hpp"

#include "gui/IGuiManager.hpp"
#include "gui/backgrounds.hpp"
#include "gui/clickButton.hpp"
#include "gui/core/widget.hpp"
#include "gui/icons.hpp"

#include "bak/gameState.hpp"
#include "bak/party.hpp"
#include "bak/types.hpp"

#include "com/logger.hpp"

#include <glm/glm.hpp>

#include <GLFW/glfw3.h>

#include <cmath>
#include <cstddef>
#include <tuple>
#include <variant>

namespace Gui {

OverheadMap::OverheadMap(
    IGuiManager& guiManager,
    const Backgrounds& backgrounds,
    const Icons& icons,
    BAK::GameState& gameState)
:
    Widget{
        Graphics::DrawMode::Sprite,
        backgrounds.GetScreen("FRAME.SCX").mSpriteSheet,
        backgrounds.GetScreen("FRAME.SCX").mTexture,
        Graphics::ColorMode::Texture,
        glm::vec4{1},
        glm::vec2{0},
        glm::vec2{320, 200},
        true
    },
    mGuiManager{guiManager},
    mIcons{icons},
    mGameState{gameState},
    mZoom{},
    mFullMapButton{
        sFullMapPos, sButtonDims,
        icons.GetButtonTextures(kIconFullMap),
        [this]{ mGuiManager.ShowFullMap(); },
        []{}
    },
    mZoomOutButton{
        sZoomOutPos, sButtonDims,
        icons.GetButtonTextures(kIconZoomOut),
        [this]{ StepZoom(+1); },   // zoom out: more tiles visible
        []{}
    },
    mZoomInButton{
        sZoomInPos, sButtonDims,
        icons.GetButtonTextures(kIconZoomIn),
        [this]{ StepZoom(-1); },   // zoom in: fewer tiles visible
        []{}
    },
    mCampButton{
        sCampPos, sButtonDims,
        icons.GetButtonTextures(kIconCamp),
        [this]{ mGuiManager.ShowCamp(false, nullptr); },
        []{}
    },
    mMainButton{
        sMainPos, sButtonDims,
        icons.GetButtonTextures(kIconMain),
        [this]{   // Main → leave the overhead map, return to the main view
            mGuiManager.DoFade(.8, [this]{
                mGuiManager.ExitSimpleScreen();
            });
        },
        []{}
    },
    mCharacters{},
    mLogger{Logging::LogState::GetLogger("Gui::OverheadMap")}
{
    // Centre each BICONS icon inside its 34×29 click region (the icon's own
    // pixel dims come from the Icons sheet, as on MainView).
    mFullMapButton.CenterImage(std::get<glm::vec2>(mIcons.GetButton(kIconFullMap)));
    mZoomOutButton.CenterImage(std::get<glm::vec2>(mIcons.GetButton(kIconZoomOut)));
    mZoomInButton.CenterImage(std::get<glm::vec2>(mIcons.GetButton(kIconZoomIn)));
    mCampButton.CenterImage(std::get<glm::vec2>(mIcons.GetButton(kIconCamp)));
    mMainButton.CenterImage(std::get<glm::vec2>(mIcons.GetButton(kIconMain)));

    AddChildren();
}

void OverheadMap::Enter()
{
    mLogger.Info() << "OverheadMap::Enter\n";
    mZoom = kZoomDefault;
    UpdatePartyMembers();
    AddChildren();
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
    // Portraits first (left-click inventory, right-click character sheet), then
    // the icon bar. The zoom buttons are omitted when their limit is reached so
    // the icon disappears and the slot can't be clicked (per user spec).
    for (auto& character : mCharacters)
        AddChildBack(&character);
    AddChildBack(&mFullMapButton);
    if (!AtZoomOutLimit())
        AddChildBack(&mZoomOutButton);
    if (!AtZoomInLimit())
        AddChildBack(&mZoomInButton);
    AddChildBack(&mCampButton);
    AddChildBack(&mMainButton);
}

void OverheadMap::UpdatePartyMembers()
{
    mCharacters.clear();
    mCharacters.reserve(3);

    const auto& party = mGameState.GetParty();
    BAK::ActiveCharIndex person{0};
    do
    {
        const auto [spriteSheet, image, dimss] = mIcons.GetCharacterHead(
            party.GetCharacter(person).GetIndex().mValue);
        const auto i = person.mValue;
        mCharacters.emplace_back(
            sPortraitPos[i],
            sPortraitDims[i],
            spriteSheet,
            image,
            image,
            [this, character=person]{
                mGuiManager.ShowInventory(character);
            },
            [this, character=person]{
                mGuiManager.ShowCharacterPortrait(character);
            }
        );
        (void) dimss;
        person = party.NextActiveCharacter(person);
    } while (person != BAK::ActiveCharIndex{0});
}

void OverheadMap::StepZoom(int direction)
{
    if (direction < 0)
        mZoom = (mZoom > kZoomMinTiles + kZoomStep) ? mZoom - kZoomStep : kZoomMinTiles;
    else
        mZoom = (mZoom + kZoomStep < kZoomMaxTiles) ? mZoom + kZoomStep : kZoomMaxTiles;
    AddChildren();
}

bool OverheadMap::AtZoomInLimit() const  { return mZoom <= kZoomMinTiles; }
bool OverheadMap::AtZoomOutLimit() const { return mZoom >= kZoomMaxTiles; }

void OverheadMap::RebuildMap()
{
    // The party marker is drawn by the app render loop's top-down pass in clip
    // space, so this widget owns no per-frame map graphics. Retained as the
    // zoom-change hook (refreshes the zoom-button visibility); the 3D ortho
    // box re-reads GetZoom() next frame.
    AddChildren();
}

}