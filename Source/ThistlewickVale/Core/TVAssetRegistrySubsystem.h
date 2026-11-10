// Resolves manifest IDs (Docs/ASSET_MANIFEST.csv) to assets. Gameplay data never references assets directly.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/TVTypes.h"
#include "TVAssetRegistrySubsystem.generated.h"

class UDataTable;

UCLASS()
class THISTLEWICKVALE_API UTVAssetRegistrySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	static UTVAssetRegistrySubsystem* Get(const UObject* WorldContext);

	/** Soft reference for a manifest ID. Null pointer (and a warning, once per ID) if the ID is unknown. */
	UFUNCTION(BlueprintCallable, Category = "TV|Manifest")
	TSoftObjectPtr<UObject> Resolve(FName ManifestId) const;

	/** Blocking load. Fine for placeholders and small assets; use Resolve + streamable manager for big ones. */
	UFUNCTION(BlueprintCallable, Category = "TV|Manifest")
	UObject* LoadSync(FName ManifestId) const;

	template <typename T>
	T* LoadSync(FName ManifestId) const { return Cast<T>(LoadSync(ManifestId)); }

	const FTVAssetManifestRow* FindRow(FName ManifestId) const;

private:
	UPROPERTY(Transient) TObjectPtr<UDataTable> ManifestTable;
	mutable TSet<FName> WarnedIds;
};
