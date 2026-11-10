// Per-mob threat table (GDD section 5): damage 1 threat/point x multipliers, healing 0.5 split, taunt = top + 10 %,
// target switch at 110 % (melee range) / 130 % (ranged) of the current target's threat.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TVThreatComponent.generated.h"

class ATVPlayerState;

USTRUCT()
struct FTVThreatEntry
{
	GENERATED_BODY()

	UPROPERTY() TWeakObjectPtr<AActor> Actor;
	UPROPERTY() float Threat = 0.f;
};

UCLASS(ClassGroup = (TV), meta = (BlueprintSpawnableComponent))
class THISTLEWICKVALE_API UTVThreatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTVThreatComponent();

	/** Server. Adds (or creates) an entry. Amount may be 0 to just engage. */
	void AddThreat(AActor* Source, float Amount);
	/** Server. Source jumps to top threat x Multiplier and is the forced target for Duration seconds. */
	void Taunt(AActor* Source, float Multiplier, float Duration);
	void RemoveActor(AActor* Actor);
	void ClearThreat();

	float GetThreat(const AActor* Actor) const;
	bool HasEntry(const AActor* Actor) const;
	bool HasAnyLiving() const;
	AActor* GetTopThreatActor() const;
	/** Applies the 110 %/130 % switch rule from the current target and returns the target to attack. */
	AActor* SelectTarget(AActor* CurrentTarget, float MeleeRangeCm);
	void GetThreatActors(TArray<AActor*>& OutActors) const;
	/** Every player that ever had threat on this mob since the last reset (kill eligibility). */
	void GetTaggedPlayers(TArray<ATVPlayerState*>& OutPlayers) const;
	int32 NumLivingPlayers() const;
	/** Picks a random living hostile from the table, optionally not the current target. */
	AActor* GetRandomTarget(const AActor* Exclude) const;

	/** Every living mob in the world whose table contains Actor. */
	static void FindMobsEngagedWith(const AActor* Actor, TArray<class ATVMobCharacter*>& OutMobs);

protected:
	void PruneDead();

	UPROPERTY() TArray<FTVThreatEntry> Entries;
	TSet<TWeakObjectPtr<ATVPlayerState>> TaggedPlayers;
	TWeakObjectPtr<AActor> ForcedTarget;
	double ForcedUntil = 0.0;
};
