// Deterministic item rolls, item validation and loot-table rolls (Docs/DATA_SCHEMAS.md section 4).
// Everything here is a static, side-effect-free function of its inputs and the data tables, so the server,
// the save loader and clients all derive the same stats from the same Seed.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Core/TVTypes.h"
#include "Systems/TVItemRows.h"
#include "TVLootLibrary.generated.h"

/** Items and gold from one loot-table roll for one player. */
USTRUCT(BlueprintType)
struct THISTLEWICKVALE_API FTVLootResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TV|Loot") TArray<FTVItemInstance> Items;
	UPROPERTY(BlueprintReadOnly, Category = "TV|Loot") int32 Gold = 0;
};

UCLASS()
class THISTLEWICKVALE_API UTVLootLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ---- Table access (keys from DATA_SCHEMAS section 12) ----

	/** Copies the DT_ItemDefs row. OutDef.ItemDefId is set to the row name. */
	static bool GetItemDef(FName ItemDefId, FTVItemDefRow& OutDef);
	static bool GetStatPool(FName StatPoolId, FTVStatPoolRow& OutPool);
	static bool GetLootTable(FName LootTableId, FTVLootTableRow& OutTable);

	static bool IsPrimaryStat(ETVStat Stat);
	/** Common 1.0, Uncommon 1.1, Rare 1.25, Epic 1.45. */
	static float GetRarityMultiplier(ETVRarity Rarity);
	/** Number of rolled stats: Common 1 ... Epic 4. */
	static int32 GetStatCount(ETVRarity Rarity);

	// ---- Item roll (pure) ----

	/**
	 * Core roll, no table access. Algorithm (DATA_SCHEMAS section 4):
	 *  1. FRandomStream(Seed).
	 *  2. Stat count = rarity count (1-4), capped by the number of distinct stats in the pool.
	 *  3. Draw stats by weight without replacement; the first draw is restricted to primary stats when the pool has any.
	 *  4. For each drawn stat in order: Value = round(FRandRange(MinPerIlvl, MaxPerIlvl) x ItemLevel x RarityMultiplier), minimum 1.
	 * InstanceGuid is left invalid; callers that create a new item call AssignNewGuid().
	 */
	static FTVItemInstance RollItemWithPool(FName ItemDefId, const FTVItemDefRow& Def, const FTVStatPoolRow& Pool, ETVRarity Rarity, int32 ItemLevel, int32 Seed);

	/** RollItem(Def, Rarity, ItemLevel, Seed) as named in the schema. Looks up Def.StatPoolId in DT_StatPools. Def.ItemDefId must be set (GetItemDef does that). */
	static FTVItemInstance RollItem(const FTVItemDefRow& Def, ETVRarity Rarity, int32 ItemLevel, int32 Seed);

	/** Convenience: looks up the def by ID. False if the def or its stat pool is missing. */
	UFUNCTION(BlueprintCallable, Category = "TV|Loot")
	static bool RollItemById(FName ItemDefId, ETVRarity Rarity, int32 ItemLevel, int32 Seed, FTVItemInstance& OutItem);

	static void AssignNewGuid(FTVItemInstance& Item) { Item.InstanceGuid = FGuid::NewGuid(); }

	/** Server-side seed for a new item. Positive, never 0. */
	static int32 MakeServerSeed();

	// ---- Validation ----

	/** Re-derives the item from (ItemDefId, Rarity, ItemLevel, Seed) and compares every derived field. */
	static bool ValidateItemWithPool(const FTVItemInstance& Item, const FTVItemDefRow& Def, const FTVStatPoolRow& Pool, FString* OutReason = nullptr);

	/** Table-backed validation. Mismatch = item must be rejected (callers log the reason). */
	static bool ValidateItem(const FTVItemInstance& Item, FString* OutReason = nullptr);

	UFUNCTION(BlueprintPure, Category = "TV|Loot", meta = (DisplayName = "Validate Item"))
	static bool K2_ValidateItem(const FTVItemInstance& Item) { return ValidateItem(Item); }

	// ---- Loot tables ----

	/**
	 * Rolls one loot table for one player. Drops = GuaranteedDrops + (one more if FRand < DropChance).
	 * Each drop: item by entry weight (entries the class cannot use are skipped when ClassId is set and at least one usable entry exists),
	 * rarity by RarityWeights (Epic only if the table lists it), item level in the table range clamped to the item's range, item seed from the stream.
	 * Gold = RandRange(GoldMin, GoldMax). New items get fresh InstanceGuids.
	 */
	static FTVLootResult RollLootFromRow(const FTVLootTableRow& Table, int32 Seed, FName ClassId = NAME_None);

	/** Ledger signature: items only, any class. */
	static TArray<FTVItemInstance> RollLoot(FName LootTableId, int32 Seed);

	/** Items and gold, filtered to items usable by ClassId. */
	static FTVLootResult RollLootForClass(FName LootTableId, int32 Seed, FName ClassId);

	/** Weighted rarity pick from a table row. Returns None if the table has no positive weights. */
	static ETVRarity PickRarity(const TArray<FTVRarityWeight>& Weights, const FRandomStream& Stream);
};
