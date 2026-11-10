// ATVCharacterBase (shared), ATVPlayerCharacter (ASC on the player state), ATVMobCharacter (ASC on the pawn).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "Combat/TVCombatRows.h"
#include "TVCharacterBase.generated.h"

class UAbilitySystemComponent;
class UTVAbilitySystemComponent;
class UTVAttributeSet_Vitals;
class UTVAttributeSet_Stats;
class UTVAttributeSet_Combat;
class UStaticMeshComponent;
class USpringArmComponent;
class UCameraComponent;
class UTVThreatComponent;
class UTVBossDataAsset;
class UTVBossDriverComponent;
class ATVSpawnPoint;
class ATVPlayerState;
struct FOnAttributeChangeData;

UENUM(BlueprintType)
enum class ETVCombatTextKind : uint8 { Damage, CritDamage, Heal, CritHeal, Dodge, Absorb, Immune, Resource };

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTVOnCharacterDied, ATVCharacterBase*, Character, AActor*, Killer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FTVOnCombatText, ATVCharacterBase*, Character, float, Amount, ETVCombatTextKind, Kind, AActor*, Instigator);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTVOnCombatTargetChanged, ATVCharacterBase*, Character, AActor*, NewTarget);

UCLASS(Abstract)
class THISTLEWICKVALE_API ATVCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ATVCharacterBase(const FObjectInitializer& ObjectInitializer);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UTVAbilitySystemComponent* GetTVAbilitySystemComponent() const;

	UFUNCTION(BlueprintPure, Category = "TV|Character") bool IsDead() const { return bIsDead; }
	/** Display-name key: class id for players, mob id for mobs (UI resolves the text from the row). */
	UFUNCTION(BlueprintPure, Category = "TV|Character") virtual FName GetDisplayNameId() const { return NAME_None; }
	UFUNCTION(BlueprintPure, Category = "TV|Character") virtual int32 GetCharacterLevel() const { return 1; }

	/** Server. Health reached 0. */
	virtual void HandleDeath(AActor* Killer);
	/** Server. Back to life at full health. */
	virtual void Revive(const FTransform& Where);

	/** What this character is attacking / targeting (replicated to everyone: target-of-target, nameplates). */
	UFUNCTION(BlueprintPure, Category = "TV|Character") AActor* GetCombatTarget() const { return CombatTarget; }
	void SetCombatTarget(AActor* NewTarget);

	/** Server -> all: floating combat text. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastCombatText(float Amount, ETVCombatTextKind Kind, AActor* TextInstigator);
	/** Server -> all: montage + VFX/SFX manifest ids (art hooks; montage is played here, VFX/SFX in OnCombatCue). */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastCombatCue(FName MontageId, FName VfxId, FName SfxId, FVector_NetQuantize Location);
	UFUNCTION(BlueprintImplementableEvent, Category = "TV|Character")
	void OnCombatCue(FName VfxId, FName SfxId, FVector Location);

	/** Recomputes CharacterMovement max walk speed from MoveSpeedMultiplier, roots, stuns and combat state. */
	void UpdateMovementSpeed();
	/** Loads a manifest mesh: static mesh -> PlaceholderMesh, skeletal mesh -> character mesh. */
	void ApplyVisualsFromManifest(FName MeshId, FName AnimBlueprintId, float Scale);

	UPROPERTY(BlueprintAssignable, Category = "TV|Character") FTVOnCharacterDied OnDied;
	UPROPERTY(BlueprintAssignable, Category = "TV|Character") FTVOnCombatText OnCombatText;
	UPROPERTY(BlueprintAssignable, Category = "TV|Character") FTVOnCombatTargetChanged OnCombatTargetChanged;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TV|Character") TObjectPtr<UStaticMeshComponent> PlaceholderMesh;

protected:
	UFUNCTION(NetMulticast, Reliable) void MulticastDeath(AActor* Killer);
	UFUNCTION() void OnRep_IsDead();
	UFUNCTION() void OnRep_CombatTarget();
	virtual void OnDeathVisuals();

	/** Call once the ASC actor info is initialised (both server and clients). */
	void BindAbilitySystemCallbacks();
	void OnMoveSpeedAttributeChanged(const FOnAttributeChangeData& Data);
	void OnMovementTagChanged(const FGameplayTag Tag, int32 NewCount);

	UPROPERTY(ReplicatedUsing = OnRep_IsDead) bool bIsDead = false;
	UPROPERTY(ReplicatedUsing = OnRep_CombatTarget) TObjectPtr<AActor> CombatTarget;
	/** cm/s before MoveSpeedMultiplier. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TV|Character") float BaseWalkSpeed = 500.f;

	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
};

/** Player avatar. Ability system lives on ATVPlayerState so it survives death. */
UCLASS()
class THISTLEWICKVALE_API ATVPlayerCharacter : public ATVCharacterBase
{
	GENERATED_BODY()

public:
	ATVPlayerCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual FName GetDisplayNameId() const override;
	virtual int32 GetCharacterLevel() const override;
	virtual void HandleDeath(AActor* Killer) override;

	ATVPlayerState* GetTVPlayerState() const;

	/** Server (Net's game mode / dungeon manager): full health and resource, death state cleared, teleported to At. */
	void ServerRevive(const FTransform& At);

	// Input lives on ATVPlayerController (Net). The camera boom is found there with FindComponentByClass<USpringArmComponent>.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TV|Camera") TObjectPtr<USpringArmComponent> CameraBoom;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TV|Camera") TObjectPtr<UCameraComponent> FollowCamera;

	/** Mesh/anim/scale from the class row (called on pawn setup and when ATVPlayerState::ClassId replicates). */
	void ApplyClassVisuals();

protected:
	void InitAbilityActorInfoFromPlayerState();
};

/** AI mob. Spawned by ATVSpawnPoint (or summoned); configured from DT_Mobs. */
UCLASS()
class THISTLEWICKVALE_API ATVMobCharacter : public ATVCharacterBase
{
	GENERATED_BODY()

public:
	ATVMobCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual FName GetDisplayNameId() const override { return MobId; }
	virtual int32 GetCharacterLevel() const override { return MobLevel; }
	virtual void HandleDeath(AActor* Killer) override;

	/** Server. HealthScale/DamageScale are the party/dungeon scaling factors (1 = none). */
	void InitFromMobRow(FName InMobId, int32 Level, float HealthScale, float DamageScale);
	UFUNCTION(BlueprintPure, Category = "TV|Mob") FName GetMobId() const { return MobId; }
	const FTVMobRow* GetMobRow() const;
	/** Melee hit damage: BaseDamage x (1 + 0.12 x (Level - LevelMin)) x DamageScale x DamageDoneMultiplier excluded. */
	float GetHitDamage() const { return HitDamage; }
	float GetHealthScale() const { return HealthScaleApplied; }
	float GetDamageScale() const { return DamageScaleApplied; }
	ETVMobTier GetTier() const { return Tier; }
	UTVThreatComponent* GetThreatComponent() const { return ThreatComponent; }
	UTVBossDriverComponent* GetBossDriver() const { return BossDriver; }

	/** Server. Attaches a boss driver (BehaviorTreeId BT_TV_BossData). */
	void SetBossData(UTVBossDataAsset* InBossData);

	FVector GetHomeLocation() const { return HomeLocation; }
	void SetHomeLocation(const FVector& InHome) { HomeLocation = InHome; }
	void SetPatrolPoints(const TArray<FVector>& InPoints) { PatrolPoints = InPoints; }
	const TArray<FVector>& GetPatrolPoints() const { return PatrolPoints; }

	AActor* GetSummoner() const { return Summoner.Get(); }
	void SetSummoner(AActor* InSummoner) { Summoner = InSummoner; }
	void SetOwningSpawnPoint(ATVSpawnPoint* InSpawnPoint);

	/** Server spawn helper used by spawn points and summons. Returns null if the row is missing. */
	static ATVMobCharacter* SpawnMob(UWorld* World, TSubclassOf<ATVMobCharacter> MobClass, FName InMobId, int32 Level,
		const FTransform& Transform, float HealthScale, float DamageScale, ULevel* OverrideLevel = nullptr);

	/** Server. Re-applies party/dungeon scaling to a living mob (health fraction kept). */
	void ApplyScaling(float HealthScale, float DamageScale);
	/** Server. Open-world PartyScaling mobs (BALANCE section 1): scale to the puller's party size when the fight starts. */
	void ApplyPullScaling(AActor* Puller);
	/** Server. Back to the scaling it had before the pull (evade). */
	void ResetPullScaling();

	/** Seconds the corpse stays before it is destroyed. */
	UPROPERTY(EditDefaultsOnly, Category = "TV|Mob") float CorpseLifetime = 12.f;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TV|Mob") TObjectPtr<UTVAbilitySystemComponent> AbilitySystem;
	UPROPERTY() TObjectPtr<UTVAttributeSet_Vitals> VitalsSet;
	UPROPERTY() TObjectPtr<UTVAttributeSet_Stats> StatsSet;
	UPROPERTY() TObjectPtr<UTVAttributeSet_Combat> CombatSet;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TV|Mob") TObjectPtr<UTVThreatComponent> ThreatComponent;
	UPROPERTY() TObjectPtr<UTVBossDriverComponent> BossDriver;

	UFUNCTION() void OnRep_MobId();

	UPROPERTY(ReplicatedUsing = OnRep_MobId) FName MobId;
	UPROPERTY(Replicated) int32 MobLevel = 1;
	UPROPERTY(Replicated) ETVMobTier Tier = ETVMobTier::Normal;

	float HitDamage = 0.f;
	float HealthScaleApplied = 1.f;
	float DamageScaleApplied = 1.f;
	FVector HomeLocation = FVector::ZeroVector;
	TArray<FVector> PatrolPoints;
	TWeakObjectPtr<AActor> Summoner;
	TWeakObjectPtr<ATVSpawnPoint> OwningSpawnPoint;
	/** Scaling set by a spawn point / dungeon (pull scaling is skipped). */
	bool bScaledExternally = false;
	bool bPullScaled = false;
};
