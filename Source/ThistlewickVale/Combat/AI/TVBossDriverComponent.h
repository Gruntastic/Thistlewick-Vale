// Runs a UTVBossDataAsset on a mob: phases by health, rotation timers, on-enter abilities, health-triggered add spawns
// and the enrage timer. Server only; reset when the boss evades.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TVBossDriverComponent.generated.h"

class UTVBossDataAsset;
class ATVMobCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTVOnBossPhaseChanged, ATVMobCharacter*, Boss, int32, PhaseIndex);

UCLASS(ClassGroup = (TV))
class THISTLEWICKVALE_API UTVBossDriverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTVBossDriverComponent();

	void SetBossData(UTVBossDataAsset* InData);
	const UTVBossDataAsset* GetBossData() const { return BossData; }

	/** Called by the mob AI each think while in combat. */
	void TickBoss(float DeltaSeconds);
	void StartEncounter();
	void ResetEncounter();
	bool IsEncounterActive() const { return bActive; }
	int32 GetPhaseIndex() const { return PhaseIndex; }
	/** Players inside ArenaRadius when the encounter started (pillar cap). */
	int32 GetEngagedPartySize() const { return FMath::Max(1, PartySizeAtPull); }
	/** Mobs spawned by this encounter (despawned on reset). */
	void RegisterSpawnedAdd(AActor* Add);

	UPROPERTY(BlueprintAssignable, Category = "TV|Boss") FTVOnBossPhaseChanged OnPhaseChanged;

protected:
	void EnterPhase(int32 NewPhase);
	ATVMobCharacter* GetBoss() const;
	float GetHealthFraction() const;

	UPROPERTY() TObjectPtr<UTVBossDataAsset> BossData;
	UPROPERTY() TArray<TWeakObjectPtr<AActor>> SpawnedAdds;

	bool bActive = false;
	bool bEnraged = false;
	int32 PhaseIndex = -1;
	int32 PartySizeAtPull = 1;
	float EncounterTime = 0.f;
	/** Next cast time (encounter seconds) per rotation entry of the current phase. */
	TArray<float> NextCastTimes;
	TArray<FName> PendingOnEnter;
	TSet<int32> FiredAddSpawns;
};
