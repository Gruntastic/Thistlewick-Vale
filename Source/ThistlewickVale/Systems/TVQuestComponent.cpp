#include "Systems/TVQuestComponent.h"
#include "Systems/TVQuestRows.h"
#include "Systems/TVInventoryComponent.h"
#include "Systems/TVMountComponent.h"
#include "Systems/TVLootLibrary.h"
#include "Systems/TVInteractable.h"
#include "Combat/TVPlayerState.h"
#include "Net/TVGameState.h"
#include "Core/TVProjectSettings.h"
#include "ThistlewickVale.h"
#include "Net/UnrealNetwork.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Math/RandomStream.h"

UTVQuestComponent::UTVQuestComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	FTVRarityWeight Uncommon;
	Uncommon.Rarity = ETVRarity::Uncommon;
	Uncommon.Weight = 70.f;
	FTVRarityWeight Rare;
	Rare.Rarity = ETVRarity::Rare;
	Rare.Weight = 30.f;
	RollRarityWeights = { Uncommon, Rare };
}

void UTVQuestComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UTVQuestComponent, Quests, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UTVQuestComponent, WorldFlags, COND_OwnerOnly);
}

bool UTVQuestComponent::HasAuthority() const
{
	const AActor* Owner = GetOwner();
	return Owner && Owner->HasAuthority();
}

ATVPlayerState* UTVQuestComponent::GetTVPlayerState() const
{
	return Cast<ATVPlayerState>(GetOwner());
}

bool UTVQuestComponent::GetQuestRow(FName QuestId, FTVQuestRow& OutRow)
{
	const UDataTable* Table = UTVProjectSettings::Get()->LoadTable(TEXT("Quests"));
	const FTVQuestRow* Row = Table && !QuestId.IsNone() ? Table->FindRow<FTVQuestRow>(QuestId, TEXT("TVQuest"), false) : nullptr;
	if (!Row)
	{
		return false;
	}
	OutRow = *Row;
	OutRow.QuestId = QuestId;
	return true;
}

const FTVQuestProgress* UTVQuestComponent::GetQuestProgress(FName QuestId) const
{
	return Quests.FindByPredicate([QuestId](const FTVQuestProgress& Q) { return Q.QuestId == QuestId; });
}

FTVQuestProgress* UTVQuestComponent::FindMutable(FName QuestId)
{
	return Quests.FindByPredicate([QuestId](const FTVQuestProgress& Q) { return Q.QuestId == QuestId; });
}

ETVQuestState UTVQuestComponent::GetQuestState(FName QuestId) const
{
	const FTVQuestProgress* Progress = GetQuestProgress(QuestId);
	return Progress ? Progress->State : ETVQuestState::NotStarted;
}

void UTVQuestComponent::OnRep_Quests()
{
	OnQuestsChanged.Broadcast();
}

bool UTVQuestComponent::IsNearNpc(FName NpcId) const
{
	if (NpcId.IsNone())
	{
		return true; // quest without an NPC (auto-accepted / world-triggered)
	}
	const ATVPlayerState* PS = GetTVPlayerState();
	const APawn* Pawn = PS ? PS->GetPawn() : nullptr;
	const ATVNpc* Npc = ATVNpc::FindNpc(this, NpcId);
	if (!Pawn || !Npc)
	{
		return false;
	}
	return FVector::DistSquared(Pawn->GetActorLocation(), Npc->GetActorLocation()) <= FMath::Square(NpcRangeCm);
}

bool UTVQuestComponent::CanAcceptQuest(FName QuestId, FString* OutReason) const
{
	auto Fail = [OutReason](const TCHAR* Reason)
	{
		if (OutReason)
		{
			*OutReason = Reason;
		}
		return false;
	};
	FTVQuestRow Row;
	if (!GetQuestRow(QuestId, Row))
	{
		return Fail(TEXT("unknown quest"));
	}
	if (GetQuestState(QuestId) != ETVQuestState::NotStarted)
	{
		return Fail(TEXT("already accepted or completed"));
	}
	const ATVPlayerState* PS = GetTVPlayerState();
	if (!PS)
	{
		return Fail(TEXT("no player state"));
	}
	// INTERFACE-REQUEST: ATVPlayerState::Level is read directly (ledger lists it as replicated int32).
	if (PS->Level < Row.RequiredLevel)
	{
		return Fail(TEXT("level too low"));
	}
	for (const FName& Prereq : Row.PrereqQuestIds)
	{
		if (GetQuestState(Prereq) != ETVQuestState::Completed)
		{
			return Fail(TEXT("prerequisite not completed"));
		}
	}
	return true;
}

// ---- Accept / turn in / abandon ----

bool UTVQuestComponent::ServerAcceptQuest_Validate(FName QuestId)
{
	return !QuestId.IsNone();
}

void UTVQuestComponent::ServerAcceptQuest_Implementation(FName QuestId)
{
	FTVQuestRow Row;
	FString Reason;
	if (!CanAcceptQuest(QuestId, &Reason) || !GetQuestRow(QuestId, Row))
	{
		UE_LOG(LogTV, Verbose, TEXT("Accept '%s' refused: %s"), *QuestId.ToString(), *Reason);
		return;
	}
	if (!IsNearNpc(Row.GiverNpcId))
	{
		UE_LOG(LogTV, Verbose, TEXT("Accept '%s' refused: not near %s"), *QuestId.ToString(), *Row.GiverNpcId.ToString());
		return;
	}
	FTVQuestProgress Progress;
	Progress.QuestId = QuestId;
	Progress.State = Row.Objectives.Num() == 0 ? ETVQuestState::ReadyToTurnIn : ETVQuestState::Active;
	Progress.Progress.Init(0, Row.Objectives.Num());
	Quests.Add(Progress);
	OnQuestsChanged.Broadcast();
}

bool UTVQuestComponent::ServerTurnInQuest_Validate(FName QuestId)
{
	return !QuestId.IsNone();
}

void UTVQuestComponent::ServerTurnInQuest_Implementation(FName QuestId)
{
	FTVQuestProgress* Progress = FindMutable(QuestId);
	FTVQuestRow Row;
	if (!Progress || Progress->State != ETVQuestState::ReadyToTurnIn || !GetQuestRow(QuestId, Row))
	{
		return;
	}
	const FName TurnInNpc = Row.TurnInNpcId.IsNone() ? Row.GiverNpcId : Row.TurnInNpcId;
	if (!IsNearNpc(TurnInNpc))
	{
		return;
	}
	ATVPlayerState* PS = GetTVPlayerState();
	UTVInventoryComponent* Inventory = PS ? PS->GetInventory() : nullptr;
	const int32 ItemCount = PS ? CountRewardItemsFor(Row, PS->ClassId) : 0;
	if (!Inventory || Inventory->GetFreeSlots() < ItemCount)
	{
		UE_LOG(LogTV, Verbose, TEXT("Turn-in '%s' refused: bag needs %d free slots."), *QuestId.ToString(), ItemCount);
		return;
	}
	Progress->State = ETVQuestState::Completed;
	GrantRewards(Row);
	OnQuestsChanged.Broadcast();
}

bool UTVQuestComponent::ServerAbandonQuest_Validate(FName QuestId)
{
	return !QuestId.IsNone();
}

void UTVQuestComponent::ServerAbandonQuest_Implementation(FName QuestId)
{
	const int32 Removed = Quests.RemoveAll([QuestId](const FTVQuestProgress& Q)
	{
		return Q.QuestId == QuestId && Q.State != ETVQuestState::Completed;
	});
	if (Removed > 0)
	{
		OnQuestsChanged.Broadcast();
	}
}

ETVRarity UTVQuestComponent::ResolveRewardRarity(FName RarityName) const
{
	if (!RarityName.IsNone() && RarityName != FName(TEXT("Roll")))
	{
		const int64 Value = StaticEnum<ETVRarity>()->GetValueByNameString(RarityName.ToString());
		if (Value != INDEX_NONE && Value > 0)
		{
			return static_cast<ETVRarity>(Value);
		}
		UE_LOG(LogTV, Warning, TEXT("Quest reward rarity '%s' is not a rarity name; rolling instead."), *RarityName.ToString());
	}
	const FRandomStream Stream(UTVLootLibrary::MakeServerSeed());
	const ETVRarity Rolled = UTVLootLibrary::PickRarity(RollRarityWeights, Stream);
	return Rolled == ETVRarity::None ? ETVRarity::Uncommon : Rolled;
}

int32 UTVQuestComponent::CountRewardItemsFor(const FTVQuestRow& Row, FName ClassId)
{
	int32 Count = 0;
	for (const FTVQuestRewardItem& Reward : Row.RewardItems)
	{
		FTVItemDefRow Def;
		if (UTVLootLibrary::GetItemDef(Reward.ItemDefId, Def) && Def.IsUsableByClass(ClassId))
		{
			++Count;
		}
	}
	return Count;
}

void UTVQuestComponent::GrantRewards(const FTVQuestRow& Row)
{
	ATVPlayerState* PS = GetTVPlayerState();
	if (!PS)
	{
		return;
	}
	if (Row.RewardXP > 0)
	{
		PS->AddXP(Row.RewardXP);
	}
	if (Row.RewardGold > 0)
	{
		PS->AddGold(Row.RewardGold);
	}
	if (UTVInventoryComponent* Inventory = PS->GetInventory())
	{
		for (const FTVQuestRewardItem& Reward : Row.RewardItems)
		{
			FTVItemDefRow Def;
			if (!UTVLootLibrary::GetItemDef(Reward.ItemDefId, Def))
			{
				UE_LOG(LogTV, Warning, TEXT("Quest '%s' rewards unknown item '%s'."), *Row.QuestId.ToString(), *Reward.ItemDefId.ToString());
				continue;
			}
			// Rewards list one item per armor family / class weapon; only the ones this class can use are granted.
			if (!Def.IsUsableByClass(PS->ClassId))
			{
				continue;
			}
			const ETVRarity Rarity = ResolveRewardRarity(Reward.Rarity);
			const int32 Seed = UTVLootLibrary::MakeServerSeed();
			const int32 MaxLevel = FMath::Max(Def.ItemLevelMin, Def.ItemLevelMax);
			const int32 ItemLevel = FRandomStream(Seed ^ 0x5bd1e995).RandRange(Def.ItemLevelMin, MaxLevel);
			FTVItemInstance Item = UTVLootLibrary::RollItem(Def, Rarity, ItemLevel, Seed);
			UTVLootLibrary::AssignNewGuid(Item);
			Inventory->AddItem(Item);
		}
	}
	if (!Row.RewardMount.IsNone())
	{
		if (UTVMountComponent* Mount = PS->GetMount())
		{
			Mount->GrantMount(Row.RewardMount);
		}
	}
	if (!Row.SetsWorldFlag.IsNone())
	{
		WorldFlags.AddUnique(Row.SetsWorldFlag);
		ApplyWorldFlag(Row.SetsWorldFlag);
	}
}

void UTVQuestComponent::ApplyWorldFlag(FName Flag)
{
	UWorld* World = GetWorld();
	if (ATVGameState* GameState = World ? World->GetGameState<ATVGameState>() : nullptr)
	{
		if (!GameState->HasWorldFlag(Flag))
		{
			GameState->SetWorldFlag(Flag);
		}
	}
}

// ---- Progress ----

bool UTVQuestComponent::WantsTarget(ETVQuestObjectiveType Type, FName TargetId) const
{
	for (const FTVQuestProgress& Progress : Quests)
	{
		if (Progress.State != ETVQuestState::Active)
		{
			continue;
		}
		FTVQuestRow Row;
		if (!GetQuestRow(Progress.QuestId, Row))
		{
			continue;
		}
		for (int32 i = 0; i < Row.Objectives.Num(); ++i)
		{
			const FTVQuestObjective& Obj = Row.Objectives[i];
			const int32 Current = Progress.Progress.IsValidIndex(i) ? Progress.Progress[i] : 0;
			if (Obj.Type == Type && Obj.TargetId == TargetId && Current < Obj.Count)
			{
				return true;
			}
		}
	}
	return false;
}

void UTVQuestComponent::AddProgress(ETVQuestObjectiveType Type, FName TargetId, int32 Amount)
{
	if (!HasAuthority() || TargetId.IsNone() || Amount <= 0)
	{
		return;
	}
	bool bChanged = false;
	for (FTVQuestProgress& Progress : Quests)
	{
		if (Progress.State != ETVQuestState::Active)
		{
			continue;
		}
		FTVQuestRow Row;
		if (!GetQuestRow(Progress.QuestId, Row))
		{
			continue;
		}
		if (Progress.Progress.Num() != Row.Objectives.Num())
		{
			Progress.Progress.SetNumZeroed(Row.Objectives.Num());
		}
		bool bAllDone = true;
		for (int32 i = 0; i < Row.Objectives.Num(); ++i)
		{
			const FTVQuestObjective& Obj = Row.Objectives[i];
			const bool bMatches = Obj.Type == Type || (Type == ETVQuestObjectiveType::Event && Obj.Type == ETVQuestObjectiveType::Defend);
			if (bMatches && Obj.TargetId == TargetId && Progress.Progress[i] < Obj.Count)
			{
				Progress.Progress[i] = FMath::Min(Obj.Count, Progress.Progress[i] + Amount);
				bChanged = true;
			}
			bAllDone &= Progress.Progress[i] >= Obj.Count;
		}
		if (bAllDone)
		{
			Progress.State = ETVQuestState::ReadyToTurnIn;
			bChanged = true;
		}
	}
	if (bChanged)
	{
		OnQuestsChanged.Broadcast();
	}
}

void UTVQuestComponent::NotifyKill(FName MobId)              { AddProgress(ETVQuestObjectiveType::Kill, MobId, 1); }
void UTVQuestComponent::NotifyCollect(FName ItemOrNodeId)    { AddProgress(ETVQuestObjectiveType::Collect, ItemOrNodeId, 1); }
void UTVQuestComponent::NotifyInteract(FName TargetId)       { AddProgress(ETVQuestObjectiveType::Interact, TargetId, 1); }
void UTVQuestComponent::NotifyEvent(FName EventId)           { AddProgress(ETVQuestObjectiveType::Event, EventId, 1); }

// ---- Save ----

void UTVQuestComponent::RestoreFromSave(const TArray<FTVQuestProgress>& InQuests, const TArray<FName>& InWorldFlags)
{
	if (!HasAuthority())
	{
		return;
	}
	Quests.Reset();
	for (const FTVQuestProgress& Saved : InQuests)
	{
		FTVQuestRow Row;
		if (!GetQuestRow(Saved.QuestId, Row))
		{
			UE_LOG(LogTV, Warning, TEXT("Save: unknown quest '%s' dropped."), *Saved.QuestId.ToString());
			continue;
		}
		FTVQuestProgress Clean = Saved;
		Clean.Progress.SetNumZeroed(Row.Objectives.Num());
		for (int32 i = 0; i < Row.Objectives.Num(); ++i)
		{
			Clean.Progress[i] = FMath::Clamp(Clean.Progress[i], 0, Row.Objectives[i].Count);
		}
		Quests.Add(Clean);
	}
	WorldFlags.Reset();
	for (const FName& Flag : InWorldFlags)
	{
		if (!Flag.IsNone())
		{
			WorldFlags.AddUnique(Flag);
			ApplyWorldFlag(Flag);
		}
	}
	OnQuestsChanged.Broadcast();
}
