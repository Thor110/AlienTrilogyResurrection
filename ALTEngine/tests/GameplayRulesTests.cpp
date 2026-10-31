#include "TestFramework.h"

#include "Screens/GameplayLevelBuild.h"
#include "Screens/GameplayObjects.h"

using namespace ALTEngine::Screens;
namespace MI = ALTEngine::Formats::ModelIndices;

TEST_CASE("Objects: each type's model (FUN_00037184)")
{
    using namespace GameplayObjects;
    CHECK_EQ(Obj3DIndexForCrateType(0x14, false), MI::Obj3D::SingleCrate);
    CHECK_EQ(Obj3DIndexForCrateType(0x14, true), MI::Obj3D::EggHusk);
    CHECK_EQ(Obj3DIndexForCrateType(0x17, false), MI::Obj3D::ExplosiveBarrel);
    CHECK_EQ(Obj3DIndexForCrateType(0x19, false), MI::Obj3D::DoubleCrate);
    CHECK_EQ(Obj3DIndexForCrateType(0x20, false), MI::Obj3D::StasisPodCover);
    CHECK_EQ(Obj3DIndexForCrateType(0x23, false), MI::Obj3D::SteelCoil + 2);
    // A switch shows both lights off until its state says otherwise.
    CHECK_EQ(Obj3DIndexForCrateType(0x18, false), MI::Obj3D::SwitchRedLeftLight + static_cast<int>(SwitchState::BothOff));
    CHECK_EQ(SwitchFamilyBase(0x18, true), MI::Obj3D::BoneshipSwitchRedLeftLight1);
    CHECK_EQ(SwitchFamilyBase(0x14, false), -1);
}

TEST_CASE("Objects: the modelless types, episode 3 and the glass texture")
{
    using namespace GameplayObjects;
    CHECK(IsInvisibleObjectType(0x15));
    CHECK(IsInvisibleObjectType(WINDOW_OBJECT_TYPE));
    CHECK(IsInvisibleObjectType(0x1d));
    CHECK(!IsInvisibleObjectType(CRYOPOD_OBJECT_TYPE));
    CHECK(EpisodeThreeObjects("311"));
    CHECK(EpisodeThreeObjects("909"));
    CHECK(!EpisodeThreeObjects("111"));
    CHECK_EQ(GlassTextureIndex("131"), 0x118);
    CHECK_EQ(GlassTextureIndex("211"), 0x158);
    CHECK_EQ(GlassTextureIndex("212"), 0x14f);
}

TEST_CASE("Level build: a texture rect is cut out of its page")
{
    ALTEngine::Formats::BndTextureSet set;
    ALTEngine::Formats::BndTexture page;
    page.width = 4; page.height = 4;
    page.rgba.resize(4 * 4 * 4);
    for (int i = 0; i < 16; ++i) { page.rgba[static_cast<size_t>(i) * 4] = static_cast<uint8_t>(i); page.rgba[static_cast<size_t>(i) * 4 + 3] = 255; }
    set.textures.push_back(page);
    set.uvRects.push_back({ 1, 2, 2, 2, 0, 0 });
    const auto cut = GameplayLevelBuild::CutTextureRect(set, 0);
    REQUIRE(cut.has_value());
    CHECK_EQ(cut->width, 2);
    CHECK_EQ(cut->height, 2);
    CHECK_EQ(static_cast<int>(cut->rgba[0]), 2 * 4 + 1);    // page pixel (1, 2)
    CHECK_EQ(static_cast<int>(cut->rgba[3 * 4]), 3 * 4 + 2); // page pixel (2, 3)
    CHECK(!GameplayLevelBuild::CutTextureRect(set, 1).has_value());
}

TEST_CASE("Level build: the cryopod cover takes the level's palette entry 2, times 4")
{
    ALTEngine::Formats::ModelMesh mesh;
    mesh.vertices = { { -100, 0, 0, 0 }, { -50, 0, 0, 0 }, { -50, 50, 0, 0 } };
    ALTEngine::Formats::ModelQuad tri{};
    tri.a = 0; tri.b = 1; tri.c = 2; tri.d = -1;
    mesh.quads = { tri };
    std::vector<uint8_t> palette(256 * 3, 0);
    palette[6] = 2; palette[7] = 0; palette[8] = 0;   // L131's: near black
    const auto cover = GameplayLevelBuild::BuildCryopodCover(mesh, palette);
    REQUIRE(cover.has_value());
    CHECK_EQ(cover->colour[0], 8);
    CHECK_EQ(cover->colour[1], 0);
    CHECK_EQ(static_cast<int>(cover->mesh.translucentStart), 0);
    CHECK_EQ(cover->texture.width, 1);
    CHECK(cover->openSign < 0.0f);   // the model lies on -x, so it tips that way up
    CHECK(!GameplayLevelBuild::BuildCryopodCover(mesh, {}).has_value());
}

#include "Formats/ExplosionGraphics.h"

TEST_CASE("Explosions: FUN_000429f0's distance fade past 0x1000")
{
    CHECK(ALTEngine::Formats::ExplosionGraphics::EffectFade(0) == 1.0f);
    CHECK(ALTEngine::Formats::ExplosionGraphics::EffectFade(0xfff) == 1.0f);
    // From 0x1000 the colour goes through FUN_0004ecdc at d * 4 - 0x1200:
    // nothing taken off until (that / 0x50) passes 0xaa.
    CHECK(ALTEngine::Formats::ExplosionGraphics::EffectFade(0x1000) == 255.0f / 256.0f);
    CHECK(ALTEngine::Formats::ExplosionGraphics::EffectFade(0x11c0) == 255.0f / 256.0f);
    // At the 0x1e00 cut: (0x7800 - 0x1200) / 0x50 - 0xaa = 156 taken off.
    CHECK(ALTEngine::Formats::ExplosionGraphics::EffectFade(0x1e00) == static_cast<float>(0xff - 156) / 256.0f);
    float last = 2.0f;
    for (int d = 0x1000; d < 0x1e00; d += 0x40) { const float f = ALTEngine::Formats::ExplosionGraphics::EffectFade(float(d)); CHECK(f <= last); last = f; }
}
