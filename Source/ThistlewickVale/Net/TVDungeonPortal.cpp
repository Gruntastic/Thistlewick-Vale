#include "Net/TVDungeonPortal.h"

#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Combat/TVPlayerState.h"
#include "Net/TVDungeonManager.h"
#include "Net/TVNetPlayerAccess.h"

ATVDungeonPortal::ATVDungeonPortal()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false; // level-placed on both sides; all logic is server-side

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetBoxExtent(FVector(150.0, 150.0, 200.0));
	Trigger->SetCollisionProfileName(TEXT("Trigger"));
	Trigger->SetGenerateOverlapEvents(true);
	RootComponent = Trigger;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Trigger);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ArrivalPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("ArrivalPoint"));
	ArrivalPoint->SetupAttachment(Trigger);
	ArrivalPoint->SetRelativeLocation(FVector(400.0, 0.0, 0.0));
}

void ATVDungeonPortal::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		Trigger->OnComponentBeginOverlap.AddDynamic(this, &ATVDungeonPortal::OnTriggerBeginOverlap);
	}
}

FTransform ATVDungeonPortal::GetArrivalTransform() const
{
	return FTransform(ArrivalPoint->GetComponentRotation(), ArrivalPoint->GetComponentLocation() + FVector(0.0, 0.0, 100.0));
}

void ATVDungeonPortal::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (Pawn && Pawn->IsPlayerControlled())
	{
		Use(Pawn->GetPlayerState<ATVPlayerState>());
	}
}

void ATVDungeonPortal::Use(ATVPlayerState* Player)
{
	if (!HasAuthority() || !Player || TVNet::IsPlayerDead(Player))
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (const double* Last = LastUse.Find(Player))
	{
		if (Now - *Last < ReuseCooldown)
		{
			return;
		}
	}
	LastUse.Add(Player, Now);

	ATVDungeonManager* Dungeons = ATVDungeonManager::Get(this);
	if (!Dungeons)
	{
		return;
	}
	if (bIsExit)
	{
		Dungeons->LeaveDungeon(Player);
	}
	else
	{
		Dungeons->EnterDungeon(Player);
	}
}
