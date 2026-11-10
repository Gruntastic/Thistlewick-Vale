#include "Combat/AI/TVBossDriverComponent.h"
#include "ThistlewickVale.h"
#include "Combat/AI/TVMobAIController.h"
#include "Combat/TVBossDataAsset.h"
#include "Combat/TVAbilitySystemComponent.h"
#include "Combat/TVAttributeSets.h"
#include "Combat/TVCharacterBase.h"
#include "Combat/TVCombatLibrary.h"
#include "Combat/TVCombatTags.h"
#include "EngineUtils.h"
#include "Engine/World.h"

UTVBossDriverComponent::UTVBossDriverComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTVBossDriverComponent::SetBossData(UTVBossDataAsset* InData)
{
	BossData = InData;
	ResetEncounter();
}

ATVMobCharacter* UTVBossDriverComponent::GetBoss() const
{
	return Cast<ATVMobCharacter>(GetOwner());
}

float UTVBossDriverComponent::GetHealthFraction() const
{
	const UTVAbilitySystemComponent* ASC = UTVCombatLibrary::GetTVASC(GetOwner());
	if (!ASC)
	{
		return 1.f;
	}
	const float Max = ASC->GetNumericAttribute(UTVAttributeSet_Vitals::GetMaxHealthAttribute());
	return Max > 0.f ? ASC->GetNumericAttribute(UTVAttributeSet_Vitals::GetHealthAttribute()) / Max : 1.f;
}

void UTVBossDriverComponent::StartEncounter()
{
	ATVMobCharacter* Boss = GetBoss();
	if (!BossData || !Boss || bActive)
	{
		return;
	}
	bActive = true;
	bEnraged = false;
	EncounterTime = 0.f;
	PhaseIndex = -1;
	FiredAddSpawns.Reset();
	PendingOnEnter.Reset();

	// Party size for capped summons (shard pillars): living players inside the arena at the pull.
	PartySizeAtPull = 0;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ATVPlayerCharacter> It(World); It; ++It)
		{
			if (UTVCombatLibrary::IsAlive(*It) && FVector::Dist(It->GetActorLocation(), Boss->GetActorLocation()) <= BossData->ArenaRadius)
			{
				++PartySizeAtPull;
			}
		}
	}
	PartySizeAtPull = FMath::Max(1, PartySizeAtPull);
	if (BossData->Phases.Num() > 0)
	{
		EnterPhase(0);
	}
}

void UTVBossDriverComponent::EnterPhase(int32 NewPhase)
{
	if (!BossData || !BossData->Phases.IsValidIndex(NewPhase))
	{
		return;
	}
	PhaseIndex = NewPhase;
	const FTVBossPhase& Phase = BossData->Phases[PhaseIndex];
	NextCastTimes.Reset();
	for (const FTVBossRotationEntry& Entry : Phase.AbilityRotation)
	{
		NextCastTimes.Add(EncounterTime + FMath::Max(0.f, Entry.InitialDelay));
	}
	PendingOnEnter = Phase.OnEnterAbilities;
	OnPhaseChanged.Broadcast(GetBoss(), PhaseIndex);
}

void UTVBossDriverComponent::TickBoss(float DeltaSeconds)
{
	ATVMobCharacter* Boss = GetBoss();
	ATVMobAIController* AI = Boss ? Cast<ATVMobAIController>(Boss->GetController()) : nullptr;
	if (!BossData || !Boss || !AI || Boss->IsDead())
	{
		return;
	}
	if (!bActive)
	{
		StartEncounter();
	}
	EncounterTime += DeltaSeconds;
	const float Fraction = GetHealthFraction();

	// Health-triggered add spawns (each once).
	for (int32 i = 0; i < BossData->AddSpawns.Num(); ++i)
	{
		const FTVBossAddSpawn& Spawn = BossData->AddSpawns[i];
		if (!FiredAddSpawns.Contains(i) && Fraction <= Spawn.HealthTrigger)
		{
			FiredAddSpawns.Add(i);
			UTVCombatLibrary::SummonAdds(Boss, Spawn.MobId, Spawn.Count);
		}
	}

	// Phases only move forward: the last phase whose threshold we are at or below.
	int32 TargetPhase = PhaseIndex;
	for (int32 i = FMath::Max(0, PhaseIndex + 1); i < BossData->Phases.Num(); ++i)
	{
		if (Fraction <= BossData->Phases[i].HealthThreshold)
		{
			TargetPhase = i;
		}
	}
	if (TargetPhase != PhaseIndex)
	{
		EnterPhase(TargetPhase);
	}

	UTVAbilitySystemComponent* ASC = Boss->GetTVAbilitySystemComponent();
	if (!ASC || ASC->IsCasting())
	{
		return;
	}

	// Enrage timer (0 = none).
	if (!bEnraged && BossData->EnrageTime > 0.f && EncounterTime >= BossData->EnrageTime)
	{
		if (BossData->EnrageAbilityId.IsNone() || AI->TryCastAbility(BossData->EnrageAbilityId, true))
		{
			bEnraged = true;
			return;
		}
	}

	// On-enter abilities, in order, one per think.
	if (PendingOnEnter.Num() > 0)
	{
		if (AI->TryCastAbility(PendingOnEnter[0], true))
		{
			PendingOnEnter.RemoveAt(0);
		}
		return;
	}

	// Rotation: the first due entry that starts; failed entries retry next think.
	if (!BossData->Phases.IsValidIndex(PhaseIndex))
	{
		return;
	}
	const FTVBossPhase& Phase = BossData->Phases[PhaseIndex];
	for (int32 i = 0; i < Phase.AbilityRotation.Num() && i < NextCastTimes.Num(); ++i)
	{
		if (EncounterTime < NextCastTimes[i])
		{
			continue;
		}
		if (AI->TryCastAbility(Phase.AbilityRotation[i].AbilityId, true))
		{
			NextCastTimes[i] = EncounterTime + FMath::Max(0.5f, Phase.AbilityRotation[i].Interval);
			return;
		}
	}
}

void UTVBossDriverComponent::ResetEncounter()
{
	bActive = false;
	bEnraged = false;
	PhaseIndex = -1;
	EncounterTime = 0.f;
	NextCastTimes.Reset();
	PendingOnEnter.Reset();
	FiredAddSpawns.Reset();

	// Adds belong to the encounter: remove the living ones.
	for (const TWeakObjectPtr<AActor>& Add : SpawnedAdds)
	{
		if (AActor* Actor = Add.Get())
		{
			if (UTVCombatLibrary::IsAlive(Actor))
			{
				Actor->Destroy();
			}
		}
	}
	SpawnedAdds.Reset();

	if (UTVAbilitySystemComponent* ASC = UTVCombatLibrary::GetTVASC(GetOwner()))
	{
		const FGameplayTag EnrageTag = TVTags::Get(TEXT("TV.Buff.Enrage"));
		if (EnrageTag.IsValid() && GetOwner()->HasAuthority())
		{
			ASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(EnrageTag));
		}
	}
}

void UTVBossDriverComponent::RegisterSpawnedAdd(AActor* Add)
{
	if (Add)
	{
		SpawnedAdds.Add(Add);
	}
}
