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
        float pcWeaponArrowFactor = 0.33f;
        float pcShieldArrowFactor = 0.50f;
        float pcWeaponMagicFactor = 0.25f;
        float pcShieldMagicFactor = 0.33f;

        float NPCWeaponArrowFactor = 0.33f;
        float NPCShieldArrowFactor = 0.50f;
        float NPCWeaponMagicFactor = 0.25f;
        float NPCShieldMagicFactor = 0.33f;
    };

    // Projectile hooks may run on threads other than the menu thread.
    config Get();
    void Set(const config& value);
    void Load();
    bool Save();
    void RegisterMenu();
    void __stdcall RenderMenuPage();
}
