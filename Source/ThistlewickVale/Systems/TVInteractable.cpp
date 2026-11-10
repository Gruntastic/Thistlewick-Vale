#include "Systems/TVInteractable.h"
#include "Systems/TVQuestComponent.h"
#include "Systems/TVQuestRows.h"
#include "Combat/TVPlayerState.h"
#include "Core/TVAssetRegistrySubsystem.h"
#include "Core/TVProjectSettings.h"
#include "ThistlewickVale.h"
#include "Net/UnrealNetwork.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "GameFramework/Pawn.h"

// ---- ITVInteractable ----

bool ITVInteractable::IsInInteractRange(const ATVPlayerState* Player) const
{
	const AActor* Self = GetInteractableActor();
	const APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	if (!Self || !Pawn)
	{
		return false;
	}
	// Small tolerance for latency between client position and server position.
	const float Range = GetInteractRange() + 50.f;
	return FVector::DistSquared(Self->GetActorLocation(), Pawn->GetActorLocation()) <= FMath::Square(Range);
}

bool ITVInteractable::CanInteract(const ATVPlayerState* Player) const
{
	return IsInInteractRange(Player);
}

// ---- ATVQuestNode ----

ATVQuestNode::ATVQuestNode()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
}

void ATVQuestNode::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATVQuestNode, bAvailable);
}

void ATVQuestNode::BeginPlay()
{
	Super::BeginPlay();
	// Loaded on the server too: the mesh gives the node its collision for click-to-interact traces.
	if (UTVAssetRegistrySubsystem* Manifest = UTVAssetRegistrySubsystem::Get(this))
	{
		if (UStaticMesh* StaticMesh = Manifest->LoadSync<UStaticMesh>(MeshId))
		{
			Mesh->SetStaticMesh(StaticMesh);
		}
	}
	ApplyAvailability();
}

bool ATVQuestNode::CanInteract(const ATVPlayerState* Player) const
{
	if (!bAvailable || !Player || !IsInInteractRange(Player))
	{
		return false;
	}
	if (RespawnTime <= 0.f && UsedBy.Contains(const_cast<ATVPlayerState*>(Player)))
	{
		return false;
	}
	// GetQuests() may be non-const on ATVPlayerState; the call does not modify the player.
	const UTVQuestComponent* Quests = const_cast<ATVPlayerState*>(Player)->GetQuests();
	const ETVQuestObjectiveType Type = Kind == ETVQuestNodeKind::Collect ? ETVQuestObjectiveType::Collect : ETVQuestObjectiveType::Interact;
	return Quests && Quests->WantsTarget(Type, NodeId);
}

void ATVQuestNode::Interact(ATVPlayerState* Player)
{
	if (!HasAuthority() || !CanInteract(Player))
	{
		return;
	}
	UTVQuestComponent* Quests = Player->GetQuests();
	if (Kind == ETVQuestNodeKind::Collect)
	{
		Quests->NotifyCollect(NodeId);
	}
	else
	{
		Quests->NotifyInteract(NodeId);
	}

	if (RespawnTime > 0.f)
	{
		SetAvailable(false);
		GetWorldTimerManager().SetTimer(RespawnTimer, FTimerDelegate::CreateUObject(this, &ATVQuestNode::SetAvailable, true), RespawnTime, false);
	}
	else
	{
		UsedBy.Add(Player);
	}
}

void ATVQuestNode::SetAvailable(bool bInAvailable)
{
	bAvailable = bInAvailable;
	ApplyAvailability();
}

void ATVQuestNode::OnRep_Available()
{
	ApplyAvailability();
}

void ATVQuestNode::ApplyAvailability()
{
	Mesh->SetHiddenInGame(!bAvailable);
	Mesh->SetCollisionEnabled(bAvailable ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}

// ---- ATVNpc ----

ATVNpc::ATVNpc()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	SkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMesh"));
	SkeletalMesh->SetupAttachment(Root);
	PlaceholderMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderMesh"));
	PlaceholderMesh->SetupAttachment(Root);
}

void ATVNpc::BeginPlay()
{
	Super::BeginPlay();
	FTVNpcRow Row;
	UTVAssetRegistrySubsystem* Manifest = UTVAssetRegistrySubsystem::Get(this);
	if (!GetNpcRow(Row) || !Manifest)
	{
		return;
	}
	// Same rule as characters: skeletal mesh on the skeletal component, static placeholder on PlaceholderMesh.
	UObject* Asset = Manifest->LoadSync(Row.MeshId);
	if (USkeletalMesh* Skel = Cast<USkeletalMesh>(Asset))
	{
		SkeletalMesh->SetSkeletalMeshAsset(Skel);
	}
	else if (UStaticMesh* Static = Cast<UStaticMesh>(Asset))
	{
		PlaceholderMesh->SetStaticMesh(Static);
	}
}

bool ATVNpc::GetNpcRow(FTVNpcRow& OutRow) const
{
	const UDataTable* Table = UTVProjectSettings::Get()->LoadTable(TEXT("Npcs"));
	const FTVNpcRow* Row = Table && !NpcId.IsNone() ? Table->FindRow<FTVNpcRow>(NpcId, TEXT("TVNpc"), false) : nullptr;
	if (!Row)
	{
		return false;
	}
	OutRow = *Row;
	OutRow.NpcId = NpcId;
	return true;
}

bool ATVNpc::IsVendor() const
{
	FTVNpcRow Row;
	return GetNpcRow(Row) && Row.IsVendor;
}

bool ATVNpc::SellsItem(FName ItemDefId) const
{
	FTVNpcRow Row;
	return GetNpcRow(Row) && Row.IsVendor && Row.VendorItems.Contains(ItemDefId);
}

FText ATVNpc::GetDisplayName() const
{
	FTVNpcRow Row;
	return GetNpcRow(Row) ? Row.DisplayName : FText::FromName(NpcId);
}

FText ATVNpc::GetInteractPrompt() const
{
	return GetDisplayName();
}

void ATVNpc::Interact(ATVPlayerState* Player)
{
	if (!HasAuthority() || !CanInteract(Player))
	{
		return;
	}
	// Talking counts for "speak to X" Interact objectives. Dialogue / vendor windows open client-side from DT_Dialogue;
	// the resulting accept / turn-in / buy / sell requests come back through the Server RPCs, which re-check range.
	if (UTVQuestComponent* Quests = Player->GetQuests())
	{
		Quests->NotifyInteract(NpcId);
	}
}

TArray<FName> ATVNpc::GetAvailableQuests(const ATVPlayerState* Player) const
{
	TArray<FName> Result;
	FTVNpcRow Row;
	const UTVQuestComponent* Quests = Player ? const_cast<ATVPlayerState*>(Player)->GetQuests() : nullptr;
	if (!Quests || !GetNpcRow(Row))
	{
		return Result;
	}
	for (const FName& QuestId : Row.QuestIds)
	{
		FTVQuestRow Quest;
		if (UTVQuestComponent::GetQuestRow(QuestId, Quest) && (Quest.GiverNpcId.IsNone() || Quest.GiverNpcId == NpcId) && Quests->CanAcceptQuest(QuestId))
		{
			Result.Add(QuestId);
		}
	}
	return Result;
}

TArray<FName> ATVNpc::GetTurnInQuests(const ATVPlayerState* Player) const
{
	TArray<FName> Result;
	const UTVQuestComponent* Quests = Player ? const_cast<ATVPlayerState*>(Player)->GetQuests() : nullptr;
	if (!Quests)
	{
		return Result;
	}
	for (const FTVQuestProgress& Progress : Quests->GetQuests())
	{
		FTVQuestRow Quest;
		if (Progress.State == ETVQuestState::ReadyToTurnIn && UTVQuestComponent::GetQuestRow(Progress.QuestId, Quest))
		{
			const FName TurnIn = Quest.TurnInNpcId.IsNone() ? Quest.GiverNpcId : Quest.TurnInNpcId;
			if (TurnIn == NpcId)
			{
				Result.Add(Progress.QuestId);
			}
		}
	}
	return Result;
}

ATVNpc* ATVNpc::FindNpc(const UObject* WorldContext, FName InNpcId)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || InNpcId.IsNone())
	{
		return nullptr;
	}
	for (TActorIterator<ATVNpc> It(World); It; ++It)
	{
		if (It->NpcId == InNpcId)
		{
			return *It;
		}
	}
	return nullptr;
}
