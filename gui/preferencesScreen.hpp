#pragma once

#include "bak/layout.hpp"

#include "gui/core/widget.hpp"
#include "gui/clickButton.hpp"
#include "gui/label.hpp"

#include "app/config.hpp"
#include "graphics/graphicsConfig.hpp"

#include <array>
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Gui {

class IGuiManager;
class Backgrounds;
class Font;

// Task 4.4: in-game graphics settings. Replaces the original OPTIONS2.SCX stub
// (whose OK/Cancel/Defaults were no-ops) with a solid panel + 3 cycle/toggle
// controls (Original / Anisotropic / Resolution) + a caveat label, keeping the
// layout-file OK/Cancel/Defaults positions. Edits are best-effort at runtime and
// persisted to config.json on OK:
//   - Original / Anisotropic apply to subsequently-loaded textures (already-
//     loaded ones refresh on zone change / restart).
//   - Resolution is consumed once at window creation, so it takes effect on the
//     next boot only (the caveat label says so).
// The config path is read from the GraphicsConfig singleton (recorded by
// LoadConfigFile), so the ctor signature is unchanged -- no ripple into
// MainMenuScreen, which already constructs this screen.
class PreferencesScreen: public Widget
{
public:
    static constexpr auto sLayoutFile = "REQ_PREF.DAT";

    static constexpr auto sOk = 0;
    static constexpr auto sCancel = 1;
    static constexpr auto sDefaults = 2;

    using LeavePreferencesFn = std::function<void()>;

    // 16:10 window presets: ResolutionScale multiplies the 320x200 logical UI
    // -> window size. The default index is 1280x800 (scale 4), matching the
    // config.json default ResolutionScale.
    struct ResOption { float mScale; const char* mLabel; };
    static constexpr std::array<ResOption, 5> kResolutions{{
        {1.0f,  "320x200"},
        {4.0f,  "1280x800"},
        {6.0f,  "1920x1200"},
        {8.0f,  "2560x1600"},
        {10.8f, "3456x2160"}
    }};
    static constexpr std::size_t kDefaultResolutionIndex = 1;

    PreferencesScreen(
        IGuiManager& guiManager,
        const Backgrounds& backgrounds,
        const Font& font,
        LeavePreferencesFn&& leavePreferenceFn);

private:
    void AddChildren();
    void SyncFromConfig();
    void SyncLabels();

    void OnOk();
    void OnCancel();
    void OnDefaults();
    void OnOriginalToggle();
    void OnAnisoToggle();
    void OnResolutionCycle();

    IGuiManager& mGuiManager;
    const Font& mFont;
    const Backgrounds& mBackgrounds;

    BAK::Layout mLayout;
    LeavePreferencesFn mLeavePreferencesFn;

    // Solid panel replacing the original OPTIONS2.SCX art (the model can't see
    // the framebuffer, so a plain dark rect avoids clashing with unseen art).
    Widget mPanel;
    ClickButton mOriginalToggle;
    ClickButton mAnisoToggle;
    ClickButton mResolutionCycle;
    Label mCaveat;
    ClickButton mOk;
    ClickButton mCancel;
    ClickButton mDefaults;

    // Editable local state, synced from GraphicsConfig on construct/cancel and
    // written back to it + config.json on OK.
    bool mOriginalMode;
    bool mAnisoOn;
    std::size_t mResolutionIndex;
};

}