#include "Systems/TVSaveSubsystem.h"
#include "Systems/TVLootLibrary.h"
#include "Systems/TVInventoryComponent.h"
#include "Systems/TVEquipmentComponent.h"
#include "Systems/TVQuestComponent.h"
#include "Systems/TVMountComponent.h"
#include "Combat/TVPlayerState.h"
#include "ThistlewickVale.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/Class.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

namespace TVSave
{
	template <typename TEnum>
	static FString EnumToString(TEnum Value)
	{
		return StaticEnum<TEnum>()->GetNameStringByValue(static_cast<int64>(Value));
	}

	template <typename TEnum>
	static bool StringToEnum(const FString& Name, TEnum& Out)
	{
		const int64 Value = StaticEnum<TEnum>()->GetValueByNameString(Name);
		if (Value == INDEX_NONE)
		{
			return false;
		}
		Out = static_cast<TEnum>(Value);
		return true;
	}

	static FString NameOrEmpty(FName Name) { return Name.IsNone() ? FString() : Name.ToString(); }

	static FString CurrentMapName(const UWorld* World)
	{
		if (!World)
		{
			return FString();
		}
		// "L_Thistlewick" without the PIE prefix.
		return UWorld::RemovePIEPrefix(World->GetMapName());
	}

	static TSharedRef<FJsonObject> ItemToJson(const FTVItemInstance& Item)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("itemDefId"), Item.ItemDefId.ToString());
		Obj->SetStringField(TEXT("setTier"), EnumToString(Item.SetTier));
		Obj->SetStringField(TEXT("slot"), EnumToString(Item.Slot));
		Obj->SetStringField(TEXT("rarity"), EnumToString(Item.Rarity));
		Obj->SetNumberField(TEXT("itemLevel"), Item.ItemLevel);
		Obj->SetNumberField(TEXT("seed"), Item.Seed);
		TArray<TSharedPtr<FJsonValue>> Stats;
		for (const FTVRolledStat& Stat : Item.RolledStats)
		{
			TSharedRef<FJsonObject> StatObj = MakeShared<FJsonObject>();
			StatObj->SetStringField(TEXT("stat"), EnumToString(Stat.Stat));
			StatObj->SetNumberField(TEXT("value"), Stat.Value);
			Stats.Add(MakeShared<FJsonValueObject>(StatObj));
		}
		Obj->SetArrayField(TEXT("rolledStats"), Stats);
		Obj->SetStringField(TEXT("instanceGuid"), Item.InstanceGuid.ToString(EGuidFormats::DigitsWithHyphens));
		return Obj;
	}

	static bool ItemFromJson(const FJsonObject& Obj, FTVItemInstance& Out, FString& OutError)
	{
		FString DefId, SetTier, Slot, Rarity, Guid;
		int32 ItemLevel = 0, Seed = 0;
		if (!Obj.TryGetStringField(TEXT("itemDefId"), DefId) || !Obj.TryGetStringField(TEXT("rarity"), Rarity)
			|| !Obj.TryGetNumberField(TEXT("itemLevel"), ItemLevel) || !Obj.TryGetNumberField(TEXT("seed"), Seed)
			|| !Obj.TryGetStringField(TEXT("instanceGuid"), Guid))
		{
			OutError = TEXT("item is missing a required field");
			return false;
		}
		Out.ItemDefId = FName(*DefId);
		Out.ItemLevel = ItemLevel;
		Out.Seed = Seed;
		if (!StringToEnum(Rarity, Out.Rarity))
		{
			OutError = FString::Printf(TEXT("bad rarity '%s'"), *Rarity);
			return false;
		}
		if (Obj.TryGetStringField(TEXT("setTier"), SetTier) && !StringToEnum(SetTier, Out.SetTier))
		{
			OutError = FString::Printf(TEXT("bad setTier '%s'"), *SetTier);
			return false;
		}
		if (Obj.TryGetStringField(TEXT("slot"), Slot) && !StringToEnum(Slot, Out.Slot))
		{
			OutError = FString::Printf(TEXT("bad slot '%s'"), *Slot);
			return false;
		}
		if (!FGuid::Parse(Guid, Out.InstanceGuid))
		{
			OutError = FString::Printf(TEXT("bad instanceGuid '%s'"), *Guid);
			return false;
		}
		const TArray<TSharedPtr<FJsonValue>>* Stats = nullptr;
		if (Obj.TryGetArrayField(TEXT("rolledStats"), Stats))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Stats)
			{
				const TSharedPtr<FJsonObject>* StatObj = nullptr;
				FString StatName;
				int32 StatValue = 0;
				FTVRolledStat Rolled;
				if (!Value.IsValid() || !Value->TryGetObject(StatObj) || !(*StatObj)->TryGetStringField(TEXT("stat"), StatName)
					|| !(*StatObj)->TryGetNumberField(TEXT("value"), StatValue) || !StringToEnum(StatName, Rolled.Stat))
				{
					OutError = TEXT("bad rolledStats entry");
					return false;
				}
				Rolled.Value = StatValue;
				Out.RolledStats.Add(Rolled);
			}
		}
		return true;
	}

	static void NamesToJson(FJsonObject& Root, const TCHAR* Field, const TArray<FName>& Names, bool bNullForNone)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		for (const FName& Name : Names)
		{
			if (Name.IsNone())
			{
				if (bNullForNone)
				{
					Values.Add(MakeShared<FJsonValueNull>());
				}
				continue;
			}
			Values.Add(MakeShared<FJsonValueString>(Name.ToString()));
		}
		Root.SetArrayField(Field, Values);
	}

	static TArray<FName> NamesFromJson(const FJsonObject& Root, const TCHAR* Field, bool bKeepNulls)
	{
		TArray<FName> Out;
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (Root.TryGetArrayField(Field, Values))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Values)
			{
				FString Str;
				if (Value.IsValid() && Value->TryGetString(Str) && !Str.IsEmpty())
				{
					Out.Add(FName(*Str));
				}
				else if (bKeepNulls)
				{
					Out.Add(NAME_None);
				}
			}
		}
		return Out;
	}
}

// ---- File names ----

FString UTVSaveSubsystem::MakeSafeFileStem(const FString& Account, const FString& CharacterName)
{
	auto Clean = [](const FString& In)
	{
		FString Out;
		for (const TCHAR C : In)
		{
			if (Out.Len() >= 24)
			{
				break;
			}
			if ((C >= 'A' && C <= 'Z') || (C >= 'a' && C <= 'z') || (C >= '0' && C <= '9') || C == '-')
			{
				Out.AppendChar(C);
			}
		}
		return Out;
	};
	// '_' is the separator, so it is not allowed inside either part ("a_b"+"c" must not collide with "a"+"b_c").
	const FString A = Clean(Account);
	const FString N = Clean(CharacterName);
	if (A.IsEmpty() || N.IsEmpty())
	{
		return FString();
	}
	return A + TEXT("_") + N;
}

FString UTVSaveSubsystem::GetSaveDirectory()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Characters"));
}

FString UTVSaveSubsystem::GetSavePath(const FString& Account, const FString& CharacterName)
{
	const FString Stem = MakeSafeFileStem(Account, CharacterName);
	return Stem.IsEmpty() ? FString() : FPaths::Combine(GetSaveDirectory(), Stem + TEXT(".json"));
}

// ---- JSON ----

FString UTVSaveSubsystem::ToJsonString(const FTVCharacterSaveData& Data)
{
	using namespace TVSave;
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("schemaVersion"), FTVCharacterSaveData::CurrentSchemaVersion);
	Root->SetStringField(TEXT("account"), Data.Account);
	Root->SetStringField(TEXT("name"), Data.Name);
	Root->SetStringField(TEXT("classId"), NameOrEmpty(Data.ClassId));
	Root->SetNumberField(TEXT("level"), Data.Level);
	Root->SetNumberField(TEXT("xp"), Data.XP);
	Root->SetNumberField(TEXT("gold"), Data.Gold);

	TSharedRef<FJsonObject> Location = MakeShared<FJsonObject>();
	Location->SetStringField(TEXT("map"), Data.Map);
	Location->SetNumberField(TEXT("x"), Data.Location.X);
	Location->SetNumberField(TEXT("y"), Data.Location.Y);
	Location->SetNumberField(TEXT("z"), Data.Location.Z);
	Location->SetNumberField(TEXT("yaw"), Data.Yaw);
	Root->SetObjectField(TEXT("location"), Location);

	TArray<TSharedPtr<FJsonValue>> Items;
	for (const FTVItemInstance& Item : Data.Inventory)
	{
		Items.Add(MakeShared<FJsonValueObject>(ItemToJson(Item)));
	}
	Root->SetArrayField(TEXT("inventory"), Items);

	TSharedRef<FJsonObject> Equipment = MakeShared<FJsonObject>();
	for (const TPair<ETVEquipSlot, FGuid>& Pair : Data.Equipment)
	{
		Equipment->SetStringField(EnumToString(Pair.Key), Pair.Value.ToString(EGuidFormats::DigitsWithHyphens));
	}
	Root->SetObjectField(TEXT("equipment"), Equipment);

	TSharedRef<FJsonObject> Quests = MakeShared<FJsonObject>();
	for (const FTVQuestProgress& Quest : Data.Quests)
	{
		TSharedRef<FJsonObject> QuestObj = MakeShared<FJsonObject>();
		QuestObj->SetStringField(TEXT("state"), EnumToString(Quest.State));
		if (Quest.State != ETVQuestState::Completed && Quest.Progress.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Progress;
			for (const int32 Count : Quest.Progress)
			{
				Progress.Add(MakeShared<FJsonValueNumber>(Count));
			}
			QuestObj->SetArrayField(TEXT("progress"), Progress);
		}
		Quests->SetObjectField(Quest.QuestId.ToString(), QuestObj);
	}
	Root->SetObjectField(TEXT("quests"), Quests);

	NamesToJson(*Root, TEXT("mounts"), Data.Mounts, false);
	TArray<FName> ActionBar = Data.ActionBar;
	ActionBar.SetNum(FTVCharacterSaveData::ActionBarSlots);
	NamesToJson(*Root, TEXT("actionBar"), ActionBar, true);
	NamesToJson(*Root, TEXT("worldFlags"), Data.WorldFlags, false);

	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Root, Writer);
	return Out;
}

bool UTVSaveSubsystem::FromJsonString(const FString& Json, FTVCharacterSaveData& OutData, FString& OutError)
{
	using namespace TVSave;
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("not valid JSON");
		return false;
	}

	FTVCharacterSaveData Data;
	int32 Version = 0;
	if (!Root->TryGetNumberField(TEXT("schemaVersion"), Version))
	{
		OutError = TEXT("missing schemaVersion");
		return false;
	}
	if (Version < 1 || Version > FTVCharacterSaveData::CurrentSchemaVersion)
	{
		OutError = FString::Printf(TEXT("unsupported schemaVersion %d"), Version);
		return false;
	}
	// Future versions migrate here (switch on Version) before reading fields.
	Data.SchemaVersion = Version;

	FString ClassId;
	Root->TryGetStringField(TEXT("account"), Data.Account);
	Root->TryGetStringField(TEXT("name"), Data.Name);
	if (!Root->TryGetStringField(TEXT("classId"), ClassId) || ClassId.IsEmpty())
	{
		OutError = TEXT("missing classId");
		return false;
	}
	Data.ClassId = FName(*ClassId);
	Root->TryGetNumberField(TEXT("level"), Data.Level);
	Root->TryGetNumberField(TEXT("xp"), Data.XP);
	Root->TryGetNumberField(TEXT("gold"), Data.Gold);
	Data.Level = FMath::Max(1, Data.Level);
	Data.XP = FMath::Max(0, Data.XP);
	Data.Gold = FMath::Max(0, Data.Gold);

	const TSharedPtr<FJsonObject>* Location = nullptr;
	if (Root->TryGetObjectField(TEXT("location"), Location))
	{
		double X = 0, Y = 0, Z = 0, Yaw = 0;
		(*Location)->TryGetStringField(TEXT("map"), Data.Map);
		(*Location)->TryGetNumberField(TEXT("x"), X);
		(*Location)->TryGetNumberField(TEXT("y"), Y);
		(*Location)->TryGetNumberField(TEXT("z"), Z);
		(*Location)->TryGetNumberField(TEXT("yaw"), Yaw);
		Data.Location = FVector(X, Y, Z);
		Data.Yaw = static_cast<float>(Yaw);
	}

	const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
	if (Root->TryGetArrayField(TEXT("inventory"), Items))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Items)
		{
			const TSharedPtr<FJsonObject>* ItemObj = nullptr;
			FTVItemInstance Item;
			FString ItemError;
			if (Value.IsValid() && Value->TryGetObject(ItemObj) && ItemFromJson(**ItemObj, Item, ItemError))
			{
				Data.Inventory.Add(Item);
			}
			else
			{
				UE_LOG(LogTV, Warning, TEXT("Save: unreadable item skipped (%s)."), *ItemError);
			}
		}
	}

	const TSharedPtr<FJsonObject>* Equipment = nullptr;
	if (Root->TryGetObjectField(TEXT("equipment"), Equipment))
	{
		// 5.8: FJsonObject keys are UE::FSharedString by default (FString with UE_JSONOBJECT_LEGACY_STRING_KEYS); both have operator*.
		for (const auto& Pair : (*Equipment)->Values)
		{
			const FString Key(*Pair.Key);
			ETVEquipSlot Slot = ETVEquipSlot::None;
			FString GuidString;
			FGuid Guid;
			if (StringToEnum(Key, Slot) && Slot != ETVEquipSlot::None && Pair.Value.IsValid()
				&& Pair.Value->TryGetString(GuidString) && FGuid::Parse(GuidString, Guid))
			{
				Data.Equipment.Add(Slot, Guid);
			}
		}
	}

	const TSharedPtr<FJsonObject>* Quests = nullptr;
	if (Root->TryGetObjectField(TEXT("quests"), Quests))
	{
		for (const auto& Pair : (*Quests)->Values)
		{
			const FString Key(*Pair.Key);
			const TSharedPtr<FJsonObject>* QuestObj = nullptr;
			FString State;
			FTVQuestProgress Progress;
			Progress.QuestId = FName(*Key);
			if (!Pair.Value.IsValid() || !Pair.Value->TryGetObject(QuestObj) || !(*QuestObj)->TryGetStringField(TEXT("state"), State)
				|| !StringToEnum(State, Progress.State))
			{
				UE_LOG(LogTV, Warning, TEXT("Save: quest '%s' unreadable, skipped."), *Key);
				continue;
			}
			const TArray<TSharedPtr<FJsonValue>>* Counts = nullptr;
			if ((*QuestObj)->TryGetArrayField(TEXT("progress"), Counts))
			{
				for (const TSharedPtr<FJsonValue>& Count : *Counts)
				{
					int32 N = 0;
					Progress.Progress.Add(Count.IsValid() && Count->TryGetNumber(N) ? N : 0);
				}
			}
			Data.Quests.Add(Progress);
		}
	}

	Data.Mounts = NamesFromJson(*Root, TEXT("mounts"), false);
	Data.ActionBar = NamesFromJson(*Root, TEXT("actionBar"), true);
	Data.ActionBar.SetNum(FTVCharacterSaveData::ActionBarSlots);
	Data.WorldFlags = NamesFromJson(*Root, TEXT("worldFlags"), false);

	OutData = MoveTemp(Data);
	return true;
}

bool UTVSaveSubsystem::WriteFile(const FString& Path, const FTVCharacterSaveData& Data)
{
	if (Path.IsEmpty())
	{
		return false;
	}
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), /*Tree*/ true);
	// Write to a temp file first so a crash mid-write never truncates the real save.
	const FString TempPath = Path + TEXT(".tmp");
	if (!FFileHelper::SaveStringToFile(ToJsonString(Data), *TempPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		return false;
	}
	return IFileManager::Get().Move(*Path, *TempPath, /*bReplace*/ true);
}

bool UTVSaveSubsystem::ReadFile(const FString& Path, FTVCharacterSaveData& OutData, FString& OutError)
{
	FString Json;
	if (Path.IsEmpty() || !FFileHelper::LoadFileToString(Json, *Path))
	{
		OutError = TEXT("file not found");
		return false;
	}
	return FromJsonString(Json, OutData, OutError);
}

int32 UTVSaveSubsystem::RejectInvalidItems(FTVCharacterSaveData& Data, TFunctionRef<bool(const FTVItemInstance&, FString&)> Validator)
{
	int32 Rejected = 0;
	TSet<FGuid> Seen;
	for (int32 i = Data.Inventory.Num() - 1; i >= 0; --i)
	{
		const FTVItemInstance& Item = Data.Inventory[i];
		FString Reason;
		bool bOk = Validator(Item, Reason);
		if (bOk && Seen.Contains(Item.InstanceGuid))
		{
			bOk = false;
			Reason = TEXT("duplicate instanceGuid");
		}
		if (!bOk)
		{
			UE_LOG(LogTV, Warning, TEXT("Save '%s_%s': item '%s' (%s) rejected: %s"), *Data.Account, *Data.Name,
				*Item.ItemDefId.ToString(), *Item.InstanceGuid.ToString(), *Reason);
			Data.Inventory.RemoveAt(i);
			++Rejected;
			continue;
		}
		Seen.Add(Item.InstanceGuid);
	}
	for (auto It = Data.Equipment.CreateIterator(); It; ++It)
	{
		if (!Seen.Contains(It.Value()))
		{
			It.RemoveCurrent();
		}
	}
	return Rejected;
}

// ---- Player state glue ----

void UTVSaveSubsystem::RegisterAccount(ATVPlayerState* Player, const FString& Account)
{
	if (Player)
	{
		Accounts.Add(Player, Account);
	}
}

void UTVSaveSubsystem::SetActionBar(ATVPlayerState* Player, const TArray<FName>& AbilityIds)
{
	if (Player)
	{
		TArray<FName> Bar = AbilityIds;
		Bar.SetNum(FTVCharacterSaveData::ActionBarSlots);
		ActionBars.Add(Player, Bar);
	}
}

TArray<FName> UTVSaveSubsystem::GetActionBar(const ATVPlayerState* Player) const
{
	if (const TArray<FName>* Bar = ActionBars.Find(const_cast<ATVPlayerState*>(Player)))
	{
		return *Bar;
	}
	TArray<FName> Empty;
	Empty.SetNum(FTVCharacterSaveData::ActionBarSlots);
	return Empty;
}

bool UTVSaveSubsystem::GetSavedSpawn(const ATVPlayerState* Player, FTransform& OutTransform) const
{
	const TPair<FString, FTransform>* Spawn = SavedSpawns.Find(const_cast<ATVPlayerState*>(Player));
	if (!Spawn || Spawn->Key != TVSave::CurrentMapName(Player ? Player->GetWorld() : nullptr))
	{
		return false;
	}
	OutTransform = Spawn->Value;
	return true;
}

FTVCharacterSaveData UTVSaveSubsystem::Gather(ATVPlayerState* Player, const FString& Account) const
{
	FTVCharacterSaveData Data;
	Data.Account = Account;
	// INTERFACE-REQUEST: ATVPlayerState's replicated CharacterName / ClassId / Level / XP / Gold are read directly (ledger lists them).
	Data.Name = Player->CharacterName;
	Data.ClassId = Player->ClassId;
	Data.Level = Player->Level;
	Data.XP = Player->XP;
	Data.Gold = Player->Gold;

	if (const APawn* Pawn = Player->GetPawn())
	{
		Data.Map = TVSave::CurrentMapName(Player->GetWorld());
		Data.Location = Pawn->GetActorLocation();
		Data.Yaw = Pawn->GetActorRotation().Yaw;
	}
	else if (const TPair<FString, FTransform>* Spawn = SavedSpawns.Find(Player))
	{
		Data.Map = Spawn->Key;
		Data.Location = Spawn->Value.GetLocation();
		Data.Yaw = Spawn->Value.Rotator().Yaw;
	}

	if (const UTVInventoryComponent* Inventory = Player->GetInventory())
	{
		Data.Inventory = Inventory->GetItems();
	}
	if (const UTVEquipmentComponent* Equipment = Player->GetEquipment())
	{
		for (const FTVItemInstance& Item : Equipment->GetEquippedItems())
		{
			Data.Inventory.Add(Item);
			Data.Equipment.Add(Item.Slot, Item.InstanceGuid);
		}
	}
	if (const UTVQuestComponent* Quests = Player->GetQuests())
	{
		Data.Quests = Quests->GetQuests();
		Data.WorldFlags = Quests->GetWorldFlags();
	}
	if (const UTVMountComponent* Mount = Player->GetMount())
	{
		Data.Mounts = Mount->GetOwnedMounts();
	}
	Data.ActionBar = GetActionBar(Player);
	return Data;
}

void UTVSaveSubsystem::Apply(ATVPlayerState* Player, const FTVCharacterSaveData& Data)
{
	Player->CharacterName = Data.Name;
	Player->InitCharacter(Data.ClassId, Data.Level, Data.XP);
	Player->AddGold(Data.Gold - Player->Gold);

	// Split equipped and bag items.
	TArray<FTVItemInstance> BagItems;
	TArray<FTVItemInstance> EquippedItems;
	TSet<FGuid> EquippedGuids;
	for (const TPair<ETVEquipSlot, FGuid>& Pair : Data.Equipment)
	{
		EquippedGuids.Add(Pair.Value);
	}
	for (const FTVItemInstance& Item : Data.Inventory)
	{
		(EquippedGuids.Contains(Item.InstanceGuid) ? EquippedItems : BagItems).Add(Item);
	}
	if (UTVEquipmentComponent* Equipment = Player->GetEquipment())
	{
		TArray<FTVItemInstance> Rejected;
		Equipment->RestoreFromSave(EquippedItems, Rejected);
		BagItems.Append(Rejected); // could not be worn (class/slot) -> back to the bag
	}
	if (UTVInventoryComponent* Inventory = Player->GetInventory())
	{
		Inventory->SetItemsFromSave(BagItems);
	}
	if (UTVQuestComponent* Quests = Player->GetQuests())
	{
		Quests->RestoreFromSave(Data.Quests, Data.WorldFlags);
	}
	if (UTVMountComponent* Mount = Player->GetMount())
	{
		for (const FName& MountId : Data.Mounts)
		{
			Mount->GrantMount(MountId);
		}
	}
	SetActionBar(Player, Data.ActionBar);

	const FTransform Spawn(FRotator(0.f, Data.Yaw, 0.f), Data.Location);
	SavedSpawns.Add(Player, TPair<FString, FTransform>(Data.Map, Spawn));
	if (APawn* Pawn = Player->GetPawn())
	{
		if (!Data.Map.IsEmpty() && Data.Map == TVSave::CurrentMapName(Player->GetWorld()))
		{
			Pawn->TeleportTo(Spawn.GetLocation(), Spawn.Rotator());
		}
	}
}

bool UTVSaveSubsystem::LoadCharacter(ATVPlayerState* Player, const FString& Account, const FString& CharacterName)
{
	if (!Player || !Player->HasAuthority())
	{
		return false;
	}
	RegisterAccount(Player, Account);
	const FString Path = GetSavePath(Account, CharacterName);
	if (Path.IsEmpty())
	{
		UE_LOG(LogTV, Warning, TEXT("Save: invalid account/character name '%s' / '%s'."), *Account, *CharacterName);
		return false;
	}
	if (!IFileManager::Get().FileExists(*Path))
	{
		return false; // new character
	}

	FTVCharacterSaveData Data;
	FString Error;
	if (!ReadFile(Path, Data, Error))
	{
		UE_LOG(LogTV, Error, TEXT("Save: %s could not be loaded: %s"), *Path, *Error);
		return false;
	}
	const int32 Rejected = RejectInvalidItems(Data, [](const FTVItemInstance& Item, FString& Reason)
	{
		return UTVLootLibrary::ValidateItem(Item, &Reason);
	});
	if (Rejected > 0)
	{
		UE_LOG(LogTV, Warning, TEXT("Save: %d item(s) in %s failed validation and were removed."), Rejected, *Path);
	}
	Apply(Player, Data);
	return true;
}

bool UTVSaveSubsystem::SaveCharacter(ATVPlayerState* Player)
{
	if (!Player || !Player->HasAuthority())
	{
		return false;
	}
	const FString* Account = Accounts.Find(Player);
	if (!Account)
	{
		UE_LOG(LogTV, Warning, TEXT("Save: no account registered for %s; call LoadCharacter or RegisterAccount first."), *Player->GetPlayerName());
		return false;
	}
	const FTVCharacterSaveData Data = Gather(Player, *Account);
	const FString Path = GetSavePath(*Account, Data.Name);
	if (!WriteFile(Path, Data))
	{
		UE_LOG(LogTV, Error, TEXT("Save: writing '%s' failed."), *Path);
		return false;
	}
	return true;
}
