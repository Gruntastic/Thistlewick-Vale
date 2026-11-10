// Quest log on ATVPlayerState. Progress is server-only; the log replicates to the owner.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/TVTypes.h"
#include "Systems/TVItemRows.h"
#include "TVQuestComponent.generated.h"

class ATVPlayerState;
struct FTVQuestRow;

/** One quest in the log. Progress has one counter per objective, same order as FTVQuestRow::Objectives. */
USTRUCT(BlueprintType)
struct THISTLEWICKVALE_API FTVQuestProgress
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TV|Quest") FName QuestId;
	UPROPERTY(BlueprintReadOnly, Category = "TV|Quest") ETVQuestState State = ETVQuestState::NotStarted;
	UPROPERTY(BlueprintReadOnly, Category = "TV|Quest") TArray<int32> Progress;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTVOnQuestsChanged);

UCLASS(ClassGroup = (TV), meta = (BlueprintSpawnableComponent))
class THISTLEWICKVALE_API UTVQuestComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTVQuestComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Player must be near the giver NPC (if the quest has one), meet RequiredLevel and have every prerequisite completed. */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "TV|Quest")
	void ServerAcceptQuest(FName QuestId);

	/** Quest must be ReadyToTurnIn, player near the turn-in NPC, and the bag must have room for the reward items. */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "TV|Quest")
	void ServerTurnInQuest(FName QuestId);

	/** Drop an active quest (progress lost). */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "TV|Quest")
	void ServerAbandonQuest(FName QuestId);

	UFUNCTION(BlueprintPure, Category = "TV|Quest")
	ETVQuestState GetQuestState(FName QuestId) const;

	const FTVQuestProgress* GetQuestProgress(FName QuestId) const;
	const TArray<FTVQuestProgress>& GetQuests() const { return Quests; }

	UFUNCTION(BlueprintPure, Category = "TV|Quest", meta = (DisplayName = "Get Quests"))
	TArray<FTVQuestProgress> K2_GetQuests() const { return Quests; }

	/** Server-side checks (also usable by UI on the client for greying out). */
	bool CanAcceptQuest(FName QuestId, FString* OutReason = nullptr) const;

	// ---- Progress (server) ----
	void NotifyKill(FName MobId);
	void NotifyCollect(FName ItemOrNodeId);
	void NotifyInteract(FName TargetId);
	/** Event and Defend objectives (e.g. "DEFEND_STAG_SHRINE" fired by the shrine encounter when all waves die). */
	void NotifyEvent(FName EventId);

	/** True if an Active quest still needs progress of this type on TargetId (quest nodes use it to gate interaction). */
	bool WantsTarget(ETVQuestObjectiveType Type, FName TargetId) const;

	// ---- Save support (server) ----
	void RestoreFromSave(const TArray<FTVQuestProgress>& InQuests, const TArray<FName>& InWorldFlags);
	/** World flags this character has set through quests (saved per character, re-applied to ATVGameState on load). */
	const TArray<FName>& GetWorldFlags() const { return WorldFlags; }

	UPROPERTY(BlueprintAssignable, Category = "TV|Quest")
	FTVOnQuestsChanged OnQuestsChanged;

	/** Rarity weights for reward items whose Rarity is "Roll". */
	UPROPERTY(EditDefaultsOnly, Category = "TV|Quest")
	TArray<FTVRarityWeight> RollRarityWeights;

	/** Max distance (cm) from the giver / turn-in NPC. */
	UPROPERTY(EditDefaultsOnly, Category = "TV|Quest")
	float NpcRangeCm = 800.f;

	static bool GetQuestRow(FName QuestId, FTVQuestRow& OutRow);

protected:
	UFUNCTION() void OnRep_Quests();

	ATVPlayerState* GetTVPlayerState() const;
	bool HasAuthority() const;
	FTVQuestProgress* FindMutable(FName QuestId);
	bool IsNearNpc(FName NpcId) const;
	void AddProgress(ETVQuestObjectiveType Type, FName TargetId, int32 Amount);
	void GrantRewards(const FTVQuestRow& Row);
	ETVRarity ResolveRewardRarity(FName RarityName) const;
	void ApplyWorldFlag(FName Flag);

	UPROPERTY(ReplicatedUsing = OnRep_Quests)
	TArray<FTVQuestProgress> Quests;

	UPROPERTY(Replicated)
	TArray<FName> WorldFlags;
};
