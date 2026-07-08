#include "gui/overheadMap.hpp"

#include "gui/IGuiManager.hpp"
#include "gui/clickButton.hpp"
#include "gui/core/widget.hpp"

#include "bak/constants.hpp"
#include "bak/coordinates.hpp"
#include "bak/gameState.hpp"
#include "bak/overheadMapClassifier.hpp"
#include "bak/startupFiles.hpp"
#include "bak/types.hpp"
#include "bak/worldFactory.hpp"

#include "com/logger.hpp"

#include <glm/glm.hpp>

#include <GLFW/glfw3.h>

#include <cmath>
#include <cstddef>
#include <memory>
#include <variant>

namespace Gui {

namespace {

// Stylized top-down terrain colors (RGBA). Tunable calibration constants;
// the original 1993 overhead map uses a similar muted palette.
glm::vec4 TerrainColor(BAK::Terrain t)
{
    switch (t)
    {
    case BAK::Terrain::Road:      return glm::vec4{0.45f, 0.30f, 0.15f, 1.0f};
    case BAK::Terrain::Path:      return glm::vec4{0.30f, 0.55f, 0.25f, 1.0f};
    case BAK::Terrain::River:     return glm::vec4{0.20f, 0.35f, 0.65f, 1.0f};
    case BAK::Terrain::Waterfall: return glm::vec4{0.35f, 0.55f, 0.80f, 1.0f};
    case BAK::Terrain::Bank:      return glm::vec4{0.62f, 0.55f, 0.35f, 1.0f};
    case BAK::Terrain::Dirt:      return glm::vec4{0.50f, 0.40f, 0.25f, 1.0f};
    case BAK::Terrain::Sand:      return glm::vec4{0.70f, 0.62f, 0.40f, 1.0f};
    case BAK::Terrain::Ground:
    default:                      return glm::vec4{0.24f, 0.34f, 0.20f, 1.0f};
    }
}

constexpr glm::vec4 sStructureColor{0.18f, 0.18f, 0.18f, 1.0f};
constexpr glm::vec4 sPlayerColor{0.85f, 0.15f, 0.15f, 1.0f};
constexpr glm::vec4 sPlayerNoseColor{0.95f, 0.85f, 0.55f, 1.0f};

// BAK heading is a 0..255 circle (0 = north). North maps to screen-up (-y).
constexpr double kFullHeading = 256.0;

} // namespace

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
    const auto def = BAK::LoadZoneDefDatInfo(mGameState.GetZone());
    mZoomMin  = def.mMinMapZoom;
    mZoomMax  = def.mMaxMapZoom;
    mZoomRate = def.mMapZoomRate;
    // Start at the most local (closest) view the zone defines.
    mZoom     = mZoomMax;
    RebuildMap();
}

bool OverheadMap::OnKeyEvent(const KeyEvent& event)
{
    if (!std::holds_alternative<KeyPress>(event))
        return false;
    const auto key = std::get<KeyPress>(event).mValue;
    if (key == GLFW_KEY_EQUAL)
    {
        StepZoom(-1); // zoom in: smaller view span
        return true;
    }
    if (key == GLFW_KEY_MINUS)
    {
        StepZoom(+1); // zoom out: larger view span
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
        mZoom = (mZoom > mZoomMin + mZoomRate) ? mZoom - mZoomRate : mZoomMin;
    else
        mZoom = (mZoom + mZoomRate < mZoomMax) ? mZoom + mZoomRate : mZoomMax;
    RebuildMap();
}

glm::vec2 OverheadMap::Project(glm::uvec2 bakPos) const
{
    const auto player = mGameState.GetLocation().mPosition; // glm::uvec2
    const float pxPerBAK = (static_cast<float>(kMapH) / static_cast<float>(mZoom)) * kZoomScale;
    const long dx = static_cast<long>(bakPos.x) - static_cast<long>(player.x);
    const long dy = static_cast<long>(bakPos.y) - static_cast<long>(player.y);
    // BAK y is up; screen y is down → negate Y. Player is centered on the map.
    return glm::vec2{
        sMapCenter.x + static_cast<float>(dx) * pxPerBAK,
        sMapCenter.y - static_cast<float>(dy) * pxPerBAK};
}

void OverheadMap::RebuildMap()
{
    // Drop the raw-ptr children before freeing the Widgets they point at.
    mClipRegion.ClearChildren();
    mTerrainQuads.clear();
    mStructureQuads.clear();
    mPlayerBody.reset();
    mPlayerNose.reset();

    const auto& tiles = mGuiManager.GetWorldTileStore().GetTiles();
    const auto player = mGameState.GetLocation().mPosition;
    const float pxPerBAK = (static_cast<float>(kMapH) / static_cast<float>(mZoom)) * kZoomScale;

    // Cull tiles whose center is outside the visible BAK extent plus one
    // tile of margin (so edge tiles still fill the frame).
    const float halfW = (kMapW * 0.5f) / pxPerBAK + BAK::gTileSize;
    const float halfH = (kMapH * 0.5f) / pxPerBAK + BAK::gTileSize;
    const float tilePx = BAK::gTileSize * pxPerBAK;

    for (const auto& world : tiles)
    {
        const auto tile = world.GetTile(); // tile coords (glm::uvec2)
        const glm::uvec2 centerBAK{
            static_cast<unsigned>(tile.x * BAK::gTileSize + BAK::gTileSize * 0.5f),
            static_cast<unsigned>(tile.y * BAK::gTileSize + BAK::gTileSize * 0.5f)};

        const float dx = static_cast<float>(static_cast<long>(centerBAK.x) - static_cast<long>(player.x));
        const float dy = static_cast<float>(static_cast<long>(centerBAK.y) - static_cast<long>(player.y));
        if (std::fabs(dx) > halfW || std::fabs(dy) > halfH)
            continue;

        const auto screen = Project(centerBAK);
        const auto terrain = BAK::ClassifyTileTerrain(world.GetItems());

        auto quad = std::make_unique<Widget>(
            RectTag{},
            glm::vec2{screen.x - tilePx * 0.5f, screen.y - tilePx * 0.5f},
            glm::vec2{tilePx, tilePx},
            TerrainColor(terrain),
            false);
        mClipRegion.AddChildBack(quad.get());
        mTerrainQuads.push_back(std::move(quad));

        // Structure markers (buildings / bridges / entrances / ...) for this tile.
        for (const auto& item : world.GetItems())
        {
            if (!BAK::IsOverheadMapStructure(item.GetZoneItem().GetEntityType()))
                continue;
            const auto sLoc = Project(item.GetBakLocation());
            constexpr float ss = 4.0f;
            auto sq = std::make_unique<Widget>(
                RectTag{},
                glm::vec2{sLoc.x - ss * 0.5f, sLoc.y - ss * 0.5f},
                glm::vec2{ss, ss},
                sStructureColor,
                false);
            mClipRegion.AddChildBack(sq.get());
            mStructureQuads.push_back(std::move(sq));
        }
    }

    // Party marker: red square on the player tile + a cream "nose" offset in
    // the facing direction (heading 0 = north = screen-up).
    const auto screen = Project(player);
    constexpr float body = 6.0f;
    mPlayerBody = std::make_unique<Widget>(
        RectTag{},
        glm::vec2{screen.x - body * 0.5f, screen.y - body * 0.5f},
        glm::vec2{body, body},
        sPlayerColor,
        false);
    mClipRegion.AddChildBack(mPlayerBody.get());

    const auto heading = mGameState.GetLocation().mHeading; // 0..255, 0 = north
    const double angle = static_cast<double>(heading) / kFullHeading * 6.283185307179586;
    constexpr float noseR = 6.0f;
    constexpr float nose = 3.0f;
    const glm::vec2 noseCenter{
        screen.x + static_cast<float>(std::sin(angle)) * noseR,
        screen.y - static_cast<float>(std::cos(angle)) * noseR};
    mPlayerNose = std::make_unique<Widget>(
        RectTag{},
        glm::vec2{noseCenter.x - nose * 0.5f, noseCenter.y - nose * 0.5f},
        glm::vec2{nose, nose},
        sPlayerNoseColor,
        false);
    mClipRegion.AddChildBack(mPlayerNose.get());
}

}