#pragma once

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

            {
                REL::Relocation<std::uintptr_t> vtable{ RE::VTABLE_MissileProjectile[0]};
                _originalMissile = vtable.write_vfunc(0xBD, AddImpactMissile);
            }

            {
                REL::Relocation<std::uintptr_t> vtable{ RE::VTABLE_BeamProjectile[0]};
                _originalBeam = vtable.write_vfunc(0xBD, AddImpactBeam);
            }

            {
                REL::Relocation<std::uintptr_t> vtable{ RE::VTABLE_FlameProjectile[0]};
                _originalFlame = vtable.write_vfunc(0xBD, AddImpactFlame);
            }

            SKSE::log::info("[hooks] attempting hooking projectile addimpact functions");
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
                    SKSE::log::info("[checkBlockAngle]: negligeble horizontal speed: {}", horizontalSpeed);
                    return false;
                }
                
                const float blockerAngle = actor->GetAngleZ();
                const float dotProduct = std::sin(blockerAngle) * (-projX / horizontalSpeed) + std::cos(blockerAngle) * (-projY / horizontalSpeed);
                SKSE::log::info("[checkBlockAngle] angle: {}/{}", std::acos(std::clamp(dotProduct, -1.0f, 1.0f)) * 180.0f/3.1415927f, fCombatHitConeAngle);
                return dotProduct >= std::cos(fCombatHitConeAngle * 3.1415927f/180.0f);
            }
            return false;
        }

        static void performProjectileBlock(RE::Actor* blocker, RE::Projectile* projectile) {
            if (!blocker || !projectile) {
                return;
            }

            if (!checkBlockAngle(blocker, projectile) || !blocker->IsBlocking()) {
                return;
            }

            if (projectile->formType == RE::FormType::ProjectileFlame) {
                if (!applyCD(blocker)) {
                    // SKSE::log::info("[performProjectileBlock]: flame projectile blocker has CD effect");
                    return; 
                }
            }
            blocker->NotifyAnimationGraph("BlockHitStart");
            
        }

        static void processProjectileCollision(RE::Projectile* a_projectile, RE::TESObjectREFR* a_ref) { 
            // SKSE::log::info("[processProjCollision]");
            if (!a_projectile || !a_ref) {
                return;
            }
            if (a_ref->formType == RE::FormType::ActorCharacter) {
                performProjectileBlock(a_ref->As<RE::Actor>(), a_projectile);
            }
        }

        //need to hook specific vtable funcs, hooking the base vfunc doesnt work.
        static RE::Projectile::ImpactData* AddImpactProj(RE::ArrowProjectile* a_projectile, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc, const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6, std::uint32_t a_arg7) {
            // SKSE::log::info("[AddImpactProj]");
            processProjectileCollision(a_projectile, a_ref);
            return _originalArrow(a_projectile, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
        }

        static RE::Projectile::ImpactData* AddImpactMissile(RE::MissileProjectile* a_projectile, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc, const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6, std::uint32_t a_arg7) {
            // SKSE::log::info("[AddImpactMissile]");
            processProjectileCollision(a_projectile, a_ref);
            return _originalMissile(a_projectile, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
        }
        
        static RE::Projectile::ImpactData* AddImpactBeam(RE::BeamProjectile* a_projectile, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc, const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6, std::uint32_t a_arg7) {
            // SKSE::log::info("[AddImpactBeam]");
            processProjectileCollision(a_projectile, a_ref);
            return _originalBeam(a_projectile, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
        }

        static RE::Projectile::ImpactData* AddImpactFlame(RE::FlameProjectile* a_projectile, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc, const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6, std::uint32_t a_arg7) {
            // SKSE::log::info("[AddImpactFlame]");
            processProjectileCollision(a_projectile, a_ref);
            return _originalFlame(a_projectile, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
        }

        static inline REL::Relocation<decltype(AddImpactProj)> _originalArrow;
        static inline REL::Relocation<decltype(AddImpactMissile)> _originalMissile;
        static inline REL::Relocation<decltype(AddImpactBeam)> _originalBeam;
        static inline REL::Relocation<decltype(AddImpactFlame)> _originalFlame;

        static inline RE::SpellItem* cooldownSpell = nullptr;       // 0x800
        static inline RE::EffectSetting* cooldownEffect = nullptr;  // 0x801
};  