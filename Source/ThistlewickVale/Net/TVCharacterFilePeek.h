// Read-only peek into a character save file (DATA_SCHEMAS 8) for the two fields the Net module needs at login:
// the saved location and the action bar. The file is written by UTVSaveSubsystem (Systems); this never writes it.
#pragma once

#include "CoreMinimal.h"

struct THISTLEWICKVALE_API FTVCharacterFilePeek
{
	bool bFileFound = false;
	bool bHasLocation = false;
	FString MapName;
	FVector Location = FVector::ZeroVector;
	float Yaw = 0.f;

	/** 10 entries, NAME_None for empty slots. Empty array if the file has no actionBar. */
	TArray<FName> ActionBar;

	/** Saved/Characters/<Account>_<CharacterName>.json, same convention as UTVSaveSubsystem. */
	static FString GetSavePath(const FString& Account, const FString& CharacterName);

	static FTVCharacterFilePeek Read(const FString& Account, const FString& CharacterName);
};
