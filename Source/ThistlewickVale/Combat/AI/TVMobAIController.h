// Mob AI: a small C++ state machine (Idle/Patrol -> Combat (chase, melee swing, cast from row) -> Evade/reset),
// ticked on a timer on the server. No Behavior Tree asset is required; FTVMobRow::BehaviorTreeId selects a profile:
//   BT_TV_Melee, BT_TV_MeleeFlee (Call for Help at 20 %), BT_TV_Charger (opens with its Leap), BT_TV_Caster (keeps range,
//   heals allies under 50 %), BT_TV_Floater (ranged, kites), BT_TV_ShieldKnight (melee, keeps its block up),
//   BT_TV_BossData (UTVBossDriverComponent picks abilities), BT_TV_Static (never moves, casts on its summoner).
#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "TVMobAIController.generated.h"

class ATVMobCharacter;
struct FTVAbilityRow;

UENUM(BlueprintType)
enum class ETVMobAIState : uint8 { Idle, Patrol, Combat, Evade, Dead };

UENUM(BlueprintType)
enum class ETVMobAIProfile : uint8 { Melee, MeleeFlee, Charger, Caster, Floater, ShieldKnight, BossData, Static };

UCLASS()
class THISTLEWICKVALE_API ATVMobAIController : public AAIController
{
	GENERATED_BODY()

public:
	ATVMobAIController(const FObjectInitializer& ObjectInitializer);

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	/** Server. Enter combat against Attacker (damage taken, social aggro, summon). */
	void EngageTarget(AActor* Attacker);
	/** Server. Leash / wipe: drop threat, go home, full reset. */
	void BeginEvade();
	ETVMobAIState GetState() const { return State; }
	ETVMobAIProfile GetProfile() const { return Profile; }
	/** Server. Used by the boss driver: cast an ability at the right target now. Returns true if it started. */
	bool TryCastAbility(FName AbilityId, bool bFromBossRotation);
	/** Time since the current fight started (0 out of combat). */
	float GetCombatTime() const;

	static ETVMobAIProfile ProfileFromName(FName BehaviorTreeId);

	UPROPERTY(EditDefaultsOnly, Category = "TV|AI") float ThinkInterval = 0.25f;
	/** Sight radius cap (GDD: 20 m). */
	UPROPERTY(EditDefaultsOnly, Category = "TV|AI") float MaxSightCm = 2000.f;
	/** Seconds idle at each patrol point. */
	UPROPERTY(EditDefaultsOnly, Category = "TV|AI") float PatrolPause = 3.f;

protected:
	void Think();
	void ThinkIdle();
	void ThinkCombat();
	void ThinkEvade();
	AActor* FindAggroTarget() const;
	float GetAggroRadiusFor(const AActor* Player) const;
	bool ShouldLeash() const;
	/** Abilities from the row (non-boss profiles). Returns true if one started. */
	bool TryUseRowAbility(AActor* Target);
	AActor* ChooseAbilityTarget(const FTVAbilityRow& Row, AActor* CombatTarget, bool bFromBossRotation) const;
	void DoMeleeSwing(AActor* Target);
	void MoveTowards(AActor* Target, float AcceptRadius);
	void FinishEvade();
	ATVMobCharacter* GetMob() const;

	ETVMobAIState State = ETVMobAIState::Idle;
	ETVMobAIProfile Profile = ETVMobAIProfile::Melee;
	FTimerHandle ThinkTimer;
	double LastSwingTime = -100.0;
	double CombatStartTime = 0.0;
	double NextPatrolMoveTime = 0.0;
	int32 PatrolIndex = 0;
	bool bUsedFleeCall = false;
	bool bUsedOpener = false;
	TMap<FName, double> NextAbilityTime;
};
