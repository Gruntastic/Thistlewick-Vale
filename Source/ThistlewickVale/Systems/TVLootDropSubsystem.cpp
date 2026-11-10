#include "Systems/TVLootDropSubsystem.h"
#include "Systems/TVLootLibrary.h"
#include "Systems/TVInventoryComponent.h"
#include "Systems/TVQuestComponent.h"
#include "Combat/TVCombatEventsSubsystem.h"
#include "Combat/TVCharacterBase.h"
#include "Combat/TVPlayerState.h"
#include "Combat/TVCombatLibrary.h"
#include "Combat/TVCombatRows.h"
#include "Core/TVProjectSettings.h"
#include "ThistlewickVale.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"

bool UTVLootDropSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}
	const UWorld* World = Cast<UWorld>(Outer);
	// Game worlds only. Net mode may not be final yet at creation, so the client check happens in OnWorldBeginPlay.
	return World && World->IsGameWorld();
}

void UTVLootDropSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.GetNetMode() == NM_Client)
	{
		return;
	}
	if (UTVCombatEventsSubsystem* Events = InWorld.GetSubsystem<UTVCombatEventsSubsystem>())
	{
		MobKilledHandle = Events->OnMobKilled.AddUObject(this, &UTVLootDropSubsystem::HandleMobKilled);
	}
	else
	{
		UE_LOG(LogTV, Warning, TEXT("LootDrop: UTVCombatEventsSubsystem missing; kills will not drop loot."));
	}
}

void UTVLootDropSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		if (UTVCombatEventsSubsystem* Events = World->GetSubsystem<UTVCombatEventsSubsystem>())
		{
			Events->OnMobKilled.Remove(MobKilledHandle);
		}
	}
	MobKilledHandle.Reset();
	Super::Deinitialize();
}

bool UTVLootDropSubsystem::GetMobRewards(FName MobId, FName& OutLootTableId, int32& OutXPReward)
{
	OutLootTableId = NAME_None;
	OutXPReward = 0;
	const FTVMobRow* Row = MobId.IsNone() ? nullptr : UTVCombatLibrary::FindMobRow(MobId);
	if (!Row)
	{
		return false;
	}
	OutLootTableId = Row->LootTableId;
	OutXPReward = Row->XPReward;
	return true;
}

void UTVLootDropSubsystem::HandleMobKilled(ATVMobCharacter* Mob, const TArray<ATVPlayerState*>& EligiblePlayers)
{
	if (!Mob)
	{
		return;
	}
	const FName MobId = Mob->GetMobId();
	FName LootTableId;
	int32 XPReward = 0;
	GetMobRewards(MobId, LootTableId, XPReward);

	FTVLootTableRow LootTable;
	const bool bHasLoot = UTVLootLibrary::GetLootTable(LootTableId, LootTable);

	for (ATVPlayerState* Player : EligiblePlayers)
	{
		if (!Player)
		{
			continue;
		}
		if (XPReward > 0)
		{
			Player->AddXP(XPReward);
		}
		if (bHasLoot)
		{
			// Personal loot: independent seed per player, items filtered to the player's class.
			const FTVLootResult Loot = UTVLootLibrary::RollLootFromRow(LootTable, UTVLootLibrary::MakeServerSeed(), Player->ClassId);
			if (Loot.Gold > 0)
			{
				Player->AddGold(Loot.Gold);
			}
			if (UTVInventoryComponent* Inventory = Player->GetInventory())
			{
				for (const FTVItemInstance& Item : Loot.Items)
				{
					if (!Inventory->AddItem(Item))
					{
						// No mail or ground loot in the slice: a full bag loses the drop.
						UE_LOG(LogTV, Log, TEXT("LootDrop: %s's bag is full; '%s' lost."), *Player->GetPlayerName(), *Item.ItemDefId.ToString());
					}
				}
			}
		}
		if (UTVQuestComponent* Quests = Player->GetQuests())
		{
			Quests->NotifyKill(MobId);
		}
	}
}
