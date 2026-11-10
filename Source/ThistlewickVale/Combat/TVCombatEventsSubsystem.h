// Server-side combat events other systems listen to (loot/XP/quests, mount dismount).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TVCombatEventsSubsystem.generated.h"

class ATVMobCharacter;
class ATVPlayerState;

DECLARE_MULTICAST_DELEGATE_TwoParams(FTVOnMobKilled, ATVMobCharacter* /*Mob*/, const TArray<ATVPlayerState*>& /*EligiblePlayers*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FTVOnPlayerDamaged, ATVPlayerState*, float);
DECLARE_MULTICAST_DELEGATE_TwoParams(FTVOnPlayerDied, ATVPlayerState* /*Player*/, AActor* /*Killer*/);

UCLASS()
class THISTLEWICKVALE_API UTVCombatEventsSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UTVCombatEventsSubsystem* Get(const UObject* WorldContext);

	/** Mob died; EligiblePlayers = players on its threat table plus their parties (UTVPartySubsystem), no duplicates. */
	FTVOnMobKilled OnMobKilled;
	/** A player took damage (after mitigation, > 0). */
	FTVOnPlayerDamaged OnPlayerDamaged;
	FTVOnPlayerDied OnPlayerDied;

	void BroadcastMobKilled(ATVMobCharacter* Mob);
	void BroadcastPlayerDamaged(ATVPlayerState* Player, float Amount);
	void BroadcastPlayerDied(ATVPlayerState* Player, AActor* Killer);

	/** Tagged players of the mob + their party members. */
	static void ComputeKillEligibility(ATVMobCharacter* Mob, TArray<ATVPlayerState*>& OutPlayers);
};
