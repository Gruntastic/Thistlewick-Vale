// Attribute sets (Docs/DATA_SCHEMAS.md section 3). Players own these on ATVPlayerState, mobs on their pawn.
#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "TVAttributeSets.generated.h"

// ATTRIBUTE_ACCESSORS_BASIC exists in 5.8 AttributeSet.h (checked).
#define TV_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) ATTRIBUTE_ACCESSORS_BASIC(ClassName, PropertyName)

/** Health and the class resource (Rage/Mana/Focus), plus the meta attributes IncomingDamage / IncomingHeal. */
UCLASS()
class THISTLEWICKVALE_API UTVAttributeSet_Vitals : public UAttributeSet
{
	GENERATED_BODY()

public:
	UTVAttributeSet_Vitals();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "TV|Vitals") FGameplayAttributeData Health;
	TV_ATTRIBUTE_ACCESSORS(UTVAttributeSet_Vitals, Health)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "TV|Vitals") FGameplayAttributeData MaxHealth;
	TV_ATTRIBUTE_ACCESSORS(UTVAttributeSet_Vitals, MaxHealth)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Resource, Category = "TV|Vitals") FGameplayAttributeData Resource;
	TV_ATTRIBUTE_ACCESSORS(UTVAttributeSet_Vitals, Resource)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxResource, Category = "TV|Vitals") FGameplayAttributeData MaxResource;
	TV_ATTRIBUTE_ACCESSORS(UTVAttributeSet_Vitals, MaxResource)
	/** Per second. Rage: decay rate out of combat; Mana: regen (5-second rule applies); Focus: regen. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ResourceRegen, Category = "TV|Vitals") FGameplayAttributeData ResourceRegen;
	TV_ATTRIBUTE_ACCESSORS(UTVAttributeSet_Vitals, ResourceRegen)

	/** Meta, server only: raw damage before target-side mitigation. */
	UPROPERTY(BlueprintReadOnly, Category = "TV|Meta") FGameplayAttributeData IncomingDamage;
	TV_ATTRIBUTE_ACCESSORS(UTVAttributeSet_Vitals, IncomingDamage)
	/** Meta, server only: raw heal before heal absorb. */
	UPROPERTY(BlueprintReadOnly, Category = "TV|Meta") FGameplayAttributeData IncomingHeal;
	TV_ATTRIBUTE_ACCESSORS(UTVAttributeSet_Vitals, IncomingHeal)

protected:
	UFUNCTION() void OnRep_Health(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Resource(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxResource(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_ResourceRegen(const FGameplayAttributeData& OldValue);
};

/** Primary and secondary stats. Crit and Haste are ratings: 1 point = 1 percent. Armor is flat. */
UCLASS()
class THISTLEWICKVALE_API UTVAttributeSet_Stats : public UAttributeSet
{
	GENERATED_BODY()

public:
	UTVAttributeSet_Stats();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Stamina, Category = "TV|Stats") FGameplayAttributeData Stamina;
	TV_ATTRIBUTE_ACCESSORS(UTVAttributeSet_Stats, Stamina)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Strength, Category = "TV|Stats") FGameplayAttributeData Strength;
	TV_ATTRIBUTE_ACCESSORS(UTVAttributeSet_Stats, Strength)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Agility, Category = "TV|Stats") FGameplayAttributeData Agility;
	TV_ATTRIBUTE_ACCESSORS(UTVAttributeSet_Stats, Agility)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Intellect, Category = "TV|Stats") FGameplayAttributeData Intellect;
	TV_ATTRIBUTE_ACCESSORS(UTVAttributeSet_Stats, Intellect)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Spirit, Category = "TV|Stats") FGameplayAttributeData Spirit;
	TV_ATTRIBUTE_ACCESSORS(UTVAttributeSet_Stats, Spirit)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Crit, Category = "TV|Stats") FGameplayAttributeData Crit;
	TV_ATTRIBUTE_ACCESSORS(UTVAttributeSet_Stats, Crit)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Haste, Category = "TV|Stats") FGameplayAttributeData Haste;
	TV_ATTRIBUTE_ACCESSORS(UTVAttributeSet_Stats, Haste)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Armor, Category = "TV|Stats") FGameplayAttributeData Armor;
	TV_ATTRIBUTE_ACCESSORS(UTVAttributeSet_Stats, Armor)
	/** 1.0 = normal. Character max walk speed = base x this (INTERFACES.md). Mount adds +0.6. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MoveSpeedMultiplier, Category = "TV|Stats") FGameplayAttributeData MoveSpeedMultiplier;
	TV_ATTRIBUTE_ACCESSORS(UTVAttributeSet_Stats, MoveSpeedMultiplier)

protected:
	UFUNCTION() void OnRep_Stamina(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Strength(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Agility(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Intellect(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Spirit(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Crit(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Haste(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Armor(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MoveSpeedMultiplier(const FGameplayAttributeData& OldValue);
};

/**
 * Combat modifiers that buffs/debuffs/set bonuses touch. Not in DATA_SCHEMAS §3 but internal to Combat:
 * keeping them as attributes lets every buff be a plain gameplay effect modifier.
 */
UCLASS()
class THISTLEWICKVALE_API UTVAttributeSet_Combat : public UAttributeSet
{
	GENERATED_BODY()

public:
	UTVAttributeSet_Combat();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	/** 1.0 normal. Iron Wall 0.6, Dawn's Vigil 0.5. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DamageTakenMultiplier, Category = "TV|Combat") FGameplayAttributeData DamageTakenMultiplier;
	TV_ATTRIBUTE_ACCESSORS(UTVAttributeSet_Combat, DamageTakenMultiplier)
	/** 1.0 normal. Mob party scaling and boss enrage write here. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DamageDoneMultiplier, Category = "TV|Combat") FGameplayAttributeData DamageDoneMultiplier;
	TV_ATTRIBUTE_ACCESSORS(UTVAttributeSet_Combat, DamageDoneMultiplier)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HealingDoneMultiplier, Category = "TV|Combat") FGameplayAttributeData HealingDoneMultiplier;
	TV_ATTRIBUTE_ACCESSORS(UTVAttributeSet_Combat, HealingDoneMultiplier)
	/** 1.0 normal. Bulwark of the Vale x2. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ThreatMultiplier, Category = "TV|Combat") FGameplayAttributeData ThreatMultiplier;
	TV_ATTRIBUTE_ACCESSORS(UTVAttributeSet_Combat, ThreatMultiplier)
	/** Remaining absorb shield points (Glimmer Ward). Base value is consumed by damage. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AbsorbShield, Category = "TV|Combat") FGameplayAttributeData AbsorbShield;
	TV_ATTRIBUTE_ACCESSORS(UTVAttributeSet_Combat, AbsorbShield)
	/** 1.0 normal. Grave Brand -0.3 (healing absorbed). */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HealingReceivedMultiplier, Category = "TV|Combat") FGameplayAttributeData HealingReceivedMultiplier;
	TV_ATTRIBUTE_ACCESSORS(UTVAttributeSet_Combat, HealingReceivedMultiplier)
	/** Damage from attackers in front of this actor x (1 + this). Footman shield block -0.5. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_FrontalDamageTakenMod, Category = "TV|Combat") FGameplayAttributeData FrontalDamageTakenMod;
	TV_ATTRIBUTE_ACCESSORS(UTVAttributeSet_Combat, FrontalDamageTakenMod)
	/** 0-1 chance to dodge a direct hit (Fade Step 0.5). */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DodgeChance, Category = "TV|Combat") FGameplayAttributeData DodgeChance;
	TV_ATTRIBUTE_ACCESSORS(UTVAttributeSet_Combat, DodgeChance)
	/** Extra crit chance as a fraction (T3 2-piece +0.05). */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CritChanceBonus, Category = "TV|Combat") FGameplayAttributeData CritChanceBonus;
	TV_ATTRIBUTE_ACCESSORS(UTVAttributeSet_Combat, CritChanceBonus)
	/** Multiplies Defensive-slot cooldowns (T2 4-piece 0.85). */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DefensiveCooldownMultiplier, Category = "TV|Combat") FGameplayAttributeData DefensiveCooldownMultiplier;
	TV_ATTRIBUTE_ACCESSORS(UTVAttributeSet_Combat, DefensiveCooldownMultiplier)
	/** Multiplies Ultimate-slot damage/heal (T3 4-piece 1.2). */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_UltimatePowerMultiplier, Category = "TV|Combat") FGameplayAttributeData UltimatePowerMultiplier;
	TV_ATTRIBUTE_ACCESSORS(UTVAttributeSet_Combat, UltimatePowerMultiplier)
	/** Effective armor = Armor x (1 + this). Sundering Blows stacks -0.1 each. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ArmorPctModifier, Category = "TV|Combat") FGameplayAttributeData ArmorPctModifier;
	TV_ATTRIBUTE_ACCESSORS(UTVAttributeSet_Combat, ArmorPctModifier)
	/** Added to MoveSpeedMultiplier while out of combat (T1 2-piece 0.05). */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_OutOfCombatMoveSpeedBonus, Category = "TV|Combat") FGameplayAttributeData OutOfCombatMoveSpeedBonus;
	TV_ATTRIBUTE_ACCESSORS(UTVAttributeSet_Combat, OutOfCombatMoveSpeedBonus)

protected:
	UFUNCTION() void OnRep_DamageTakenMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_DamageDoneMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_HealingDoneMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_ThreatMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_AbsorbShield(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_HealingReceivedMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_DodgeChance(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_FrontalDamageTakenMod(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_CritChanceBonus(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_DefensiveCooldownMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_UltimatePowerMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_ArmorPctModifier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_OutOfCombatMoveSpeedBonus(const FGameplayAttributeData& OldValue);
};
