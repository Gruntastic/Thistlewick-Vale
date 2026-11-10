// Shared fixtures for Systems automation tests. Rows are built in code so the tests do not need imported data tables.
#pragma once

#include "CoreMinimal.h"
#include "Systems/TVItemRows.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TVSystemsTest
{
	/** Mirrors POOL_PLATE_HEAD-style data from Data/Source/DT_StatPools.json: all eight stats, primaries favoured. */
	inline FTVStatPoolRow MakePlatePool()
	{
		FTVStatPoolRow Pool;
		Pool.StatPoolId = TEXT("POOL_TEST_PLATE_CHEST");
		Pool.Slot = ETVEquipSlot::Chest;
		Pool.ArmorFamily = ETVArmorFamily::Plate;
		auto Add = [&Pool](ETVStat Stat, float Weight, float Min, float Max)
		{
			FTVStatPoolEntry Entry;
			Entry.Stat = Stat;
			Entry.Weight = Weight;
			Entry.MinPerIlvl = Min;
			Entry.MaxPerIlvl = Max;
			Pool.Stats.Add(Entry);
		};
		Add(ETVStat::Strength, 34.f, 0.5f, 0.75f);
		Add(ETVStat::Stamina, 30.f, 0.5f, 0.75f);
		Add(ETVStat::Agility, 4.f, 0.5f, 0.75f);
		Add(ETVStat::Intellect, 2.f, 0.5f, 0.75f);
		Add(ETVStat::Spirit, 3.f, 0.5f, 0.75f);
		Add(ETVStat::Armor, 14.f, 0.8f, 1.2f);
		Add(ETVStat::Crit, 9.f, 0.25f, 0.375f);
		Add(ETVStat::Haste, 6.f, 0.25f, 0.375f);
		return Pool;
	}

	inline FTVItemDefRow MakePlateChestDef()
	{
		FTVItemDefRow Def;
		Def.ItemDefId = TEXT("ITM_TEST_PLATE_CHEST");
		Def.SetTier = ETVSetTier::T2_ValeGuard;
		Def.Slot = ETVEquipSlot::Chest;
		Def.ArmorFamily = ETVArmorFamily::Plate;
		Def.AllowedClasses = { TEXT("CLS_WARRIOR") };
		Def.ItemLevelMin = 9;
		Def.ItemLevelMax = 12;
		Def.StatPoolId = TEXT("POOL_TEST_PLATE_CHEST");
		Def.BaseArmor = 34.f;
		Def.SellValue = 12;
		return Def;
	}
}

#endif
