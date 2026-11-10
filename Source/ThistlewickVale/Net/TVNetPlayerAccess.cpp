#include "Net/TVNetPlayerAccess.h"

#include "AbilitySystemComponent.h"
#include "GameFramework/Pawn.h"
#include "Combat/TVPlayerState.h"
#include "Combat/TVCharacterBase.h"
#include "Combat/TVAbilitySystemComponent.h"
#include "Combat/TVAttributeSets.h"
#include "Systems/TVMountComponent.h"

// INTERFACE-REQUEST: Docs/INTERFACES.md lists ClassId, CharacterName and Level as replicated members of ATVPlayerState
// but not their access level. This file assumes they are public UPROPERTYs. If they are private, please add
// GetClassId(), GetCharacterName(), GetCharacterLevel() and SetCharacterName(const FString&) and switch the bodies below.

namespace TVNet
{
	FName GetClassId(const ATVPlayerState* PS)
	{
		return PS ? PS->ClassId : NAME_None;
	}

	FString GetCharacterName(const ATVPlayerState* PS)
	{
		if (!PS)
		{
			return FString();
		}
		return PS->CharacterName.IsEmpty() ? PS->GetPlayerName() : PS->CharacterName;
	}

	int32 GetCharacterLevel(const ATVPlayerState* PS)
	{
		return PS ? PS->Level : 0;
	}

	void SetCharacterName(ATVPlayerState* PS, const FString& Name)
	{
		if (!PS)
		{
			return;
		}
		PS->CharacterName = Name;
		PS->SetPlayerName(Name);
		PS->ForceNetUpdate();
	}

	UTVAbilitySystemComponent* GetTVAbilitySystem(const ATVPlayerState* PS)
	{
		// ATVPlayerState implements IAbilitySystemInterface (INTERFACES.md).
		return PS ? Cast<UTVAbilitySystemComponent>(PS->GetAbilitySystemComponent()) : nullptr;
	}

	UTVMountComponent* GetMount(const ATVPlayerState* PS)
	{
		return PS ? const_cast<ATVPlayerState*>(PS)->GetMount() : nullptr; // getter constness not specified in INTERFACES.md
	}

	TArray<FName> GetActionBar(const ATVPlayerState* PS)
	{
		// Lead: Combat adds `TArray<FName> ActionBar` (10 slots) to ATVPlayerState; assumed public.
		return PS ? PS->ActionBar : TArray<FName>();
	}

	void SetActionBar(ATVPlayerState* PS, const TArray<FName>& Bar)
	{
		if (PS)
		{
			PS->ActionBar = Bar;
			PS->ForceNetUpdate();
		}
	}

	static float GetRatio(const ATVPlayerState* PS, const FGameplayAttribute& Current, const FGameplayAttribute& Max)
	{
		const UAbilitySystemComponent* ASC = PS ? const_cast<ATVPlayerState*>(PS)->GetAbilitySystemComponent() : nullptr;
		if (!ASC || !ASC->HasAttributeSetForAttribute(Max))
		{
			return 1.f;
		}
		const float MaxValue = ASC->GetNumericAttribute(Max);
		return MaxValue > 0.f ? FMath::Clamp(ASC->GetNumericAttribute(Current) / MaxValue, 0.f, 1.f) : 0.f;
	}

	// VERIFY-5.8: assumes UTVAttributeSet_Vitals uses the standard ATTRIBUTE_ACCESSORS macro (static GetHealthAttribute() etc.).
	float GetHealthPercent(const ATVPlayerState* PS)
	{
		return GetRatio(PS, UTVAttributeSet_Vitals::GetHealthAttribute(), UTVAttributeSet_Vitals::GetMaxHealthAttribute());
	}

	float GetResourcePercent(const ATVPlayerState* PS)
	{
		return GetRatio(PS, UTVAttributeSet_Vitals::GetResourceAttribute(), UTVAttributeSet_Vitals::GetMaxResourceAttribute());
	}

	bool IsPawnDead(const APawn* Pawn)
	{
		const ATVCharacterBase* Character = Cast<ATVCharacterBase>(Pawn);
		return Character && Character->IsDead();
	}

	bool IsPlayerDead(const ATVPlayerState* PS)
	{
		if (!PS)
		{
			return true;
		}
		const APawn* Pawn = PS->GetPawn();
		if (!Pawn)
		{
			return true;
		}
		return IsPawnDead(Pawn);
	}

	void RestoreVitalsAfterRespawn(ATVPlayerState* PS)
	{
		// INTERFACE-REQUEST: Combat should own a proper ATVPlayerState::ReviveAtFullHealth() (clears death tags, resets
		// combo resources). Until then the Net module sets the base values directly.
		UAbilitySystemComponent* ASC = PS ? PS->GetAbilitySystemComponent() : nullptr;
		if (!ASC || !ASC->HasAttributeSetForAttribute(UTVAttributeSet_Vitals::GetMaxHealthAttribute()))
		{
			return;
		}
		const float MaxHealth = ASC->GetNumericAttribute(UTVAttributeSet_Vitals::GetMaxHealthAttribute());
		ASC->SetNumericAttributeBase(UTVAttributeSet_Vitals::GetHealthAttribute(), MaxHealth);

		// Rage starts at 0 (GDD 4); Mana and Focus come back full.
		const float MaxResource = ASC->GetNumericAttribute(UTVAttributeSet_Vitals::GetMaxResourceAttribute());
		const bool bIsRage = GetClassId(PS) == FName(TEXT("CLS_WARRIOR"));
		ASC->SetNumericAttributeBase(UTVAttributeSet_Vitals::GetResourceAttribute(), bIsRage ? 0.f : MaxResource);
	}
}
