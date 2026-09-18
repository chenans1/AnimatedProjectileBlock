#pragma once

#include <atomic>
#include "settings.h"

class hooks {
    //logic adapted from valhalla combat, hooks adapted from arrowInterpreter. 
    //not doing flame projectile for now - it would require interrupting the caster
    //with maxsu block hit overhaul if you block a flame projectile you would literally never be able to move.
    public: 
        static inline void install() {
            SKSE::log::info("[hooks] attempting hooking projectile addimpact functions");

            {
                REL::Relocation<std::uintptr_t> vtable{ RE::VTABLE_ArrowProjectile[0]};
                _originalArrow = vtable.write_vfunc(0xBD, AddImpactProj);
            }

            SKSE::log::info("[hooks] attempting hooking projectile ApplyProjectileSpell");

            auto& trampoline = SKSE::GetTrampoline();
            // originalApply = trampoline.write_call<5>(REL::RelocationID(42943, 44123).address() + REL::Relocate(0x31C, 0x312), ApplyProjectileSpell);
            // originalApply = trampoline.write_call<5>(REL::Relocation<std::uintptr_t>{ REL::Offset(0x7EC608) }.address(), ApplyProjectileSpell);
            originalApply = trampoline.write_call<5>(REL::RelocationID(43015, 44206).address() + REL::Relocate(0x216, 0x218), ApplyProjectileSpell);
            SKSE::log::info("[Hooks] originalApply at address: 0x{:X}", originalApply.address());
            // projectileHooksInstalled = true;
            SKSE::log::info("[hooks] Finished hooks");
        }

        static void InstallSetEffectiveness() {
            if (originalSetEffectiveness.address()) {
                return;
            }

            REL::Relocation<std::uintptr_t> checkAddEffect{
                RELOCATION_ID(33763, 34547), REL::VariantOffset(0x4A3, 0x656, 0x427)
            };
            const auto callSite = checkAddEffect.address();
            if (*reinterpret_cast<const std::uint8_t*>(callSite) != 0xE8) {
                SKSE::log::error("[hooks] CheckAddEffect site 0x{:X} is not a 5-byte CALL", callSite);
                return;
            }

            originalSetEffectiveness = SKSE::GetTrampoline().write_call<5>(callSite, SetEffectiveness);
            SKSE::log::info("[hooks] SetEffectiveness installed at 0x{:X}; previous target 0x{:X}",
                callSite, originalSetEffectiveness.address());
        }

        static inline bool LoadForms() {
            auto* dataHandler = RE::TESDataHandler::GetSingleton();

            cooldownSpell = dataHandler->LookupForm<RE::SpellItem>(0x801, "AnimatedProjectileBlocking.esp");
            cooldownEffect = dataHandler->LookupForm<RE::EffectSetting>(0x800, "AnimatedProjectileBlocking.esp");

            if (!cooldownSpell || !cooldownEffect) {
                SKSE::log::error("Failed to load enchant cooldown forms: spell={}, effect={}", static_cast<void*>(cooldownSpell), static_cast<void*>(cooldownEffect));
                return false;
            }
            SKSE::log::info("Sucessfully loaded enchant cd forms: spell={}, effect={}", static_cast<void*>(cooldownSpell), static_cast<void*>(cooldownEffect));
            return true;
        }
        
    private: 
        struct Hit {
            RE::ObjectRefHandle target;
            RE::MagicItem* spell;
            float remainingDamage;
        };

        struct HitScope {
            std::optional<Hit> previous;
            explicit HitScope(std::optional<Hit> hit) : 
                previous(std::exchange(currentHit, std::move(hit)))
            {}
            ~HitScope()
            {
                currentHit = std::move(previous);
            }
        };

        static inline thread_local std::optional<Hit> currentHit;

        struct BlockProfile {
            bool enabled;
            float factor;
            bool shield;
        };

        static BlockProfile getBlockProfile(RE::Actor* actor, bool spell, const settings::config& cfg) {
            const bool shield = actor->GetWornArmor(RE::BGSBipedObjectForm::BipedObjectSlot::kShield) != nullptr;
            const bool player = actor == RE::PlayerCharacter::GetSingleton();
            if (player) {
                if (spell) {
                    return shield ? BlockProfile{cfg.playerShieldMagicEnabled, cfg.pcShieldMagicFactor, true}
                                  : BlockProfile{cfg.playerWeaponMagicEnabled, cfg.pcWeaponMagicFactor, false};
                }
                return shield ? BlockProfile{cfg.playerShieldArrowEnabled, cfg.pcShieldArrowFactor, true}
                              : BlockProfile{cfg.playerWeaponArrowEnabled, cfg.pcWeaponArrowFactor, false};
            }
            if (spell) {
                return shield ? BlockProfile{cfg.NPCShieldMagicEnabled, cfg.NPCShieldMagicFactor, true}
                              : BlockProfile{cfg.NPCWeaponMagicEnabled, cfg.NPCWeaponMagicFactor, false};
            }
            return shield ? BlockProfile{cfg.NPCShieldArrowEnabled, cfg.NPCShieldArrowFactor, true}
                          : BlockProfile{cfg.NPCWeaponArrowEnabled, cfg.NPCWeaponArrowFactor, false};
        }

        static float blockSetting(const char* name, float fallback) {
            auto* collection = RE::GameSettingCollection::GetSingleton();
            auto* setting = collection ? collection->GetSetting(name) : nullptr;
            return setting && setting->GetType() == RE::Setting::Type::kFloat ? setting->GetFloat() : fallback;
        }

        static float blockedFraction(RE::Actor* actor, const BlockProfile& profile) {
            // The GMST supplies the starting block value. Perks then modify that value via the same entry point used by the game's block calculation.
            float block = profile.shield ? blockSetting("fShieldBaseFactor", 0.45f) : blockSetting("fBlockWeaponBase", 0.30f);
            RE::BGSEntryPoint::HandleEntryPoint(RE::BGSEntryPoint::ENTRY_POINT::kModPercentBlocked, actor, &block);
            const float cap = std::clamp(blockSetting("fBlockMax", 0.70f), 0.0f, 1.0f);
            return std::clamp(block, 0.0f, cap) * std::clamp(profile.factor, 0.0f, 1.0f);
        }

        //cooldown to not excessively make actors play blockhit animations
        static bool applyCD(RE::Actor* actor) {
            if (!actor || !cooldownSpell || !cooldownEffect) {
                return false;
            }
            auto* magicTarget = actor->GetMagicTarget();
            if (!magicTarget) {
                return false;
            }

            if (magicTarget->HasMagicEffect(cooldownEffect)) {
                return false;
            }

            if (auto* caster = actor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant)) {
                // SKSE::log::info("[EnchantCooldown] applying cooldown spell");
                caster->CastSpellImmediate(cooldownSpell, true, actor, 1.0f, false, 0.0f, actor);
                return true;

            }
            return false;
        }
        
        //gonna do this via velocity check instead, I think. Using dot product to check for projectile heading to calculate if it's
        //within the block angle cone. 
        static bool checkBlockAngle(RE::Actor* actor, RE::Projectile* projectile) {
            auto* gameSettings = RE::GameSettingCollection::GetSingleton();
            auto* gmst = gameSettings ? gameSettings->GetSetting("fCombatHitConeAngle") : nullptr;
            if (gmst) {
                float projX;
                float projY;
                const float fCombatHitConeAngle = gmst->GetFloat();
                if (projectile->formType == RE::FormType::ProjectileFlame) {
                    //flame projs have no velocity. I'm going to check for the actual beam heading instead.
                    const float heading = projectile->GetAngleZ();
                    // SKSE::log::info("[checkBlockAngle]: flame projectile heading: {}", heading);
                    projX = std::sin(heading);
                    projY = std::cos(heading);
                } else {
                    const auto& v = projectile->GetProjectileRuntimeData().velocity;
                    projX = v.x;
                    projY = v.y;
                    // SKSE::log::info("[checkBlockAngle]: not flame projectile: [{}, {}]", v.x, v.y);
                }
                
                const float horizontalSpeed = std::hypot(projX, projY);
                if (horizontalSpeed < 0.0001f) {
                    if (settings::Get().log) {
                        SKSE::log::info("[checkBlockAngle]: negligible horizontal speed: {}", horizontalSpeed);
                    }
                    return false;
                }
                
                const float blockerAngle = actor->GetAngleZ();
                const float dotProduct = std::sin(blockerAngle) * (-projX / horizontalSpeed) + std::cos(blockerAngle) * (-projY / horizontalSpeed);
                // SKSE::log::info("[checkBlockAngle] angle: {}/{}", std::acos(std::clamp(dotProduct, -1.0f, 1.0f)) * 180.0f/3.1415927f, fCombatHitConeAngle);
                return dotProduct >= std::cos(fCombatHitConeAngle * 3.1415927f/180.0f);
            }
            return false;
        }

        static bool performProjectileBlock(RE::Actor* blocker, RE::Projectile* projectile, const settings::config& cfg) {
            if (!blocker || !projectile) {
                return false;
            }

            if (!getBlockProfile(blocker, false, cfg).enabled) {
                return false;
            }
            if (!checkBlockAngle(blocker, projectile) || !blocker->IsBlocking()) {
                return false;
            }
            if (projectile->formType == RE::FormType::ProjectileFlame) {
                if (!applyCD(blocker)) {
                    // SKSE::log::info("[performProjectileBlock]: flame projectile blocker has CD effect");
                    return true; 
                }
            }
            blocker->NotifyAnimationGraph("BlockHitStart");
            return true;
        }

        static void processProjectileCollision(RE::Projectile* a_projectile, RE::TESObjectREFR* a_ref) { 
            // SKSE::log::info("[processProjCollision]");
            if (!a_projectile || !a_ref) {
                return;
            }
            if (a_ref->formType == RE::FormType::ActorCharacter) {
                auto* actor = a_ref->As<RE::Actor>();
                const auto cfg = settings::Get();
                if (performProjectileBlock(actor, a_projectile, cfg)) {
                    const auto profile = getBlockProfile(actor, false, cfg);
                    auto& rd = a_projectile->GetProjectileRuntimeData();
                    const float reduction = blockedFraction(actor, profile);
                    rd.weaponDamage *= 1.0f - reduction;
                    if (cfg.log) {
                        SKSE::log::info("[processProjectileCollision] projectile={} target={} reduction={} remainingWeaponDamage={}",
                            static_cast<void*>(a_projectile), static_cast<void*>(a_ref), reduction, rd.weaponDamage);
                    }
                }
            }
        }

        //need to hook specific vtable funcs, hooking the base vfunc doesnt work.
        static RE::Projectile::ImpactData* AddImpactProj(RE::ArrowProjectile* a_projectile, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc, const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6, std::uint32_t a_arg7) {
            // SKSE::log::info("[AddImpactProj]");
            processProjectileCollision(a_projectile, a_ref);
            return _originalArrow(a_projectile, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
        }

        static inline REL::Relocation<decltype(AddImpactProj)> _originalArrow;

        static inline RE::SpellItem* cooldownSpell = nullptr;       // 0x800
        static inline RE::EffectSetting* cooldownEffect = nullptr;  // 0x801

        //it turns out this function - which applies the spell effects from projectile collision - actually runs before the addimpact() hooks
        //it also turns out in the same synchronous call, setEffectiveness is called. 
        static void ApplyProjectileSpell(RE::MagicCaster* caster, const RE::NiPoint3* impactPos, RE::Projectile* projectile, RE::TESObjectREFR* target, float arg5, float arg6, std::uint8_t arg7, std::uint8_t arg8) {
            // SKSE::log::info("[ApplyProjectileSpell] ENTER: projectile={} target={} blocked={}", static_cast<void*>(projectile), static_cast<void*>(target), currentHit.has_value());
            std::optional<Hit> hit;
            if (projectile && target) {
                if (auto* actor = target->As<RE::Actor>()) {
                    if (actor->IsBlocking() && checkBlockAngle(actor, projectile)) {
                        const auto cfg = settings::Get();
                        const auto profile = getBlockProfile(actor, true, cfg);
                        auto* spell = projectile->GetProjectileRuntimeData().spell;
                        if (spell && profile.enabled) {
                            hit = Hit{
                                target->GetHandle(),
                                spell,
                                1.0f - blockedFraction(actor, profile)
                            };
                            if (projectile->formType == RE::FormType::ProjectileFlame) {
                                if (applyCD(actor)) {
                                    actor->NotifyAnimationGraph("BlockHitStart");
                                }
                            } else {
                                actor->NotifyAnimationGraph("BlockHitStart");
                            }
                        }
                    }
                }
            }
            HitScope scope(std::move(hit));
            originalApply(caster, impactPos, projectile, target, arg5, arg6, arg7, arg8);
            // SKSE::log::info("[ApplyProjectileSpell] EXIT: projectile={} target={} blocked={}", static_cast<void*>(projectile), static_cast<void*>(target), currentHit.has_value());
        }

        static void SetEffectiveness(RE::ActiveEffect* effect, float power, bool onlyHostile) {
            originalSetEffectiveness(effect, power, onlyHostile);
            if (!currentHit || !effect) {
                // SKSE::log::info("[SetEffectiveness] No currentHit");
                return;
            }
            // Must belong to the spell from the blocked projectile.
            if (effect->spell != currentHit->spell) {
                if (settings::Get().log) {
                    SKSE::log::info("[SetEffectiveness] effect spell: {} is not currenthit spell: {}", static_cast<void*>(effect->spell), static_cast<void*>(currentHit->spell));
                }
                return;
            }
            //only affect damage to H/M/S
            const auto* baseEffect = effect->GetBaseObject();
            constexpr auto isVitalActorValue = [](RE::ActorValue value) {
                return value == RE::ActorValue::kHealth || value == RE::ActorValue::kStamina || value == RE::ActorValue::kMagicka;
            };
            const bool damageHMS = effect->IsCausingHealthDamage() || (baseEffect && baseEffect->IsDetrimental() && (isVitalActorValue(baseEffect->data.primaryAV) || isVitalActorValue(baseEffect->data.secondaryAV)));
            if (!damageHMS) {
                if (settings::Get().log) {
                    SKSE::log::info("[SetEffectiveness] effect does not damage Health, Stamina or Magicka");
                }
                return;
            }
            // MagicTarget is a secondary base of Actor. Ask it for the owning reference instead of reinterpreting its address as an Actor*.
            auto* victimRef = effect->target ? effect->target->GetTargetStatsObject() : nullptr;
            if (!victimRef || !victimRef->As<RE::Actor>()) {
                if (settings::Get().log) {
                    SKSE::log::info("[SetEffectiveness]: unusable victimref");
                }
                return;
            }
            const auto victimHandle = victimRef->GetHandle();
            if (victimHandle != currentHit->target) {
                if (settings::Get().log) {
                    SKSE::log::info("[SetEffectiveness] target mismatch: effect target {:08X} (handle {:08X}), impact handle {:08X}", victimRef->GetFormID(), victimHandle.native_handle(), currentHit->target.native_handle());
                }
                return;
            }
            const float oldMagnitude = effect->magnitude;
            effect->magnitude *= currentHit->remainingDamage;
            if (settings::Get().log) {
                SKSE::log::info("[SetEffectiveness] blocked spell effect={} magnitude {} -> {}", static_cast<void*>(effect), oldMagnitude, effect->magnitude);
            }

        }

        static inline REL::Relocation<decltype(ApplyProjectileSpell)> originalApply;
        static inline REL::Relocation<decltype(SetEffectiveness)> originalSetEffectiveness;
};
