// Gameplay tags used by combat code. Every tag here is declared in Config/Tags/TVGameplayTags.ini (root "TV.").
// Tags are resolved lazily (first call) so nothing touches the tag manager during CDO construction.
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

namespace TVTags
{
	/** Resolve a tag by name; logs once and returns an empty tag if it is missing from the ini. */
	THISTLEWICKVALE_API FGameplayTag Get(const TCHAR* TagName);

	/** TV.Ability.Warrior.ShieldSlam -> TV.Cooldown.Ability.Warrior.ShieldSlam (empty tag if not declared). */
	THISTLEWICKVALE_API FGameplayTag CooldownTagFor(const FGameplayTag& AbilityTag);

	// States
	THISTLEWICKVALE_API FGameplayTag State_Dead();
	THISTLEWICKVALE_API FGameplayTag State_Mounted();
	THISTLEWICKVALE_API FGameplayTag State_Casting();
	THISTLEWICKVALE_API FGameplayTag State_Channeling();
	THISTLEWICKVALE_API FGameplayTag State_Stunned();
	THISTLEWICKVALE_API FGameplayTag State_InCombat();
	THISTLEWICKVALE_API FGameplayTag State_Evading();

	// Cooldowns
	THISTLEWICKVALE_API FGameplayTag Cooldown_GCD();

	// Debuffs / buffs used by code paths
	THISTLEWICKVALE_API FGameplayTag Debuff();
	THISTLEWICKVALE_API FGameplayTag Debuff_Magic();
	THISTLEWICKVALE_API FGameplayTag Debuff_Curse();
	THISTLEWICKVALE_API FGameplayTag Debuff_Root();
	THISTLEWICKVALE_API FGameplayTag Debuff_Stun();
	THISTLEWICKVALE_API FGameplayTag Buff_Absorb();
	THISTLEWICKVALE_API FGameplayTag Buff_FrontalBlock();
	THISTLEWICKVALE_API FGameplayTag Buff_EmpowerBuilder();

	// SetByCaller
	THISTLEWICKVALE_API FGameplayTag SBC_Damage();
	THISTLEWICKVALE_API FGameplayTag SBC_Heal();
	THISTLEWICKVALE_API FGameplayTag SBC_Duration();
	THISTLEWICKVALE_API FGameplayTag SBC_Magnitude();
	THISTLEWICKVALE_API FGameplayTag SBC_Threat();
	THISTLEWICKVALE_API FGameplayTag SBC_BreakOnDamageAfter();

	// Damage flags (dynamic asset tags on damage/heal specs)
	THISTLEWICKVALE_API FGameplayTag Hit_Crit();
	THISTLEWICKVALE_API FGameplayTag Hit_Periodic();

	// Combo resources
	THISTLEWICKVALE_API FGameplayTag Ability_Mount_Summon();
}

/** SetByCaller data names (FName) used by native gameplay effects; names avoid tag lookups at CDO construction time. */
namespace TVSetByCaller
{
	static const FName Damage(TEXT("Damage"));
	static const FName Heal(TEXT("Heal"));
	static const FName Duration(TEXT("Duration"));
	static const FName Magnitude(TEXT("Magnitude"));
	static const FName Threat(TEXT("Threat"));
	static const FName BreakOnDamageAfter(TEXT("BreakOnDamageAfter"));
}
