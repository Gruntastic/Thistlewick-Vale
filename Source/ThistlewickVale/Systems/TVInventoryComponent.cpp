#include "Systems/TVInventoryComponent.h"
#include "Systems/TVLootLibrary.h"
#include "Systems/TVInteractable.h"
#include "Combat/TVPlayerState.h"
#include "ThistlewickVale.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"

UTVInventoryComponent::UTVInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UTVInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// Plain (non push-model) replication: PushModel.h could not be read from the 5.8 install, so it is not used here.
	DOREPLIFETIME_CONDITION(UTVInventoryComponent, Items, COND_OwnerOnly);
}

bool UTVInventoryComponent::HasAuthority() const
{
	const AActor* Owner = GetOwner();
	return Owner && Owner->HasAuthority();
}

ATVPlayerState* UTVInventoryComponent::GetTVPlayerState() const
{
	return Cast<ATVPlayerState>(GetOwner());
}

void UTVInventoryComponent::NotifyChanged()
{
	OnInventoryChanged.Broadcast();
}

void UTVInventoryComponent::OnRep_Items()
{
	NotifyChanged();
}

const FTVItemInstance* UTVInventoryComponent::FindItem(FGuid InstanceGuid) const
{
	return Items.FindByPredicate([&InstanceGuid](const FTVItemInstance& It) { return It.InstanceGuid == InstanceGuid; });
}

bool UTVInventoryComponent::AddItem(const FTVItemInstance& Item)
{
	if (!HasAuthority())
	{
		return false;
	}
	if (!Item.IsValid())
	{
		UE_LOG(LogTV, Warning, TEXT("Inventory: refused invalid item '%s'."), *Item.ItemDefId.ToString());
		return false;
	}
	if (IsFull() || FindItem(Item.InstanceGuid))
	{
		return false;
	}
	Items.Add(Item);
	NotifyChanged(); // server-side listeners (listen-server UI, save dirty flags)
	return true;
}

bool UTVInventoryComponent::RemoveItem(FGuid InstanceGuid)
{
	FTVItemInstance Unused;
	return TakeItem(InstanceGuid, Unused);
}

bool UTVInventoryComponent::TakeItem(FGuid InstanceGuid, FTVItemInstance& OutItem)
{
	if (!HasAuthority())
	{
		return false;
	}
	const int32 Index = Items.IndexOfByPredicate([&InstanceGuid](const FTVItemInstance& It) { return It.InstanceGuid == InstanceGuid; });
	if (Index == INDEX_NONE)
	{
		return false;
	}
	OutItem = Items[Index];
	Items.RemoveAt(Index);
	NotifyChanged();
	return true;
}

void UTVInventoryComponent::SetItemsFromSave(const TArray<FTVItemInstance>& InItems)
{
	if (!HasAuthority())
	{
		return;
	}
	Items.Reset();
	for (const FTVItemInstance& Item : InItems)
	{
		if (Items.Num() >= MaxSlots)
		{
			UE_LOG(LogTV, Warning, TEXT("Inventory: save has more than %d items; '%s' dropped."), MaxSlots, *Item.ItemDefId.ToString());
			continue;
		}
		if (Item.IsValid() && !FindItem(Item.InstanceGuid))
		{
			Items.Add(Item);
		}
	}
	NotifyChanged();
}

// ---- RPCs ----

bool UTVInventoryComponent::ServerMoveItem_Validate(int32 FromIndex, int32 ToIndex)
{
	return FromIndex >= 0 && FromIndex < MaxSlots && ToIndex >= 0 && ToIndex < MaxSlots;
}

void UTVInventoryComponent::ServerMoveItem_Implementation(int32 FromIndex, int32 ToIndex)
{
	if (Items.IsValidIndex(FromIndex) && Items.IsValidIndex(ToIndex) && FromIndex != ToIndex)
	{
		Items.Swap(FromIndex, ToIndex);
		NotifyChanged();
	}
}

bool UTVInventoryComponent::ServerDestroyItem_Validate(FGuid InstanceGuid)
{
	return InstanceGuid.IsValid();
}

void UTVInventoryComponent::ServerDestroyItem_Implementation(FGuid InstanceGuid)
{
	RemoveItem(InstanceGuid);
}

bool UTVInventoryComponent::ServerSellItem_Validate(FGuid InstanceGuid, ATVNpc* Vendor)
{
	return InstanceGuid.IsValid();
}

void UTVInventoryComponent::ServerSellItem_Implementation(FGuid InstanceGuid, ATVNpc* Vendor)
{
	ATVPlayerState* PS = GetTVPlayerState();
	if (!PS || !Vendor || !Vendor->IsVendor() || !Vendor->IsInInteractRange(PS))
	{
		return;
	}
	const FTVItemInstance* Item = FindItem(InstanceGuid);
	if (!Item)
	{
		return;
	}
	FTVItemDefRow Def;
	const int32 Price = UTVLootLibrary::GetItemDef(Item->ItemDefId, Def) ? FMath::Max(0, Def.SellValue) : 0;
	if (RemoveItem(InstanceGuid))
	{
		PS->AddGold(Price);
	}
}

bool UTVInventoryComponent::ServerBuyItem_Validate(FName ItemDefId, ATVNpc* Vendor)
{
	return !ItemDefId.IsNone();
}

void UTVInventoryComponent::ServerBuyItem_Implementation(FName ItemDefId, ATVNpc* Vendor)
{
	ATVPlayerState* PS = GetTVPlayerState();
	if (!PS || !Vendor || !Vendor->IsVendor() || !Vendor->SellsItem(ItemDefId) || !Vendor->IsInInteractRange(PS) || IsFull())
	{
		return;
	}
	FTVItemDefRow Def;
	if (!UTVLootLibrary::GetItemDef(ItemDefId, Def))
	{
		return;
	}
	const int32 Price = FMath::Max(1, Def.SellValue * BuyPriceMultiplier);
	// INTERFACE-REQUEST: ATVPlayerState exposes Gold as a replicated int32 (ledger); read here directly.
	if (PS->Gold < Price)
	{
		return;
	}
	FTVItemInstance Item = UTVLootLibrary::RollItem(Def, ETVRarity::Common, FMath::Max(1, Def.ItemLevelMin), UTVLootLibrary::MakeServerSeed());
	UTVLootLibrary::AssignNewGuid(Item);
	if (AddItem(Item))
	{
		PS->AddGold(-Price);
	}
}
