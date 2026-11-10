// DA_Boss_<Name> (Docs/DATA_SCHEMAS.md section 5, plus BossMobId / EnrageAbilityId from the Content Designer's data).
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TVBossDataAsset.generated.h"

USTRUCT(BlueprintType)
struct FTVBossRotationEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Boss") FName AbilityId;
	/** Seconds between casts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Boss") float Interval = 10.f;
	/** Seconds after the phase starts before the first cast. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Boss") float InitialDelay = 5.f;
};

USTRUCT(BlueprintType)
struct FTVBossPhase
{
	GENERATED_BODY()

	/** Phase is active while health fraction <= this (1.0 = from the pull). Phases are entered in descending order. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Boss") float HealthThreshold = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Boss") TArray<FTVBossRotationEntry> AbilityRotation;
	/** Cast once, in order, when the phase starts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Boss") TArray<FName> OnEnterAbilities;
};

USTRUCT(BlueprintType)
struct FTVBossAddSpawn
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Boss") FName MobId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Boss") int32 Count = 1;
	/** Spawned once when boss health fraction first drops to or below this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Boss") float HealthTrigger = 0.5f;
};

UCLASS(BlueprintType)
class THISTLEWICKVALE_API UTVBossDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** DT_Mobs row this asset drives. ATVSpawnPoint attaches the asset to that mob automatically if registered. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Boss") FName BossMobId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Boss") TArray<FTVBossPhase> Phases;
	/** Seconds from the pull; 0 = no enrage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Boss") float EnrageTime = 0.f;
	/** Ability cast on enrage (e.g. ABL_MOB_BOSS_ENRAGE). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Boss") FName EnrageAbilityId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Boss") TArray<FTVBossAddSpawn> AddSpawns;
	/** cm. Players inside count for party size (pillar count) and for leaving the fight. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Boss") float ArenaRadius = 2500.f;
};
