#include "PCH.h"
#include "settings.h"
#include <SimpleIni.h>
#include <SKSEMenuFramework.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <mutex>

namespace {
    constexpr auto iniPath = "Data/SKSE/Plugins/AnimatedProjectileBlock.ini";
    constexpr auto general = "General";
    constexpr auto player = "Player";
    constexpr auto npc = "NPC";

    std::mutex configMutex;
    settings::config activeConfig{};

    bool readBool(const CSimpleIniA& ini, const char* section, const char* key, bool fallback) {
        return ini.GetBoolValue(section, key, fallback);
    }

    float readFactor(const CSimpleIniA& ini, const char* section, const char* key, float fallback) {
        const float value = static_cast<float>(ini.GetDoubleValue(section, key, fallback));
        return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : fallback;
    }

    float readCostFactor(const CSimpleIniA& ini, const char* section, const char* key, float fallback) {
        const float value = static_cast<float>(ini.GetDoubleValue(section, key, fallback));
        return std::isfinite(value) ? std::clamp(value, 0.0f, 5.0f) : fallback;
    }

    float readSpellCost(const CSimpleIniA& ini, const char* section, const char* key, float fallback) {
        const float value = static_cast<float>(ini.GetDoubleValue(section, key, fallback));
        return std::isfinite(value) ? std::clamp(value, 0.0f, 250.0f) : fallback;
    }

    void writeBool(CSimpleIniA& ini, const char* section, const char* key, bool value) {
        ini.SetBoolValue(section, key, value);
    }

    void writeFactor(CSimpleIniA& ini, const char* section, const char* key, float value) {
        ini.SetDoubleValue(section, key, value, nullptr, false);
    }

    bool drawBlockRow(const char* label, bool& enabled, float& factor) {
        bool changed = ImGuiMCP::Checkbox(label, &enabled);
        ImGuiMCP::BeginDisabled(!enabled);
        const std::string sliderLabel = std::string("Effectiveness factor##") + label;
        changed |= ImGuiMCP::SliderFloat(sliderLabel.c_str(), &factor, 0.0f, 1.0f, "%.2f");
        ImGuiMCP::EndDisabled();
        return changed;
    }
}

namespace settings {
    config Get() {
        std::scoped_lock lock(configMutex);
        return activeConfig;
    }

    void Set(const config& value) {
        std::scoped_lock lock(configMutex);
        activeConfig = value;
    }

    void Load() {
        config loaded{};
        CSimpleIniA ini;
        ini.SetUnicode(false);

        std::error_code ec;
        if (!std::filesystem::exists(iniPath, ec) && !ec) {
            Set(loaded);
            if (Save()) {
                SKSE::log::info("[settings] Created {} with defaults", iniPath);
            }
            return;
        }
        if (ec || ini.LoadFile(iniPath) < 0) {
            SKSE::log::error("[settings] Could not read {}; using defaults without replacing it", iniPath);
            Set(loaded);
            return;
        }

        loaded.log = readBool(ini, general, "log", loaded.log);

        loaded.playerWeaponArrowEnabled = readBool(ini, player, "weaponArrowEnabled", loaded.playerWeaponArrowEnabled);
        loaded.playerShieldArrowEnabled = readBool(ini, player, "shieldArrowEnabled", loaded.playerShieldArrowEnabled);
        loaded.playerWeaponMagicEnabled = readBool(ini, player, "weaponMagicEnabled", loaded.playerWeaponMagicEnabled);
        loaded.playerShieldMagicEnabled = readBool(ini, player, "shieldMagicEnabled", loaded.playerShieldMagicEnabled);
        loaded.pcWeaponArrowFactor = readFactor(ini, player, "weaponArrowFactor", loaded.pcWeaponArrowFactor);
        loaded.pcShieldArrowFactor = readFactor(ini, player, "shieldArrowFactor", loaded.pcShieldArrowFactor);
        loaded.pcWeaponMagicFactor = readFactor(ini, player, "weaponMagicFactor", loaded.pcWeaponMagicFactor);
        loaded.pcShieldMagicFactor = readFactor(ini, player, "shieldMagicFactor", loaded.pcShieldMagicFactor);
        loaded.pcArrowBlockCostFactor = readCostFactor(ini, player, "arrowBlockCostFactor", loaded.pcArrowBlockCostFactor);
        loaded.pcWeaponSpellStaminaCost = readSpellCost(ini, player, "weaponSpellStaminaCost", loaded.pcWeaponSpellStaminaCost);
        loaded.pcShieldSpellStaminaCost = readSpellCost(ini, player, "shieldSpellStaminaCost", loaded.pcShieldSpellStaminaCost);
        loaded.pcWeaponSpellMagickaCost = readSpellCost(ini, player, "weaponSpellMagickaCost", loaded.pcWeaponSpellMagickaCost);
        loaded.pcShieldSpellMagickaCost = readSpellCost(ini, player, "shieldSpellMagickaCost", loaded.pcShieldSpellMagickaCost);
        loaded.pcWeaponFlameCostMultiplier = readCostFactor(ini, player, "weaponFlameCostMultiplier", loaded.pcWeaponFlameCostMultiplier);
        loaded.pcShieldFlameCostMultiplier = readCostFactor(ini, player, "shieldFlameCostMultiplier", loaded.pcShieldFlameCostMultiplier);

        loaded.NPCWeaponArrowEnabled = readBool(ini, npc, "weaponArrowEnabled", loaded.NPCWeaponArrowEnabled);
        loaded.NPCShieldArrowEnabled = readBool(ini, npc, "shieldArrowEnabled", loaded.NPCShieldArrowEnabled);
        loaded.NPCWeaponMagicEnabled = readBool(ini, npc, "weaponMagicEnabled", loaded.NPCWeaponMagicEnabled);
        loaded.NPCShieldMagicEnabled = readBool(ini, npc, "shieldMagicEnabled", loaded.NPCShieldMagicEnabled);
        loaded.NPCWeaponArrowFactor = readFactor(ini, npc, "weaponArrowFactor", loaded.NPCWeaponArrowFactor);
        loaded.NPCShieldArrowFactor = readFactor(ini, npc, "shieldArrowFactor", loaded.NPCShieldArrowFactor);
        loaded.NPCWeaponMagicFactor = readFactor(ini, npc, "weaponMagicFactor", loaded.NPCWeaponMagicFactor);
        loaded.NPCShieldMagicFactor = readFactor(ini, npc, "shieldMagicFactor", loaded.NPCShieldMagicFactor);
        loaded.NPCArrowBlockCostFactor = readCostFactor(ini, npc, "arrowBlockCostFactor", loaded.NPCArrowBlockCostFactor);
        loaded.NPCWeaponSpellStaminaCost = readSpellCost(ini, npc, "weaponSpellStaminaCost", loaded.NPCWeaponSpellStaminaCost);
        loaded.NPCShieldSpellStaminaCost = readSpellCost(ini, npc, "shieldSpellStaminaCost", loaded.NPCShieldSpellStaminaCost);
        loaded.NPCWeaponSpellMagickaCost = readSpellCost(ini, npc, "weaponSpellMagickaCost", loaded.NPCWeaponSpellMagickaCost);
        loaded.NPCShieldSpellMagickaCost = readSpellCost(ini, npc, "shieldSpellMagickaCost", loaded.NPCShieldSpellMagickaCost);
        loaded.NPCWeaponFlameCostMultiplier = readCostFactor(ini, npc, "weaponFlameCostMultiplier", loaded.NPCWeaponFlameCostMultiplier);
        loaded.NPCShieldFlameCostMultiplier = readCostFactor(ini, npc, "shieldFlameCostMultiplier", loaded.NPCShieldFlameCostMultiplier);

        Set(loaded);
        SKSE::log::info("[settings] Loaded {}", iniPath);
    }

    bool Save() {
        const config current = Get();
        CSimpleIniA ini;
        ini.SetUnicode(false);
        (void)ini.LoadFile(iniPath);  // Preserve any keys added by other versions.

        writeBool(ini, general, "log", current.log);

        writeBool(ini, player, "weaponArrowEnabled", current.playerWeaponArrowEnabled);
        writeBool(ini, player, "shieldArrowEnabled", current.playerShieldArrowEnabled);
        writeBool(ini, player, "weaponMagicEnabled", current.playerWeaponMagicEnabled);
        writeBool(ini, player, "shieldMagicEnabled", current.playerShieldMagicEnabled);
        writeFactor(ini, player, "weaponArrowFactor", current.pcWeaponArrowFactor);
        writeFactor(ini, player, "shieldArrowFactor", current.pcShieldArrowFactor);
        writeFactor(ini, player, "weaponMagicFactor", current.pcWeaponMagicFactor);
        writeFactor(ini, player, "shieldMagicFactor", current.pcShieldMagicFactor);
        writeFactor(ini, player, "arrowBlockCostFactor", current.pcArrowBlockCostFactor);
        writeFactor(ini, player, "weaponSpellStaminaCost", current.pcWeaponSpellStaminaCost);
        writeFactor(ini, player, "shieldSpellStaminaCost", current.pcShieldSpellStaminaCost);
        writeFactor(ini, player, "weaponSpellMagickaCost", current.pcWeaponSpellMagickaCost);
        writeFactor(ini, player, "shieldSpellMagickaCost", current.pcShieldSpellMagickaCost);
        writeFactor(ini, player, "weaponFlameCostMultiplier", current.pcWeaponFlameCostMultiplier);
        writeFactor(ini, player, "shieldFlameCostMultiplier", current.pcShieldFlameCostMultiplier);

        writeBool(ini, npc, "weaponArrowEnabled", current.NPCWeaponArrowEnabled);
        writeBool(ini, npc, "shieldArrowEnabled", current.NPCShieldArrowEnabled);
        writeBool(ini, npc, "weaponMagicEnabled", current.NPCWeaponMagicEnabled);
        writeBool(ini, npc, "shieldMagicEnabled", current.NPCShieldMagicEnabled);
        writeFactor(ini, npc, "weaponArrowFactor", current.NPCWeaponArrowFactor);
        writeFactor(ini, npc, "shieldArrowFactor", current.NPCShieldArrowFactor);
        writeFactor(ini, npc, "weaponMagicFactor", current.NPCWeaponMagicFactor);
        writeFactor(ini, npc, "shieldMagicFactor", current.NPCShieldMagicFactor);
        writeFactor(ini, npc, "arrowBlockCostFactor", current.NPCArrowBlockCostFactor);
        writeFactor(ini, npc, "weaponSpellStaminaCost", current.NPCWeaponSpellStaminaCost);
        writeFactor(ini, npc, "shieldSpellStaminaCost", current.NPCShieldSpellStaminaCost);
        writeFactor(ini, npc, "weaponSpellMagickaCost", current.NPCWeaponSpellMagickaCost);
        writeFactor(ini, npc, "shieldSpellMagickaCost", current.NPCShieldSpellMagickaCost);
        writeFactor(ini, npc, "weaponFlameCostMultiplier", current.NPCWeaponFlameCostMultiplier);
        writeFactor(ini, npc, "shieldFlameCostMultiplier", current.NPCShieldFlameCostMultiplier);

        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(iniPath).parent_path(), ec);
        if (ec || ini.SaveFile(iniPath) < 0) {
            SKSE::log::error("[settings] Could not save {}: {}", iniPath, ec.message());
            return false;
        }
        SKSE::log::info("[settings] Saved {}", iniPath);
        return true;
    }

    void __stdcall RenderMenuPage() {
        config current = Get();
        static bool unsaved = false;
        bool changed = false;

        ImGuiMCP::TextUnformatted("Factors multiply calculated block effectiveness (0 = none, 1 = full effectiveness).");
        ImGuiMCP::TextUnformatted("Spell costs are per application; flame costs also use the flame multiplier.");
        changed |= ImGuiMCP::Checkbox("Enable diagnostic logging", &current.log);

        ImGuiMCP::Separator();
        ImGuiMCP::TextUnformatted("Player - arrows");
        changed |= drawBlockRow("Weapon arrow block##player", current.playerWeaponArrowEnabled, current.pcWeaponArrowFactor);
        changed |= drawBlockRow("Shield arrow block##player", current.playerShieldArrowEnabled, current.pcShieldArrowFactor);
        changed |= ImGuiMCP::SliderFloat("Arrow block stamina cost factor##player", &current.pcArrowBlockCostFactor, 0.0f, 5.0f, "%.1f");
        ImGuiMCP::TextUnformatted("Player - spells");
        changed |= drawBlockRow("Weapon spell block##player", current.playerWeaponMagicEnabled, current.pcWeaponMagicFactor);
        changed |= drawBlockRow("Shield spell block##player", current.playerShieldMagicEnabled, current.pcShieldMagicFactor);
        changed |= ImGuiMCP::SliderFloat("Weapon spell stamina cost##player", &current.pcWeaponSpellStaminaCost, 0.0f, 50.0f, "%.1f");
        changed |= ImGuiMCP::SliderFloat("Shield spell stamina cost##player", &current.pcShieldSpellStaminaCost, 0.0f, 50.0f, "%.1f");
        changed |= ImGuiMCP::SliderFloat("Weapon spell magicka cost##player", &current.pcWeaponSpellMagickaCost, 0.0f, 50.0f, "%.1f");
        changed |= ImGuiMCP::SliderFloat("Shield spell magicka cost##player", &current.pcShieldSpellMagickaCost, 0.0f, 50.0f, "%.1f");
        changed |= ImGuiMCP::SliderFloat("Weapon flame cost multiplier##player", &current.pcWeaponFlameCostMultiplier, 0.0f, 5.0f, "%.1f");
        changed |= ImGuiMCP::SliderFloat("Shield flame cost multiplier##player", &current.pcShieldFlameCostMultiplier, 0.0f, 5.0f, "%.1f");

        ImGuiMCP::Separator();
        ImGuiMCP::TextUnformatted("NPCs - arrows");
        changed |= drawBlockRow("Weapon arrow block##npc", current.NPCWeaponArrowEnabled, current.NPCWeaponArrowFactor);
        changed |= drawBlockRow("Shield arrow block##npc", current.NPCShieldArrowEnabled, current.NPCShieldArrowFactor);
        changed |= ImGuiMCP::SliderFloat("Arrow block stamina cost factor##npc", &current.NPCArrowBlockCostFactor, 0.0f, 5.0f, "%.1f");
        ImGuiMCP::TextUnformatted("NPCs - spells");
        changed |= drawBlockRow("Weapon spell block##npc", current.NPCWeaponMagicEnabled, current.NPCWeaponMagicFactor);
        changed |= drawBlockRow("Shield spell block##npc", current.NPCShieldMagicEnabled, current.NPCShieldMagicFactor);
        changed |= ImGuiMCP::SliderFloat("Weapon spell stamina cost##npc", &current.NPCWeaponSpellStaminaCost, 0.0f, 50.0f, "%.1f");
        changed |= ImGuiMCP::SliderFloat("Shield spell stamina cost##npc", &current.NPCShieldSpellStaminaCost, 0.0f, 50.0f, "%.1f");
        changed |= ImGuiMCP::SliderFloat("Weapon spell magicka cost##npc", &current.NPCWeaponSpellMagickaCost, 0.0f, 50.0f, "%.1f");
        changed |= ImGuiMCP::SliderFloat("Shield spell magicka cost##npc", &current.NPCShieldSpellMagickaCost, 0.0f, 50.0f, "%.1f");
        changed |= ImGuiMCP::SliderFloat("Weapon flame cost multiplier##npc", &current.NPCWeaponFlameCostMultiplier, 0.0f, 5.0f, "%.1f");
        changed |= ImGuiMCP::SliderFloat("Shield flame cost multiplier##npc", &current.NPCShieldFlameCostMultiplier, 0.0f, 5.0f, "%.1f");

        if (changed) {
            Set(current);  // Apply slider and checkbox changes immediately.
            unsaved = true;
        }
        ImGuiMCP::Separator();
        if (ImGuiMCP::Button("Save")) {
            if (Save()) {
                unsaved = false;
            }
        }
        ImGuiMCP::SameLine();
        if (ImGuiMCP::Button("Revert")) {
            Load();
            unsaved = false;
        }
        if (unsaved) {
            ImGuiMCP::TextUnformatted("Unsaved changes");
        }
    }

    void RegisterMenu() {
        if (!SKSEMenuFramework::IsInstalled()) {
            SKSE::log::info("[settings] SKSE Menu Framework is not installed; INI settings remain available");
            return;
        }
        // The API header captures the module handle at DLL load time; SKSE may
        // load the framework after this plugin, so refresh it at kPostLoad.
        menuFramework = GetModuleHandleW(L"SKSEMenuFramework.dll");
        if (!menuFramework) {
            SKSE::log::warn("[settings] SKSE Menu Framework DLL exists but is not loaded");
            return;
        }
        SKSEMenuFramework::SetSection("Animated Projectile Block");
        SKSEMenuFramework::AddSectionItem("Settings", RenderMenuPage);
        SKSE::log::info("[settings] Registered SKSE Menu Framework page");
    }
}
