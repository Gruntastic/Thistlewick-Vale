#include "Combat/TVCombatTags.h"
#include "ThistlewickVale.h"
#include "GameplayTagsManager.h"

namespace TVTags
{
	FGameplayTag Get(const TCHAR* TagName)
	{
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(TagName), /*ErrorIfNotFound*/ false);
		if (!Tag.IsValid())
		{
			static TSet<FString> Warned;
			if (!Warned.Contains(TagName))
			{
				Warned.Add(TagName);
				UE_LOG(LogTV, Warning, TEXT("Gameplay tag '%s' is not declared in Config/Tags/TVGameplayTags.ini"), TagName);
			}
		}
		return Tag;
	}

	FGameplayTag CooldownTagFor(const FGameplayTag& AbilityTag)
	{
		if (!AbilityTag.IsValid())
		{
			return FGameplayTag();
		}
		static TMap<FGameplayTag, FGameplayTag> Cache;
		if (const FGameplayTag* Found = Cache.Find(AbilityTag))
		{
			return *Found;
		}
		FString Name = AbilityTag.ToString();
		// TV.Ability.X.Y -> TV.Cooldown.Ability.X.Y
		if (Name.StartsWith(TEXT("TV.")))
		{
			Name = TEXT("TV.Cooldown.") + Name.RightChop(3);
		}
		const FGameplayTag Result = Get(*Name);
		Cache.Add(AbilityTag, Result);
		return Result;
	}

#define TV_DEFINE_TAG(FuncName, TagString) \
	FGameplayTag FuncName() { static FGameplayTag Cached; if (!Cached.IsValid()) { Cached = Get(TEXT(TagString)); } return Cached; }

	TV_DEFINE_TAG(State_Dead, "TV.State.Dead")
	TV_DEFINE_TAG(State_Mounted, "TV.State.Mounted")
	TV_DEFINE_TAG(State_Casting, "TV.State.Casting")
	TV_DEFINE_TAG(State_Channeling, "TV.State.Channeling")
	TV_DEFINE_TAG(State_Stunned, "TV.State.Stunned")
	TV_DEFINE_TAG(State_InCombat, "TV.State.InCombat")
	TV_DEFINE_TAG(State_Evading, "TV.State.Evading")
	TV_DEFINE_TAG(Cooldown_GCD, "TV.Cooldown.GCD")
	TV_DEFINE_TAG(Debuff, "TV.Debuff")
	TV_DEFINE_TAG(Debuff_Magic, "TV.Debuff.Magic")
	TV_DEFINE_TAG(Debuff_Curse, "TV.Debuff.Curse")
	TV_DEFINE_TAG(Debuff_Root, "TV.Debuff.Root")
	TV_DEFINE_TAG(Debuff_Stun, "TV.Debuff.Stun")
	TV_DEFINE_TAG(Buff_Absorb, "TV.Buff.Absorb")
	TV_DEFINE_TAG(Buff_FrontalBlock, "TV.Buff.FrontalBlock")
	TV_DEFINE_TAG(Buff_EmpowerBuilder, "TV.Buff.EmpowerBuilder")
	TV_DEFINE_TAG(SBC_Damage, "TV.SetByCaller.Damage")
	TV_DEFINE_TAG(SBC_Heal, "TV.SetByCaller.Heal")
	TV_DEFINE_TAG(SBC_Duration, "TV.SetByCaller.Duration")
	TV_DEFINE_TAG(SBC_Magnitude, "TV.SetByCaller.Magnitude")
	TV_DEFINE_TAG(SBC_Threat, "TV.SetByCaller.Threat")
	TV_DEFINE_TAG(SBC_BreakOnDamageAfter, "TV.SetByCaller.BreakOnDamageAfter")
	TV_DEFINE_TAG(Hit_Crit, "TV.Hit.Crit")
	TV_DEFINE_TAG(Hit_Periodic, "TV.Hit.Periodic")
	TV_DEFINE_TAG(Ability_Mount_Summon, "TV.Ability.Mount.Summon")

#undef TV_DEFINE_TAG
}
