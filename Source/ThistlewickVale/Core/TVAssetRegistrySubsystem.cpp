#include "Core/TVAssetRegistrySubsystem.h"
#include "Core/TVProjectSettings.h"
#include "ThistlewickVale.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Animation/AnimInstance.h"

void UTVAssetRegistrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ManifestTable = UTVProjectSettings::Get()->AssetManifest.LoadSynchronous();
	if (!ManifestTable)
	{
		UE_LOG(LogTV, Warning, TEXT("DT_AssetManifest is not set in Project Settings > Thistlewick Vale. Manifest IDs will not resolve."));
	}
}

UTVAssetRegistrySubsystem* UTVAssetRegistrySubsystem::Get(const UObject* WorldContext)
{
	if (!GEngine || !WorldContext)
	{
		return nullptr;
	}
	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull);
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UTVAssetRegistrySubsystem>() : nullptr;
}

const FTVAssetManifestRow* UTVAssetRegistrySubsystem::FindRow(FName ManifestId) const
{
	if (!ManifestTable || ManifestId.IsNone())
	{
		return nullptr;
	}
	return ManifestTable->FindRow<FTVAssetManifestRow>(ManifestId, TEXT("TVAssetRegistry"), /*bWarnIfRowMissing*/ false);
}

TSoftObjectPtr<UObject> UTVAssetRegistrySubsystem::Resolve(FName ManifestId) const
{
	if (const FTVAssetManifestRow* Row = FindRow(ManifestId))
	{
		return Row->Asset;
	}
	if (!ManifestId.IsNone() && !WarnedIds.Contains(ManifestId))
	{
		WarnedIds.Add(ManifestId);
		UE_LOG(LogTV, Warning, TEXT("Unknown manifest ID '%s'"), *ManifestId.ToString());
	}
	return nullptr;
}

UObject* UTVAssetRegistrySubsystem::LoadSync(FName ManifestId) const
{
	return Resolve(ManifestId).LoadSynchronous();
}

FSoftObjectPath UTVAssetRegistrySubsystem::ToGeneratedClassPath(const FSoftObjectPath& AssetPath)
{
	if (AssetPath.IsNull())
	{
		return AssetPath;
	}
	// Blueprint generated classes live in the blueprint's package as <AssetName>_C.
	const FString AssetName = AssetPath.GetAssetName();
	if (AssetName.EndsWith(TEXT("_C")))
	{
		return AssetPath;
	}
	return FSoftObjectPath(FString::Printf(TEXT("%s.%s_C"), *AssetPath.GetLongPackageName(), *AssetName));
}

UClass* UTVAssetRegistrySubsystem::LoadAnimClass(FName ManifestId) const
{
	if (ManifestId.IsNone())
	{
		return nullptr;
	}
	const FTVAssetManifestRow* Row = FindRow(ManifestId);
	if (!Row)
	{
		Resolve(ManifestId); // logs the unknown-ID warning once
		return nullptr;
	}
	const FSoftObjectPath ClassPath = ToGeneratedClassPath(Row->Asset.ToSoftObjectPath());
	if (ClassPath.IsNull())
	{
		return nullptr; // planned entry without an asset yet
	}
	UClass* Class = TSoftClassPtr<UObject>(ClassPath).LoadSynchronous();
	if (!Class || !Class->IsChildOf(UAnimInstance::StaticClass()))
	{
		if (!WarnedIds.Contains(ManifestId))
		{
			WarnedIds.Add(ManifestId);
			UE_LOG(LogTV, Warning, TEXT("Manifest ID '%s' (%s) is not an animation blueprint class"), *ManifestId.ToString(), *ClassPath.ToString());
		}
		return nullptr;
	}
	return Class;
}
