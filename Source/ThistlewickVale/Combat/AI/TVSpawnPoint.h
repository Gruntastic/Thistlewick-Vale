// Level-placed mob spawner (BP_TVSpawnPoint). Data only: MobId from DT_Mobs, no hard-coded mob classes.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TVSpawnPoint.generated.h"

class ATVMobCharacter;
class UTVBossDataAsset;
class UBillboardComponent;

UCLASS(Blueprintable)
class THISTLEWICKVALE_API ATVSpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	ATVSpawnPoint();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Spawn") FName MobId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Spawn", meta = (ClampMin = "1")) int32 Count = 1;
	/** Seconds after a death before that mob respawns (GDD: 60 s, quest mobs 30 s). <= 0: never. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Spawn") float RespawnTime = 60.f;
	/** 0 = random in the row's LevelMin..LevelMax. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Spawn") int32 Level = 0;
	/** Mobs are placed randomly inside this radius. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Spawn") float SpawnRadiusCm = 300.f;
	/** Local-space patrol points (shown as widgets). Empty = stand still. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Spawn", meta = (MakeEditWidget = "true")) TArray<FVector> PatrolPath;
	/** Spawn on BeginPlay. Dungeon spawn points can leave this off and let ATVDungeonManager call SetActiveSpawning. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Spawn") bool bSpawnOnBeginPlay = true;
	/** Optional boss script for BT_TV_BossData mobs (DA_Boss_<Name>). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Spawn") TSoftObjectPtr<UTVBossDataAsset> BossData;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Spawn") TSubclassOf<ATVMobCharacter> MobClass;

	/** Health / damage multipliers for future and currently living mobs (party scaling). */
	void SetScaling(float Health, float Damage);
	/** Tops up to Count living mobs now (idempotent). */
	void SpawnNow();
	/** Enables/disables spawning and respawning. Disabling does not despawn living mobs. */
	void SetActiveSpawning(bool bActive);
	/** Destroys every mob this point spawned. */
	void DespawnAll();

	/** Called by ATVMobCharacter when it dies. */
	void NotifyMobDied(ATVMobCharacter* Mob);
	int32 GetNumAlive() const;

protected:
	ATVMobCharacter* SpawnOne();
	void HandleRespawnTimer();

	UPROPERTY(VisibleAnywhere, Category = "TV|Spawn") TObjectPtr<USceneComponent> Root;
#if WITH_EDITORONLY_DATA
	UPROPERTY() TObjectPtr<UBillboardComponent> Billboard;
#endif

	UPROPERTY() TArray<TWeakObjectPtr<ATVMobCharacter>> SpawnedMobs;
	float HealthScale = 1.f;
	float DamageScale = 1.f;
	bool bActiveSpawning = false;
	int32 PendingRespawns = 0;
	FTimerHandle RespawnTimer;
	TArray<double> RespawnDueTimes;
};
