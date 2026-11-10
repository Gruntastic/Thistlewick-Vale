// Data-driven ability base. One instance per granted row (InstancedPerActor); the row is found through the ASC's
// replicated handle -> AbilityId map, so one executor class serves many rows.
//
// Flow: CanActivate (dead/stun/mount/casting/GCD/cooldown/resource/target/range) -> GCD (predicted) ->
//       cast bar for CastTime (moving cancels unless UsableWhileMoving) -> server re-validates range + LOS ->
//       commit (cost/generation, combo, cooldown) -> ExecuteAbility (authority: damage/heal/effects) -> end.
//
// 5.8: GameplayAbility.h sits too deep in the plugin to be staged from the PC, so the UGameplayAbility override
// signatures below were reconstructed from the calls in AbilitySystemComponent_Abilities.cpp (5.8) and are marked.
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Combat/TVCombatRows.h"
#include "TVGameplayAbility.generated.h"

class UTVAbilitySystemComponent;

UCLASS(Abstract)
class THISTLEWICKVALE_API UTVGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UTVGameplayAbility();

	// VERIFY-5.8: signature taken from ASC call sites (CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, &FailureTags)).
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	// VERIFY-5.8: long-stable signature; the ASC calls CallActivateAbility which forwards here.
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	// VERIFY-5.8: matches Instance->EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled) in the ASC.
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** Server-only twin used when granting to mobs (NetExecutionPolicy ServerOnly). */
	virtual TSubclassOf<UTVGameplayAbility> GetAIVariantClass() const { return GetClass(); }

	FName GetAbilityId() const { return ActiveAbilityId; }
	const FTVAbilityRow& GetRow() const { return Row; }
	bool IsCastingOrChanneling() const { return bCasting || bChanneling; }
	bool IsInterruptible() const { return Row.Interruptible; }
	bool CancelsOnDamage() const { return bCancelCastOnDamage; }
	/** Server: stop the cast/channel (interrupt, damage, death). */
	void CancelFromServer();

	/** Shared validation for CanActivate and the AI. Reason is NAME_None on success. */
	static bool CheckActivation(const UTVAbilitySystemComponent* ASC, const FTVAbilityRow& InRow, FName& OutReason, bool bCheckCosts);

protected:
	/** Authority + predicting client. Default implementation ends the ability. */
	virtual void ExecuteAbility();
	/** False for executors that never use a unit target (ground, channel, mount); the row decides otherwise. */
	virtual bool NeedsUnitTarget() const { return true; }
	/** Executors that manage their own lifetime (channel, leap) return true and call FinishAbility later. */
	virtual bool IsLatent() const { return false; }

	void StartCast(float Duration, bool bIsChannel);
	void OnCastComplete();
	void CheckMovement();
	/** Cost / generation, combo stacks, cooldown. Server applies; predicted GCD/cooldown on the client for instants. */
	bool CommitRow();
	void FinishAbility(bool bCancelled);

	UTVAbilitySystemComponent* GetTVASC() const;
	AActor* GetAvatar() const;
	bool IsAuthority() const;
	/** Unit target for this activation (locked at activation, re-checked at cast end). */
	AActor* GetLockedTarget() const { return LockedTarget.Get(); }
	/** Resolves Enemy/Friendly/Self targets per row rules (friendly spells fall back to self). */
	AActor* ResolveUnitTarget(AActor* Requested) const;
	/** Ground-targeted rows: hostile target -> its location, else caster location (mobs: random threat target). */
	FVector ResolveGroundLocation() const;
	/** Magnitude incl. combo multiplier (1 + consumed stacks). */
	float GetEffectiveMagnitude() const;
	/** Plays MontageId / VFX on everyone (server). */
	void PlayCue(FName VfxId, const FVector& Location) const;
	void SendFailure(FName Reason) const;
	/** Applies a cooldown GE (Duration seconds) with the row's cooldown tag. */
	void ApplyCooldownEffect(float Duration, const FGameplayTag& CooldownTag);

	/** Cast is cancelled when the caster takes damage (mount summon). */
	UPROPERTY(EditDefaultsOnly, Category = "TV|Ability") bool bCancelCastOnDamage = false;
	/** Movement speed (cm/s) above which a cast counts as moving. */
	UPROPERTY(EditDefaultsOnly, Category = "TV|Ability") float MovingSpeedThreshold = 20.f;

	FTVAbilityRow Row;
	FName ActiveAbilityId;
	TWeakObjectPtr<AActor> LockedTarget;
	FVector LockedGroundLocation = FVector::ZeroVector;
	int32 ConsumedComboStacks = 0;
	bool bCasting = false;
	bool bChanneling = false;
	bool bEnding = false;
	/** True while ActivateAbility runs synchronously (prediction window open on the predicting client). */
	bool bInActivationScope = false;
	FTimerHandle CastTimerHandle;
	FTimerHandle MoveCheckTimerHandle;
};
