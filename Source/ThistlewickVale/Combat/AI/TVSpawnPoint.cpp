#include "Combat/AI/TVSpawnPoint.h"
#include "ThistlewickVale.h"
#include "Combat/TVCharacterBase.h"
#include "Combat/TVCombatLibrary.h"
#include "Combat/TVBossDataAsset.h"
#include "Components/SceneComponent.h"
#include "Components/BillboardComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "CollisionQueryParams.h"

ATVSpawnPoint::ATVSpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

#if WITH_EDITORONLY_DATA
	Billboard = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
	if (Billboard)
	{
		Billboard->SetupAttachment(Root);
	}
#endif
}

void ATVSpawnPoint::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && bSpawnOnBeginPlay)
	{
		bActiveSpawning = true;
		SpawnNow();
	}
}

void ATVSpawnPoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnTimer);
	}
	Super::EndPlay(EndPlayReason);
}

void ATVSpawnPoint::SetScaling(float Health, float Damage)
{
	HealthScale = FMath::Max(0.01f, Health);
	DamageScale = FMath::Max(0.f, Damage);
	for (const TWeakObjectPtr<ATVMobCharacter>& Weak : SpawnedMobs)
	{
		if (ATVMobCharacter* Mob = Weak.Get())
		{
			if (!Mob->IsDead())
			{
				Mob->ApplyScaling(HealthScale, DamageScale);
			}
		}
	}
}

int32 ATVSpawnPoint::GetNumAlive() const
{
	int32 Alive = 0;
	for (const TWeakObjectPtr<ATVMobCharacter>& Weak : SpawnedMobs)
	{
		const ATVMobCharacter* Mob = Weak.Get();
		if (Mob && !Mob->IsDead())
		{
			++Alive;
		}
	}
	return Alive;
}

void ATVSpawnPoint::SpawnNow()
{
	if (!HasAuthority())
	{
		return;
	}
	SpawnedMobs.RemoveAll([](const TWeakObjectPtr<ATVMobCharacter>& Weak) { return !Weak.IsValid() || Weak->IsDead(); });
	// Topping up replaces any pending respawns.
	RespawnDueTimes.Reset();
	PendingRespawns = 0;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnTimer);
	}
	const int32 ToSpawn = FMath::Max(0, Count - GetNumAlive());
	for (int32 i = 0; i < ToSpawn; ++i)
	{
		SpawnOne();
	}
}

ATVMobCharacter* ATVSpawnPoint::SpawnOne()
{
	UWorld* World = GetWorld();
	const FTVMobRow* Row = UTVCombatLibrary::FindMobRow(MobId);
	if (!World || !Row)
	{
		UE_LOG(LogTV, Warning, TEXT("Spawn point %s: mob row '%s' not found"), *GetName(), *MobId.ToString());
		return nullptr;
	}
	const int32 SpawnLevel = Level > 0 ? Level : FMath::RandRange(Row->LevelMin, FMath::Max(Row->LevelMin, Row->LevelMax));

	// Random point in the radius, dropped onto the ground.
	const FVector2D Offset = FMath::RandPointInCircle(FMath::Max(0.f, SpawnRadiusCm));
	FVector Location = GetActorLocation() + FVector(Offset.X, Offset.Y, 0.f);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TVSpawnGround), false, this);
	if (World->LineTraceSingleByObjectType(Hit, Location + FVector(0.f, 0.f, 300.f), Location - FVector(0.f, 0.f, 1000.f),
		FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		Location = Hit.ImpactPoint + FVector(0.f, 0.f, 100.f);
	}
	const FTransform Where(GetActorRotation(), Location);

	ATVMobCharacter* Mob = ATVMobCharacter::SpawnMob(World, MobClass, MobId, SpawnLevel, Where, HealthScale, DamageScale, GetLevel());
	if (!Mob)
	{
		return nullptr;
	}
	Mob->SetOwningSpawnPoint(this);
	Mob->SetHomeLocation(Location);
	if (PatrolPath.Num() > 0)
	{
		TArray<FVector> WorldPoints;
		WorldPoints.Add(Location);
		for (const FVector& Local : PatrolPath)
		{
			WorldPoints.Add(GetActorTransform().TransformPosition(Local));
		}
		Mob->SetPatrolPoints(WorldPoints);
	}
	if (!BossData.IsNull())
	{
		if (UTVBossDataAsset* Data = BossData.LoadSynchronous())
		{
			if (!Data->BossMobId.IsNone() && Data->BossMobId != MobId)
			{
				UE_LOG(LogTV, Warning, TEXT("Spawn point %s: boss data %s is for %s, not %s"), *GetName(), *Data->GetName(),
					*Data->BossMobId.ToString(), *MobId.ToString());
			}
			Mob->SetBossData(Data);
		}
	}
	SpawnedMobs.Add(Mob);
	return Mob;
}

void ATVSpawnPoint::SetActiveSpawning(bool bActive)
{
	bActiveSpawning = bActive;
	if (!bActive)
	{
		RespawnDueTimes.Reset();
		PendingRespawns = 0;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(RespawnTimer);
		}
	}
}

void ATVSpawnPoint::DespawnAll()
{
	for (const TWeakObjectPtr<ATVMobCharacter>& Weak : SpawnedMobs)
	{
		if (ATVMobCharacter* Mob = Weak.Get())
		{
			Mob->Destroy();
		}
	}
	SpawnedMobs.Reset();
	RespawnDueTimes.Reset();
	PendingRespawns = 0;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnTimer);
	}
}

void ATVSpawnPoint::NotifyMobDied(ATVMobCharacter* Mob)
{
	SpawnedMobs.RemoveAll([Mob](const TWeakObjectPtr<ATVMobCharacter>& Weak) { return !Weak.IsValid() || Weak.Get() == Mob; });
	UWorld* World = GetWorld();
	if (!World || !bActiveSpawning || RespawnTime <= 0.f)
	{
		return;
	}
	RespawnDueTimes.Add(World->GetTimeSeconds() + RespawnTime);
	PendingRespawns = RespawnDueTimes.Num();
	if (!World->GetTimerManager().IsTimerActive(RespawnTimer))
	{
		World->GetTimerManager().SetTimer(RespawnTimer, this, &ATVSpawnPoint::HandleRespawnTimer, RespawnTime, false);
	}
}

void ATVSpawnPoint::HandleRespawnTimer()
{
	UWorld* World = GetWorld();
	if (!World || !bActiveSpawning)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	RespawnDueTimes.Sort();
	while (RespawnDueTimes.Num() > 0 && RespawnDueTimes[0] <= Now + 0.05)
	{
		RespawnDueTimes.RemoveAt(0);
		if (GetNumAlive() < Count)
		{
			SpawnOne();
		}
	}
	PendingRespawns = RespawnDueTimes.Num();
	if (RespawnDueTimes.Num() > 0)
	{
		World->GetTimerManager().SetTimer(RespawnTimer, this, &ATVSpawnPoint::HandleRespawnTimer,
			FMath::Max(0.1f, static_cast<float>(RespawnDueTimes[0] - Now)), false);
	}
}
