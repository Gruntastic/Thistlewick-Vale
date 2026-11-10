// Save format round-trip tests (DATA_SCHEMAS section 8). Run: see Tests/Systems/README.md.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Systems/TVSaveSubsystem.h"
#include "Systems/TVLootLibrary.h"
#include "Systems/Tests/TVSystemsTestData.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags TVSaveTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	FTVItemInstance MakeItem(ETVRarity Rarity, int32 ItemLevel, int32 Seed)
	{
		const FTVItemDefRow Def = TVSystemsTest::MakePlateChestDef();
		const FTVStatPoolRow Pool = TVSystemsTest::MakePlatePool();
		FTVItemInstance Item = UTVLootLibrary::RollItemWithPool(Def.ItemDefId, Def, Pool, Rarity, ItemLevel, Seed);
		UTVLootLibrary::AssignNewGuid(Item);
		return Item;
	}

	FTVCharacterSaveData MakeSample()
	{
		FTVCharacterSaveData Data;
		Data.Account = TEXT("PlayerOne");
		Data.Name = TEXT("Ash");
		Data.ClassId = TEXT("CLS_WARRIOR");
		Data.Level = 7;
		Data.XP = 1240;
		Data.Gold = 85;
		Data.Map = TEXT("L_Thistlewick");
		Data.Location = FVector(1200.5, -340.25, 96.0);
		Data.Yaw = 90.f;
		Data.Inventory.Add(MakeItem(ETVRarity::Uncommon, 10, 918273));
		Data.Inventory.Add(MakeItem(ETVRarity::Epic, 12, 5550123));
		Data.Equipment.Add(ETVEquipSlot::Chest, Data.Inventory[0].InstanceGuid);

		FTVQuestProgress Done;
		Done.QuestId = TEXT("Q01_FADING_GLOW");
		Done.State = ETVQuestState::Completed;
		FTVQuestProgress Active;
		Active.QuestId = TEXT("Q02_MOSSBACK_MISCHIEF");
		Active.State = ETVQuestState::Active;
		Active.Progress = { 5 };
		Data.Quests = { Done, Active };

		Data.Mounts = { TEXT("MNT_DAWNSTAG") };
		Data.ActionBar = { TEXT("ABL_WAR_CLEAVE_STRIKE"), TEXT("ABL_WAR_SHIELD_SLAM") };
		Data.ActionBar.SetNum(FTVCharacterSaveData::ActionBarSlots);
		Data.WorldFlags = { TEXT("WF_EMBER_CROWN_RELIT") };
		return Data;
	}

	bool ItemsEqual(const FTVItemInstance& A, const FTVItemInstance& B)
	{
		return A.ItemDefId == B.ItemDefId && A.SetTier == B.SetTier && A.Slot == B.Slot && A.Rarity == B.Rarity
			&& A.ItemLevel == B.ItemLevel && A.Seed == B.Seed && A.RolledStats == B.RolledStats && A.InstanceGuid == B.InstanceGuid;
	}

	void CompareSaves(FAutomationTestBase& Test, const FTVCharacterSaveData& A, const FTVCharacterSaveData& B)
	{
		Test.TestEqual(TEXT("schemaVersion"), B.SchemaVersion, FTVCharacterSaveData::CurrentSchemaVersion);
		Test.TestEqual(TEXT("account"), B.Account, A.Account);
		Test.TestEqual(TEXT("name"), B.Name, A.Name);
		Test.TestTrue(TEXT("classId"), B.ClassId == A.ClassId);
		Test.TestEqual(TEXT("level"), B.Level, A.Level);
		Test.TestEqual(TEXT("xp"), B.XP, A.XP);
		Test.TestEqual(TEXT("gold"), B.Gold, A.Gold);
		Test.TestEqual(TEXT("map"), B.Map, A.Map);
		Test.TestTrue(TEXT("location"), B.Location.Equals(A.Location, 0.01));
		Test.TestEqual(TEXT("yaw"), B.Yaw, A.Yaw, 0.01f);
		Test.TestEqual(TEXT("inventory count"), B.Inventory.Num(), A.Inventory.Num());
		for (int32 i = 0; i < FMath::Min(A.Inventory.Num(), B.Inventory.Num()); ++i)
		{
			Test.TestTrue(FString::Printf(TEXT("item %d identical"), i), ItemsEqual(A.Inventory[i], B.Inventory[i]));
		}
		Test.TestEqual(TEXT("equipment count"), B.Equipment.Num(), A.Equipment.Num());
		const FGuid* Chest = B.Equipment.Find(ETVEquipSlot::Chest);
		Test.TestTrue(TEXT("chest guid"), Chest && *Chest == A.Equipment.FindChecked(ETVEquipSlot::Chest));
		Test.TestEqual(TEXT("quest count"), B.Quests.Num(), A.Quests.Num());
		for (const FTVQuestProgress& Quest : A.Quests)
		{
			const FTVQuestProgress* Loaded = B.Quests.FindByPredicate([&Quest](const FTVQuestProgress& Q) { return Q.QuestId == Quest.QuestId; });
			Test.TestTrue(FString::Printf(TEXT("quest %s state"), *Quest.QuestId.ToString()), Loaded && Loaded->State == Quest.State);
			if (Loaded && Quest.State != ETVQuestState::Completed)
			{
				Test.TestTrue(TEXT("quest progress"), Loaded->Progress == Quest.Progress);
			}
		}
		Test.TestTrue(TEXT("mounts"), B.Mounts == A.Mounts);
		Test.TestTrue(TEXT("actionBar (10 slots, nulls kept)"), B.ActionBar == A.ActionBar);
		Test.TestTrue(TEXT("worldFlags"), B.WorldFlags == A.WorldFlags);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTVSaveRoundTripStringTest, "ThistlewickVale.Systems.Save.RoundTripJson", TVSaveTestFlags)
bool FTVSaveRoundTripStringTest::RunTest(const FString& Parameters)
{
	const FTVCharacterSaveData Original = MakeSample();
	const FString Json = UTVSaveSubsystem::ToJsonString(Original);

	// The file must use the DATA_SCHEMAS section 8 keys.
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	TestTrue(TEXT("output parses"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
	if (Root.IsValid())
	{
		for (const TCHAR* Key : { TEXT("schemaVersion"), TEXT("account"), TEXT("name"), TEXT("classId"), TEXT("level"), TEXT("xp"), TEXT("gold"),
			TEXT("location"), TEXT("inventory"), TEXT("equipment"), TEXT("quests"), TEXT("mounts"), TEXT("actionBar"), TEXT("worldFlags") })
		{
			TestTrue(FString::Printf(TEXT("has key %s"), Key), Root->HasField(Key));
		}
		const TArray<TSharedPtr<FJsonValue>>* Bar = nullptr;
		TestTrue(TEXT("actionBar has 10 entries"), Root->TryGetArrayField(TEXT("actionBar"), Bar) && Bar->Num() == 10);
	}

	FTVCharacterSaveData Loaded;
	FString Error;
	TestTrue(TEXT("parse back"), UTVSaveSubsystem::FromJsonString(Json, Loaded, Error));
	CompareSaves(*this, Original, Loaded);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTVSaveRoundTripFileTest, "ThistlewickVale.Systems.Save.RoundTripFile", TVSaveTestFlags)
bool FTVSaveRoundTripFileTest::RunTest(const FString& Parameters)
{
	const FTVCharacterSaveData Original = MakeSample();
	const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("TVSaveTest_PlayerOne_Ash.json"));
	TestTrue(TEXT("write"), UTVSaveSubsystem::WriteFile(Path, Original));
	FTVCharacterSaveData Loaded;
	FString Error;
	TestTrue(TEXT("read"), UTVSaveSubsystem::ReadFile(Path, Loaded, Error));
	CompareSaves(*this, Original, Loaded);
	IFileManager::Get().Delete(*Path);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTVSaveRejectsTamperedItemsTest, "ThistlewickVale.Systems.Save.RejectsTamperedItems", TVSaveTestFlags)
bool FTVSaveRejectsTamperedItemsTest::RunTest(const FString& Parameters)
{
	FTVCharacterSaveData Data = MakeSample();
	Data.Inventory[0].RolledStats[0].Value += 10; // the equipped chest is now forged

	FTVCharacterSaveData Loaded;
	FString Error;
	TestTrue(TEXT("parse"), UTVSaveSubsystem::FromJsonString(UTVSaveSubsystem::ToJsonString(Data), Loaded, Error));

	const FTVItemDefRow Def = TVSystemsTest::MakePlateChestDef();
	const FTVStatPoolRow Pool = TVSystemsTest::MakePlatePool();
	// The rejection is logged as a warning (not an error), which does not fail the test.
	const int32 Rejected = UTVSaveSubsystem::RejectInvalidItems(Loaded, [&Def, &Pool](const FTVItemInstance& Item, FString& Reason)
	{
		return UTVLootLibrary::ValidateItemWithPool(Item, Def, Pool, &Reason);
	});
	TestEqual(TEXT("one item rejected"), Rejected, 1);
	TestEqual(TEXT("one item left"), Loaded.Inventory.Num(), 1);
	TestFalse(TEXT("equipment entry for the rejected item removed"), Loaded.Equipment.Contains(ETVEquipSlot::Chest));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTVSaveFileNameAndVersionTest, "ThistlewickVale.Systems.Save.SafeNamesAndVersion", TVSaveTestFlags)
bool FTVSaveFileNameAndVersionTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("plain"), UTVSaveSubsystem::MakeSafeFileStem(TEXT("PlayerOne"), TEXT("Ash")), FString(TEXT("PlayerOne_Ash")));
	TestEqual(TEXT("path tricks stripped"), UTVSaveSubsystem::MakeSafeFileStem(TEXT("../Player One!"), TEXT("Ash/..\\x")), FString(TEXT("PlayerOne_Ashx")));
	TestEqual(TEXT("underscore removed so names cannot collide"), UTVSaveSubsystem::MakeSafeFileStem(TEXT("a_b"), TEXT("c")), FString(TEXT("ab_c")));
	TestTrue(TEXT("empty after cleaning is refused"), UTVSaveSubsystem::MakeSafeFileStem(TEXT("***"), TEXT("Ash")).IsEmpty());
	TestTrue(TEXT("long names are capped"), UTVSaveSubsystem::MakeSafeFileStem(FString::ChrN(100, TEXT('a')), TEXT("b")).Len() <= 26);

	FTVCharacterSaveData Out;
	FString Error;
	TestFalse(TEXT("future schemaVersion refused"), UTVSaveSubsystem::FromJsonString(TEXT("{\"schemaVersion\": 99, \"classId\": \"CLS_MAGE\"}"), Out, Error));
	TestFalse(TEXT("missing schemaVersion refused"), UTVSaveSubsystem::FromJsonString(TEXT("{\"classId\": \"CLS_MAGE\"}"), Out, Error));
	TestFalse(TEXT("garbage refused"), UTVSaveSubsystem::FromJsonString(TEXT("not json"), Out, Error));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
