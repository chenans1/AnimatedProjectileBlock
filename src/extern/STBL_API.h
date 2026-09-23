#pragma once

#include <cstdint>
/*
CanTimedBlock evaluates the window and damage policy without applying effects.
TriggerTimedBlock commits effects after a caller completes its own checks.
TryTriggerTimedBlock is the convenience operation that evaluates and commits.
External consumers remain responsible for applying the returned damage multiplier.
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

        // Additional cost relative to the base block cost. For example, 1.0
        // means reflection costs one additional base cost (2.0x total).
        // This is zero when the projectile should not be reflected.
        float reflectionCostMultiplier = 0.0f;
        bool reflectProjectile = false;
        [[nodiscard]] bool Triggered() const noexcept {
            return outcome != TimedBlockOutcome::NotTriggered;
        }

        [[nodiscard]] bool FullyBlocked() const noexcept {
            return outcome == TimedBlockOutcome::FullyBlocked;
        }

        [[nodiscard]] bool ShouldReflect() const noexcept {
            return Triggered() && reflectProjectile;
        }

    };

    class STBL {
        public:
            // Pure query: never applies gameplay effects.
            [[nodiscard]] virtual TimedBlockResult CanTimedBlock(const TimedBlockRequest& request) noexcept = 0;

            // Revalidates the timed-block context and applies gameplay effects.
            virtual bool TriggerTimedBlock(const TimedBlockRequest& request) noexcept = 0;

            // Convenience operation that evaluates and immediately applies effects.
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
