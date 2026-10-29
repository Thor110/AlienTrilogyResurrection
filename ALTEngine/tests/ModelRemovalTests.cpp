#include "TestFramework.h"

#include "Formats/ModelRemoval.h"

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

    // FORM, a 4-byte MAP0 whose byte 2 stands for a door record's model
    // number, then D000, D001 (a copy of D000) and an odd-sized L000.
    std::vector<uint8_t> MakeLevel()
    {
        std::vector<uint8_t> body;
        Chunk(body, "MAP0", { 9, 9, 1, 9 });
        Chunk(body, "D000", { 1, 2, 3, 4 });
        Chunk(body, "D001", { 1, 2, 3, 4 });
        Chunk(body, "L000", { 5, 6, 7 });
        std::vector<uint8_t> file{ 'F', 'O', 'R', 'M', 0, 0, 0, 0, '0', '0', '0', '0' };
        file.insert(file.end(), body.begin(), body.end());
        const uint32_t n = static_cast<uint32_t>(file.size() - 8);
        file[4] = uint8_t(n >> 24); file[5] = uint8_t(n >> 16); file[6] = uint8_t(n >> 8); file[7] = uint8_t(n);
        return file;
    }

    ModelRemoval RemoveD001()
    {
        const uint8_t payload[] = { 1, 2, 3, 4 };
        ModelRemoval r;
        r.targetFile = "L.MAP";
        r.chunks.push_back({ "D001", 4, Crc32(payload, 4) });
        r.recordEdits.push_back(BinaryEdit{ 12 + 8 + 2, { 0 } });   // MAP0 byte 2: model 1 -> 0
        return r;
    }
}

TEST_CASE("Crc32: the standard check value")
{
    const std::string s = "123456789";
    CHECK(Crc32(reinterpret_cast<const uint8_t*>(s.data()), s.size()) == 0xCBF43926u);
}

TEST_CASE("ModelRemoval: drops the chunk, renumbers the record, fixes the FORM size, then reads as applied")
{
    std::vector<uint8_t> file = MakeLevel();
    const size_t before = file.size();
    const ModelRemoval removal = RemoveD001();
    std::string error;
    CHECK(ApplyModelRemoval(file, removal, error) == ModelRemovalOutcome::Applied);
    CHECK(file.size() == before - 12);
    CHECK(((uint32_t(file[4]) << 24) | (uint32_t(file[5]) << 16) | (uint32_t(file[6]) << 8) | file[7]) == file.size() - 8);
    CHECK(file[12 + 8 + 2] == 0);
    // D000 is followed straight by L000, its pad byte kept.
    CHECK(std::string(file.begin() + 12 + 12, file.begin() + 12 + 16) == "D000");
    CHECK(std::string(file.begin() + 12 + 24, file.begin() + 12 + 28) == "L000");
    CHECK(file.size() == 12 + 12 + 12 + 12);

    const std::vector<uint8_t> once = file;
    CHECK(ApplyModelRemoval(file, removal, error) == ModelRemovalOutcome::AlreadyApplied);
    CHECK(file == once);
}

TEST_CASE("ModelRemoval: refuses a chunk that is not the expected one, and a half-removed file")
{
    std::vector<uint8_t> file = MakeLevel();
    ModelRemoval wrong = RemoveD001();
    wrong.chunks[0].crc32 ^= 1u;
    std::string error;
    const std::vector<uint8_t> original = file;
    CHECK(ApplyModelRemoval(file, wrong, error) == ModelRemovalOutcome::Failed);
    CHECK(file == original);

    ModelRemoval two = RemoveD001();
    const uint8_t l000[] = { 5, 6, 7 };
    two.chunks.push_back({ "L001", 3, Crc32(l000, 3) });   // not in the file
    CHECK(ApplyModelRemoval(file, two, error) == ModelRemovalOutcome::Failed);
    CHECK(file == original);

    // Chunk gone but the record never renumbered: not "already applied".
    ModelRemoval r = RemoveD001();
    CHECK(ApplyModelRemoval(file, r, error) == ModelRemovalOutcome::Applied);
    file[12 + 8 + 2] = 1;
    CHECK(ApplyModelRemoval(file, r, error) == ModelRemovalOutcome::Failed);
}
