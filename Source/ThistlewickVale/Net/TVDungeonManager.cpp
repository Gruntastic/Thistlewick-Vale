#include "Net/TVDungeonManager.h"

#include "ThistlewickVale.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/NetConnection.h"
#include "GameFramework/Pawn.h"
#include "Misc/PackageName.h"
#include "Combat/TVPlayerState.h"
#include "Combat/TVCharacterBase.h"
#include "Combat/AI/TVSpawnPoint.h"
#include "Systems/TVMountComponent.h"
#include "Net/TVGameMode.h"
#include "Net/TVPlayerController.h"
#include "Net/TVPartySubsystem.h"
#include "Net/TVDungeonPortal.h"
#include "Net/TVNetPlayerAccess.h"
#include "Net/TVNetTypes.h"

// INTERFACE-REQUEST (Combat, ATVSpawnPoint): please spawn mobs with FActorSpawnParameters::OverrideLevel = GetLevel(),
// so mobs live in the instance's streamed level (removed with it, replicated only to clients that loaded it).
// Please also make SpawnNow() top up to Count (idempotent) and SetScaling() apply to already living mobs.
// The manager destroys leftover ATVMobCharacters inside an instance's volume on reset/unload as a fallback.

namespace
{
	ATVPlayerController* GetTVPC(const ATVPlayerState* PS)
	{
		return PS ? Cast<ATVPlayerController>(PS->GetPlayerController()) : nullptr;
	}

	ATVGameMode* GetTVGameMode(const UObject* Context)
	{
		const UWorld* World = Context ? Context->GetWorld() : nullptr;
		return World ? World->GetAuthGameMode<ATVGameMode>() : nullptr;
	}
}

ATVDungeonManager::ATVDungeonManager()
{
	bReplicates = false;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;
	SetHidden(true);
}

ATVDungeonManager* ATVDungeonManager::Get(const UObject* WorldContext)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}
	for (TActorIterator<ATVDungeonManager> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void ATVDungeonManager::GetPartyScaling(int32 PartySize, float& OutHealth, float& OutDamage)
{
	const int32 N = FMath::Clamp(PartySize, 1, 5);
	OutHealth = FMath::Min(1.f, 0.3f + 0.175f * N);
	OutDamage = FMath::Min(1.f, 0.5f + 0.1f * N);
}

// ------------------------------------------------------------------------------------------------ lookups

FTVDungeonInstance* ATVDungeonManager::FindInstanceByKey(int32 PartyKey)
{
	for (TPair<int32, FTVDungeonInstance>& Pair : Instances)
	{
		if (Pair.Value.PartyKey == PartyKey)
		{
			return &Pair.Value;
		}
	}
	return nullptr;
}

FTVDungeonInstance* ATVDungeonManager::FindInstanceOf(const ATVPlayerState* Player)
{
	return const_cast<FTVDungeonInstance*>(static_cast<const ATVDungeonManager*>(this)->FindInstanceOf(Player));
}

const FTVDungeonInstance* ATVDungeonManager::FindInstanceOf(const ATVPlayerState* Player) const
{
	if (!Player)
	{
		return nullptr;
	}
	for (const TPair<int32, FTVDungeonInstance>& Pair : Instances)
	{
		for (const TWeakObjectPtr<ATVPlayerState>& Inside : Pair.Value.Inside)
		{
			if (Inside.Get() == Player)
			{
				return &Pair.Value;
			}
		}
	}
	return nullptr;
}

bool ATVDungeonManager::IsInsideDungeon(const ATVPlayerState* Player) const
{
	return FindInstanceOf(Player) != nullptr;
}

bool ATVDungeonManager::GetEntranceTransformFor(const ATVPlayerState* Player, FTransform& OutTransform) const
{
	const FTVDungeonInstance* Instance = FindInstanceOf(Player);
	if (!Instance || !Instance->bReady)
	{
		return false;
	}
	OutTransform = Instance->EntranceTransform;
	return true;
}

bool ATVDungeonManager::IsLocationInInstanceSpace(const FVector& Location) const
{
	return Location.Z < InstanceSpaceZ;
}

FTransform ATVDungeonManager::GetWorldReturnTransform() const
{
	UWorld* World = GetWorld();
	for (TActorIterator<ATVDungeonPortal> It(World); It; ++It)
	{
		if (!It->bIsExit && It->GetLevel() == World->PersistentLevel)
		{
			return It->GetArrivalTransform();
		}
	}
	if (const ATVGameMode* GameMode = GetTVGameMode(this))
	{
		return GameMode->GetBindStoneTransform();
	}
	return FTransform(FVector(0.0, 0.0, 300.0));
}

int32 ATVDungeonManager::AllocateSlot() const
{
	for (int32 Slot = 0;; ++Slot)
	{
		bool bUsed = false;
		for (const TPair<int32, FTVDungeonInstance>& Pair : Instances)
		{
			bUsed |= Pair.Value.Slot == Slot;
		}
		if (!bUsed)
		{
			return Slot;
		}
	}
}

// ------------------------------------------------------------------------------------------------ enter / leave

void ATVDungeonManager::EnterDungeon(ATVPlayerState* Leader)
{
	if (!HasAuthority() || !Leader)
	{
		return;
	}
	if (IsInsideDungeon(Leader))
	{
		return;
	}
	const UTVPartySubsystem* Parties = UTVPartySubsystem::Get(this);
	const int32 PartyKey = Parties ? Parties->GetPartyKey(Leader) : -(Leader->GetPlayerId() + 1);
	const TArray<ATVPlayerState*> Members = Parties ? Parties->GetPartyMembers(Leader) : TArray<ATVPlayerState*>{ Leader };

	FTVDungeonInstance* Instance = FindInstanceByKey(PartyKey);
	if (!Instance)
	{
		Instance = CreateInstance(PartyKey);
		if (!Instance)
		{
			if (ATVPlayerController* PC = GetTVPC(Leader))
			{
				PC->SendSystemMessage(TEXT("Maldrek's Barrow could not be opened (see server log)."));
			}
			return;
		}
	}
	Instance->DesiredPartySize = FMath::Max(Instance->DesiredPartySize, Members.Num());
	if (Instance->bReady && Instance->DesiredPartySize > Instance->ScaledForPartySize)
	{
		ApplyScaling(*Instance, Instance->DesiredPartySize);
	}

	const APawn* LeaderPawn = Leader->GetPawn();
	for (ATVPlayerState* Member : Members)
	{
		if (!Member || IsInsideDungeon(Member))
		{
			continue;
		}
		const APawn* MemberPawn = Member->GetPawn();
		const bool bNear = Member == Leader
			|| (LeaderPawn && MemberPawn && !TVNet::IsPawnDead(MemberPawn)
				&& FVector::Dist(LeaderPawn->GetActorLocation(), MemberPawn->GetActorLocation()) <= GatherRadius);
		if (bNear)
		{
			QueueEntry(Member, *Instance);
		}
	}
}

void ATVDungeonManager::QueueEntry(ATVPlayerState* Player, FTVDungeonInstance& Instance)
{
	for (const FTVPendingDungeonEntry& Pending : PendingEntries)
	{
		if (Pending.Player.Get() == Player)
		{
			return;
		}
	}
	FTVPendingDungeonEntry& Entry = PendingEntries.AddDefaulted_GetRef();
	Entry.Player = Player;
	Entry.InstanceId = Instance.InstanceId;
	Entry.RequestedAt = GetWorld()->GetTimeSeconds();
	Instance.EmptySince = -1.0;
	if (ATVPlayerController* PC = GetTVPC(Player))
	{
		PC->SendSystemMessage(TEXT("Entering Maldrek's Barrow..."));
	}
}

FTVDungeonInstance* ATVDungeonManager::CreateInstance(int32 PartyKey)
{
	UWorld* World = GetWorld();
	const int32 InstanceId = NextInstanceId++;
	const int32 Slot = AllocateSlot();
	const FVector Origin = FirstInstanceOrigin + FVector(InstanceSpacing * Slot, 0.0, 0.0);
	const FString InstanceName = FString::Printf(TEXT("%s_TVInst%d"), *FPackageName::GetShortName(DungeonLevelPackage), InstanceId);

	// Checked against 5.8.3 LevelStreamingDynamic.h: FLoadLevelInstanceParams(World, LongPackageName, Transform),
	// OptionalLevelNameOverride is a const FString*, LoadLevelInstance(const FLoadLevelInstanceParams&, bool&).
	ULevelStreamingDynamic::FLoadLevelInstanceParams Params(World, DungeonLevelPackage, FTransform(FRotator::ZeroRotator, Origin));
	Params.OptionalLevelNameOverride = &InstanceName;
	Params.bInitiallyVisible = true;
	Params.bLoadAsTempPackage = false;

	bool bSuccess = false;
	ULevelStreamingDynamic* Streaming = ULevelStreamingDynamic::LoadLevelInstance(Params, bSuccess);
	if (!bSuccess || !Streaming)
	{
		UE_LOG(LogTV, Error, TEXT("%s LoadLevelInstance failed for %s (does the map exist? run Tools/ue/create_maps.py)"), TV_NET_LOG_PREFIX, *DungeonLevelPackage);
		return nullptr;
	}

	FTVDungeonInstance& Instance = Instances.Add(InstanceId);
	Instance.InstanceId = InstanceId;
	Instance.Slot = Slot;
	Instance.PartyKey = PartyKey;
	Instance.InstanceName = InstanceName;
	Instance.Origin = Origin;
	Instance.Streaming = Streaming;
	Instance.LoadedPackageName = Streaming->GetWorldAssetPackageFName();
	Instance.CreatedAt = World->GetTimeSeconds();
	Instance.EntranceTransform = FTransform(Origin + FVector(0.0, 0.0, 200.0));

	UE_LOG(LogTV, Log, TEXT("%s Dungeon instance %s loading at %s for party key %d (package %s)"), TV_NET_LOG_PREFIX,
		*InstanceName, *Origin.ToString(), PartyKey, *Instance.LoadedPackageName.ToString());
	return &Instance;
}

void ATVDungeonManager::LeaveDungeon(ATVPlayerState* Player)
{
	if (!HasAuthority() || !Player)
	{
		return;
	}
	PendingEntries.RemoveAll([Player](const FTVPendingDungeonEntry& Entry) { return Entry.Player.Get() == Player; });

	FTVDungeonInstance* Instance = FindInstanceOf(Player);
	if (!Instance)
	{
		return;
	}
	Instance->Inside.RemoveAll([Player](const TWeakObjectPtr<ATVPlayerState>& P) { return !P.IsValid() || P.Get() == Player; });
	const FString InstanceName = Instance->InstanceName;
	if (UTVMountComponent* Mount = TVNet::GetMount(Player))
	{
		Mount->NotifyLeftDungeon();
	}

	const FTransform Out = GetWorldReturnTransform();
	ATVPlayerController* PC = GetTVPC(Player);
	APawn* Pawn = Player->GetPawn();
	if (Pawn && !TVNet::IsPawnDead(Pawn))
	{
		Pawn->TeleportTo(Out.GetLocation(), Out.Rotator());
		if (PC)
		{
			PC->ClientSetRotation(Out.Rotator());
		}
	}
	else if (PC)
	{
		if (ATVGameMode* GameMode = GetTVGameMode(this))
		{
			GameMode->RespawnPlayerAt(PC, Out);
		}
	}
	if (PC)
	{
		PC->ClientUnloadDungeonInstance(InstanceName);
	}
	UE_LOG(LogTV, Log, TEXT("%s %s left dungeon instance %s (%d still inside)"), TV_NET_LOG_PREFIX,
		*TVNet::GetCharacterName(Player), *InstanceName, Instance->Inside.Num());
}

void ATVDungeonManager::TeleportInto(ATVPlayerState* Player, FTVDungeonInstance& Instance)
{
	ATVPlayerController* PC = GetTVPC(Player);
	if (!PC)
	{
		return;
	}
	// GDD 11: dismount on entering the dungeon; Systems blocks mounting until NotifyLeftDungeon.
	if (UTVMountComponent* Mount = TVNet::GetMount(Player))
	{
		Mount->NotifyEnteredDungeon();
	}

	const FTransform& Entrance = Instance.EntranceTransform;
	APawn* Pawn = Player->GetPawn();
	if (Pawn && !TVNet::IsPawnDead(Pawn))
	{
		Pawn->TeleportTo(Entrance.GetLocation(), Entrance.Rotator(), false, true);
		PC->ClientSetRotation(Entrance.Rotator());
	}
	else if (ATVGameMode* GameMode = GetTVGameMode(this))
	{
		GameMode->RespawnPlayerAt(PC, Entrance);
	}
	Instance.Inside.AddUnique(Player);
	Instance.EmptySince = -1.0;
	UE_LOG(LogTV, Log, TEXT("%s %s entered dungeon instance %s (party key %d, %d inside)"), TV_NET_LOG_PREFIX,
		*TVNet::GetCharacterName(Player), *Instance.InstanceName, Instance.PartyKey, Instance.Inside.Num());
}

bool ATVDungeonManager::HasClientLoaded(const ATVPlayerState* Player, const FTVDungeonInstance& Instance) const
{
	const ATVPlayerController* PC = GetTVPC(Player);
	if (!PC)
	{
		return false;
	}
	if (PC->IsLocalController())
	{
		return true; // listen server / standalone: the server world already has the level
	}
	const UNetConnection* Connection = PC->GetNetConnection();
	// ClientVisibleLevelNames is public in 5.8.3 NetConnection.h; filled by ServerUpdateLevelVisibility.
	return Connection && Connection->ClientVisibleLevelNames.Contains(Instance.LoadedPackageName);
}

// ------------------------------------------------------------------------------------------------ party / logout hooks

void ATVDungeonManager::HandlePartyKeyChanged(ATVPlayerState* Player, int32 OldKey, int32 NewKey)
{
	if (OldKey == NewKey || !Player)
	{
		return;
	}
	FTVDungeonInstance* Instance = FindInstanceOf(Player);
	if (!Instance || Instance->PartyKey != OldKey)
	{
		return;
	}
	const UTVPartySubsystem* Parties = UTVPartySubsystem::Get(this);
	bool bOthersOfOldParty = false;
	for (const TWeakObjectPtr<ATVPlayerState>& Other : Instance->Inside)
	{
		if (Other.IsValid() && Other.Get() != Player && Parties && Parties->GetPartyKey(Other.Get()) == OldKey)
		{
			bOthersOfOldParty = true;
		}
	}
	if (!bOthersOfOldParty && !FindInstanceByKey(NewKey))
	{
		// e.g. a solo player inside forms a party: the instance follows the new party.
		Instance->PartyKey = NewKey;
		UE_LOG(LogTV, Log, TEXT("%s Dungeon instance %s now belongs to party key %d"), TV_NET_LOG_PREFIX, *Instance->InstanceName, NewKey);
		return;
	}
	if (ATVPlayerController* PC = GetTVPC(Player))
	{
		PC->SendSystemMessage(TEXT("You are no longer in this dungeon's party and have been moved outside."));
	}
	LeaveDungeon(Player);
}

void ATVDungeonManager::HandleLogout(ATVPlayerState* Player)
{
	PendingEntries.RemoveAll([Player](const FTVPendingDungeonEntry& Entry) { return Entry.Player.Get() == Player; });
	FTVDungeonInstance* Instance = FindInstanceOf(Player);
	if (!Instance)
	{
		return;
	}
	Instance->Inside.RemoveAll([Player](const TWeakObjectPtr<ATVPlayerState>& P) { return !P.IsValid() || P.Get() == Player; });
	if (UTVMountComponent* Mount = TVNet::GetMount(Player))
	{
		Mount->NotifyLeftDungeon();
	}
	if (APawn* Pawn = Player->GetPawn())
	{
		const FTransform Out = GetWorldReturnTransform();
		Pawn->TeleportTo(Out.GetLocation(), Out.Rotator(), false, true);
	}
	UE_LOG(LogTV, Log, TEXT("%s %s logged out inside %s; moved outside for the save"), TV_NET_LOG_PREFIX,
		*TVNet::GetCharacterName(Player), *Instance->InstanceName);
}

// ------------------------------------------------------------------------------------------------ instance lifecycle

void ATVDungeonManager::OnInstanceReady(FTVDungeonInstance& Instance)
{
	Instance.bReady = true;
	ULevel* Level = Instance.Streaming ? Instance.Streaming->GetLoadedLevel() : nullptr;

	bool bFoundEntrance = false;
	if (Level)
	{
		for (AActor* Actor : Level->Actors)
		{
			if (IsValid(Actor) && Actor->ActorHasTag(EntranceTag))
			{
				Instance.EntranceTransform = FTransform(Actor->GetActorRotation(), Actor->GetActorLocation() + FVector(0.0, 0.0, 100.0));
				bFoundEntrance = true;
				break;
			}
		}
		if (!bFoundEntrance)
		{
			for (AActor* Actor : Level->Actors)
			{
				const ATVDungeonPortal* Portal = Cast<ATVDungeonPortal>(Actor);
				if (Portal && Portal->bIsExit)
				{
					Instance.EntranceTransform = Portal->GetArrivalTransform();
					bFoundEntrance = true;
					break;
				}
			}
		}
	}
	if (!bFoundEntrance)
	{
		UE_LOG(LogTV, Warning, TEXT("%s %s has no actor tagged %s or exit portal; using the instance origin"), TV_NET_LOG_PREFIX,
			*Instance.InstanceName, *EntranceTag.ToString());
	}
	UE_LOG(LogTV, Log, TEXT("%s Dungeon instance %s ready"), TV_NET_LOG_PREFIX, *Instance.InstanceName);
	ApplyScaling(Instance, Instance.DesiredPartySize);
}

void ATVDungeonManager::GetSpawnPoints(const FTVDungeonInstance& Instance, TArray<ATVSpawnPoint*>& Out) const
{
	const ULevel* Level = Instance.Streaming ? Instance.Streaming->GetLoadedLevel() : nullptr;
	if (!Level)
	{
		return;
	}
	for (AActor* Actor : Level->Actors)
	{
		if (ATVSpawnPoint* SpawnPoint = Cast<ATVSpawnPoint>(Actor))
		{
			Out.Add(SpawnPoint);
		}
	}
}

void ATVDungeonManager::ApplyScaling(FTVDungeonInstance& Instance, int32 PartySize)
{
	float Health = 1.f;
	float Damage = 1.f;
	GetPartyScaling(PartySize, Health, Damage);

	TArray<ATVSpawnPoint*> SpawnPoints;
	GetSpawnPoints(Instance, SpawnPoints);
	for (ATVSpawnPoint* SpawnPoint : SpawnPoints)
	{
		SpawnPoint->SetScaling(Health, Damage);
		SpawnPoint->SetActiveSpawning(true);
		if (!Instance.bSpawnedOnce)
		{
			SpawnPoint->SpawnNow();
		}
	}
	Instance.bSpawnedOnce = true;
	Instance.ScaledForPartySize = PartySize;
	UE_LOG(LogTV, Log, TEXT("%s Dungeon instance %s scaled for party size %d: health x%.3f damage x%.2f (%d spawn points)"),
		TV_NET_LOG_PREFIX, *Instance.InstanceName, PartySize, Health, Damage, SpawnPoints.Num());
}

void ATVDungeonManager::DestroyInstanceMobs(const FTVDungeonInstance& Instance)
{
	const ULevel* Level = Instance.Streaming ? Instance.Streaming->GetLoadedLevel() : nullptr;
	TArray<ATVMobCharacter*> ToDestroy;
	for (TActorIterator<ATVMobCharacter> It(GetWorld()); It; ++It)
	{
		const FVector Location = It->GetActorLocation();
		const bool bInLevel = Level && It->GetLevel() == Level;
		const bool bInVolume = FVector::DistXY(Location, Instance.Origin) < InstanceRadius && FMath::Abs(Location.Z - Instance.Origin.Z) < InstanceRadius;
		if (bInLevel || bInVolume)
		{
			ToDestroy.Add(*It);
		}
	}
	for (ATVMobCharacter* Mob : ToDestroy)
	{
		Mob->Destroy();
	}
}

void ATVDungeonManager::ResetInstance(FTVDungeonInstance& Instance)
{
	UE_LOG(LogTV, Log, TEXT("%s Party wiped in %s; resetting the instance"), TV_NET_LOG_PREFIX, *Instance.InstanceName);
	Instance.WipeSince = -1.0;
	DestroyInstanceMobs(Instance);

	ATVGameMode* GameMode = GetTVGameMode(this);
	for (const TWeakObjectPtr<ATVPlayerState>& Member : Instance.Inside)
	{
		ATVPlayerController* PC = GetTVPC(Member.Get());
		if (PC && GameMode)
		{
			PC->SendSystemMessage(TEXT("Your party has fallen. The Barrow resets."));
			GameMode->RespawnPlayerAt(PC, Instance.EntranceTransform);
		}
	}

	Instance.bSpawnedOnce = false;
	ApplyScaling(Instance, FMath::Max(Instance.DesiredPartySize, Instance.ScaledForPartySize));
}

void ATVDungeonManager::UnloadInstance(int32 InstanceId)
{
	FTVDungeonInstance* Instance = Instances.Find(InstanceId);
	if (!Instance)
	{
		return;
	}
	TArray<TWeakObjectPtr<ATVPlayerState>> StillInside = Instance->Inside;
	for (const TWeakObjectPtr<ATVPlayerState>& Member : StillInside)
	{
		LeaveDungeon(Member.Get());
	}
	DestroyInstanceMobs(*Instance);
	if (Instance->Streaming)
	{
		Instance->Streaming->SetIsRequestingUnloadAndRemoval(true);
	}
	UE_LOG(LogTV, Log, TEXT("%s Dungeon instance %s unloaded"), TV_NET_LOG_PREFIX, *Instance->InstanceName);
	Instances.Remove(InstanceId);
}

void ATVDungeonManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}
	TickInstances();
	TickPendingEntries();
}

void ATVDungeonManager::TickPendingEntries()
{
	const double Now = GetWorld()->GetTimeSeconds();
	for (int32 Index = PendingEntries.Num() - 1; Index >= 0; --Index)
	{
		FTVPendingDungeonEntry& Entry = PendingEntries[Index];
		ATVPlayerState* Player = Entry.Player.Get();
		FTVDungeonInstance* Instance = Instances.Find(Entry.InstanceId);
		if (!Player || !Instance)
		{
			PendingEntries.RemoveAt(Index);
			continue;
		}
		if (!Instance->bReady)
		{
			continue;
		}
		if (!Entry.bClientLoadSent)
		{
			if (ATVPlayerController* PC = GetTVPC(Player))
			{
				PC->ClientLoadDungeonInstance(DungeonLevelPackage, Instance->InstanceName, Instance->Origin);
			}
			Entry.bClientLoadSent = true;
			Entry.RequestedAt = Now;
		}
		const bool bLoaded = HasClientLoaded(Player, *Instance);
		const bool bTimedOut = Now - Entry.RequestedAt > ClientLoadTimeoutSeconds;
		if (bLoaded || bTimedOut)
		{
			if (!bLoaded)
			{
				UE_LOG(LogTV, Warning, TEXT("%s %s did not report %s visible within %.0f s; teleporting anyway"), TV_NET_LOG_PREFIX,
					*TVNet::GetCharacterName(Player), *Instance->LoadedPackageName.ToString(), ClientLoadTimeoutSeconds);
			}
			TeleportInto(Player, *Instance);
			PendingEntries.RemoveAt(Index);
		}
	}
}

void ATVDungeonManager::TickInstances()
{
	const double Now = GetWorld()->GetTimeSeconds();
	TArray<int32> ToUnload;
	for (TPair<int32, FTVDungeonInstance>& Pair : Instances)
	{
		FTVDungeonInstance& Instance = Pair.Value;
		if (!Instance.bReady)
		{
			if (Instance.Streaming && Instance.Streaming->IsLevelLoaded() && Instance.Streaming->IsLevelVisible())
			{
				OnInstanceReady(Instance);
			}
			else if (Now - Instance.CreatedAt > 120.0)
			{
				UE_LOG(LogTV, Error, TEXT("%s %s did not finish loading in 120 s"), TV_NET_LOG_PREFIX, *Instance.InstanceName);
				ToUnload.Add(Pair.Key);
			}
			continue;
		}

		Instance.Inside.RemoveAll([](const TWeakObjectPtr<ATVPlayerState>& P) { return !P.IsValid(); });
		const bool bHasPending = PendingEntries.ContainsByPredicate([&Instance](const FTVPendingDungeonEntry& E) { return E.InstanceId == Instance.InstanceId; });

		// Unload 60 s after the last member leaves.
		if (Instance.Inside.Num() == 0 && !bHasPending)
		{
			if (Instance.EmptySince < 0.0)
			{
				Instance.EmptySince = Now;
				UE_LOG(LogTV, Log, TEXT("%s Dungeon instance %s is empty; unloading in %.0f s"), TV_NET_LOG_PREFIX, *Instance.InstanceName, UnloadDelaySeconds);
			}
			else if (Now - Instance.EmptySince >= UnloadDelaySeconds)
			{
				ToUnload.Add(Pair.Key);
			}
			continue;
		}
		Instance.EmptySince = -1.0;

		// Wipe: everyone inside is dead (or has no pawn).
		bool bAllDead = Instance.Inside.Num() > 0;
		for (const TWeakObjectPtr<ATVPlayerState>& Member : Instance.Inside)
		{
			bAllDead &= TVNet::IsPlayerDead(Member.Get());
		}
		if (!bAllDead)
		{
			Instance.WipeSince = -1.0;
		}
		else if (Instance.WipeSince < 0.0)
		{
			Instance.WipeSince = Now;
		}
		else if (Now - Instance.WipeSince >= WipeResetDelaySeconds)
		{
			ResetInstance(Instance);
		}
	}
	for (const int32 InstanceId : ToUnload)
	{
		UnloadInstance(InstanceId);
	}
}

void ATVDungeonManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Instances.Reset();
	PendingEntries.Reset();
	Super::EndPlay(EndPlayReason);
}
