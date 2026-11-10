// Per-party runtime instances of L_Barrow on one dedicated server.
//
// How it works (API checked against 5.8.3 LevelStreamingDynamic.h / LevelStreaming.cpp / NetConnection.h):
//  * The server calls ULevelStreamingDynamic::LoadLevelInstance(FLoadLevelInstanceParams) with a unique
//    OptionalLevelNameOverride ("L_Barrow_TVInst<N>") at a far offset. The loaded package is
//    "<dir of L_Barrow>/L_Barrow_TVInst<N>" (GetLevelInstancePackageName).
//  * Dynamically created streaming levels are NOT replicated to clients. Each entering member receives
//    ATVPlayerController::ClientLoadDungeonInstance and loads the same instance locally with the same name override
//    (the header says this is required for networking). The client then reports the level visible
//    (APlayerController::ServerUpdateLevelVisibility, engine-internal), which adds the package to
//    UNetConnection::ClientVisibleLevelNames. Legacy replication only replicates actors of a streamed level to
//    connections that have that level visible, so other parties never receive this instance's level actors.
//  * The server teleports a member only after its connection reports the level visible (or after a timeout).
//  * Pawns and actors spawned into the persistent level are separated by distance instead: instances sit
//    InstanceSpacing (5 km) apart and InstanceDepth below the zone, far outside the default 150 m pawn cull distance.
//  * LIMITATION: every member's client has to load the instance too (one extra streamed level per client, its own
//    party's only). A client never loads other parties' instances.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TVDungeonManager.generated.h"

class ATVPlayerState;
class ATVSpawnPoint;
class ULevelStreamingDynamic;
class ULevel;

USTRUCT()
struct FTVDungeonInstance
{
	GENERATED_BODY()

	int32 InstanceId = 0;
	int32 Slot = 0;
	int32 PartyKey = 0;
	FString InstanceName;
	FVector Origin = FVector::ZeroVector;
	FName LoadedPackageName;

	UPROPERTY()
	TObjectPtr<ULevelStreamingDynamic> Streaming = nullptr;

	bool bReady = false;
	int32 ScaledForPartySize = 0;
	int32 DesiredPartySize = 1;
	bool bSpawnedOnce = false;
	FTransform EntranceTransform = FTransform::Identity;

	TArray<TWeakObjectPtr<ATVPlayerState>> Inside;
	double EmptySince = -1.0;
	double WipeSince = -1.0;
	double CreatedAt = 0.0;
};

USTRUCT()
struct FTVPendingDungeonEntry
{
	GENERATED_BODY()

	TWeakObjectPtr<ATVPlayerState> Player;
	int32 InstanceId = 0;
	double RequestedAt = 0.0;
	bool bClientLoadSent = false;
};

UCLASS(NotPlaceable)
class THISTLEWICKVALE_API ATVDungeonManager : public AActor
{
	GENERATED_BODY()

public:
	ATVDungeonManager();

	/** Server-side singleton (spawned by ATVGameMode::StartPlay). Null on clients. */
	static ATVDungeonManager* Get(const UObject* WorldContext);

	/** GDD 7: health x (0.3 + 0.175 n), damage x (0.5 + 0.1 n), both capped at 1.0 (n = 5). */
	static void GetPartyScaling(int32 PartySize, float& OutHealth, float& OutDamage);

	/** Enters Leader's party instance (created on demand). Takes Leader plus party members within GatherRadius. */
	void EnterDungeon(ATVPlayerState* Leader);

	/** Teleports the player out to the world-side portal and unloads the instance on their client. */
	void LeaveDungeon(ATVPlayerState* Player);

	bool IsInsideDungeon(const ATVPlayerState* Player) const;

	/** Release point inside the player's instance. False when not inside one. */
	bool GetEntranceTransformFor(const ATVPlayerState* Player, FTransform& OutTransform) const;

	/** World-side return point (entrance portal arrival point, else the bind stone). */
	FTransform GetWorldReturnTransform() const;

	/** True for locations in the instance band (below the zone); used to reject saved locations inside a dungeon. */
	bool IsLocationInInstanceSpace(const FVector& Location) const;

	/** Party subsystem hook: membership changed while the player may be inside an instance. */
	void HandlePartyKeyChanged(ATVPlayerState* Player, int32 OldKey, int32 NewKey);

	/** GameMode hook: moves the pawn out (so the save has a world location) and forgets the player. */
	void HandleLogout(ATVPlayerState* Player);

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Long package name of the dungeon map. */
	UPROPERTY(EditAnywhere, Category = "Dungeon")
	FString DungeonLevelPackage = TEXT("/Game/MMO/Maps/L_Barrow");

	/** Instance n is placed at FirstInstanceOrigin + n * InstanceSpacing on X. */
	UPROPERTY(EditAnywhere, Category = "Dungeon")
	FVector FirstInstanceOrigin = FVector(600000.0, 0.0, -200000.0);

	UPROPERTY(EditAnywhere, Category = "Dungeon")
	double InstanceSpacing = 500000.0;

	/** Z below which a location is considered instance space. */
	UPROPERTY(EditAnywhere, Category = "Dungeon")
	double InstanceSpaceZ = -100000.0;

	/** Half extent used to find mobs belonging to an instance during a reset. */
	UPROPERTY(EditAnywhere, Category = "Dungeon")
	double InstanceRadius = 200000.0;

	UPROPERTY(EditAnywhere, Category = "Dungeon")
	float UnloadDelaySeconds = 60.f;

	UPROPERTY(EditAnywhere, Category = "Dungeon")
	float GatherRadius = 3000.f;

	UPROPERTY(EditAnywhere, Category = "Dungeon")
	float ClientLoadTimeoutSeconds = 20.f;

	UPROPERTY(EditAnywhere, Category = "Dungeon")
	float WipeResetDelaySeconds = 5.f;

	/** Tag of the actor in L_Barrow that marks the entrance / release point (a TargetPoint). */
	UPROPERTY(EditAnywhere, Category = "Dungeon")
	FName EntranceTag = TEXT("TV_DungeonEntrance");

private:
	FTVDungeonInstance* FindInstanceByKey(int32 PartyKey);
	FTVDungeonInstance* FindInstanceOf(const ATVPlayerState* Player);
	const FTVDungeonInstance* FindInstanceOf(const ATVPlayerState* Player) const;
	FTVDungeonInstance* CreateInstance(int32 PartyKey);
	void UnloadInstance(int32 InstanceId);
	void OnInstanceReady(FTVDungeonInstance& Instance);
	void ApplyScaling(FTVDungeonInstance& Instance, int32 PartySize);
	void ResetInstance(FTVDungeonInstance& Instance);
	void DestroyInstanceMobs(const FTVDungeonInstance& Instance);
	void GetSpawnPoints(const FTVDungeonInstance& Instance, TArray<ATVSpawnPoint*>& Out) const;
	bool HasClientLoaded(const ATVPlayerState* Player, const FTVDungeonInstance& Instance) const;
	void TeleportInto(ATVPlayerState* Player, FTVDungeonInstance& Instance);
	void TickPendingEntries();
	void TickInstances();
	int32 AllocateSlot() const;
	void QueueEntry(ATVPlayerState* Player, FTVDungeonInstance& Instance);

	UPROPERTY(Transient)
	TMap<int32, FTVDungeonInstance> Instances;

	UPROPERTY(Transient)
	TArray<FTVPendingDungeonEntry> PendingEntries;

	int32 NextInstanceId = 1;
};
