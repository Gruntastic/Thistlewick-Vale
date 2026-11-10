// Native gameplay effects referenced by DT_Abilities rows (FTVEffectSpec::EffectClass) and by code.
// Path for data: /Script/ThistlewickVale.GE_TV_<Name>.
//
// 5.8 notes (checked in GameplayEffect.h / GameplayEffect.cpp):
//  - Tag-granting via GE components needs PostLoad to cache tags, which native CDOs never get, and AddComponent() uses
//    NewObject (not allowed in constructors). So these effects carry NO component tags: every tag is added at runtime
//    to FGameplayEffectSpec::DynamicGrantedTags (replicated, included by FGameplayEffectQuery owning-tag matching).
//  - StackingType is deprecated-for-direct-access in 5.7+ but still the only way to set stacking natively; wrapped in
//    PRAGMA_DISABLE_DEPRECATION_WARNINGS. bFactorInStackCount defaults to true.
//  - Magnitudes and durations are SetByCaller by FName (no tag lookups at CDO construction).
//
// FTVEffectSpec.Magnitude meaning per class is documented on each class.
#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "TVGameplayEffects.generated.h"

/** Extra server-side behaviour run by UTVCombatLibrary::ApplyEffectSpec when an effect of this class is applied. */
UENUM(BlueprintType)
enum class ETVEffectHook : uint8
{
	None,
	/** Adds Magnitude (<= 1: fraction of target max health) to the AbsorbShield base; cleared when the effect ends. */
	AbsorbShield,
	/** Pushes the target away from the source at Magnitude cm/s. */
	Knockback,
	/** Interrupts the target's current interruptible cast or channel. */
	Interrupt,
	/** Root/stun that breaks on damage once it is older than Magnitude seconds (0 = never breaks). */
	BreakOnDamage,
};

UCLASS(Abstract)
class THISTLEWICKVALE_API UTVGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UTVGameplayEffect(const FObjectInitializer& ObjectInitializer);

	/** Tag names granted to the target while active (resolved at apply time, added to DynamicGrantedTags). */
	UPROPERTY(EditDefaultsOnly, Category = "TV") TArray<FName> GrantedTagNames;
	UPROPERTY(EditDefaultsOnly, Category = "TV") ETVEffectHook Hook = ETVEffectHook::None;
	/** If false, FTVEffectSpec.Duration is ignored (instant and infinite effects). */
	UPROPERTY(EditDefaultsOnly, Category = "TV") bool bUsesDuration = false;
	/** Duration used when the data leaves Duration at 0. */
	UPROPERTY(EditDefaultsOnly, Category = "TV") float DefaultDuration = 5.f;

protected:
	void AddSetByCallerModifier(const FGameplayAttribute& Attribute, EGameplayModOp::Type Op, FName DataName);
	void MakeDurationEffect();
	void MakeInfiniteEffect();
	void MakePeriodic(float PeriodSeconds);
	void MakeStacking(int32 Limit);
};

// ---- Instant, used by code ---------------------------------------------------------------------------------------

/** Raw damage into IncomingDamage (SetByCaller Damage). Mitigation runs on the target. */
UCLASS() class THISTLEWICKVALE_API UGE_TV_Damage : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_Damage(const FObjectInitializer& OI); };
/** Raw heal into IncomingHeal (SetByCaller Heal). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_Heal : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_Heal(const FObjectInitializer& OI); };
/** Resource +Magnitude (negative drains, e.g. Barrow Wisp mana drain -15). Also used for ability costs. */
UCLASS() class THISTLEWICKVALE_API UGE_TV_ResourceDelta : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_ResourceDelta(const FObjectInitializer& OI); };
/** Cooldown / GCD marker. Tags come from the caller (TV.Cooldown.*). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_Cooldown : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_Cooldown(const FObjectInitializer& OI); };
/** Knockback: Magnitude = push speed in cm/s (Boulder Toss). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_Knockback : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_Knockback(const FObjectInitializer& OI); };
/** Interrupts the target's cast. Put this in the Effects of any ability that should interrupt. */
UCLASS() class THISTLEWICKVALE_API UGE_TV_Interrupt : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_Interrupt(const FObjectInitializer& OI); };

// ---- Over time ----------------------------------------------------------------------------------------------------

/** Damage every 1 s, Magnitude per tick. Not dispellable. */
UCLASS() class THISTLEWICKVALE_API UGE_TV_DamageOverTime : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_DamageOverTime(const FObjectInitializer& OI); };
/** Magic DoT (Purify removes it): Withering Mark. */
UCLASS() class THISTLEWICKVALE_API UGE_TV_DamageOverTime_Magic : public UGE_TV_DamageOverTime { GENERATED_BODY() public: UGE_TV_DamageOverTime_Magic(const FObjectInitializer& OI); };
/** Curse DoT (Purify removes it). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_DamageOverTime_Curse : public UGE_TV_DamageOverTime { GENERATED_BODY() public: UGE_TV_DamageOverTime_Curse(const FObjectInitializer& OI); };
/** Bleed DoT (not dispellable): Rend. */
UCLASS() class THISTLEWICKVALE_API UGE_TV_DamageOverTime_Bleed : public UGE_TV_DamageOverTime { GENERATED_BODY() public: UGE_TV_DamageOverTime_Bleed(const FObjectInitializer& OI); };
/** Heal every 1 s, Magnitude per tick (Purify HoT). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_HealOverTime : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_HealOverTime(const FObjectInitializer& OI); };
/** Resource every 1 s, Magnitude per tick (Quick Shot: +10 Focus over 3 s -> Duration 3, Magnitude 3.34). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_ResourceOverTime : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_ResourceOverTime(const FObjectInitializer& OI); };

// ---- Buffs / debuffs (duration; Duration < 0 = effectively permanent) ---------------------------------------------

/** DamageTakenMultiplier += Magnitude (Iron Wall -0.4, Dawn's Vigil -0.5). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_DamageTaken : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_DamageTaken(const FObjectInitializer& OI); };
/** DamageDoneMultiplier += Magnitude (boss enrage +2.0). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_DamageDone : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_DamageDone(const FObjectInitializer& OI); };
/** MaxHealth x (1 + Magnitude) (Bulwark +0.3). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_MaxHealthPct : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_MaxHealthPct(const FObjectInitializer& OI); };
/** ThreatMultiplier x (1 + Magnitude) (Bulwark +1.0 = threat x2). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_Threat : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_Threat(const FObjectInitializer& OI); };
/** Absorb shield; Magnitude <= 1 = fraction of max health (Glimmer Ward 0.25), else flat points. */
UCLASS() class THISTLEWICKVALE_API UGE_TV_AbsorbShield : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_AbsorbShield(const FObjectInitializer& OI); };
/** Share of healing absorbed (Grave Brand 0.3). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_HealAbsorb : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_HealAbsorb(const FObjectInitializer& OI); };
/** DodgeChance += Magnitude (Fade Step 0.5). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_Dodge : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_Dodge(const FObjectInitializer& OI); };
/** Root. Magnitude = seconds after which damage breaks it (Frost Snare 1.0, Snare Trap 0 = unbreakable). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_Root : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_Root(const FObjectInitializer& OI); };
/** Magic root (dispellable): Frost Snare. */
UCLASS() class THISTLEWICKVALE_API UGE_TV_Root_Magic : public UGE_TV_Root { GENERATED_BODY() public: UGE_TV_Root_Magic(const FObjectInitializer& OI); };
/** Stun (blocks abilities and movement). Magnitude as Root. */
UCLASS() class THISTLEWICKVALE_API UGE_TV_Stun : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_Stun(const FObjectInitializer& OI); };
/** MoveSpeedMultiplier += Magnitude (snare -0.5). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_MoveSpeed : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_MoveSpeed(const FObjectInitializer& OI); };
/** Stacking armor debuff, max 5 stacks, refreshes on application: Armor x (1 + Magnitude x stacks) (Sundering Blows -0.1, 10 s). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_ArmorPctStacking : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_ArmorPctStacking(const FObjectInitializer& OI); };
/** Builder-slot abilities of the holder cost nothing and fire twice (Stag Volley). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_EmpowerBuilder : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_EmpowerBuilder(const FObjectInitializer& OI); };
/** Frontal hits taken x (1 - 0.5) (Hollowed Footman shield block). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_FrontalBlock : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_FrontalBlock(const FObjectInitializer& OI); };
/** Tag-only marker; the ability's AbilityTag is the granted tag. */
UCLASS() class THISTLEWICKVALE_API UGE_TV_Marker : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_Marker(const FObjectInitializer& OI); };

// ---- Infinite, applied through UTVAbilitySystemComponent (equipment, set bonuses, mount) ---------------------------

/** One modifier per stat, SetByCaller names "Stat.<Name>". Replaced as a whole by SetEquipmentStats. */
UCLASS() class THISTLEWICKVALE_API UGE_TV_EquipmentStats : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_EquipmentStats(const FObjectInitializer& OI); };
/** MoveSpeedMultiplier + Magnitude (mount 0.6). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_Bonus_MoveSpeed : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_Bonus_MoveSpeed(const FObjectInitializer& OI); };
/** Out-of-combat move speed + Magnitude (T1 2-piece 0.05). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_Bonus_MoveSpeedOutOfCombat : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_Bonus_MoveSpeedOutOfCombat(const FObjectInitializer& OI); };
/** All primary stats x (1 + Magnitude) (T1 4-piece 0.03). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_Bonus_PrimaryStatsPct : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_Bonus_PrimaryStatsPct(const FObjectInitializer& OI); };
/** MaxHealth x (1 + Magnitude) (T2 2-piece 0.05). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_Bonus_MaxHealthPct : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_Bonus_MaxHealthPct(const FObjectInitializer& OI); };
/** Defensive cooldowns x (1 + Magnitude) (T2 4-piece -0.15). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_Bonus_DefensiveCooldown : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_Bonus_DefensiveCooldown(const FObjectInitializer& OI); };
/** Crit chance + Magnitude (T3 2-piece 0.05). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_Bonus_CritChance : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_Bonus_CritChance(const FObjectInitializer& OI); };
/** Ultimate damage/heal x (1 + Magnitude) (T3 4-piece 0.2). Also grants TV.Buff.UltimateGlow for the VFX. */
UCLASS() class THISTLEWICKVALE_API UGE_TV_Bonus_UltimatePower : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_Bonus_UltimatePower(const FObjectInitializer& OI); };
