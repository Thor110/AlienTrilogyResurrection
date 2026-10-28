#include "SaveGame.h"

#include <nlohmann/json.hpp>

#include <fstream>

namespace ALTEngine::Game::SaveGame
{
    std::filesystem::path SlotPath(const std::filesystem::path& directory, int slot)
    {
        return directory / ("slot" + std::to_string(slot) + ".json");
    }

    std::string DefaultName(const std::string& levelDigits)
    {
        std::string dotted;
        for (char c : levelDigits)
        {
            if (!dotted.empty()) { dotted += '.'; }
            dotted += c;
        }
        return "MISSION " + dotted;
    }

    bool Write(const std::filesystem::path& directory, int slot, const SaveData& data)
    {
        if (slot < 1 || slot > SLOT_COUNT) { return false; }

        nlohmann::json weapons = nlohmann::json::array();
        for (int i = 0; i < 5; ++i)
        {
            const Screens::WeaponState* w = data.inventory.ByIndex(i);
            weapons.push_back({ { "available", w->available }, { "equipped", w->equipped },
                                { "rounds", w->rounds }, { "units", w->units }, { "grenades", w->grenades } });
        }

        nlohmann::json root = {
            { "version", FORMAT_VERSION },
            { "name", data.name },
            { "level", data.levelDigits },
            { "difficulty", data.difficulty },
            { "inventory", {
                { "weapons", weapons },
                { "autoMapper", data.inventory.hasAutoMapper },
                { "shoulderLamp", data.inventory.hasShoulderLamp },
                { "batteries", data.inventory.batteries },
            } },
        };

        std::error_code ec;
        std::filesystem::create_directories(directory, ec);

        // Write beside the slot and rename over it, so a failed write never
        // destroys the save that was already there.
        const std::filesystem::path target = SlotPath(directory, slot);
        const std::filesystem::path temp = target.string() + ".tmp";
        {
            std::ofstream out(temp, std::ios::binary | std::ios::trunc);
            if (!out) { return false; }
            out << root.dump(2);
            if (!out) { return false; }
        }
        std::filesystem::rename(temp, target, ec);
        if (ec)
        {
            // Not every platform's rename replaces an existing file.
            std::error_code removeEc;
            std::filesystem::remove(target, removeEc);
            ec.clear();
            std::filesystem::rename(temp, target, ec);
        }
        if (ec)
        {
            std::filesystem::remove(temp, ec);
            return false;
        }
        return true;
    }

    std::optional<SaveData> Read(const std::filesystem::path& directory, int slot)
    {
        if (slot < 1 || slot > SLOT_COUNT) { return std::nullopt; }
        std::ifstream in(SlotPath(directory, slot), std::ios::binary);
        if (!in) { return std::nullopt; }

        try
        {
            const nlohmann::json root = nlohmann::json::parse(in);
            if (root.value("version", 0) != FORMAT_VERSION) { return std::nullopt; }

            SaveData data;
            data.name = root.value("name", std::string{});
            data.levelDigits = root.value("level", std::string{});
            data.difficulty = root.value("difficulty", 0);
            if (data.levelDigits.empty()) { return std::nullopt; }

            const nlohmann::json& inv = root.at("inventory");
            const nlohmann::json& weapons = inv.at("weapons");
            for (int i = 0; i < 5 && i < static_cast<int>(weapons.size()); ++i)
            {
                Screens::WeaponState* w = data.inventory.ByIndex(i);
                const nlohmann::json& j = weapons[static_cast<size_t>(i)];
                w->available = j.value("available", false);
                w->equipped = j.value("equipped", false);
                w->rounds = j.value("rounds", 0);
                w->units = j.value("units", 0);
                w->grenades = j.value("grenades", 0);
            }
            data.inventory.hasAutoMapper = inv.value("autoMapper", false);
            data.inventory.hasShoulderLamp = inv.value("shoulderLamp", false);
            data.inventory.batteries = inv.value("batteries", 0);
            return data;
        }
        catch (const std::exception&)
        {
            return std::nullopt;
        }
    }

    std::array<std::optional<std::string>, SLOT_COUNT> ListNames(const std::filesystem::path& directory)
    {
        std::array<std::optional<std::string>, SLOT_COUNT> names{};
        for (int slot = 1; slot <= SLOT_COUNT; ++slot)
        {
            if (auto data = Read(directory, slot)) { names[static_cast<size_t>(slot - 1)] = data->name; }
        }
        return names;
    }
}
