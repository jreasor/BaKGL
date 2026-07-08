#include "gui/mainView.hpp"

#include "bak/dialog.hpp"
#include "bak/dialogSources.hpp"
#include "bak/gameState.hpp"

#include "gui/IGuiManager.hpp"
#include "gui/tickAnimator.hpp"
#include "gui/backgrounds.hpp"
#include "gui/fontManager.hpp"
#include "gui/icons.hpp"

#include <glm/glm.hpp>

#include <iostream>
#include <memory>

namespace Gui {

MainView::MainView(
    IGuiManager& guiManager,
    const Backgrounds& backgrounds,
    const Icons& icons,
    const Font& spellFont,
    const Font& gameFont)
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
    mSpellFont{spellFont},
    mGameFont{gameFont},
    mLayout{sLayoutFile},
    mActiveSpells{},
    mCompass{
        glm::vec2{144,121},
        glm::vec2{32,12},
        std::get<glm::vec2>(icons.GetCompass())
            + glm::vec2{0, 1},
        std::get<Graphics::SpriteSheetIndex>(icons.GetCompass()),
        std::get<Graphics::TextureIndex>(icons.GetCompass())
    },
    mButtons{},
    mCharacters{},
    mBookmarkPopup{
        glm::vec2{60, 35},
        glm::vec2{200, 55},
        mGameFont,
        "",
        []{}
    },
    mLogger{Logging::LogState::GetLogger("Gui::MainView")}
{
    const auto& snippet = BAK::DialogStore::Get().GetSnippet(
        BAK::DialogSources::mBookmarkCheck);
    const auto popup = snippet.GetPopup();
    assert(popup);
    mBookmarkPopup.SetPosition(popup->mPos);
    mBookmarkPopup.SetDimensions(popup->mDims);
    mBookmarkPopup.SetText(snippet.GetText(), true);
    mBookmarkPopup.SetInactive();

    mButtons.reserve(mLayout.GetSize());

    for (unsigned i = 0; i < mLayout.GetSize(); i++)
    {
        const auto& widget = mLayout.GetWidget(i);
        switch (widget.mWidget)
        {
        case 3: //REQ_IMAGEBUTTON
        case 4: //REQ_IMAGEBUTTON toggle variant (e.g. sSnapToRoad)
        {
            const auto textures = icons.GetButtonTextures(widget.mImage);
            const auto& button = icons.GetButton(widget.mImage);
            assert(std::get<Graphics::SpriteSheetIndex>(button)
                == textures.mSpriteSheet);
            mButtons.emplace_back(
                mLayout.GetWidgetLocation(i),
                mLayout.GetWidgetDimensions(i),
                textures,
                [this, buttonIndex=i]{ HandleButton(buttonIndex); },
                []{});

            mButtons.back().CenterImage(std::get<glm::vec2>(button));
            // Not sure why the dims aren't right to begin with for these buttons
            if (i == sForward || i == sBackward)
            {
                mButtons.back().AdjustPosition(
                    glm::vec2{-mButtons.back().GetDimensions().x / 4 + 1.5, 0});
            }
        }
            break;
        default:
            mLogger.Info() << "Unhandled: " << i << "\n";
            break;
        }
    }

    AddChildren();
}

void MainView::SetHeading(BAK::GameHeading heading)
{
    mCompass.SetHeading(heading);
}

void MainView::HandleButton(unsigned buttonIndex)
{
    switch (buttonIndex)
    {
    case sCast:
        mGuiManager.ShowCast(false);
        break;
    case sCamp:
        mGuiManager.ShowCamp(false, nullptr);
        break;
    case sFullMap:
        mGuiManager.ShowOverheadMap();
        break;
    case sBookmark:
        mShowingBookmarkDialog = true;
        mNeedRefresh = true;
        break;
    case sMainMenu:
        mGuiManager.EnterMainMenu(true);
        break;
    case sForward:
        mGuiManager.MoveForward();
        StartMovementRepeat(sForward);
        break;
    case sBackward:
        mGuiManager.MoveBackward();
        StartMovementRepeat(sBackward);
        break;
    case sRotateLeft:
        mGuiManager.RotateLeft();
        StartMovementRepeat(sRotateLeft);
        break;
    case sRotateRight:
        mGuiManager.RotateRight();
        StartMovementRepeat(sRotateRight);
        break;
    case sSnapToRoad:
        mGuiManager.ToggleSnapToRoad();
        break;
    default:
        break;
    }
}

void MainView::SetCanSaveBookmark(bool canSaveBookmark)
{
    mCanSaveBookmark = canSaveBookmark;
    mNeedRefresh = true;
}

void MainView::RefreshCompass()
{
    // A sub-screen just came up (or MainView otherwise stopped being the top
    // screen). The AnimatorStore keeps ticking under pushed sub-screens, so a
    // held movement button's repeat must be stopped here or it would keep
    // firing Move*/Rotate* into a dialog/camp/cast screen (ROADMAP 4.5).
    if (!mGuiManager.InMainView())
    {
        StopMovementRepeat();
    }

    // Rebuild children. With the compass-always behavior restored this no longer
    // toggles the compass, but it's harmless and keeps MainView's child tree in
    // sync on every push/pop (ROADMAP 4.5).
    AddChildren();
}

bool MainView::OnMouseEvent(const MouseEvent& event)
{
    // Any mouse-button release ends a hold-to-repeat, regardless of where the
    // cursor is (GLFW delivers the release even after a drag-off). Only one
    // movement button can be held at a time, so this is unambiguous.
    if (std::holds_alternative<LeftMouseRelease>(event))
    {
        StopMovementRepeat();
    }

    if (mShowingBookmarkDialog)
    {
        if (std::holds_alternative<LeftMousePress>(event))
        {
            mGuiManager.SaveBookmark();
        }

        if (std::holds_alternative<LeftMousePress>(event)
            || std::holds_alternative<RightMousePress>(event))
        {
            mShowingBookmarkDialog = false;
            mNeedRefresh = true;
        }

        if (mNeedRefresh)
        {
            AddChildren();
            mNeedRefresh = false;
        }

        return true;
    }

    const bool handled = Widget::OnMouseEvent(event);

    if (mNeedRefresh)
    {
        AddChildren();
        mNeedRefresh = false;
    }

    return handled;
}

void MainView::StartMovementRepeat(unsigned buttonIndex)
{
    StopMovementRepeat();

    auto repeat = [this, buttonIndex]{
        switch (buttonIndex)
        {
        case sForward:     mGuiManager.MoveForward();  break;
        case sBackward:    mGuiManager.MoveBackward(); break;
        case sRotateLeft:  mGuiManager.RotateLeft();   break;
        case sRotateRight: mGuiManager.RotateRight();  break;
        default: break;
        }
    };

    auto animator = std::make_unique<TickAnimator>(
        sMovementRepeatSeconds,
        std::move(repeat));
    // Keep a raw handle before handing ownership to the AnimatorStore so we can
    // Stop() the repeat on release / sub-screen push. The store frees the
    // animator once IsAlive() is false.
    mMovementRepeatAnimator = animator.get();
    mGuiManager.AddAnimator(std::move(animator));
}

void MainView::StopMovementRepeat()
{
    if (mMovementRepeatAnimator)
    {
        mMovementRepeatAnimator->Stop();
        mMovementRepeatAnimator = nullptr;
    }
}

void MainView::UpdatePartyMembers(const BAK::GameState& gameState)
{
    ClearChildren();

    mCharacters.clear();
    mCharacters.reserve(3);

    const auto& party = gameState.GetParty();
    mLogger.Spam() << "Updating Party: " << party<< "\n";
    BAK::ActiveCharIndex person{0};
    do
    {
        const auto [spriteSheet, image, dimss] = mIcons.GetCharacterHead(
            party.GetCharacter(person).GetIndex().mValue);
        mCharacters.emplace_back(
            mLayout.GetWidgetLocation(person.mValue + sCharacterWidgetBegin),
            mLayout.GetWidgetDimensions(person.mValue + sCharacterWidgetBegin),
            spriteSheet,
            image,
            image,
            [this, character=person]{
                ShowInventory(character);
            },
            [this, character=person]{
                ShowPortrait(character);
            }
        );
        
        person = party.NextActiveCharacter(person);
    } while (person != BAK::ActiveCharIndex{0});

    auto pos = glm::vec2{140, 1};

    // FIXME: Update these whenever time changes...
    mActiveSpells.clear();
    for (std::uint16_t i = 0; i < 6; i++)
    {
        if (gameState.GetSpellActive(BAK::StaticSpells{i}))
        {
            auto spellI = BAK::sStaticSpellMapping[i];
            mActiveSpells.emplace_back(Gui::Widget{
                Graphics::DrawMode::Sprite,
                mSpellFont.GetSpriteSheet(),
                static_cast<Graphics::TextureIndex>(
                    mSpellFont.GetFont().GetIndex(spellI)),
                Graphics::ColorMode::Texture,
                glm::vec4{1.2f, 0.f, 0.f, 1.f},
                pos,
                glm::vec2{
                    mSpellFont.GetFont().GetWidth(spellI),
                    mSpellFont.GetFont().GetHeight()},
                true
            });
            pos += glm::vec2{mSpellFont.GetFont().GetWidth(spellI) + 1, 0};
        }
    }

    AddChildren();
}

void MainView::ShowPortrait(BAK::ActiveCharIndex character)
{
    mGuiManager.ShowCharacterPortrait(character);
}

void MainView::ShowInventory(BAK::ActiveCharIndex character)
{
    mGuiManager.ShowInventory(character);
}

void MainView::AddChildren()
{
    ClearChildren();
    for (unsigned i = 0; i < mButtons.size(); i++)
    {
        if (i == sBookmark && !mCanSaveBookmark)
            continue;
        AddChildBack(&mButtons[i]);
    }
    for (auto& spell : mActiveSpells)
    {
        AddChildBack(&spell);
    }

    // Compass always renders — original behavior preserved on Camp/Cast (the
    // 4K ENCAMP.PNG override is transparent at the slot, so MainView's compass
    // widget shows through sub-screen backgrounds). ROADMAP 4.5.
    AddChildBack(&mCompass);

    for (auto& character : mCharacters)
        AddChildBack(&character);

    if (mShowingBookmarkDialog)
    {
        AddChildBack(&mBookmarkPopup);
    }
}

}
