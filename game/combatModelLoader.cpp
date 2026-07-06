#include "game/combatModelLoader.hpp"

#include "bak/combat/combatModel.hpp"
#include "bak/fileBufferFactory.hpp"
#include "bak/image.hpp"
#include "bak/imageStore.hpp"
#include "bak/model.hpp"
#include "bak/monster.hpp"
#include "bak/palette.hpp"
#include "bak/resourceNames.hpp"
#include "bak/textureFactory.hpp"
#include "bak/worldFactory.hpp"

#include "com/logger.hpp"
#include "com/ostream.hpp"
#include "com/string.hpp"

#include "graphics/meshObject.hpp"
#include "graphics/texture.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Game {

CombatModelLoader::CombatModelLoader()
{
    const auto& logger = Logging::Logger("CombatModelLoader");
    auto tblBuf = BAK::FileBufferFactory::Get().CreateDataBuffer(sCombatModels);
    auto models = BAK::LoadTBL(tblBuf);
    for (unsigned i = 0; i < models.size(); i++)
    {
        if (models[i].mEntityType == 0 && models[i].mSprite > 0)
        {
            logger.Debug() << "Loaded Combatant Model #" << i << " " << models[i].mName << "\n";
            mCombatModels.emplace_back(BAK::CombatModel{models[i]});
            LoadMonsterSprites(BAK::MonsterIndex{i});
        }
        else
        {
            mCombatModels.emplace_back(std::nullopt);
            mCombatModelDatas.emplace_back(std::nullopt);
        }
    }
}

void CombatModelLoader::LoadMonsterSprites(BAK::MonsterIndex m)
{
    const auto& logger = Logging::LogState::GetLogger(__FUNCTION__);
    logger.Spam() << "Loading monster: " << m << "\n";
    const auto& mnames = BAK::MonsterNames::Get();
    auto textureStore = Graphics::TextureStore{};
    auto monster = mnames.GetMonster(m);
    logger.Spam() << "Monster Sprites: " << monster << "\n";
    const auto prefix = ToUpper(monster.mPrefix);
    if (prefix.empty())
    {
        return;
    }

    auto pal = BAK::Palette{BAK::ZoneLabel{1}.GetPalette()};
    if (monster.mColorSwap <= 9)
    {
        auto ss = std::stringstream{};
        ss << "CS";
        ss << +monster.mColorSwap << ".DAT";
        const auto cs = BAK::ColorSwap{ss.str()};
        pal = BAK::Palette{pal, cs};
    }

    auto LoadImages = [&](auto suffix)
    {
        std::stringstream ss{};
        ss << prefix << +suffix << ".BMX";
        const auto bmxName = ss.str();
        auto fb = BAK::FileBufferFactory::Get().CreateDataBuffer(bmxName);
        const auto images = BAK::LoadImages(fb);
        // Task 1.8: per-variant 4K substitute seam. The color-swap is already
        // baked into `pal` for the proprietary fallback; a substitute PNG (final
        // RGBA colors) bypasses the palette entirely. See textureFactory.cpp.
        BAK::TextureFactory::AddToTextureStore(textureStore, bmxName, images, pal, monster.mColorSwap);
    };

    std::vector<std::size_t> offsets{0};
    LoadImages(monster.mSuffix0);
    offsets.emplace_back(textureStore.size());
    LoadImages(monster.mSuffix1);
    offsets.emplace_back(textureStore.size());
    LoadImages(monster.mSuffix2);

    logger.Spam() << "Loaded all textures: " << textureStore.size()
        << " Offsets: " << offsets << "\n";

    assert(mCombatModels[m.mValue]);
    const auto& model = *mCombatModels[m.mValue];
    Graphics::MeshObjectStorage objects{};
    std::unordered_map<AnimationRequest, AnimationMeta> offsetMap{};
    std::vector<Graphics::MeshObjectStorage::OffsetAndLength> objectOffsets{};
    using enum BAK::Direction;
    std::stringstream ss{};
    for (const auto& animType : model.GetSupportedAnimations())
    {
        logger.Spam() << "AnimatioNType: " << BAK::ToString(animType) << "(" << +std::to_underlying(animType) << ")\n";
        for (const auto& direction : model.GetDirections(animType))
        {
            logger.Spam() << "Direction: " << BAK::ToString(direction) << "\n";
            const auto& animation = model.GetAnimation(animType, direction);
            offsetMap.emplace(
                AnimationRequest{animType, direction},
                AnimationMeta{objects.size(), animation.mImageIndices.size()});

            for (const auto& index : animation.mImageIndices)
            {
                assert(animation.mSpriteFileIndex < offsets.size());
                logger.Spam() << "SFI: " << +animation.mSpriteFileIndex << " offset: "
                    << offsets[animation.mSpriteFileIndex] << " index: " << +index << "\n";
                const auto fullOffset = offsets[animation.mSpriteFileIndex] + index;
                if (fullOffset >= textureStore.size())
                {
                    logger.Error() << "Image offset past loaded textures:" << fullOffset << " >= size: " << textureStore.size() << "\n";
                    continue;
                }
                auto zoneItem = BAK::ZoneItem(fullOffset, textureStore.GetTexture(fullOffset));
                ss.clear();
                ss << objects.size();
                auto offset = objects.AddObject(
                    ss.str(),
                    BAK::ZoneItemToMeshObject(zoneItem, textureStore));
                objectOffsets.emplace_back(offset);
            }
        }
    }

    auto renderData = Graphics::RenderData{};
    // Task 3.1 (final rollout): 4K monster art now has a load path (Task 1.8), so
    // upload the combat sprite array with mipmapped linear filtering. Wrap is
    // ClampToEdge (combat sprites are individual cutouts, not tiles -- unlike the
    // zone/terrain RenderData caller which keeps the Repeat default). The filter
    // applies to the whole shared array, so remaining proprietary low-res combat
    // sprites are bilinearly smoothed until their 4K substitutes land.
    renderData.LoadData(
        objects,
        textureStore.GetTextures(),
        textureStore.GetMaxDim(),
        Graphics::FilterMode::LinearMipmap,
        Graphics::WrapMode::ClampToEdge);
    mCombatModelDatas.emplace_back(
        std::make_optional(
            CombatModelData{
                std::move(renderData),
                std::move(offsetMap),
                std::move(objectOffsets)}));
    Logging::LogDebug(__FUNCTION__) <<"Loaded model: " << prefix << " at index: " << mCombatModelDatas.size() - 1 << "\n";
}

}

namespace std {

std::size_t hash<Game::AnimationRequest>::operator()(const Game::AnimationRequest& t) const noexcept
{
    return std::hash<std::size_t>{}(
        std::to_underlying(t.mAnimation) | (std::to_underlying(t.mDirection) << 8));
}

}
