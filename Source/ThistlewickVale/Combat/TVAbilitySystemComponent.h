// Project ASC. Players: owned by ATVPlayerState (Mixed replication), avatar = pawn. Mobs: on the pawn (Minimal).
#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "Core/TVTypes.h"
#include "Combat/TVCombatRows.h"
#include "TVAbilitySystemComponent.generated.h"

class UGameplayEffect;
class UTVGameplayAbility;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTVOnCastBarChanged, const FTVCastBarState&, CastBar);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTVOnGrantedAbilitiesChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTVOnComboChanged, FName, ComboResource, int32, Stacks);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTVOnAbilityFailed, FName, AbilityId, FName, Reason);

UCLASS(ClassGroup = (TV), meta = (BlueprintSpawnableComponent))
class THISTLEWICKVALE_API UTVAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UTVAbilitySystemComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ---- INTERFACES.md ---------------------------------------------------------------------------------------------
	/** Server. Replaces the single infinite equipment effect with these totals (ratings/flat values per stat). */
	void SetEquipmentStats(const TMap<ETVStat, float>& Totals);
	/** Server. Infinite effect; Magnitude goes to SetByCaller "Magnitude" (and TV.SetByCaller.Magnitude), transformed per class. */
	FActiveGameplayEffectHandle ApplyBonusEffect(TSubclassOf<UGameplayEffect> Effect, float Magnitude);
	void RemoveBonusEffect(FActiveGameplayEffectHandle Handle);
	/** Server. Equipped weapon's BaseWeaponDamage (ETVStat has no weapon entry). Sets the WeaponDamage base value. */
	void SetWeaponDamage(float Damage);
	/** Server. Grants every StartingAbilities entry with UnlockLevel <= Level (and ClassId "ALL" rows such as the mount). */
	void GrantAbilitiesForClass(FName ClassId, int32 Level);
	/** Any side. On a client also syncs the current ability target to the server first (same actor channel, so ordered). */
	UFUNCTION(BlueprintCallable, Category = "TV|Abilities")
	bool TryActivateAbilityById(FName AbilityId);
	UFUNCTION(BlueprintPure, Category = "TV|Abilities")
	float GetCooldownRemaining(FName AbilityId) const;
	UFUNCTION(BlueprintPure, Category = "TV|Abilities")
	float GetGCDRemaining() const;

	// ---- Setup -----------------------------------------------------------------------------------------------------
	/** Server. Base stats for the level, resource type, derived attributes; optionally refills vitals. */
	void InitializeForClass(FName InClassId, int32 Level, bool bRefillVitals);
	/** Server. Mob attributes (BALANCE.md section 1) and its abilities (server-only variants). */
	void InitializeForMob(const FTVMobRow& Row, int32 Level, float HealthScale);
	/** Server. Grants one ability row; returns the existing handle if already granted. */
	FGameplayAbilitySpecHandle GrantAbilityById(FName AbilityId, int32 AbilityLevel, bool bForAI);
	void ResetVitalsToFull();
	/** MaxHealth / MaxResource / ResourceRegen base values from stats (BALANCE.md section 1). */
	void RecalculateDerivedAttributes();

	FName GetAbilityIdForHandle(FGameplayAbilitySpecHandle Handle) const;
	FGameplayAbilitySpecHandle FindHandleForAbilityId(FName AbilityId) const;
	UFUNCTION(BlueprintPure, Category = "TV|Abilities")
	TArray<FName> GetGrantedAbilityIds() const;
	UFUNCTION(BlueprintPure, Category = "TV|Abilities")
	ETVResource GetResourceType() const { return ResourceType; }
	FName GetClassId() const { return ClassId; }
	bool IsMob() const { return bIsMob; }

	// ---- Ability target --------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintPure, Category = "TV|Abilities")
	AActor* GetAbilityTarget() const { return AbilityTarget; }
	/** Sets locally; on a client also tells the server. Also mirrors to the avatar's replicated CombatTarget. */
	void SetAbilityTarget(AActor* NewTarget);
	UFUNCTION(Server, Reliable)
	void ServerSetAbilityTarget(AActor* NewTarget);

	// ---- Casting ---------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintPure, Category = "TV|Cast")
	const FTVCastBarState& GetCastBarState() const { return CastBar; }
	/** Called by abilities on the server and the predicting client. */
	void BeginCastBar(FName AbilityId, float Duration, bool bInterruptible, bool bIsChannel);
	void EndCastBar();
	void SetActiveCastAbility(UTVGameplayAbility* Ability);
	UTVGameplayAbility* GetActiveCastAbility() const;
	bool IsCasting() const;
	/** Server. Cancels the current cast/channel if it is interruptible (or bForce). */
	bool InterruptCast(AActor* Interrupter, bool bForce = false);
	/** Server. Cancels casts flagged to break on damage (mount summon). */
	void NotifyDamageTakenForCasts();

	// ---- Combo resources (Kindling / Devotion) ---------------------------------------------------------------------
	UFUNCTION(BlueprintPure, Category = "TV|Abilities")
	int32 GetComboStacks(FName ComboResource) const;
	/** Server. Returns the stacks before the change. Delta <= -99 consumes all. */
	int32 ModifyComboStacks(FName ComboResource, int32 Delta);
	static constexpr int32 MaxComboStacks = 3;

	// ---- Combat state ----------------------------------------------------------------------------------------------
	/** Server. Refreshes the in-combat window (damage dealt/taken, threat). */
	void NotifyCombatActivity();
	UFUNCTION(BlueprintPure, Category = "TV|Combat")
	bool IsInCombat() const;
	/** Server. Mana 5-second rule. */
	void NotifyResourceSpent(float Amount);
	/** Server. Clears debuffs, combat state, cast; used on mob reset and player revive. */
	void ClearCombatState(bool bRemoveDebuffs);

	UPROPERTY(BlueprintAssignable, Category = "TV|Cast") FTVOnCastBarChanged OnCastBarChanged;
	UPROPERTY(BlueprintAssignable, Category = "TV|Abilities") FTVOnGrantedAbilitiesChanged OnGrantedAbilitiesChanged;
	UPROPERTY(BlueprintAssignable, Category = "TV|Abilities") FTVOnComboChanged OnComboChanged;
	UPROPERTY(BlueprintAssignable, Category = "TV|Abilities") FTVOnAbilityFailed OnAbilityFailed;

	/** Owning-client notification of a failed activation (out of range, no target, ...). */
	UFUNCTION(Client, Unreliable)
	void ClientAbilityFailed(FName AbilityId, FName Reason);

protected:
	UFUNCTION() void OnRep_GrantedAbilities();
	UFUNCTION() void OnRep_CastBar();
	UFUNCTION() void OnRep_Combo();
	UFUNCTION() void OnRep_InCombat();
	void SetInCombat(bool bNewInCombat);
	void RefreshAvatarMovement() const;

	void RegenTick();
	void OnStatChanged(const struct FOnAttributeChangeData& Data);
	void OnAbsorbTagChanged(const FGameplayTag Tag, int32 NewCount);
	void BindStatCallbacks();

	UPROPERTY(ReplicatedUsing = OnRep_GrantedAbilities) TArray<FTVGrantedAbility> GrantedAbilities;
	UPROPERTY(Replicated) TObjectPtr<AActor> AbilityTarget;
	UPROPERTY(ReplicatedUsing = OnRep_CastBar) FTVCastBarState CastBar;
	UPROPERTY(ReplicatedUsing = OnRep_Combo) int32 KindlingStacks = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Combo) int32 DevotionStacks = 0;
	UPROPERTY(Replicated) ETVResource ResourceType = ETVResource::None;
	UPROPERTY(Replicated) FName ClassId;
	UPROPERTY(ReplicatedUsing = OnRep_InCombat) bool bInCombat = false;

	UPROPERTY(Transient) TWeakObjectPtr<UTVGameplayAbility> ActiveCastAbility;

	bool bIsMob = false;
	bool bStatCallbacksBound = false;
	FTVClassLevelStatsRow BaseStats;
	TMap<ETVStat, float> EquipmentTotals;
	FActiveGameplayEffectHandle EquipmentEffectHandle;
	double LastCombatTime = -1000.0;
	double LastManaSpendTime = -1000.0;
	FTimerHandle RegenTimerHandle;
	static constexpr float RegenInterval = 0.5f;
};
