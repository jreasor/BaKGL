#include "gui/preferencesScreen.hpp"

#include "gui/IGuiManager.hpp"
#include "gui/colors.hpp"
#include "gui/clickButton.hpp"
#include "gui/core/widget.hpp"
#include "com/logger.hpp"

#include <cmath>
#include <string>

namespace Gui {

PreferencesScreen::PreferencesScreen(
    IGuiManager& guiManager,
    const Backgrounds& backgrounds,
    const Font& font,
    LeavePreferencesFn&& leavePreferenceFn)
:
    Widget{
        RectTag{},
        glm::vec2{0, 0},
        glm::vec2{320, 200},
        Color::black,
        false
    },
    mGuiManager{guiManager},
    mFont{font},
    mBackgrounds{backgrounds},
    mLayout{sLayoutFile},
    mLeavePreferencesFn{std::move(leavePreferenceFn)},
    mPanel{
        RectTag{},
        glm::vec2{0, 0},
        glm::vec2{320, 200},
        Color::infoBackground,
        false
    },
    mOriginalToggle{
        glm::vec2{20, 40},
        glm::vec2{150, 20},
        mFont,
        "Original: OFF",
        [this]{ OnOriginalToggle(); }
    },
    mAnisoToggle{
        glm::vec2{20, 65},
        glm::vec2{150, 20},
        mFont,
        "Anisotropic: On",
        [this]{ OnAnisoToggle(); }
    },
    mResolutionCycle{
        glm::vec2{20, 90},
        glm::vec2{180, 20},
        mFont,
        "Resolution: 1280x800",
        [this]{ OnResolutionCycle(); }
    },
    mCaveat{
        glm::vec2{20, 115},
        glm::vec2{200, 12},
        mFont,
        "Resolution applies after restart"
    },
    mOk{
        mLayout.GetWidgetLocation(sOk),
        mLayout.GetWidgetDimensions(sOk),
        mFont,
        "#OK",
        [this]{ OnOk(); }
    },
    mCancel{
        mLayout.GetWidgetLocation(sCancel),
        mLayout.GetWidgetDimensions(sCancel),
        mFont,
        "#Cancel",
        [this]{ OnCancel(); }
    },
    mDefaults{
        mLayout.GetWidgetLocation(sDefaults),
        mLayout.GetWidgetDimensions(sDefaults),
        mFont,
        "#Defaults",
        [this]{ OnDefaults(); }
    },
    mOriginalMode{false},
    mAnisoOn{true},
    mResolutionIndex{kDefaultResolutionIndex}
{
    SyncFromConfig();
    SyncLabels();
    AddChildren();
}

void PreferencesScreen::AddChildren()
{
    ClearChildren();

    // Panel first so it draws behind the controls + action buttons.
    AddChildBack(&mPanel);
    AddChildBack(&mOriginalToggle);
    AddChildBack(&mAnisoToggle);
    AddChildBack(&mResolutionCycle);
    AddChildBack(&mCaveat);

    mOk.SetPosition(mLayout.GetWidgetLocation(sOk));
    mCancel.SetPosition(mLayout.GetWidgetLocation(sCancel));
    mDefaults.SetPosition(mLayout.GetWidgetLocation(sDefaults));
    AddChildBack(&mOk);
    AddChildBack(&mCancel);
    AddChildBack(&mDefaults);
}

void PreferencesScreen::SyncFromConfig()
{
    auto& cfg = Graphics::GraphicsConfig::Get();
    mOriginalMode = cfg.GetOriginalMode();
    mAnisoOn = cfg.GetAnisotropicFilter() > 0.0f;

    // Snap to the matching preset; if the configured scale isn't one of the 5,
    // fall back to the default rather than leaving the index dangling.
    const float scale = cfg.GetResolutionScale();
    mResolutionIndex = kDefaultResolutionIndex;
    for (std::size_t i = 0; i < kResolutions.size(); ++i)
    {
        if (std::abs(kResolutions[i].mScale - scale) < 0.01f)
        {
            mResolutionIndex = i;
            break;
        }
    }
}

void PreferencesScreen::SyncLabels()
{
    mOriginalToggle.SetText(mOriginalMode ? "Original: ON" : "Original: OFF");
    mAnisoToggle.SetText(mAnisoOn ? "Anisotropic: On" : "Anisotropic: Off");
    mResolutionCycle.SetText(
        std::string{"Resolution: "} + kResolutions[mResolutionIndex].mLabel);
    mCaveat.SetText("Resolution applies after restart");
}

void PreferencesScreen::OnOriginalToggle()
{
    mOriginalMode = !mOriginalMode;
    SyncLabels();
}

void PreferencesScreen::OnAnisoToggle()
{
    mAnisoOn = !mAnisoOn;
    SyncLabels();
}

void PreferencesScreen::OnResolutionCycle()
{
    mResolutionIndex = (mResolutionIndex + 1) % kResolutions.size();
    SyncLabels();
}

void PreferencesScreen::OnOk()
{
    auto& cfg = Graphics::GraphicsConfig::Get();
    const auto aniso = mAnisoOn ? 4.0f : 0.0f;
    const auto scale = kResolutions[mResolutionIndex].mScale;

    // Best-effort runtime apply: Original + Anisotropic affect subsequently-
    // loaded textures; Resolution is consumed once at next boot's window create.
    cfg.SetOriginalMode(mOriginalMode);
    cfg.SetAnisotropicFilter(aniso);
    cfg.SetResolutionScale(scale);

    // Persist the 3 owned knobs. mMaxTextureDim is NOT owned by this screen, but
    // SaveGraphicsValues rewrites it unconditionally, so echo the current cap back
    // -- else a non-2048 config would be clobbered to the struct default 2048.
    Config::Graphics g{};
    g.mOriginalMode = mOriginalMode;
    g.mAnisotropicFilter = aniso;
    g.mResolutionScale = scale;
    g.mMaxTextureDim = cfg.GetMaxTextureDim();

    const auto& path = cfg.GetConfigPath();
    if (path.empty())
    {
        Logging::LogInfo("Gui::PreferencesScreen")
            << "Cannot save graphics: no config path recorded.\n";
    }
    else if (!Config::SaveGraphicsValues(path, g))
    {
        Logging::LogInfo("Gui::PreferencesScreen")
            << "Failed to save graphics to: " << path << "\n";
    }
    else
    {
        Logging::LogInfo("Gui::PreferencesScreen")
            << "Graphics saved to " << path
            << " (OriginalMode=" << mOriginalMode
            << ", AnisotropicFilter=" << aniso
            << ", ResolutionScale=" << scale
            << "; resolution applies after restart)\n";
    }

    mLeavePreferencesFn();
}

void PreferencesScreen::OnCancel()
{
    // Discard edits: re-sync local state from the (unchanged) singleton, then leave.
    SyncFromConfig();
    SyncLabels();
    mLeavePreferencesFn();
}

void PreferencesScreen::OnDefaults()
{
    mOriginalMode = false;
    mAnisoOn = true;
    mResolutionIndex = kDefaultResolutionIndex;
    SyncLabels();
}

}