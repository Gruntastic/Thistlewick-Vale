// Combat-owned data rows (Docs/DATA_SCHEMAS.md sections 2, 3 and 5). Field names match the schema exactly,
// because Data/Source/DT_<Key>.json is imported with these structs.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "GameplayAbilitySpecHandle.h"
#include "Core/TVTypes.h"
#include "TVCombatRows.generated.h"

class UGameplayEffect;
class UTVGameplayAbility;

/** One gameplay effect an ability applies. Magnitude meaning depends on the effect class (see Combat/Effects/TVGameplayEffects.h). */
USTRUCT(BlueprintType)
struct FTVEffectSpec
{
	GENERATED_BODY()

	/** e.g. /Script/ThistlewickVale.GE_TV_DamageOverTime_Magic */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Ability") TSoftClassPtr<UGameplayEffect> EffectClass;
	/** Seconds. 0 = use the effect class default (instant effects ignore it), negative = infinite. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Ability") float Duration = 0.f;
	/** Per-tick amount for over-time effects, fraction for multipliers (-0.4 = -40%), fraction of max health for shields <= 1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Ability") float Magnitude = 0.f;
	/** Optional extra SetByCaller tag that also receives Magnitude (for designer-made Blueprint effects). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Ability") FGameplayTag SetByCallerTag;
};

USTRUCT(BlueprintType)
struct FTVAbilityUnlock
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Class") FName AbilityId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Class") int32 UnlockLevel = 1;
	/** Action bar slot 0-9, -1 = none. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Class") int32 DefaultSlot = -1;
};

/** DT_Classes. Row name = ClassId. */
USTRUCT(BlueprintType)
struct FTVClassRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Class") FName ClassId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Class") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Class") FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Class") ETVRole Role = ETVRole::DPS;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Class") ETVResource ResourceType = ETVResource::Mana;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Class") FName BodyMeshId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Class") FName AnimBlueprintId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Class") FName PortraitIconId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Class") FLinearColor ClassTintPrimary = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Class") FLinearColor ClassTintSecondary = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Class") ETVArmorFamily ArmorFamily = ETVArmorFamily::None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Class") ETVWeaponType WeaponType = ETVWeaponType::None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Class") TArray<FTVAbilityUnlock> StartingAbilities;
	/** Matched against FTVClassLevelStatsRow::ClassId (falls back to ClassId when None). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Class") FName BaseStatsCurveId;
};

/** DT_ClassBaseStats. One row per class and level; row name is free (e.g. CLS_WARRIOR_01), lookup is by ClassId + Level. */
USTRUCT(BlueprintType)
struct FTVClassLevelStatsRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Class") FName ClassId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Class") int32 Level = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Class") float Stamina = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Class") float Strength = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Class") float Agility = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Class") float Intellect = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Class") float Spirit = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Class") float BaseHealth = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Class") float BaseResourceMax = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Class") float BaseResourceRegen = 0.f;
};

/** DT_XPCurve. Lookup is by Level. */
USTRUCT(BlueprintType)
struct FTVXPCurveRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Class") int32 Level = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Class") int32 XPToNext = 100;
};

/** DT_Abilities. Row name = AbilityId. Player and mob abilities share this table (mobs use ClassId MOB). */
USTRUCT(BlueprintType)
struct FTVAbilityRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Ability") FName AbilityId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Ability") FName ClassId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Ability") ETVAbilitySlot SlotKind = ETVAbilitySlot::Builder;
	/** Generic executors: /Script/ThistlewickVale.GA_TV_DirectDamage, GA_TV_Heal, GA_TV_ApplyEffect, GA_TV_GroundAoE, GA_TV_Channel,
	 *  GA_TV_Taunt, GA_TV_Leap, GA_TV_SummonMount, plus GA_TV_MultiStrike, GA_TV_Dispel, GA_TV_GroundTrap, GA_TV_Cone, GA_TV_FleeForHelp. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Ability") TSoftClassPtr<UTVGameplayAbility> AbilityClass;
	/** Identity tag, e.g. TV.Ability.Warrior.ShieldSlam. Its cooldown tag is TV.Cooldown.Ability.Warrior.ShieldSlam. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Ability") FGameplayTag AbilityTag;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Ability") float CastTime = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Ability") float ChannelTime = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Ability") float Cooldown = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Ability") bool TriggersGCD = true;
	/** Negative = generates. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Ability") float ResourceCost = 0.f;
	/** Kindling, Devotion or None. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Ability") FName ComboResource;
	/** +1 builders, -99 spenders (consume all). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Ability") int32 ComboDelta = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Ability") float RangeCm = 500.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Ability") float RadiusCm = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Ability") bool RequiresTarget = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Ability") bool RequiresLOS = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Ability") bool Interruptible = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Ability") bool UsableWhileMoving = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Ability") ETVTargetType TargetType = ETVTargetType::Enemy;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Ability") float BaseMagnitude = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Ability") ETVStat CoefficientStat = ETVStat::None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Ability") float Coefficient = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Ability") float ThreatMultiplier = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Ability") TArray<FTVEffectSpec> Effects;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Ability") FName MontageId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Ability") FName VfxCastId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Ability") FName VfxImpactId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Ability") FName SfxId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Ability") FName IconId;
	/** Supports {Magnitude} and {Cooldown}; see UTVCombatLibrary::FormatAbilityTooltip. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Ability") FText Tooltip;
};

/** DT_Mobs. Row name = MobId. */
USTRUCT(BlueprintType)
struct FTVMobRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Mob") FName MobId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Mob") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Mob") ETVMobTier Tier = ETVMobTier::Normal;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Mob") int32 LevelMin = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Mob") int32 LevelMax = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Mob") FName MeshId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Mob") FName AnimBlueprintId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Mob") float Scale = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Mob") float BaseHealth = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Mob") float HealthPerLevel = 20.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Mob") float BaseDamage = 5.f;
	/** Seconds between melee swings. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Mob") float AttackSpeed = 2.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Mob") float AttackRangeCm = 200.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Mob") float AggroRadiusCm = 1200.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Mob") float LeashRadiusCm = 4000.f;
	/** cm/s */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Mob") float MoveSpeed = 350.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Mob") TArray<FName> Abilities;
	/** Unused by the C++ state machine AI (kept for schema compatibility; a Behavior Tree can be swapped in later). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Mob") FName BehaviorTreeId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Mob") FName LootTableId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Mob") int32 XPReward = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Mob") FGameplayTag FactionTag;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Mob") bool PartyScaling = false;
};

/** Replicated cast bar state, readable by any client (target cast bars, nameplates). */
USTRUCT(BlueprintType)
struct FTVCastBarState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TV|Cast") FName AbilityId;
	/** Server world time (AGameStateBase::GetServerWorldTimeSeconds) when the cast started. */
	UPROPERTY(BlueprintReadOnly, Category = "TV|Cast") float StartServerTime = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "TV|Cast") float Duration = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "TV|Cast") bool bInterruptible = false;
	/** Channels drain right to left. */
	UPROPERTY(BlueprintReadOnly, Category = "TV|Cast") bool bIsChannel = false;
	UPROPERTY(BlueprintReadOnly, Category = "TV|Cast") bool bActive = false;
};

/** Maps a granted ability spec to its row, replicated so clients can resolve ids too. */
USTRUCT(BlueprintType)
struct FTVGrantedAbility
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TV|Ability") FName AbilityId;
	// FGameplayAbilitySpecHandle: USTRUCT with UPROPERTY int32 Handle (checked in 5.8 GameplayAbilitySpecHandle.h).
	UPROPERTY() FGameplayAbilitySpecHandle Handle;
};
