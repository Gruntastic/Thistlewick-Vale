#include "Combat/Abilities/TVGroundEffectActor.h"
#include "Combat/TVCombatLibrary.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

ATVGroundEffectActor::ATVGroundEffectActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ATVGroundEffectActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATVGroundEffectActor, Mode);
	DOREPLIFETIME(ATVGroundEffectActor, Radius);
	DOREPLIFETIME(ATVGroundEffectActor, VfxId);
	DOREPLIFETIME(ATVGroundEffectActor, bArmed);
}

void ATVGroundEffectActor::InitTrap(AActor* InSource, const FTVAbilityRow& InRow, float InRadius, float InLifetime, float InArmTime, float InAbilityMagnitude)
{
	if (!HasAuthority())
	{
		return;
	}
	Source = InSource;
	Row = InRow;
	Mode = ETVGroundEffectMode::Trap;
	Radius = FMath::Max(50.f, InRadius);
	VfxId = !InRow.VfxImpactId.IsNone() ? InRow.VfxImpactId : InRow.VfxCastId;
	AbilityMagnitude = InAbilityMagnitude;
	bArmed = false;
	SetLifeSpan(FMath::Max(1.f, InLifetime));
	if (UWorld* World = GetWorld())
	{
		if (InArmTime > 0.f)
		{
			World->GetTimerManager().SetTimer(ArmTimer, this, &ATVGroundEffectActor::Arm, InArmTime, false);
		}
		else
		{
			Arm();
		}
		World->GetTimerManager().SetTimer(TickTimer, this, &ATVGroundEffectActor::ServerTick, 0.2f, true);
	}
	OnRep_Visuals();
}

void ATVGroundEffectActor::InitPool(AActor* InSource, const FTVAbilityRow& InRow, float InRadius, float InLifetime, float InTickDamage)
{
	if (!HasAuthority())
	{
		return;
	}
	Source = InSource;
	Row = InRow;
	Mode = ETVGroundEffectMode::Pool;
	Radius = FMath::Max(50.f, InRadius);
	VfxId = !InRow.VfxImpactId.IsNone() ? InRow.VfxImpactId : InRow.VfxCastId;
	TickDamage = FMath::Max(0.f, InTickDamage);
	bArmed = true;
	SetLifeSpan(FMath::Max(1.f, InLifetime));
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(TickTimer, this, &ATVGroundEffectActor::ServerTick, 1.f, true);
	}
	OnRep_Visuals();
}

void ATVGroundEffectActor::Arm()
{
	bArmed = true;
	OnRep_Visuals();
}

void ATVGroundEffectActor::ServerTick()
{
	AActor* Caster = Source.Get();
	if (!Caster || !UTVCombatLibrary::IsAlive(Caster))
	{
		// A pool or trap does nothing without its living caster (threat and hostility come from it).
		Destroy();
		return;
	}
	if (!bArmed)
	{
		return;
	}
	TArray<AActor*> Hostiles;
	UTVCombatLibrary::GetHostilesInRadius(Caster, GetActorLocation(), Radius, Hostiles);
	if (Mode == ETVGroundEffectMode::Trap)
	{
		if (Hostiles.Num() > 0)
		{
			// The first hostile to step in gets every applied effect of the row (Snare Trap: unbreakable root).
			UTVCombatLibrary::ApplyRowEffects(Caster, Hostiles[0], Row, AbilityMagnitude);
			Destroy();
		}
		return;
	}
	for (AActor* Target : Hostiles)
	{
		UTVCombatLibrary::DealDamage(Caster, Target, TickDamage, Row.ThreatMultiplier, false);
	}
}

void ATVGroundEffectActor::OnRep_Visuals()
{
	OnZoneVisualsChanged();
}
