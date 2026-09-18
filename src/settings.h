#pragma once

namespace settings {
    struct config {
        bool log = true;

        bool playerWeaponArrowEnabled = true;
        bool playerShieldArrowEnabled = true;
        bool playerWeaponMagicEnabled = true;
        bool playerShieldMagicEnabled = true;
        bool playerWeaponSpellDamageReductionEnabled = true;
        bool playerShieldSpellDamageReductionEnabled = true;

        bool NPCWeaponArrowEnabled = true;
        bool NPCShieldArrowEnabled = true;
        bool NPCWeaponMagicEnabled = true;
        bool NPCShieldMagicEnabled = true;
        bool NPCWeaponSpellDamageReductionEnabled = true;
        bool NPCShieldSpellDamageReductionEnabled = true;

        // Multiplier applied to the calculated block fraction (0.0 to 1.0).
        float pcWeaponArrowFactor = 0.70f;
        float pcShieldArrowFactor = 0.85f;
        float pcWeaponMagicFactor = 0.70f;
        float pcShieldMagicFactor = 0.85f;

        float NPCWeaponArrowFactor = 0.70f;
        float NPCShieldArrowFactor = 0.85f;
        float NPCWeaponMagicFactor = 0.70f;
        float NPCShieldMagicFactor = 0.85f;

        // Multiplies the full arrow block stamina cost fStaminaBlockBase + arrow damage x fStaminaBlockDmgMult) x factor.
        float pcArrowBlockCostFactor = 1.0f;
        float NPCArrowBlockCostFactor = 1.0f;

        // Flat resource cost per blocked spell impact
        float pcWeaponSpellStaminaCost = 8.0f;
        float pcShieldSpellStaminaCost = 5.0f;
        float pcWeaponSpellMagickaCost = 8.0f;
        float pcShieldSpellMagickaCost = 5.0f;
        float NPCWeaponSpellStaminaCost = 8.0f;
        float NPCShieldSpellStaminaCost = 5.0f;
        float NPCWeaponSpellMagickaCost = 8.0f;
        float NPCShieldSpellMagickaCost = 5.0f;

        float pcWeaponFlameCostMultiplier = 0.1f;
        float pcShieldFlameCostMultiplier = 0.05f;
        float NPCWeaponFlameCostMultiplier = 0.1f;
        float NPCShieldFlameCostMultiplier = 0.05f;
    };

    config Get();
    void Set(const config& value);
    void Load();
    bool Save();
    void RegisterMenu();
    void __stdcall RenderMenuPage();
    void __stdcall RenderCostsPage();
}
