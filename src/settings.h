#pragma once

namespace settings {
    struct config {
        bool log = false;

        bool playerWeaponArrowEnabled;
        bool playerShieldArrowEnabled;
        bool playerWeaponMagicEnabled;
        bool playerShieldMagicEnabled;

        bool NPCWeaponArrowEnabled;
        bool NPCShieldArrowEnabled;
        bool NPCWeaponMagicEnabled;
        bool NPCShieldMagicEnabled;

        float pcWeaponArrowFactor;
        float pcShieldArrowFactor;
        float pcWeaponMagicFactor;
        float pcShieldMagicFactor;

        float NPCWeaponArrowFactor;
        float NPCShieldArrowFactor;
        float NPCWeaponMagicFactor;
        float NPCShieldMagicFactor;

        void RegisterMenu();
        void __stdcall RenderMenuPage();

        void load();
        void save();
    };
}