// DA_Mount_Dawnstag (Docs/DATA_SCHEMAS.md section 7). Created from Data/Source/DA_Mount_*.json by Tools/ue/import_data.py.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TVMountDataAsset.generated.h"

UCLASS(BlueprintType)
class THISTLEWICKVALE_API UTVMountDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Mount") FName MountId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Mount") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Mount") FName MeshId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Mount") FName AnimBlueprintId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Mount") float SummonCastTime = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Mount") float SpeedMultiplier = 1.6f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Mount") bool DismountOnDamage = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Mount") bool BlockAbilities = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Mount") FName SeatSocket = TEXT("mount_seat");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Mount") FName RiderPoseAnimId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Mount") FName SummonVfxId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Mount") FName SummonSfxId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Mount") FName IconId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Mount") FName RequiredQuestId = TEXT("Q06_DAWNSTAG_TRUST");
};
