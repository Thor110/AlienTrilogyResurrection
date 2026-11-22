#include "InstallPipeline.h"

#include "Config.h"
#include "PlatformPaths.h"
#include "TestingSettings.h"
#include "MuthurConsole.h"

#include "../Game/LevelCleanup.h"
#include "../Game/TextureBlank.h"
#include "../Formats/LevelTrim.h"

#include "../Formats/CddaRipper.h"
#include "../Formats/DiscManifest.h"
#include "../Formats/EngineModelFaceEdits.h"
#include "../Formats/Installer.h"
#include "../Formats/ModelRemoval.h"
#include "../Formats/PatchLoader.h"
#include "../Formats/PatchRunner.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>

#include <nlohmann/json.hpp>

namespace ALTEngine::Bootstrap
{
    using ALTEngine::Formats::CddaRipper;
    using ALTEngine::Formats::CddaRipSummary;
    using ALTEngine::Formats::CddaTrack;
    using ALTEngine::Formats::DiscManifest;
    using ALTEngine::Formats::DiscManifestLoader;
    using ALTEngine::Formats::InstallProgress;
    using ALTEngine::Formats::Installer;
    using ALTEngine::Formats::InstallResult;
    using ALTEngine::Formats::PatchLoader;
    using ALTEngine::Formats::PatchOperation;
    using ALTEngine::Formats::PatchOutcome;
    using ALTEngine::Formats::PatchResult;
    using ALTEngine::Formats::PatchRunner;

    namespace
    {
        // LEVEL CLEANUP's files (Testing > Level Cleanup; Game::LevelCleanup).
        // Beside each cleaned level: <file>.untrimmed, the level as the fixes
        // left it. The order every start: put the untrimmed file back (the
        // cleaned one set aside as <file>.cleaned), run the fixes against it
        // as they were written, then - with the option on - put the cleaned
        // copy back if it was made from exactly this file and list (their
        // CRCs are kept in LevelCleanup.json), or apply the level's plan from
        // data/LevelCleanupList.json again (worked out beforehand by
        // ALTLevelCleanupList; nothing is judged here); with it off, the
        // untrimmed file stays and the cleaned copy goes.
        std::filesystem::path With(const std::filesystem::path& p, const char* suffix)
        {
            auto out = p;
            out += suffix;
            return out;
        }

        bool ReadAll(const std::filesystem::path& p, std::vector<uint8_t>& out)
        {
            std::ifstream in(p, std::ios::binary);
            if (!in) { return false; }
            out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
            return true;
        }

        bool WriteAll(const std::filesystem::path& p, const std::vector<uint8_t>& bytes)
        {
            const auto temp = With(p, ".tmp");
            {
                std::ofstream out(temp, std::ios::binary | std::ios::trunc);
                out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
                if (!out) { return false; }
            }
            std::error_code ec;
            std::filesystem::rename(temp, p, ec);
            if (ec) { std::filesystem::remove(temp, ec); return false; }
            return true;
        }

        void SetCleanedAside(const std::filesystem::path& cdDirectory)
        {
            std::error_code ec;
            for (const auto& relative : ALTEngine::Game::LevelCleanup::LevelFiles(cdDirectory))
            {
                const auto file = cdDirectory / relative;
                const auto untrimmed = With(file, ".untrimmed");
                if (!std::filesystem::exists(untrimmed, ec)) { continue; }
                const auto cleaned = With(file, ".cleaned");
                std::filesystem::remove(cleaned, ec);
                if (std::filesystem::exists(file, ec)) { std::filesystem::rename(file, cleaned, ec); }
                std::filesystem::rename(untrimmed, file, ec);
                if (ec) { std::cout << "Level cleanup: could not restore " << file.string() << ": " << ec.message() << "\n"; }
            }
        }

        // THE MISSING FACES (data/MissingFaces.json; Game::LevelCleanup::
        // ApplyMissingFaces): a global fix, applied after the byte patches and
        // before the Level Cleanup, on or off. Beside each level it changed:
        // <file>.unfaced, the level as the byte patches leave it. Every start
        // puts that back first (the faced one set aside as <file>.faced) so the
        // byte patches meet the file they were written for; afterwards the
        // faced copy goes back if it is exactly what the list makes (its
        // facedCrc), or the faces are put in again.
        nlohmann::json LoadMissingFaces()
        {
            nlohmann::json list;
            std::ifstream in(ExecutableDirectory() / "data" / "MissingFaces.json");
            if (in) { try { in >> list; } catch (...) { list = nlohmann::json(); } }
            if (!list.is_object() || !list.contains("levels") || !list["levels"].is_object()) { return nlohmann::json(); }
            return list;
        }

        bool ReadMissingFaces(const nlohmann::json& entry, ALTEngine::Game::LevelCleanup::MissingFaces& faces, std::string& error)
        {
            faces.firstCell = entry.contains("firstCell") && entry["firstCell"].is_number_integer() ? entry["firstCell"].get<int>() : -1;
            faces.recordZero = entry.contains("recordZero") && entry["recordZero"].is_boolean() && entry["recordZero"].get<bool>();
            faces.recordZeroTexture = entry.contains("recordZeroTexture") && entry["recordZeroTexture"].is_number_integer() ? entry["recordZeroTexture"].get<int>() : -1;
            faces.recordZeroRoutine = entry.contains("recordZeroRoutine") && entry["recordZeroRoutine"].is_number_integer() ? entry["recordZeroRoutine"].get<int>() : -1;
            return !entry.contains("add") || ALTEngine::Game::LevelCleanup::ReadAdditions(entry["add"].dump(), faces.add, error);
        }

        void SetFacedAside(const std::filesystem::path& cdDirectory)
        {
            std::error_code ec;
            const nlohmann::json list = LoadMissingFaces();
            for (const auto& relative : ALTEngine::Game::LevelCleanup::LevelFiles(cdDirectory))
            {
                const auto file = cdDirectory / relative;
                const auto unfaced = With(file, ".unfaced");
                if (!std::filesystem::exists(unfaced, ec))
                {
                    // Faced with no copy of itself without them - ALTViewer's
                    // patch button put them in, and this was installed from
                    // that game: they are taken back out, exactly, so the
                    // byte patches meet the file they were written for.
                    const std::string key = relative.generic_string();
                    if (list.is_null() || !list["levels"].contains(key)) { continue; }
                    const auto& entry = list["levels"][key];
                    std::vector<uint8_t> bytes;
                    if (!entry.contains("facedCrc") || !ReadAll(file, bytes)
                        || ALTEngine::Formats::Crc32(bytes.data(), bytes.size()) != entry["facedCrc"].get<uint32_t>()) { continue; }
                    ALTEngine::Game::LevelCleanup::MissingFaces faces;
                    std::string error;
                    const bool undone = ReadMissingFaces(entry, faces, error) && ALTEngine::Game::LevelCleanup::RemoveMissingFaces(bytes, faces, error);
                    if (undone && entry.contains("crc") && ALTEngine::Formats::Crc32(bytes.data(), bytes.size()) != entry["crc"].get<uint32_t>())
                    {
                        std::cout << "Missing faces: taking them out of " << file.string() << " does not give the file they were listed for - left as it is.\n";
                        continue;
                    }
                    if (!undone || !WriteAll(file, bytes))
                    {
                        std::cout << "Missing faces: could not take them out of " << file.string() << ": " << error << "\n";
                    }
                    continue;
                }
                const auto faced = With(file, ".faced");
                std::filesystem::remove(faced, ec);
                if (std::filesystem::exists(file, ec)) { std::filesystem::rename(file, faced, ec); }
                std::filesystem::rename(unfaced, file, ec);
                if (ec) { std::cout << "Missing faces: could not restore " << file.string() << ": " << ec.message() << "\n"; }
            }
        }

        void ApplyMissingFaces(const std::filesystem::path& cdDirectory)
        {
            namespace LC = ALTEngine::Game::LevelCleanup;
            const nlohmann::json list = LoadMissingFaces();
            if (list.is_null())
            {
                std::cout << "Missing faces: data/MissingFaces.json is missing or cannot be read - none put in.\n";
                return;
            }
            int put = 0, kept = 0, skipped = 0;
            std::error_code ec;
            for (const auto& relative : LC::LevelFiles(cdDirectory))
            {
                const auto file = cdDirectory / relative;
                const auto facedCopy = With(file, ".faced");
                const std::string key = relative.generic_string();
                const auto& levels = list["levels"];
                if (!levels.contains(key)) { std::filesystem::remove(facedCopy, ec); continue; }
                const auto& entry = levels[key];
                auto crcOf = [&](const char* name) {
                    return entry.contains(name) && entry[name].is_number_unsigned() ? entry[name].get<uint32_t>() : 0u;
                };
                std::vector<uint8_t> bytes;
                if (!ReadAll(file, bytes)) { continue; }
                const uint32_t source = ALTEngine::Formats::Crc32(bytes.data(), bytes.size());
                if (source != crcOf("crc"))
                {
                    std::cout << "  [skipped]  " << key << " - its missing faces were listed for another version of the file\n";
                    std::filesystem::remove(facedCopy, ec);
                    ++skipped;
                    continue;
                }
                // The faced copy from last time, if it is still the right one.
                std::vector<uint8_t> cached;
                if (std::filesystem::exists(facedCopy, ec) && ReadAll(facedCopy, cached)
                    && ALTEngine::Formats::Crc32(cached.data(), cached.size()) == crcOf("facedCrc"))
                {
                    std::filesystem::rename(file, With(file, ".unfaced"), ec);
                    if (!ec)
                    {
                        std::filesystem::rename(facedCopy, file, ec);
                        if (ec) { std::error_code back; std::filesystem::rename(With(file, ".unfaced"), file, back); }
                    }
                    if (ec) { std::cout << "Missing faces: could not swap " << file.string() << ": " << ec.message() << "\n"; }
                    ++kept;
                    continue;
                }
                std::filesystem::remove(facedCopy, ec);
                LC::MissingFaces faces;
                std::string error;
                if (!ReadMissingFaces(entry, faces, error))
                {
                    std::cout << "  [FAILED]   " << key << " - " << error << "\n";
                    continue;
                }
                LC::Report report;
                std::vector<uint8_t> original = bytes;
                if (!LC::ApplyMissingFaces(bytes, faces, report, error))
                {
                    std::cout << "  [FAILED]   " << key << " - " << error << "\n";
                    continue;
                }
                if (!WriteAll(With(file, ".unfaced"), original) || !WriteAll(file, bytes))
                {
                    std::cout << "  [FAILED]   " << key << " - could not write the level\n";
                    std::filesystem::remove(With(file, ".unfaced"), ec);
                    continue;
                }
                ++put;
            }
            std::cout << "Missing faces: " << put << " levels given theirs, " << kept << " already"
                      << (skipped ? ", " + std::to_string(skipped) + " skipped" : std::string()) << ".\n";
        }

        void ApplyLevelCleanup(const std::filesystem::path& cdDirectory, bool enabled)
        {
            const auto statePath = cdDirectory / "LevelCleanup.json";
            nlohmann::json state = nlohmann::json::object();
            {
                std::ifstream in(statePath);
                if (in) { try { in >> state; } catch (...) { state = nlohmann::json::object(); } }
                if (!state.is_object()) { state = nlohmann::json::object(); }
            }
            // The plans, worked out once by ALTLevelCleanupList: nothing is
            // judged here, only applied.
            nlohmann::json list;
            uint32_t listCrc = 0;
            if (enabled)
            {
                const auto listPath = ExecutableDirectory() / "data" / "LevelCleanupList.json";
                std::vector<uint8_t> raw;
                if (!ReadAll(listPath, raw))
                {
                    std::cout << "Level cleanup: " << listPath.string() << " is missing - levels left as the fixes leave them.\n";
                    enabled = false;
                }
                else
                {
                    listCrc = ALTEngine::Formats::Crc32(raw.data(), raw.size());
                    try { list = nlohmann::json::parse(raw.begin(), raw.end()); }
                    catch (...) { list = nlohmann::json(); }
                    if (!list.is_object() || !list.contains("levels") || !list["levels"].is_object())
                    {
                        std::cout << "Level cleanup: " << listPath.string() << " cannot be read - levels left as the fixes leave them.\n";
                        enabled = false;
                    }
                }
            }
            // Cleaned copies made from another list, or by another Apply, are
            // made again.
            const bool current = state.contains("list") && state["list"].is_number_unsigned() && state["list"].get<uint32_t>() == listCrc
                                 && state.contains("apply") && state["apply"].is_number_integer()
                                 && state["apply"].get<int>() == ALTEngine::Game::LevelCleanup::APPLY_VERSION;
            state.erase("version");
            if (enabled) { state["list"] = listCrc; state["apply"] = ALTEngine::Game::LevelCleanup::APPLY_VERSION; }
            else { state.erase("list"); state.erase("apply"); }

            int cleaned = 0, kept = 0, restored = 0, skipped = 0;
            size_t saved = 0;
            std::error_code ec;
            for (const auto& relative : ALTEngine::Game::LevelCleanup::LevelFiles(cdDirectory))
            {
                const auto file = cdDirectory / relative;
                const auto cleanedCopy = With(file, ".cleaned");
                const std::string key = relative.generic_string();
                if (!enabled)
                {
                    if (std::filesystem::remove(cleanedCopy, ec)) { ++restored; }
                    state.erase(key);
                    continue;
                }
                std::vector<uint8_t> bytes;
                if (!ReadAll(file, bytes)) { continue; }
                const uint32_t source = ALTEngine::Formats::Crc32(bytes.data(), bytes.size());
                if (current && std::filesystem::exists(cleanedCopy, ec) && state.contains(key) && state[key].is_number_unsigned()
                    && state[key].get<uint32_t>() == source)
                {
                    std::filesystem::rename(file, With(file, ".untrimmed"), ec);
                    if (!ec)
                    {
                        std::filesystem::rename(cleanedCopy, file, ec);
                        // Never leave the level missing: put the untrimmed one back.
                        if (ec) { std::error_code back; std::filesystem::rename(With(file, ".untrimmed"), file, back); }
                    }
                    if (ec) { std::cout << "Level cleanup: could not swap " << file.string() << ": " << ec.message() << "\n"; }
                    ++kept;
                    continue;
                }
                std::filesystem::remove(cleanedCopy, ec);
                state.erase(key);
                const auto& levels = list["levels"];
                if (!levels.contains(key)) { continue; }
                const auto& entry = levels[key];
                if (!entry.is_object() || !entry.contains("crc") || !entry["crc"].is_number_unsigned() || entry["crc"].get<uint32_t>() != source)
                {
                    std::cout << "  [skipped]  " << key << " - its plan was made for another version of the file (a fix changed it?);"
                              << " remake data/LevelCleanupList.json with ALTLevelCleanupList\n";
                    ++skipped;
                    continue;
                }
                ALTEngine::Game::LevelCleanup::Plan plan;
                auto text = [&](const char* name) { return entry.contains(name) && entry[name].is_string() ? entry[name].get<std::string>() : std::string(); };
                auto count = [&](const char* name) { return entry.contains(name) && entry[name].is_number_integer() ? entry[name].get<int>() : 0; };
                if (!ALTEngine::Game::LevelCleanup::DecodeList(text("remove"), plan.remove)
                    || !ALTEngine::Game::LevelCleanup::DecodeList(text("clear"), plan.clear))
                {
                    std::cout << "  [FAILED]   " << key << " - its plan cannot be read\n";
                    continue;
                }
                plan.firstCell = entry.contains("firstCell") && entry["firstCell"].is_number_integer() ? entry["firstCell"].get<int>() : -1;
                plan.recordZero = entry.contains("recordZero") && entry["recordZero"].is_boolean() && entry["recordZero"].get<bool>();
                std::string addError;
                if (entry.contains("add") && !ALTEngine::Game::LevelCleanup::ReadAdditions(entry["add"].dump(), plan.add, addError))
                {
                    std::cout << "  [FAILED]   " << key << " - " << addError << "\n";
                    continue;
                }
                ALTEngine::Game::LevelCleanup::Report report;
                std::string error;
                std::vector<uint8_t> original = bytes;
                if (!ALTEngine::Game::LevelCleanup::Apply(bytes, plan, report, error))
                {
                    std::cout << "  [FAILED]   " << key << " - " << error << "\n";
                    continue;
                }
                if (!report.Changed()) { continue; }
                if (!WriteAll(With(file, ".untrimmed"), original) || !WriteAll(file, bytes))
                {
                    std::cout << "  [FAILED]   " << key << " - could not write the cleaned level\n";
                    std::filesystem::remove(With(file, ".untrimmed"), ec);
                    continue;
                }
                state[key] = source;
                ++cleaned;
                if (report.bytesBefore > report.bytesAfter) { saved += report.bytesBefore - report.bytesAfter; }
                std::cout << "  [cleaned]  " << key << ": " << report.facesRemoved << " faces (" << count("outOfView") << " out of view, "
                          << count("facingAway") << " facing away, " << count("inLifts") << " in sealed lifts, " << count("hidden")
                          << " hidden" << (count("byHand") ? ", " + std::to_string(count("byHand")) + " by hand" : std::string()) << ") and " << report.verticesRemoved << " vertices taken out"
                          << (report.firstCellRestored ? ", first cell's " + std::to_string(report.firstCellFaces) + " faces restored" : std::string())
                          << (report.facesAdded ? ", " + std::to_string(report.facesAdded) + " missing faces added" : std::string())
                          << "\n";
            }
            std::ofstream out(statePath, std::ios::trunc);
            out << state.dump(1);
            if (enabled)
            {
                std::cout << "Level cleanup: " << cleaned << " cleaned (" << saved << " bytes), " << kept << " already clean"
                          << (skipped ? ", " + std::to_string(skipped) + " skipped" : std::string()) << ".\n";
            }
            else if (restored) { std::cout << "Level cleanup off: " << restored << " levels back as the fixes left them.\n"; }
        }
    }

    namespace
    {
        // TEXTURE BLANKING's files (with Testing > Level Cleanup; Game::
        // TextureBlank). Beside each blanked sheet: <sheet>.unblanked, the
        // sheet as the fixes left it. Every start puts it back before the
        // fixes run (they are written against the sheets as shipped), the
        // blanked one set aside as <sheet>.blanked; after the fixes the
        // blanked copy goes back if it was made from exactly this sheet and
        // list (their CRCs are kept in TextureBlank.json), so an unchanged
        // install only renames files; otherwise the sheet is blanked again
        // from data/TextureBlankList.json.
        constexpr int BLANK_APPLY_VERSION = 1;

        void SetBlankedAside(const std::filesystem::path& cdDirectory)
        {
            std::error_code ec;
            for (const auto& relative : ALTEngine::Game::TextureBlank::SheetFiles(cdDirectory))
            {
                const auto file = cdDirectory / relative;
                const auto unblanked = With(file, ".unblanked");
                if (!std::filesystem::exists(unblanked, ec)) { continue; }
                const auto blanked = With(file, ".blanked");
                std::filesystem::remove(blanked, ec);
                if (std::filesystem::exists(file, ec)) { std::filesystem::rename(file, blanked, ec); }
                std::filesystem::rename(unblanked, file, ec);
                if (ec) { std::cout << "Texture blanking: could not restore " << file.string() << ": " << ec.message() << "\n"; }
            }
        }

        void ApplyTextureBlanking(const std::filesystem::path& cdDirectory, bool enabled)
        {
            const auto statePath = cdDirectory / "TextureBlank.json";
            nlohmann::json state = nlohmann::json::object();
            {
                std::ifstream in(statePath);
                if (in) { try { in >> state; } catch (...) { state = nlohmann::json::object(); } }
                if (!state.is_object()) { state = nlohmann::json::object(); }
            }
            std::error_code ec;
            const auto sheets = ALTEngine::Game::TextureBlank::SheetFiles(cdDirectory);
            auto dropCopies = [&] {
                for (const auto& relative : sheets) { std::filesystem::remove(With(cdDirectory / relative, ".blanked"), ec); }
                std::filesystem::remove(statePath, ec);
            };
            if (!enabled) { dropCopies(); return; }

            const auto listPath = ExecutableDirectory() / "data" / "TextureBlankList.json";
            std::vector<uint8_t> raw;
            nlohmann::json list;
            uint32_t listCrc = 0;
            if (ReadAll(listPath, raw))
            {
                listCrc = ALTEngine::Formats::Crc32(raw.data(), raw.size());
                try { list = nlohmann::json::parse(raw.begin(), raw.end()); }
                catch (...) { list = nlohmann::json(); }
            }
            if (!list.is_object() || !list.contains("sheets") || !list["sheets"].is_object())
            {
                std::cout << "Texture blanking: " << listPath.string() << " is missing or cannot be read - sheets left as the fixes leave them.\n";
                dropCopies();
                return;
            }
            // Blanked copies made from another list, or by another Apply, are made again.
            const bool current = state.contains("list") && state["list"].is_number_unsigned() && state["list"].get<uint32_t>() == listCrc
                                 && state.contains("apply") && state["apply"].is_number_integer()
                                 && state["apply"].get<int>() == BLANK_APPLY_VERSION;
            nlohmann::json next = nlohmann::json::object();
            next["list"] = listCrc;
            next["apply"] = BLANK_APPLY_VERSION;

            int blanked = 0, kept = 0, skipped = 0;
            size_t changed = 0;
            for (const auto& relative : sheets)
            {
                const std::string key = relative.generic_string();
                const auto file = cdDirectory / relative;
                const auto blankedCopy = With(file, ".blanked");
                std::vector<uint8_t> bytes;
                if (!list["sheets"].contains(key) || !ReadAll(file, bytes)) { std::filesystem::remove(blankedCopy, ec); continue; }
                const uint32_t source = ALTEngine::Formats::Crc32(bytes.data(), bytes.size());
                if (current && std::filesystem::exists(blankedCopy, ec) && state.contains(key) && state[key].is_number_unsigned()
                    && state[key].get<uint32_t>() == source)
                {
                    std::filesystem::rename(file, With(file, ".unblanked"), ec);
                    if (!ec)
                    {
                        std::filesystem::rename(blankedCopy, file, ec);
                        // Never leave the sheet missing: put the unblanked one back.
                        if (ec) { std::error_code back; std::filesystem::rename(With(file, ".unblanked"), file, back); }
                    }
                    if (!ec) { next[key] = source; ++kept; continue; }
                    std::cout << "Texture blanking: could not swap " << file.string() << ": " << ec.message() << "\n";
                }
                std::filesystem::remove(blankedCopy, ec);
                const auto& entry = list["sheets"][key];
                if (!entry.contains("crc") || !entry["crc"].is_number_unsigned() || entry["crc"].get<uint32_t>() != source)
                {
                    std::cout << "  [skipped]  " << key << " - its plan was made for another version of the sheet (a fix changed it?);"
                              << " remake data/TextureBlankList.json with ALTTextureBlankList\n";
                    ++skipped;
                    continue;
                }
                ALTEngine::Game::TextureBlank::Plan plan;
                std::string error;
                if (!entry.contains("unused") || !entry["unused"].is_string()
                    || !ALTEngine::Game::LevelCleanup::DecodeList(entry["unused"].get<std::string>(), plan.unused))
                {
                    std::cout << "  [FAILED]   " << key << " - its plan cannot be read\n";
                    continue;
                }
                if (entry.contains("backgrounds") && entry["backgrounds"].is_array())
                {
                    for (const auto& b : entry["backgrounds"])
                    {
                        if (b.contains("page") && b["page"].is_number_integer() && b.contains("background") && b["background"].is_number_integer())
                        {
                            plan.background[b["page"].get<int>()] = b["background"].get<int>();
                        }
                    }
                }
                const std::vector<uint8_t> original = bytes;
                ALTEngine::Game::TextureBlank::Report report;
                if (!ALTEngine::Game::TextureBlank::Apply(bytes, plan, report, error))
                {
                    std::cout << "  [FAILED]   " << key << " - " << error << "\n";
                    continue;
                }
                if (report.texelsChanged == 0) { continue; }
                if (!WriteAll(With(file, ".unblanked"), original) || !WriteAll(file, bytes))
                {
                    std::cout << "  [FAILED]   " << key << " - could not write the blanked sheet\n";
                    std::filesystem::remove(With(file, ".unblanked"), ec);
                    continue;
                }
                next[key] = source;
                ++blanked;
                changed += report.texelsChanged;
            }
            std::ofstream out(statePath, std::ios::trunc);
            out << next.dump(1);
            std::cout << "Texture blanking: " << blanked << " sheets blanked (" << changed << " texels set to their page's background), "
                      << kept << " already blanked" << (skipped ? ", " + std::to_string(skipped) + " skipped" : std::string()) << ".\n";
        }
    }

    namespace
    {
        // ALTENGINE-ONLY MODEL FACE EDITS (EngineFilePatches.json "modelFaces";
        // Formats/EngineModelFaceEdits.h). Door and lift faces turned over and
        // marked to draw from both sides, which the original cannot draw: never
        // for the original game, never part of ALTViewer's patch button. Made
        // after the Level Cleanup and the texture blanking; taken back out at
        // the start of every run (RevertEngineModelFaceEdits) so the global
        // stages always meet the files without them.
        std::vector<ALTEngine::Formats::EngineModelFaceEdit> LoadEngineModelFaceEdits()
        {
            std::vector<uint8_t> raw;
            if (!ReadAll(ExecutableDirectory() / "data" / "EngineFilePatches.json", raw)) { return {}; }
            std::vector<std::string> errors;
            auto edits = ALTEngine::Formats::EngineModelFaceEdits::Parse(std::string(raw.begin(), raw.end()), errors);
            for (const auto& e : errors) { std::cout << "ALTEngine model face edits: " << e << "\n"; }
            return edits;
        }

        void RevertEngineModelFaceEdits(const std::filesystem::path& cdDirectory)
        {
            const auto edits = LoadEngineModelFaceEdits();
            std::map<std::string, std::vector<const ALTEngine::Formats::EngineModelFaceEdit*>> byFile;
            for (const auto& e : edits) { byFile[e.target].push_back(&e); }
            for (const auto& [target, list] : byFile)
            {
                std::vector<uint8_t> bytes;
                if (!ReadAll(cdDirectory / target, bytes)) { continue; }
                bool changed = false;
                for (const auto* e : list) { changed = ALTEngine::Formats::EngineModelFaceEdits::Revert(bytes, *e) || changed; }
                if (changed && !WriteAll(cdDirectory / target, bytes))
                {
                    std::cout << "ALTEngine model face edits: could not take them out of " << target << ".\n";
                }
            }
        }

        void ApplyEngineModelFaceEdits(const std::filesystem::path& cdDirectory)
        {
            using ALTEngine::Formats::EngineModelFaceOutcome;
            const auto edits = LoadEngineModelFaceEdits();
            std::map<std::string, std::vector<const ALTEngine::Formats::EngineModelFaceEdit*>> byFile;
            for (const auto& e : edits) { byFile[e.target].push_back(&e); }
            size_t made = 0, already = 0;
            for (const auto& [target, list] : byFile)
            {
                std::vector<uint8_t> bytes;
                if (!ReadAll(cdDirectory / target, bytes)) { std::cout << "ALTEngine model face edits: " << target << " not found.\n"; continue; }
                bool changed = false;
                for (const auto* e : list)
                {
                    switch (ALTEngine::Formats::EngineModelFaceEdits::Apply(bytes, *e))
                    {
                    case EngineModelFaceOutcome::Applied: ++made; changed = true; break;
                    case EngineModelFaceOutcome::AlreadyApplied: ++already; break;
                    case EngineModelFaceOutcome::NotFound:
                        std::cout << "ALTEngine model face edits: " << target << " has no face " << e->face << " in " << e->section << ".\n";
                        break;
                    case EngineModelFaceOutcome::Mismatch:
                        std::cout << "ALTEngine model face edits: " << target << " " << e->section << " face " << e->face
                                  << " is not the face the edit was made for - left as it is.\n";
                        break;
                    }
                }
                if (changed)
                {
                    if (WriteAll(cdDirectory / target, bytes)) { std::cout << "  [applied]  " << target << " model faces (ALTEngine only)\n"; }
                    else { std::cout << "ALTEngine model face edits: could not write " << target << ".\n"; }
                }
            }
            std::cout << "ALTEngine model face edits: " << made << " made, " << already << " already made.\n";
        }

        // ALTENGINE-ONLY FILE FIXES (data/EngineFilePatches.json): stage 5,
        // never part of ALTViewer's patch button. Each file is checked by its
        // CRC-32 before (as shipped) and after (as fixed); one already fixed is
        // skipped, one that is neither is left alone and reported. The first is
        // the face hugger's art, FINGERS.B16 (Patch/FINGERS.txt).
        void ApplyEngineFilePatches(const std::filesystem::path& cdDirectory)
        {
            const auto listPath = ExecutableDirectory() / "data" / "EngineFilePatches.json";
            nlohmann::json list;
            {
                std::ifstream in(listPath);
                if (!in) { std::cout << "ALTEngine file fixes: " << listPath.string() << " is missing - none made.\n"; return; }
                try { in >> list; } catch (...) { list = nlohmann::json(); }
            }
            if (!list.is_object() || !list.contains("files") || !list["files"].is_array())
            {
                std::cout << "ALTEngine file fixes: " << listPath.string() << " cannot be read - none made.\n";
                return;
            }
            auto hexValue = [](char c) -> int {
                if (c >= '0' && c <= '9') { return c - '0'; }
                if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
                if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
                return -1;
            };
            size_t made = 0, already = 0;
            for (const auto& entry : list["files"])
            {
                if (!entry.is_object() || !entry.contains("target") || !entry.contains("crc") || !entry.contains("patchedCrc")
                    || !entry.contains("edits") || !entry["edits"].is_array()) { continue; }
                const std::string target = entry["target"].get<std::string>();
                const auto file = cdDirectory / target;
                std::vector<uint8_t> bytes;
                if (!ReadAll(file, bytes)) { std::cout << "ALTEngine file fixes: " << target << " not found.\n"; continue; }
                const uint32_t crc = ALTEngine::Formats::Crc32(bytes.data(), bytes.size());
                if (crc == entry["patchedCrc"].get<uint32_t>()) { ++already; continue; }
                if (crc != entry["crc"].get<uint32_t>())
                {
                    std::cout << "ALTEngine file fixes: " << target << " is not the file the fix was made for - left as it is.\n";
                    continue;
                }
                bool ok = true;
                for (const auto& edit : entry["edits"])
                {
                    const std::string offsetText = edit.value("offset", "");
                    const std::string hex = edit.value("hex", "");
                    size_t offset = 0;
                    try { offset = static_cast<size_t>(std::stoull(offsetText, nullptr, 16)); } catch (...) { ok = false; break; }
                    if (hex.size() % 2 != 0 || offset + hex.size() / 2 > bytes.size()) { ok = false; break; }
                    for (size_t i = 0; i < hex.size() / 2 && ok; ++i)
                    {
                        const int hi = hexValue(hex[2 * i]), lo = hexValue(hex[2 * i + 1]);
                        if (hi < 0 || lo < 0) { ok = false; break; }
                        bytes[offset + i] = static_cast<uint8_t>(hi * 16 + lo);
                    }
                }
                if (!ok || ALTEngine::Formats::Crc32(bytes.data(), bytes.size()) != entry["patchedCrc"].get<uint32_t>())
                {
                    std::cout << "ALTEngine file fixes: " << target << " - the fix does not give the expected file; left as it is.\n";
                    continue;
                }
                if (!WriteAll(file, bytes)) { std::cout << "ALTEngine file fixes: could not write " << target << ".\n"; continue; }
                ++made;
                std::cout << "  [applied]  " << target << " (ALTEngine only)\n";
            }
            std::cout << "ALTEngine file fixes: " << made << " made, " << already << " already made.\n";
            ApplyEngineModelFaceEdits(cdDirectory);
        }
    }

    namespace
    {
        // THE PADDING TRIM (Formats::LevelTrim): ALTEngine only, the very last
        // step, since every offset a patch, the cleanup or the blanking names
        // is in the untrimmed layout. Beside each trimmed level:
        // <file>.padded, the level before the trim. Every start puts that back
        // first (the trimmed one kept as <file>.trimmed), so every stage before
        // meets untrimmed files; afterwards the kept copy goes back if it is
        // still the trim of what the stages made, or the level is trimmed again.
        // Testing > Padding Trim off: the untrimmed files stay.
        void SetPaddedAside(const std::filesystem::path& cdDirectory)
        {
            std::error_code ec;
            for (const auto& relative : ALTEngine::Game::LevelCleanup::LevelFiles(cdDirectory))
            {
                const auto file = cdDirectory / relative;
                const auto padded = With(file, ".padded");
                if (!std::filesystem::exists(padded, ec)) { continue; }
                const auto trimmed = With(file, ".trimmed");
                std::filesystem::remove(trimmed, ec);
                if (std::filesystem::exists(file, ec)) { std::filesystem::rename(file, trimmed, ec); }
                std::filesystem::rename(padded, file, ec);
                if (ec) { std::cout << "Padding trim: could not restore " << file.string() << ": " << ec.message() << "\n"; }
            }
        }

        void ApplyPaddingTrim(const std::filesystem::path& cdDirectory, bool enabled)
        {
            namespace LT = ALTEngine::Formats::LevelTrim;
            int made = 0, kept = 0, refused = 0;
            size_t saved = 0;
            std::error_code ec;
            for (const auto& relative : ALTEngine::Game::LevelCleanup::LevelFiles(cdDirectory))
            {
                const auto file = cdDirectory / relative;
                const auto trimmedCopy = With(file, ".trimmed");
                if (!enabled) { std::filesystem::remove(trimmedCopy, ec); continue; }
                std::vector<uint8_t> bytes, trimmed;
                if (!ReadAll(file, bytes)) { continue; }
                std::string error;
                if (!LT::Trim(bytes, trimmed, error))
                {
                    ++refused;
                    std::cout << "Padding trim: " << relative.generic_string() << " left as it is: " << error << "\n";
                    std::filesystem::remove(trimmedCopy, ec);
                    continue;
                }
                saved += bytes.size() - trimmed.size();
                // The level before the trim is kept; the trimmed one takes its name.
                std::vector<uint8_t> cached;
                const bool same = std::filesystem::exists(trimmedCopy, ec) && ReadAll(trimmedCopy, cached) && cached == trimmed;
                std::filesystem::rename(file, With(file, ".padded"), ec);
                if (ec) { std::cout << "Padding trim: could not set " << file.string() << " aside: " << ec.message() << "\n"; continue; }
                if (same) { std::filesystem::rename(trimmedCopy, file, ec); }
                if (!same || ec)
                {
                    std::filesystem::remove(trimmedCopy, ec);
                    if (!WriteAll(file, trimmed))
                    {
                        // Never leave the level missing: the untrimmed one goes back.
                        std::filesystem::rename(With(file, ".padded"), file, ec);
                        std::cout << "Padding trim: could not write " << file.string() << "\n";
                        continue;
                    }
                    ++made;
                }
                else { ++kept; }
            }
            if (enabled)
            {
                std::cout << "Padding trim: " << made << " levels trimmed, " << kept << " as before, " << refused
                          << " left as they are; " << saved << " bytes nothing reads taken out.\n";
            }
        }
    }

    void RunPatches(const std::filesystem::path& cdDirectory)
    {
        // THE STAGES, in order (Patch/README.txt):
        //   1. the duplicate door/lift model removals,
        //   2. the level fixes and 3. the texture fixes (Patches.json's
        //      "level" then "texture" byte patches),
        //   4. the missing faces (MissingFaces.json),
        //   5. ALTEngine's own: the Level Cleanup, then the texture blanking,
        //      then its file fixes (EngineFilePatches.json: FINGERS.B16, and
        //      the door/lift model face edits, which the original cannot draw),
        //      then, truly last, the padding trim (Formats::LevelTrim).
        // 1-4 are global (ALTViewer's patch button makes the same files); 5 is
        // ALTEngine's alone. The byte patches always meet the files as shipped
        // (less the removed models), so every later stage's work is put aside
        // first, last stage first.
        SetPaddedAside(cdDirectory);
        // ALTEngine's model face edits come out before anything else is set
        // aside: the cleaned copy kept for reuse must not carry them.
        RevertEngineModelFaceEdits(cdDirectory);
        SetCleanedAside(cdDirectory);
        SetFacedAside(cdDirectory);
        SetBlankedAside(cdDirectory);

        std::filesystem::path patchesJson = ExecutableDirectory() / "data" / "Patches.json";

        std::vector<PatchOperation> operations;
        try
        {
            operations = PatchLoader::Load(patchesJson);
        }
        catch (const std::exception& e)
        {
            std::cout << "Failed to load " << patchesJson.string() << ": " << e.what() << "\n";
            return;
        }

        // Duplicate and unused door/lift models go first: the byte patches
        // are written against the files without them (ModelRemoval.h).
        try
        {
            using namespace ALTEngine::Formats;
            const std::vector<ModelRemoval> removals = ModelRemovalLoader::Load(patchesJson);
            size_t removed = 0, already = 0, removalFailed = 0;
            for (const auto& result : ModelRemovalRunner::ApplyAll(cdDirectory, removals))
            {
                switch (result.outcome)
                {
                case ModelRemovalOutcome::Applied: ++removed; break;
                case ModelRemovalOutcome::AlreadyApplied: ++already; break;
                case ModelRemovalOutcome::Failed:
                    ++removalFailed;
                    std::cout << "  [FAILED]   " << result.removal->targetFile << " - " << result.error << "\n";
                    break;
                }
            }
            std::cout << "Model removals: " << removed << " applied, " << already << " already applied, "
                      << removalFailed << " failed.\n";
        }
        catch (const std::exception& e)
        {
            std::cout << "Failed to load the model removals from " << patchesJson.string() << ": " << e.what() << "\n";
        }

        std::cout << "Applying " << operations.size() << " patch operations...\n";
        std::vector<PatchResult> results = PatchRunner::ApplyAll(cdDirectory, operations);

        size_t applied = 0, alreadyApplied = 0, failed = 0;
        for (const auto& result : results)
        {
            switch (result.outcome)
            {
            case PatchOutcome::Applied:
                ++applied;
                std::cout << "  [applied]  " << result.operation->targetFile;
                if (!result.operation->note.empty()) { std::cout << "  (" << result.operation->note << ")"; }
                std::cout << "\n";
                break;
            case PatchOutcome::AlreadyApplied:
                ++alreadyApplied;
                break;
            case PatchOutcome::Failed:
                ++failed;
                std::cout << "  [FAILED]   " << result.operation->targetFile << " - " << result.error << "\n";
                break;
            }
        }

        std::cout << "Patches: " << applied << " applied, " << alreadyApplied
                   << " already applied, " << failed << " failed.\n";

        ApplyMissingFaces(cdDirectory);
        Config config;
        ApplyLevelCleanup(cdDirectory, TestingSettings(config).IsOn(TestingOption::LevelCleanup));
        // Last: the texture blanking, against the sheets as the fixes leave them.
        ApplyTextureBlanking(cdDirectory, TestingSettings(config).IsOn(TestingOption::LevelCleanup));
        ApplyEngineFilePatches(cdDirectory);
        // Truly last: every offset named before this is in the untrimmed layout.
        ApplyPaddingTrim(cdDirectory, TestingSettings(config).IsOn(TestingOption::PaddingTrim));
    }

    bool InstallFromDisc(const std::filesystem::path& discRoot, const std::filesystem::path& destination)
    {
        // An install is read on the MU/TH/UR readout to the end, so it waits for a key.
        MuthurConsole::WaitForKeyAtEnd();
        std::cout << "Installing from disc (" << discRoot.string() << ") to " << destination.string() << "...\n";

        std::filesystem::path manifestPath = ExecutableDirectory() / "data" / "DiscFileManifest.json";
        DiscManifest manifest;
        try
        {
            manifest = DiscManifestLoader::Load(manifestPath);
        }
        catch (const std::exception& e)
        {
            std::cout << "Failed to load " << manifestPath.string() << ": " << e.what() << "\n";
            return false;
        }

        InstallResult copyResult = Installer::CopyFiles(discRoot, destination, manifest,
            [](const InstallProgress& p) {
                if (p.filesCompleted % 25 == 0 || p.filesCompleted == p.filesTotal)
                {
                    std::cout << "  [" << p.filesCompleted << "/" << p.filesTotal << "] " << p.currentFile << "\n";
                }
            });

        if (!copyResult.success)
        {
            std::cout << "Install incomplete - " << copyResult.failedFiles.size() << " file(s) failed:\n";
            for (const auto& f : copyResult.failedFiles) { std::cout << "  " << f << "\n"; }
            return false;
        }
        std::cout << "File copy complete.\n";

        // Patch immediately, as part of installation itself - done and
        // dusted from the moment of installation, not deferred to
        // whatever happens to run next.
        RunPatches(destination / "CD");

        // The music: the disc's audio tracks as CD/MUSIC/trackNN.wav, by
        // whichever route this system has (CddaRipper::RipDisc).
        std::filesystem::path musicDir = destination / "CD" / "MUSIC";
        std::error_code ec;
        std::filesystem::create_directories(musicDir, ec);

        const CddaRipSummary rip = CddaRipper::RipDisc(discRoot, musicDir,
            [](const std::string& line) { std::cout << line << "\n"; });
        if (!rip.error.empty())
        {
            std::cout << "CDDA rip failed entirely: " << rip.error << "\n";
            std::cout << "Continuing without music - this can be retried later.\n";
        }
        else
        {
            std::cout << "CDDA rip: " << rip.ripped << " tracks ripped, " << rip.failed << " failed.\n";
        }

        return true;
    }
}
