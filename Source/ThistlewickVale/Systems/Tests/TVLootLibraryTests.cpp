// Roll determinism and validator tests (DATA_SCHEMAS section 4). Run: see Tests/Systems/README.md.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Math/RandomStream.h"
#include "Systems/TVLootLibrary.h"
#include "Systems/Tests/TVSystemsTestData.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// Verified in 5.8 Misc/AutomationTest.h: EAutomationTestFlags is an enum class with ENUM_CLASS_FLAGS,
	// EAutomationTestFlags_ApplicationContextMask is an inline constexpr, ProductFilter exists.
	constexpr EAutomationTestFlags TVTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	const ETVRarity AllRarities[] = { ETVRarity::Common, ETVRarity::Uncommon, ETVRarity::Rare, ETVRarity::Epic };

	FTVItemInstance Roll(ETVRarity Rarity, int32 ItemLevel, int32 Seed)
	{
		const FTVItemDefRow Def = TVSystemsTest::MakePlateChestDef();
		const FTVStatPoolRow Pool = TVSystemsTest::MakePlatePool();
		return UTVLootLibrary::RollItemWithPool(Def.ItemDefId, Def, Pool, Rarity, ItemLevel, Seed);
	}

	bool SameDerivedFields(const FTVItemInstance& A, const FTVItemInstance& B)
	{
		return A.ItemDefId == B.ItemDefId && A.SetTier == B.SetTier && A.Slot == B.Slot && A.Rarity == B.Rarity
			&& A.ItemLevel == B.ItemLevel && A.Seed == B.Seed && A.RolledStats == B.RolledStats;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTVLootRollSameSeedTest, "ThistlewickVale.Systems.Loot.SameSeedSameItem", TVTestFlags)
bool FTVLootRollSameSeedTest::RunTest(const FString& Parameters)
{
	for (const ETVRarity Rarity : AllRarities)
	{
		for (int32 Seed = 1; Seed <= 500; ++Seed)
		{
			const FTVItemInstance A = Roll(Rarity, 10, Seed * 7919);
			const FTVItemInstance B = Roll(Rarity, 10, Seed * 7919);
			if (!SameDerivedFields(A, B))
			{
				AddError(FString::Printf(TEXT("Seed %d rarity %d rolled two different items."), Seed * 7919, static_cast<int32>(Rarity)));
				return false;
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTVLootRollDifferentSeedsTest, "ThistlewickVale.Systems.Loot.DifferentSeedsDiffer", TVTestFlags)
bool FTVLootRollDifferentSeedsTest::RunTest(const FString& Parameters)
{
	// Single rolls can collide by chance (few stats, small values), so check the spread over many seeds.
	for (const ETVRarity Rarity : AllRarities)
	{
		TSet<FString> Unique;
		const int32 Samples = 200;
		for (int32 Seed = 1; Seed <= Samples; ++Seed)
		{
			const FTVItemInstance Item = Roll(Rarity, 12, Seed);
			FString Key;
			for (const FTVRolledStat& Stat : Item.RolledStats)
			{
				Key += FString::Printf(TEXT("%d:%d,"), static_cast<int32>(Stat.Stat), Stat.Value);
			}
			Unique.Add(Key);
		}
		TestTrue(FString::Printf(TEXT("Rarity %d: %d distinct results out of %d seeds"), static_cast<int32>(Rarity), Unique.Num(), Samples), Unique.Num() >= 10);
	}
	// And a direct pair check on an Epic item, where a full collision is practically impossible.
	TestFalse(TEXT("Epic seeds 1 and 2 differ"), Roll(ETVRarity::Epic, 15, 1).RolledStats == Roll(ETVRarity::Epic, 15, 2).RolledStats);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTVLootRollStatRulesTest, "ThistlewickVale.Systems.Loot.StatCountAndUniqueness", TVTestFlags)
bool FTVLootRollStatRulesTest::RunTest(const FString& Parameters)
{
	const FTVStatPoolRow Pool = TVSystemsTest::MakePlatePool();
	for (const ETVRarity Rarity : AllRarities)
	{
		const float Mult = UTVLootLibrary::GetRarityMultiplier(Rarity);
		for (int32 Seed = 1; Seed <= 300; ++Seed)
		{
			const int32 ItemLevel = 9 + Seed % 4;
			const FTVItemInstance Item = Roll(Rarity, ItemLevel, Seed * 104729);
			if (Item.RolledStats.Num() != static_cast<int32>(Rarity))
			{
				AddError(FString::Printf(TEXT("Rarity %d rolled %d stats."), static_cast<int32>(Rarity), Item.RolledStats.Num()));
				return false;
			}
			if (!UTVLootLibrary::IsPrimaryStat(Item.RolledStats[0].Stat))
			{
				AddError(TEXT("First rolled stat is not a primary stat."));
				return false;
			}
			TSet<ETVStat> Seen;
			for (const FTVRolledStat& Stat : Item.RolledStats)
			{
				if (Seen.Contains(Stat.Stat))
				{
					AddError(TEXT("Duplicate stat in one item."));
					return false;
				}
				Seen.Add(Stat.Stat);

				const FTVStatPoolEntry* Entry = Pool.Stats.FindByPredicate([&Stat](const FTVStatPoolEntry& E) { return E.Stat == Stat.Stat; });
				if (!Entry)
				{
					AddError(TEXT("Rolled a stat that is not in the pool."));
					return false;
				}
				// Same double math as the roller.
				const int32 Lo = FMath::Max(1, static_cast<int32>(FMath::RoundToDouble(static_cast<double>(Entry->MinPerIlvl) * ItemLevel * static_cast<double>(Mult))));
				const int32 Hi = FMath::Max(1, static_cast<int32>(FMath::RoundToDouble(static_cast<double>(Entry->MaxPerIlvl) * ItemLevel * static_cast<double>(Mult))));
				if (Stat.Value < Lo || Stat.Value > Hi)
				{
					AddError(FString::Printf(TEXT("Value %d outside [%d, %d]."), Stat.Value, Lo, Hi));
					return false;
				}
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTVLootValidatorTest, "ThistlewickVale.Systems.Loot.ValidatorRejectsTampering", TVTestFlags)
bool FTVLootValidatorTest::RunTest(const FString& Parameters)
{
	const FTVItemDefRow Def = TVSystemsTest::MakePlateChestDef();
	const FTVStatPoolRow Pool = TVSystemsTest::MakePlatePool();
	FTVItemInstance Good = Roll(ETVRarity::Rare, 11, 424242);
	UTVLootLibrary::AssignNewGuid(Good);

	FString Reason;
	TestTrue(TEXT("Untouched item validates"), UTVLootLibrary::ValidateItemWithPool(Good, Def, Pool, &Reason));

	FTVItemInstance Bad = Good;
	Bad.RolledStats[0].Value += 1;
	TestFalse(TEXT("Raised stat value is rejected"), UTVLootLibrary::ValidateItemWithPool(Bad, Def, Pool));

	Bad = Good;
	Bad.RolledStats[1].Stat = Bad.RolledStats[1].Stat == ETVStat::Haste ? ETVStat::Crit : ETVStat::Haste;
	TestFalse(TEXT("Swapped stat type is rejected"), UTVLootLibrary::ValidateItemWithPool(Bad, Def, Pool));

	Bad = Good;
	Bad.Rarity = ETVRarity::Epic;
	TestFalse(TEXT("Raised rarity is rejected"), UTVLootLibrary::ValidateItemWithPool(Bad, Def, Pool));

	Bad = Good;
	Bad.ItemLevel = 30;
	TestFalse(TEXT("Item level outside the definition is rejected"), UTVLootLibrary::ValidateItemWithPool(Bad, Def, Pool));

	Bad = Good;
	FTVRolledStat Extra;
	Extra.Stat = ETVStat::Spirit;
	Extra.Value = 50;
	Bad.RolledStats.Add(Extra);
	TestFalse(TEXT("Extra stat is rejected"), UTVLootLibrary::ValidateItemWithPool(Bad, Def, Pool));

	Bad = Good;
	Bad.Slot = ETVEquipSlot::MainHand;
	TestFalse(TEXT("Changed slot is rejected"), UTVLootLibrary::ValidateItemWithPool(Bad, Def, Pool));

	// A different seed with the same stats must fail unless that seed happens to derive identical stats.
	Bad = Good;
	Bad.Seed = Good.Seed + 1;
	const bool bSeedCollides = Roll(Good.Rarity, Good.ItemLevel, Bad.Seed).RolledStats == Good.RolledStats;
	if (!bSeedCollides)
	{
		TestFalse(TEXT("Changed seed is rejected"), UTVLootLibrary::ValidateItemWithPool(Bad, Def, Pool));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTVLootRarityPickTest, "ThistlewickVale.Systems.Loot.EpicOnlyWhenAllowed", TVTestFlags)
bool FTVLootRarityPickTest::RunTest(const FString& Parameters)
{
	TArray<FTVRarityWeight> NoEpic;
	const ETVRarity Rarities[] = { ETVRarity::Common, ETVRarity::Uncommon, ETVRarity::Rare };
	const float Weights[] = { 70.f, 27.f, 3.f };
	for (int32 i = 0; i < 3; ++i)
	{
		FTVRarityWeight W;
		W.Rarity = Rarities[i];
		W.Weight = Weights[i];
		NoEpic.Add(W);
	}
	TArray<FTVRarityWeight> WithEpic = NoEpic;
	FTVRarityWeight Epic;
	Epic.Rarity = ETVRarity::Epic;
	Epic.Weight = 50.f;
	WithEpic.Add(Epic);

	const FRandomStream StreamA(1234);
	const FRandomStream StreamB(1234);
	int32 EpicsWithout = 0;
	int32 EpicsWith = 0;
	for (int32 i = 0; i < 5000; ++i)
	{
		EpicsWithout += UTVLootLibrary::PickRarity(NoEpic, StreamA) == ETVRarity::Epic ? 1 : 0;
		EpicsWith += UTVLootLibrary::PickRarity(WithEpic, StreamB) == ETVRarity::Epic ? 1 : 0;
	}
	TestEqual(TEXT("No Epic from a table without Epic weight"), EpicsWithout, 0);
	TestTrue(TEXT("Epic appears when the table lists it"), EpicsWith > 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
