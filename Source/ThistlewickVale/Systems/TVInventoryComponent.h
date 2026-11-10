// 20-slot bag on ATVPlayerState. Server-authoritative; the item list replicates to the owning client only.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/TVTypes.h"
#include "TVInventoryComponent.generated.h"

class ATVPlayerState;
class ATVNpc;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTVOnInventoryChanged);

UCLASS(ClassGroup = (TV), meta = (BlueprintSpawnableComponent))
class THISTLEWICKVALE_API UTVInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	static constexpr int32 MaxSlots = 20;

	UTVInventoryComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ---- Server API (authority only; returns false on clients) ----

	/** Adds an item that the server created or validated. Fails when full, when the GUID already exists, or when the item is invalid. */
	bool AddItem(const FTVItemInstance& Item);
	/** Removes by GUID. */
	bool RemoveItem(FGuid InstanceGuid);
	/** Removes by GUID and hands the item back (used by equip). */
	bool TakeItem(FGuid InstanceGuid, FTVItemInstance& OutItem);
	/** Replaces the whole bag (save loader). Items must already be validated. Extra items beyond MaxSlots are dropped and logged. */
	void SetItemsFromSave(const TArray<FTVItemInstance>& InItems);

	// ---- Queries ----

	const TArray<FTVItemInstance>& GetItems() const { return Items; }
	const FTVItemInstance* FindItem(FGuid InstanceGuid) const;
	int32 GetFreeSlots() const { return MaxSlots - Items.Num(); }
	bool IsFull() const { return Items.Num() >= MaxSlots; }

	UFUNCTION(BlueprintPure, Category = "TV|Inventory", meta = (DisplayName = "Get Items"))
	TArray<FTVItemInstance> K2_GetItems() const { return Items; }

	// ---- Client requests ----

	/** Swap two bag positions (UI drag and drop). */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "TV|Inventory")
	void ServerMoveItem(int32 FromIndex, int32 ToIndex);

	/** Destroy an item. */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "TV|Inventory")
	void ServerDestroyItem(FGuid InstanceGuid);

	/** Sell to a vendor NPC in range for the item's SellValue. */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "TV|Inventory")
	void ServerSellItem(FGuid InstanceGuid, ATVNpc* Vendor);

	/** Buy one item the vendor lists; price = SellValue x BuyPriceMultiplier. Rolls a Common item at the item's minimum level. */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "TV|Inventory")
	void ServerBuyItem(FName ItemDefId, ATVNpc* Vendor);

	UPROPERTY(BlueprintAssignable, Category = "TV|Inventory")
	FTVOnInventoryChanged OnInventoryChanged;

	UPROPERTY(EditDefaultsOnly, Category = "TV|Inventory")
	int32 BuyPriceMultiplier = 4;

protected:
	UFUNCTION()
	void OnRep_Items();

	ATVPlayerState* GetTVPlayerState() const;
	bool HasAuthority() const;
	void NotifyChanged();

	UPROPERTY(ReplicatedUsing = OnRep_Items)
	TArray<FTVItemInstance> Items;
};
