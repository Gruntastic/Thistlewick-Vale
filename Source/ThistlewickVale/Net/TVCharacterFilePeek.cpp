#include "Net/TVCharacterFilePeek.h"

#include "ThistlewickVale.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// INTERFACE-REQUEST: UTVSaveSubsystem has no accessor for the loaded location or action bar. If Systems adds
// `bool GetLoadedLocation(const ATVPlayerState*, FString& OutMap, FTransform& Out) const` and
// `TArray<FName> GetLoadedActionBar(const ATVPlayerState*) const`, this file can be deleted.

FString FTVCharacterFilePeek::GetSavePath(const FString& Account, const FString& CharacterName)
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Characters"), FString::Printf(TEXT("%s_%s.json"), *Account, *CharacterName));
}

FTVCharacterFilePeek FTVCharacterFilePeek::Read(const FString& Account, const FString& CharacterName)
{
	FTVCharacterFilePeek Result;

	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *GetSavePath(Account, CharacterName)))
	{
		return Result;
	}
	Result.bFileFound = true;

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogTV, Warning, TEXT("[TVNet] Save file for %s_%s is not valid JSON"), *Account, *CharacterName);
		return Result;
	}

	const TSharedPtr<FJsonObject>* LocationObject = nullptr;
	if (Root->TryGetObjectField(TEXT("location"), LocationObject) && LocationObject && LocationObject->IsValid())
	{
		const TSharedPtr<FJsonObject>& Loc = *LocationObject;
		double X = 0.0, Y = 0.0, Z = 0.0, Yaw = 0.0;
		const bool bHasXYZ = Loc->TryGetNumberField(TEXT("x"), X) && Loc->TryGetNumberField(TEXT("y"), Y) && Loc->TryGetNumberField(TEXT("z"), Z);
		Loc->TryGetNumberField(TEXT("yaw"), Yaw);
		Loc->TryGetStringField(TEXT("map"), Result.MapName);
		if (bHasXYZ)
		{
			Result.bHasLocation = true;
			Result.Location = FVector(X, Y, Z);
			Result.Yaw = static_cast<float>(Yaw);
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* Bar = nullptr;
	if (Root->TryGetArrayField(TEXT("actionBar"), Bar) && Bar)
	{
		Result.ActionBar.Init(NAME_None, 10);
		for (int32 Index = 0; Index < Bar->Num() && Index < 10; ++Index)
		{
			FString AbilityId;
			const TSharedPtr<FJsonValue>& Value = (*Bar)[Index];
			if (Value.IsValid() && Value->Type == EJson::String && Value->TryGetString(AbilityId) && !AbilityId.IsEmpty())
			{
				Result.ActionBar[Index] = FName(*AbilityId);
			}
		}
	}
	return Result;
}
