// Server-side JSON character saves (Docs/DATA_SCHEMAS.md section 8): Saved/Characters/<Account>_<Character>.json.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/TVTypes.h"
#include "Systems/TVQuestComponent.h"
#include "TVSaveSubsystem.generated.h"

class ATVPlayerState;

/** Everything in one save file, independent of actors (so it can be round-tripped in tests). */
USTRUCT(BlueprintType)
struct THISTLEWICKVALE_API FTVCharacterSaveData
{
	GENERATED_BODY()

	static constexpr int32 CurrentSchemaVersion = 1;
	static constexpr int32 ActionBarSlots = 10;

	UPROPERTY() int32 SchemaVersion = CurrentSchemaVersion;
	UPROPERTY() FString Account;
	UPROPERTY() FString Name;
	UPROPERTY() FName ClassId;
	UPROPERTY() int32 Level = 1;
	UPROPERTY() int32 XP = 0;
	UPROPERTY() int32 Gold = 0;
	UPROPERTY() FString Map;
	UPROPERTY() FVector Location = FVector::ZeroVector;
	UPROPERTY() float Yaw = 0.f;
	/** Every item the character owns, bag and equipped. */
	UPROPERTY() TArray<FTVItemInstance> Inventory;
	/** Slot -> InstanceGuid of an item in Inventory. */
	UPROPERTY() TMap<ETVEquipSlot, FGuid> Equipment;
	UPROPERTY() TArray<FTVQuestProgress> Quests;
	UPROPERTY() TArray<FName> Mounts;
	/** 10 entries; NAME_None is written as null. */
	UPROPERTY() TArray<FName> ActionBar;
	UPROPERTY() TArray<FName> WorldFlags;
};

UCLASS()
class THISTLEWICKVALE_API UTVSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Reads the save and applies it to the player. False = no save (new character) or the file could not be used (logged). */
	bool LoadCharacter(ATVPlayerState* Player, const FString& Account, const FString& CharacterName);
	/** Writes the player's current state. Needs a prior LoadCharacter (or RegisterAccount) to know the account. */
	bool SaveCharacter(ATVPlayerState* Player);

	/** For new characters: remember the account name so SaveCharacter can build the file name. */
	void RegisterAccount(ATVPlayerState* Player, const FString& Account);

	/** Saved position for the game mode to spawn at, if the save's map matches the current map. */
	bool GetSavedSpawn(const ATVPlayerState* Player, FTransform& OutTransform) const;

	// ---- Pure helpers (tested) ----

	/** "<Account>_<Name>" with only [A-Za-z0-9-] kept from each part (max 24 chars each). Empty if either part ends up empty. */
	static FString MakeSafeFileStem(const FString& Account, const FString& CharacterName);
	static FString GetSaveDirectory();
	static FString GetSavePath(const FString& Account, const FString& CharacterName);

	static FString ToJsonString(const FTVCharacterSaveData& Data);
	static bool FromJsonString(const FString& Json, FTVCharacterSaveData& OutData, FString& OutError);

	static bool WriteFile(const FString& Path, const FTVCharacterSaveData& Data);
	static bool ReadFile(const FString& Path, FTVCharacterSaveData& OutData, FString& OutError);

	/** Removes items that fail UTVLootLibrary::ValidateItem (and equipment entries pointing at them). Returns how many were rejected. */
	static int32 RejectInvalidItems(FTVCharacterSaveData& Data, TFunctionRef<bool(const FTVItemInstance&, FString&)> Validator);

private:
	FTVCharacterSaveData Gather(ATVPlayerState* Player, const FString& Account) const;
	void Apply(ATVPlayerState* Player, const FTVCharacterSaveData& Data);

	TMap<TWeakObjectPtr<ATVPlayerState>, FString> Accounts;
	TMap<TWeakObjectPtr<ATVPlayerState>, TPair<FString, FTransform>> SavedSpawns;
};
