#include "Combat/Abilities/TVAbilityExecutors.h"
#include "ThistlewickVale.h"
#include "Combat/Abilities/TVGroundEffectActor.h"
#include "Combat/TVAbilitySystemComponent.h"
#include "Combat/TVAttributeSets.h"
#include "Combat/TVCharacterBase.h"
#include "Combat/TVPlayerState.h"
#include "Combat/TVCombatLibrary.h"
#include "Combat/TVCombatTags.h"
#include "Combat/Effects/TVGameplayEffects.h"
#include "Systems/TVMountComponent.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
	/** Applies the row's effects: caster-side effects once, the rest to every target. */
	void ApplyEffectsToTargets(AActor* Source, const TArray<AActor*>& Targets, const FTVAbilityRow& Row, float Magnitude)
	{
		for (const FTVEffectSpec& Effect : Row.Effects)
		{
			const UClass* EffectClass = Effect.EffectClass.LoadSynchronous();
			const UTVGameplayEffect* Def = EffectClass ? Cast<UTVGameplayEffect>(EffectClass->GetDefaultObject()) : nullptr;
			if (Def && Def->Role == ETVEffectRole::Parameter)
			{
				continue;
			}
			if (Def && Def->bAppliesToSource)
			{
				UTVCombatLibrary::ApplyEffectSpec(Source, Source, Effect, Row, Magnitude);
				continue;
			}
			for (AActor* Target : Targets)
			{
				UTVCombatLibrary::ApplyEffectSpec(Source, Target, Effect, Row, Magnitude);
			}
		}
	}

	float ParamMagnitude(const FTVAbilityRow& Row, const UClass* ParamClass, float Default)
	{
		const FTVEffectSpec* Spec = UTVCombatLibrary::FindEffectOfClass(Row, ParamClass);
		return (Spec && Spec->Magnitude > 0.f) ? Spec->Magnitude : Default;
	}

	bool IsMob(const AActor* Actor)
	{
		return Actor && Actor->IsA<ATVMobCharacter>();
	}

	float CapsuleRadius(const AActor* Actor)
	{
		const ACharacter* Character = Cast<ACharacter>(Actor);
		return (Character && Character->GetCapsuleComponent()) ? Character->GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.f;
	}

	/** Hit count while the Stag Volley buff is up (its Magnitude), else 1. */
	int32 EmpoweredBuilderHits(const UAbilitySystemComponent* ASC)
	{
		const FGameplayTag Tag = TVTags::Buff_EmpowerBuilder();
		if (!ASC || !Tag.IsValid() || !ASC->HasMatchingGameplayTag(Tag))
		{
			return 1;
		}
		const TArray<FActiveGameplayEffectHandle> Handles = ASC->GetActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(Tag)));
		for (const FActiveGameplayEffectHandle& Handle : Handles)
		{
			if (const FActiveGameplayEffect* Active = ASC->GetActiveGameplayEffect(Handle))
			{
				return FMath::Max(1, FMath::RoundToInt(Active->Spec.GetSetByCallerMagnitude(TVSetByCaller::Magnitude, false, 2.f)));
			}
		}
		return 2;
	}
}

// =====================================================================================================================
// DirectDamage
// =====================================================================================================================

TSubclassOf<UTVGameplayAbility> UGA_TV_DirectDamage::GetAIVariantClass() const { return UGA_TV_DirectDamage_AI::StaticClass(); }

void UGA_TV_DirectDamage::GatherTargets(TArray<AActor*>& OutTargets) const
{
	AActor* Avatar = GetAvatar();
	if (!Avatar)
	{
		return;
	}
	if (Row.TargetType == ETVTargetType::PartyAoE)
	{
		UTVCombatLibrary::GetHostilesInRadius(Avatar, Avatar->GetActorLocation(), Row.RadiusCm > 0.f ? Row.RadiusCm : Row.RangeCm, OutTargets);
		return;
	}
	if (AActor* Target = GetLockedTarget())
	{
		OutTargets.Add(Target);
	}
	// A radius on a single-target row cleaves: hostiles within RadiusCm in front of the caster.
	if (Row.RadiusCm > 0.f && Row.TargetType == ETVTargetType::Enemy)
	{
		TArray<AActor*> Nearby;
		UTVCombatLibrary::GetHostilesInRadius(Avatar, Avatar->GetActorLocation(), Row.RadiusCm + CapsuleRadius(Avatar), Nearby);
		for (AActor* Other : Nearby)
		{
			if (UTVCombatLibrary::IsInFrontOf(Avatar, Other, TVCombatTuning::FrontalHalfAngleDeg))
			{
				OutTargets.AddUnique(Other);
			}
		}
	}
}

void UGA_TV_DirectDamage::ExecuteAbility()
{
	if (!IsAuthority())
	{
		return;
	}
	AActor* Avatar = GetAvatar();
	TArray<AActor*> Targets;
	GatherTargets(Targets);
	if (Targets.Num() == 0)
	{
		return;
	}
	const float Magnitude = GetEffectiveMagnitude();
	int32 Hits = FMath::Max(1, FMath::RoundToInt(ParamMagnitude(Row, UGE_TV_MultiHit::StaticClass(), 1.f)));
	if (Row.SlotKind == ETVAbilitySlot::Builder)
	{
		Hits = FMath::Max(Hits, EmpoweredBuilderHits(GetTVASC()));
	}
	const bool bCanCrit = !IsMob(Avatar);
	for (AActor* Target : Targets)
	{
		for (int32 Hit = 0; Hit < Hits; ++Hit)
		{
			UTVCombatLibrary::DealDamage(Avatar, Target, Magnitude, Row.ThreatMultiplier, bCanCrit);
		}
	}
	ApplyEffectsToTargets(Avatar, Targets, Row, Magnitude);
}

// =====================================================================================================================
// Heal
// =====================================================================================================================

TSubclassOf<UTVGameplayAbility> UGA_TV_Heal::GetAIVariantClass() const { return UGA_TV_Heal_AI::StaticClass(); }

void UGA_TV_Heal::ExecuteAbility()
{
	if (!IsAuthority())
	{
		return;
	}
	AActor* Avatar = GetAvatar();
	TArray<AActor*> Targets;
	if (Row.TargetType == ETVTargetType::PartyAoE)
	{
		UTVCombatLibrary::GetFriendliesInRadius(Avatar, Avatar->GetActorLocation(), Row.RadiusCm > 0.f ? Row.RadiusCm : 1500.f, Targets);
	}
	else if (AActor* Target = GetLockedTarget())
	{
		Targets.Add(Target);
	}
	const bool bMob = IsMob(Avatar);
	const float Magnitude = GetEffectiveMagnitude();
	for (AActor* Target : Targets)
	{
		float Amount = Magnitude;
		if (bMob)
		{
			// BALANCE.md: mob heal BaseMagnitude = fraction of the target's max health.
			const UTVAbilitySystemComponent* TargetASC = UTVCombatLibrary::GetTVASC(Target);
			Amount = TargetASC ? Row.BaseMagnitude * TargetASC->GetNumericAttribute(UTVAttributeSet_Vitals::GetMaxHealthAttribute()) : 0.f;
		}
		UTVCombatLibrary::DealHeal(Avatar, Target, Amount, Row.ThreatMultiplier, !bMob);
	}
	ApplyEffectsToTargets(Avatar, Targets, Row, Magnitude);
}

// =====================================================================================================================
// ApplyEffect
// =====================================================================================================================

TSubclassOf<UTVGameplayAbility> UGA_TV_ApplyEffect::GetAIVariantClass() const { return UGA_TV_ApplyEffect_AI::StaticClass(); }

void UGA_TV_ApplyEffect::ExecuteAbility()
{
	if (!IsAuthority())
	{
		return;
	}
	AActor* Avatar = GetAvatar();
	TArray<AActor*> Targets;
	switch (Row.TargetType)
	{
	case ETVTargetType::PartyAoE:
		UTVCombatLibrary::GetFriendliesInRadius(Avatar, Avatar->GetActorLocation(), Row.RadiusCm > 0.f ? Row.RadiusCm : 4000.f, Targets);
		break;
	case ETVTargetType::Self:
		Targets.Add(Avatar);
		break;
	default:
		if (AActor* Target = GetLockedTarget())
		{
			Targets.Add(Target);
		}
		break;
	}
	if (Targets.Num() == 0)
	{
		return;
	}
	ApplyEffectsToTargets(Avatar, Targets, Row, GetEffectiveMagnitude());
}

// =====================================================================================================================
// GroundAoE
// =====================================================================================================================

TSubclassOf<UTVGameplayAbility> UGA_TV_GroundAoE::GetAIVariantClass() const { return UGA_TV_GroundAoE_AI::StaticClass(); }

void UGA_TV_GroundAoE::ExecuteAbility()
{
	UWorld* World = GetWorld();
	AActor* Avatar = GetAvatar();
	if (!IsAuthority() || !World || !Avatar)
	{
		return;
	}
	const FVector Location = LockedGroundLocation;
	const float Radius = Row.RadiusCm > 0.f ? Row.RadiusCm : 300.f;
	const float Magnitude = GetEffectiveMagnitude();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Instigator = Cast<APawn>(Avatar);
	Params.OverrideLevel = Avatar->GetLevel();

	if (const FTVEffectSpec* Trap = UTVCombatLibrary::FindEffectOfClass(Row, UGE_TV_TrapZone::StaticClass()))
	{
		if (ATVGroundEffectActor* Zone = World->SpawnActor<ATVGroundEffectActor>(ATVGroundEffectActor::StaticClass(), Location, FRotator::ZeroRotator, Params))
		{
			Zone->InitTrap(Avatar, Row, Radius, Trap->Duration > 0.f ? Trap->Duration : 30.f, FMath::Max(0.f, Trap->Magnitude), Magnitude);
		}
		return;
	}

	TArray<AActor*> Targets;
	UTVCombatLibrary::GetHostilesInRadius(Avatar, Location, Radius, Targets);
	const bool bMob = IsMob(Avatar);
	for (AActor* Target : Targets)
	{
		UTVCombatLibrary::DealDamage(Avatar, Target, Magnitude, Row.ThreatMultiplier, !bMob);
	}
	ApplyEffectsToTargets(Avatar, Targets, Row, Magnitude);

	if (const FTVEffectSpec* Pool = UTVCombatLibrary::FindEffectOfClass(Row, UGE_TV_GroundZoneDot::StaticClass()))
	{
		// Mobs: Magnitude x hit damage per second (Crownfire 80 on impact + 64/s). Players: x ability magnitude.
		const float PerTick = Pool->Magnitude * (bMob ? UTVCombatLibrary::GetHitDamage(Avatar) : Magnitude);
		if (ATVGroundEffectActor* Zone = World->SpawnActor<ATVGroundEffectActor>(ATVGroundEffectActor::StaticClass(), Location, FRotator::ZeroRotator, Params))
		{
			Zone->InitPool(Avatar, Row, Radius, Pool->Duration > 0.f ? Pool->Duration : 8.f, PerTick);
		}
	}
}

// =====================================================================================================================
// Channel
// =====================================================================================================================

TSubclassOf<UTVGameplayAbility> UGA_TV_Channel::GetAIVariantClass() const { return UGA_TV_Channel_AI::StaticClass(); }

void UGA_TV_Channel::ExecuteAbility()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FTVEffectSpec* TickSpec = UTVCombatLibrary::FindEffectOfClass(Row, UGE_TV_ChannelTick::StaticClass());
	const float Interval = FMath::Max(0.1f, (TickSpec && TickSpec->Magnitude > 0.f) ? TickSpec->Magnitude : 1.f);
	const float Duration = Row.ChannelTime > 0.f ? Row.ChannelTime : ((TickSpec && TickSpec->Duration > 0.f) ? TickSpec->Duration : 3.f);
	TicksDone = 0;
	TicksTotal = FMath::Max(1, FMath::FloorToInt(Duration / Interval + KINDA_SMALL_NUMBER));

	StartCast(Duration, true);
	if (IsAuthority())
	{
		World->GetTimerManager().SetTimer(TickTimerHandle, this, &UGA_TV_Channel::ChannelTick, Interval, true);
	}
	World->GetTimerManager().SetTimer(EndTimerHandle, this, &UGA_TV_Channel::ChannelFinished, Duration + 0.01f, false);
}

void UGA_TV_Channel::ChannelTick()
{
	AActor* Avatar = GetAvatar();
	if (!IsAuthority() || !Avatar || TicksDone >= TicksTotal)
	{
		return;
	}
	++TicksDone;
	TArray<AActor*> Targets;
	UTVCombatLibrary::GetHostilesInRadius(Avatar, LockedGroundLocation, Row.RadiusCm > 0.f ? Row.RadiusCm : 500.f, Targets);
	const float Magnitude = GetEffectiveMagnitude();
	for (AActor* Target : Targets)
	{
		UTVCombatLibrary::DealDamage(Avatar, Target, Magnitude, Row.ThreatMultiplier, !IsMob(Avatar));
	}
	if (UTVAbilitySystemComponent* ASC = GetTVASC())
	{
		ASC->NotifyCombatActivity();
	}
}

void UGA_TV_Channel::ChannelFinished()
{
	if (IsAuthority())
	{
		while (TicksDone < TicksTotal)
		{
			ChannelTick();
		}
	}
	bChanneling = false;
	FinishAbility(false);
}

// =====================================================================================================================
// Taunt
// =====================================================================================================================

TSubclassOf<UTVGameplayAbility> UGA_TV_Taunt::GetAIVariantClass() const { return UGA_TV_Taunt_AI::StaticClass(); }

void UGA_TV_Taunt::ExecuteAbility()
{
	AActor* Avatar = GetAvatar();
	if (!IsAuthority() || !Avatar)
	{
		return;
	}
	TArray<AActor*> Targets;
	if (AActor* Target = GetLockedTarget())
	{
		Targets.Add(Target);
	}
	if (Row.RadiusCm > 0.f)
	{
		UTVCombatLibrary::GetHostilesInRadius(Avatar, Avatar->GetActorLocation(), Row.RadiusCm, Targets);
	}
	if (Targets.Num() == 0)
	{
		return;
	}
	ApplyEffectsToTargets(Avatar, Targets, Row, GetEffectiveMagnitude());
	if (UTVAbilitySystemComponent* ASC = GetTVASC())
	{
		ASC->NotifyCombatActivity();
	}
}

// =====================================================================================================================
// Leap
// =====================================================================================================================

TSubclassOf<UTVGameplayAbility> UGA_TV_Leap::GetAIVariantClass() const { return UGA_TV_Leap_AI::StaticClass(); }

void UGA_TV_Leap::ExecuteAbility()
{
	ACharacter* Character = Cast<ACharacter>(GetAvatar());
	UWorld* World = GetWorld();
	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	if (!Character || !World || !Info)
	{
		FinishAbility(true);
		return;
	}
	// Movement is launched where it is simulated authoritatively or predicted: the server and the owning client.
	const bool bShouldLaunch = Info->IsNetAuthority() || Info->IsLocallyControlled();

	if (Row.TargetType == ETVTargetType::Enemy)
	{
		AActor* Target = GetLockedTarget();
		if (!Target)
		{
			FinishAbility(true);
			return;
		}
		const FVector ToTarget = Target->GetActorLocation() - Character->GetActorLocation();
		const float Distance = FMath::Max(0.f, ToTarget.Size2D() - CapsuleRadius(Character) - CapsuleRadius(Target) - 50.f);
		const float Speed = Distance / FMath::Max(0.1f, ChargeImpactDelay);
		if (bShouldLaunch)
		{
			Character->LaunchCharacter(ToTarget.GetSafeNormal2D() * Speed + FVector(0.f, 0.f, 50.f), true, true);
		}
		World->GetTimerManager().SetTimer(LandTimerHandle, this, &UGA_TV_Leap::OnLeapLanded, FMath::Max(0.1f, ChargeImpactDelay), false);
		return;
	}

	// Self: jump backwards RangeCm.
	const float UpSpeed = 350.f;
	const float AirTime = 2.f * UpSpeed / 980.f;
	const FVector Back = -Character->GetActorForwardVector().GetSafeNormal2D();
	if (bShouldLaunch)
	{
		Character->LaunchCharacter(Back * (FMath::Max(0.f, Row.RangeCm) / AirTime) + FVector(0.f, 0.f, UpSpeed), true, true);
	}
	if (IsAuthority())
	{
		TArray<AActor*> Self;
		Self.Add(Character);
		ApplyEffectsToTargets(Character, Self, Row, GetEffectiveMagnitude());
	}
	World->GetTimerManager().SetTimer(LandTimerHandle, this, &UGA_TV_Leap::OnLeapLanded, AirTime, false);
}

void UGA_TV_Leap::OnLeapLanded()
{
	ACharacter* Character = Cast<ACharacter>(GetAvatar());
	if (IsAuthority() && Character && Row.TargetType == ETVTargetType::Enemy)
	{
		if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
		{
			Move->StopMovementImmediately();
		}
		AActor* Target = GetLockedTarget();
		const float Reach = 250.f + CapsuleRadius(Character) + CapsuleRadius(Target);
		if (UTVCombatLibrary::IsAlive(Target) && UTVCombatLibrary::DistanceBetween(Character, Target) <= Reach)
		{
			const float Magnitude = GetEffectiveMagnitude();
			UTVCombatLibrary::DealDamage(Character, Target, Magnitude, Row.ThreatMultiplier, !IsMob(Character));
			TArray<AActor*> Targets;
			Targets.Add(Target);
			ApplyEffectsToTargets(Character, Targets, Row, Magnitude);
		}
	}
	FinishAbility(false);
}

// =====================================================================================================================
// SummonMount
// =====================================================================================================================

UGA_TV_SummonMount::UGA_TV_SummonMount()
{
	bCancelCastOnDamage = true;
}

void UGA_TV_SummonMount::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const ATVPlayerState* PS = ActorInfo ? Cast<ATVPlayerState>(ActorInfo->OwnerActor.Get()) : nullptr;
	UTVMountComponent* Mount = PS ? PS->GetMount() : nullptr;
	if (Mount && Mount->IsMounted())
	{
		// Toggle: the mount key while mounted dismounts at once.
		if (ActorInfo->IsNetAuthority())
		{
			Mount->ServerSetMounted(false);
		}
		EndAbility(Handle, ActorInfo, ActivationInfo, ActorInfo->IsNetAuthority(), false);
		return;
	}
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UGA_TV_SummonMount::ExecuteAbility()
{
	if (!IsAuthority())
	{
		return;
	}
	const ATVPlayerState* PS = Cast<ATVPlayerState>(GetOwningActorFromActorInfo());
	if (UTVMountComponent* Mount = PS ? PS->GetMount() : nullptr)
	{
		Mount->ServerSetMounted(true);
	}
}

// =====================================================================================================================
// Cone
// =====================================================================================================================

TSubclassOf<UTVGameplayAbility> UGA_TV_Cone::GetAIVariantClass() const { return UGA_TV_Cone_AI::StaticClass(); }

void UGA_TV_Cone::ExecuteAbility()
{
	AActor* Avatar = GetAvatar();
	if (!IsAuthority() || !Avatar)
	{
		return;
	}
	TArray<AActor*> Nearby;
	UTVCombatLibrary::GetHostilesInRadius(Avatar, Avatar->GetActorLocation(), Row.RangeCm + CapsuleRadius(Avatar), Nearby);
	TArray<AActor*> Targets;
	for (AActor* Other : Nearby)
	{
		if (UTVCombatLibrary::IsInFrontOf(Avatar, Other, ConeHalfAngleDeg))
		{
			Targets.Add(Other);
		}
	}
	const float Magnitude = GetEffectiveMagnitude();
	for (AActor* Target : Targets)
	{
		UTVCombatLibrary::DealDamage(Avatar, Target, Magnitude, Row.ThreatMultiplier, !IsMob(Avatar));
	}
	ApplyEffectsToTargets(Avatar, Targets, Row, Magnitude);
}

// =====================================================================================================================
// Server-only twins
// =====================================================================================================================

// VERIFY-5.8: NetExecutionPolicy written directly (GameplayAbility.h not staged).
UGA_TV_DirectDamage_AI::UGA_TV_DirectDamage_AI() { NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly; }
UGA_TV_Heal_AI::UGA_TV_Heal_AI() { NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly; }
UGA_TV_ApplyEffect_AI::UGA_TV_ApplyEffect_AI() { NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly; }
UGA_TV_GroundAoE_AI::UGA_TV_GroundAoE_AI() { NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly; }
UGA_TV_Channel_AI::UGA_TV_Channel_AI() { NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly; }
UGA_TV_Taunt_AI::UGA_TV_Taunt_AI() { NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly; }
UGA_TV_Leap_AI::UGA_TV_Leap_AI() { NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly; }
UGA_TV_Cone_AI::UGA_TV_Cone_AI() { NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly; }
