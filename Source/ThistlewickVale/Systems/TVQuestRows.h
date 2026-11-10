// Quest, NPC and dialogue rows (Docs/DATA_SCHEMAS.md section 6). Owned by the Systems Engineer.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Core/TVTypes.h"
#include "TVQuestRows.generated.h"

USTRUCT(BlueprintType)
struct THISTLEWICKVALE_API FTVQuestObjective
{
	GENERATED_BODY()

	/** Kill, Collect, Interact, Defend (the schema's "Escort/Defend"), Event. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Quest") ETVQuestObjectiveType Type = ETVQuestObjectiveType::Kill;
	/** MobId for Kill, node ID for Collect, NPC or node ID for Interact, event ID for Defend and Event. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Quest") FName TargetId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Quest") int32 Count = 1;
	/** Area hint for the tracker/minimap. The schema gives no type; FName is used. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Quest") FName AreaTag;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Quest") FText TrackerText;
};

USTRUCT(BlueprintType)
struct THISTLEWICKVALE_API FTVQuestRewardItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Quest") FName ItemDefId;
	/** "Common", "Uncommon", "Rare", "Epic", or "Roll" (server picks with UTVQuestComponent::RollRarityWeights). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Quest") FName Rarity = TEXT("Roll");
};

/** DT_Quests row. Row name = QuestId. */
USTRUCT(BlueprintType)
struct THISTLEWICKVALE_API FTVQuestRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Quest") FName QuestId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Quest") FText Title;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Quest") FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Quest") FText CompletionText;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Quest") FName GiverNpcId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Quest") FName TurnInNpcId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Quest") int32 RequiredLevel = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Quest") TArray<FName> PrereqQuestIds;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Quest") TArray<FTVQuestObjective> Objectives;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Quest") int32 RewardXP = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Quest") int32 RewardGold = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Quest") TArray<FTVQuestRewardItem> RewardItems;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Quest") FName RewardMount;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Quest") FName SetsWorldFlag;
};

/** DT_Npcs row. Row name = NpcId. */
USTRUCT(BlueprintType)
struct THISTLEWICKVALE_API FTVNpcRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Npc") FName NpcId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Npc") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Npc") FName MeshId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Npc") FText Greeting;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Npc") TArray<FName> QuestIds;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Npc") bool IsVendor = false;
	/** ItemDefIds the vendor sells. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Npc") TArray<FName> VendorItems;
};

UENUM(BlueprintType)
enum class ETVDialogueAction : uint8 { AcceptQuest, CompleteQuest, OpenVendor, Close };

USTRUCT(BlueprintType)
struct THISTLEWICKVALE_API FTVDialogueOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Dialogue") FText Text;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Dialogue") ETVDialogueAction ActionType = ETVDialogueAction::Close;
	/** QuestId for AcceptQuest / CompleteQuest, otherwise unused. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Dialogue") FName Param;
};

/** DT_Dialogue row. Row name = DialogueId. */
USTRUCT(BlueprintType)
struct THISTLEWICKVALE_API FTVDialogueRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Dialogue") FName DialogueId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Dialogue") FName NpcId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Dialogue") TArray<FText> Lines;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Dialogue") TArray<FTVDialogueOption> Options;
};
