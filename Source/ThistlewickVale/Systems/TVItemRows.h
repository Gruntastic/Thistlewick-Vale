// Item, stat pool and loot table rows (Docs/DATA_SCHEMAS.md section 4). Owned by the Systems Engineer.
// Field names match the schema exactly; JSON source files in Data/Source/ use the same keys.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Core/TVTypes.h"
#include "TVItemRows.generated.h"

/** DT_ItemDefs row. Row name = ItemDefId. */
USTRUCT(BlueprintType)
struct THISTLEWICKVALE_API FTVItemDefRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Same as the row name. Optional in source data; code always uses the row name (see UTVLootLibrary::FindItemDef). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Item") FName ItemDefId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Item") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Item") ETVSetTier SetTier = ETVSetTier::None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Item") ETVEquipSlot Slot = ETVEquipSlot::None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Item") ETVArmorFamily ArmorFamily = ETVArmorFamily::None;
	/** Class IDs (CLS_WARRIOR ...). Empty = usable by every class. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Item") TArray<FName> AllowedClasses;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Item") int32 ItemLevelMin = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Item") int32 ItemLevelMax = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Item") FName MeshId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Item") FName MaterialInstanceId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Item") FName IconId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Item") FName StatPoolId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Item") float BaseArmor = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Item") float BaseWeaponDamage = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Item") int32 SellValue = 0;

	bool IsUsableByClass(FName ClassId) const { return AllowedClasses.Num() == 0 || AllowedClasses.Contains(ClassId); }
};

/** One weighted stat in a stat pool. */
USTRUCT(BlueprintType)
struct THISTLEWICKVALE_API FTVStatPoolEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Item") ETVStat Stat = ETVStat::None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Item") float Weight = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Item") float MinPerIlvl = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Item") float MaxPerIlvl = 1.f;
};

/** DT_StatPools row. Row name = StatPoolId (referenced by FTVItemDefRow::StatPoolId). */
USTRUCT(BlueprintType)
struct THISTLEWICKVALE_API FTVStatPoolRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Same as the row name (present in Data/Source/DT_StatPools.json). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Item") FName StatPoolId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Item") ETVEquipSlot Slot = ETVEquipSlot::None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Item") ETVArmorFamily ArmorFamily = ETVArmorFamily::None;
	/** The schema leaves this array unnamed; it is called Stats. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Item") TArray<FTVStatPoolEntry> Stats;
};

USTRUCT(BlueprintType)
struct THISTLEWICKVALE_API FTVLootEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Loot") FName ItemDefId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Loot") float Weight = 1.f;
};

USTRUCT(BlueprintType)
struct THISTLEWICKVALE_API FTVRarityWeight
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Loot") ETVRarity Rarity = ETVRarity::Common;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Loot") float Weight = 1.f;
};

/** DT_LootTables row. Row name = LootTableId. Epic is only ever rolled when RarityWeights lists it with Weight > 0. */
USTRUCT(BlueprintType)
struct THISTLEWICKVALE_API FTVLootTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Loot") FName LootTableId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Loot") int32 GoldMin = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Loot") int32 GoldMax = 0;
	/** Chance (0-1) of one random drop on top of GuaranteedDrops. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Loot") float DropChance = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Loot") TArray<FTVLootEntry> Entries;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Loot") TArray<FTVRarityWeight> RarityWeights;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Loot") int32 ItemLevelMin = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Loot") int32 ItemLevelMax = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Loot") int32 GuaranteedDrops = 0;
};
