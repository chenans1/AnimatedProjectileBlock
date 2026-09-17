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
        static bool checkBlockAngle(RE::Actor* actor, RE::TESObjectREFR* a_obj) {
            auto angle = actor->GetHeadingAngle(a_obj->GetAngle(), true);
            auto* gameSettings = RE::GameSettingCollection::GetSingleton();
            auto* gmst = gameSettings ? gameSettings->GetSetting("fCombatHitConeAngle") : nullptr;
            if (gmst) {
                const float fCombatHitConeAngle = gmst->GetFloat();
                SKSE::log::info("[hooks] angle: {}/{}", angle, fCombatHitConeAngle);
                return (angle <= fCombatHitConeAngle);
            }
            return false;
        }

        static void performProjectileBlock(RE::Actor* blocker, RE::TESObjectREFR* a_obj) {
            if (!blocker || !a_obj) {
                return;
            }
            if (!checkBlockAngle(blocker, a_obj)) {
                return;
            }
            if (blocker->IsBlocking()) {
                blocker->NotifyAnimationGraph("BlockHitStart");
            }
        }

        static void processProjectileCollision(RE::ArrowProjectile* a_projectile, RE::TESObjectREFR* a_ref) { 
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

        static RE::Projectile::ImpactData* AddImpactMissile(RE::ArrowProjectile* a_projectile, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc, const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6, std::uint32_t a_arg7) {
            // SKSE::log::info("[AddImpactMissile]");
            processProjectileCollision(a_projectile, a_ref);
            return _originalMissile(a_projectile, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
        }
        
        static RE::Projectile::ImpactData* AddImpactBeam(RE::ArrowProjectile* a_projectile, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_targetLoc, const RE::NiPoint3& a_velocity, RE::hkpCollidable* a_collidable, std::int32_t a_arg6, std::uint32_t a_arg7) {
            // SKSE::log::info("[AddImpactBeam]");
            processProjectileCollision(a_projectile, a_ref);
            return _originalBeam(a_projectile, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
        }

        static inline REL::Relocation<decltype(AddImpactProj)> _originalArrow;
        static inline REL::Relocation<decltype(AddImpactMissile)> _originalMissile;
        static inline REL::Relocation<decltype(AddImpactBeam)> _originalBeam;
};