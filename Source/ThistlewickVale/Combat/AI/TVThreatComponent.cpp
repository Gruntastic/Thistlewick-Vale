#include "Combat/AI/TVThreatComponent.h"
#include "Combat/TVCharacterBase.h"
#include "Combat/TVPlayerState.h"
#include "Combat/TVCombatLibrary.h"
#include "EngineUtils.h"
#include "Engine/World.h"

UTVThreatComponent::UTVThreatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTVThreatComponent::AddThreat(AActor* Source, float Amount)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !Source || !UTVCombatLibrary::IsAlive(Source) || !UTVCombatLibrary::IsHostile(Owner, Source))
	{
		return;
	}
	FTVThreatEntry* Entry = Entries.FindByPredicate([Source](const FTVThreatEntry& E) { return E.Actor.Get() == Source; });
	if (!Entry)
	{
		Entry = &Entries.AddDefaulted_GetRef();
		Entry->Actor = Source;
	}
	Entry->Threat += FMath::Max(0.f, Amount);

	if (const APawn* Pawn = Cast<APawn>(Source))
	{
		if (ATVPlayerState* PS = Pawn->GetPlayerState<ATVPlayerState>())
		{
			TaggedPlayers.Add(PS);
		}
	}
}

void UTVThreatComponent::Taunt(AActor* Source, float Multiplier, float Duration)
{
	AddThreat(Source, 0.f);
	FTVThreatEntry* Entry = Entries.FindByPredicate([Source](const FTVThreatEntry& E) { return E.Actor.Get() == Source; });
	if (!Entry)
	{
		return;
	}
	float Top = 0.f;
	for (const FTVThreatEntry& Other : Entries)
	{
		if (Other.Actor.Get() != Source)
		{
			Top = FMath::Max(Top, Other.Threat);
		}
	}
	// Jump to the top x Multiplier (GDD: top + 10 %).
	Entry->Threat = FMath::Max(Entry->Threat, Top * FMath::Max(1.f, Multiplier));
	ForcedTarget = Source;
	const UWorld* World = GetWorld();
	ForcedUntil = (World ? World->GetTimeSeconds() : 0.0) + FMath::Max(0.f, Duration);
}

void UTVThreatComponent::RemoveActor(AActor* Actor)
{
	Entries.RemoveAll([Actor](const FTVThreatEntry& E) { return E.Actor.Get() == Actor; });
	if (ForcedTarget.Get() == Actor)
	{
		ForcedTarget.Reset();
	}
}

void UTVThreatComponent::ClearThreat()
{
	Entries.Reset();
	TaggedPlayers.Reset();
	ForcedTarget.Reset();
	ForcedUntil = 0.0;
}

float UTVThreatComponent::GetThreat(const AActor* Actor) const
{
	const FTVThreatEntry* Entry = Entries.FindByPredicate([Actor](const FTVThreatEntry& E) { return E.Actor.Get() == Actor; });
	return Entry ? Entry->Threat : 0.f;
}

bool UTVThreatComponent::HasEntry(const AActor* Actor) const
{
	return Actor && Entries.ContainsByPredicate([Actor](const FTVThreatEntry& E) { return E.Actor.Get() == Actor; });
}

void UTVThreatComponent::PruneDead()
{
	Entries.RemoveAll([](const FTVThreatEntry& E) { return !UTVCombatLibrary::IsAlive(E.Actor.Get()); });
}

bool UTVThreatComponent::HasAnyLiving() const
{
	for (const FTVThreatEntry& Entry : Entries)
	{
		if (UTVCombatLibrary::IsAlive(Entry.Actor.Get()))
		{
			return true;
		}
	}
	return false;
}

AActor* UTVThreatComponent::GetTopThreatActor() const
{
	AActor* Best = nullptr;
	float BestThreat = -1.f;
	for (const FTVThreatEntry& Entry : Entries)
	{
		AActor* Actor = Entry.Actor.Get();
		if (UTVCombatLibrary::IsAlive(Actor) && Entry.Threat > BestThreat)
		{
			Best = Actor;
			BestThreat = Entry.Threat;
		}
	}
	return Best;
}

AActor* UTVThreatComponent::SelectTarget(AActor* CurrentTarget, float MeleeRangeCm)
{
	PruneDead();
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	if (AActor* Forced = ForcedTarget.Get())
	{
		if (Now < ForcedUntil && UTVCombatLibrary::IsAlive(Forced) && HasEntry(Forced))
		{
			return Forced;
		}
		ForcedTarget.Reset();
	}

	if (!CurrentTarget || !HasEntry(CurrentTarget))
	{
		return GetTopThreatActor();
	}

	// Switch only when someone beats the current target's threat by 110 % (in melee range) or 130 % (ranged).
	const float CurrentThreat = GetThreat(CurrentTarget);
	const AActor* Owner = GetOwner();
	AActor* Best = CurrentTarget;
	float BestThreat = CurrentThreat;
	for (const FTVThreatEntry& Entry : Entries)
	{
		AActor* Actor = Entry.Actor.Get();
		if (!Actor || Actor == CurrentTarget)
		{
			continue;
		}
		const bool bInMelee = Owner && FVector::Dist(Owner->GetActorLocation(), Actor->GetActorLocation()) <= MeleeRangeCm;
		const float Required = CurrentThreat * (bInMelee ? 1.1f : 1.3f);
		if (Entry.Threat > Required && Entry.Threat > BestThreat)
		{
			Best = Actor;
			BestThreat = Entry.Threat;
		}
	}
	return Best;
}

void UTVThreatComponent::GetThreatActors(TArray<AActor*>& OutActors) const
{
	for (const FTVThreatEntry& Entry : Entries)
	{
		if (AActor* Actor = Entry.Actor.Get())
		{
			OutActors.Add(Actor);
		}
	}
}

void UTVThreatComponent::GetTaggedPlayers(TArray<ATVPlayerState*>& OutPlayers) const
{
	for (const TWeakObjectPtr<ATVPlayerState>& Player : TaggedPlayers)
	{
		if (ATVPlayerState* PS = Player.Get())
		{
			OutPlayers.AddUnique(PS);
		}
	}
}

int32 UTVThreatComponent::NumLivingPlayers() const
{
	int32 Count = 0;
	for (const FTVThreatEntry& Entry : Entries)
	{
		if (UTVCombatLibrary::IsAlive(Entry.Actor.Get()) && UTVCombatLibrary::IsPlayerSide(Entry.Actor.Get()))
		{
			++Count;
		}
	}
	return Count;
}

AActor* UTVThreatComponent::GetRandomTarget(const AActor* Exclude) const
{
	TArray<AActor*> Living;
	for (const FTVThreatEntry& Entry : Entries)
	{
		AActor* Actor = Entry.Actor.Get();
		if (UTVCombatLibrary::IsAlive(Actor) && Actor != Exclude)
		{
			Living.Add(Actor);
		}
	}
	if (Living.Num() == 0)
	{
		return Exclude && UTVCombatLibrary::IsAlive(Exclude) ? const_cast<AActor*>(Exclude) : nullptr;
	}
	return Living[FMath::RandRange(0, Living.Num() - 1)];
}

void UTVThreatComponent::FindMobsEngagedWith(const AActor* Actor, TArray<ATVMobCharacter*>& OutMobs)
{
	UWorld* World = Actor ? Actor->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}
	for (TActorIterator<ATVMobCharacter> It(World); It; ++It)
	{
		ATVMobCharacter* Mob = *It;
		if (UTVCombatLibrary::IsAlive(Mob) && Mob->GetThreatComponent() && Mob->GetThreatComponent()->HasEntry(Actor))
		{
			OutMobs.Add(Mob);
		}
	}
}
