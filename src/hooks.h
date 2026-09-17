#pragma once

namespace hooks {
    //adapted from valhalla combat
    static void OnArrowCollision(RE::Projectile* a_this, RE::hkpAllCdPointCollector* a_AllCdPointCollector);
    static void OnMissileCollision(RE::Projectile* a_this, RE::hkpAllCdPointCollector* a_AllCdPointCollector);
    static inline REL::Relocation<decltype(OnArrowCollision)> _arrowCollision;
    static inline REL::Relocation<decltype(OnMissileCollision)> _missileCollision;

    static inline void install() {
        SKSE::log::info("[hooks] attempting hooking projectile collision functions");
        REL::Relocation<std::uintptr_t> arrowProjectileVtbl{ RE::VTABLE_ArrowProjectile[0] };
        REL::Relocation<std::uintptr_t> missileProjectileVtbl{ RE::VTABLE_MissileProjectile[0] };

        _arrowCollision = arrowProjectileVtbl.write_vfunc(190, OnArrowCollision);
        _missileCollision = missileProjectileVtbl.write_vfunc(190, OnMissileCollision);
        SKSE::log::info("[hooks] attempting hooking projectile collision functions");
    
    }

    bool checkBlockAngle(RE::Actor* actor, RE::TESObjectREFR* a_obj) {
        auto angle = actor->GetHeadingAngle(a_obj->GetAngle(), true);
        auto* gameSettings = RE::GameSettingCollection::GetSingleton();
        auto* gmst = gameSettings ? gameSettings->GetSetting("fCombatHitConeAngle") : nullptr;
        const float fCombatHitConeAngle = gmst->GetFloat();
        SKSE::log::info("[hooks] angle: {}", angle);
	    return (angle <= fCombatHitConeAngle);
    }

    void performProjectileBlock(RE::Actor* blocker, RE::TESObjectREFR* a_obj) {
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

    void processProjectileCollision(RE::Projectile* a_projectile, RE::hkpAllCdPointCollector* a_AllCdPointCol) {
        if (a_AllCdPointCol) {
            for (auto& hit : a_AllCdPointCol->hits) {
                auto refA = RE::TESHavokUtilities::FindCollidableRef(*hit.rootCollidableA);
                auto refB = RE::TESHavokUtilities::FindCollidableRef(*hit.rootCollidableB);
                if (refA && refA->formType == RE::FormType::ActorCharacter) {
                    performProjectileBlock(refA->As<RE::Actor>(), a_projectile);
                }
                if (refB && refB->formType == RE::FormType::ActorCharacter) {
                    performProjectileBlock(refB->As<RE::Actor>(), a_projectile);
                }
            }
        }
    }

    void arrowCollisionhook(RE::Projectile* a_this, RE::hkpAllCdPointCollector* a_AllCdPointCollector) {
        processProjectileCollision(a_this, a_AllCdPointCollector);
        _arrowCollision(a_this, a_AllCdPointCollector);
    }

    void missileCollisionHook(RE::Projectile* a_this, RE::hkpAllCdPointCollector* a_AllCdPointCollector) {
        processProjectileCollision(a_this, a_AllCdPointCollector);
        _missileCollision(a_this, a_AllCdPointCollector);
    }
}