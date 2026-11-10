#include "Combat/TVAbilitySystemComponent.h"
#include "ThistlewickVale.h"
#include "Core/TVProjectSettings.h"
#include "Combat/TVAttributeSets.h"
#include "Combat/TVCombatLibrary.h"
#include "Combat/TVCombatTags.h"
#include "Combat/TVCharacterBase.h"
#include "Combat/Effects/TVGameplayEffects.h"
#include "Combat/Abilities/TVGameplayAbility.h"
#include "GameplayEffect.h"
#include "GameplayAbilitySpec.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

namespace
{
	FName StatSetByCallerName(ETVStat Stat)
	{
		switch (Stat)
		{
		case ETVStat::Stamina:   return TEXT("Stat.Stamina");
		case ETVStat::Strength:  return TEXT("Stat.Strength");
		case ETVStat::Agility:   return TEXT("Stat.Agility");
		case ETVStat::Intellect: return TEXT("Stat.Intellect");
		case ETVStat::Spirit:    return TEXT("Stat.Spirit");
		case ETVStat::Crit:      return TEXT("Stat.Crit");
		case ETVStat::Haste:     return TEXT("Stat.Haste");
		case ETVStat::Armor:     return TEXT("Stat.Armor");
		default:                 return NAME_None;
		}
	}

	const FName ComboKindling(TEXT("Kindling"));
	const FName ComboDevotion(TEXT("Devotion"));
	const FName ClassAll(TEXT("ALL"));
	const FName ClassMob(TEXT("MOB"));
}

UTVAbilitySystemComponent::UTVAbilitySystemComponent()
{
	SetIsReplicatedByDefault(true);
}

void UTVAbilitySystemComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UTVAbilitySystemComponent, GrantedAbilities, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UTVAbilitySystemComponent, AbilityTarget, COND_OwnerOnly);
	DOREPLIFETIME(UTVAbilitySystemComponent, CastBar);
	DOREPLIFETIME_CONDITION(UTVAbilitySystemComponent, KindlingStacks, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UTVAbilitySystemComponent, DevotionStacks, COND_OwnerOnly);
	DOREPLIFETIME(UTVAbilitySystemComponent, ResourceType);
	DOREPLIFETIME(UTVAbilitySystemComponent, ClassId);
	DOREPLIFETIME(UTVAbilitySystemComponent, bInCombat);
}

void UTVAbilitySystemComponent::BeginPlay()
{
	Super::BeginPlay();

	if (TVTags::Buff_Absorb().IsValid())
	{
		RegisterGameplayTagEvent(TVTags::Buff_Absorb(), EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UTVAbilitySystemComponent::OnAbsorbTagChanged);
	}
	BindStatCallbacks();

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(RegenTimerHandle, this, &UTVAbilitySystemComponent::RegenTick, RegenInterval, true);
		}
	}
}

void UTVAbilitySystemComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RegenTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void UTVAbilitySystemComponent::BindStatCallbacks()
{
	if (bStatCallbacksBound)
	{
		return;
	}
	bStatCallbacksBound = true;
	GetGameplayAttributeValueChangeDelegate(UTVAttributeSet_Stats::GetStaminaAttribute()).AddUObject(this, &UTVAbilitySystemComponent::OnStatChanged);
	GetGameplayAttributeValueChangeDelegate(UTVAttributeSet_Stats::GetIntellectAttribute()).AddUObject(this, &UTVAbilitySystemComponent::OnStatChanged);
	GetGameplayAttributeValueChangeDelegate(UTVAttributeSet_Stats::GetSpiritAttribute()).AddUObject(this, &UTVAbilitySystemComponent::OnStatChanged);
	GetGameplayAttributeValueChangeDelegate(UTVAttributeSet_Vitals::GetMaxHealthAttribute()).AddUObject(this, &UTVAbilitySystemComponent::OnStatChanged);
}

void UTVAbilitySystemComponent::OnStatChanged(const FOnAttributeChangeData& Data)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	if (Data.Attribute == UTVAttributeSet_Vitals::GetMaxHealthAttribute())
	{
		// Keep the health fraction when max health changes (Bulwark, gear, scaling).
		const FGameplayAttribute HealthAttr = UTVAttributeSet_Vitals::GetHealthAttribute();
		const float Health = GetNumericAttributeBase(HealthAttr);
		if (Data.OldValue > 0.f && Health > 0.f && Data.NewValue > 0.f)
		{
			SetNumericAttributeBase(HealthAttr, FMath::Max(1.f, Health * Data.NewValue / Data.OldValue));
		}
		return;
	}
	RecalculateDerivedAttributes();
}

void UTVAbilitySystemComponent::OnAbsorbTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount <= 0 && GetOwner() && GetOwner()->HasAuthority() && HasAttributeSetForAttribute(UTVAttributeSet_Combat::GetAbsorbShieldAttribute()))
	{
		SetNumericAttributeBase(UTVAttributeSet_Combat::GetAbsorbShieldAttribute(), 0.f);
	}
}

// ---- INTERFACES.md ---------------------------------------------------------------------------------------------------

void UTVAbilitySystemComponent::SetEquipmentStats(const TMap<ETVStat, float>& Totals)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	EquipmentTotals = Totals;
	if (EquipmentEffectHandle.IsValid())
	{
		RemoveActiveGameplayEffect(EquipmentEffectHandle);
		EquipmentEffectHandle.Invalidate();
	}
	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(UGE_TV_EquipmentStats::StaticClass(), 1.f, MakeEffectContext());
	if (SpecHandle.IsValid())
	{
		static const ETVStat AllStats[] = { ETVStat::Stamina, ETVStat::Strength, ETVStat::Agility, ETVStat::Intellect,
			ETVStat::Spirit, ETVStat::Crit, ETVStat::Haste, ETVStat::Armor };
		for (ETVStat Stat : AllStats)
		{
			SpecHandle.Data->SetSetByCallerMagnitude(StatSetByCallerName(Stat), Totals.FindRef(Stat));
		}
		EquipmentEffectHandle = ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
	RecalculateDerivedAttributes();
}

FActiveGameplayEffectHandle UTVAbilitySystemComponent::ApplyBonusEffect(TSubclassOf<UGameplayEffect> Effect, float Magnitude)
{
	FActiveGameplayEffectHandle Handle;
	if (!Effect || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return Handle;
	}
	const UTVGameplayEffect* Def = Cast<UTVGameplayEffect>(Effect->GetDefaultObject());
	float Value = Magnitude;
	if (Def)
	{
		if (Def->MagnitudeMode == ETVMagnitudeMode::OnePlus)
		{
			Value = 1.f + Magnitude;
		}
		else if (Def->MagnitudeMode == ETVMagnitudeMode::Negate)
		{
			Value = -Magnitude;
		}
	}
	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(Effect, 1.f, MakeEffectContext());
	if (!SpecHandle.IsValid())
	{
		return Handle;
	}
	FGameplayEffectSpec& Spec = *SpecHandle.Data.Get();
	Spec.SetSetByCallerMagnitude(TVSetByCaller::Magnitude, Value);
	Spec.SetSetByCallerMagnitude(TVSetByCaller::Duration, 3600.f);
	if (TVTags::SBC_Magnitude().IsValid())
	{
		Spec.SetSetByCallerMagnitude(TVTags::SBC_Magnitude(), Value);
	}
	if (Def)
	{
		for (const FName& TagName : Def->GrantedTagNames)
		{
			const FGameplayTag Tag = TVTags::Get(*TagName.ToString());
			if (Tag.IsValid())
			{
				Spec.DynamicGrantedTags.AddTag(Tag);
			}
		}
	}
	Handle = ApplyGameplayEffectSpecToSelf(Spec);
	RecalculateDerivedAttributes();
	RefreshAvatarMovement();
	return Handle;
}

void UTVAbilitySystemComponent::RemoveBonusEffect(FActiveGameplayEffectHandle Handle)
{
	if (Handle.IsValid() && GetOwner() && GetOwner()->HasAuthority())
	{
		RemoveActiveGameplayEffect(Handle);
		RecalculateDerivedAttributes();
		RefreshAvatarMovement();
	}
}

void UTVAbilitySystemComponent::SetWeaponDamage(float Damage)
{
	if (GetOwner() && GetOwner()->HasAuthority() && HasAttributeSetForAttribute(UTVAttributeSet_Stats::GetWeaponDamageAttribute()))
	{
		SetNumericAttributeBase(UTVAttributeSet_Stats::GetWeaponDamageAttribute(), FMath::Max(0.f, Damage));
	}
}

void UTVAbilitySystemComponent::GrantAbilitiesForClass(FName InClassId, int32 Level)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	if (const FTVClassRow* ClassRow = UTVCombatLibrary::FindClassRow(InClassId))
	{
		for (const FTVAbilityUnlock& Unlock : ClassRow->StartingAbilities)
		{
			if (Unlock.UnlockLevel <= Level)
			{
				GrantAbilityById(Unlock.AbilityId, 1, false);
			}
		}
	}
	// Rows shared by every class (the mount summon).
	const UTVProjectSettings* Settings = UTVProjectSettings::Get();
	if (UDataTable* Table = Settings ? Settings->LoadTable(TEXT("Abilities")) : nullptr)
	{
		TArray<FTVAbilityRow*> Rows;
		Table->GetAllRows<FTVAbilityRow>(TEXT("TVGrant"), Rows);
		for (const FTVAbilityRow* Row : Rows)
		{
			if (Row && Row->ClassId == ClassAll)
			{
				GrantAbilityById(Row->AbilityId, 1, false);
			}
		}
	}
}

bool UTVAbilitySystemComponent::TryActivateAbilityById(FName AbilityId)
{
	const FGameplayAbilitySpecHandle Handle = FindHandleForAbilityId(AbilityId);
	const FTVAbilityRow* Row = UTVCombatLibrary::FindAbilityRow(AbilityId);
	if (!Handle.IsValid() || !Row)
	{
		OnAbilityFailed.Broadcast(AbilityId, TEXT("NotLearned"));
		return false;
	}
	FName Reason;
	if (!UTVGameplayAbility::CheckActivation(this, *Row, Reason, true))
	{
		OnAbilityFailed.Broadcast(AbilityId, Reason);
		return false;
	}
	// Client: make sure the server uses the same target. Same actor channel as ServerTryActivateAbility, so ordered.
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		ServerSetAbilityTarget(AbilityTarget);
	}
	return TryActivateAbility(Handle, true);
}

float UTVAbilitySystemComponent::GetCooldownRemaining(FName AbilityId) const
{
	const FTVAbilityRow* Row = UTVCombatLibrary::FindAbilityRow(AbilityId);
	if (!Row)
	{
		return 0.f;
	}
	const FGameplayTag CooldownTag = TVTags::CooldownTagFor(Row->AbilityTag);
	if (!CooldownTag.IsValid() || !HasMatchingGameplayTag(CooldownTag))
	{
		return 0.f;
	}
	const TArray<float> Times = GetActiveEffectsTimeRemaining(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(CooldownTag)));
	float Max = 0.f;
	for (float Time : Times)
	{
		Max = FMath::Max(Max, Time);
	}
	return Max;
}

float UTVAbilitySystemComponent::GetGCDRemaining() const
{
	const FGameplayTag GCDTag = TVTags::Cooldown_GCD();
	if (!GCDTag.IsValid() || !HasMatchingGameplayTag(GCDTag))
	{
		return 0.f;
	}
	const TArray<float> Times = GetActiveEffectsTimeRemaining(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(GCDTag)));
	float Max = 0.f;
	for (float Time : Times)
	{
		Max = FMath::Max(Max, Time);
	}
	return Max;
}

// ---- Setup -----------------------------------------------------------------------------------------------------------

void UTVAbilitySystemComponent::InitializeForClass(FName InClassId, int32 Level, bool bRefillVitals)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	bIsMob = false;
	ClassId = InClassId;
	const FTVClassRow* ClassRow = UTVCombatLibrary::FindClassRow(InClassId);
	if (!ClassRow)
	{
		UE_LOG(LogTV, Warning, TEXT("InitializeForClass: class %s not found"), *InClassId.ToString());
		return;
	}
	ResourceType = ClassRow->ResourceType;

	const FName CurveId = ClassRow->BaseStatsCurveId.IsNone() ? InClassId : ClassRow->BaseStatsCurveId;
	const FTVClassLevelStatsRow* Stats = UTVCombatLibrary::FindClassLevelStats(CurveId, Level);
	if (!Stats && CurveId != InClassId)
	{
		Stats = UTVCombatLibrary::FindClassLevelStats(InClassId, Level);
	}
	if (Stats)
	{
		BaseStats = *Stats;
	}
	else
	{
		UE_LOG(LogTV, Warning, TEXT("InitializeForClass: no base stats for %s level %d"), *InClassId.ToString(), Level);
	}

	SetNumericAttributeBase(UTVAttributeSet_Stats::GetStaminaAttribute(), BaseStats.Stamina);
	SetNumericAttributeBase(UTVAttributeSet_Stats::GetStrengthAttribute(), BaseStats.Strength);
	SetNumericAttributeBase(UTVAttributeSet_Stats::GetAgilityAttribute(), BaseStats.Agility);
	SetNumericAttributeBase(UTVAttributeSet_Stats::GetIntellectAttribute(), BaseStats.Intellect);
	SetNumericAttributeBase(UTVAttributeSet_Stats::GetSpiritAttribute(), BaseStats.Spirit);
	RecalculateDerivedAttributes();
	if (bRefillVitals)
	{
		ResetVitalsToFull();
	}
	GrantAbilitiesForClass(InClassId, Level);
}

void UTVAbilitySystemComponent::InitializeForMob(const FTVMobRow& Row, int32 Level, float HealthScale)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	bIsMob = true;
	ClassId = ClassMob;
	ResourceType = ETVResource::None;

	const float MaxHealth = FMath::Max(1.f, (Row.BaseHealth + Row.HealthPerLevel * FMath::Max(0, Level - Row.LevelMin)) * FMath::Max(0.01f, HealthScale));
	SetNumericAttributeBase(UTVAttributeSet_Vitals::GetMaxHealthAttribute(), MaxHealth);
	SetNumericAttributeBase(UTVAttributeSet_Vitals::GetHealthAttribute(), MaxHealth);
	SetNumericAttributeBase(UTVAttributeSet_Vitals::GetMaxResourceAttribute(), 0.f);
	SetNumericAttributeBase(UTVAttributeSet_Vitals::GetResourceAttribute(), 0.f);
	SetNumericAttributeBase(UTVAttributeSet_Vitals::GetResourceRegenAttribute(), 0.f);

	for (const FName& AbilityId : Row.Abilities)
	{
		GrantAbilityById(AbilityId, 1, true);
	}
}

FGameplayAbilitySpecHandle UTVAbilitySystemComponent::GrantAbilityById(FName AbilityId, int32 AbilityLevel, bool bForAI)
{
	const FGameplayAbilitySpecHandle Existing = FindHandleForAbilityId(AbilityId);
	if (Existing.IsValid() || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return Existing;
	}
	const FTVAbilityRow* Row = UTVCombatLibrary::FindAbilityRow(AbilityId);
	if (!Row)
	{
		UE_LOG(LogTV, Warning, TEXT("GrantAbilityById: ability %s not found"), *AbilityId.ToString());
		return FGameplayAbilitySpecHandle();
	}
	TSubclassOf<UTVGameplayAbility> AbilityClass = Row->AbilityClass.LoadSynchronous();
	if (!AbilityClass)
	{
		UE_LOG(LogTV, Warning, TEXT("GrantAbilityById: ability %s has no valid AbilityClass (%s)"), *AbilityId.ToString(), *Row->AbilityClass.ToString());
		return FGameplayAbilitySpecHandle();
	}
	if (bForAI)
	{
		const UTVGameplayAbility* CDO = AbilityClass->GetDefaultObject<UTVGameplayAbility>();
		if (CDO && CDO->GetAIVariantClass())
		{
			AbilityClass = CDO->GetAIVariantClass();
		}
	}
	FGameplayAbilitySpec Spec(AbilityClass, FMath::Max(1, AbilityLevel), INDEX_NONE, GetOwner());
	const FGameplayAbilitySpecHandle Handle = GiveAbility(Spec);
	if (Handle.IsValid())
	{
		FTVGrantedAbility& Entry = GrantedAbilities.AddDefaulted_GetRef();
		Entry.AbilityId = AbilityId;
		Entry.Handle = Handle;
		OnGrantedAbilitiesChanged.Broadcast();
	}
	return Handle;
}

void UTVAbilitySystemComponent::ResetVitalsToFull()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	SetNumericAttributeBase(UTVAttributeSet_Vitals::GetHealthAttribute(), GetNumericAttribute(UTVAttributeSet_Vitals::GetMaxHealthAttribute()));
	const float MaxResource = GetNumericAttribute(UTVAttributeSet_Vitals::GetMaxResourceAttribute());
	SetNumericAttributeBase(UTVAttributeSet_Vitals::GetResourceAttribute(), ResourceType == ETVResource::Rage ? 0.f : MaxResource);
}

void UTVAbilitySystemComponent::RecalculateDerivedAttributes()
{
	if (bIsMob || ClassId.IsNone() || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	const float Stamina = GetNumericAttribute(UTVAttributeSet_Stats::GetStaminaAttribute());
	const float Spirit = GetNumericAttribute(UTVAttributeSet_Stats::GetSpiritAttribute());

	const float MaxHealth = FMath::Max(1.f, BaseStats.BaseHealth + TVCombatTuning::HealthPerStamina * Stamina);
	if (!FMath::IsNearlyEqual(GetNumericAttributeBase(UTVAttributeSet_Vitals::GetMaxHealthAttribute()), MaxHealth))
	{
		SetNumericAttributeBase(UTVAttributeSet_Vitals::GetMaxHealthAttribute(), MaxHealth);
	}

	float MaxResource = 100.f;
	float Regen = BaseStats.BaseResourceRegen;
	switch (ResourceType)
	{
	case ETVResource::Mana:
		MaxResource = BaseStats.BaseResourceMax + TVCombatTuning::ManaPerGearIntellect * EquipmentTotals.FindRef(ETVStat::Intellect);
		Regen = BaseStats.BaseResourceRegen + TVCombatTuning::ManaRegenPerSpirit * Spirit;
		break;
	case ETVResource::Rage:
	case ETVResource::Focus:
		MaxResource = BaseStats.BaseResourceMax > 0.f ? BaseStats.BaseResourceMax : 100.f;
		break;
	default:
		MaxResource = 0.f;
		Regen = 0.f;
		break;
	}
	SetNumericAttributeBase(UTVAttributeSet_Vitals::GetMaxResourceAttribute(), FMath::Max(0.f, MaxResource));
	SetNumericAttributeBase(UTVAttributeSet_Vitals::GetResourceRegenAttribute(), Regen);
	const FGameplayAttribute ResourceAttr = UTVAttributeSet_Vitals::GetResourceAttribute();
	if (GetNumericAttributeBase(ResourceAttr) > MaxResource)
	{
		SetNumericAttributeBase(ResourceAttr, MaxResource);
	}
}

FName UTVAbilitySystemComponent::GetAbilityIdForHandle(FGameplayAbilitySpecHandle Handle) const
{
	for (const FTVGrantedAbility& Entry : GrantedAbilities)
	{
		if (Entry.Handle == Handle)
		{
			return Entry.AbilityId;
		}
	}
	return NAME_None;
}

FGameplayAbilitySpecHandle UTVAbilitySystemComponent::FindHandleForAbilityId(FName AbilityId) const
{
	for (const FTVGrantedAbility& Entry : GrantedAbilities)
	{
		if (Entry.AbilityId == AbilityId)
		{
			return Entry.Handle;
		}
	}
	return FGameplayAbilitySpecHandle();
}

TArray<FName> UTVAbilitySystemComponent::GetGrantedAbilityIds() const
{
	TArray<FName> Ids;
	for (const FTVGrantedAbility& Entry : GrantedAbilities)
	{
		Ids.Add(Entry.AbilityId);
	}
	return Ids;
}

// ---- Ability target --------------------------------------------------------------------------------------------------

void UTVAbilitySystemComponent::SetAbilityTarget(AActor* NewTarget)
{
	AbilityTarget = NewTarget;
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		if (ATVCharacterBase* Avatar = Cast<ATVCharacterBase>(GetAvatarActor()))
		{
			Avatar->SetCombatTarget(NewTarget);
		}
	}
}

void UTVAbilitySystemComponent::ServerSetAbilityTarget_Implementation(AActor* NewTarget)
{
	// Reject targets in other worlds or absurdly far away (client input is untrusted).
	if (NewTarget && GetAvatarActor() && FVector::Dist(NewTarget->GetActorLocation(), GetAvatarActor()->GetActorLocation()) > 20000.f)
	{
		return;
	}
	SetAbilityTarget(NewTarget);
}

// ---- Casting ---------------------------------------------------------------------------------------------------------

void UTVAbilitySystemComponent::BeginCastBar(FName AbilityId, float Duration, bool bInterruptible, bool bIsChannel)
{
	UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	CastBar.AbilityId = AbilityId;
	CastBar.StartServerTime = GameState ? static_cast<float>(GameState->GetServerWorldTimeSeconds()) : (World ? World->GetTimeSeconds() : 0.f);
	CastBar.Duration = Duration;
	CastBar.bInterruptible = bInterruptible;
	CastBar.bIsChannel = bIsChannel;
	CastBar.bActive = true;
	const FGameplayTag StateTag = bIsChannel ? TVTags::State_Channeling() : TVTags::State_Casting();
	if (StateTag.IsValid())
	{
		SetLooseGameplayTagCount(StateTag, 1);
	}
	OnCastBarChanged.Broadcast(CastBar);
}

void UTVAbilitySystemComponent::EndCastBar()
{
	if (TVTags::State_Casting().IsValid())
	{
		SetLooseGameplayTagCount(TVTags::State_Casting(), 0);
	}
	if (TVTags::State_Channeling().IsValid())
	{
		SetLooseGameplayTagCount(TVTags::State_Channeling(), 0);
	}
	if (!CastBar.bActive)
	{
		return;
	}
	CastBar = FTVCastBarState();
	OnCastBarChanged.Broadcast(CastBar);
}

void UTVAbilitySystemComponent::SetActiveCastAbility(UTVGameplayAbility* Ability)
{
	ActiveCastAbility = Ability;
}

UTVGameplayAbility* UTVAbilitySystemComponent::GetActiveCastAbility() const
{
	return ActiveCastAbility.Get();
}

bool UTVAbilitySystemComponent::IsCasting() const
{
	const UTVGameplayAbility* Ability = ActiveCastAbility.Get();
	return (Ability && Ability->IsCastingOrChanneling()) || CastBar.bActive;
}

bool UTVAbilitySystemComponent::InterruptCast(AActor* Interrupter, bool bForce)
{
	UTVGameplayAbility* Ability = ActiveCastAbility.Get();
	if (!Ability || !Ability->IsCastingOrChanneling())
	{
		return false;
	}
	if (!bForce && !Ability->IsInterruptible())
	{
		return false;
	}
	Ability->CancelFromServer();
	if (ATVCharacterBase* Avatar = Cast<ATVCharacterBase>(GetAvatarActor()))
	{
		if (Interrupter)
		{
			Avatar->MulticastCombatText(0.f, ETVCombatTextKind::Immune, Interrupter);
		}
	}
	return true;
}

void UTVAbilitySystemComponent::NotifyDamageTakenForCasts()
{
	UTVGameplayAbility* Ability = ActiveCastAbility.Get();
	if (Ability && Ability->IsCastingOrChanneling() && Ability->CancelsOnDamage())
	{
		Ability->CancelFromServer();
	}
}

// ---- Combo -----------------------------------------------------------------------------------------------------------

int32 UTVAbilitySystemComponent::GetComboStacks(FName ComboResource) const
{
	if (ComboResource == ComboKindling)
	{
		return KindlingStacks;
	}
	if (ComboResource == ComboDevotion)
	{
		return DevotionStacks;
	}
	return 0;
}

int32 UTVAbilitySystemComponent::ModifyComboStacks(FName ComboResource, int32 Delta)
{
	int32* Stacks = nullptr;
	if (ComboResource == ComboKindling)
	{
		Stacks = &KindlingStacks;
	}
	else if (ComboResource == ComboDevotion)
	{
		Stacks = &DevotionStacks;
	}
	if (!Stacks)
	{
		return 0;
	}
	const int32 Previous = *Stacks;
	*Stacks = Delta <= -99 ? 0 : FMath::Clamp(Previous + Delta, 0, MaxComboStacks);
	if (*Stacks != Previous)
	{
		OnComboChanged.Broadcast(ComboResource, *Stacks);
	}
	return Previous;
}

void UTVAbilitySystemComponent::OnRep_Combo()
{
	OnComboChanged.Broadcast(ComboKindling, KindlingStacks);
	OnComboChanged.Broadcast(ComboDevotion, DevotionStacks);
}

// ---- Combat state ----------------------------------------------------------------------------------------------------

void UTVAbilitySystemComponent::NotifyCombatActivity()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	const UWorld* World = GetWorld();
	LastCombatTime = World ? World->GetTimeSeconds() : 0.0;
	SetInCombat(true);
}

bool UTVAbilitySystemComponent::IsInCombat() const
{
	return bInCombat;
}

void UTVAbilitySystemComponent::NotifyResourceSpent(float Amount)
{
	if (Amount > 0.f && ResourceType == ETVResource::Mana)
	{
		const UWorld* World = GetWorld();
		LastManaSpendTime = World ? World->GetTimeSeconds() : 0.0;
	}
}

void UTVAbilitySystemComponent::SetInCombat(bool bNewInCombat)
{
	if (bInCombat == bNewInCombat)
	{
		return;
	}
	bInCombat = bNewInCombat;
	if (TVTags::State_InCombat().IsValid())
	{
		SetLooseGameplayTagCount(TVTags::State_InCombat(), bInCombat ? 1 : 0);
	}
	RefreshAvatarMovement();
}

void UTVAbilitySystemComponent::OnRep_InCombat()
{
	if (TVTags::State_InCombat().IsValid())
	{
		SetLooseGameplayTagCount(TVTags::State_InCombat(), bInCombat ? 1 : 0);
	}
	RefreshAvatarMovement();
}

void UTVAbilitySystemComponent::RefreshAvatarMovement() const
{
	if (ATVCharacterBase* Avatar = Cast<ATVCharacterBase>(GetAvatarActor()))
	{
		Avatar->UpdateMovementSpeed();
	}
}

void UTVAbilitySystemComponent::ClearCombatState(bool bRemoveDebuffs)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	if (bRemoveDebuffs && TVTags::Debuff().IsValid())
	{
		RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(TVTags::Debuff()));
	}
	InterruptCast(nullptr, true);
	EndCastBar();
	ModifyComboStacks(ComboKindling, -99);
	ModifyComboStacks(ComboDevotion, -99);
	LastCombatTime = -1000.0;
	SetInCombat(false);
}

void UTVAbilitySystemComponent::ClientAbilityFailed_Implementation(FName AbilityId, FName Reason)
{
	OnAbilityFailed.Broadcast(AbilityId, Reason);
}

void UTVAbilitySystemComponent::OnRep_GrantedAbilities()
{
	OnGrantedAbilitiesChanged.Broadcast();
}

void UTVAbilitySystemComponent::OnRep_CastBar()
{
	OnCastBarChanged.Broadcast(CastBar);
}

// ---- Regen (server, every RegenInterval) -----------------------------------------------------------------------------

void UTVAbilitySystemComponent::RegenTick()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const ATVCharacterBase* Avatar = Cast<ATVCharacterBase>(GetAvatarActor());
	if (!Avatar || Avatar->IsDead())
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	const float Dt = RegenInterval;

	// Combat drops after CombatDropTime without activity (mobs refresh players on their threat tables).
	if (bInCombat && Now - LastCombatTime > TVCombatTuning::CombatDropTime)
	{
		SetInCombat(false);
	}

	const FGameplayAttribute ResourceAttr = UTVAttributeSet_Vitals::GetResourceAttribute();
	const float Resource = GetNumericAttributeBase(ResourceAttr);
	const float MaxResource = GetNumericAttribute(UTVAttributeSet_Vitals::GetMaxResourceAttribute());
	const float Regen = GetNumericAttribute(UTVAttributeSet_Vitals::GetResourceRegenAttribute());
	float NewResource = Resource;
	switch (ResourceType)
	{
	case ETVResource::Rage:
		if (!bInCombat)
		{
			// BaseResourceRegen is negative (decay).
			NewResource = Resource + FMath::Min(Regen, 0.f) * Dt;
		}
		break;
	case ETVResource::Mana:
	{
		const bool bFiveSecondRule = IsCasting() || (Now - LastManaSpendTime) < TVCombatTuning::FiveSecondRuleWindow;
		NewResource = Resource + Regen * Dt * (bFiveSecondRule ? TVCombatTuning::FiveSecondRuleFactor : 1.f);
		break;
	}
	case ETVResource::Focus:
		NewResource = Resource + Regen * Dt;
		break;
	default:
		break;
	}
	NewResource = FMath::Clamp(NewResource, 0.f, FMath::Max(0.f, MaxResource));
	if (!FMath::IsNearlyEqual(NewResource, Resource))
	{
		SetNumericAttributeBase(ResourceAttr, NewResource);
	}

	// Players regenerate health out of combat.
	if (!bIsMob && !bInCombat)
	{
		const FGameplayAttribute HealthAttr = UTVAttributeSet_Vitals::GetHealthAttribute();
		const float Health = GetNumericAttributeBase(HealthAttr);
		const float MaxHealth = GetNumericAttribute(UTVAttributeSet_Vitals::GetMaxHealthAttribute());
		if (Health > 0.f && Health < MaxHealth)
		{
			SetNumericAttributeBase(HealthAttr, FMath::Min(MaxHealth, Health + MaxHealth * TVCombatTuning::PlayerOutOfCombatHealthRegen * Dt));
		}
	}
}
