#pragma once

#include <atomic>
#include <charconv>
#include <cctype>
#include "settings.h"
#include "extern/PerkEntryPointExtenderAPI.h"
#include "extern/STBL_API.h"

class hooks {
    //logic adapted from valhalla combat, hooks adapted from arrowInterpreter. 
    public: 
        static inline void install() {
            SKSE::log::info("[hooks] attempting hooking projectile addimpact functions");
            {
                REL::Relocation<std::uintptr_t> vtable{ RE::VTABLE_ArrowProjectile[0]};
                _originalArrow = vtable.write_vfunc(0xBD, AddImpactProj);
            }

            SKSE::log::info("[hooks] attempting hooking projectile ApplyProjectileSpell");

            auto& trampoline = SKSE::GetTrampoline();
            originalApply = trampoline.write_call<5>(REL::RelocationID(43015, 44206).address() + REL::Relocate(0x216, 0x218), ApplyProjectileSpell);
            SKSE::log::info("[Hooks] originalApply at address: 0x{:X}", originalApply.address());
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
            
            ArrowBlockerSpell = dataHandler->LookupForm<RE::SpellItem>(0x803, "AnimatedProjectileBlocking.esp");
            ArrowAttackerSpell = dataHandler->LookupForm<RE::SpellItem>(0x805, "AnimatedProjectileBlocking.esp");
            SpellBlockerSpell = dataHandler->LookupForm<RE::SpellItem>(0x807, "AnimatedProjectileBlocking.esp");
            SpellAttackerSpell = dataHandler->LookupForm<RE::SpellItem>(0x809, "AnimatedProjectileBlocking.esp");

            if (!ArrowBlockerSpell || !ArrowAttackerSpell || !SpellBlockerSpell || !SpellAttackerSpell) {
                SKSE::log::error("Failed to load attacker/blocker spell forms: ArrowBlockerSpell={}, ArrowAttackerSpell={}, SpellBlockerSpell={}, SpellAttackerSpell={}", 
                    static_cast<void*>(ArrowBlockerSpell), static_cast<void*>(ArrowAttackerSpell), static_cast<void*>(SpellBlockerSpell), static_cast<void*>(SpellAttackerSpell));
                return false;
            }

            SKSE::log::info("Successfully loaded arrow/spell attacker/blocker spell forms: ArrowBlockerSpell={}, ArrowAttackerSpell={}, SpellBlockerSpell={}, SpellAttackerSpell={}", 
                    static_cast<void*>(ArrowBlockerSpell), static_cast<void*>(ArrowAttackerSpell), static_cast<void*>(SpellBlockerSpell), static_cast<void*>(SpellAttackerSpell));

            const auto cfg = settings::Get();
            playerWeaponArrowPerk = loadPerkRequirement(cfg.playerWeaponArrowPerkRequirement, "weapon arrow");
            playerShieldArrowPerk = loadPerkRequirement(cfg.playerShieldArrowPerkRequirement, "shield arrow");
            playerWeaponSpellPerk = loadPerkRequirement(cfg.playerWeaponSpellPerkRequirement, "weapon spell");
            playerShieldSpellPerk = loadPerkRequirement(cfg.playerShieldSpellPerkRequirement, "shield spell");
            return true;
        }

        static void requestSTBL() {
            stbl = STBL_API::RequestInterface();
            if (stbl) {
                SKSE::log::info("Simple Timed Block API acquired");
            } else {
                SKSE::log::info("Simple Timed Block - tweaked not available; using normal behavior");
            }
        }

    private:
        static inline STBL_API::STBL* stbl = nullptr;

        static inline RE::SpellItem* cooldownSpell = nullptr;       // 0x800
        static inline RE::EffectSetting* cooldownEffect = nullptr;  // 0x801

        static inline RE::SpellItem* ArrowBlockerSpell = nullptr; // 0x803 attacker casts this spell on the blocker
        static inline RE::SpellItem* ArrowAttackerSpell = nullptr; // 0x805 blocker casts this spell on the attacker
        static inline RE::SpellItem* SpellBlockerSpell = nullptr;  // 0x807 attacker casts this spell on the blocker
        static inline RE::SpellItem* SpellAttackerSpell = nullptr; // 0x809 blocker casts this spell on the attacker

        struct PerkRequirement {
            bool configured = false;
            RE::BGSPerk* perk = nullptr;
        };

        static inline PerkRequirement playerWeaponArrowPerk;
        static inline PerkRequirement playerShieldArrowPerk;
        static inline PerkRequirement playerWeaponSpellPerk;
        static inline PerkRequirement playerShieldSpellPerk;

        static PerkRequirement loadPerkRequirement(std::string_view setting, std::string_view context);

        struct Hit {
            RE::ObjectRefHandle target;
            RE::MagicItem* spell;
            float remainingDamage;
            // bool shield;
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

        // The AE getter checks this per-ActiveEffect bit before returning its
        // MGEF hit shader. CommonLib does not currently name the bit.
        static constexpr auto noHitShaderFlag = static_cast<RE::ActiveEffect::Flag>(1u << 1);

        static void suppressHitShader(RE::ActiveEffect* effect) {
            if (!effect) {
                return;
            }

            auto* baseEffect = effect->GetBaseObject();
            if (!baseEffect || !baseEffect->data.effectShader ||
                baseEffect->data.flags.any(RE::EffectSetting::EffectSettingData::Flag::kNoHitEffect)) {
                return;
            }

            effect->flags.set(noHitShaderFlag);
        }

        struct BlockProfile {
            bool enabled;
            float factor;
            bool shield;
            const PerkRequirement* perkRequirement;
        };

        static BlockProfile getBlockProfile(RE::Actor* actor, bool spell, const settings::config& cfg);

        static bool hasRequiredPerk(RE::Actor* actor, const BlockProfile& profile);

        enum class BlockMode {
            kDisabled,
            kAnimationOnly,
            kDamageReduction
        };

        static BlockMode getBlockMode(RE::Actor* actor, const BlockProfile& profile, bool damageReductionEnabled);

        static bool spellDamageReductionEnabled(RE::Actor* actor, const BlockProfile& profile, const settings::config& cfg);

        static bool arrowDamageReductionEnabled(RE::Actor* actor, const BlockProfile& profile, const settings::config& cfg);

        static float projectilePerkMultiplier(RE::Actor* actor, std::string_view category);

        static float blockSetting(const char* name, float fallback);

        //calculates how much to block
        static float blockedFraction(RE::Actor* actor, const BlockProfile& profile, bool isSpell);

        struct SpellCosts {
            float stamina;
            float magicka;
            float flameMultiplier;
        };

        static SpellCosts spellCosts(RE::Actor* actor, const BlockProfile& profile, const settings::config& cfg);
        static float PEPE_costMult(RE::Actor* actor, std::string_view category);

        static bool tryConsumeSpellBlockResources(RE::Actor* blocker, float staminaCost, float magickaCost);

        //cooldown to not excessively make actors play blockhit animations
        static bool applyCD(RE::Actor* actor);

        static void castContextSpell(RE::Actor* a_caster, RE::Actor* a_target, RE::SpellItem* a_spell);

        static void awardBlockExperience(RE::Actor* blocker, float incomingDamage);

        static void sendBlockModEvent(RE::Actor* blocker, RE::Actor* attacker, bool isSpell);
        
        static bool playSpellBlockAnimation(RE::Actor* actor, bool flame);

        //gonna do this via velocity check instead, I think. Using dot product to check for projectile heading compared to actor heading and blockangle
        static bool checkBlockAngle(RE::Actor* actor, RE::Projectile* projectile);
        
        static bool tryConsumeArrowBlockStamina(RE::Actor* blocker, float incomingDamage, const settings::config& cfg, float& cost);

        static bool reflectArrow(RE::Actor* blocker, RE::Actor* target, RE::TESAmmo* ammo, RE::TESObjectWEAP* weapon);
        static bool reflectSpell(RE::Actor* blocker, RE::Actor* target, RE::SpellItem* a_spell);

        static void processArrowCollision(RE::Projectile* a_projectile, RE::TESObjectREFR* a_ref) { 
            // SKSE::log::info("[processProjCollision]");
            if (!a_projectile || !a_ref) {
                return;
            }
            if (a_ref->formType == RE::FormType::ActorCharacter) {
                auto* actor = a_ref->As<RE::Actor>();
                if (!actor) {
                    return;
                }
                const auto cfg = settings::Get();
                const auto profile = getBlockProfile(actor, false, cfg);
                if (!actor->IsBlocking() || !checkBlockAngle(actor, a_projectile)) {
                    return;
                }
                auto& rd = a_projectile->GetProjectileRuntimeData();
                auto* attackerRef = rd.shooter.get().get();
                auto* attacker = attackerRef ? attackerRef->As<RE::Actor>() : nullptr;
                const auto mode = getBlockMode(actor, profile, arrowDamageReductionEnabled(actor, profile, cfg));
                const float incomingDamage = rd.weaponDamage;
                float staminaCost = 0.0f;
                //stbl integration, for overcap timed block. For undercap timed block damage reduction api is not needed - i will consider hit data modification some day for native compat?
                if (stbl && actor->IsPlayerRef()) {
                    const STBL_API::TimedBlockRequest request{STBL_API::AttackType::Arrow, attacker, actor};
                    const auto isTimedBlocking = stbl->CanTimedBlock(request);
                    const float reflectionMult = isTimedBlocking.reflectionCostMultiplier;
                    // if (isTimedBlocking.outcome != STBL_API::TimedBlockOutcome::NotTriggered) {
                    if (isTimedBlocking.Triggered()) {
                        if (tryConsumeArrowBlockStamina(actor, incomingDamage, cfg, staminaCost)) {
                            if (cfg.log) {
                                SKSE::log::info("[processArrowCollision] arrow timed block success");
                            }
                            actor->NotifyAnimationGraph("BlockHitStart");
                            //we accept 0 reduction here. in this case purely timed block DR from STBL applies
                            const float reduction = mode == BlockMode::kDamageReduction ? blockedFraction(actor, profile, false) : 0.0f;
                            rd.weaponDamage = incomingDamage * (1.0f - reduction) * (isTimedBlocking.damageMultiplier);
                            awardBlockExperience(actor, incomingDamage);
                            stbl->TriggerTimedBlock(request);
                            if (attacker) {
                                castContextSpell(actor, attacker, ArrowBlockerSpell);
                                castContextSpell(attacker, actor, ArrowAttackerSpell);
                                sendBlockModEvent(actor, attacker, false);
                                //handle projectile reflection here
                                if (isTimedBlocking.ShouldReflect()) {
                                    float reflectionCost = staminaCost * reflectionMult;
                                    if (tryConsumeArrowBlockStamina(actor, incomingDamage, cfg, reflectionCost)) {
                                        if (cfg.log) {
                                            SKSE::log::info("[processArrowCollision] attempting arrow reflection");
                                        }
                                        reflectArrow(actor, attacker, rd.ammoSource, rd.weaponSource);
                                    }
                                }
                            }
                            return;
                        }
                    }
                }

                if (mode == BlockMode::kDisabled) {
                    return;
                }
                if (mode == BlockMode::kAnimationOnly) {
                    actor->NotifyAnimationGraph("BlockHitStart");
                    return;
                }
                const float reduction = blockedFraction(actor, profile, false);
                if (reduction <= 0.0f) {
                    return;
                }

                if (!tryConsumeArrowBlockStamina(actor, incomingDamage, cfg, staminaCost)) {
                    if (cfg.log) {
                        SKSE::log::info("[processArrowCollision] arrow block failed: target={} stamina={} required={}",
                            static_cast<void*>(actor), actor->GetActorValue(RE::ActorValue::kStamina), staminaCost);
                    }
                    return;
                }

                actor->NotifyAnimationGraph("BlockHitStart");
                rd.weaponDamage = incomingDamage * (1.0f - reduction);
                awardBlockExperience(actor, incomingDamage);

                if (attacker) {
                    castContextSpell(actor, attacker, ArrowBlockerSpell);
                    castContextSpell(attacker, actor, ArrowAttackerSpell);
                    sendBlockModEvent(actor, attacker, false);
                }
                if (cfg.log) {
                    SKSE::log::info("[processArrowCollision] projectile={} target={} reduction={} staminaCost={} remainingWeaponDamage={}",
                        static_cast<void*>(a_projectile), static_cast<void*>(a_ref), reduction, staminaCost, rd.weaponDamage);
                }
            }
        }

        //need to hook specific vtable funcs, hooking the base vfunc doesnt work.
        static RE::Projectile::ImpactData* AddImpactProj(RE::ArrowProjectile* a_projectile, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc, const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6, std::uint32_t a_arg7) {
            // SKSE::log::info("[AddImpactProj]");
            processArrowCollision(a_projectile, a_ref);
            return _originalArrow(a_projectile, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
        }

        static inline REL::Relocation<decltype(AddImpactProj)> _originalArrow;

        //it turns out this function - which applies the spell effects from projectile collision - actually runs before the addimpact() for projectiles, super convenient
        //it also turns out in the same synchronous call, setEffectiveness is called. 
        static void ApplyProjectileSpell(RE::MagicCaster* caster, const RE::NiPoint3* impactPos, RE::Projectile* projectile, RE::TESObjectREFR* target, float arg5, float arg6, std::uint8_t arg7, std::uint8_t arg8) {
            std::optional<Hit> hit;
            if (projectile && target) {
                if (auto* actor = target->As<RE::Actor>()) {
                    if (actor->IsBlocking() && checkBlockAngle(actor, projectile)) {
                        const auto cfg = settings::Get();
                        const auto profile = getBlockProfile(actor, true, cfg);
                        auto* spell = projectile->GetProjectileRuntimeData().spell;
                        if (!spell) {
                            HitScope scope(std::move(hit));
                            originalApply(caster, impactPos, projectile, target, arg5, arg6, arg7, arg8);
                            return;
                        }
                        const bool flame = projectile->formType == RE::FormType::ProjectileFlame;
                        const auto mode = getBlockMode(actor, profile, spellDamageReductionEnabled(actor, profile, cfg));
                        auto* attacker = caster ? caster->GetCasterAsActor() : nullptr;
                        auto costs = spellCosts(actor, profile, cfg);
                        if (flame) {
                            const float scale = std::clamp(costs.flameMultiplier, 0.0f, 5.0f);
                            costs.stamina *= scale;
                            costs.magicka *= scale;
                        }
                        //stbl integration, for overcap timed block. For undercap timed block damage reduction api is not needed - i will consider hit data modification some day for native compat?
                        if (stbl && actor->IsPlayerRef()) {
                            const STBL_API::TimedBlockRequest request{STBL_API::AttackType::Spell, attacker, actor};
                            const auto isTimedBlocking = stbl->CanTimedBlock(request);
                            // if (isTimedBlocking.outcome != STBL_API::TimedBlockOutcome::NotTriggered) {
                            if (isTimedBlocking.Triggered()) {
                                if (tryConsumeSpellBlockResources(actor, costs.stamina, costs.magicka) && actor->IsPlayerRef()) {
                                    if (cfg.log) {
                                        SKSE::log::info("[ApplyProjectileSpell] spell timed block success");
                                    }
                                    //always damage reduction, but not always return the simple timed block event to avoid spam - only if blockhit
                                    const float reduction = mode == BlockMode::kDamageReduction ? blockedFraction(actor, profile, false) : 0.0f;
                                    hit = Hit{target->GetHandle(), spell, (1.0f - reduction) * (isTimedBlocking.damageMultiplier)};
                                    if (playSpellBlockAnimation(actor, flame)){
                                        //block experience should work - it's always being called anyways, and with stbl it overwrites the damage reduction.
                                        stbl->TriggerTimedBlock(request);
                                        if (attacker) {
                                            castContextSpell(actor, attacker, SpellBlockerSpell);
                                            castContextSpell(attacker, actor, SpellAttackerSpell);
                                            sendBlockModEvent(actor, attacker, true);
                                            // if (isTimedBlocking.reflectProjectile) {
                                            //handle projectile reflection here
                                            if (isTimedBlocking.ShouldReflect()) {
                                                if (auto* reflectedSpell = spell->As<RE::SpellItem>()) {
                                                    if (tryConsumeSpellBlockResources(actor, costs.stamina * isTimedBlocking.reflectionCostMultiplier, costs.magicka * isTimedBlocking.reflectionCostMultiplier)) {
                                                        if (cfg.log) {
                                                            SKSE::log::info("[ApplyProjectileSpell] attempting spell reflection");
                                                        }
                                                        reflectSpell(actor, attacker, reflectedSpell);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    HitScope scope(std::move(hit));
                                    originalApply(caster, impactPos, projectile, target, arg5, arg6, arg7, arg8);
                                    return;
                                }
                            }
                        }

                        if (mode == BlockMode::kAnimationOnly) {
                            playSpellBlockAnimation(actor, flame);
                        } else if (mode == BlockMode::kDamageReduction) {
                            const float reduction = blockedFraction(actor, profile, true);
                            if (reduction > 0.0f) {
                                const bool paid = tryConsumeSpellBlockResources(actor, costs.stamina, costs.magicka);
                                if (cfg.log && (!flame || !paid)) {
                                    SKSE::log::info("[ApplyProjectileSpell] spell block {}: target={} staminaCost={} magickaCost={}",
                                        paid ? "paid" : "failed", static_cast<void*>(actor), costs.stamina, costs.magicka);
                                }
                                if (paid) {
                                    hit = Hit{target->GetHandle(), spell, 1.0f - reduction};
                                    if (playSpellBlockAnimation(actor, flame)) {
                                        if (attacker) {
                                            castContextSpell(actor, attacker, SpellBlockerSpell);
                                            castContextSpell(attacker, actor, SpellAttackerSpell);
                                            sendBlockModEvent(actor, attacker, true);
                                        }
                                    }
                                }
                            }
                        }
                        
                    }
                }
            }
            HitScope scope(std::move(hit));
            originalApply(caster, impactPos, projectile, target, arg5, arg6, arg7, arg8);
        }

        static void SetEffectiveness(RE::ActiveEffect* effect, float power, bool onlyHostile) {
            originalSetEffectiveness(effect, power, onlyHostile);
            if (!currentHit || !effect) {
                return;
            }
            // Must belong to the spell from the blocked projectile.
            if (effect->spell != currentHit->spell) {
                if (settings::Get().log) {
                    SKSE::log::info("[SetEffectiveness] effect spell: {} is not currenthit spell: {}", static_cast<void*>(effect->spell), static_cast<void*>(currentHit->spell));
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

            suppressHitShader(effect);

            // Only damage to Health, Magicka, or Stamina is reduced. Shader
            // suppression above applies to every effect from the blocked spell.
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

            const float oldMagnitude = effect->magnitude;
            effect->magnitude *= currentHit->remainingDamage;
            if (auto* victim = victimRef->As<RE::Actor>()) {
                awardBlockExperience(victim, oldMagnitude);
            }

            if (settings::Get().log) {
                SKSE::log::info("[SetEffectiveness] blocked spell effect={} magnitude {} -> {}", static_cast<void*>(effect), oldMagnitude, effect->magnitude);
            }

        }
        static inline REL::Relocation<decltype(ApplyProjectileSpell)> originalApply;
        static inline REL::Relocation<decltype(SetEffectiveness)> originalSetEffectiveness;
};
