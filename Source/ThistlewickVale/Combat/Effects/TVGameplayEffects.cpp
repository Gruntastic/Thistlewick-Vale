#include "Combat/Effects/TVGameplayEffects.h"
#include "Combat/TVAttributeSets.h"
#include "Combat/TVCombatTags.h"
#include "ScalableFloat.h"

UTVGameplayEffect::UTVGameplayEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
}

void UTVGameplayEffect::AddSetByCallerModifier(const FGameplayAttribute& Attribute, EGameplayModOp::Type Op, FName DataName)
{
	FSetByCallerFloat SetByCaller;
	SetByCaller.DataName = DataName;

	FGameplayModifierInfo Info;
	Info.Attribute = Attribute;
	Info.ModifierOp = Op;
	Info.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
	Modifiers.Add(Info);
}

void UTVGameplayEffect::MakeDurationEffect(float InDefaultDuration)
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	FSetByCallerFloat SetByCaller;
	SetByCaller.DataName = TVSetByCaller::Duration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
	bUsesDuration = true;
	DefaultDuration = InDefaultDuration;
}

void UTVGameplayEffect::MakeInfiniteEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	bUsesDuration = false;
}

void UTVGameplayEffect::MakePeriodic(float PeriodSeconds)
{
	Period = FScalableFloat(PeriodSeconds);
	bExecutePeriodicEffectOnApplication = false;
	TickPeriod = PeriodSeconds;
}

void UTVGameplayEffect::MakeStacking(int32 Limit)
{
	// StackingType: deprecated for direct access since 5.7 (setter is editor-only); UGameplayEffect's own constructor
	// writes it the same way.
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	StackLimitCount = Limit;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	StackPeriodResetPolicy = EGameplayEffectStackingPeriodPolicy::NeverReset;
	StackExpirationPolicy = EGameplayEffectStackingExpirationPolicy::ClearEntireStack;
}

// ---- Internal -----------------------------------------------------------------------------------------------------

UGE_TV_Damage::UGE_TV_Damage(const FObjectInitializer& OI) : Super(OI)
{
	AddSetByCallerModifier(UTVAttributeSet_Vitals::GetIncomingDamageAttribute(), EGameplayModOp::AddBase, TVSetByCaller::Damage);
}

UGE_TV_Heal::UGE_TV_Heal(const FObjectInitializer& OI) : Super(OI)
{
	AddSetByCallerModifier(UTVAttributeSet_Vitals::GetIncomingHealAttribute(), EGameplayModOp::AddBase, TVSetByCaller::Heal);
}

UGE_TV_ResourceDelta::UGE_TV_ResourceDelta(const FObjectInitializer& OI) : Super(OI)
{
	AddSetByCallerModifier(UTVAttributeSet_Vitals::GetResourceAttribute(), EGameplayModOp::AddBase, TVSetByCaller::Magnitude);
}

UGE_TV_Cooldown::UGE_TV_Cooldown(const FObjectInitializer& OI) : Super(OI)
{
	MakeDurationEffect(1.5f);
}

UGE_TV_Marker::UGE_TV_Marker(const FObjectInitializer& OI) : Super(OI)
{
	MakeDurationEffect(10.f);
}

UGE_TV_EquipmentStats::UGE_TV_EquipmentStats(const FObjectInitializer& OI) : Super(OI)
{
	MakeInfiniteEffect();
	AddSetByCallerModifier(UTVAttributeSet_Stats::GetStaminaAttribute(), EGameplayModOp::AddBase, TEXT("Stat.Stamina"));
	AddSetByCallerModifier(UTVAttributeSet_Stats::GetStrengthAttribute(), EGameplayModOp::AddBase, TEXT("Stat.Strength"));
	AddSetByCallerModifier(UTVAttributeSet_Stats::GetAgilityAttribute(), EGameplayModOp::AddBase, TEXT("Stat.Agility"));
	AddSetByCallerModifier(UTVAttributeSet_Stats::GetIntellectAttribute(), EGameplayModOp::AddBase, TEXT("Stat.Intellect"));
	AddSetByCallerModifier(UTVAttributeSet_Stats::GetSpiritAttribute(), EGameplayModOp::AddBase, TEXT("Stat.Spirit"));
	AddSetByCallerModifier(UTVAttributeSet_Stats::GetCritAttribute(), EGameplayModOp::AddBase, TEXT("Stat.Crit"));
	AddSetByCallerModifier(UTVAttributeSet_Stats::GetHasteAttribute(), EGameplayModOp::AddBase, TEXT("Stat.Haste"));
	AddSetByCallerModifier(UTVAttributeSet_Stats::GetArmorAttribute(), EGameplayModOp::AddBase, TEXT("Stat.Armor"));
}

UGE_TV_Bonus_MoveSpeed::UGE_TV_Bonus_MoveSpeed(const FObjectInitializer& OI) : Super(OI)
{
	MakeInfiniteEffect();
	AddSetByCallerModifier(UTVAttributeSet_Stats::GetMoveSpeedMultiplierAttribute(), EGameplayModOp::AddBase, TVSetByCaller::Magnitude);
}

UGE_TV_Interrupt::UGE_TV_Interrupt(const FObjectInitializer& OI) : Super(OI)
{
	Hook = ETVEffectHook::Interrupt;
}

// ---- Applied ------------------------------------------------------------------------------------------------------

UGE_TV_Taunt::UGE_TV_Taunt(const FObjectInitializer& OI) : Super(OI)
{
	MakeDurationEffect(3.f);
	Hook = ETVEffectHook::Taunt;
	GrantedTagNames.Add(TEXT("TV.Debuff.Taunted"));
	bIsDebuff = true;
}

UGE_TV_DamageTakenMod::UGE_TV_DamageTakenMod(const FObjectInitializer& OI) : Super(OI)
{
	MakeDurationEffect(6.f);
	AddSetByCallerModifier(UTVAttributeSet_Combat::GetDamageTakenMultiplierAttribute(), EGameplayModOp::AddBase, TVSetByCaller::Magnitude);
	GrantedTagNames.Add(TEXT("TV.Buff.DamageReduction"));
}

UGE_TV_PartyDamageTakenMod::UGE_TV_PartyDamageTakenMod(const FObjectInitializer& OI) : Super(OI)
{
	MakeDurationEffect(6.f);
	AddSetByCallerModifier(UTVAttributeSet_Combat::GetDamageTakenMultiplierAttribute(), EGameplayModOp::AddBase, TVSetByCaller::Magnitude);
	GrantedTagNames.Add(TEXT("TV.Buff.DamageReduction"));
}

UGE_TV_FrontalDamageTakenMod::UGE_TV_FrontalDamageTakenMod(const FObjectInitializer& OI) : Super(OI)
{
	MakeDurationEffect(5.f);
	AddSetByCallerModifier(UTVAttributeSet_Combat::GetFrontalDamageTakenModAttribute(), EGameplayModOp::AddBase, TVSetByCaller::Magnitude);
	GrantedTagNames.Add(TEXT("TV.Buff.FrontalBlock"));
}

UGE_TV_MaxHealthMod::UGE_TV_MaxHealthMod(const FObjectInitializer& OI) : Super(OI)
{
	MakeDurationEffect(12.f);
	MagnitudeMode = ETVMagnitudeMode::OnePlus;
	AddSetByCallerModifier(UTVAttributeSet_Vitals::GetMaxHealthAttribute(), EGameplayModOp::MultiplyAdditive, TVSetByCaller::Magnitude);
}

UGE_TV_ThreatMod::UGE_TV_ThreatMod(const FObjectInitializer& OI) : Super(OI)
{
	MakeDurationEffect(12.f);
	// Data gives the multiplier itself (2.0), which is exactly the MultiplyAdditive value.
	AddSetByCallerModifier(UTVAttributeSet_Combat::GetThreatMultiplierAttribute(), EGameplayModOp::MultiplyAdditive, TVSetByCaller::Magnitude);
}

UGE_TV_Root::UGE_TV_Root(const FObjectInitializer& OI) : Super(OI)
{
	MakeDurationEffect(4.f);
	Hook = ETVEffectHook::BreakOnDamage;
	GrantedTagNames.Add(TEXT("TV.Debuff.Root"));
	GrantedTagNames.Add(TEXT("TV.Debuff.Magic"));
	bIsDebuff = true;
}

UGE_TV_Stun::UGE_TV_Stun(const FObjectInitializer& OI) : Super(OI)
{
	MakeDurationEffect(1.f);
	Hook = ETVEffectHook::None; // stuns do not break on damage
	GrantedTagNames.Add(TEXT("TV.Debuff.Stun"));
	GrantedTagNames.Add(TEXT("TV.State.Stunned"));
	bIsDebuff = true;
}

UGE_TV_Absorb::UGE_TV_Absorb(const FObjectInitializer& OI) : Super(OI)
{
	MakeDurationEffect(10.f);
	Hook = ETVEffectHook::AbsorbShield;
	GrantedTagNames.Add(TEXT("TV.Buff.Absorb"));
}

UGE_TV_Dispel::UGE_TV_Dispel(const FObjectInitializer& OI) : Super(OI)
{
	Hook = ETVEffectHook::Dispel;
}

UGE_TV_HoT::UGE_TV_HoT(const FObjectInitializer& OI) : Super(OI)
{
	MakeDurationEffect(6.f);
	MakePeriodic(1.f);
	MagnitudeMode = ETVMagnitudeMode::AbilityMagnitudePerTick;
	AddSetByCallerModifier(UTVAttributeSet_Vitals::GetIncomingHealAttribute(), EGameplayModOp::AddBase, TVSetByCaller::Magnitude);
	GrantedTagNames.Add(TEXT("TV.Buff.HoT"));
}

UGE_TV_FocusGain::UGE_TV_FocusGain(const FObjectInitializer& OI) : Super(OI)
{
	MakeDurationEffect(3.f);
	MakePeriodic(1.f);
	MagnitudeMode = ETVMagnitudeMode::TotalOverDuration;
	bAppliesToSource = true;
	AddSetByCallerModifier(UTVAttributeSet_Vitals::GetResourceAttribute(), EGameplayModOp::AddBase, TVSetByCaller::Magnitude);
}

UGE_TV_DodgeMod::UGE_TV_DodgeMod(const FObjectInitializer& OI) : Super(OI)
{
	MakeDurationEffect(3.f);
	AddSetByCallerModifier(UTVAttributeSet_Combat::GetDodgeChanceAttribute(), EGameplayModOp::AddBase, TVSetByCaller::Magnitude);
}

UGE_TV_StagVolleyBuff::UGE_TV_StagVolleyBuff(const FObjectInitializer& OI) : Super(OI)
{
	MakeDurationEffect(6.f);
	GrantedTagNames.Add(TEXT("TV.Buff.EmpowerBuilder"));
}

UGE_TV_SocialAggro::UGE_TV_SocialAggro(const FObjectInitializer& OI) : Super(OI)
{
	Hook = ETVEffectHook::SocialAggro;
}

UGE_TV_ManaDrain::UGE_TV_ManaDrain(const FObjectInitializer& OI) : Super(OI)
{
	MagnitudeMode = ETVMagnitudeMode::DrainFractionOfMaxResource;
	AddSetByCallerModifier(UTVAttributeSet_Vitals::GetResourceAttribute(), EGameplayModOp::AddBase, TVSetByCaller::Magnitude);
}

UGE_TV_DotBleed::UGE_TV_DotBleed(const FObjectInitializer& OI) : Super(OI)
{
	MakeDurationEffect(8.f);
	MakePeriodic(2.f);
	MagnitudeMode = ETVMagnitudeMode::TimesHitDamage;
	AddSetByCallerModifier(UTVAttributeSet_Vitals::GetIncomingDamageAttribute(), EGameplayModOp::AddBase, TVSetByCaller::Magnitude);
	GrantedTagNames.Add(TEXT("TV.Debuff.DoT"));
	GrantedTagNames.Add(TEXT("TV.Debuff.Bleed"));
	bIsDebuff = true;
}

UGE_TV_DotMagic::UGE_TV_DotMagic(const FObjectInitializer& OI) : Super(OI)
{
	MakeDurationEffect(12.f);
	MakePeriodic(2.f);
	MagnitudeMode = ETVMagnitudeMode::TimesHitDamage;
	AddSetByCallerModifier(UTVAttributeSet_Vitals::GetIncomingDamageAttribute(), EGameplayModOp::AddBase, TVSetByCaller::Magnitude);
	GrantedTagNames.Add(TEXT("TV.Debuff.DoT"));
	GrantedTagNames.Add(TEXT("TV.Debuff.Magic"));
	bIsDebuff = true;
}

UGE_TV_Knockback::UGE_TV_Knockback(const FObjectInitializer& OI) : Super(OI)
{
	Hook = ETVEffectHook::Knockback;
}

UGE_TV_ArmorShred::UGE_TV_ArmorShred(const FObjectInitializer& OI) : Super(OI)
{
	MakeDurationEffect(10.f);
	MakeStacking(5);
	AddSetByCallerModifier(UTVAttributeSet_Combat::GetArmorPctModifierAttribute(), EGameplayModOp::AddBase, TVSetByCaller::Magnitude);
	GrantedTagNames.Add(TEXT("TV.Debuff.ArmorShred"));
	bIsDebuff = true;
}

UGE_TV_SummonAdds::UGE_TV_SummonAdds(const FObjectInitializer& OI) : Super(OI)
{
	Hook = ETVEffectHook::SummonAdds;
}

UGE_TV_HealingReceivedMod::UGE_TV_HealingReceivedMod(const FObjectInitializer& OI) : Super(OI)
{
	MakeDurationEffect(12.f);
	AddSetByCallerModifier(UTVAttributeSet_Combat::GetHealingReceivedMultiplierAttribute(), EGameplayModOp::AddBase, TVSetByCaller::Magnitude);
	GrantedTagNames.Add(TEXT("TV.Debuff.HealAbsorb"));
	bIsDebuff = true;
}

UGE_TV_Enrage::UGE_TV_Enrage(const FObjectInitializer& OI) : Super(OI)
{
	MakeDurationEffect(-1.f);
	AddSetByCallerModifier(UTVAttributeSet_Combat::GetDamageDoneMultiplierAttribute(), EGameplayModOp::AddBase, TVSetByCaller::Magnitude);
	GrantedTagNames.Add(TEXT("TV.Buff.Enrage"));
}

// ---- Parameters ---------------------------------------------------------------------------------------------------

UGE_TV_ChannelTick::UGE_TV_ChannelTick(const FObjectInitializer& OI) : Super(OI) { Role = ETVEffectRole::Parameter; }
UGE_TV_MultiHit::UGE_TV_MultiHit(const FObjectInitializer& OI) : Super(OI) { Role = ETVEffectRole::Parameter; }
UGE_TV_TrapZone::UGE_TV_TrapZone(const FObjectInitializer& OI) : Super(OI) { Role = ETVEffectRole::Parameter; }
UGE_TV_StackLimit::UGE_TV_StackLimit(const FObjectInitializer& OI) : Super(OI) { Role = ETVEffectRole::Parameter; }
UGE_TV_GroundZoneDot::UGE_TV_GroundZoneDot(const FObjectInitializer& OI) : Super(OI) { Role = ETVEffectRole::Parameter; }

// ---- Set bonuses --------------------------------------------------------------------------------------------------

UGE_TV_Set_MoveSpeedOutOfCombat::UGE_TV_Set_MoveSpeedOutOfCombat(const FObjectInitializer& OI) : Super(OI)
{
	MakeInfiniteEffect();
	AddSetByCallerModifier(UTVAttributeSet_Combat::GetOutOfCombatMoveSpeedBonusAttribute(), EGameplayModOp::AddBase, TVSetByCaller::Magnitude);
}

UGE_TV_Set_PrimaryStatPct::UGE_TV_Set_PrimaryStatPct(const FObjectInitializer& OI) : Super(OI)
{
	MakeInfiniteEffect();
	MagnitudeMode = ETVMagnitudeMode::OnePlus;
	AddSetByCallerModifier(UTVAttributeSet_Stats::GetStaminaAttribute(), EGameplayModOp::MultiplyAdditive, TVSetByCaller::Magnitude);
	AddSetByCallerModifier(UTVAttributeSet_Stats::GetStrengthAttribute(), EGameplayModOp::MultiplyAdditive, TVSetByCaller::Magnitude);
	AddSetByCallerModifier(UTVAttributeSet_Stats::GetAgilityAttribute(), EGameplayModOp::MultiplyAdditive, TVSetByCaller::Magnitude);
	AddSetByCallerModifier(UTVAttributeSet_Stats::GetIntellectAttribute(), EGameplayModOp::MultiplyAdditive, TVSetByCaller::Magnitude);
	AddSetByCallerModifier(UTVAttributeSet_Stats::GetSpiritAttribute(), EGameplayModOp::MultiplyAdditive, TVSetByCaller::Magnitude);
}

UGE_TV_Set_MaxHealthPct::UGE_TV_Set_MaxHealthPct(const FObjectInitializer& OI) : Super(OI)
{
	MakeInfiniteEffect();
	MagnitudeMode = ETVMagnitudeMode::OnePlus;
	AddSetByCallerModifier(UTVAttributeSet_Vitals::GetMaxHealthAttribute(), EGameplayModOp::MultiplyAdditive, TVSetByCaller::Magnitude);
}

UGE_TV_Set_DefensiveCooldownReduction::UGE_TV_Set_DefensiveCooldownReduction(const FObjectInitializer& OI) : Super(OI)
{
	MakeInfiniteEffect();
	MagnitudeMode = ETVMagnitudeMode::Negate;
	AddSetByCallerModifier(UTVAttributeSet_Combat::GetDefensiveCooldownMultiplierAttribute(), EGameplayModOp::AddBase, TVSetByCaller::Magnitude);
}

UGE_TV_Set_CritChance::UGE_TV_Set_CritChance(const FObjectInitializer& OI) : Super(OI)
{
	MakeInfiniteEffect();
	AddSetByCallerModifier(UTVAttributeSet_Combat::GetCritChanceBonusAttribute(), EGameplayModOp::AddBase, TVSetByCaller::Magnitude);
}

UGE_TV_Set_UltimatePower::UGE_TV_Set_UltimatePower(const FObjectInitializer& OI) : Super(OI)
{
	MakeInfiniteEffect();
	AddSetByCallerModifier(UTVAttributeSet_Combat::GetUltimatePowerMultiplierAttribute(), EGameplayModOp::AddBase, TVSetByCaller::Magnitude);
	GrantedTagNames.Add(TEXT("TV.Buff.UltimateGlow"));
}
