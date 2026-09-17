#pragma once

class hooks {
    //logic adapted from valhalla combat, hooks adapted from arrowInterpreter. 
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

            SKSE::log::info("[hooks] attempting hooking projectile addimpact functions");
        }
    
    private: 
        //gonna do this via velocity check instead, I think. Using dot product to check for projectile heading to calculate if it's
        //within the block angle cone. 
        static bool checkBlockAngle(RE::Actor* actor, RE::Projectile* projectile) {
            auto* gameSettings = RE::GameSettingCollection::GetSingleton();
            auto* gmst = gameSettings ? gameSettings->GetSetting("fCombatHitConeAngle") : nullptr;
            if (gmst) {
                const float fCombatHitConeAngle = gmst->GetFloat();
                const auto& v = projectile->GetProjectileRuntimeData().velocity;
                const float horizontalSpeed = std::hypot(v.x, v.y);
                if (horizontalSpeed < 0.0001f) {
                    SKSE::log::info("[checkBlockAngle]: negligeble horizontal speed: {}", horizontalSpeed);
                    return false;
                }
                
                const float blockerAngle = actor->GetAngleZ();
                const float forwardX = std::sin(blockerAngle);
                const float forwardY = std::cos(blockerAngle);
                const float dotProduct = forwardX * (-v.x / horizontalSpeed) + forwardY * (-v.y / horizontalSpeed);
                SKSE::log::info("[checkBlockAngle] angle: {}/{}", std::acos(std::clamp(dotProduct, -1.0f, 1.0f)) * 180.0f/3.1415927f, fCombatHitConeAngle);
                return dotProduct >= std::cos(fCombatHitConeAngle * 3.1415927f/180.0f);
            }
            return false;
        }


        static void performProjectileBlock(RE::Actor* blocker, RE::Projectile* projectile) {
            if (!blocker || !projectile) {
                return;
            }
            if (!checkBlockAngle(blocker, projectile)) {
                return;
            }
            if (blocker->IsBlocking()) {
                blocker->NotifyAnimationGraph("BlockHitStart");
            }
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

        static inline REL::Relocation<decltype(AddImpactProj)> _originalArrow;
        static inline REL::Relocation<decltype(AddImpactMissile)> _originalMissile;
        static inline REL::Relocation<decltype(AddImpactBeam)> _originalBeam;
};