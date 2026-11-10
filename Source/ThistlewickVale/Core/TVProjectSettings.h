// Central list of data tables. Every system finds its table here, so tables can be replaced without code changes.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TVProjectSettings.generated.h"

class UDataTable;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Thistlewick Vale"))
class THISTLEWICKVALE_API UTVProjectSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UTVProjectSettings* Get() { return GetDefault<UTVProjectSettings>(); }

	/** Loads the table registered under Key (e.g. "Abilities", "Mobs", "ItemDefs"). Null if missing. */
	UDataTable* LoadTable(FName Key) const;

	UPROPERTY(Config, EditAnywhere, Category = "Data") TSoftObjectPtr<UDataTable> AssetManifest;

	/** Key -> table. Keys used by code are listed in Docs/DATA_SCHEMAS.md (section 12). */
	UPROPERTY(Config, EditAnywhere, Category = "Data") TMap<FName, TSoftObjectPtr<UDataTable>> Tables;
};
