#include "PCH.h"
#include "hooks.h"
#include "settings.h"

#include <atomic>
#include <charconv>
#include <cctype>

#include "extern/PerkEntryPointExtenderAPI.h"
#include "extern/STBL_API.h"


static std::string_view trim(std::string_view value) {
    const auto isSpace = [](char ch) { return std::isspace(static_cast<unsigned char>(ch)) != 0; };
    while (!value.empty() && isSpace(value.front())) {
        value.remove_prefix(1);
    }
    while (!value.empty() && isSpace(value.back())) {
        value.remove_suffix(1);
    }
    return value;
}

hooks::PerkRequirement hooks::loadPerkRequirement(std::string_view setting, std::string_view context) {
    setting = trim(setting);
    if (setting.empty()) {
        SKSE::log::info("[perk requirement] {} blocking is unrestricted", context);
        return {};
    }

    PerkRequirement requirement{};
    const auto separator = setting.rfind('-');
    if (separator == std::string_view::npos) {
        SKSE::log::error("[perk requirement] Invalid {} requirement '{}'; expected <plugin>-0x<FormID>; no perk will be required", context, setting);
        return requirement;
    }

    const auto plugin = trim(setting.substr(0, separator));
    auto formIDText = trim(setting.substr(separator + 1));
    if (plugin.empty() || formIDText.empty()) {
        SKSE::log::error("[perk requirement] Invalid {} requirement '{}'; expected <plugin>-0x<FormID>; no perk will be required", context, setting);
        return requirement;
    }
    if (formIDText.starts_with("0x") || formIDText.starts_with("0X")) {
        formIDText.remove_prefix(2);
    }

    RE::FormID localFormID = 0;
    const auto [end, error] = std::from_chars(formIDText.data(), formIDText.data() + formIDText.size(), localFormID, 16);
    if (formIDText.empty() || error != std::errc{} || end != formIDText.data() + formIDText.size() || localFormID > 0x00FFFFFF) {
        SKSE::log::error("[perk requirement] Invalid {} FormID in '{}'; use a plugin-local hexadecimal FormID; no perk will be required", context, setting);
        return requirement;
    }

    auto* dataHandler = RE::TESDataHandler::GetSingleton();
    requirement.perk = dataHandler ? dataHandler->LookupForm<RE::BGSPerk>(localFormID, plugin) : nullptr;
    if (!requirement.perk) {
        SKSE::log::error("[perk requirement] Could not resolve {} perk '{}'; no perk will be required", context, setting);
        return requirement;
    }

    requirement.configured = true;
    SKSE::log::info("[perk requirement] Loaded {} perk '{}' as {:08X}", context, setting, requirement.perk->GetFormID());
    return requirement;
}

hooks::BlockProfile hooks::getBlockProfile(RE::Actor* actor, bool spell, const settings::config& cfg) {
    const bool shield = actor->GetWornArmor(RE::BGSBipedObjectForm::BipedObjectSlot::kShield) != nullptr;
    const bool player = actor == RE::PlayerCharacter::GetSingleton();
    if (player) {
        if (spell) {
            return shield ? BlockProfile{cfg.playerShieldMagicEnabled, cfg.pcShieldMagicFactor, true, &playerShieldSpellPerk}
                            : BlockProfile{cfg.playerWeaponMagicEnabled, cfg.pcWeaponMagicFactor, false, &playerWeaponSpellPerk};
        }
        return shield ? BlockProfile{cfg.playerShieldArrowEnabled, cfg.pcShieldArrowFactor, true, &playerShieldArrowPerk}
                        : BlockProfile{cfg.playerWeaponArrowEnabled, cfg.pcWeaponArrowFactor, false, &playerWeaponArrowPerk};
    }
    if (spell) {
        return shield ? BlockProfile{cfg.NPCShieldMagicEnabled, cfg.NPCShieldMagicFactor, true, nullptr}
                        : BlockProfile{cfg.NPCWeaponMagicEnabled, cfg.NPCWeaponMagicFactor, false, nullptr};
    }
    return shield ? BlockProfile{cfg.NPCShieldArrowEnabled, cfg.NPCShieldArrowFactor, true, nullptr}
                    : BlockProfile{cfg.NPCWeaponArrowEnabled, cfg.NPCWeaponArrowFactor, false, nullptr};
}

bool hooks::hasRequiredPerk(RE::Actor* actor, const hooks::BlockProfile& profile) {
    if (actor != RE::PlayerCharacter::GetSingleton() || !profile.perkRequirement || !profile.perkRequirement->configured) {
        return true;
    }
    return profile.perkRequirement->perk && actor->HasPerk(profile.perkRequirement->perk);
}

hooks::BlockMode hooks::getBlockMode(RE::Actor* actor, const hooks::BlockProfile& profile, bool damageReductionEnabled) {
    if (!profile.enabled) {
        return hooks::BlockMode::kDisabled;
    }

    const bool requirementMet = hooks::hasRequiredPerk(actor, profile);
    if (damageReductionEnabled) {
        return requirementMet ? hooks::BlockMode::kDamageReduction : hooks::BlockMode::kAnimationOnly;
    }
    return requirementMet ? hooks::BlockMode::kAnimationOnly : hooks::BlockMode::kDisabled;
}

bool hooks::spellDamageReductionEnabled(RE::Actor* actor, const hooks::BlockProfile& profile, const settings::config& cfg) {
    const bool player = actor == RE::PlayerCharacter::GetSingleton();
    if (player) {
        return profile.shield ? cfg.playerShieldSpellDamageReductionEnabled : cfg.playerWeaponSpellDamageReductionEnabled;
    }
    return profile.shield ? cfg.NPCShieldSpellDamageReductionEnabled : cfg.NPCWeaponSpellDamageReductionEnabled;
}

bool hooks::arrowDamageReductionEnabled(RE::Actor* actor, const hooks::BlockProfile& profile, const settings::config& cfg) {
    const bool player = actor == RE::PlayerCharacter::GetSingleton();
    if (player) {
        return profile.shield ? cfg.playerShieldArrowDamageReductionEnabled : cfg.playerWeaponArrowDamageReductionEnabled;
    }
    return profile.shield ? cfg.NPCShieldArrowDamageReductionEnabled : cfg.NPCWeaponArrowDamageReductionEnabled;
}

float hooks::projectilePerkMultiplier(RE::Actor* actor, std::string_view category) {
    float multiplier = 1.0f;
    const auto result = RE::HandleEntryPoint(RE::PerkEntryPoint::kModTelekinesisDistance, actor, multiplier, category);
    if (settings::Get().log) {
        SKSE::log::info("[PEPE] category={} result={} value={}",category,static_cast<int>(result),multiplier);
    }
    return (std::max)(0.0f, multiplier);
}

float hooks::blockSetting(const char* name, float fallback) {
    auto* collection = RE::GameSettingCollection::GetSingleton();
    auto* setting = collection ? collection->GetSetting(name) : nullptr;
    return setting && setting->GetType() == RE::Setting::Type::kFloat ? setting->GetFloat() : fallback;
}

//calculates how much to block
float hooks::blockedFraction(RE::Actor* actor, const hooks::BlockProfile& profile, bool isSpell) {
    const settings::config& cfg = settings::Get();

    // The GMST supplies the starting block value. not factoring in attacker base weapon damage or the spell damage incoming.
    float blockBase = profile.shield ? hooks::blockSetting("fShieldBaseFactor", 0.45f) : hooks::blockSetting("fBlockWeaponBase", 0.30f);
    const float blockSkill =(std::max)(0.0f, actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kBlock));
    // fBlockSkillBase and fBlockSkillMult are unused as far as I can tell
    // const float skillFactor = blockSetting("fBlockSkillBase", 1.0f) + (blockSkill/100.0f) * blockSetting("fBlockSkillMult", 1.5f);
    // SKSE::log::info("[blockedFraction]: fBlockSkillBase={} fBlockSkillMult={}", blockSetting("fBlockSkillBase", 1.0f), blockSetting("fBlockSkillMult", 1.5f));
    const float skillPercentPerLevel = std::clamp(cfg.blockSkillPercentPerLevel, 0.0f, 10.0f);
    const float skillFactor = 1.0f + blockSkill * (skillPercentPerLevel / 100.0f);
    //fortify block 10% translates to 10 av with in game inspection, so 1+(av/100) - value defaults to 0
    const float blockMod = 1+(actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kBlockModifier))/100;
    float block = blockBase * skillFactor * blockMod;
    RE::BGSEntryPoint::HandleEntryPoint(RE::BGSEntryPoint::ENTRY_POINT::kModPercentBlocked, actor, &block);
    const float cap = std::clamp(hooks::blockSetting("fBlockMax", 0.85f), 0.0f, 1.0f);
    const float PEPEMultiplier = projectilePerkMultiplier(actor, isSpell ? "APB_kModSpellBlock" : "APB_kModArrowBlock");
    
    if (cfg.log) {
        SKSE::log::info("[blockedFraction]: blockBase={} blockskill={} skillPercentPerLevel={} skillFactor={} blockMod={} block={} * PEPEMult={} ",
            blockBase, blockSkill, skillPercentPerLevel, skillFactor, blockMod, block, PEPEMultiplier);
    }
    // return std::clamp(block, 0.0f, cap) * std::clamp(profile.factor, 0.0f, 1.0f);
    return std::clamp(block*PEPEMultiplier*profile.factor, 0.0f, cap);
}

hooks::SpellCosts hooks::spellCosts(RE::Actor* actor, const hooks::BlockProfile& profile, const settings::config& cfg) {
    const bool player = actor == RE::PlayerCharacter::GetSingleton();
    if (player) {
        return profile.shield
            ? SpellCosts{cfg.pcShieldSpellStaminaCost, cfg.pcShieldSpellMagickaCost, cfg.pcShieldFlameCostMultiplier}
            : SpellCosts{cfg.pcWeaponSpellStaminaCost, cfg.pcWeaponSpellMagickaCost, cfg.pcWeaponFlameCostMultiplier};
    }
    return profile.shield
        ? SpellCosts{cfg.NPCShieldSpellStaminaCost, cfg.NPCShieldSpellMagickaCost, cfg.NPCShieldFlameCostMultiplier}
        : SpellCosts{cfg.NPCWeaponSpellStaminaCost, cfg.NPCWeaponSpellMagickaCost, cfg.NPCWeaponFlameCostMultiplier};
}

float hooks::PEPE_costMult(RE::Actor* actor, std::string_view category) {
    float multiplier = 1.0f;
    const auto result = RE::HandleEntryPoint(RE::PerkEntryPoint::kModTelekinesisDistance, actor, multiplier, category);
    if (settings::Get().log) {
        SKSE::log::info("[PEPE] CostMult: category={} result={} value={}",category,static_cast<int>(result),multiplier);
    }
    return (std::max)(0.0f, multiplier);
}

bool hooks::tryConsumeSpellBlockResources(RE::Actor* blocker, float staminaCost, float magickaCost) {
    if (!std::isfinite(staminaCost) || !std::isfinite(magickaCost) ||
        staminaCost < 0.0f || magickaCost < 0.0f) {
        return false;
    }
    auto* values = blocker->AsActorValueOwner();
    if (!values) {
        return false;
    }
    const float PEPEfactor = PEPE_costMult(blocker,"APB_SpellBlockCost");
    
    staminaCost *= PEPEfactor;
    magickaCost *= PEPEfactor;

    const float stamina = values->GetActorValue(RE::ActorValue::kStamina);
    const float magicka = values->GetActorValue(RE::ActorValue::kMagicka);
    if ((staminaCost > 0.0f && (!std::isfinite(stamina) || stamina < staminaCost)) ||
        (magickaCost > 0.0f && (!std::isfinite(magicka) || magicka < magickaCost))) {
        return false;
    }
    
    if (staminaCost > 0.0f) {
        values->DamageActorValue(RE::ActorValue::kStamina, staminaCost);
    }
    if (magickaCost > 0.0f) {
        values->DamageActorValue(RE::ActorValue::kMagicka, magickaCost);
    }
    return true;
}

//cooldown to not excessively make actors play blockhit animations
bool hooks::applyCD(RE::Actor* actor) {
    if (!actor || !hooks::cooldownSpell || !hooks::cooldownEffect) {
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

void hooks::castContextSpell(RE::Actor* a_caster, RE::Actor* a_target, RE::SpellItem* a_spell) {
    if (!a_caster || !a_target || !a_spell) {
        return;
    }
    if (auto* caster = a_caster->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant)) {
        caster->CastSpellImmediate(a_spell, false, a_target, 1.0f, false, 0.0f, a_caster);
        if (const auto cfg = settings::Get().log) { 
            SKSE::log::info("[castContextSpell]: Cast spell={} on target={} caster={}", 
                static_cast<void*>(a_spell), static_cast<void*>(a_target), static_cast<void*>(a_caster));
        }
    }
}

void hooks::awardBlockExperience(RE::Actor* blocker, float incomingDamage) {
    if (blocker != RE::PlayerCharacter::GetSingleton() || !std::isfinite(incomingDamage) || incomingDamage <= 0.0f) {
        return;
    }
    const auto* blockSkill = RE::ActorValueList::GetActorValueInfo(RE::ActorValue::kBlock);
    if (!blockSkill || !blockSkill->skill || !std::isfinite(blockSkill->skill->useMult) || blockSkill->skill->useMult <= 0.0f) {
        return;
    }
    const auto cfg = settings::Get();
    const float skillUse = incomingDamage * cfg.pcProjectileBlockExpMult;
    if (!std::isfinite(skillUse) || skillUse <= 0.0f) {
        return;
    }

    const auto* tasks = SKSE::GetTaskInterface();
    if (!tasks) {
        SKSE::log::error("[awardBlockExperience] SKSE task interface unavailable");
        return;
    }
    if (cfg.log) {
        SKSE::log::info("[awardBlockExperience] queuing Block skill use: {}", skillUse);
    }
    //task interface is probably not needed
    tasks->AddTask([skillUse]() {
        if (auto* player = RE::PlayerCharacter::GetSingleton()) {
            // player->AddSkillExperience(RE::ActorValue::kBlock, skillUse); ctds??
            // NG's Actor::UseSkill declaration omits the fourth SKILL_ACTION argument? checking the charmedbaryon fork it says to use:
            using UseSkillFn = void(RE::Actor*, RE::ActorValue, float, RE::TESForm*, RE::SKILL_ACTION);
            REL::RelocateVirtual<UseSkillFn>(0x0F7, 0x0F9, static_cast<RE::Actor*>(player), RE::ActorValue::kBlock, skillUse, nullptr, RE::SKILL_ACTION::kNormalUse);
        }
    });
}

void hooks::sendBlockModEvent(RE::Actor* blocker, RE::Actor* attacker, bool isSpell) {
    if (!blocker || !attacker) {
        return;
    }
    auto* source = SKSE::GetModCallbackEventSource();
    if (!source) {
        return;
    }
    const SKSE::ModCallbackEvent event{
        .eventName = RE::BSFixedString("APB_OnProjectileBlocked"),
        .strArg = RE::BSFixedString(std::to_string(attacker->GetFormID())),
        .numArg = isSpell ? 1.0f : 0.0f,
        .sender = blocker
    };

    source->SendEvent(&event);
}

bool hooks::playSpellBlockAnimation(RE::Actor* actor, bool flame) {
    if (!flame || hooks::applyCD(actor)) {
        actor->NotifyAnimationGraph("BlockHitStart");
        return true;
    }
    return false;
}

//gonna do this via velocity check instead, I think. Using dot product to check for projectile heading compared to actor heading and blockangle
bool hooks::checkBlockAngle(RE::Actor* actor, RE::Projectile* projectile) {
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

bool hooks::tryConsumeArrowBlockStamina(RE::Actor* blocker, float incomingDamage, const settings::config& cfg, float& cost) {
    if (!std::isfinite(incomingDamage) || incomingDamage <= 0.0f) {
        return false;
    }
    const bool player = blocker == RE::PlayerCharacter::GetSingleton();
    const float factor = player ? cfg.pcArrowBlockCostFactor : cfg.NPCArrowBlockCostFactor;
    cost = (blockSetting("fStaminaBlockBase", 0.0f) + incomingDamage * blockSetting("fStaminaBlockDmgMult", 0.25f)) * std::clamp(factor, 0.0f, 5.0f);
    if (!std::isfinite(cost)) {
        return false;
    }
    cost = (std::max)(0.0f, cost) * PEPE_costMult(blocker,"APB_ArrowBlockCost");;
    auto* values = blocker->AsActorValueOwner();
    if (!values || values->GetActorValue(RE::ActorValue::kStamina) < cost) {
        return false;
    }
    if (cost > 0.0f) {
        values->DamageActorValue(RE::ActorValue::kStamina, cost);
    }
    return true;
}