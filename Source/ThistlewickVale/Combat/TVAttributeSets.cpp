#include "Combat/TVAttributeSets.h"
#include "Combat/TVCombatLibrary.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

// ---------------------------------------------------------------------------------------------------------------------
// Vitals
// ---------------------------------------------------------------------------------------------------------------------

UTVAttributeSet_Vitals::UTVAttributeSet_Vitals()
{
	InitHealth(100.f);
	InitMaxHealth(100.f);
	InitResource(0.f);
	InitMaxResource(100.f);
	InitResourceRegen(0.f);
	InitIncomingDamage(0.f);
	InitIncomingHeal(0.f);
}

void UTVAttributeSet_Vitals::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UTVAttributeSet_Vitals, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTVAttributeSet_Vitals, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTVAttributeSet_Vitals, Resource, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTVAttributeSet_Vitals, MaxResource, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTVAttributeSet_Vitals, ResourceRegen, COND_OwnerOnly, REPNOTIFY_Always);
}

void UTVAttributeSet_Vitals::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, FMath::Max(1.f, GetMaxHealth()));
	}
	else if (Attribute == GetResourceAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, FMath::Max(0.f, GetMaxResource()));
	}
	else if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(1.f, NewValue);
	}
}

void UTVAttributeSet_Vitals::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, FMath::Max(1.f, GetMaxHealth()));
	}
	else if (Attribute == GetResourceAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, FMath::Max(0.f, GetMaxResource()));
	}
}

void UTVAttributeSet_Vitals::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float Raw = GetIncomingDamage();
		SetIncomingDamage(0.f);
		if (Raw > 0.f)
		{
			UTVCombatLibrary::ResolveIncomingDamage(&Data.Target, Data.EffectSpec, Raw);
		}
	}
	else if (Data.EvaluatedData.Attribute == GetIncomingHealAttribute())
	{
		const float Raw = GetIncomingHeal();
		SetIncomingHeal(0.f);
		if (Raw > 0.f)
		{
			UTVCombatLibrary::ResolveIncomingHeal(&Data.Target, Data.EffectSpec, Raw);
		}
	}
	else if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
	}
	else if (Data.EvaluatedData.Attribute == GetResourceAttribute())
	{
		SetResource(FMath::Clamp(GetResource(), 0.f, GetMaxResource()));
	}
}

void UTVAttributeSet_Vitals::OnRep_Health(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UTVAttributeSet_Vitals, Health, OldValue); }
void UTVAttributeSet_Vitals::OnRep_MaxHealth(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UTVAttributeSet_Vitals, MaxHealth, OldValue); }
void UTVAttributeSet_Vitals::OnRep_Resource(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UTVAttributeSet_Vitals, Resource, OldValue); }
void UTVAttributeSet_Vitals::OnRep_MaxResource(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UTVAttributeSet_Vitals, MaxResource, OldValue); }
void UTVAttributeSet_Vitals::OnRep_ResourceRegen(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UTVAttributeSet_Vitals, ResourceRegen, OldValue); }

// ---------------------------------------------------------------------------------------------------------------------
// Stats
// ---------------------------------------------------------------------------------------------------------------------

UTVAttributeSet_Stats::UTVAttributeSet_Stats()
{
	InitStamina(0.f);
	InitStrength(0.f);
	InitAgility(0.f);
	InitIntellect(0.f);
	InitSpirit(0.f);
	InitCrit(0.f);
	InitHaste(0.f);
	InitArmor(0.f);
	InitMoveSpeedMultiplier(1.f);
	InitWeaponDamage(0.f);
}

void UTVAttributeSet_Stats::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// Character sheet stats only matter to the owner; move speed must reach everyone for smooth simulated movement.
	DOREPLIFETIME_CONDITION_NOTIFY(UTVAttributeSet_Stats, Stamina, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTVAttributeSet_Stats, Strength, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTVAttributeSet_Stats, Agility, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTVAttributeSet_Stats, Intellect, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTVAttributeSet_Stats, Spirit, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTVAttributeSet_Stats, Crit, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTVAttributeSet_Stats, Haste, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTVAttributeSet_Stats, Armor, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTVAttributeSet_Stats, MoveSpeedMultiplier, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTVAttributeSet_Stats, WeaponDamage, COND_OwnerOnly, REPNOTIFY_Always);
}

void UTVAttributeSet_Stats::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	if (Attribute == GetMoveSpeedMultiplierAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, 3.f);
	}
	else
	{
		NewValue = FMath::Max(0.f, NewValue);
	}
}

void UTVAttributeSet_Stats::OnRep_WeaponDamage(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UTVAttributeSet_Stats, WeaponDamage, OldValue); }
void UTVAttributeSet_Stats::OnRep_Stamina(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UTVAttributeSet_Stats, Stamina, OldValue); }
void UTVAttributeSet_Stats::OnRep_Strength(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UTVAttributeSet_Stats, Strength, OldValue); }
void UTVAttributeSet_Stats::OnRep_Agility(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UTVAttributeSet_Stats, Agility, OldValue); }
void UTVAttributeSet_Stats::OnRep_Intellect(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UTVAttributeSet_Stats, Intellect, OldValue); }
void UTVAttributeSet_Stats::OnRep_Spirit(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UTVAttributeSet_Stats, Spirit, OldValue); }
void UTVAttributeSet_Stats::OnRep_Crit(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UTVAttributeSet_Stats, Crit, OldValue); }
void UTVAttributeSet_Stats::OnRep_Haste(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UTVAttributeSet_Stats, Haste, OldValue); }
void UTVAttributeSet_Stats::OnRep_Armor(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UTVAttributeSet_Stats, Armor, OldValue); }
void UTVAttributeSet_Stats::OnRep_MoveSpeedMultiplier(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UTVAttributeSet_Stats, MoveSpeedMultiplier, OldValue); }

// ---------------------------------------------------------------------------------------------------------------------
// Combat
// ---------------------------------------------------------------------------------------------------------------------

UTVAttributeSet_Combat::UTVAttributeSet_Combat()
{
	InitDamageTakenMultiplier(1.f);
	InitDamageDoneMultiplier(1.f);
	InitHealingDoneMultiplier(1.f);
	InitThreatMultiplier(1.f);
	InitAbsorbShield(0.f);
	InitHealingReceivedMultiplier(1.f);
	InitFrontalDamageTakenMod(0.f);
	InitDodgeChance(0.f);
	InitCritChanceBonus(0.f);
	InitDefensiveCooldownMultiplier(1.f);
	InitUltimatePowerMultiplier(1.f);
	InitOutOfCombatMoveSpeedBonus(0.f);
	InitArmorPctModifier(0.f);
}

void UTVAttributeSet_Combat::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UTVAttributeSet_Combat, DamageTakenMultiplier, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTVAttributeSet_Combat, DamageDoneMultiplier, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTVAttributeSet_Combat, HealingDoneMultiplier, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTVAttributeSet_Combat, ThreatMultiplier, COND_OwnerOnly, REPNOTIFY_Always);
	// Shields are shown on everyone's health bars.
	DOREPLIFETIME_CONDITION_NOTIFY(UTVAttributeSet_Combat, AbsorbShield, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTVAttributeSet_Combat, HealingReceivedMultiplier, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTVAttributeSet_Combat, DodgeChance, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTVAttributeSet_Combat, FrontalDamageTakenMod, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTVAttributeSet_Combat, CritChanceBonus, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTVAttributeSet_Combat, DefensiveCooldownMultiplier, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTVAttributeSet_Combat, UltimatePowerMultiplier, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTVAttributeSet_Combat, OutOfCombatMoveSpeedBonus, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTVAttributeSet_Combat, ArmorPctModifier, COND_OwnerOnly, REPNOTIFY_Always);
}

void UTVAttributeSet_Combat::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	if (Attribute == GetDamageTakenMultiplierAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.1f, 5.f);
	}
	else if (Attribute == GetDodgeChanceAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, 0.95f);
	}
	else if (Attribute == GetHealingReceivedMultiplierAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, 5.f);
	}
	else if (Attribute == GetFrontalDamageTakenModAttribute())
	{
		NewValue = FMath::Clamp(NewValue, -0.9f, 5.f);
	}
	else if (Attribute == GetDefensiveCooldownMultiplierAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.25f, 2.f);
	}
	else if (Attribute == GetArmorPctModifierAttribute())
	{
		NewValue = FMath::Clamp(NewValue, -1.f, 5.f);
	}
	else
	{
		NewValue = FMath::Max(0.f, NewValue);
	}
}

void UTVAttributeSet_Combat::OnRep_DamageTakenMultiplier(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UTVAttributeSet_Combat, DamageTakenMultiplier, OldValue); }
void UTVAttributeSet_Combat::OnRep_DamageDoneMultiplier(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UTVAttributeSet_Combat, DamageDoneMultiplier, OldValue); }
void UTVAttributeSet_Combat::OnRep_HealingDoneMultiplier(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UTVAttributeSet_Combat, HealingDoneMultiplier, OldValue); }
void UTVAttributeSet_Combat::OnRep_ThreatMultiplier(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UTVAttributeSet_Combat, ThreatMultiplier, OldValue); }
void UTVAttributeSet_Combat::OnRep_AbsorbShield(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UTVAttributeSet_Combat, AbsorbShield, OldValue); }
void UTVAttributeSet_Combat::OnRep_HealingReceivedMultiplier(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UTVAttributeSet_Combat, HealingReceivedMultiplier, OldValue); }
void UTVAttributeSet_Combat::OnRep_DodgeChance(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UTVAttributeSet_Combat, DodgeChance, OldValue); }
void UTVAttributeSet_Combat::OnRep_CritChanceBonus(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UTVAttributeSet_Combat, CritChanceBonus, OldValue); }
void UTVAttributeSet_Combat::OnRep_DefensiveCooldownMultiplier(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UTVAttributeSet_Combat, DefensiveCooldownMultiplier, OldValue); }
void UTVAttributeSet_Combat::OnRep_UltimatePowerMultiplier(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UTVAttributeSet_Combat, UltimatePowerMultiplier, OldValue); }
void UTVAttributeSet_Combat::OnRep_OutOfCombatMoveSpeedBonus(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UTVAttributeSet_Combat, OutOfCombatMoveSpeedBonus, OldValue); }
void UTVAttributeSet_Combat::OnRep_ArmorPctModifier(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UTVAttributeSet_Combat, ArmorPctModifier, OldValue); }
void UTVAttributeSet_Combat::OnRep_FrontalDamageTakenMod(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UTVAttributeSet_Combat, FrontalDamageTakenMod, OldValue); }
