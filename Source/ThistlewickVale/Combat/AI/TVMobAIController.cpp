#include "Combat/AI/TVMobAIController.h"
#include "ThistlewickVale.h"
#include "Combat/AI/TVThreatComponent.h"
#include "Combat/AI/TVBossDriverComponent.h"
#include "Combat/TVBossDataAsset.h"
#include "Combat/TVAbilitySystemComponent.h"
#include "Combat/TVAttributeSets.h"
#include "Combat/TVCharacterBase.h"
#include "Combat/TVCombatLibrary.h"
#include "Combat/TVCombatTags.h"
#include "Combat/Abilities/TVAbilityExecutors.h"
#include "Combat/Effects/TVGameplayEffects.h"
#include "Navigation/PathFollowingComponent.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
	float HealthFraction(const AActor* Actor)
	{
		const UTVAbilitySystemComponent* ASC = UTVCombatLibrary::GetTVASC(Actor);
		if (!ASC)
		{
			return 1.f;
		}
		const float Max = ASC->GetNumericAttribute(UTVAttributeSet_Vitals::GetMaxHealthAttribute());
		return Max > 0.f ? ASC->GetNumericAttribute(UTVAttributeSet_Vitals::GetHealthAttribute()) / Max : 1.f;
	}


	/** Edge-to-edge distance between two characters. */
	float EdgeDistance(const AActor* A, const AActor* B)
	{
		return FMath::Max(0.f, UTVCombatLibrary::DistanceBetween(A, B) - UTVCombatLibrary::GetCapsuleRadius(A) - UTVCombatLibrary::GetCapsuleRadius(B));
	}

	bool IsLeapRow(const FTVAbilityRow& Row)
	{
		const UClass* Class = Row.AbilityClass.LoadSynchronous();
		return Class && Class->IsChildOf(UGA_TV_Leap::StaticClass());
	}
}

ATVMobAIController::ATVMobAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bStartAILogicOnPossess = true;
}

void ATVMobAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	State = ETVMobAIState::Idle;
	ResolvedProfileMobId = NAME_None;
	if (UWorld* World = GetWorld())
	{
		// Small random offset so a pack of mobs does not think on the same frame.
		World->GetTimerManager().SetTimer(ThinkTimer, this, &ATVMobAIController::Think, ThinkInterval, true, FMath::FRandRange(0.f, ThinkInterval));
	}
}

void ATVMobAIController::OnUnPossess()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ThinkTimer);
	}
	Super::OnUnPossess();
}

ATVMobCharacter* ATVMobAIController::GetMob() const
{
	return Cast<ATVMobCharacter>(GetPawn());
}

ETVMobAIProfile ATVMobAIController::ProfileFromName(FName BehaviorTreeId)
{
	const FString Name = BehaviorTreeId.ToString();
	if (Name == TEXT("BT_TV_MeleeFlee"))    return ETVMobAIProfile::MeleeFlee;
	if (Name == TEXT("BT_TV_Charger"))      return ETVMobAIProfile::Charger;
	if (Name == TEXT("BT_TV_Caster"))       return ETVMobAIProfile::Caster;
	if (Name == TEXT("BT_TV_Floater"))      return ETVMobAIProfile::Floater;
	if (Name == TEXT("BT_TV_ShieldKnight")) return ETVMobAIProfile::ShieldKnight;
	if (Name == TEXT("BT_TV_BossData"))     return ETVMobAIProfile::BossData;
	if (Name == TEXT("BT_TV_Static"))       return ETVMobAIProfile::Static;
	if (Name != TEXT("BT_TV_Melee") && !Name.IsEmpty() && Name != TEXT("None"))
	{
		UE_LOG(LogTV, Warning, TEXT("Unknown mob AI profile '%s', using BT_TV_Melee"), *Name);
	}
	return ETVMobAIProfile::Melee;
}

float ATVMobAIController::GetCombatTime() const
{
	const UWorld* World = GetWorld();
	return (State == ETVMobAIState::Combat && World) ? static_cast<float>(World->GetTimeSeconds() - CombatStartTime) : 0.f;
}

// ---- Think ---------------------------------------------------------------------------------------------------------

void ATVMobAIController::Think()
{
	ATVMobCharacter* Mob = GetMob();
	if (!Mob || !HasAuthority())
	{
		return;
	}
	if (Mob->IsDead())
	{
		if (State != ETVMobAIState::Dead)
		{
			State = ETVMobAIState::Dead;
			StopMovement();
			ClearFocus(EAIFocusPriority::Gameplay);
		}
		return;
	}
	if (Mob->GetMobId().IsNone())
	{
		return; // not initialised yet
	}
	if (ResolvedProfileMobId != Mob->GetMobId())
	{
		ResolvedProfileMobId = Mob->GetMobId();
		const FTVMobRow* Row = Mob->GetMobRow();
		Profile = Row ? ProfileFromName(Row->BehaviorTreeId) : ETVMobAIProfile::Melee;
	}

	switch (State)
	{
	case ETVMobAIState::Idle:
	case ETVMobAIState::Patrol:
		ThinkIdle();
		break;
	case ETVMobAIState::Combat:
		ThinkCombat();
		break;
	case ETVMobAIState::Evade:
		ThinkEvade();
		break;
	case ETVMobAIState::Dead:
		break;
	}
}

void ATVMobAIController::ThinkIdle()
{
	ATVMobCharacter* Mob = GetMob();
	UWorld* World = GetWorld();
	if (!Mob || !World)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();

	// Static mobs (shard pillars) only channel on their summoner.
	if (Profile == ETVMobAIProfile::Static)
	{
		if (AActor* Summoner = Mob->GetSummoner())
		{
			if (UTVCombatLibrary::IsAlive(Summoner))
			{
				TryUseRowAbility(Summoner);
			}
		}
		return;
	}

	if (AActor* Enemy = FindAggroTarget())
	{
		EngageTarget(Enemy);
		return;
	}

	const TArray<FVector>& Points = Mob->GetPatrolPoints();
	if (Points.Num() > 0)
	{
		State = ETVMobAIState::Patrol;
		if (GetMoveStatus() == EPathFollowingStatus::Idle)
		{
			if (NextPatrolMoveTime <= 0.0)
			{
				NextPatrolMoveTime = Now + PatrolPause;
			}
			else if (Now >= NextPatrolMoveTime)
			{
				PatrolIndex = (PatrolIndex + 1) % Points.Num();
				MoveToLocation(Points[PatrolIndex], 50.f);
				NextPatrolMoveTime = 0.0;
			}
		}
		return;
	}

	State = ETVMobAIState::Idle;
	if (FVector::Dist2D(Mob->GetActorLocation(), Mob->GetHomeLocation()) > 200.f && GetMoveStatus() == EPathFollowingStatus::Idle)
	{
		MoveToLocation(Mob->GetHomeLocation(), 50.f);
	}
}

AActor* ATVMobAIController::FindAggroTarget() const
{
	ATVMobCharacter* Mob = GetMob();
	UWorld* World = GetWorld();
	if (!Mob || !World)
	{
		return nullptr;
	}
	AActor* Best = nullptr;
	float BestDistance = TNumericLimits<float>::Max();
	for (TActorIterator<ATVPlayerCharacter> It(World); It; ++It)
	{
		ATVPlayerCharacter* Player = *It;
		if (!UTVCombatLibrary::IsAlive(Player))
		{
			continue;
		}
		const float Distance = FVector::Dist(Player->GetActorLocation(), Mob->GetActorLocation());
		if (Distance > FMath::Min(GetAggroRadiusFor(Player), MaxSightCm) || Distance >= BestDistance)
		{
			continue;
		}
		if (!UTVCombatLibrary::HasLineOfSight(Mob, Player))
		{
			continue;
		}
		Best = Player;
		BestDistance = Distance;
	}
	return Best;
}

float ATVMobAIController::GetAggroRadiusFor(const AActor* Player) const
{
	const ATVMobCharacter* Mob = GetMob();
	const FTVMobRow* Row = Mob ? Mob->GetMobRow() : nullptr;
	if (!Row || Row->AggroRadiusCm <= 0.f)
	{
		return 0.f;
	}
	// Higher-level players draw less attention: -10 % radius per level above the mob (50 %..150 %).
	const int32 LevelDiff = UTVCombatLibrary::GetCombatLevel(Player) - Mob->GetCharacterLevel();
	return Row->AggroRadiusCm * FMath::Clamp(1.f - 0.1f * LevelDiff, 0.5f, 1.5f);
}

void ATVMobAIController::EngageTarget(AActor* Attacker)
{
	ATVMobCharacter* Mob = GetMob();
	UWorld* World = GetWorld();
	if (!Mob || !World || Mob->IsDead() || State == ETVMobAIState::Evade || Profile == ETVMobAIProfile::Static)
	{
		return;
	}
	if (!UTVCombatLibrary::IsAlive(Attacker) || !UTVCombatLibrary::IsHostile(Mob, Attacker))
	{
		return;
	}
	if (UTVThreatComponent* Threat = Mob->GetThreatComponent())
	{
		Threat->AddThreat(Attacker, 0.f);
	}
	if (State == ETVMobAIState::Combat)
	{
		return;
	}
	State = ETVMobAIState::Combat;
	CombatStartTime = World->GetTimeSeconds();
	bUsedFleeCall = false;
	bUsedOpener = false;
	NextAbilityTime.Reset();
	StopMovement();
	Mob->ApplyPullScaling(Attacker);
	if (UTVAbilitySystemComponent* ASC = Mob->GetTVAbilitySystemComponent())
	{
		ASC->NotifyCombatActivity();
		ASC->SetAbilityTarget(Attacker);
	}
	if (UTVBossDriverComponent* Driver = Mob->GetBossDriver())
	{
		Driver->StartEncounter();
	}
}

bool ATVMobAIController::ShouldLeash() const
{
	const ATVMobCharacter* Mob = GetMob();
	const FTVMobRow* Row = Mob ? Mob->GetMobRow() : nullptr;
	if (!Row || Profile == ETVMobAIProfile::Static || Row->LeashRadiusCm <= 0.f)
	{
		return false;
	}
	return FVector::Dist(Mob->GetActorLocation(), Mob->GetHomeLocation()) > Row->LeashRadiusCm;
}

void ATVMobAIController::ThinkCombat()
{
	ATVMobCharacter* Mob = GetMob();
	UWorld* World = GetWorld();
	const FTVMobRow* Row = Mob ? Mob->GetMobRow() : nullptr;
	UTVThreatComponent* Threat = Mob ? Mob->GetThreatComponent() : nullptr;
	UTVAbilitySystemComponent* ASC = Mob ? Mob->GetTVAbilitySystemComponent() : nullptr;
	if (!Mob || !World || !Row || !Threat || !ASC)
	{
		return;
	}
	if (!Threat->HasAnyLiving() || ShouldLeash())
	{
		BeginEvade();
		return;
	}

	// Everyone on the table (and the mob) stays in combat.
	ASC->NotifyCombatActivity();
	TArray<AActor*> Engaged;
	Threat->GetThreatActors(Engaged);
	for (AActor* Actor : Engaged)
	{
		if (UTVAbilitySystemComponent* OtherASC = UTVCombatLibrary::GetTVASC(Actor))
		{
			OtherASC->NotifyCombatActivity();
		}
	}

	AActor* Target = Threat->SelectTarget(Mob->GetCombatTarget(), Row->AttackRangeCm + 150.f);
	if (!Target)
	{
		BeginEvade();
		return;
	}
	if (Target != Mob->GetCombatTarget())
	{
		ASC->SetAbilityTarget(Target);
	}
	SetFocus(Target);

	const bool bStunned = TVTags::State_Stunned().IsValid() && ASC->HasMatchingGameplayTag(TVTags::State_Stunned());
	if (bStunned)
	{
		StopMovement();
		return;
	}
	if (ASC->IsCasting())
	{
		StopMovement();
		return;
	}

	// Abilities.
	bool bStartedAbility = false;
	UTVBossDriverComponent* Driver = Mob->GetBossDriver();
	if (Profile == ETVMobAIProfile::BossData && Driver && Driver->GetBossData())
	{
		Driver->TickBoss(ThinkInterval);
		bStartedAbility = ASC->IsCasting();
	}
	else
	{
		bStartedAbility = TryUseRowAbility(Target);
	}
	if (bStartedAbility && ASC->IsCasting())
	{
		StopMovement();
		return;
	}

	// Movement and auto attack.
	const float Distance = EdgeDistance(Mob, Target);
	const float AttackRange = FMath::Max(100.f, Row->AttackRangeCm);
	const bool bRanged = AttackRange > 600.f;
	const bool bNeedsSight = bRanged && !UTVCombatLibrary::HasLineOfSight(Mob, Target);
	if (Profile != ETVMobAIProfile::Static && (Distance > AttackRange || bNeedsSight))
	{
		MoveTowards(Target, bRanged ? AttackRange * 0.7f : AttackRange * 0.8f);
		if (Distance > AttackRange)
		{
			return;
		}
	}
	else if (GetMoveStatus() != EPathFollowingStatus::Idle)
	{
		StopMovement();
	}

	const double Now = World->GetTimeSeconds();
	if (!bNeedsSight && Row->BaseDamage > 0.f && Now - LastSwingTime >= FMath::Max(0.5f, Row->AttackSpeed))
	{
		DoMeleeSwing(Target);
	}
}

bool ATVMobAIController::TryUseRowAbility(AActor* Target)
{
	ATVMobCharacter* Mob = GetMob();
	UWorld* World = GetWorld();
	const FTVMobRow* MobRow = Mob ? Mob->GetMobRow() : nullptr;
	UTVAbilitySystemComponent* ASC = Mob ? Mob->GetTVAbilitySystemComponent() : nullptr;
	if (!MobRow || !ASC || !World || ASC->GetGCDRemaining() > 0.f || ASC->IsCasting())
	{
		return false;
	}
	const double Now = World->GetTimeSeconds();
	const float MyHealth = HealthFraction(Mob);

	for (const FName& AbilityId : MobRow->Abilities)
	{
		const FTVAbilityRow* Row = UTVCombatLibrary::FindAbilityRow(AbilityId);
		if (!Row || Now < NextAbilityTime.FindRef(AbilityId) || ASC->GetCooldownRemaining(AbilityId) > 0.f)
		{
			continue;
		}

		// Per-ability conditions (data has no conditions, so they come from the row's shape).
		if (UTVCombatLibrary::FindEffectOfClass(*Row, UGE_TV_SocialAggro::StaticClass()))
		{
			const float Threshold = Profile == ETVMobAIProfile::MeleeFlee ? 0.2f : 0.4f;
			if (bUsedFleeCall || MyHealth > Threshold)
			{
				continue;
			}
		}
		else if (IsLeapRow(*Row) && Row->TargetType == ETVTargetType::Enemy)
		{
			// Chargers open with the charge and use it again when the target gets away.
			if (!Target || EdgeDistance(Mob, Target) < 500.f)
			{
				continue;
			}
		}

		const bool bStarted = TryCastAbility(AbilityId, false);
		// Retry throttle (cooldowns themselves are effects on the ASC).
		NextAbilityTime.Add(AbilityId, Now + (bStarted ? 1.0 : 2.0));
		if (bStarted)
		{
			if (UTVCombatLibrary::FindEffectOfClass(*Row, UGE_TV_SocialAggro::StaticClass()))
			{
				bUsedFleeCall = true;
			}
			if (IsLeapRow(*Row))
			{
				bUsedOpener = true;
			}
			return true;
		}
	}
	return false;
}

AActor* ATVMobAIController::ChooseAbilityTarget(const FTVAbilityRow& Row, AActor* CombatTarget, bool bFromBossRotation) const
{
	ATVMobCharacter* Mob = GetMob();
	if (!Mob)
	{
		return nullptr;
	}
	switch (Row.TargetType)
	{
	case ETVTargetType::Self:
	case ETVTargetType::PartyAoE:
		return Mob;
	case ETVTargetType::Friendly:
	{
		if (Profile == ETVMobAIProfile::Static)
		{
			return Mob->GetSummoner();
		}
		// Lowest health ally under 50 % in range (self included).
		TArray<AActor*> Friends;
		UTVCombatLibrary::GetFriendliesInRadius(Mob, Mob->GetActorLocation(), FMath::Max(Row.RangeCm, 500.f), Friends);
		AActor* Best = nullptr;
		float BestFraction = 0.5f;
		for (AActor* Friend : Friends)
		{
			const float Fraction = HealthFraction(Friend);
			if (Fraction < BestFraction)
			{
				Best = Friend;
				BestFraction = Fraction;
			}
		}
		return Best;
	}
	case ETVTargetType::Ground:
	case ETVTargetType::Enemy:
	default:
		// Boss rotation: long-range abilities go to a random player other than the tank when possible.
		if (bFromBossRotation && Row.RangeCm >= 2000.f && Mob->GetThreatComponent())
		{
			if (AActor* Random = Mob->GetThreatComponent()->GetRandomTarget(CombatTarget))
			{
				return Random;
			}
		}
		return CombatTarget;
	}
}

bool ATVMobAIController::TryCastAbility(FName AbilityId, bool bFromBossRotation)
{
	ATVMobCharacter* Mob = GetMob();
	UTVAbilitySystemComponent* ASC = Mob ? Mob->GetTVAbilitySystemComponent() : nullptr;
	const FTVAbilityRow* Row = UTVCombatLibrary::FindAbilityRow(AbilityId);
	if (!Mob || !ASC || !Row || Mob->IsDead())
	{
		return false;
	}
	AActor* CombatTarget = Mob->GetCombatTarget();
	AActor* AbilityTarget = ChooseAbilityTarget(*Row, CombatTarget, bFromBossRotation);
	const bool bUnitTarget = Row->TargetType == ETVTargetType::Enemy || Row->TargetType == ETVTargetType::Friendly;
	if (bUnitTarget && !AbilityTarget)
	{
		return false;
	}
	if (AbilityTarget && AbilityTarget != Mob && Row->RangeCm > 0.f && EdgeDistance(Mob, AbilityTarget) > Row->RangeCm)
	{
		return false;
	}
	if (!Row->UsableWhileMoving)
	{
		StopMovement();
	}
	ASC->SetAbilityTarget(AbilityTarget ? AbilityTarget : CombatTarget);
	const bool bStarted = ASC->TryActivateAbilityById(AbilityId);
	// The ability locked its target; the mob keeps facing / attacking its threat target.
	if (CombatTarget && ASC->GetAbilityTarget() != CombatTarget)
	{
		ASC->SetAbilityTarget(CombatTarget);
	}
	return bStarted;
}

void ATVMobAIController::DoMeleeSwing(AActor* Target)
{
	ATVMobCharacter* Mob = GetMob();
	UWorld* World = GetWorld();
	if (!Mob || !World || !UTVCombatLibrary::IsAlive(Target))
	{
		return;
	}
	LastSwingTime = World->GetTimeSeconds();
	UTVCombatLibrary::DealDamage(Mob, Target, Mob->GetHitDamage(), 1.f, /*bCanCrit*/ false);
	Mob->MulticastCombatCue(NAME_None, NAME_None, NAME_None, Target->GetActorLocation());
}

void ATVMobAIController::MoveTowards(AActor* Target, float AcceptRadius)
{
	if (Target)
	{
		MoveToActor(Target, FMath::Max(50.f, AcceptRadius), /*bStopOnOverlap*/ true, /*bUsePathfinding*/ true, /*bCanStrafe*/ false);
	}
}

// ---- Evade -----------------------------------------------------------------------------------------------------------

void ATVMobAIController::BeginEvade()
{
	ATVMobCharacter* Mob = GetMob();
	UWorld* World = GetWorld();
	if (!Mob || !World || State == ETVMobAIState::Evade)
	{
		return;
	}
	State = ETVMobAIState::Evade;
	EvadeStartTime = World->GetTimeSeconds();
	ClearFocus(EAIFocusPriority::Gameplay);
	if (UTVAbilitySystemComponent* ASC = Mob->GetTVAbilitySystemComponent())
	{
		ASC->ClearCombatState(true);
		ASC->SetAbilityTarget(nullptr);
		if (TVTags::State_Evading().IsValid())
		{
			ASC->SetLooseGameplayTagCount(TVTags::State_Evading(), 1);
		}
	}
	if (UTVThreatComponent* Threat = Mob->GetThreatComponent())
	{
		Threat->ClearThreat();
	}
	if (UTVBossDriverComponent* Driver = Mob->GetBossDriver())
	{
		Driver->ResetEncounter();
	}
	Mob->ResetPullScaling();
	MoveToLocation(Mob->GetHomeLocation(), 50.f);
}

void ATVMobAIController::ThinkEvade()
{
	ATVMobCharacter* Mob = GetMob();
	UWorld* World = GetWorld();
	if (!Mob || !World)
	{
		return;
	}
	const bool bHome = FVector::Dist2D(Mob->GetActorLocation(), Mob->GetHomeLocation()) < 150.f;
	const bool bStuck = GetMoveStatus() == EPathFollowingStatus::Idle && !bHome;
	if (bHome)
	{
		FinishEvade();
	}
	else if (bStuck || World->GetTimeSeconds() - EvadeStartTime > 10.0)
	{
		// No path home (or too slow): teleport.
		Mob->TeleportTo(Mob->GetHomeLocation(), Mob->GetActorRotation());
		FinishEvade();
	}
}

void ATVMobAIController::FinishEvade()
{
	ATVMobCharacter* Mob = GetMob();
	if (!Mob)
	{
		return;
	}
	StopMovement();
	if (UTVAbilitySystemComponent* ASC = Mob->GetTVAbilitySystemComponent())
	{
		ASC->ResetVitalsToFull();
		if (TVTags::State_Evading().IsValid())
		{
			ASC->SetLooseGameplayTagCount(TVTags::State_Evading(), 0);
		}
	}
	State = ETVMobAIState::Idle;
	NextPatrolMoveTime = 0.0;
}
