// Generic ability executors referenced by DT_Abilities.AbilityClass (/Script/ThistlewickVale.GA_TV_<Name>).
// Player versions are LocalPredicted; each has a ServerOnly "_AI" twin that UTVAbilitySystemComponent grants to mobs.
//
// How the 41 rows of DT_Abilities map (BALANCE.md section 8 effect classes are read as parameters where noted):
//  GA_TV_DirectDamage  Cleave Strike, Shield Slam, Ember Bolt, Crown Flare (combo x(1+stacks)), Radiant Smite, Quick Shot
//                      (+GE_TV_FocusGain; Stag Volley: free + hits twice), Twin Fang (GE_TV_MultiHit = 2 hits), Bog Bolt,
//                      Wisp siphon (+GE_TV_ManaDrain), Rend (+GE_TV_DotBleed), Boulder Toss (+GE_TV_Knockback), Moss Slam,
//                      Sundering Blows (+GE_TV_ArmorShred), Grave Lament (PartyAoE: every hostile in RadiusCm), Hollow Cleave(+Fury).
//                      A RadiusCm > 0 on an Enemy row also hits hostiles within RadiusCm in front of the caster (frontal cone).
//  GA_TV_Heal          Sanctuary Pulse (PartyAoE, combo), Mending Light, Mossy Mending / Shard Surge (mob: fraction of max health).
//  GA_TV_ApplyEffect   Iron Wall, Bulwark, Frost Snare, Glimmer Ward, Purify (GE_TV_Dispel + GE_TV_HoT), Dawn's Vigil (PartyAoE),
//                      Stag Volley, Call for Help (GE_TV_SocialAggro), Shield Block, Warchief Call / Shard Pillars
//                      (GE_TV_SummonAdds), Withering Mark, Grave Brand, Boss Enrage.
//  GA_TV_GroundAoE     Snare Trap (GE_TV_TrapZone -> trap), Crownfire (GE_TV_GroundZoneDot -> fire pool).
//  GA_TV_Channel       Starfall (GE_TV_ChannelTick = tick interval).
//  GA_TV_Taunt         Taunting Roar (target + hostiles within RadiusCm get GE_TV_Taunt).
//  GA_TV_Leap          Fade Step (Self: backwards RangeCm + effects on self), Boar Charge (Enemy: rush, hit, effects on target).
//  GA_TV_SummonMount   ABL_MOUNT_SUMMON (toggle).
//  GA_TV_Cone          Optional explicit frontal cone (RangeCm long, 60 degree half angle) for future data.
#pragma once

#include "CoreMinimal.h"
#include "Combat/Abilities/TVGameplayAbility.h"
#include "TVAbilityExecutors.generated.h"

UCLASS() class THISTLEWICKVALE_API UGA_TV_DirectDamage : public UTVGameplayAbility
{
	GENERATED_BODY()
public:
	virtual TSubclassOf<UTVGameplayAbility> GetAIVariantClass() const override;
protected:
	virtual void ExecuteAbility() override;
	void GatherTargets(TArray<AActor*>& OutTargets) const;
};

UCLASS() class THISTLEWICKVALE_API UGA_TV_Heal : public UTVGameplayAbility
{
	GENERATED_BODY()
public:
	virtual TSubclassOf<UTVGameplayAbility> GetAIVariantClass() const override;
protected:
	virtual void ExecuteAbility() override;
};

UCLASS() class THISTLEWICKVALE_API UGA_TV_ApplyEffect : public UTVGameplayAbility
{
	GENERATED_BODY()
public:
	virtual TSubclassOf<UTVGameplayAbility> GetAIVariantClass() const override;
protected:
	virtual void ExecuteAbility() override;
};

UCLASS() class THISTLEWICKVALE_API UGA_TV_GroundAoE : public UTVGameplayAbility
{
	GENERATED_BODY()
public:
	virtual TSubclassOf<UTVGameplayAbility> GetAIVariantClass() const override;
protected:
	virtual void ExecuteAbility() override;
	virtual bool NeedsUnitTarget() const override { return false; }
};

UCLASS() class THISTLEWICKVALE_API UGA_TV_Channel : public UTVGameplayAbility
{
	GENERATED_BODY()
public:
	virtual TSubclassOf<UTVGameplayAbility> GetAIVariantClass() const override;
protected:
	virtual void ExecuteAbility() override;
	virtual bool IsLatent() const override { return true; }
	virtual bool NeedsUnitTarget() const override { return false; }
	void ChannelTick();
	void ChannelFinished();
	FTimerHandle TickTimerHandle;
	FTimerHandle EndTimerHandle;
	int32 TicksDone = 0;
	int32 TicksTotal = 0;
};

UCLASS() class THISTLEWICKVALE_API UGA_TV_Taunt : public UTVGameplayAbility
{
	GENERATED_BODY()
public:
	virtual TSubclassOf<UTVGameplayAbility> GetAIVariantClass() const override;
protected:
	virtual void ExecuteAbility() override;
};

UCLASS() class THISTLEWICKVALE_API UGA_TV_Leap : public UTVGameplayAbility
{
	GENERATED_BODY()
public:
	virtual TSubclassOf<UTVGameplayAbility> GetAIVariantClass() const override;
protected:
	virtual void ExecuteAbility() override;
	virtual bool IsLatent() const override { return true; }
	void OnLeapLanded();
	FTimerHandle LandTimerHandle;
	/** Seconds after launch when a charge's hit is applied. */
	UPROPERTY(EditDefaultsOnly, Category = "TV|Ability") float ChargeImpactDelay = 0.4f;
};

UCLASS() class THISTLEWICKVALE_API UGA_TV_SummonMount : public UTVGameplayAbility
{
	GENERATED_BODY()
public:
	UGA_TV_SummonMount();
	/** Mounted: dismounts at once (toggle). Otherwise the normal 1.5 s cast, broken by damage or moving. */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
protected:
	virtual void ExecuteAbility() override;
	virtual bool NeedsUnitTarget() const override { return false; }
};

UCLASS() class THISTLEWICKVALE_API UGA_TV_Cone : public UTVGameplayAbility
{
	GENERATED_BODY()
public:
	virtual TSubclassOf<UTVGameplayAbility> GetAIVariantClass() const override;
protected:
	virtual void ExecuteAbility() override;
	UPROPERTY(EditDefaultsOnly, Category = "TV|Ability") float ConeHalfAngleDeg = 60.f;
};

// ---- Server-only twins for mobs ------------------------------------------------------------------------------------
UCLASS() class THISTLEWICKVALE_API UGA_TV_DirectDamage_AI : public UGA_TV_DirectDamage { GENERATED_BODY() public: UGA_TV_DirectDamage_AI(); };
UCLASS() class THISTLEWICKVALE_API UGA_TV_Heal_AI : public UGA_TV_Heal { GENERATED_BODY() public: UGA_TV_Heal_AI(); };
UCLASS() class THISTLEWICKVALE_API UGA_TV_ApplyEffect_AI : public UGA_TV_ApplyEffect { GENERATED_BODY() public: UGA_TV_ApplyEffect_AI(); };
UCLASS() class THISTLEWICKVALE_API UGA_TV_GroundAoE_AI : public UGA_TV_GroundAoE { GENERATED_BODY() public: UGA_TV_GroundAoE_AI(); };
UCLASS() class THISTLEWICKVALE_API UGA_TV_Channel_AI : public UGA_TV_Channel { GENERATED_BODY() public: UGA_TV_Channel_AI(); };
UCLASS() class THISTLEWICKVALE_API UGA_TV_Taunt_AI : public UGA_TV_Taunt { GENERATED_BODY() public: UGA_TV_Taunt_AI(); };
UCLASS() class THISTLEWICKVALE_API UGA_TV_Leap_AI : public UGA_TV_Leap { GENERATED_BODY() public: UGA_TV_Leap_AI(); };
UCLASS() class THISTLEWICKVALE_API UGA_TV_Cone_AI : public UGA_TV_Cone { GENERATED_BODY() public: UGA_TV_Cone_AI(); };
