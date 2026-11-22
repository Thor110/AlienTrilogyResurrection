#include "TestFramework.h"

#include "Formats/EngineModelFaceEdits.h"
#include "Formats/ModelLoader.h"
#include "Formats/RenderMesh.h"

#include <string>
#include <vector>

using namespace ALTEngine::Formats;

namespace
{
    void Chunk(std::vector<uint8_t>& out, const std::string& tag, const std::vector<uint8_t>& payload)
    {
        out.insert(out.end(), tag.begin(), tag.end());
        const uint32_t n = static_cast<uint32_t>(payload.size());
        out.push_back(uint8_t(n >> 24)); out.push_back(uint8_t(n >> 16)); out.push_back(uint8_t(n >> 8)); out.push_back(uint8_t(n));
        out.insert(out.end(), payload.begin(), payload.end());
        if (n & 1u) { out.push_back(0); }
    }

    void Le32(std::vector<uint8_t>& out, int32_t v)
    {
        for (int i = 0; i < 4; ++i) { out.push_back(uint8_t(uint32_t(v) >> (8 * i))); }
    }

    std::array<uint8_t, 20> Record(int32_t a, int32_t b, int32_t c, int32_t d, uint16_t tex, uint8_t routine, uint8_t marker)
    {
        std::vector<uint8_t> r;
        Le32(r, a); Le32(r, b); Le32(r, c); Le32(r, d);
        r.push_back(uint8_t(tex)); r.push_back(uint8_t(tex >> 8)); r.push_back(routine); r.push_back(marker);
        std::array<uint8_t, 20> out{};
        std::copy(r.begin(), r.end(), out.begin());
        return out;
    }

    // An odd-length MAP0 (so the padding is walked), then L000: one quad and
    // one triangle-as-quad over four vertices.
    std::vector<uint8_t> MakeLevel()
    {
        std::vector<uint8_t> model{ 'O', 'B', 'J', '1', 0, 0, 0, 0, 1, 2, 3, 4 };
        Le32(model, 2); Le32(model, 4);
        const auto q0 = Record(0, 1, 2, 3, 7, 0, 0);
        const auto q1 = Record(0, 1, 2, 2, 7, 2, 0x00);
        model.insert(model.end(), q0.begin(), q0.end());
        model.insert(model.end(), q1.begin(), q1.end());
        const int16_t verts[4][3] = { { 0, 0, 0 }, { 100, 0, 0 }, { 100, 0, 100 }, { 0, 0, 100 } };
        for (const auto& v : verts)
        {
            for (int16_t c : v) { model.push_back(uint8_t(c)); model.push_back(uint8_t(uint16_t(c) >> 8)); }
            model.push_back(0); model.push_back(0);
        }
        model[20 + 20 + 18] |= 0x80;   // the last face ends the model
        std::vector<uint8_t> body;
        Chunk(body, "MAP0", { 9, 9, 9 });
        Chunk(body, "L000", model);
        std::vector<uint8_t> file{ 'F', 'O', 'R', 'M', 0, 0, 0, 0, '0', '0', '0', '0' };
        file.insert(file.end(), body.begin(), body.end());
        return file;
    }

    EngineModelFaceEdit TurnOverFace1()
    {
        EngineModelFaceEdit e;
        e.target = "SECTxx/LxxxLEV.MAP";
        e.section = "L000";
        e.face = 1;
        e.was = Record(0, 1, 2, 2, 7, 0x82, 0);
        e.now = Record(1, 0, 2, 2, 7, 0x82, ENGINE_DOUBLE_SIDED);
        return e;
    }
}

TEST_CASE("Engine model face edits find a face past an odd-length section")
{
    const auto file = MakeLevel();
    const int64_t at = EngineModelFaceEdits::FaceOffset(file, "L000", 1);
    CHECK(at > 0);
    CHECK(file[size_t(at) + 16] == 7);
    CHECK(EngineModelFaceEdits::FaceOffset(file, "L000", 2) == -1);
    CHECK(EngineModelFaceEdits::FaceOffset(file, "L001", 0) == -1);
}

TEST_CASE("Engine model face edits apply once, revert exactly, and leave other files alone")
{
    const auto shipped = MakeLevel();
    auto file = shipped;
    const auto edit = TurnOverFace1();
    CHECK(EngineModelFaceEdits::Apply(file, edit) == EngineModelFaceOutcome::Applied);
    CHECK(file != shipped);
    CHECK(EngineModelFaceEdits::Apply(file, edit) == EngineModelFaceOutcome::AlreadyApplied);
    CHECK(EngineModelFaceEdits::Revert(file, edit));
    CHECK(file == shipped);
    CHECK(!EngineModelFaceEdits::Revert(file, edit));

    auto other = shipped;
    auto wrong = edit;
    wrong.face = 0;
    CHECK(EngineModelFaceEdits::Apply(other, wrong) == EngineModelFaceOutcome::Mismatch);
    CHECK(other == shipped);
}

TEST_CASE("Engine model face edits parse from EngineFilePatches.json")
{
    std::vector<std::string> errors;
    const std::string json = R"({"files": [], "modelFaces": [
        {"target": "SECT32/L381LEV.MAP", "section": "L000", "face": 1,
         "was": "0700000004000000030000000300000029010200",
         "now": "0400000007000000030000000300000029010205", "note": "n"},
        {"target": "x", "section": "L00", "face": 1, "was": "00", "now": "00"}]})";
    const auto edits = EngineModelFaceEdits::Parse(json, errors);
    CHECK(edits.size() == 1);
    CHECK(errors.size() == 1);
    CHECK(edits[0].now[19] == ENGINE_DOUBLE_SIDED);
    CHECK(edits[0].was[0] == 7);
}

TEST_CASE("Door and lift meshes draw a marked face from both sides; other models do not")
{
    auto file = MakeLevel();
    CHECK(EngineModelFaceEdits::Apply(file, TurnOverFace1()) == EngineModelFaceOutcome::Applied);
    ModelMesh mesh;
    mesh.vertices = { { 0, 0, 0, 0 }, { 100, 0, 0, 0 }, { 100, 0, 100, 0 }, { 0, 0, 100, 0 } };
    ModelQuad marked{ 1, 0, 2, 2, 7, 2, ENGINE_DOUBLE_SIDED };
    ModelQuad plain{ 0, 1, 2, 2, 7, 2, 0 };
    std::vector<BxRectangle> rects(8);
    mesh.quads = { marked };
    const auto honoured = BuildRenderMeshPerGroup(mesh, rects, true);
    const auto ignored = BuildRenderMeshPerGroup(mesh, rects, false);
    mesh.quads = { plain };
    const auto unmarked = BuildRenderMeshPerGroup(mesh, rects, true);
    CHECK(honoured[0].indices.size() == 2 * ignored[0].indices.size());
    CHECK(unmarked[0].indices.size() == ignored[0].indices.size());
}
