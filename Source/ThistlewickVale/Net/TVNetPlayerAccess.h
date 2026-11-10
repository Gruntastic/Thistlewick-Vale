// Single place where the Net module touches ATVPlayerState / GAS internals owned by the Combat Engineer.
// If a member named in Docs/INTERFACES.md turns out to be private or renamed, only this file and its .cpp change.
#pragma once

#include "CoreMinimal.h"

class ATVPlayerState;
class UTVAbilitySystemComponent;
class UTVMountComponent;
class APawn;

namespace TVNet
{
	/** Class id (CLS_WARRIOR ...). */
	THISTLEWICKVALE_API FName GetClassId(const ATVPlayerState* PS);

	/** Character name; falls back to APlayerState::GetPlayerName(). */
	THISTLEWICKVALE_API FString GetCharacterName(const ATVPlayerState* PS);

	THISTLEWICKVALE_API int32 GetCharacterLevel(const ATVPlayerState* PS);

	/** Server: sets ATVPlayerState::CharacterName and the engine player name (used by chat and /invite lookups). */
	THISTLEWICKVALE_API void SetCharacterName(ATVPlayerState* PS, const FString& Name);

	THISTLEWICKVALE_API UTVAbilitySystemComponent* GetTVAbilitySystem(const ATVPlayerState* PS);

	THISTLEWICKVALE_API UTVMountComponent* GetMount(const ATVPlayerState* PS);

	/** ATVPlayerState::ActionBar (10 slots, owned and replicated by Combat). Empty array if PS is null. */
	THISTLEWICKVALE_API TArray<FName> GetActionBar(const ATVPlayerState* PS);


	/** 0..1, 1 if attributes are unavailable. */
	THISTLEWICKVALE_API float GetHealthPercent(const ATVPlayerState* PS);
	THISTLEWICKVALE_API float GetResourcePercent(const ATVPlayerState* PS);

	/** True when the pawn is missing, is a dead ATVCharacterBase, or health is 0. */
	THISTLEWICKVALE_API bool IsPlayerDead(const ATVPlayerState* PS);

	/** True when Pawn is an ATVCharacterBase that reports IsDead(). */
	THISTLEWICKVALE_API bool IsPawnDead(const APawn* Pawn);

	/** Server: sets Health = MaxHealth and Resource to a class-appropriate value after a release/respawn. */
	THISTLEWICKVALE_API void RestoreVitalsAfterRespawn(ATVPlayerState* PS);
}
