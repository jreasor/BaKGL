#pragma once

#include "bak/coordinates.hpp"
#include "bak/dialog.hpp"
#include "bak/layout.hpp"

#include "gui/compass.hpp"
#include "gui/clickButton.hpp"
#include "gui/core/widget.hpp"

#include <glm/glm.hpp>

namespace BAK {
class GameState;
}

namespace Logging {
class Logger;
}

namespace Gui {

class IGuiManager;

class Backgrounds;
class Icons;
class Font;
class TickAnimator;

class MainView : public Widget
{
public:
    static constexpr auto sLayoutFile = "REQ_MAIN.DAT";

    // Screen coords are y-down, so layout idx 1 (y=112) is the visually-UP
    // button and idx 2 (y=132) is visually-DOWN. Names match screen position:
    // up == forward (into the scene), down == backward (ROADMAP 4.5).
    static constexpr auto sForward = 1;
    static constexpr auto sBackward = 2;
    static constexpr auto sRotateLeft = 0;
    static constexpr auto sRotateRight = 3;
    static constexpr auto sSnapToRoad = 4;
    static constexpr auto sFullMap = 5;
    static constexpr auto sCast = 6;
    static constexpr auto sBookmark = 7;
    static constexpr auto sCamp = 8;
    static constexpr auto sMainMenu = 9;
    static constexpr auto sCharacterWidgetBegin = 10;

    // Hold-to-repeat interval for the movement buttons (ROADMAP 4.5). A press
    // fires the movement once immediately; a TickAnimator then re-fires it on
    // this cadence until the mouse button is released or MainView stops being
    // the top screen.
    static constexpr auto sMovementRepeatSeconds = 0.30;

    MainView(
        IGuiManager& guiManager,
        const Backgrounds& backgrounds,
        const Icons& icons,
        const Font& spellFont,
        const Font& gameFont);

    void SetHeading(BAK::GameHeading heading);
    void HandleButton(unsigned buttonIndex);
    void UpdatePartyMembers(const BAK::GameState& gameState);
    void ShowPortrait(BAK::ActiveCharIndex character);
    void ShowInventory(BAK::ActiveCharIndex character);
    void SetCanSaveBookmark(bool can);
    // Called by GuiManager on push/pop. The compass always renders (original
    // behavior preserved on Camp/Cast); RefreshCompass is retained only to stop
    // the movement hold-to-repeat when a sub-screen comes up, since the
    // AnimatorStore keeps ticking under pushed sub-screens (ROADMAP 4.5).
    void RefreshCompass();
    [[nodiscard]] bool OnMouseEvent(const MouseEvent& event) override;
private:
    void AddChildren();
    void StartMovementRepeat(unsigned buttonIndex);
    void StopMovementRepeat();

    IGuiManager& mGuiManager;
    const Icons& mIcons;
    const Font& mSpellFont;
    const Font& mGameFont;

    BAK::Layout mLayout;

    std::vector<Widget> mActiveSpells;
    Compass mCompass;
    std::vector<ClickButtonImage> mButtons;
    std::vector<ClickButtonImage> mCharacters;

    bool mCanSaveBookmark{false};
    bool mShowingBookmarkDialog{false};
    bool mNeedRefresh{false};
    ClickButton mBookmarkPopup;

    // Live TickAnimator for the currently-held movement button, or null. The
    // AnimatorStore owns the animator; MainView keeps only a raw handle so it
    // can Stop() the repeat on release / when a sub-screen comes up.
    TickAnimator* mMovementRepeatAnimator{nullptr};

    const Logging::Logger& mLogger;
};

}
