// Interaction contract + the two interactable actors Systems owns: quest nodes (glowmoss, lanterns, chests) and NPCs.
// ATVPlayerController::ServerInteract(AActor*) (Net) does:
//     if (ITVInteractable* I = Cast<ITVInteractable>(Actor)) { if (I->CanInteract(PS)) I->Interact(PS); }
// Interact() runs on the server only. Range is checked inside CanInteract.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameFramework/Actor.h"
#include "Engine/TimerHandle.h"
#include "TVInteractable.generated.h"

class ATVPlayerState;
class USceneComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;
struct FTVNpcRow;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UTVInteractable : public UInterface
{
	GENERATED_BODY()
};

class THISTLEWICKVALE_API ITVInteractable
{
	GENERATED_BODY()

public:
	/** Server and client (client uses it for the prompt). Default: player pawn within GetInteractRange(). */
	virtual bool CanInteract(const ATVPlayerState* Player) const;
	/** Server only. */
	virtual void Interact(ATVPlayerState* Player) = 0;
	virtual float GetInteractRange() const { return 400.f; }
	virtual FText GetInteractPrompt() const { return FText::GetEmpty(); }

	/** Distance check from the player's pawn to this interactable's actor. */
	bool IsInInteractRange(const ATVPlayerState* Player) const;

protected:
	virtual const AActor* GetInteractableActor() const = 0;
};

/** What a quest node reports to the quest component when used. */
UENUM(BlueprintType)
enum class ETVQuestNodeKind : uint8 { Collect, Interact };

/**
 * Glowmoss clump, lost lantern, camp chest, shrine... Placed in the level with a NodeId that quest objectives target.
 * Only usable by a player whose active quest needs NodeId. Shared node: after use it hides for RespawnTime (0 = stays, e.g. lanterns per player).
 */
UCLASS(Blueprintable)
class THISTLEWICKVALE_API ATVQuestNode : public AActor, public ITVInteractable
{
	GENERATED_BODY()

public:
	ATVQuestNode();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	virtual bool CanInteract(const ATVPlayerState* Player) const override;
	virtual void Interact(ATVPlayerState* Player) override;
	virtual float GetInteractRange() const override { return InteractRange; }
	virtual FText GetInteractPrompt() const override { return Prompt; }

	/** Objective TargetId this node satisfies. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|QuestNode") FName NodeId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|QuestNode") ETVQuestNodeKind Kind = ETVQuestNodeKind::Collect;
	/** Manifest ID of the node's mesh (static mesh). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|QuestNode") FName MeshId;
	/** Seconds until a used node is usable again. 0 = never hides (interact-once-per-player nodes such as lanterns). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|QuestNode") float RespawnTime = 30.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|QuestNode") float InteractRange = 350.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|QuestNode") FText Prompt;

protected:
	virtual const AActor* GetInteractableActor() const override { return this; }

	UFUNCTION() void OnRep_Available();
	void SetAvailable(bool bInAvailable);
	void ApplyAvailability();

	UPROPERTY(VisibleAnywhere, Category = "TV|QuestNode") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "TV|QuestNode") TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(ReplicatedUsing = OnRep_Available) bool bAvailable = true;
	FTimerHandle RespawnTimer;
	/** For RespawnTime == 0 nodes: players who already used this node. */
	TSet<TWeakObjectPtr<ATVPlayerState>> UsedBy;
};

/** Quest giver / vendor. Data from DT_Npcs[NpcId]. Dialogue and vendor windows are UI; the server side is here. */
UCLASS(Blueprintable)
class THISTLEWICKVALE_API ATVNpc : public AActor, public ITVInteractable
{
	GENERATED_BODY()

public:
	ATVNpc();

	virtual void BeginPlay() override;

	virtual void Interact(ATVPlayerState* Player) override;
	virtual float GetInteractRange() const override { return InteractRange; }
	virtual FText GetInteractPrompt() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Npc") FName NpcId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Npc") float InteractRange = 500.f;

	/** Copy of the DT_Npcs row (false if missing). */
	bool GetNpcRow(FTVNpcRow& OutRow) const;

	UFUNCTION(BlueprintPure, Category = "TV|Npc") bool IsVendor() const;
	UFUNCTION(BlueprintPure, Category = "TV|Npc") bool SellsItem(FName ItemDefId) const;
	UFUNCTION(BlueprintPure, Category = "TV|Npc") FText GetDisplayName() const;

	/** Quests this NPC can offer the player right now (quest log UI / "!" marker). */
	TArray<FName> GetAvailableQuests(const ATVPlayerState* Player) const;
	/** Quests this NPC takes back that are ready to turn in ("?" marker). */
	TArray<FName> GetTurnInQuests(const ATVPlayerState* Player) const;

	/** Finds the NPC actor with NpcId (server range checks for accept / turn-in). */
	static ATVNpc* FindNpc(const UObject* WorldContext, FName NpcId);

protected:
	virtual const AActor* GetInteractableActor() const override { return this; }

	UPROPERTY(VisibleAnywhere, Category = "TV|Npc") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "TV|Npc") TObjectPtr<USkeletalMeshComponent> SkeletalMesh;
	UPROPERTY(VisibleAnywhere, Category = "TV|Npc") TObjectPtr<UStaticMeshComponent> PlaceholderMesh;
};
