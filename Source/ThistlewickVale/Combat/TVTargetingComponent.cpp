#include "Combat/TVTargetingComponent.h"
#include "Combat/TVAbilitySystemComponent.h"
#include "Combat/TVCharacterBase.h"
#include "Combat/TVCombatLibrary.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

UTVTargetingComponent::UTVTargetingComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.25f;
}

void UTVTargetingComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UTVTargetingComponent, CurrentTarget);
}

APawn* UTVTargetingComponent::GetControlledPawn() const
{
	if (const AController* Controller = Cast<AController>(GetOwner()))
	{
		return Controller->GetPawn();
	}
	return Cast<APawn>(GetOwner());
}

bool UTVTargetingComponent::GetViewPoint(FVector& OutLocation, FVector& OutForward) const
{
	if (const APlayerController* PC = Cast<APlayerController>(GetOwner()))
	{
		FRotator Rotation;
		PC->GetPlayerViewPoint(OutLocation, Rotation);
		OutForward = Rotation.Vector();
		return true;
	}
	if (const APawn* Pawn = GetControlledPawn())
	{
		OutLocation = Pawn->GetActorLocation();
		OutForward = Pawn->GetActorForwardVector();
		return true;
	}
	return false;
}

void UTVTargetingComponent::CycleTarget()
{
	APawn* Pawn = GetControlledPawn();
	UWorld* World = GetWorld();
	FVector ViewLocation;
	FVector ViewForward;
	if (!Pawn || !World || !GetViewPoint(ViewLocation, ViewForward))
	{
		return;
	}
	const FVector Origin = Pawn->GetActorLocation();
	const FVector Forward2D = ViewForward.GetSafeNormal2D();
	const float CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(TabConeHalfAngleDeg));

	struct FCandidate
	{
		AActor* Actor = nullptr;
		float Distance = 0.f;
	};
	TArray<FCandidate> Candidates;
	for (TActorIterator<ATVCharacterBase> It(World); It; ++It)
	{
		ATVCharacterBase* Character = *It;
		if (!UTVCombatLibrary::IsAlive(Character) || !UTVCombatLibrary::IsHostile(Pawn, Character))
		{
			continue;
		}
		const FVector ToTarget = Character->GetActorLocation() - Origin;
		const float Distance = ToTarget.Size();
		if (Distance > TabRangeCm)
		{
			continue;
		}
		if (!Forward2D.IsNearlyZero() && FVector::DotProduct(Forward2D, ToTarget.GetSafeNormal2D()) < CosHalfAngle)
		{
			continue;
		}
		FCandidate& Candidate = Candidates.AddDefaulted_GetRef();
		Candidate.Actor = Character;
		Candidate.Distance = Distance;
	}
	if (Candidates.Num() == 0)
	{
		return;
	}
	Candidates.Sort([](const FCandidate& A, const FCandidate& B) { return A.Distance < B.Distance; });

	// Nearest first; pressing again moves to the next one and wraps around.
	int32 NextIndex = 0;
	for (int32 i = 0; i < Candidates.Num(); ++i)
	{
		if (Candidates[i].Actor == CurrentTarget)
		{
			NextIndex = (i + 1) % Candidates.Num();
			break;
		}
	}
	SetTarget(Candidates[NextIndex].Actor);
}

void UTVTargetingComponent::SetTarget(AActor* NewTarget)
{
	ApplyTarget(NewTarget);
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		ServerSetTarget(NewTarget);
	}
}

void UTVTargetingComponent::ServerSetTarget_Implementation(AActor* NewTarget)
{
	const APawn* Pawn = GetControlledPawn();
	if (NewTarget && Pawn && FVector::Dist(NewTarget->GetActorLocation(), Pawn->GetActorLocation()) > MaxKeepRangeCm * 2.f)
	{
		return;
	}
	ApplyTarget(NewTarget);
}

void UTVTargetingComponent::ApplyTarget(AActor* NewTarget)
{
	if (CurrentTarget == NewTarget)
	{
		return;
	}
	CurrentTarget = NewTarget;
	if (UTVAbilitySystemComponent* ASC = UTVCombatLibrary::GetTVASC(GetControlledPawn()))
	{
		ASC->SetAbilityTarget(NewTarget);
	}
	OnTargetChanged.Broadcast(CurrentTarget);
}

void UTVTargetingComponent::OnRep_CurrentTarget()
{
	if (UTVAbilitySystemComponent* ASC = UTVCombatLibrary::GetTVASC(GetControlledPawn()))
	{
		ASC->SetAbilityTarget(CurrentTarget);
	}
	OnTargetChanged.Broadcast(CurrentTarget);
}

void UTVTargetingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AActor* Target = CurrentTarget;
	if (!Target)
	{
		return;
	}
	const APawn* Pawn = GetControlledPawn();
	const bool bGone = !IsValid(Target)
		|| (UTVCombatLibrary::IsHostile(Pawn, Target) && !UTVCombatLibrary::IsAlive(Target))
		|| (Pawn && FVector::Dist(Pawn->GetActorLocation(), Target->GetActorLocation()) > MaxKeepRangeCm);
	if (bGone)
	{
		// Runs on the server and the owning client, so both drop the target without an RPC.
		ApplyTarget(nullptr);
	}
}
