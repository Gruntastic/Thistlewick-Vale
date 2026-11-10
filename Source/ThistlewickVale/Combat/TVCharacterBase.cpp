#include "Combat/TVCharacterBase.h"
#include "ThistlewickVale.h"
#include "Core/TVAssetRegistrySubsystem.h"
#include "Combat/TVAbilitySystemComponent.h"
#include "Combat/TVAttributeSets.h"
#include "Combat/TVPlayerState.h"
#include "Combat/TVCombatLibrary.h"
#include "Combat/TVCombatTags.h"
#include "Combat/TVCombatEventsSubsystem.h"
#include "Combat/AI/TVThreatComponent.h"
#include "Combat/AI/TVBossDriverComponent.h"
#include "Combat/AI/TVMobAIController.h"
#include "Combat/AI/TVSpawnPoint.h"
#include "Net/TVPartySubsystem.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Animation/AnimMontage.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

// =====================================================================================================================
// ATVCharacterBase
// =====================================================================================================================

ATVCharacterBase::ATVCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

	PlaceholderMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderMesh"));
	PlaceholderMesh->SetupAttachment(GetCapsuleComponent());
	PlaceholderMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlaceholderMesh->SetRelativeLocation(FVector(0.f, 0.f, -90.f));

	bUseControllerRotationYaw = false;
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->bOrientRotationToMovement = true;
		Move->RotationRate = FRotator(0.f, 540.f, 0.f);
		Move->MaxWalkSpeed = BaseWalkSpeed;
	}
}

void ATVCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATVCharacterBase, bIsDead);
	DOREPLIFETIME(ATVCharacterBase, CombatTarget);
}

UAbilitySystemComponent* ATVCharacterBase::GetAbilitySystemComponent() const
{
	// Players: the ASC lives on the player state. ATVMobCharacter overrides this.
	if (const ATVPlayerState* PS = GetPlayerState<ATVPlayerState>())
	{
		return PS->GetAbilitySystemComponent();
	}
	return nullptr;
}

UTVAbilitySystemComponent* ATVCharacterBase::GetTVAbilitySystemComponent() const
{
	return Cast<UTVAbilitySystemComponent>(GetAbilitySystemComponent());
}

void ATVCharacterBase::HandleDeath(AActor* Killer)
{
	if (!HasAuthority() || bIsDead)
	{
		return;
	}
	bIsDead = true;

	if (UTVAbilitySystemComponent* ASC = GetTVAbilitySystemComponent())
	{
		ASC->CancelAllAbilities();
		ASC->ClearCombatState(true);
		ASC->SetNumericAttributeBase(UTVAttributeSet_Vitals::GetHealthAttribute(), 0.f);
		if (TVTags::State_Dead().IsValid())
		{
			ASC->SetLooseGameplayTagCount(TVTags::State_Dead(), 1);
		}
	}
	SetCombatTarget(nullptr);

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}

	MulticastDeath(Killer);
	OnDied.Broadcast(this, Killer);
}

void ATVCharacterBase::Revive(const FTransform& Where)
{
	if (!HasAuthority())
	{
		return;
	}
	bIsDead = false;
	if (UTVAbilitySystemComponent* ASC = GetTVAbilitySystemComponent())
	{
		if (TVTags::State_Dead().IsValid())
		{
			ASC->SetLooseGameplayTagCount(TVTags::State_Dead(), 0);
		}
		ASC->ClearCombatState(true);
		ASC->ResetVitalsToFull();
	}
	SetActorLocationAndRotation(Where.GetLocation(), Where.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_Walking);
	}
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	}
	OnRep_IsDead();
	UpdateMovementSpeed();
}

void ATVCharacterBase::SetCombatTarget(AActor* NewTarget)
{
	if (CombatTarget == NewTarget)
	{
		return;
	}
	CombatTarget = NewTarget;
	OnCombatTargetChanged.Broadcast(this, CombatTarget);
}

void ATVCharacterBase::OnRep_CombatTarget()
{
	OnCombatTargetChanged.Broadcast(this, CombatTarget);
}

void ATVCharacterBase::MulticastCombatText_Implementation(float Amount, ETVCombatTextKind Kind, AActor* TextInstigator)
{
	OnCombatText.Broadcast(this, Amount, Kind, TextInstigator);
}

void ATVCharacterBase::MulticastCombatCue_Implementation(FName MontageId, FName VfxId, FName SfxId, FVector_NetQuantize Location)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (!MontageId.IsNone())
	{
		if (UTVAssetRegistrySubsystem* Registry = UTVAssetRegistrySubsystem::Get(this))
		{
			if (UAnimMontage* Montage = Registry->LoadSync<UAnimMontage>(MontageId))
			{
				PlayAnimMontage(Montage);
			}
		}
	}
	OnCombatCue(VfxId, SfxId, Location);
}

void ATVCharacterBase::MulticastDeath_Implementation(AActor* Killer)
{
	if (GetNetMode() != NM_DedicatedServer)
	{
		OnDeathVisuals();
	}
}

void ATVCharacterBase::OnRep_IsDead()
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		if (TVTags::State_Dead().IsValid())
		{
			ASC->SetLooseGameplayTagCount(TVTags::State_Dead(), bIsDead ? 1 : 0);
		}
	}
	if (bIsDead)
	{
		if (GetNetMode() != NM_DedicatedServer)
		{
			OnDeathVisuals();
		}
	}
	else
	{
		// Undo the death pose.
		PlaceholderMesh->SetRelativeRotation(FRotator::ZeroRotator);
		if (USkeletalMeshComponent* SkelMesh = GetMesh())
		{
			if (SkelMesh->IsSimulatingPhysics())
			{
				SkelMesh->SetSimulatePhysics(false);
				SkelMesh->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
				SkelMesh->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -90.f), FRotator(0.f, -90.f, 0.f));
			}
		}
	}
	UpdateMovementSpeed();
}

void ATVCharacterBase::OnDeathVisuals()
{
	// Placeholder art: tip the mesh over. Skeletal meshes with a physics asset go ragdoll.
	USkeletalMeshComponent* SkelMesh = GetMesh();
	if (SkelMesh && SkelMesh->GetPhysicsAsset())
	{
		SkelMesh->SetCollisionProfileName(TEXT("Ragdoll"));
		SkelMesh->SetSimulatePhysics(true);
	}
	else
	{
		PlaceholderMesh->SetRelativeRotation(FRotator(0.f, 0.f, 90.f));
	}
}

void ATVCharacterBase::BindAbilitySystemCallbacks()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC || BoundASC.Get() == ASC)
	{
		return;
	}
	BoundASC = ASC;
	ASC->GetGameplayAttributeValueChangeDelegate(UTVAttributeSet_Stats::GetMoveSpeedMultiplierAttribute())
		.AddUObject(this, &ATVCharacterBase::OnMoveSpeedAttributeChanged);
	ASC->GetGameplayAttributeValueChangeDelegate(UTVAttributeSet_Combat::GetOutOfCombatMoveSpeedBonusAttribute())
		.AddUObject(this, &ATVCharacterBase::OnMoveSpeedAttributeChanged);
	const FGameplayTag MovementTags[] = { TVTags::Debuff_Root(), TVTags::State_Stunned(), TVTags::State_InCombat() };
	for (const FGameplayTag& Tag : MovementTags)
	{
		if (Tag.IsValid())
		{
			ASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ATVCharacterBase::OnMovementTagChanged);
		}
	}
	UpdateMovementSpeed();
}

void ATVCharacterBase::OnMoveSpeedAttributeChanged(const FOnAttributeChangeData& Data)
{
	UpdateMovementSpeed();
}

void ATVCharacterBase::OnMovementTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	UpdateMovementSpeed();
}

void ATVCharacterBase::UpdateMovementSpeed()
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!Move)
	{
		return;
	}
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	float Multiplier = 1.f;
	bool bImmobile = bIsDead;
	if (ASC)
	{
		if (ASC->HasAttributeSetForAttribute(UTVAttributeSet_Stats::GetMoveSpeedMultiplierAttribute()))
		{
			Multiplier = ASC->GetNumericAttribute(UTVAttributeSet_Stats::GetMoveSpeedMultiplierAttribute());
		}
		const UTVAbilitySystemComponent* TVASC = Cast<UTVAbilitySystemComponent>(ASC);
		if (TVASC && !TVASC->IsInCombat() && ASC->HasAttributeSetForAttribute(UTVAttributeSet_Combat::GetOutOfCombatMoveSpeedBonusAttribute()))
		{
			Multiplier += ASC->GetNumericAttribute(UTVAttributeSet_Combat::GetOutOfCombatMoveSpeedBonusAttribute());
		}
		if ((TVTags::Debuff_Root().IsValid() && ASC->HasMatchingGameplayTag(TVTags::Debuff_Root()))
			|| (TVTags::State_Stunned().IsValid() && ASC->HasMatchingGameplayTag(TVTags::State_Stunned())))
		{
			bImmobile = true;
		}
	}
	Move->MaxWalkSpeed = bImmobile ? 0.f : BaseWalkSpeed * FMath::Max(0.f, Multiplier);
}

void ATVCharacterBase::ApplyVisualsFromManifest(FName MeshId, FName AnimBlueprintId, float Scale)
{
	UTVAssetRegistrySubsystem* Registry = UTVAssetRegistrySubsystem::Get(this);
	if (!Registry)
	{
		return;
	}
	if (!MeshId.IsNone())
	{
		UObject* MeshAsset = Registry->LoadSync(MeshId);
		if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(MeshAsset))
		{
			// SetSkeletalMeshAsset checked in 5.8 Components/SkeletalMeshComponent.h.
			GetMesh()->SetSkeletalMeshAsset(SkeletalMesh);
			GetMesh()->SetVisibility(true);
			PlaceholderMesh->SetVisibility(false);
		}
		else if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(MeshAsset))
		{
			PlaceholderMesh->SetStaticMesh(StaticMesh);
			PlaceholderMesh->SetVisibility(true);
		}
	}
	if (!AnimBlueprintId.IsNone())
	{
		// Loads the generated _C class, so this also works in a packaged build (the UAnimBlueprint asset is editor-only).
		if (UClass* AnimClass = Registry->LoadAnimClass(AnimBlueprintId))
		{
			GetMesh()->SetAnimInstanceClass(AnimClass);
		}
	}
	if (Scale > 0.f && !FMath::IsNearlyEqual(Scale, 1.f))
	{
		SetActorScale3D(FVector(Scale));
	}
}

// =====================================================================================================================
// ATVPlayerCharacter
// =====================================================================================================================

ATVPlayerCharacter::ATVPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetCapsuleComponent());
	CameraBoom->TargetArmLength = 800.f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->SetRelativeLocation(FVector(0.f, 0.f, 60.f));

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
}

ATVPlayerState* ATVPlayerCharacter::GetTVPlayerState() const
{
	return GetPlayerState<ATVPlayerState>();
}

void ATVPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitAbilityActorInfoFromPlayerState();
	if (ATVPlayerState* PS = GetTVPlayerState())
	{
		UTVAbilitySystemComponent* ASC = PS->GetTVAbilitySystemComponent();
		// A fresh pawn after a death: the player state still says dead / 0 health.
		if (ASC && !PS->ClassId.IsNone() && ASC->GetNumericAttribute(UTVAttributeSet_Vitals::GetHealthAttribute()) <= 0.f)
		{
			PS->ReviveAtFullHealth();
		}
	}
	ApplyClassVisuals();
}

void ATVPlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	InitAbilityActorInfoFromPlayerState();
	ApplyClassVisuals();
}

void ATVPlayerCharacter::InitAbilityActorInfoFromPlayerState()
{
	ATVPlayerState* PS = GetTVPlayerState();
	UTVAbilitySystemComponent* ASC = PS ? PS->GetTVAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return;
	}
	ASC->InitAbilityActorInfo(PS, this);
	BindAbilitySystemCallbacks();
	if (HasAuthority())
	{
		SetCombatTarget(ASC->GetAbilityTarget());
	}
}

FName ATVPlayerCharacter::GetDisplayNameId() const
{
	const ATVPlayerState* PS = GetTVPlayerState();
	return PS ? PS->ClassId : NAME_None;
}

int32 ATVPlayerCharacter::GetCharacterLevel() const
{
	const ATVPlayerState* PS = GetTVPlayerState();
	return PS ? PS->Level : 1;
}

void ATVPlayerCharacter::HandleDeath(AActor* Killer)
{
	if (!HasAuthority() || IsDead())
	{
		return;
	}
	Super::HandleDeath(Killer);

	// Off every threat table.
	TArray<ATVMobCharacter*> Mobs;
	UTVThreatComponent::FindMobsEngagedWith(this, Mobs);
	for (ATVMobCharacter* Mob : Mobs)
	{
		if (UTVThreatComponent* Threat = Mob ? Mob->GetThreatComponent() : nullptr)
		{
			Threat->RemoveActor(this);
		}
	}

	if (UTVCombatEventsSubsystem* Events = UTVCombatEventsSubsystem::Get(this))
	{
		Events->BroadcastPlayerDied(GetTVPlayerState(), Killer);
	}
}

void ATVPlayerCharacter::ServerRevive(const FTransform& At)
{
	if (!HasAuthority())
	{
		return;
	}
	if (ATVPlayerState* PS = GetTVPlayerState())
	{
		PS->ReviveAtFullHealth();
	}
	Revive(At);
}

void ATVPlayerCharacter::ApplyClassVisuals()
{
	const ATVPlayerState* PS = GetTVPlayerState();
	const FTVClassRow* Row = PS ? UTVCombatLibrary::FindClassRow(PS->ClassId) : nullptr;
	if (Row)
	{
		ApplyVisualsFromManifest(Row->BodyMeshId, Row->AnimBlueprintId, 1.f);
	}
}

// =====================================================================================================================
// ATVMobCharacter
// =====================================================================================================================

ATVMobCharacter::ATVMobCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilitySystem = CreateDefaultSubobject<UTVAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	VitalsSet = CreateDefaultSubobject<UTVAttributeSet_Vitals>(TEXT("VitalsSet"));
	StatsSet = CreateDefaultSubobject<UTVAttributeSet_Stats>(TEXT("StatsSet"));
	CombatSet = CreateDefaultSubobject<UTVAttributeSet_Combat>(TEXT("CombatSet"));

	ThreatComponent = CreateDefaultSubobject<UTVThreatComponent>(TEXT("ThreatComponent"));

	AIControllerClass = ATVMobAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->bUseRVOAvoidance = true;
		Move->RotationRate = FRotator(0.f, 360.f, 0.f);
	}
}

void ATVMobCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATVMobCharacter, MobId);
	DOREPLIFETIME(ATVMobCharacter, MobLevel);
	DOREPLIFETIME(ATVMobCharacter, Tier);
}

UAbilitySystemComponent* ATVMobCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void ATVMobCharacter::BeginPlay()
{
	Super::BeginPlay();
	AbilitySystem->InitAbilityActorInfo(this, this);
	BindAbilitySystemCallbacks();
	if (HomeLocation.IsZero())
	{
		HomeLocation = GetActorLocation();
	}
	// Placed directly in a level with a MobId set in the details panel.
	if (HasAuthority() && !MobId.IsNone() && HitDamage <= 0.f)
	{
		const FTVMobRow* Row = UTVCombatLibrary::FindMobRow(MobId);
		InitFromMobRow(MobId, Row ? Row->LevelMin : 1, 1.f, 1.f);
	}
}

const FTVMobRow* ATVMobCharacter::GetMobRow() const
{
	return UTVCombatLibrary::FindMobRow(MobId);
}

void ATVMobCharacter::InitFromMobRow(FName InMobId, int32 Level, float HealthScale, float DamageScale)
{
	if (!HasAuthority())
	{
		return;
	}
	const FTVMobRow* Row = UTVCombatLibrary::FindMobRow(InMobId);
	if (!Row)
	{
		UE_LOG(LogTV, Warning, TEXT("InitFromMobRow: mob %s not found"), *InMobId.ToString());
		return;
	}
	MobId = InMobId;
	MobLevel = FMath::Max(1, Level);
	Tier = Row->Tier;
	HealthScaleApplied = FMath::Max(0.01f, HealthScale);
	DamageScaleApplied = FMath::Max(0.f, DamageScale);
	bScaledExternally = !FMath::IsNearlyEqual(HealthScale, 1.f) || !FMath::IsNearlyEqual(DamageScale, 1.f);
	bPullScaled = false;
	HitDamage = Row->BaseDamage * (1.f + TVCombatTuning::MobDamagePerLevel * FMath::Max(0, MobLevel - Row->LevelMin)) * DamageScaleApplied;

	AbilitySystem->InitAbilityActorInfo(this, this);
	AbilitySystem->InitializeForMob(*Row, MobLevel, HealthScaleApplied);
	if (Row->FactionTag.IsValid())
	{
		AbilitySystem->SetLooseGameplayTagCount(Row->FactionTag, 1);
	}

	BaseWalkSpeed = Row->MoveSpeed;
	UpdateMovementSpeed();
	ApplyVisualsFromManifest(Row->MeshId, Row->AnimBlueprintId, Row->Scale);
}

void ATVMobCharacter::OnRep_MobId()
{
	if (const FTVMobRow* Row = GetMobRow())
	{
		BaseWalkSpeed = Row->MoveSpeed;
		UpdateMovementSpeed();
		ApplyVisualsFromManifest(Row->MeshId, Row->AnimBlueprintId, Row->Scale);
	}
}

void ATVMobCharacter::ApplyScaling(float HealthScale, float DamageScale)
{
	const FTVMobRow* Row = GetMobRow();
	if (!HasAuthority() || !Row || IsDead())
	{
		return;
	}
	HealthScaleApplied = FMath::Max(0.01f, HealthScale);
	DamageScaleApplied = FMath::Max(0.f, DamageScale);
	bScaledExternally = !FMath::IsNearlyEqual(HealthScale, 1.f) || !FMath::IsNearlyEqual(DamageScale, 1.f);
	HitDamage = Row->BaseDamage * (1.f + TVCombatTuning::MobDamagePerLevel * FMath::Max(0, MobLevel - Row->LevelMin)) * DamageScaleApplied;
	const float MaxHealth = FMath::Max(1.f, (Row->BaseHealth + Row->HealthPerLevel * FMath::Max(0, MobLevel - Row->LevelMin)) * HealthScaleApplied);
	// Health fraction is kept by UTVAbilitySystemComponent's MaxHealth callback.
	AbilitySystem->SetNumericAttributeBase(UTVAttributeSet_Vitals::GetMaxHealthAttribute(), MaxHealth);
}

void ATVMobCharacter::ApplyPullScaling(AActor* Puller)
{
	const FTVMobRow* Row = GetMobRow();
	if (!HasAuthority() || !Row || !Row->PartyScaling || bScaledExternally || bPullScaled)
	{
		return;
	}
	int32 PartySize = 1;
	const APawn* PullerPawn = Cast<APawn>(Puller);
	ATVPlayerState* PS = PullerPawn ? PullerPawn->GetPlayerState<ATVPlayerState>() : nullptr;
	UTVPartySubsystem* Party = UTVPartySubsystem::Get(this);
	if (PS && Party)
	{
		PartySize = FMath::Max(1, Party->GetPartyMembers(PS).Num());
	}
	// BALANCE.md section 1: health x (0.3 + 0.175 N), damage x (0.5 + 0.1 N), capped at 1.0.
	const float Health = FMath::Min(1.f, 0.3f + 0.175f * PartySize);
	const float Damage = FMath::Min(1.f, 0.5f + 0.1f * PartySize);
	bPullScaled = true;
	ApplyScaling(Health, Damage);
	bScaledExternally = false;
}

void ATVMobCharacter::ResetPullScaling()
{
	if (bPullScaled)
	{
		bPullScaled = false;
		ApplyScaling(1.f, 1.f);
		bScaledExternally = false;
	}
}

void ATVMobCharacter::SetBossData(UTVBossDataAsset* InBossData)
{
	if (!HasAuthority() || !InBossData)
	{
		return;
	}
	if (!BossDriver)
	{
		BossDriver = NewObject<UTVBossDriverComponent>(this, TEXT("BossDriver"));
		BossDriver->RegisterComponent();
	}
	BossDriver->SetBossData(InBossData);
}

void ATVMobCharacter::SetOwningSpawnPoint(ATVSpawnPoint* InSpawnPoint)
{
	OwningSpawnPoint = InSpawnPoint;
}

void ATVMobCharacter::HandleDeath(AActor* Killer)
{
	if (!HasAuthority() || IsDead())
	{
		return;
	}
	Super::HandleDeath(Killer);

	// Eligibility is computed from the threat table, so broadcast before clearing it.
	if (UTVCombatEventsSubsystem* Events = UTVCombatEventsSubsystem::Get(this))
	{
		Events->BroadcastMobKilled(this);
	}
	if (BossDriver)
	{
		BossDriver->ResetEncounter();
	}
	if (ThreatComponent)
	{
		ThreatComponent->ClearThreat();
	}
	if (ATVSpawnPoint* SpawnPoint = OwningSpawnPoint.Get())
	{
		SpawnPoint->NotifyMobDied(this);
	}
	if (ATVMobAIController* AI = Cast<ATVMobAIController>(GetController()))
	{
		AI->StopMovement();
	}
	SetLifeSpan(FMath::Max(1.f, CorpseLifetime));
}

ATVMobCharacter* ATVMobCharacter::SpawnMob(UWorld* World, TSubclassOf<ATVMobCharacter> MobClass, FName InMobId, int32 Level,
	const FTransform& Transform, float HealthScale, float DamageScale, ULevel* OverrideLevel)
{
	if (!World || !UTVCombatLibrary::FindMobRow(InMobId))
	{
		return nullptr;
	}
	UClass* Class = MobClass ? MobClass.Get() : ATVMobCharacter::StaticClass();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	Params.OverrideLevel = OverrideLevel;
	ATVMobCharacter* Mob = World->SpawnActor<ATVMobCharacter>(Class, Transform, Params);
	if (Mob)
	{
		Mob->SetHomeLocation(Transform.GetLocation());
		Mob->InitFromMobRow(InMobId, Level, HealthScale, DamageScale);
	}
	return Mob;
}
