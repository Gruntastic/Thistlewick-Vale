#include "Systems/TVLootLibrary.h"
#include "Core/TVProjectSettings.h"
#include "ThistlewickVale.h"
#include "Engine/DataTable.h"
#include "Math/RandomStream.h"
#include "Math/UnrealMathUtility.h"
#include "HAL/PlatformTime.h"
#include "Misc/Guid.h"

namespace TVLoot
{
	static const FName KeyItemDefs(TEXT("ItemDefs"));
	static const FName KeyStatPools(TEXT("StatPools"));
	static const FName KeyLootTables(TEXT("LootTables"));

	template <typename RowType>
	static bool CopyRow(FName Key, FName RowName, RowType& Out)
	{
		if (RowName.IsNone())
		{
			return false;
		}
		const UDataTable* Table = UTVProjectSettings::Get()->LoadTable(Key);
		if (!Table)
		{
			return false;
		}
		if (const RowType* Row = Table->FindRow<RowType>(RowName, TEXT("TVLootLibrary"), /*bWarnIfRowMissing*/ false))
		{
			Out = *Row;
			return true;
		}
		return false;
	}

	/** Weighted pick over Candidates (indices into Weights). Returns INDEX_NONE if all weights are zero. */
	static int32 PickWeighted(const TArray<int32>& Candidates, TFunctionRef<float(int32)> GetWeight, const FRandomStream& Stream)
	{
		double Total = 0.0;
		for (const int32 Index : Candidates)
		{
			Total += FMath::Max(0.f, GetWeight(Index));
		}
		if (Total <= 0.0)
		{
			return INDEX_NONE;
		}
		const double Roll = static_cast<double>(Stream.FRand()) * Total;
		double Running = 0.0;
		int32 LastPositive = INDEX_NONE;
		for (const int32 Index : Candidates)
		{
			const float W = FMath::Max(0.f, GetWeight(Index));
			if (W <= 0.f)
			{
				continue;
			}
			LastPositive = Index;
			Running += W;
			if (Roll < Running)
			{
				return Index;
			}
		}
		return LastPositive; // FRand can return values that round to Total
	}
}

bool UTVLootLibrary::GetItemDef(FName ItemDefId, FTVItemDefRow& OutDef)
{
	if (TVLoot::CopyRow(TVLoot::KeyItemDefs, ItemDefId, OutDef))
	{
		OutDef.ItemDefId = ItemDefId;
		return true;
	}
	return false;
}

bool UTVLootLibrary::GetStatPool(FName StatPoolId, FTVStatPoolRow& OutPool)
{
	return TVLoot::CopyRow(TVLoot::KeyStatPools, StatPoolId, OutPool);
}

bool UTVLootLibrary::GetLootTable(FName LootTableId, FTVLootTableRow& OutTable)
{
	if (TVLoot::CopyRow(TVLoot::KeyLootTables, LootTableId, OutTable))
	{
		OutTable.LootTableId = LootTableId;
		return true;
	}
	return false;
}

bool UTVLootLibrary::IsPrimaryStat(ETVStat Stat)
{
	switch (Stat)
	{
	case ETVStat::Stamina:
	case ETVStat::Strength:
	case ETVStat::Agility:
	case ETVStat::Intellect:
	case ETVStat::Spirit:
		return true;
	default:
		return false;
	}
}

float UTVLootLibrary::GetRarityMultiplier(ETVRarity Rarity)
{
	switch (Rarity)
	{
	case ETVRarity::Common:   return 1.0f;
	case ETVRarity::Uncommon: return 1.1f;
	case ETVRarity::Rare:     return 1.25f;
	case ETVRarity::Epic:     return 1.45f;
	default:                  return 1.0f;
	}
}

int32 UTVLootLibrary::GetStatCount(ETVRarity Rarity)
{
	return FMath::Clamp(static_cast<int32>(Rarity), 0, 4);
}

FTVItemInstance UTVLootLibrary::RollItemWithPool(FName ItemDefId, const FTVItemDefRow& Def, const FTVStatPoolRow& Pool, ETVRarity Rarity, int32 ItemLevel, int32 Seed)
{
	FTVItemInstance Item;
	Item.ItemDefId = ItemDefId;
	Item.SetTier = Def.SetTier;
	Item.Slot = Def.Slot;
	Item.Rarity = Rarity;
	Item.ItemLevel = ItemLevel;
	Item.Seed = Seed;

	// 1. Stream from the seed. Nothing else may draw from it, or the roll stops being reproducible.
	const FRandomStream Stream(Seed);

	// Candidate entries: valid stat, positive weight, first occurrence of each stat only (no duplicates possible).
	TArray<int32> Candidates;
	TSet<ETVStat> SeenStats;
	for (int32 i = 0; i < Pool.Stats.Num(); ++i)
	{
		const FTVStatPoolEntry& Entry = Pool.Stats[i];
		if (Entry.Stat != ETVStat::None && Entry.Weight > 0.f && !SeenStats.Contains(Entry.Stat))
		{
			SeenStats.Add(Entry.Stat);
			Candidates.Add(i);
		}
	}

	// 2. Stat count = rarity count.
	const int32 StatCount = FMath::Min(GetStatCount(Rarity), Candidates.Num());

	// 3. Draw without replacement. First draw is primary-only when possible.
	TArray<int32> Drawn;
	Drawn.Reserve(StatCount);
	auto WeightOf = [&Pool](int32 Index) { return Pool.Stats[Index].Weight; };
	for (int32 DrawIndex = 0; DrawIndex < StatCount; ++DrawIndex)
	{
		const TArray<int32>* DrawFrom = &Candidates;
		TArray<int32> Primaries;
		if (DrawIndex == 0)
		{
			for (const int32 Index : Candidates)
			{
				if (IsPrimaryStat(Pool.Stats[Index].Stat))
				{
					Primaries.Add(Index);
				}
			}
			if (Primaries.Num() > 0)
			{
				DrawFrom = &Primaries;
			}
		}
		const int32 Picked = TVLoot::PickWeighted(*DrawFrom, WeightOf, Stream);
		if (Picked == INDEX_NONE)
		{
			break;
		}
		Drawn.Add(Picked);
		Candidates.Remove(Picked);
	}

	// 4. Values, in draw order.
	const float RarityMultiplier = GetRarityMultiplier(Rarity);
	for (const int32 Index : Drawn)
	{
		const FTVStatPoolEntry& Entry = Pool.Stats[Index];
		const double PerIlvl = Stream.FRandRange(Entry.MinPerIlvl, Entry.MaxPerIlvl);
		const int32 Value = FMath::Max(1, static_cast<int32>(FMath::RoundToDouble(PerIlvl * static_cast<double>(ItemLevel) * static_cast<double>(RarityMultiplier))));
		FTVRolledStat Rolled;
		Rolled.Stat = Entry.Stat;
		Rolled.Value = Value;
		Item.RolledStats.Add(Rolled);
	}
	return Item;
}

FTVItemInstance UTVLootLibrary::RollItem(const FTVItemDefRow& Def, ETVRarity Rarity, int32 ItemLevel, int32 Seed)
{
	FTVStatPoolRow Pool;
	if (!GetStatPool(Def.StatPoolId, Pool))
	{
		UE_LOG(LogTV, Warning, TEXT("RollItem: stat pool '%s' for item '%s' not found; item gets no stats."), *Def.StatPoolId.ToString(), *Def.ItemDefId.ToString());
	}
	return RollItemWithPool(Def.ItemDefId, Def, Pool, Rarity, ItemLevel, Seed);
}

bool UTVLootLibrary::RollItemById(FName ItemDefId, ETVRarity Rarity, int32 ItemLevel, int32 Seed, FTVItemInstance& OutItem)
{
	FTVItemDefRow Def;
	FTVStatPoolRow Pool;
	if (!GetItemDef(ItemDefId, Def) || !GetStatPool(Def.StatPoolId, Pool))
	{
		return false;
	}
	OutItem = RollItemWithPool(ItemDefId, Def, Pool, Rarity, ItemLevel, Seed);
	return true;
}

int32 UTVLootLibrary::MakeServerSeed()
{
	// Mixes the global RNG with the cycle counter so two players rolling in the same frame differ.
	const uint32 Mixed = static_cast<uint32>(FMath::Rand()) * 2654435761u ^ FPlatformTime::Cycles();
	const int32 Seed = static_cast<int32>(Mixed & 0x7FFFFFFF);
	return Seed == 0 ? 1 : Seed;
}

bool UTVLootLibrary::ValidateItemWithPool(const FTVItemInstance& Item, const FTVItemDefRow& Def, const FTVStatPoolRow& Pool, FString* OutReason)
{
	auto Fail = [OutReason](const FString& Reason)
	{
		if (OutReason)
		{
			*OutReason = Reason;
		}
		return false;
	};

	if (Item.ItemDefId.IsNone())
	{
		return Fail(TEXT("missing ItemDefId"));
	}
	if (Item.Rarity < ETVRarity::Common || Item.Rarity > ETVRarity::Epic)
	{
		return Fail(TEXT("invalid rarity"));
	}
	if (Item.Slot != Def.Slot || Item.SetTier != Def.SetTier)
	{
		return Fail(TEXT("slot or set tier differs from the item definition"));
	}
	const int32 MinLevel = Def.ItemLevelMin;
	const int32 MaxLevel = FMath::Max(Def.ItemLevelMin, Def.ItemLevelMax);
	if (Item.ItemLevel < MinLevel || Item.ItemLevel > MaxLevel)
	{
		return Fail(FString::Printf(TEXT("item level %d outside %d-%d"), Item.ItemLevel, MinLevel, MaxLevel));
	}

	const FTVItemInstance Expected = RollItemWithPool(Item.ItemDefId, Def, Pool, Item.Rarity, Item.ItemLevel, Item.Seed);
	if (Expected.RolledStats != Item.RolledStats)
	{
		return Fail(TEXT("rolled stats do not match the seed"));
	}
	return true;
}

bool UTVLootLibrary::ValidateItem(const FTVItemInstance& Item, FString* OutReason)
{
	FTVItemDefRow Def;
	if (!GetItemDef(Item.ItemDefId, Def))
	{
		if (OutReason)
		{
			*OutReason = FString::Printf(TEXT("unknown item definition '%s'"), *Item.ItemDefId.ToString());
		}
		return false;
	}
	FTVStatPoolRow Pool;
	if (!GetStatPool(Def.StatPoolId, Pool))
	{
		if (OutReason)
		{
			*OutReason = FString::Printf(TEXT("unknown stat pool '%s'"), *Def.StatPoolId.ToString());
		}
		return false;
	}
	if (!Item.InstanceGuid.IsValid())
	{
		if (OutReason)
		{
			*OutReason = TEXT("invalid InstanceGuid");
		}
		return false;
	}
	return ValidateItemWithPool(Item, Def, Pool, OutReason);
}

ETVRarity UTVLootLibrary::PickRarity(const TArray<FTVRarityWeight>& Weights, const FRandomStream& Stream)
{
	TArray<int32> Candidates;
	for (int32 i = 0; i < Weights.Num(); ++i)
	{
		if (Weights[i].Rarity != ETVRarity::None && Weights[i].Weight > 0.f)
		{
			Candidates.Add(i);
		}
	}
	const int32 Picked = TVLoot::PickWeighted(Candidates, [&Weights](int32 Index) { return Weights[Index].Weight; }, Stream);
	return Picked == INDEX_NONE ? ETVRarity::None : Weights[Picked].Rarity;
}

FTVLootResult UTVLootLibrary::RollLootFromRow(const FTVLootTableRow& Table, int32 Seed, FName ClassId)
{
	FTVLootResult Result;
	const FRandomStream Stream(Seed);

	const int32 GoldMin = FMath::Max(0, Table.GoldMin);
	const int32 GoldMax = FMath::Max(GoldMin, Table.GoldMax);
	Result.Gold = Stream.RandRange(GoldMin, GoldMax);

	int32 DropCount = FMath::Max(0, Table.GuaranteedDrops);
	if (Table.DropChance > 0.f && Stream.FRand() < Table.DropChance)
	{
		++DropCount;
	}
	if (DropCount == 0 || Table.Entries.Num() == 0)
	{
		return Result;
	}

	// Entry candidates, preferring items this class can use (personal loot).
	TArray<int32> AllEntries;
	TArray<int32> ClassEntries;
	TMap<FName, FTVItemDefRow> DefCache;
	for (int32 i = 0; i < Table.Entries.Num(); ++i)
	{
		const FTVLootEntry& Entry = Table.Entries[i];
		FTVItemDefRow Def;
		if (Entry.Weight <= 0.f || !GetItemDef(Entry.ItemDefId, Def))
		{
			continue;
		}
		AllEntries.Add(i);
		if (ClassId.IsNone() || Def.IsUsableByClass(ClassId))
		{
			ClassEntries.Add(i);
		}
		DefCache.Add(Entry.ItemDefId, Def);
	}
	const TArray<int32>& Entries = ClassEntries.Num() > 0 ? ClassEntries : AllEntries;

	for (int32 Drop = 0; Drop < DropCount; ++Drop)
	{
		const int32 EntryIndex = TVLoot::PickWeighted(Entries, [&Table](int32 Index) { return Table.Entries[Index].Weight; }, Stream);
		const ETVRarity Rarity = PickRarity(Table.RarityWeights, Stream);
		const int32 TableIlvlMin = FMath::Max(1, Table.ItemLevelMin);
		const int32 TableIlvl = Stream.RandRange(TableIlvlMin, FMath::Max(TableIlvlMin, Table.ItemLevelMax));
		const int32 ItemSeed = static_cast<int32>(Stream.GetUnsignedInt() & 0x7FFFFFFF) | 1;
		if (EntryIndex == INDEX_NONE || Rarity == ETVRarity::None)
		{
			continue;
		}

		const FTVItemDefRow& Def = DefCache.FindChecked(Table.Entries[EntryIndex].ItemDefId);
		const int32 ItemLevel = FMath::Clamp(TableIlvl, Def.ItemLevelMin, FMath::Max(Def.ItemLevelMin, Def.ItemLevelMax));
		FTVItemInstance Item = RollItem(Def, Rarity, ItemLevel, ItemSeed);
		AssignNewGuid(Item);
		Result.Items.Add(MoveTemp(Item));
	}
	return Result;
}

TArray<FTVItemInstance> UTVLootLibrary::RollLoot(FName LootTableId, int32 Seed)
{
	FTVLootTableRow Table;
	if (!GetLootTable(LootTableId, Table))
	{
		UE_LOG(LogTV, Warning, TEXT("RollLoot: loot table '%s' not found."), *LootTableId.ToString());
		return {};
	}
	return RollLootFromRow(Table, Seed).Items;
}

FTVLootResult UTVLootLibrary::RollLootForClass(FName LootTableId, int32 Seed, FName ClassId)
{
	FTVLootTableRow Table;
	if (!GetLootTable(LootTableId, Table))
	{
		UE_LOG(LogTV, Warning, TEXT("RollLoot: loot table '%s' not found."), *LootTableId.ToString());
		return {};
	}
	return RollLootFromRow(Table, Seed, ClassId);
}
