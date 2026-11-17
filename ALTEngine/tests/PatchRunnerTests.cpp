#include "TestFramework.h"

#include "Formats/PatchRunner.h"

#include <filesystem>
#include <fstream>
#include <vector>

using ALTEngine::Formats::BinaryEdit;
using ALTEngine::Formats::PatchOperation;
using ALTEngine::Formats::PatchOutcome;
using ALTEngine::Formats::PatchRunner;

namespace
{
    std::vector<uint8_t> ReadAll(const std::filesystem::path& path)
    {
        std::ifstream in(path, std::ios::binary);
        return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }
}

TEST_CASE("PatchRunner: a later edit to a file whose first edit is already applied is still written")
{
    const auto root = std::filesystem::temp_directory_path() / "altengine_patchrunner_test";
    std::filesystem::create_directories(root / "SECT32");
    const auto file = root / "SECT32" / "L351LEV.MAP";
    {
        std::ofstream out(file, std::ios::binary);
        const std::vector<uint8_t> bytes{ 0xAA, 0x00, 0x00, 0x00 };   // the first fix is already in
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    PatchOperation op;
    op.targetFile = "SECT32/L351LEV.MAP";
    op.edits.push_back(BinaryEdit{ 0, { 0xAA } });   // an older fix
    op.edits.push_back(BinaryEdit{ 2, { 0xBB } });   // added later

    auto results = PatchRunner::ApplyAll(root, { op });
    CHECK(results.size() == 1);
    CHECK(results[0].outcome == PatchOutcome::Applied);
    CHECK(ReadAll(file) == std::vector<uint8_t>({ 0xAA, 0x00, 0xBB, 0x00 }));

    results = PatchRunner::ApplyAll(root, { op });
    CHECK(results[0].outcome == PatchOutcome::AlreadyApplied);

    std::filesystem::remove_all(root);
}

TEST_CASE("PatchRunner: a byte rewritten by a later edit still reads as applied")
{
    const auto root = std::filesystem::temp_directory_path() / "altengine_patchrunner_test2";
    std::filesystem::create_directories(root);
    const auto file = root / "F.MAP";
    {
        std::ofstream out(file, std::ios::binary);
        const std::vector<uint8_t> bytes{ 0x00, 0x00 };
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    PatchOperation op;
    op.targetFile = "F.MAP";
    op.edits.push_back(BinaryEdit{ 0, { 0x04 } });
    op.edits.push_back(BinaryEdit{ 0, { 0x01, 0x04 } });

    CHECK(PatchRunner::ApplyAll(root, { op })[0].outcome == PatchOutcome::Applied);
    CHECK(ReadAll(file) == std::vector<uint8_t>({ 0x01, 0x04 }));
    CHECK(PatchRunner::ApplyAll(root, { op })[0].outcome == PatchOutcome::AlreadyApplied);
    std::filesystem::remove_all(root);
}

#include "Formats/LevelLoader.h"
#include <string>

TEST_CASE("PatchRunner: a patched file is replaced whole, with no temporary file left behind")
{
    const auto root = std::filesystem::temp_directory_path() / "altengine_patchrunner_replace";
    std::filesystem::create_directories(root / "SECT12");
    const auto file = root / "SECT12" / "L141LEV.MAP";
    {
        std::ofstream out(file, std::ios::binary);
        const std::vector<uint8_t> bytes(64, 0x11);
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    PatchOperation op;
    op.targetFile = "SECT12/L141LEV.MAP";
    op.edits.push_back(BinaryEdit{ 10, { 0x22, 0x33 } });
    const auto results = PatchRunner::ApplyAll(root, { op });
    CHECK(results.size() == 1);
    CHECK(results[0].outcome == PatchOutcome::Applied);
    const auto after = ReadAll(file);
    CHECK_EQ(after.size(), size_t(64));
    CHECK(after[10] == 0x22 && after[11] == 0x33 && after[12] == 0x11);
    CHECK(!std::filesystem::exists(root / "SECT12" / "L141LEV.MAP.tmp"));
    std::filesystem::remove_all(root);
}

TEST_CASE("LevelLoader: an empty or cut-short level names the file")
{
    const auto root = std::filesystem::temp_directory_path() / "altengine_empty_level";
    std::filesystem::create_directories(root);
    const auto file = root / "L141LEV.MAP";
    { std::ofstream out(file, std::ios::binary); out.write("FOR", 3); }
    std::string message;
    try { ALTEngine::Formats::LevelLoader::Load(file); }
    catch (const std::exception& e) { message = e.what(); }
    CHECK(message.find("L141LEV.MAP") != std::string::npos);
    CHECK(message.find("cut short") != std::string::npos);
    std::filesystem::remove_all(root);
}

#include "Formats/PatchLoader.h"

TEST_CASE("PatchLoader: every level patch runs before every texture patch, one operation per file and stage")
{
    const auto path = std::filesystem::temp_directory_path() / "altengine_patchloader_stages.json";
    {
        std::ofstream out(path);
        out << R"({"patches": [
            {"target": "A.MAP", "stage": "texture", "note": "t1", "edits": [{"offset": "0x10", "bytes": ["0x01"]}]},
            {"target": "B.MAP", "stage": "level", "note": "l1", "edits": [{"offset": "0x20", "bytes": ["0x02"]}]},
            {"target": "A.MAP", "stage": "level", "note": "l2", "edits": [{"offset": "0x30", "bytes": ["0x03"]}]},
            {"target": "A.MAP", "stage": "texture", "note": "t2", "edits": [{"offset": "0x40", "bytes": ["0x04"]}]},
            {"target": "C.MAP", "edits": [{"offset": "0x50", "bytes": ["0x05"]}]}
        ]})";
    }
    const auto ops = ALTEngine::Formats::PatchLoader::Load(path);
    REQUIRE(ops.size() == 4);
    CHECK(ops[0].stage == "level" && ops[0].targetFile == "B.MAP");
    CHECK(ops[1].stage == "level" && ops[1].targetFile == "A.MAP");
    CHECK(ops[2].stage == "texture" && ops[2].targetFile == "A.MAP");
    CHECK_EQ(ops[2].edits.size(), size_t(2));                         // t1 and t2 together, in order
    CHECK(ops[3].stage == "texture" && ops[3].targetFile == "C.MAP");   // no stage given: a texture patch

    {
        std::ofstream out(path);
        out << R"({"patches": [{"target": "A.MAP", "stage": "models", "edits": [{"offset": "0x10", "bytes": ["0x01"]}]}]})";
    }
    bool threw = false;
    try { (void)ALTEngine::Formats::PatchLoader::Load(path); } catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);
    std::filesystem::remove(path);
}
