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

	/**
	 * Animation blueprint entry -> its generated anim instance class, loaded synchronously. The manifest may point at the
	 * UAnimBlueprint asset ("/Game/X/ABP_Y.ABP_Y") or at the class ("/Game/X/ABP_Y.ABP_Y_C"); either way the class is
	 * loaded by its _C path and the blueprint asset itself is never loaded, because it is editor-only and is not in a
	 * packaged build. Null (warning once per ID) if the ID is unknown or does not name an anim instance class.
	 */
	UFUNCTION(BlueprintCallable, Category = "TV|Manifest")
	UClass* LoadAnimClass(FName ManifestId) const;

	/** "/Game/X/ABP_Y.ABP_Y" -> "/Game/X/ABP_Y.ABP_Y_C"; paths already ending in _C are returned unchanged. */
	static FSoftObjectPath ToGeneratedClassPath(const FSoftObjectPath& AssetPath);

private:
	UPROPERTY(Transient) TObjectPtr<UDataTable> ManifestTable;
	mutable TSet<FName> WarnedIds;
};
