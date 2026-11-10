// Editor-scripting helpers used by Tools/ue/import_data.py (Python cannot fill nested UStruct arrays generically).
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TVDataImportLibrary.generated.h"

class UDataTable;

UCLASS()
class THISTLEWICKVALE_API UTVDataImportLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Fills Target's UPROPERTYs from a JSON object string (same field names; enums as names; soft paths as strings).
	 * Keys that do not match a property (e.g. "Name") are ignored. Python: unreal.TVDataImportLibrary.fill_object_from_json_string(obj, text).
	 */
	UFUNCTION(BlueprintCallable, Category = "TV|Editor")
	static bool FillObjectFromJsonString(UObject* Target, const FString& JsonString, FString& OutError);

	/**
	 * Writes the table registry into UTVProjectSettings and saves Config/DefaultGame.ini
	 * ([/Script/ThistlewickVale.TVProjectSettings] AssetManifest + Tables). Paths are object paths ("/Game/MMO/Data/DT_X.DT_X").
	 */
	UFUNCTION(BlueprintCallable, Category = "TV|Editor")
	static bool RegisterTables(const FString& AssetManifestPath, const TMap<FName, FString>& Tables);
};
