#include "Combat/Abilities/TVGameplayAbility.h"
#include "ThistlewickVale.h"
#include "Combat/TVAbilitySystemComponent.h"
#include "Combat/TVAttributeSets.h"
#include "Combat/TVCharacterBase.h"
#include "Combat/TVPlayerState.h"
#include "Combat/TVCombatLibrary.h"
#include "Combat/TVCombatTags.h"
#include "Combat/Effects/TVGameplayEffects.h"
#include "Systems/TVMountComponent.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
	/** Server-side slack for GCD/cooldown checks: the server's copy started half a round trip after the client's. */
	constexpr float ServerTimingTolerance = 0.3f;
	/** Server-side slack on range checks (movement between client and server). */
	constexpr float ServerRangeTolerance = 150.f;

	bool IsMountRow(const FTVAbilityRow& Row)
	{
		return (TVTags::Ability_Mount_Summon().IsValid() && Row.AbilityTag == TVTags::Ability_Mount_Summon()) || Row.ClassId == FName(TEXT("ALL"));
	}


	float RangeTo(const AActor* From, const AActor* To)
	{
		return FMath::Max(0.f, UTVCombatLibrary::DistanceBetween(From, To) - UTVCombatLibrary::GetCapsuleRadius(From) - UTVCombatLibrary::GetCapsuleRadius(To));
	}

	UTVMountComponent* GetMountComponent(const UTVAbilitySystemComponent* ASC)
	{
		const ATVPlayerState* PS = ASC ? Cast<ATVPlayerState>(ASC->GetOwner()) : nullptr;
		return PS ? PS->GetMount() : nullptr;
	}
}

UTVGameplayAbility::UTVGameplayAbility()
{
	// VERIFY-5.8: GameplayAbility.h could not be staged; direct member writes as in earlier 5.x (the ASC reads them through
	// GetInstancingPolicy()/GetNetExecutionPolicy(), which exist in 5.8).
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

// ---- Activation checks -----------------------------------------------------------------------------------------------

bool UTVGameplayAbility::CheckActivation(const UTVAbilitySystemComponent* ASC, const FTVAbilityRow& InRow, FName& OutReason, bool bCheckCosts)
{
	OutReason = NAME_None;
	if (!ASC)
	{
		OutReason = TEXT("NoAbilitySystem");
		return false;
	}
	const AActor* Avatar = ASC->GetAvatarActor();
	const ATVCharacterBase* Character = Cast<ATVCharacterBase>(Avatar);
	if (!Character || Character->IsDead())
	{
		OutReason = TEXT("Dead");
		return false;
	}
	if (TVTags::State_Stunned().IsValid() && ASC->HasMatchingGameplayTag(TVTags::State_Stunned()))
	{
		OutReason = TEXT("Stunned");
		return false;
	}
	if (TVTags::State_Evading().IsValid() && ASC->HasMatchingGameplayTag(TVTags::State_Evading()))
	{
		OutReason = TEXT("Evading");
		return false;
	}

	const bool bMountRow = IsMountRow(InRow);
	const bool bMounted = TVTags::State_Mounted().IsValid() && ASC->HasMatchingGameplayTag(TVTags::State_Mounted());
	if (bMounted && !bMountRow)
	{
		OutReason = TEXT("Mounted");
		return false;
	}
	if (bMountRow && !bMounted)
	{
		const UTVMountComponent* Mount = GetMountComponent(ASC);
		if (!Mount || Mount->GetOwnedMounts().Num() == 0)
		{
			OutReason = TEXT("NoMount");
			return false;
		}
		if (Mount->IsInDungeon())
		{
			OutReason = TEXT("CannotMountHere");
			return false;
		}
		if (ASC->IsInCombat())
		{
			OutReason = TEXT("InCombat");
			return false;
		}
	}

	if (ASC->IsCasting())
	{
		OutReason = TEXT("Casting");
		return false;
	}

	const bool bServerChecksPlayer = ASC->GetOwner() && ASC->GetOwner()->HasAuthority() && !ASC->IsMob();
	const float Tolerance = bServerChecksPlayer ? ServerTimingTolerance : 0.f;
	if (InRow.TriggersGCD && ASC->GetGCDRemaining() > Tolerance)
	{
		OutReason = TEXT("GlobalCooldown");
		return false;
	}
	if (ASC->GetCooldownRemaining(InRow.AbilityId) > Tolerance)
	{
		OutReason = TEXT("Cooldown");
		return false;
	}

	if (bCheckCosts && InRow.ResourceCost > 0.f && ASC->GetResourceType() != ETVResource::None)
	{
		const bool bFreeBuilder = InRow.SlotKind == ETVAbilitySlot::Builder && TVTags::Buff_EmpowerBuilder().IsValid()
			&& ASC->HasMatchingGameplayTag(TVTags::Buff_EmpowerBuilder());
		const float Resource = ASC->GetNumericAttribute(UTVAttributeSet_Vitals::GetResourceAttribute());
		if (!bFreeBuilder && Resource + KINDA_SMALL_NUMBER < InRow.ResourceCost)
		{
			OutReason = TEXT("NotEnoughResource");
			return false;
		}
	}

	// Target: only for executors that use a unit target.
	const UClass* ExecutorClass = InRow.AbilityClass.Get();
	const UTVGameplayAbility* Executor = ExecutorClass ? ExecutorClass->GetDefaultObject<UTVGameplayAbility>() : nullptr;
	const bool bExecutorUsesTarget = !Executor || Executor->NeedsUnitTarget();
	AActor* Target = ASC->GetAbilityTarget();
	const float RangeSlack = bServerChecksPlayer ? ServerRangeTolerance : 0.f;
	if (bExecutorUsesTarget && InRow.TargetType == ETVTargetType::Enemy && InRow.RequiresTarget)
	{
		if (!Target)
		{
			OutReason = TEXT("NoTarget");
			return false;
		}
		if (!UTVCombatLibrary::IsAlive(Target) || !UTVCombatLibrary::IsHostile(Avatar, Target))
		{
			OutReason = TEXT("InvalidTarget");
			return false;
		}
		if (InRow.RangeCm > 0.f && RangeTo(Avatar, Target) > InRow.RangeCm + RangeSlack)
		{
			OutReason = TEXT("OutOfRange");
			return false;
		}
	}
	else if (bExecutorUsesTarget && InRow.TargetType == ETVTargetType::Friendly && Target && Target != Avatar
		&& UTVCombatLibrary::IsAlive(Target) && !UTVCombatLibrary::IsHostile(Avatar, Target))
	{
		if (InRow.RangeCm > 0.f && RangeTo(Avatar, Target) > InRow.RangeCm + RangeSlack)
		{
			OutReason = TEXT("OutOfRange");
			return false;
		}
	}
	return true;
}

// VERIFY-5.8: override signature reconstructed from AbilitySystemComponent_Abilities.cpp call sites.
bool UTVGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	UTVAbilitySystemComponent* ASC = ActorInfo ? Cast<UTVAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()) : nullptr;
	if (!ASC)
	{
		return false;
	}
	const FName Id = ASC->GetAbilityIdForHandle(Handle);
	const FTVAbilityRow* FoundRow = UTVCombatLibrary::FindAbilityRow(Id);
	if (!FoundRow)
	{
		return false;
	}
	FName Reason;
	if (!CheckActivation(ASC, *FoundRow, Reason, true))
	{
		// The predicting client already showed its own reason; tell it why the server refused.
		if (ActorInfo->IsNetAuthority() && !ActorInfo->IsLocallyControlled())
		{
			ASC->ClientAbilityFailed(Id, Reason);
		}
		return false;
	}
	return true;
}

// ---- Activation flow -------------------------------------------------------------------------------------------------

void UTVGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	bEnding = false;
	bCasting = false;
	bChanneling = false;
	ConsumedComboStacks = 0;
	bInActivationScope = true;

	UTVAbilitySystemComponent* ASC = GetTVASC();
	ActiveAbilityId = ASC ? ASC->GetAbilityIdForHandle(Handle) : NAME_None;
	const FTVAbilityRow* FoundRow = UTVCombatLibrary::FindAbilityRow(ActiveAbilityId);
	if (!ASC || !FoundRow)
	{
		FinishAbility(true);
		bInActivationScope = false;
		return;
	}
	Row = *FoundRow;
	LockedTarget = ResolveUnitTarget(ASC->GetAbilityTarget());
	LockedGroundLocation = ResolveGroundLocation();

	if (IsAuthority() && Row.RequiresLOS && LockedTarget.IsValid() && LockedTarget.Get() != GetAvatar()
		&& !UTVCombatLibrary::HasLineOfSight(GetAvatar(), LockedTarget.Get()))
	{
		SendFailure(TEXT("NoLineOfSight"));
		FinishAbility(true);
		bInActivationScope = false;
		return;
	}

	if (Row.TriggersGCD && TVTags::Cooldown_GCD().IsValid())
	{
		const float Haste = UTVCombatLibrary::GetStatValue(ASC, ETVStat::Haste);
		ApplyCooldownEffect(UTVCombatLibrary::ComputeGCD(Haste), TVTags::Cooldown_GCD());
	}

	// Hostile actions put the caster in combat.
	if (IsAuthority() && Row.TargetType == ETVTargetType::Enemy && LockedTarget.IsValid())
	{
		ASC->NotifyCombatActivity();
	}

	const float CastTime = UTVCombatLibrary::ComputeCastTime(Row.CastTime, UTVCombatLibrary::GetStatValue(ASC, ETVStat::Haste));
	if (CastTime > 0.f)
	{
		StartCast(CastTime, false);
	}
	else
	{
		OnCastComplete();
	}
	bInActivationScope = false;
}

void UTVGameplayAbility::StartCast(float Duration, bool bIsChannel)
{
	UTVAbilitySystemComponent* ASC = GetTVASC();
	UWorld* World = GetWorld();
	if (!ASC || !World)
	{
		return;
	}
	bCasting = !bIsChannel;
	bChanneling = bIsChannel;
	ASC->SetActiveCastAbility(this);
	ASC->BeginCastBar(ActiveAbilityId, Duration, Row.Interruptible, bIsChannel);
	if (!bIsChannel)
	{
		World->GetTimerManager().SetTimer(CastTimerHandle, this, &UTVGameplayAbility::OnCastComplete, Duration, false);
		if (IsAuthority())
		{
			PlayCue(Row.VfxCastId, GetAvatar() ? GetAvatar()->GetActorLocation() : FVector::ZeroVector);
		}
	}
	if (!Row.UsableWhileMoving)
	{
		World->GetTimerManager().SetTimer(MoveCheckTimerHandle, this, &UTVGameplayAbility::CheckMovement, 0.1f, true);
	}
}

void UTVGameplayAbility::CheckMovement()
{
	const ACharacter* Character = Cast<ACharacter>(GetAvatar());
	if (!Character || !(bCasting || bChanneling))
	{
		return;
	}
	const UCharacterMovementComponent* Move = Character->GetCharacterMovement();
	const bool bMoving = Character->GetVelocity().Size2D() > MovingSpeedThreshold || (Move && Move->IsFalling());
	if (bMoving)
	{
		SendFailure(TEXT("Moved"));
		FinishAbility(true);
	}
}

void UTVGameplayAbility::OnCastComplete()
{
	UTVAbilitySystemComponent* ASC = GetTVASC();
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(CastTimerHandle);
		World->GetTimerManager().ClearTimer(MoveCheckTimerHandle);
	}
	const bool bWasCasting = bCasting;
	bCasting = false;
	if (!ASC || !IsActive())
	{
		return;
	}

	if (IsAuthority())
	{
		// Re-validate the locked target at the end of the cast.
		const bool bUnitRow = NeedsUnitTarget() && Row.RequiresTarget && Row.TargetType == ETVTargetType::Enemy;
		AActor* Target = LockedTarget.Get();
		if (bUnitRow)
		{
			const float Slack = ASC->IsMob() ? 50.f : ServerRangeTolerance;
			if (!UTVCombatLibrary::IsAlive(Target))
			{
				SendFailure(TEXT("InvalidTarget"));
				FinishAbility(true);
				return;
			}
			if (Row.RangeCm > 0.f && RangeTo(GetAvatar(), Target) > Row.RangeCm + Slack)
			{
				SendFailure(TEXT("OutOfRange"));
				FinishAbility(true);
				return;
			}
			if (Row.RequiresLOS && !UTVCombatLibrary::HasLineOfSight(GetAvatar(), Target))
			{
				SendFailure(TEXT("NoLineOfSight"));
				FinishAbility(true);
				return;
			}
		}
		if (!CommitRow())
		{
			SendFailure(TEXT("NotEnoughResource"));
			FinishAbility(true);
			return;
		}
		if (bWasCasting && ASC->GetActiveCastAbility() == this)
		{
			ASC->EndCastBar();
			ASC->SetActiveCastAbility(nullptr);
		}
		const AActor* CueTarget = Target ? Target : GetAvatar();
		PlayCue(Row.VfxImpactId, CueTarget ? CueTarget->GetActorLocation() : LockedGroundLocation);
		ExecuteAbility();
		if (!IsLatent())
		{
			FinishAbility(false);
		}
		return;
	}

	// Predicting client.
	if (bInActivationScope)
	{
		CommitRow();
		ExecuteAbility();
		if (!IsLatent())
		{
			FinishAbility(false);
		}
	}
	// A cast that finished on the client: the server ends the ability (and replicates the end) once it has executed it.
}

bool UTVGameplayAbility::CommitRow()
{
	UTVAbilitySystemComponent* ASC = GetTVASC();
	if (!ASC)
	{
		return false;
	}
	if (IsAuthority())
	{
		// Resource cost (negative = generation).
		float Cost = Row.ResourceCost;
		if (Cost > 0.f && Row.SlotKind == ETVAbilitySlot::Builder && TVTags::Buff_EmpowerBuilder().IsValid()
			&& ASC->HasMatchingGameplayTag(TVTags::Buff_EmpowerBuilder()))
		{
			Cost = 0.f;
		}
		if (Cost != 0.f && ASC->GetResourceType() != ETVResource::None)
		{
			const FGameplayAttribute ResourceAttr = UTVAttributeSet_Vitals::GetResourceAttribute();
			const float Resource = ASC->GetNumericAttributeBase(ResourceAttr);
			const float Max = ASC->GetNumericAttribute(UTVAttributeSet_Vitals::GetMaxResourceAttribute());
			if (Cost > 0.f && Resource + KINDA_SMALL_NUMBER < Cost)
			{
				return false;
			}
			ASC->SetNumericAttributeBase(ResourceAttr, FMath::Clamp(Resource - Cost, 0.f, FMath::Max(0.f, Max)));
			if (Cost > 0.f)
			{
				ASC->NotifyResourceSpent(Cost);
			}
		}

		// Kindling / Devotion.
		if (!Row.ComboResource.IsNone() && Row.ComboDelta != 0)
		{
			const int32 Before = ASC->ModifyComboStacks(Row.ComboResource, Row.ComboDelta);
			if (Row.ComboDelta < 0)
			{
				ConsumedComboStacks = FMath::Max(0, Before - ASC->GetComboStacks(Row.ComboResource));
			}
		}
	}

	// Cooldown: server always; the predicting client only inside the prediction window.
	if (Row.Cooldown > 0.f && (IsAuthority() || bInActivationScope))
	{
		float Duration = Row.Cooldown;
		if (Row.SlotKind == ETVAbilitySlot::Defensive && ASC->HasAttributeSetForAttribute(UTVAttributeSet_Combat::GetDefensiveCooldownMultiplierAttribute()))
		{
			Duration *= ASC->GetNumericAttribute(UTVAttributeSet_Combat::GetDefensiveCooldownMultiplierAttribute());
		}
		ApplyCooldownEffect(Duration, TVTags::CooldownTagFor(Row.AbilityTag));
	}
	return true;
}

void UTVGameplayAbility::ApplyCooldownEffect(float Duration, const FGameplayTag& CooldownTag)
{
	UTVAbilitySystemComponent* ASC = GetTVASC();
	if (!ASC || !CooldownTag.IsValid() || Duration <= 0.f)
	{
		return;
	}
	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(UGE_TV_Cooldown::StaticClass(), 1.f, ASC->MakeEffectContext());
	if (!SpecHandle.IsValid())
	{
		return;
	}
	SpecHandle.Data->SetSetByCallerMagnitude(TVSetByCaller::Duration, Duration);
	SpecHandle.Data->DynamicGrantedTags.AddTag(CooldownTag);
	if (bInActivationScope)
	{
		// VERIFY-5.8: UGameplayAbility::ApplyGameplayEffectSpecToOwner (handles the activation prediction key).
		ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpecHandle);
	}
	else if (IsAuthority())
	{
		ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
}

void UTVGameplayAbility::FinishAbility(bool bCancelled)
{
	if (!IsActive() || bEnding)
	{
		return;
	}
	if (IsAuthority())
	{
		// VERIFY-5.8: EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled).
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bCancelled);
	}
	else if (bCancelled)
	{
		// Client-side cancel (moved while casting): tell the server.
		CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
	}
	else if (bInActivationScope)
	{
		// Instant ability: the client copy ends locally; the server ends its own copy.
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
	}
	// Otherwise (latent work finished on the client) the server's replicated end closes the client copy.
}

void UTVGameplayAbility::CancelFromServer()
{
	if (IsAuthority() && IsActive() && (bCasting || bChanneling))
	{
		SendFailure(TEXT("Interrupted"));
		FinishAbility(true);
	}
}

void UTVGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bEnding)
	{
		return;
	}
	bEnding = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
	if (UTVAbilitySystemComponent* ASC = GetTVASC())
	{
		if (ASC->GetActiveCastAbility() == this)
		{
			ASC->SetActiveCastAbility(nullptr);
			ASC->EndCastBar();
		}
		else if (bCasting || bChanneling)
		{
			ASC->EndCastBar();
		}
	}
	bCasting = false;
	bChanneling = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	bEnding = false;
}

void UTVGameplayAbility::ExecuteAbility()
{
}

// ---- Helpers ---------------------------------------------------------------------------------------------------------

UTVAbilitySystemComponent* UTVGameplayAbility::GetTVASC() const
{
	return Cast<UTVAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());
}

AActor* UTVGameplayAbility::GetAvatar() const
{
	return GetAvatarActorFromActorInfo();
}

bool UTVGameplayAbility::IsAuthority() const
{
	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	return Info && Info->IsNetAuthority();
}

AActor* UTVGameplayAbility::ResolveUnitTarget(AActor* Requested) const
{
	AActor* Avatar = GetAvatar();
	switch (Row.TargetType)
	{
	case ETVTargetType::Self:
	case ETVTargetType::PartyAoE:
		return Avatar;
	case ETVTargetType::Friendly:
		if (Requested && UTVCombatLibrary::IsAlive(Requested) && !UTVCombatLibrary::IsHostile(Avatar, Requested)
			&& UTVCombatLibrary::IsPlayerSide(Requested) == UTVCombatLibrary::IsPlayerSide(Avatar))
		{
			return Requested;
		}
		return Avatar;
	case ETVTargetType::Enemy:
	case ETVTargetType::Ground:
	default:
		return (Requested && UTVCombatLibrary::IsAlive(Requested) && UTVCombatLibrary::IsHostile(Avatar, Requested)) ? Requested : nullptr;
	}
}

FVector UTVGameplayAbility::ResolveGroundLocation() const
{
	const AActor* Avatar = GetAvatar();
	if (!Avatar)
	{
		return FVector::ZeroVector;
	}
	const AActor* Target = LockedTarget.Get();
	if (!Target || !UTVCombatLibrary::IsHostile(Avatar, Target))
	{
		return Avatar->GetActorLocation();
	}
	FVector Location = Target->GetActorLocation();
	const FVector Offset = Location - Avatar->GetActorLocation();
	if (Row.RangeCm > 0.f && Offset.Size2D() > Row.RangeCm)
	{
		Location = Avatar->GetActorLocation() + Offset.GetSafeNormal2D() * Row.RangeCm;
		Location.Z = Target->GetActorLocation().Z;
	}
	return Location;
}

float UTVGameplayAbility::GetEffectiveMagnitude() const
{
	float Magnitude = UTVCombatLibrary::ComputeAbilityMagnitude(GetAvatar(), Row);
	if (Row.ComboDelta < 0)
	{
		Magnitude *= 1.f + ConsumedComboStacks;
	}
	return Magnitude;
}

void UTVGameplayAbility::PlayCue(FName VfxId, const FVector& Location) const
{
	if (!IsAuthority())
	{
		return;
	}
	if (ATVCharacterBase* Character = Cast<ATVCharacterBase>(GetAvatar()))
	{
		Character->MulticastCombatCue(Row.MontageId, VfxId, Row.SfxId, Location);
	}
}

void UTVGameplayAbility::SendFailure(FName Reason) const
{
	UTVAbilitySystemComponent* ASC = GetTVASC();
	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	if (!ASC || !Info)
	{
		return;
	}
	if (Info->IsLocallyControlled())
	{
		ASC->OnAbilityFailed.Broadcast(ActiveAbilityId, Reason);
	}
	else if (Info->IsNetAuthority())
	{
		ASC->ClientAbilityFailed(ActiveAbilityId, Reason);
	}
}
