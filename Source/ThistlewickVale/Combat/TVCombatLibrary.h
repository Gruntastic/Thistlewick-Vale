// Combat rules in one place: data lookups, hostility, LOS, formulas (Docs/BALANCE.md section 1), and the server-side
// damage / heal / effect pipeline that every ability executor, the mob AI and the attribute sets go through.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ActiveGameplayEffectHandle.h"
#include "Core/TVTypes.h"
#include "Combat/TVCombatRows.h"
#include "TVCombatLibrary.generated.h"

class AActor;
class UAbilitySystemComponent;
class UTVAbilitySystemComponent;
class UTVGameplayEffect;
struct FGameplayEffectSpec;

/** Tuning constants that BALANCE.md does not pin down. */
namespace TVCombatTuning
{
	constexpr float GlobalCooldown = 1.5f;
	constexpr float GlobalCooldownFloor = 1.0f;
	constexpr float BaseCritChance = 0.05f;
	constexpr float CritMultiplier = 1.5f;
	/** Rating points per 1 % (BALANCE: Crit/3 %, Haste/3 %). */
	constexpr float RatingPerPercent = 3.f;
	/** Mitigation = A / (A + ArmorPerLevel x attackerLevel + ArmorConstant). */
	constexpr float ArmorPerLevel = 25.f;
	constexpr float ArmorConstant = 100.f;
	constexpr float MaxMitigation = 0.75f;
	constexpr float HealthPerStamina = 10.f;
	constexpr float ManaPerGearIntellect = 5.f;
	constexpr float ManaRegenPerSpirit = 0.04f;
	/** Mana regen multiplier while casting or within 5 s of spending mana. */
	constexpr float FiveSecondRuleFactor = 0.5f;
	constexpr float FiveSecondRuleWindow = 5.f;
	constexpr float CombatDropTime = 5.f;
	/** Rage from damage taken: (damage / max health) x this. */
	constexpr float RageFromDamageTakenScale = 50.f;
	/** Rage per direct damaging hit dealt (on top of ability generation). */
	constexpr float RageFromHitDealt = 2.f;
	/** Out-of-combat health regen for players, fraction of max health per second. */
	constexpr float PlayerOutOfCombatHealthRegen = 0.02f;
	constexpr float HealingThreatFactor = 0.5f;
	constexpr float FrontalHalfAngleDeg = 60.f;
	constexpr float DefaultMeleeRangeCm = 500.f;
	/** Mob hit damage growth per level above LevelMin (BALANCE). */
	constexpr float MobDamagePerLevel = 0.12f;
}

UCLASS()
class THISTLEWICKVALE_API UTVCombatLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ---- Data ------------------------------------------------------------------------------------------------------
	static const FTVAbilityRow* FindAbilityRow(FName AbilityId);
	static const FTVMobRow* FindMobRow(FName MobId);
	static const FTVClassRow* FindClassRow(FName ClassId);
	/** Matches FTVClassLevelStatsRow::ClassId == CurveOrClassId and Level (row names <ClassId>_L<n> are tried first). */
	static const FTVClassLevelStatsRow* FindClassLevelStats(FName CurveOrClassId, int32 Level);
	/** 0 at max level or if the row is missing. */
	static int32 GetXPToNext(int32 Level);
	static int32 GetMaxLevel();
	/** The FTVEffectSpec of a parameter effect class in the row, or null. */
	static const FTVEffectSpec* FindEffectOfClass(const FTVAbilityRow& Row, const UClass* EffectClass);

	// ---- Actors ----------------------------------------------------------------------------------------------------
	static UTVAbilitySystemComponent* GetTVASC(const AActor* Actor);
	UFUNCTION(BlueprintPure, Category = "TV|Combat") static bool IsAlive(const AActor* Actor);
	/** Players vs mobs; mobs never fight mobs, players never fight players in the slice. */
	UFUNCTION(BlueprintPure, Category = "TV|Combat") static bool IsHostile(const AActor* A, const AActor* B);
	UFUNCTION(BlueprintPure, Category = "TV|Combat") static bool IsPlayerSide(const AActor* Actor);
	UFUNCTION(BlueprintPure, Category = "TV|Combat") static int32 GetCombatLevel(const AActor* Actor);
	/** Server visibility trace from eye height to eye height (ECC_Visibility, pawns ignored). */
	UFUNCTION(BlueprintPure, Category = "TV|Combat") static bool HasLineOfSight(const AActor* From, const AActor* To);
	static bool HasLineOfSightToLocation(const AActor* From, const FVector& Location);
	static void GetHostilesInRadius(const AActor* Instigator, const FVector& Center, float Radius, TArray<AActor*>& OutActors);
	/** Players: party members (server: UTVPartySubsystem, plus self). Mobs: other living mobs. */
	static void GetFriendliesInRadius(const AActor* Instigator, const FVector& Center, float Radius, TArray<AActor*>& OutActors);
	static bool IsInFrontOf(const AActor* Defender, const AActor* Attacker, float HalfAngleDeg);
	static float DistanceBetween(const AActor* A, const AActor* B);

	// ---- Formulas --------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintPure, Category = "TV|Combat") static float HastePercent(float HasteRating);
	UFUNCTION(BlueprintPure, Category = "TV|Combat") static float ComputeGCD(float HasteRating);
	UFUNCTION(BlueprintPure, Category = "TV|Combat") static float ComputeCastTime(float BaseCastTime, float HasteRating);
	UFUNCTION(BlueprintPure, Category = "TV|Combat") static float ComputeCritChance(float CritRating, float BonusFraction);
	UFUNCTION(BlueprintPure, Category = "TV|Combat") static float ComputeArmorMitigation(float Armor, int32 AttackerLevel);
	static float GetStatValue(const UAbilitySystemComponent* ASC, ETVStat Stat);
	/** Mob melee hit damage (already scaled by level and party); players return 0. */
	static float GetHitDamage(const AActor* Actor);
	/**
	 * Ability magnitude before crit and target mitigation.
	 * Players: BaseMagnitude + stat x Coefficient (x UltimatePower for Ultimate slot).
	 * Mobs: damage abilities BaseMagnitude x hit damage; heals are a fraction of the target's max health (see executor).
	 */
	static float ComputeAbilityMagnitude(const AActor* Source, const FTVAbilityRow& Row);
	static FText FormatAbilityTooltip(const FTVAbilityRow& Row, float Magnitude);

	// ---- Server pipeline -------------------------------------------------------------------------------------------
	/** Crit roll + DamageDone, then GE_TV_Damage on the target. Returns the raw amount sent (0 if nothing happened). */
	static float DealDamage(AActor* Source, AActor* Target, float Amount, float ThreatMultiplier, bool bCanCrit);
	static float DealHeal(AActor* Source, AActor* Target, float Amount, float ThreatMultiplier, bool bCanCrit);
	/** Applies one data effect (Applied role) incl. hooks. Parameter-role effects are ignored. */
	static FActiveGameplayEffectHandle ApplyEffectSpec(AActor* Source, AActor* Target, const FTVEffectSpec& EffectSpec, const FTVAbilityRow& Row, float AbilityMagnitude);
	/** Applies every Applied-role effect of the row to Target. */
	static void ApplyRowEffects(AActor* Source, AActor* Target, const FTVAbilityRow& Row, float AbilityMagnitude);

	/** Called by UTVAttributeSet_Vitals for IncomingDamage. */
	static void ResolveIncomingDamage(UAbilitySystemComponent* TargetASC, const FGameplayEffectSpec& Spec, float RawDamage);
	/** Called by UTVAttributeSet_Vitals for IncomingHeal. */
	static void ResolveIncomingHeal(UAbilitySystemComponent* TargetASC, const FGameplayEffectSpec& Spec, float RawHeal);

	/** Threat from healing: HealingThreatFactor x amount, split across mobs engaged with Target. */
	static void AddHealingThreat(AActor* Healer, AActor* HealedTarget, float EffectiveHeal, float ThreatMultiplier);

	/** Spawns Count mobs around Summoner that inherit its scaling and join its fight. */
	static void SummonAdds(AActor* Summoner, FName MobId, int32 Count);
};
