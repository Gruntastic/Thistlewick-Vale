// DA_SetBonus_<Tier> (Docs/DATA_SCHEMAS.md section 4). Created from Data/Source/DA_SetBonus_*.json by Tools/ue/import_data.py.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UObject/SoftObjectPtr.h"
#include "GameplayEffect.h"
#include "Core/TVTypes.h"
#include "TVSetBonusDataAsset.generated.h"

USTRUCT(BlueprintType)
struct THISTLEWICKVALE_API FTVSetBonusEntry
{
	GENERATED_BODY()

	/** 2 or 4. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|SetBonus") int32 PiecesRequired = 2;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|SetBonus") FText DisplayText;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|SetBonus") TSoftClassPtr<UGameplayEffect> GameplayEffect;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|SetBonus") float Magnitude = 0.f;
};

UCLASS(BlueprintType)
class THISTLEWICKVALE_API UTVSetBonusDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|SetBonus") ETVSetTier SetTier = ETVSetTier::None;
	/** The schema leaves this array unnamed; it is called Bonuses. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|SetBonus") TArray<FTVSetBonusEntry> Bonuses;
};
