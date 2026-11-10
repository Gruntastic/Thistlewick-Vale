#include "Combat/TVCombatEventsSubsystem.h"
#include "Combat/TVCharacterBase.h"
#include "Combat/TVPlayerState.h"
#include "Combat/AI/TVThreatComponent.h"
#include "Net/TVPartySubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

UTVCombatEventsSubsystem* UTVCombatEventsSubsystem::Get(const UObject* WorldContext)
{
	UWorld* World = (WorldContext && GEngine) ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UTVCombatEventsSubsystem>() : nullptr;
}

void UTVCombatEventsSubsystem::BroadcastMobKilled(ATVMobCharacter* Mob)
{
	if (!Mob)
	{
		return;
	}
	TArray<ATVPlayerState*> Eligible;
	ComputeKillEligibility(Mob, Eligible);
	OnMobKilled.Broadcast(Mob, Eligible);
}

void UTVCombatEventsSubsystem::BroadcastPlayerDamaged(ATVPlayerState* Player, float Amount)
{
	if (Player && Amount > 0.f)
	{
		OnPlayerDamaged.Broadcast(Player, Amount);
	}
}

void UTVCombatEventsSubsystem::BroadcastPlayerDied(ATVPlayerState* Player, AActor* Killer)
{
	if (Player)
	{
		OnPlayerDied.Broadcast(Player, Killer);
	}
}

void UTVCombatEventsSubsystem::ComputeKillEligibility(ATVMobCharacter* Mob, TArray<ATVPlayerState*>& OutPlayers)
{
	OutPlayers.Reset();
	UTVThreatComponent* Threat = Mob ? Mob->GetThreatComponent() : nullptr;
	if (!Threat)
	{
		return;
	}
	TArray<ATVPlayerState*> Tagged;
	Threat->GetTaggedPlayers(Tagged);
	UTVPartySubsystem* Party = UTVPartySubsystem::Get(Mob);
	for (ATVPlayerState* Player : Tagged)
	{
		if (!Player)
		{
			continue;
		}
		OutPlayers.AddUnique(Player);
		if (Party)
		{
			for (ATVPlayerState* Member : Party->GetPartyMembers(Player))
			{
				if (Member)
				{
					OutPlayers.AddUnique(Member);
				}
			}
		}
	}
}
