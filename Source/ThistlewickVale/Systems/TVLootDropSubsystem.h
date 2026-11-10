// Server: on UTVCombatEventsSubsystem::OnMobKilled, every eligible player gets personal loot, gold, XP and a quest kill credit.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TVLootDropSubsystem.generated.h"

class ATVMobCharacter;
class ATVPlayerState;

UCLASS()
class THISTLEWICKVALE_API UTVLootDropSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** Public so tests / GM commands can award a kill without a combat event. */
	void HandleMobKilled(ATVMobCharacter* Mob, const TArray<ATVPlayerState*>& EligiblePlayers);

	/** Reads LootTableId and XPReward from the DT_Mobs row by reflection (FTVMobRow is Combat's struct). */
	static bool GetMobRewards(FName MobId, FName& OutLootTableId, int32& OutXPReward);

private:
	FDelegateHandle MobKilledHandle;
};
