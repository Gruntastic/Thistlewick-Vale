// Paper doll on ATVPlayerState: Head, Chest, Legs, Boots, MainHand, OffHand.
// Server: class/slot checks, stat totals -> UTVAbilitySystemComponent::SetEquipmentStats, 2/4-piece set bonuses.
// Everyone: equipped visuals replicate as manifest IDs and are applied to the owner's pawn.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "UObject/SoftObjectPtr.h"
#include "Core/TVTypes.h"
#include "TVEquipmentComponent.generated.h"

class ATVPlayerState;
class APlayerState;
class APawn;
class ACharacter;
class UMeshComponent;
class UTVSetBonusDataAsset;

/** What other clients need to draw an equipped item. */
USTRUCT(BlueprintType)
struct THISTLEWICKVALE_API FTVEquippedVisual
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TV|Equipment") ETVEquipSlot Slot = ETVEquipSlot::None;
	UPROPERTY(BlueprintReadOnly, Category = "TV|Equipment") FName MeshId;
	UPROPERTY(BlueprintReadOnly, Category = "TV|Equipment") FName MaterialInstanceId;

	bool operator==(const FTVEquippedVisual& Other) const
	{
		return Slot == Other.Slot && MeshId == Other.MeshId && MaterialInstanceId == Other.MaterialInstanceId;
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTVOnEquipmentChanged);

UCLASS(ClassGroup = (TV), meta = (BlueprintSpawnableComponent))
class THISTLEWICKVALE_API UTVEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTVEquipmentComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Moves an item from the bag to its slot; the previous item in that slot goes back to the bag. */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "TV|Equipment")
	void ServerEquip(FGuid InstanceGuid);

	/** Moves the item in Slot back to the bag (needs a free bag slot). */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "TV|Equipment")
	void ServerUnequip(ETVEquipSlot Slot);

	const FTVItemInstance* GetEquipped(ETVEquipSlot Slot) const;
	const TArray<FTVItemInstance>& GetEquippedItems() const { return EquippedItems; }

	UFUNCTION(BlueprintPure, Category = "TV|Equipment")
	int32 GetSetPieceCount(ETVSetTier Tier) const;

	/** Sum of rolled stats plus BaseArmor (into Armor). */
	TMap<ETVStat, float> ComputeStatTotals() const;

	/** Save loader: equips already-validated items directly (no bag round trip). Items failing class/slot checks are returned in OutRejected. */
	void RestoreFromSave(const TArray<FTVItemInstance>& InItems, TArray<FTVItemInstance>& OutRejected);

	/** Re-applies the replicated visuals to the current pawn. Called automatically on replication and pawn change. */
	void RefreshVisuals();

	const TArray<FTVEquippedVisual>& GetVisuals() const { return Visuals; }

	UPROPERTY(BlueprintAssignable, Category = "TV|Equipment")
	FTVOnEquipmentChanged OnEquipmentChanged;

	/** Set bonus assets per tier. Defaults follow the import script's naming (/Game/MMO/Data/DA_SetBonus_T1 / _T2 / _T3). */
	UPROPERTY(EditDefaultsOnly, Category = "TV|Equipment")
	TMap<ETVSetTier, TSoftObjectPtr<UTVSetBonusDataAsset>> SetBonusAssets;

	static FName GetSocketForSlot(ETVEquipSlot Slot);

protected:
	UFUNCTION() void OnRep_EquippedItems();
	UFUNCTION() void OnRep_Visuals();
	UFUNCTION() void HandlePawnSet(APlayerState* Player, APawn* NewPawn, APawn* OldPawn);

	ATVPlayerState* GetTVPlayerState() const;
	bool HasAuthority() const;

	/** Server: everything that follows a change of EquippedItems. */
	void OnEquippedItemsChangedServer();
	void PushStatsToASC();
	void UpdateSetBonuses();
	void RebuildVisualList();
	void ClearVisualComponents();
	int32 IndexOfSlot(ETVEquipSlot Slot) const;

	/** Owner-only: full items (stats for the character sheet). */
	UPROPERTY(ReplicatedUsing = OnRep_EquippedItems)
	TArray<FTVItemInstance> EquippedItems;

	/** Everyone: manifest IDs for drawing. */
	UPROPERTY(ReplicatedUsing = OnRep_Visuals)
	TArray<FTVEquippedVisual> Visuals;

	/** Server: active set-bonus effects, key = tier * 10 + pieces. */
	TMap<int32, FActiveGameplayEffectHandle> ActiveBonusHandles;

	/** Local: spawned mesh components per slot, and what they show. */
	UPROPERTY(Transient)
	TMap<ETVEquipSlot, TObjectPtr<UMeshComponent>> VisualComponents;
	TMap<ETVEquipSlot, FTVEquippedVisual> AppliedVisuals;
	TWeakObjectPtr<APawn> VisualPawn;
};
