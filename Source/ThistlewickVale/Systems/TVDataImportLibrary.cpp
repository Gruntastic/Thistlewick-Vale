#include "Systems/TVDataImportLibrary.h"
#include "Core/TVProjectSettings.h"
#include "ThistlewickVale.h"
#include "JsonObjectConverter.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/DataTable.h"
#include "UObject/SoftObjectPath.h"

bool UTVDataImportLibrary::FillObjectFromJsonString(UObject* Target, const FString& JsonString, FString& OutError)
{
	if (!Target)
	{
		OutError = TEXT("no target object");
		return false;
	}
	TSharedPtr<FJsonObject> Json;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
	{
		OutError = TEXT("not a JSON object");
		return false;
	}
	FText FailReason;
	// Verified in 5.8 JsonObjectConverter.h: JsonObjectToUStruct(const TSharedRef<FJsonObject>&, const UStruct*, void*, CheckFlags, SkipFlags, bStrictMode, FText* OutFailReason, ImportCb).
	// A UClass is a UStruct, so the object's own class describes its memory.
	const bool bOk = FJsonObjectConverter::JsonObjectToUStruct(Json.ToSharedRef(), Target->GetClass(), Target, 0, 0, /*bStrictMode*/ false, &FailReason);
	if (!bOk)
	{
		OutError = FailReason.ToString();
		return false;
	}
	Target->MarkPackageDirty();
	return true;
}

bool UTVDataImportLibrary::RegisterTables(const FString& AssetManifestPath, const TMap<FName, FString>& Tables)
{
	UTVProjectSettings* Settings = GetMutableDefault<UTVProjectSettings>();
	if (!Settings)
	{
		return false;
	}
	if (!AssetManifestPath.IsEmpty())
	{
		Settings->AssetManifest = TSoftObjectPtr<UDataTable>(FSoftObjectPath(AssetManifestPath));
	}
	for (const TPair<FName, FString>& Pair : Tables)
	{
		Settings->Tables.Add(Pair.Key, TSoftObjectPtr<UDataTable>(FSoftObjectPath(Pair.Value)));
	}
	// Verified in 5.8 UObject/Object.h: bool TryUpdateDefaultConfigFile(const FString& SpecificFileLocation = "", bool bWarnIfFail = true).
	const bool bSaved = Settings->TryUpdateDefaultConfigFile();
	if (!bSaved)
	{
		UE_LOG(LogTV, Error, TEXT("RegisterTables: could not write DefaultGame.ini (read-only or checked in?)."));
	}
	return bSaved;
}
