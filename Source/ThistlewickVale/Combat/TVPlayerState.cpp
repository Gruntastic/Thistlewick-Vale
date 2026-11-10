#include "Combat/TVPlayerState.h"
#include "ThistlewickVale.h"
#include "Combat/TVAbilitySystemComponent.h"
#include "Combat/TVAttributeSets.h"
#include "Combat/TVCharacterBase.h"
#include "Combat/TVCombatLibrary.h"
#include "Combat/TVCombatTags.h"
#include "Systems/TVInventoryComponent.h"
#include "Systems/TVEquipmentComponent.h"
#include "Systems/TVQuestComponent.h"
#include "Systems/TVMountComponent.h"
#include "Net/UnrealNetwork.h"

ATVPlayerState::ATVPlayerState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilitySystem = CreateDefaultSubobject<UTVAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	VitalsSet = CreateDefaultSubobject<UTVAttributeSet_Vitals>(TEXT("VitalsSet"));
	StatsSet = CreateDefaultSubobject<UTVAttributeSet_Stats>(TEXT("StatsSet"));
	CombatSet = CreateDefaultSubobject<UTVAttributeSet_Combat>(TEXT("CombatSet"));

	Inventory = CreateDefaultSubobject<UTVInventoryComponent>(TEXT("Inventory"));
	Equipment = CreateDefaultSubobject<UTVEquipmentComponent>(TEXT("Equipment"));
	Quests = CreateDefaultSubobject<UTVQuestComponent>(TEXT("Quests"));
	Mount = CreateDefaultSubobject<UTVMountComponent>(TEXT("Mount"));

	// The ASC lives here, so the player state must replicate about as often as a pawn (default is 1 Hz).
	SetNetUpdateFrequency(100.f);

	ActionBar.SetNum(NumActionSlots);
}

void ATVPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ATVPlayerState, ActionBar, COND_OwnerOnly);
	DOREPLIFETIME(ATVPlayerState, ClassId);
	DOREPLIFETIME(ATVPlayerState, CharacterName);
	DOREPLIFETIME(ATVPlayerState, Level);
	DOREPLIFETIME_CONDITION(ATVPlayerState, XP, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ATVPlayerState, Gold, COND_OwnerOnly);
}

UAbilitySystemComponent* ATVPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void ATVPlayerState::InitCharacter(FName InClassId, int32 InLevel, int32 InXP)
{
	if (!HasAuthority())
	{
		return;
	}
	if (!AbilitySystem->GetAvatarActor())
	{
		// No pawn yet (save load at login): the pawn re-initialises the avatar on possession.
		AbilitySystem->InitAbilityActorInfo(this, this);
	}
	ClassId = InClassId;
	const int32 OldLevel = Level;
	Level = FMath::Clamp(InLevel, 1, UTVCombatLibrary::GetMaxLevel());
	XP = FMath::Max(0, InXP);

	AbilitySystem->InitializeForClass(ClassId, Level, /*bRefillVitals*/ true);

	bool bBarEmpty = true;
	for (const FName& Id : ActionBar)
	{
		bBarEmpty &= Id.IsNone();
	}
	if (bBarEmpty)
	{
		FillDefaultActionBar();
	}

	OnRep_ClassId();
	OnLevelChanged.Broadcast(OldLevel, Level);
	OnXPChanged.Broadcast(XP, GetXPToNext());
}

void ATVPlayerState::AddXP(int32 Amount)
{
	if (!HasAuthority() || Amount <= 0)
	{
		return;
	}
	XP += Amount;
	const int32 MaxLevel = UTVCombatLibrary::GetMaxLevel();
	while (Level < MaxLevel)
	{
		const int32 ToNext = UTVCombatLibrary::GetXPToNext(Level);
		if (ToNext <= 0 || XP < ToNext)
		{
			break;
		}
		XP -= ToNext;
		SetLevelInternal(Level + 1);
	}
	if (Level >= MaxLevel)
	{
		XP = 0;
	}
	OnXPChanged.Broadcast(XP, GetXPToNext());
}

void ATVPlayerState::SetLevelInternal(int32 NewLevel)
{
	const int32 OldLevel = Level;
	Level = NewLevel;
	// Level up: new base stats, newly unlocked abilities, full vitals.
	AbilitySystem->InitializeForClass(ClassId, Level, /*bRefillVitals*/ true);
	FillDefaultActionBar();
	OnLevelChanged.Broadcast(OldLevel, Level);
}

void ATVPlayerState::AddGold(int32 Amount)
{
	if (!HasAuthority() || Amount == 0)
	{
		return;
	}
	Gold = FMath::Max(0, Gold + Amount);
	OnGoldChanged.Broadcast(Gold);
}

int32 ATVPlayerState::GetXPToNext() const
{
	return UTVCombatLibrary::GetXPToNext(Level);
}

void ATVPlayerState::SetActionBar(const TArray<FName>& InActionBar)
{
	if (!HasAuthority())
	{
		return;
	}
	ActionBar = InActionBar;
	ActionBar.SetNum(NumActionSlots);
	OnActionBarChanged.Broadcast();
}

void ATVPlayerState::ServerSetActionBarSlot_Implementation(int32 SlotIndex, FName AbilityId)
{
	if (!ActionBar.IsValidIndex(SlotIndex))
	{
		return;
	}
	// Only abilities the player actually has.
	if (!AbilityId.IsNone() && !AbilitySystem->FindHandleForAbilityId(AbilityId).IsValid())
	{
		return;
	}
	ActionBar[SlotIndex] = AbilityId;
	OnActionBarChanged.Broadcast();
}

void ATVPlayerState::FillDefaultActionBar()
{
	ActionBar.SetNum(NumActionSlots);
	const FTVClassRow* Row = UTVCombatLibrary::FindClassRow(ClassId);
	if (!Row)
	{
		return;
	}
	bool bChanged = false;
	for (const FTVAbilityUnlock& Unlock : Row->StartingAbilities)
	{
		if (Unlock.UnlockLevel > Level || !ActionBar.IsValidIndex(Unlock.DefaultSlot) || ActionBar.Contains(Unlock.AbilityId))
		{
			continue;
		}
		if (ActionBar[Unlock.DefaultSlot].IsNone())
		{
			ActionBar[Unlock.DefaultSlot] = Unlock.AbilityId;
			bChanged = true;
		}
	}
	if (bChanged)
	{
		OnActionBarChanged.Broadcast();
	}
}

void ATVPlayerState::ReviveAtFullHealth()
{
	if (!HasAuthority())
	{
		return;
	}
	if (TVTags::State_Dead().IsValid())
	{
		AbilitySystem->SetLooseGameplayTagCount(TVTags::State_Dead(), 0);
	}
	AbilitySystem->ClearCombatState(true);
	AbilitySystem->ResetVitalsToFull();
}

void ATVPlayerState::OnRep_Level(int32 OldLevel)
{
	OnLevelChanged.Broadcast(OldLevel, Level);
	OnXPChanged.Broadcast(XP, GetXPToNext());
}

void ATVPlayerState::OnRep_ClassId()
{
	if (ATVPlayerCharacter* Character = Cast<ATVPlayerCharacter>(GetPawn()))
	{
		Character->ApplyClassVisuals();
	}
}

void ATVPlayerState::OnRep_XP()
{
	OnXPChanged.Broadcast(XP, GetXPToNext());
}

void ATVPlayerState::OnRep_Gold()
{
	OnGoldChanged.Broadcast(Gold);
}

void ATVPlayerState::OnRep_ActionBar()
{
	OnActionBarChanged.Broadcast();
}
