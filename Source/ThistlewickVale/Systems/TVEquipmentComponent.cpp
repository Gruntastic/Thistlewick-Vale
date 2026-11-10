#include "Systems/TVEquipmentComponent.h"
#include "Systems/TVInventoryComponent.h"
#include "Systems/TVLootLibrary.h"
#include "Systems/TVSetBonusDataAsset.h"
#include "Combat/TVPlayerState.h"
#include "Combat/TVAbilitySystemComponent.h"
#include "Core/TVAssetRegistrySubsystem.h"
#include "ThistlewickVale.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"

namespace TVEquip
{
	static const FName SocketMainHand(TEXT("hand_r_weapon"));
	static const FName SocketOffHand(TEXT("hand_l_offhand"));
	static const FName SocketHead(TEXT("head_slot"));
	static const FName SocketChest(TEXT("fx_chest"));

	static int32 BonusKey(ETVSetTier Tier, int32 Pieces) { return static_cast<int32>(Tier) * 10 + Pieces; }

	static TSoftObjectPtr<UTVSetBonusDataAsset> DefaultSetAsset(const TCHAR* Name)
	{
		return TSoftObjectPtr<UTVSetBonusDataAsset>(FSoftObjectPath(FString::Printf(TEXT("/Game/MMO/Data/%s.%s"), Name, Name)));
	}
}

UTVEquipmentComponent::UTVEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	SetBonusAssets.Add(ETVSetTier::T1_Wanderers, TVEquip::DefaultSetAsset(TEXT("DA_SetBonus_T1")));
	SetBonusAssets.Add(ETVSetTier::T2_ValeGuard, TVEquip::DefaultSetAsset(TEXT("DA_SetBonus_T2")));
	SetBonusAssets.Add(ETVSetTier::T3_HollowCrown, TVEquip::DefaultSetAsset(TEXT("DA_SetBonus_T3")));
}

void UTVEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UTVEquipmentComponent, EquippedItems, COND_OwnerOnly);
	DOREPLIFETIME(UTVEquipmentComponent, Visuals);
}

void UTVEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();
	if (APlayerState* PS = Cast<APlayerState>(GetOwner()))
	{
		PS->OnPawnSet.AddDynamic(this, &UTVEquipmentComponent::HandlePawnSet);
	}
	RefreshVisuals();
}

void UTVEquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearVisualComponents();
	Super::EndPlay(EndPlayReason);
}

bool UTVEquipmentComponent::HasAuthority() const
{
	const AActor* Owner = GetOwner();
	return Owner && Owner->HasAuthority();
}

ATVPlayerState* UTVEquipmentComponent::GetTVPlayerState() const
{
	return Cast<ATVPlayerState>(GetOwner());
}

int32 UTVEquipmentComponent::IndexOfSlot(ETVEquipSlot Slot) const
{
	return EquippedItems.IndexOfByPredicate([Slot](const FTVItemInstance& It) { return It.Slot == Slot; });
}

const FTVItemInstance* UTVEquipmentComponent::GetEquipped(ETVEquipSlot Slot) const
{
	const int32 Index = IndexOfSlot(Slot);
	return Index == INDEX_NONE ? nullptr : &EquippedItems[Index];
}

int32 UTVEquipmentComponent::GetSetPieceCount(ETVSetTier Tier) const
{
	if (Tier == ETVSetTier::None)
	{
		return 0;
	}
	int32 Count = 0;
	for (const FTVItemInstance& Item : EquippedItems)
	{
		// Every equipped piece of the tier counts, weapon included (max 5), per Docs/BALANCE.md section 1.
		if (Item.SetTier == Tier)
		{
			++Count;
		}
	}
	return Count;
}

TMap<ETVStat, float> UTVEquipmentComponent::ComputeStatTotals() const
{
	TMap<ETVStat, float> Totals;
	for (const FTVItemInstance& Item : EquippedItems)
	{
		for (const FTVRolledStat& Stat : Item.RolledStats)
		{
			Totals.FindOrAdd(Stat.Stat) += static_cast<float>(Stat.Value);
		}
		FTVItemDefRow Def;
		if (UTVLootLibrary::GetItemDef(Item.ItemDefId, Def) && Def.BaseArmor > 0.f)
		{
			Totals.FindOrAdd(ETVStat::Armor) += Def.BaseArmor;
		}
		// INTERFACE-REQUEST: BaseWeaponDamage has no attribute in DATA_SCHEMAS section 3; Combat to say where weapon damage goes.
	}
	return Totals;
}

// ---- Equip / unequip ----

bool UTVEquipmentComponent::ServerEquip_Validate(FGuid InstanceGuid)
{
	return InstanceGuid.IsValid();
}

void UTVEquipmentComponent::ServerEquip_Implementation(FGuid InstanceGuid)
{
	ATVPlayerState* PS = GetTVPlayerState();
	UTVInventoryComponent* Inventory = PS ? PS->GetInventory() : nullptr;
	if (!Inventory)
	{
		return;
	}
	const FTVItemInstance* InBag = Inventory->FindItem(InstanceGuid);
	if (!InBag)
	{
		return;
	}
	FTVItemDefRow Def;
	if (!UTVLootLibrary::GetItemDef(InBag->ItemDefId, Def))
	{
		return;
	}
	if (Def.Slot == ETVEquipSlot::None || InBag->Slot != Def.Slot)
	{
		UE_LOG(LogTV, Warning, TEXT("Equip refused: '%s' has no valid slot."), *InBag->ItemDefId.ToString());
		return;
	}
	if (!Def.IsUsableByClass(PS->ClassId))
	{
		UE_LOG(LogTV, Verbose, TEXT("Equip refused: class %s cannot use '%s'."), *PS->ClassId.ToString(), *InBag->ItemDefId.ToString());
		return;
	}

	FTVItemInstance Item;
	if (!Inventory->TakeItem(InstanceGuid, Item))
	{
		return;
	}
	const int32 Existing = IndexOfSlot(Item.Slot);
	if (Existing != INDEX_NONE)
	{
		const FTVItemInstance Previous = EquippedItems[Existing];
		EquippedItems.RemoveAt(Existing);
		// A slot just freed up in the bag, so this cannot fail unless something else filled it.
		if (!Inventory->AddItem(Previous))
		{
			UE_LOG(LogTV, Error, TEXT("Equip: could not return '%s' to the bag; restoring."), *Previous.ItemDefId.ToString());
			EquippedItems.Add(Previous);
			Inventory->AddItem(Item);
			return;
		}
	}
	EquippedItems.Add(Item);
	OnEquippedItemsChangedServer();
}

bool UTVEquipmentComponent::ServerUnequip_Validate(ETVEquipSlot Slot)
{
	return Slot != ETVEquipSlot::None;
}

void UTVEquipmentComponent::ServerUnequip_Implementation(ETVEquipSlot Slot)
{
	ATVPlayerState* PS = GetTVPlayerState();
	UTVInventoryComponent* Inventory = PS ? PS->GetInventory() : nullptr;
	const int32 Index = IndexOfSlot(Slot);
	if (!Inventory || Index == INDEX_NONE || Inventory->IsFull())
	{
		return;
	}
	const FTVItemInstance Item = EquippedItems[Index];
	if (Inventory->AddItem(Item))
	{
		EquippedItems.RemoveAt(Index);
		OnEquippedItemsChangedServer();
	}
}

void UTVEquipmentComponent::RestoreFromSave(const TArray<FTVItemInstance>& InItems, TArray<FTVItemInstance>& OutRejected)
{
	if (!HasAuthority())
	{
		return;
	}
	const ATVPlayerState* PS = GetTVPlayerState();
	EquippedItems.Reset();
	for (const FTVItemInstance& Item : InItems)
	{
		FTVItemDefRow Def;
		const bool bKnown = UTVLootLibrary::GetItemDef(Item.ItemDefId, Def);
		const bool bOk = bKnown && Def.Slot != ETVEquipSlot::None && Item.Slot == Def.Slot && IndexOfSlot(Item.Slot) == INDEX_NONE
			&& (!PS || Def.IsUsableByClass(PS->ClassId));
		if (bOk)
		{
			EquippedItems.Add(Item);
		}
		else
		{
			OutRejected.Add(Item);
		}
	}
	OnEquippedItemsChangedServer();
}

// ---- Server side effects ----

void UTVEquipmentComponent::OnEquippedItemsChangedServer()
{
	PushStatsToASC();
	UpdateSetBonuses();
	RebuildVisualList();
	OnEquipmentChanged.Broadcast();
	RefreshVisuals(); // listen server / standalone draw locally
}

void UTVEquipmentComponent::PushStatsToASC()
{
	ATVPlayerState* PS = GetTVPlayerState();
	if (!PS)
	{
		return;
	}
	if (UTVAbilitySystemComponent* ASC = Cast<UTVAbilitySystemComponent>(PS->GetAbilitySystemComponent()))
	{
		ASC->SetEquipmentStats(ComputeStatTotals());
	}
}

void UTVEquipmentComponent::UpdateSetBonuses()
{
	ATVPlayerState* PS = GetTVPlayerState();
	UTVAbilitySystemComponent* ASC = PS ? Cast<UTVAbilitySystemComponent>(PS->GetAbilitySystemComponent()) : nullptr;
	if (!ASC)
	{
		return;
	}

	TSet<int32> ShouldBeActive;
	for (const TPair<ETVSetTier, TSoftObjectPtr<UTVSetBonusDataAsset>>& Pair : SetBonusAssets)
	{
		const int32 Pieces = GetSetPieceCount(Pair.Key);
		if (Pieces < 2)
		{
			continue;
		}
		const UTVSetBonusDataAsset* Asset = Pair.Value.LoadSynchronous();
		if (!Asset)
		{
			continue;
		}
		for (const FTVSetBonusEntry& Bonus : Asset->Bonuses)
		{
			if (Bonus.PiecesRequired > Pieces)
			{
				continue;
			}
			const int32 Key = TVEquip::BonusKey(Pair.Key, Bonus.PiecesRequired);
			ShouldBeActive.Add(Key);
			if (ActiveBonusHandles.Contains(Key))
			{
				continue;
			}
			TSubclassOf<UGameplayEffect> EffectClass = Bonus.GameplayEffect.LoadSynchronous();
			if (!EffectClass)
			{
				UE_LOG(LogTV, Warning, TEXT("Set bonus %d-piece for tier %d has no GameplayEffect."), Bonus.PiecesRequired, static_cast<int32>(Pair.Key));
				continue;
			}
			ActiveBonusHandles.Add(Key, ASC->ApplyBonusEffect(EffectClass, Bonus.Magnitude));
		}
	}

	for (auto It = ActiveBonusHandles.CreateIterator(); It; ++It)
	{
		if (!ShouldBeActive.Contains(It.Key()))
		{
			ASC->RemoveBonusEffect(It.Value());
			It.RemoveCurrent();
		}
	}
}

void UTVEquipmentComponent::RebuildVisualList()
{
	TArray<FTVEquippedVisual> NewVisuals;
	for (const FTVItemInstance& Item : EquippedItems)
	{
		FTVItemDefRow Def;
		if (!UTVLootLibrary::GetItemDef(Item.ItemDefId, Def) || Def.MeshId.IsNone())
		{
			continue;
		}
		FTVEquippedVisual Visual;
		Visual.Slot = Item.Slot;
		Visual.MeshId = Def.MeshId;
		Visual.MaterialInstanceId = Def.MaterialInstanceId;
		NewVisuals.Add(Visual);
	}
	Visuals = MoveTemp(NewVisuals);
}

// ---- Replication ----

void UTVEquipmentComponent::OnRep_EquippedItems()
{
	OnEquipmentChanged.Broadcast();
}

void UTVEquipmentComponent::OnRep_Visuals()
{
	RefreshVisuals();
}

void UTVEquipmentComponent::HandlePawnSet(APlayerState* Player, APawn* NewPawn, APawn* OldPawn)
{
	RefreshVisuals();
}

// ---- Visuals ----

FName UTVEquipmentComponent::GetSocketForSlot(ETVEquipSlot Slot)
{
	switch (Slot)
	{
	case ETVEquipSlot::MainHand: return TVEquip::SocketMainHand;
	case ETVEquipSlot::OffHand:  return TVEquip::SocketOffHand;
	case ETVEquipSlot::Head:     return TVEquip::SocketHead;  // used only by static-mesh placeholders
	case ETVEquipSlot::Chest:    return TVEquip::SocketChest; // used only by static-mesh placeholders
	default:                     return NAME_None;
	}
}

void UTVEquipmentComponent::ClearVisualComponents()
{
	for (TPair<ETVEquipSlot, TObjectPtr<UMeshComponent>>& Pair : VisualComponents)
	{
		if (UMeshComponent* Comp = Pair.Value.Get())
		{
			Comp->DestroyComponent();
		}
	}
	VisualComponents.Reset();
	AppliedVisuals.Reset();
}

void UTVEquipmentComponent::RefreshVisuals()
{
	const UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return; // nothing to draw
	}
	const APlayerState* PS = Cast<APlayerState>(GetOwner());
	ACharacter* Character = PS ? PS->GetPawn<ACharacter>() : nullptr;
	if (VisualPawn.Get() != Character)
	{
		ClearVisualComponents(); // components belonged to the old pawn (or it died)
		VisualPawn = Character;
	}
	if (!Character || !Character->GetMesh())
	{
		return;
	}
	USkeletalMeshComponent* Body = Character->GetMesh();
	UTVAssetRegistrySubsystem* Manifest = UTVAssetRegistrySubsystem::Get(this);

	// Remove slots that are no longer equipped or changed.
	for (auto It = AppliedVisuals.CreateIterator(); It; ++It)
	{
		const FTVEquippedVisual* Wanted = Visuals.FindByPredicate([&It](const FTVEquippedVisual& V) { return V.Slot == It.Key(); });
		if (!Wanted || !(*Wanted == It.Value()))
		{
			if (TObjectPtr<UMeshComponent>* Comp = VisualComponents.Find(It.Key()))
			{
				if (*Comp)
				{
					(*Comp)->DestroyComponent();
				}
				VisualComponents.Remove(It.Key());
			}
			It.RemoveCurrent();
		}
	}

	if (!Manifest)
	{
		return;
	}

	for (const FTVEquippedVisual& Visual : Visuals)
	{
		if (AppliedVisuals.Contains(Visual.Slot))
		{
			continue;
		}
		UObject* Asset = Manifest->LoadSync(Visual.MeshId);
		UMaterialInterface* Material = Visual.MaterialInstanceId.IsNone() ? nullptr : Cast<UMaterialInterface>(Manifest->LoadSync(Visual.MaterialInstanceId));
		const bool bWeapon = Visual.Slot == ETVEquipSlot::MainHand || Visual.Slot == ETVEquipSlot::OffHand;
		const FName CompName = MakeUniqueObjectName(Character, UMeshComponent::StaticClass(), *FString::Printf(TEXT("TVGear_%d"), static_cast<int32>(Visual.Slot)));
		UMeshComponent* NewComp = nullptr;

		if (USkeletalMesh* SkelMesh = Cast<USkeletalMesh>(Asset))
		{
			USkeletalMeshComponent* Comp = NewObject<USkeletalMeshComponent>(Character, CompName);
			Comp->SetSkeletalMeshAsset(SkelMesh);
			Comp->RegisterComponent();
			if (bWeapon)
			{
				Comp->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, GetSocketForSlot(Visual.Slot));
			}
			else
			{
				// Armor follows the body skeleton. Verified in 5.8 SkinnedMeshComponent.h: SetLeaderPoseComponent(USkinnedMeshComponent*, bool bForceUpdate, bool bInFollowerShouldTickPose).
				Comp->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
				Comp->SetLeaderPoseComponent(Body, /*bForceUpdate*/ true);
			}
			NewComp = Comp;
		}
		else if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Asset))
		{
			// Placeholder (or a real weapon, which ART_CONTRACT says are static meshes): attach to the slot socket.
			UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(Character, CompName);
			Comp->SetStaticMesh(StaticMesh);
			Comp->RegisterComponent();
			Comp->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, GetSocketForSlot(Visual.Slot));
			NewComp = Comp;
		}
		else
		{
			continue; // unknown manifest ID was already warned about by the registry
		}

		NewComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		NewComp->SetCanEverAffectNavigation(false);
		if (Material)
		{
			NewComp->SetMaterial(0, Material);
		}
		VisualComponents.Add(Visual.Slot, NewComp);
		AppliedVisuals.Add(Visual.Slot, Visual);
	}
}
