#include "Systems/TVMountComponent.h"
#include "Systems/TVMountDataAsset.h"
#include "Combat/TVPlayerState.h"
#include "Combat/TVAbilitySystemComponent.h"
#include "AbilitySystemComponent.h"
#include "Combat/TVAttributeSets.h"
#include "Combat/TVCombatEventsSubsystem.h"
#include "Core/TVAssetRegistrySubsystem.h"
#include "ThistlewickVale.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/World.h"
#include "UObject/UObjectGlobals.h"

namespace TVMount
{
	static const FName MountedTagName(TEXT("TV.State.Mounted"));

	static FGameplayTag MountedTag()
	{
		// Tag is declared by Combat in Config/Tags/TVGameplayTags.ini. ErrorIfNotFound=false keeps tests and editor tools quiet if it is missing.
		return FGameplayTag::RequestGameplayTag(MountedTagName, /*ErrorIfNotFound*/ false);
	}
}

UTVMountComponent::UTVMountComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	MountCatalog.Add(TSoftObjectPtr<UTVMountDataAsset>(FSoftObjectPath(TEXT("/Game/MMO/Data/DA_Mount_Dawnstag.DA_Mount_Dawnstag"))));
}

void UTVMountComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UTVMountComponent, OwnedMounts, COND_OwnerOnly);
	DOREPLIFETIME(UTVMountComponent, bMounted);
	DOREPLIFETIME(UTVMountComponent, ActiveMountId);
}

void UTVMountComponent::BeginPlay()
{
	Super::BeginPlay();
	if (APlayerState* PS = Cast<APlayerState>(GetOwner()))
	{
		PS->OnPawnSet.AddDynamic(this, &UTVMountComponent::HandlePawnSet);
	}
	if (HasAuthority())
	{
		BindServerEvents();
	}
}

void UTVMountComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindServerEvents();
	DestroyMountVisual();
	Super::EndPlay(EndPlayReason);
}

bool UTVMountComponent::HasAuthority() const
{
	const AActor* Owner = GetOwner();
	return Owner && Owner->HasAuthority();
}

ATVPlayerState* UTVMountComponent::GetTVPlayerState() const
{
	return Cast<ATVPlayerState>(GetOwner());
}

const UTVMountDataAsset* UTVMountComponent::GetMountData(FName MountId) const
{
	for (const TSoftObjectPtr<UTVMountDataAsset>& Soft : MountCatalog)
	{
		const UTVMountDataAsset* Data = Soft.LoadSynchronous();
		if (Data && (MountId.IsNone() || Data->MountId == MountId))
		{
			return Data;
		}
	}
	return nullptr;
}

void UTVMountComponent::GrantMount(FName MountId)
{
	if (HasAuthority() && !MountId.IsNone())
	{
		OwnedMounts.AddUnique(MountId);
	}
}

// ---- Server events ----

void UTVMountComponent::BindServerEvents()
{
	UWorld* World = GetWorld();
	if (UTVCombatEventsSubsystem* Events = World ? World->GetSubsystem<UTVCombatEventsSubsystem>() : nullptr)
	{
		DamagedHandle = Events->OnPlayerDamaged.AddUObject(this, &UTVMountComponent::HandlePlayerDamaged);
	}
	ATVPlayerState* PS = GetTVPlayerState();
	if (UAbilitySystemComponent* ASC = PS ? PS->GetAbilitySystemComponent() : nullptr)
	{
		// FGenericAbilityDelegate is DECLARE_MULTICAST_DELEGATE_OneParam(FGenericAbilityDelegate, UGameplayAbility*) (checked in 5.8 Abilities/GameplayAbilityTypes.h).
		AbilityActivatedHandle = ASC->AbilityActivatedCallbacks.AddUObject(this, &UTVMountComponent::HandleAbilityActivated);
		// Verified in 5.8 AbilitySystemComponent.h: FAbilityFailedDelegate(const UGameplayAbility*, const FGameplayTagContainer&).
		AbilityFailedHandle = ASC->AbilityFailedCallbacks.AddUObject(this, &UTVMountComponent::HandleAbilityFailed);
	}
}

void UTVMountComponent::UnbindServerEvents()
{
	UWorld* World = GetWorld();
	if (UTVCombatEventsSubsystem* Events = World ? World->GetSubsystem<UTVCombatEventsSubsystem>() : nullptr)
	{
		Events->OnPlayerDamaged.Remove(DamagedHandle);
	}
	ATVPlayerState* PS = GetTVPlayerState();
	if (UAbilitySystemComponent* ASC = PS ? PS->GetAbilitySystemComponent() : nullptr)
	{
		ASC->AbilityActivatedCallbacks.Remove(AbilityActivatedHandle);
		ASC->AbilityFailedCallbacks.Remove(AbilityFailedHandle);
	}
	DamagedHandle.Reset();
	AbilityActivatedHandle.Reset();
	AbilityFailedHandle.Reset();
}

void UTVMountComponent::HandlePlayerDamaged(ATVPlayerState* Player, float Amount)
{
	if (!bMounted || Player != GetTVPlayerState() || Amount <= 0.f)
	{
		return;
	}
	const UTVMountDataAsset* Data = GetMountData(ActiveMountId);
	if (!Data || Data->DismountOnDamage)
	{
		ServerSetMounted(false);
	}
}

void UTVMountComponent::HandleAbilityActivated(UGameplayAbility* Ability)
{
	// Any ability other than the summon itself dismounts. The summon ability's class name contains "SummonMount" (GA_TV_SummonMount).
	if (bMounted && Ability && !Ability->GetClass()->GetName().Contains(TEXT("SummonMount")))
	{
		ServerSetMounted(false);
	}
}

void UTVMountComponent::HandleAbilityFailed(const UGameplayAbility* Ability, const FGameplayTagContainer& FailureTags)
{
	// TV.State.Mounted blocks abilities, so "dismount on ability use" mostly arrives here as a blocked activation.
	if (bMounted && Ability && !Ability->GetClass()->GetName().Contains(TEXT("SummonMount")))
	{
		ServerSetMounted(false);
	}
}

void UTVMountComponent::NotifyEnteredDungeon()
{
	if (!HasAuthority())
	{
		return;
	}
	bInDungeon = true;
	ServerSetMounted(false);
}

void UTVMountComponent::NotifyLeftDungeon()
{
	bInDungeon = false;
}

// ---- Mount state ----

void UTVMountComponent::ServerSetMounted(bool bNewMounted)
{
	if (!HasAuthority() || bNewMounted == bMounted)
	{
		return;
	}
	if (bNewMounted)
	{
		if (bInDungeon || OwnedMounts.Num() == 0)
		{
			return;
		}
		if (ActiveMountId.IsNone() || !OwnedMounts.Contains(ActiveMountId))
		{
			ActiveMountId = OwnedMounts[0];
		}
	}
	bMounted = bNewMounted;
	ApplySpeed(bMounted);
	ApplyMountedState();
	OnMountedChanged.Broadcast(bMounted);
}

void UTVMountComponent::ServerRequestDismount_Implementation()
{
	ServerSetMounted(false);
}

void UTVMountComponent::OnRep_Mounted()
{
	ApplyMountedState();
	OnMountedChanged.Broadcast(bMounted);
}

void UTVMountComponent::HandlePawnSet(APlayerState* Player, APawn* NewPawn, APawn* OldPawn)
{
	if (HasAuthority() && bMounted && NewPawn != OldPawn && OldPawn)
	{
		// New pawn (respawn): never arrive mounted.
		ServerSetMounted(false);
		return;
	}
	DestroyMountVisual();
	ApplyMountedState();
}

void UTVMountComponent::ApplyMountedState()
{
	ApplyTag(bMounted);
	if (bMounted)
	{
		SpawnMountVisual();
	}
	else
	{
		DestroyMountVisual();
	}
}

void UTVMountComponent::ApplyTag(bool bOn)
{
	if (bOn == bTagApplied)
	{
		return;
	}
	ATVPlayerState* PS = GetTVPlayerState();
	UAbilitySystemComponent* ASC = PS ? PS->GetAbilitySystemComponent() : nullptr;
	const FGameplayTag Tag = TVMount::MountedTag();
	if (!ASC || !Tag.IsValid())
	{
		return;
	}
	// Loose tags do not replicate by default; this runs on the server and in OnRep on every client, so both sides agree.
	if (bOn)
	{
		ASC->AddLooseGameplayTag(Tag);
	}
	else
	{
		ASC->RemoveLooseGameplayTag(Tag);
	}
	bTagApplied = bOn;
}

void UTVMountComponent::ApplySpeed(bool bOn)
{
	if (!HasAuthority() || bOn == bSpeedApplied)
	{
		return;
	}
	ATVPlayerState* PS = GetTVPlayerState();
	UAbilitySystemComponent* ASC = PS ? PS->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return;
	}
	const UTVMountDataAsset* Data = GetMountData(ActiveMountId);
	const float Multiplier = Data ? FMath::Max(0.01f, Data->SpeedMultiplier) : 1.6f;
	// INTERFACE-REQUEST: UTVAttributeSet_Stats::GetMoveSpeedMultiplierAttribute() (standard ATTRIBUTE_ACCESSORS) is assumed.
	const FGameplayAttribute Attribute = UTVAttributeSet_Stats::GetMoveSpeedMultiplierAttribute();
	const float Base = ASC->GetNumericAttributeBase(Attribute);
	if (bOn)
	{
		SpeedBaseBeforeMount = Base;
		ASC->SetNumericAttributeBase(Attribute, Base * Multiplier);
	}
	else
	{
		ASC->SetNumericAttributeBase(Attribute, SpeedBaseBeforeMount);
	}
	bSpeedApplied = bOn;
}

void UTVMountComponent::SpawnMountVisual()
{
	const UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer || MountMesh)
	{
		return;
	}
	const APlayerState* PS = Cast<APlayerState>(GetOwner());
	ACharacter* Character = PS ? PS->GetPawn<ACharacter>() : nullptr;
	const UTVMountDataAsset* Data = GetMountData(ActiveMountId);
	UTVAssetRegistrySubsystem* Manifest = UTVAssetRegistrySubsystem::Get(this);
	if (!Character || !Character->GetMesh() || !Data || !Manifest)
	{
		return;
	}

	UObject* Asset = Manifest->LoadSync(Data->MeshId);
	USceneComponent* Root = Character->GetCapsuleComponent();
	const float HalfHeight = Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
	const FName CompName = MakeUniqueObjectName(Character, UMeshComponent::StaticClass(), TEXT("TVMountMesh"));

	if (USkeletalMesh* SkelMesh = Cast<USkeletalMesh>(Asset))
	{
		USkeletalMeshComponent* Comp = NewObject<USkeletalMeshComponent>(Character, CompName);
		Comp->SetSkeletalMeshAsset(SkelMesh);
		if (UObject* AnimAsset = Manifest->LoadSync(Data->AnimBlueprintId))
		{
			UClass* AnimClass = Cast<UClass>(AnimAsset);
			if (!AnimClass)
			{
				// Manifest points at the ABP asset; the runtime class is the generated "_C" class.
				// Generated classes are named <Asset>_C in the same package ("/Game/X/ABP.ABP_C"). In a cooked build the
				// UAnimBlueprint itself is editor-only and does not load, so the manifest should point at the _C class there.
				const FString ClassPath = AnimAsset->GetPathName() + TEXT("_C");
				AnimClass = LoadObject<UClass>(nullptr, *ClassPath);
			}
			if (AnimClass && AnimClass->IsChildOf(UAnimInstance::StaticClass()))
			{
				Comp->SetAnimInstanceClass(AnimClass);
			}
		}
		MountMesh = Comp;
	}
	else if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Asset))
	{
		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(Character, CompName);
		Comp->SetStaticMesh(StaticMesh);
		MountMesh = Comp;
	}
	if (!MountMesh)
	{
		return;
	}

	MountMesh->SetupAttachment(Root);
	MountMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MountMesh->SetCanEverAffectNavigation(false);
	MountMesh->SetRelativeLocation(FVector(0.f, 0.f, -HalfHeight)); // feet on the ground, same convention as the character mesh
	MountMesh->RegisterComponent();

	// Seat the rider: the body mesh rides the mount_seat socket.
	USkeletalMeshComponent* Body = Character->GetMesh();
	RiddenBy = Character;
	RiderOriginalParent = Body->GetAttachParent();
	RiderOriginalRelative = Body->GetRelativeTransform();
	RiderOriginalAnimClass = Body->GetAnimClass();
	const FName Seat = Data->SeatSocket.IsNone() ? FName(TEXT("mount_seat")) : Data->SeatSocket;
	Body->AttachToComponent(MountMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Seat);
	if (UAnimSequenceBase* Pose = Cast<UAnimSequenceBase>(Manifest->LoadSync(Data->RiderPoseAnimId)))
	{
		Body->PlayAnimation(Pose, /*bLooping*/ true);
	}
}

void UTVMountComponent::DestroyMountVisual()
{
	if (ACharacter* Character = RiddenBy.Get())
	{
		if (USkeletalMeshComponent* Body = Character->GetMesh())
		{
			USceneComponent* Parent = RiderOriginalParent.IsValid() ? RiderOriginalParent.Get() : Character->GetRootComponent();
			Body->AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform);
			Body->SetRelativeTransform(RiderOriginalRelative);
			if (UClass* AnimClass = RiderOriginalAnimClass.Get())
			{
				Body->SetAnimationMode(EAnimationMode::AnimationBlueprint);
				Body->SetAnimInstanceClass(AnimClass);
			}
		}
	}
	RiddenBy.Reset();
	RiderOriginalParent.Reset();
	RiderOriginalAnimClass.Reset();
	if (MountMesh)
	{
		MountMesh->DestroyComponent();
		MountMesh = nullptr;
	}
}
