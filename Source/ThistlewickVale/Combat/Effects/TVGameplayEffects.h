// Native gameplay effects referenced by data (Docs/BALANCE.md section 8): /Script/ThistlewickVale.GE_TV_<Name>.
//
// Two kinds of classes live here:
//  - Applied effects: real gameplay effects put on the target (DoTs, buffs, debuffs, set bonuses...).
//  - Parameter effects (Role = Parameter): never applied; executors read their FTVEffectSpec (Duration/Magnitude) as
//    extra ability parameters (GE_TV_ChannelTick, GE_TV_MultiHit, GE_TV_TrapZone, GE_TV_StackLimit, GE_TV_GroundZoneDot).
//
// 5.8 notes (checked in GameplayEffect.h / GameplayEffect.cpp):
//  - Tag-granting GE components cache their tags in PostLoad, which native CDOs never get, and AddComponent() uses
//    NewObject (asserts inside constructors). So these effects carry NO component tags: tags are added at runtime to
//    FGameplayEffectSpec::DynamicGrantedTags (replicated; FGameplayEffectQuery owning-tag matching includes them).
//  - StackingType is deprecated for direct access since 5.7 but is still what UGameplayEffect's own constructor writes;
//    wrapped in PRAGMA_DISABLE_DEPRECATION_WARNINGS. bFactorInStackCount defaults to true.
//  - MultiplyAdditive aggregates as 1 + sum(value - 1), so "+30%" must be sent as 1.3 (MagnitudeMode OnePlus).
//  - Magnitudes and durations are SetByCaller by FName (no tag lookups during CDO construction).
#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "TVGameplayEffects.generated.h"

UENUM(BlueprintType)
enum class ETVEffectRole : uint8
{
	/** Applied to the target as a gameplay effect. */
	Applied,
	/** Only read by the ability executor; never applied. */
	Parameter,
};

/** Extra server-side behaviour run by UTVCombatLibrary::ApplyEffectSpec when an effect of this class is applied. */
UENUM(BlueprintType)
enum class ETVEffectHook : uint8
{
	None,
	/** Adds an absorb shield (Magnitude <= 1: fraction of target max health) to the AbsorbShield base; cleared when the effect ends. */
	AbsorbShield,
	/** Pushes the target away from the source by Magnitude cm. */
	Knockback,
	/** Interrupts the target's current interruptible cast or channel. */
	Interrupt,
	/** Root/stun that breaks on damage once older than Magnitude seconds (0 = never breaks). */
	BreakOnDamage,
	/** Removes Magnitude magic/curse debuffs from the target. */
	Dispel,
	/** Taunt: target mob puts the source at top threat x Magnitude and must attack it for Duration. */
	Taunt,
	/** Pulls friendly mobs within the ability RadiusCm into the caster's fight. */
	SocialAggro,
	/** Spawns Magnitude mobs; the MobId is the last element of the SetByCallerTag (TV.Spawn.<MOB_ID>). */
	SummonAdds,
};

/** How the data Magnitude becomes the SetByCaller value. */
UENUM(BlueprintType)
enum class ETVMagnitudeMode : uint8
{
	Raw,
	/** 1 + Magnitude (MultiplyAdditive percentage modifiers). */
	OnePlus,
	/** -Magnitude (data gives a reduction as a positive number). */
	Negate,
	/** Magnitude x source hit damage per tick (mob DoTs: BALANCE.md section 1). Players: x ability magnitude. */
	TimesHitDamage,
	/** The ability's computed magnitude per tick; data Magnitude is the tick interval (only 1.0 is supported). */
	AbilityMagnitudePerTick,
	/** Magnitude is a total spread over Duration with 1 s ticks. */
	TotalOverDuration,
	/** -(Magnitude <= 1 ? Magnitude x target MaxResource : Magnitude). */
	DrainFractionOfMaxResource,
};

UCLASS(Abstract)
class THISTLEWICKVALE_API UTVGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UTVGameplayEffect(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditDefaultsOnly, Category = "TV") ETVEffectRole Role = ETVEffectRole::Applied;
	/** Tag names granted to the target while active (resolved at apply time, added to DynamicGrantedTags). */
	UPROPERTY(EditDefaultsOnly, Category = "TV") TArray<FName> GrantedTagNames;
	UPROPERTY(EditDefaultsOnly, Category = "TV") ETVEffectHook Hook = ETVEffectHook::None;
	UPROPERTY(EditDefaultsOnly, Category = "TV") ETVMagnitudeMode MagnitudeMode = ETVMagnitudeMode::Raw;
	/** True for HasDuration effects (Duration is SetByCaller). */
	UPROPERTY(EditDefaultsOnly, Category = "TV") bool bUsesDuration = false;
	/** Used when the data leaves Duration at 0. Negative data durations mean "until removed" (one hour). */
	UPROPERTY(EditDefaultsOnly, Category = "TV") float DefaultDuration = 5.f;
	/** Tick period for periodic effects (informational; the native Period is set in the constructor). */
	UPROPERTY(EditDefaultsOnly, Category = "TV") float TickPeriod = 0.f;
	/** Counts as a hostile debuff (dispel/cleanup on evade). */
	UPROPERTY(EditDefaultsOnly, Category = "TV") bool bIsDebuff = false;

protected:
	void AddSetByCallerModifier(const FGameplayAttribute& Attribute, EGameplayModOp::Type Op, FName DataName);
	void MakeDurationEffect(float InDefaultDuration);
	void MakeInfiniteEffect();
	void MakePeriodic(float PeriodSeconds);
	void MakeStacking(int32 Limit);
};

// ---- Internal (code only) -------------------------------------------------------------------------------------------
/** Raw damage into IncomingDamage (SetByCaller Damage). Mitigation runs on the target. */
UCLASS() class THISTLEWICKVALE_API UGE_TV_Damage : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_Damage(const FObjectInitializer& OI); };
/** Raw heal into IncomingHeal (SetByCaller Heal). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_Heal : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_Heal(const FObjectInitializer& OI); };
/** Resource + Magnitude (costs and generation). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_ResourceDelta : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_ResourceDelta(const FObjectInitializer& OI); };
/** Cooldown / GCD marker; tags come from the caller (TV.Cooldown.*). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_Cooldown : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_Cooldown(const FObjectInitializer& OI); };
/** Tag-only marker; the ability's AbilityTag is granted. */
UCLASS() class THISTLEWICKVALE_API UGE_TV_Marker : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_Marker(const FObjectInitializer& OI); };
/** Equipment totals: one modifier per stat, SetByCaller names "Stat.<Name>". */
UCLASS() class THISTLEWICKVALE_API UGE_TV_EquipmentStats : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_EquipmentStats(const FObjectInitializer& OI); };
/** MoveSpeedMultiplier + Magnitude, infinite (mount: 0.6 = SpeedMultiplier 1.6 - 1). For UTVMountComponent via ApplyBonusEffect. */
UCLASS() class THISTLEWICKVALE_API UGE_TV_Bonus_MoveSpeed : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_Bonus_MoveSpeed(const FObjectInitializer& OI); };
/** Interrupts the target's cast (not used by v1 data: no player interrupt ability exists). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_Interrupt : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_Interrupt(const FObjectInitializer& OI); };

// ---- BALANCE.md section 8: applied ---------------------------------------------------------------------------------
/** Taunt: Duration = forced-target time, Magnitude = threat multiplier over the current top (1.1). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_Taunt : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_Taunt(const FObjectInitializer& OI); };
/** DamageTakenMultiplier + Magnitude (Iron Wall -0.4). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_DamageTakenMod : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_DamageTakenMod(const FObjectInitializer& OI); };
/** Same as DamageTakenMod, applied to each party member (Dawn's Vigil -0.5). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_PartyDamageTakenMod : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_PartyDamageTakenMod(const FObjectInitializer& OI); };
/** Frontal hits taken x (1 + Magnitude) (Footman shield block -0.5). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_FrontalDamageTakenMod : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_FrontalDamageTakenMod(const FObjectInitializer& OI); };
/** MaxHealth x (1 + Magnitude) (Bulwark 0.3). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_MaxHealthMod : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_MaxHealthMod(const FObjectInitializer& OI); };
/** ThreatMultiplier x Magnitude (Bulwark 2.0 = threat x2). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_ThreatMod : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_ThreatMod(const FObjectInitializer& OI); };
/** Root; Magnitude = break-on-damage grace in seconds (Frost Snare 1.0; Snare Trap 0 = unbreakable). Magic (dispellable). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_Root : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_Root(const FObjectInitializer& OI); };
/** Stun: blocks abilities and movement (Boar Charge 0.8 s). Magnitude as Root. */
UCLASS() class THISTLEWICKVALE_API UGE_TV_Stun : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_Stun(const FObjectInitializer& OI); };
/** Absorb shield, Magnitude <= 1 = fraction of max health (Glimmer Ward 0.25). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_Absorb : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_Absorb(const FObjectInitializer& OI); };
/** Instant: removes Magnitude magic/curse debuffs (Purify 1). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_Dispel : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_Dispel(const FObjectInitializer& OI); };
/** Heal over time, 1 s ticks of the ability's magnitude (Purify). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_HoT : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_HoT(const FObjectInitializer& OI); };
/** Resource over Duration, Magnitude total (Quick Shot +10 Focus over 3 s). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_FocusGain : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_FocusGain(const FObjectInitializer& OI); };
/** DodgeChance + Magnitude (Fade Step 0.5). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_DodgeMod : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_DodgeMod(const FObjectInitializer& OI); };
/** Builder abilities cost nothing and hit Magnitude times (Stag Volley 2). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_StagVolleyBuff : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_StagVolleyBuff(const FObjectInitializer& OI); };
/** Instant on self: friendly mobs within the ability RadiusCm join the fight (Call for Help). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_SocialAggro : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_SocialAggro(const FObjectInitializer& OI); };
/** Instant: Resource - Magnitude x target MaxResource (Wisp mana siphon 0.08). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_ManaDrain : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_ManaDrain(const FObjectInitializer& OI); };
/** Bleed DoT, 2 s ticks of Magnitude x hit damage (Rend 0.25). Not dispellable. */
UCLASS() class THISTLEWICKVALE_API UGE_TV_DotBleed : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_DotBleed(const FObjectInitializer& OI); };
/** Magic DoT, 2 s ticks of Magnitude x hit damage (Withering Mark 0.6). Dispellable. */
UCLASS() class THISTLEWICKVALE_API UGE_TV_DotMagic : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_DotMagic(const FObjectInitializer& OI); };
/** Instant: push the target Magnitude cm away from the source (Boulder Toss 800). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_Knockback : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_Knockback(const FObjectInitializer& OI); };
/** Stacking armor debuff, max 5 stacks, refreshed on application: ArmorPctModifier + Magnitude per stack (Sundering Blows -0.1). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_ArmorShred : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_ArmorShred(const FObjectInitializer& OI); };
/** Instant on self: spawn Magnitude mobs of TV.Spawn.<MOB_ID> (pillar count capped by party size). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_SummonAdds : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_SummonAdds(const FObjectInitializer& OI); };
/** HealingReceivedMultiplier + Magnitude (Grave Brand -0.3). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_HealingReceivedMod : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_HealingReceivedMod(const FObjectInitializer& OI); };
/** DamageDoneMultiplier + Magnitude until removed (boss enrage 2.0 = +200%). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_Enrage : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_Enrage(const FObjectInitializer& OI); };

// ---- BALANCE.md section 8: parameters (read by executors, never applied) -------------------------------------------
/** GA_TV_Channel: Magnitude = tick interval in seconds (Starfall 0.5). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_ChannelTick : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_ChannelTick(const FObjectInitializer& OI); };
/** GA_TV_DirectDamage: Magnitude = number of hits (Twin Fang 2). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_MultiHit : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_MultiHit(const FObjectInitializer& OI); };
/** GA_TV_GroundAoE trap: Duration = trap lifetime, Magnitude = arm time. The other Effects are applied to the first enemy. */
UCLASS() class THISTLEWICKVALE_API UGE_TV_TrapZone : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_TrapZone(const FObjectInitializer& OI); };
/** With GE_TV_ArmorShred: Magnitude = max stacks. The native limit is 5; other values are logged and clamped. */
UCLASS() class THISTLEWICKVALE_API UGE_TV_StackLimit : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_StackLimit(const FObjectInitializer& OI); };
/** GA_TV_GroundAoE pool: Duration = pool lifetime, Magnitude = hit-damage multiple per 1 s tick (Crownfire 0.8). */
UCLASS() class THISTLEWICKVALE_API UGE_TV_GroundZoneDot : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_GroundZoneDot(const FObjectInitializer& OI); };

// ---- Set bonuses (UTVSetBonusDataAsset, applied by Systems through ApplyBonusEffect) ---------------------------------
UCLASS() class THISTLEWICKVALE_API UGE_TV_Set_MoveSpeedOutOfCombat : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_Set_MoveSpeedOutOfCombat(const FObjectInitializer& OI); };
UCLASS() class THISTLEWICKVALE_API UGE_TV_Set_PrimaryStatPct : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_Set_PrimaryStatPct(const FObjectInitializer& OI); };
UCLASS() class THISTLEWICKVALE_API UGE_TV_Set_MaxHealthPct : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_Set_MaxHealthPct(const FObjectInitializer& OI); };
/** Magnitude 0.15 = defensive cooldowns x 0.85. */
UCLASS() class THISTLEWICKVALE_API UGE_TV_Set_DefensiveCooldownReduction : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_Set_DefensiveCooldownReduction(const FObjectInitializer& OI); };
UCLASS() class THISTLEWICKVALE_API UGE_TV_Set_CritChance : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_Set_CritChance(const FObjectInitializer& OI); };
/** Ultimate damage/heal x (1 + Magnitude); grants TV.Buff.UltimateGlow for the glow VFX. */
UCLASS() class THISTLEWICKVALE_API UGE_TV_Set_UltimatePower : public UTVGameplayEffect { GENERATED_BODY() public: UGE_TV_Set_UltimatePower(const FObjectInitializer& OI); };
