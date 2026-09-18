#pragma once

namespace settings {
    struct config {
        bool log = true;

        bool playerWeaponArrowEnabled = true;
        bool playerShieldArrowEnabled = true;
        bool playerWeaponMagicEnabled = true;
        bool playerShieldMagicEnabled = true;

        bool NPCWeaponArrowEnabled = true;
        bool NPCShieldArrowEnabled = true;
        bool NPCWeaponMagicEnabled = true;
        bool NPCShieldMagicEnabled = true;

        // Multiplier applied to the calculated block fraction (0.0 to 1.0).
        float pcWeaponArrowFactor = 0.66f;
        float pcShieldArrowFactor = 0.85f;
        float pcWeaponMagicFactor = 0.50f;
        float pcShieldMagicFactor = 0.66f;

        float NPCWeaponArrowFactor = 0.66f;
        float NPCShieldArrowFactor = 0.85f;
        float NPCWeaponMagicFactor = 0.50f;
        float NPCShieldMagicFactor = 0.66f;

        // Multiplies the full arrow block stamina cost (base + damage term).
        float pcArrowBlockCostFactor = 1.0f;
        float NPCArrowBlockCostFactor = 1.0f;

        // Flat resource cost per blocked spell impact. A zero cost disables that resource.
        float pcWeaponSpellStaminaCost = 8.0f;
        float pcShieldSpellStaminaCost = 5.0f;
        float pcWeaponSpellMagickaCost = 8.0f;
        float pcShieldSpellMagickaCost = 5.0f;
        float NPCWeaponSpellStaminaCost = 8.0f;
        float NPCShieldSpellStaminaCost = 5.0f;
        float NPCWeaponSpellMagickaCost = 8.0f;
        float NPCShieldSpellMagickaCost = 5.0f;

        // Applied on each flame spell application: matching spell cost times this multiplier.
        float pcWeaponFlameCostMultiplier = 0.1f;
        float pcShieldFlameCostMultiplier = 0.1f;
        float NPCWeaponFlameCostMultiplier = 0.1f;
        float NPCShieldFlameCostMultiplier = 0.1f;
    };

    // Projectile hooks may run on threads other than the menu thread.
    config Get();
    void Set(const config& value);
    void Load();
    bool Save();
    void RegisterMenu();
    void __stdcall RenderMenuPage();
}
