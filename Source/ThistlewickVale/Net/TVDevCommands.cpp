// Script-friendly console commands for ATVPlayerController:
//  * UI-equivalent requests (interact, accept/turn in a quest, tab target, mount key) so a client can be driven without a UI.
//  * Dev cheats, executed on the server through ServerDevCommand and refused when the server is a Shipping build.
//  * The -TVExec / -TVExecFile hook that queues console commands on a client after login (compiled out of Shipping).
// Documented in Tests/Net/README.md.
#include "Net/TVPlayerController.h"

#include "ThistlewickVale.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "Combat/TVPlayerState.h"
#include "Combat/TVCharacterBase.h"
#include "Combat/TVCombatLibrary.h"
#include "Combat/TVTargetingComponent.h"
#include "Combat/TVAttributeSets.h"
#include "Combat/TVAbilitySystemComponent.h"
#include "Systems/TVInventoryComponent.h"
#include "Systems/TVEquipmentComponent.h"
#include "Systems/TVQuestComponent.h"
#include "Systems/TVQuestRows.h"
#include "Systems/TVMountComponent.h"
#include "Systems/TVLootLibrary.h"
#include "Net/TVDungeonManager.h"
#include "Net/TVNetPlayerAccess.h"

namespace TVDev
{
	const TCHAR* QuestStateName(ETVQuestState State)
	{
		switch (State)
		{
		case ETVQuestState::Active: return TEXT("Active");
		case ETVQuestState::ReadyToTurnIn: return TEXT("ReadyToTurnIn");
		case ETVQuestState::Completed: return TEXT("Completed");
		default: return TEXT("NotStarted");
		}
	}

	FString DescribeItem(const FTVItemInstance& Item)
	{
		FString Stats;
		for (const FTVRolledStat& Stat : Item.RolledStats)
		{
			Stats += FString::Printf(TEXT(" %s+%d"), *StaticEnum<ETVStat>()->GetNameStringByValue(static_cast<int64>(Stat.Stat)), Stat.Value);
		}
		return FString::Printf(TEXT("%s %s ilvl %d seed %d%s"), *Item.ItemDefId.ToString(),
			*StaticEnum<ETVRarity>()->GetNameStringByValue(static_cast<int64>(Item.Rarity)), Item.ItemLevel, Item.Seed, *Stats);
	}

	/** Splits a script into commands: ';' or new lines separate, '#' or '//' start a comment line. */
	void SplitScript(const FString& Script, TArray<FString>& Out)
	{
		FString Normalized = Script.Replace(TEXT("\r"), TEXT("")).Replace(TEXT("\n"), TEXT(";"));
		TArray<FString> Parts;
		Normalized.ParseIntoArray(Parts, TEXT(";"), true);
		for (FString& Part : Parts)
		{
			Part.TrimStartAndEndInline();
			if (!Part.IsEmpty() && !Part.StartsWith(TEXT("#")) && !Part.StartsWith(TEXT("//")))
			{
				Out.Add(Part);
			}
		}
	}
}

// ================================================================================================ UI-equivalent requests

void ATVPlayerController::TVInteract() { HandleInteract(); }
void ATVPlayerController::TVTabTarget() { HandleTabTarget(); }
void ATVPlayerController::TVMount() { HandleMountKey(); }

void ATVPlayerController::TVAcceptQuest(const FString& QuestId)
{
	ATVPlayerState* PS = GetTVPlayerState();
	if (UTVQuestComponent* Quests = PS ? PS->GetQuests() : nullptr)
	{
		Quests->ServerAcceptQuest(FName(*QuestId));
	}
}

void ATVPlayerController::TVTurnInQuest(const FString& QuestId)
{
	ATVPlayerState* PS = GetTVPlayerState();
	if (UTVQuestComponent* Quests = PS ? PS->GetQuests() : nullptr)
	{
		Quests->ServerTurnInQuest(FName(*QuestId));
	}
}

// ================================================================================================ dev cheats (client side)

void ATVPlayerController::TVDamageSelf(float Amount) { ServerDevCommand(FString::Printf(TEXT("DamageSelf %f"), Amount)); }
void ATVPlayerController::TVKillTarget() { ServerDevCommand(TEXT("KillTarget")); }
void ATVPlayerController::TVSetLevel(int32 NewLevel) { ServerDevCommand(FString::Printf(TEXT("SetLevel %d"), NewLevel)); }
void ATVPlayerController::TVGrantItem(const FString& ItemDefId, const FString& Rarity, int32 ItemLevel)
{
	ServerDevCommand(FString::Printf(TEXT("GrantItem %s %s %d"), *ItemDefId, *Rarity, ItemLevel));
}
void ATVPlayerController::TVGrantMount(const FString& MountId) { ServerDevCommand(TEXT("GrantMount ") + MountId); }
void ATVPlayerController::TVCompleteQuest(const FString& QuestId) { ServerDevCommand(TEXT("CompleteQuest ") + QuestId); }
void ATVPlayerController::TVTeleport(float X, float Y, float Z) { ServerDevCommand(FString::Printf(TEXT("Teleport %f %f %f"), X, Y, Z)); }
void ATVPlayerController::TVTargetMob(const FString& MobId) { ServerDevCommand(TEXT("TargetMob ") + MobId); }
void ATVPlayerController::TVStatus() { ServerDevCommand(TEXT("Status")); }
void ATVPlayerController::TVListItems() { ServerDevCommand(TEXT("ListItems")); }

void ATVPlayerController::TVVisible()
{
	// Client view: what replicated to this connection.
	TArray<FString> Players;
	TMap<FName, int32> MobCounts;
	int32 Mobs = 0;
	for (TActorIterator<ATVCharacterBase> It(GetWorld()); It; ++It)
	{
		if (const ATVMobCharacter* Mob = Cast<ATVMobCharacter>(*It))
		{
			if (!Mob->IsDead())
			{
				++Mobs;
				MobCounts.FindOrAdd(Mob->GetMobId())++;
			}
		}
		else if (const ATVPlayerState* OtherPS = It->GetPlayerState<ATVPlayerState>())
		{
			Players.Add(FString::Printf(TEXT("%s@(%.0f,%.0f,%.0f)"), *TVNet::GetCharacterName(OtherPS), It->GetActorLocation().X,
				It->GetActorLocation().Y, It->GetActorLocation().Z));
		}
	}
	FString MobList;
	for (const TPair<FName, int32>& Pair : MobCounts)
	{
		MobList += FString::Printf(TEXT(" %s x%d"), *Pair.Key.ToString(), Pair.Value);
	}
	Players.Sort();
	UE_LOG(LogTV, Log, TEXT("%s Visible: %d players [%s], %d living mobs [%s ]"), TV_NET_LOG_PREFIX, Players.Num(),
		*FString::Join(Players, TEXT(", ")), Mobs, *MobList);
}

// ================================================================================================ dev cheats (server side)

void ATVPlayerController::ServerDevCommand_Implementation(const FString& Command)
{
#if UE_BUILD_SHIPPING
	SendSystemMessage(TEXT("Dev commands are not available."));
#else
	HandleDevCommand(Command);
#endif
}

void ATVPlayerController::DevLog(const FString& Message) const
{
	const ATVPlayerState* PS = GetPlayerState<ATVPlayerState>();
	UE_LOG(LogTV, Log, TEXT("%s [Dev] %s: %s"), TV_NET_LOG_PREFIX, *TVNet::GetCharacterName(PS), *Message);
}

void ATVPlayerController::HandleDevCommand(const FString& Command)
{
#if !UE_BUILD_SHIPPING
	ATVPlayerState* PS = GetTVPlayerState();
	APawn* MyPawn = GetPawn();
	if (!bLoggedIn || !PS)
	{
		return;
	}
	TArray<FString> Args;
	Command.ParseIntoArrayWS(Args);
	if (Args.Num() == 0)
	{
		return;
	}
	const FString Verb = Args[0];
	auto Arg = [&Args](int32 Index) { return Args.IsValidIndex(Index) ? Args[Index] : FString(); };

	if (Verb == TEXT("DamageSelf"))
	{
		// No source: the player's own ASC builds the spec, as for environment damage. Goes through the normal damage
		// pipeline, so death, combat state and the OnPlayerDamaged event (mount dismount) all fire as in play.
		const float Amount = FCString::Atof(*Arg(1));
		const float Dealt = UTVCombatLibrary::DealDamage(nullptr, MyPawn, Amount, 0.f, false);
		DevLog(FString::Printf(TEXT("DamageSelf %.0f -> dealt %.0f, health %.0f%%, dead %s"), Amount, Dealt,
			TVNet::GetHealthPercent(PS) * 100.f, TVNet::IsPlayerDead(PS) ? TEXT("yes") : TEXT("no")));
	}
	else if (Verb == TEXT("KillTarget"))
	{
		AActor* Target = Targeting ? Targeting->GetCurrentTarget() : nullptr;
		const UTVAbilitySystemComponent* TargetASC = UTVCombatLibrary::GetTVASC(Target);
		if (!Target || !TargetASC || !UTVCombatLibrary::IsAlive(Target))
		{
			DevLog(TEXT("KillTarget: no living target"));
			return;
		}
		// Credited to the player (the kill and loot go through the normal mob-killed pipeline).
		const float MaxHealth = TargetASC->GetNumericAttribute(UTVAttributeSet_Vitals::GetMaxHealthAttribute());
		UTVCombatLibrary::DealDamage(MyPawn, Target, MaxHealth * 10.f, 1.f, false);
		const ATVMobCharacter* Mob = Cast<ATVMobCharacter>(Target);
		DevLog(FString::Printf(TEXT("KillTarget %s (%s): %s"), *GetNameSafe(Target), Mob ? *Mob->GetMobId().ToString() : TEXT("-"),
			UTVCombatLibrary::IsAlive(Target) ? TEXT("still alive") : TEXT("dead")));
	}
	else if (Verb == TEXT("SetLevel"))
	{
		const int32 NewLevel = FMath::Clamp(FCString::Atoi(*Arg(1)), 1, UTVCombatLibrary::GetMaxLevel());
		PS->InitCharacter(PS->GetClassId(), NewLevel, 0);
		DevLog(FString::Printf(TEXT("SetLevel -> L%d"), PS->GetCharacterLevel()));
	}
	else if (Verb == TEXT("GrantItem"))
	{
		const FName ItemDefId(*Arg(1));
		const int64 RarityValue = StaticEnum<ETVRarity>()->GetValueByNameString(Arg(2));
		const int32 ItemLevel = FCString::Atoi(*Arg(3));
		FTVItemInstance Item;
		if (RarityValue == INDEX_NONE || !UTVLootLibrary::RollItemById(ItemDefId, static_cast<ETVRarity>(RarityValue), ItemLevel, FMath::Rand(), Item))
		{
			DevLog(FString::Printf(TEXT("GrantItem %s %s %d: unknown item def or rarity"), *Arg(1), *Arg(2), ItemLevel));
			return;
		}
		UTVLootLibrary::AssignNewGuid(Item);
		UTVInventoryComponent* Inventory = PS->GetInventory();
		const bool bAdded = Inventory && Inventory->AddItem(Item);
		DevLog(FString::Printf(TEXT("GrantItem %s: %s"), *TVDev::DescribeItem(Item), bAdded ? TEXT("added") : TEXT("bag full")));
	}
	else if (Verb == TEXT("GrantMount"))
	{
		const FName MountId(*Arg(1));
		UTVMountComponent* Mount = PS->GetMount();
		if (Mount)
		{
			Mount->GrantMount(MountId);
		}
		DevLog(FString::Printf(TEXT("GrantMount %s: owns it %s"), *MountId.ToString(), Mount && Mount->OwnsMount(MountId) ? TEXT("yes") : TEXT("no")));
	}
	else if (Verb == TEXT("CompleteQuest"))
	{
		// Feeds every objective through the quest component's own Notify* entry points, as play would.
		const FName QuestId(*Arg(1));
		UTVQuestComponent* Quests = PS->GetQuests();
		FTVQuestRow Row;
		if (!Quests || !UTVQuestComponent::GetQuestRow(QuestId, Row) || Quests->GetQuestState(QuestId) != ETVQuestState::Active)
		{
			DevLog(FString::Printf(TEXT("CompleteQuest %s: quest is not active"), *QuestId.ToString()));
			return;
		}
		for (const FTVQuestObjective& Objective : Row.Objectives)
		{
			for (int32 i = 0; i < Objective.Count; ++i)
			{
				switch (Objective.Type)
				{
				case ETVQuestObjectiveType::Kill: Quests->NotifyKill(Objective.TargetId); break;
				case ETVQuestObjectiveType::Collect: Quests->NotifyCollect(Objective.TargetId); break;
				case ETVQuestObjectiveType::Interact: Quests->NotifyInteract(Objective.TargetId); break;
				default: Quests->NotifyEvent(Objective.TargetId); break;
				}
			}
		}
		DevLog(FString::Printf(TEXT("CompleteQuest %s -> %s"), *QuestId.ToString(), TVDev::QuestStateName(Quests->GetQuestState(QuestId))));
	}
	else if (Verb == TEXT("Teleport"))
	{
		const FVector Where(FCString::Atof(*Arg(1)), FCString::Atof(*Arg(2)), FCString::Atof(*Arg(3)));
		const bool bMoved = MyPawn && MyPawn->TeleportTo(Where, MyPawn->GetActorRotation());
		DevLog(FString::Printf(TEXT("Teleport %s: %s"), *Where.ToString(), bMoved ? TEXT("ok") : TEXT("blocked")));
	}
	else if (Verb == TEXT("TargetMob"))
	{
		const FString Wanted = Arg(1);
		ATVMobCharacter* Best = nullptr;
		double BestDistance = TNumericLimits<double>::Max();
		for (TActorIterator<ATVMobCharacter> It(GetWorld()); It && MyPawn; ++It)
		{
			if (It->IsDead() || (Wanted != TEXT("any") && It->GetMobId() != FName(*Wanted)))
			{
				continue;
			}
			const double Distance = FVector::DistSquared(It->GetActorLocation(), MyPawn->GetActorLocation());
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				Best = *It;
			}
		}
		if (Targeting)
		{
			Targeting->SetTarget(Best);
		}
		DevLog(Best ? FString::Printf(TEXT("TargetMob %s -> %s at %.0f m"), *Wanted, *GetNameSafe(Best), FMath::Sqrt(BestDistance) / 100.0)
			: FString::Printf(TEXT("TargetMob %s: none found"), *Wanted));
	}
	else if (Verb == TEXT("Status"))
	{
		const UTVMountComponent* Mount = PS->GetMount();
		const ATVDungeonManager* Dungeons = ATVDungeonManager::Get(this);
		FString QuestText;
		if (const UTVQuestComponent* Quests = PS->GetQuests())
		{
			for (const FTVQuestProgress& Progress : Quests->GetQuests())
			{
				TArray<FString> Counts;
				for (int32 Count : Progress.Progress)
				{
					Counts.Add(FString::FromInt(Count));
				}
				QuestText += FString::Printf(TEXT(" %s=%s(%s)"), *Progress.QuestId.ToString(), TVDev::QuestStateName(Progress.State), *FString::Join(Counts, TEXT(",")));
			}
		}
		const int32 Bag = PS->GetInventory() ? PS->GetInventory()->GetItems().Num() : 0;
		const int32 Worn = PS->GetEquipment() ? PS->GetEquipment()->GetEquippedItems().Num() : 0;
		DevLog(FString::Printf(TEXT("Status L%d XP %d gold %d HP %.0f%% dead %s mounted %s dungeon %s at %s | items %d bag + %d worn | quests:%s"),
			PS->GetCharacterLevel(), PS->GetXP(), PS->GetGold(), TVNet::GetHealthPercent(PS) * 100.f,
			TVNet::IsPlayerDead(PS) ? TEXT("yes") : TEXT("no"), Mount && Mount->IsMounted() ? TEXT("yes") : TEXT("no"),
			Dungeons && Dungeons->IsInsideDungeon(PS) ? TEXT("yes") : TEXT("no"),
			MyPawn ? *MyPawn->GetActorLocation().ToCompactString() : TEXT("-"), Bag, Worn, QuestText.IsEmpty() ? TEXT(" none") : *QuestText));
	}
	else if (Verb == TEXT("ListItems"))
	{
		auto LogItems = [this](const TCHAR* Where, const TArray<FTVItemInstance>& Items)
		{
			for (const FTVItemInstance& Item : Items)
			{
				FString Reason;
				const bool bValid = UTVLootLibrary::ValidateItem(Item, &Reason);
				DevLog(FString::Printf(TEXT("Item [%s] %s: %s"), Where, *TVDev::DescribeItem(Item), bValid ? TEXT("valid") : *(TEXT("INVALID ") + Reason)));
			}
		};
		LogItems(TEXT("bag"), PS->GetInventory() ? PS->GetInventory()->GetItems() : TArray<FTVItemInstance>());
		LogItems(TEXT("worn"), PS->GetEquipment() ? PS->GetEquipment()->GetEquippedItems() : TArray<FTVItemInstance>());
	}
	else
	{
		DevLog(FString::Printf(TEXT("unknown dev command '%s'"), *Command));
	}
#endif
}

// ================================================================================================ -TVExec hook

void ATVPlayerController::StartDevExecQueue()
{
#if !UE_BUILD_SHIPPING
	if (bDevExecStarted || !IsLocalController())
	{
		return;
	}
	bDevExecStarted = true;
	FString Script;
	FString File;
	if (FParse::Value(FCommandLine::Get(), TEXT("TVExecFile="), File))
	{
		if (FPaths::IsRelative(File))
		{
			File = FPaths::Combine(FPaths::ProjectDir(), File);
		}
		if (!FFileHelper::LoadFileToString(Script, *File))
		{
			UE_LOG(LogTV, Warning, TEXT("%s TVExec: cannot read %s"), TV_NET_LOG_PREFIX, *File);
		}
	}
	FString Inline;
	if (FParse::Value(FCommandLine::Get(), TEXT("TVExec="), Inline, /*bShouldStopOnSeparator*/ false))
	{
		// Command-line values cannot contain double quotes, so 'single quotes' stand in for them.
		Script += TEXT(";") + Inline.Replace(TEXT("'"), TEXT("\""));
	}
	TVDev::SplitScript(Script, DevExecQueue);
	if (DevExecQueue.Num() > 0)
	{
		UE_LOG(LogTV, Log, TEXT("%s TVExec: %d commands queued"), TV_NET_LOG_PREFIX, DevExecQueue.Num());
		RunNextDevExec();
	}
#endif
}

void ATVPlayerController::RunNextDevExec()
{
#if !UE_BUILD_SHIPPING
	while (DevExecQueue.Num() > 0)
	{
		const FString Command = DevExecQueue[0];
		DevExecQueue.RemoveAt(0);
		if (Command.StartsWith(TEXT("wait "), ESearchCase::IgnoreCase))
		{
			const float Seconds = FMath::Max(0.01f, FCString::Atof(*Command.Mid(5)));
			GetWorldTimerManager().SetTimer(DevExecTimer, FTimerDelegate::CreateUObject(this, &ATVPlayerController::RunNextDevExec), Seconds, false);
			return;
		}
		UE_LOG(LogTV, Log, TEXT("%s TVExec: %s"), TV_NET_LOG_PREFIX, *Command);
		ConsoleCommand(Command, /*bWriteToLog*/ true);
	}
	UE_LOG(LogTV, Log, TEXT("%s TVExec: done"), TV_NET_LOG_PREFIX);
#endif
}
