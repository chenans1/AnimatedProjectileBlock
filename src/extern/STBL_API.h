#pragma once

#include <cstdint>
/*
TryTriggerTimedBlock is called when a timed block occurs - external plugin consumer must still handle damage reduction.
The try trigger checks if the timed block window is active and if it is, apply stagger/spell/timed block mod events.
*/
namespace RE {
    class Actor;
    class TESObjectREFR;
}
namespace STBL_API {
    enum class InterfaceVersion : std::uint8_t {
        V1 = 1
    };

    enum class AttackType : std::uint8_t {
        Melee = 0,
        Spell = 1,
        Arrow = 2
    };

    enum class TimedBlockOutcome : std::uint8_t {
        NotTriggered = 0,
        Reduced = 1,
        FullyBlocked = 2
    };

    struct TimedBlockRequest {
        AttackType attackType = AttackType::Melee;
        RE::Actor* attacker = nullptr;
        RE::Actor* defender = nullptr;

        //optional
        //RE::TESObjectREFR* source = nullptr;
    };

    struct TimedBlockResult {
        TimedBlockOutcome outcome = TimedBlockOutcome::NotTriggered;

        // Multiplier applied to incoming damage:
        // 1.0 = unchanged, 0.5 = half damage, 0.0 = no damage.
        float damageMultiplier = 1.0f;
        [[nodiscard]] bool Triggered() const noexcept {
            return outcome != TimedBlockOutcome::NotTriggered;
        }

        [[nodiscard]] bool FullyBlocked() const noexcept {
            return outcome == TimedBlockOutcome::FullyBlocked;
        }
    };

    class STBL {
        public:
            [[nodiscard]] virtual TimedBlockResult TryTriggerTimedBlock(const TimedBlockRequest& request) noexcept = 0;
        protected:
            virtual ~STBL() = default;
    };

    using RequestPluginAPI_t = void* (*)(InterfaceVersion);

    [[nodiscard]] inline STBL* RequestInterface() noexcept {
        const auto module = REX::W32::GetModuleHandleW(L"simple-timed-block.dll");

        if (!module) {
            return nullptr;
        }
        const auto requestAPI =reinterpret_cast<RequestPluginAPI_t>(REX::W32::GetProcAddress(module, "RequestPluginAPI"));
        return requestAPI ? static_cast<STBL*>(requestAPI(InterfaceVersion::V1)) : nullptr;
    }
}
