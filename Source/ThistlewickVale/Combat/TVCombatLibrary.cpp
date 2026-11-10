#include "Combat/TVCombatLibrary.h"
#include "ThistlewickVale.h"
#include "Core/TVProjectSettings.h"
#include "Combat/TVAbilitySystemComponent.h"
#include "Combat/TVAttributeSets.h"
#include "Combat/TVCharacterBase.h"
#include "Combat/TVPlayerState.h"
#include "Combat/TVCombatTags.h"
#include "Combat/TVCombatEventsSubsystem.h"
#include "Combat/Effects/TVGameplayEffects.h"
#include "Combat/AI/TVThreatComponent.h"
#include "Combat/AI/TVMobAIController.h"
#include "Combat/AI/TVBossDriverComponent.h"
#include "Net/TVPartySubsystem.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "CollisionQueryParams.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
	UDataTable* GetCachedTable(FName Key)
	{
		static TMap<FName, TWeakObjectPtr<UDataTable>> Cache;
		if (TWeakObjectPtr<UDataTable>* Found = Cache.Find(Key))
		{
			if (Found->IsValid())
			{
				return Found->Get();
			}
		}
		const UTVProjectSettings* Settings = UTVProjectSettings::Get();
		UDataTable* Table = Settings ? Settings->LoadTable(Key) : nullptr;
		if (Table)
		{
			Cache.Add(Key, Table);
		}
		return Table;
	}

	template <typename RowType>
	const RowType* FindRowById(FName TableKey, FName Id, FName RowType::*IdField)
	{
		if (Id.IsNone())
		{
			return nullptr;
		}
		UDataTable* Table = GetCachedTable(TableKey);
		if (!Table)
		{
			return nullptr;
		}
		if (const RowType* Row = Table->FindRow<RowType>(Id, TEXT("TVCombat"), /*bWarnIfRowMissing*/ false))
		{
			return Row;
		}
		// Fallback: row name differs from the id field.
		TArray<RowType*> Rows;
		Table->GetAllRows<RowType>(TEXT("TVCombat"), Rows);
		for (const RowType* Row : Rows)
		{
			if (Row && Row->*IdField == Id)
			{
				return Row;
			}
		}
		return nullptr;
	}

	const UTVAttributeSet_Combat* GetCombatSet(const UAbilitySystemComponent* ASC)
	{
		return ASC ? ASC->GetSet<UTVAttributeSet_Combat>() : nullptr;
	}

	float GetAttr(const UAbilitySystemComponent* ASC, const FGameplayAttribute& Attribute, float Default)
	{
		if (!ASC || !ASC->HasAttributeSetForAttribute(Attribute))
		{
			return Default;
		}
		return ASC->GetNumericAttribute(Attribute);
	}

	ATVMobAIController* GetMobAI(AActor* Actor)
	{
		const APawn* Pawn = Cast<APawn>(Actor);
		return Pawn ? Cast<ATVMobAIController>(Pawn->GetController()) : nullptr;
	}

	/** Breaks roots that break on damage once their grace period passed. */
	void BreakRootsOnDamage(UAbilitySystemComponent* ASC)
	{
		const FGameplayTag RootTag = TVTags::Debuff_Root();
		if (!ASC || !RootTag.IsValid() || !ASC->HasMatchingGameplayTag(RootTag))
		{
			return;
		}
		const UWorld* World = ASC->GetWorld();
		const float Now = World ? World->GetTimeSeconds() : 0.f;
		const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(RootTag));
		const TArray<FActiveGameplayEffectHandle> Handles = ASC->GetActiveEffects(Query);
		for (const FActiveGameplayEffectHandle& Handle : Handles)
		{
			const FActiveGameplayEffect* Active = ASC->GetActiveGameplayEffect(Handle);
			if (!Active)
			{
				continue;
			}
			const float Grace = Active->Spec.GetSetByCallerMagnitude(TVSetByCaller::BreakOnDamageAfter, /*Warn*/ false, 0.f);
			if (Grace > 0.f && Now - Active->StartWorldTime >= Grace)
			{
				ASC->RemoveActiveGameplayEffect(Handle);
			}
		}
	}

	void AddRage(UAbilitySystemComponent* ASC, float Amount)
	{
		const UTVAbilitySystemComponent* TVASC = Cast<UTVAbilitySystemComponent>(ASC);
		if (!TVASC || TVASC->GetResourceType() != ETVResource::Rage || Amount <= 0.f)
		{
			return;
		}
		const FGameplayAttribute Resource = UTVAttributeSet_Vitals::GetResourceAttribute();
		const float Max = GetAttr(ASC, UTVAttributeSet_Vitals::GetMaxResourceAttribute(), 100.f);
		const float Current = ASC->GetNumericAttributeBase(Resource);
		ASC->SetNumericAttributeBase(Resource, FMath::Clamp(Current + Amount, 0.f, Max));
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Data
// ---------------------------------------------------------------------------------------------------------------------

const FTVAbilityRow* UTVCombatLibrary::FindAbilityRow(FName AbilityId)
{
	return FindRowById<FTVAbilityRow>(TEXT("Abilities"), AbilityId, &FTVAbilityRow::AbilityId);
}

const FTVMobRow* UTVCombatLibrary::FindMobRow(FName MobId)
{
	return FindRowById<FTVMobRow>(TEXT("Mobs"), MobId, &FTVMobRow::MobId);
}

const FTVClassRow* UTVCombatLibrary::FindClassRow(FName ClassId)
{
	return FindRowById<FTVClassRow>(TEXT("Classes"), ClassId, &FTVClassRow::ClassId);
}

const FTVClassLevelStatsRow* UTVCombatLibrary::FindClassLevelStats(FName CurveOrClassId, int32 Level)
{
	UDataTable* Table = GetCachedTable(TEXT("ClassBaseStats"));
	if (!Table || CurveOrClassId.IsNone())
	{
		return nullptr;
	}
	const FName RowName(*FString::Printf(TEXT("%s_L%d"), *CurveOrClassId.ToString(), Level));
	if (const FTVClassLevelStatsRow* Row = Table->FindRow<FTVClassLevelStatsRow>(RowName, TEXT("TVCombat"), false))
	{
		return Row;
	}
	TArray<FTVClassLevelStatsRow*> Rows;
	Table->GetAllRows<FTVClassLevelStatsRow>(TEXT("TVCombat"), Rows);
	const FTVClassLevelStatsRow* Best = nullptr;
	for (const FTVClassLevelStatsRow* Row : Rows)
	{
		if (!Row || Row->ClassId != CurveOrClassId)
		{
			continue;
		}
		if (Row->Level == Level)
		{
			return Row;
		}
		// Above the table: use the highest level row.
		if (Row->Level < Level && (!Best || Row->Level > Best->Level))
		{
			Best = Row;
		}
	}
	return Best;
}

int32 UTVCombatLibrary::GetXPToNext(int32 Level)
{
	UDataTable* Table = GetCachedTable(TEXT("XPCurve"));
	if (!Table)
	{
		return 0;
	}
	TArray<FTVXPCurveRow*> Rows;
	Table->GetAllRows<FTVXPCurveRow>(TEXT("TVCombat"), Rows);
	for (const FTVXPCurveRow* Row : Rows)
	{
		if (Row && Row->Level == Level)
		{
			return FMath::Max(0, Row->XPToNext);
		}
	}
	return 0;
}

int32 UTVCombatLibrary::GetMaxLevel()
{
	UDataTable* Table = GetCachedTable(TEXT("XPCurve"));
	if (!Table)
	{
		return 10;
	}
	TArray<FTVXPCurveRow*> Rows;
	Table->GetAllRows<FTVXPCurveRow>(TEXT("TVCombat"), Rows);
	int32 MaxLevel = 1;
	for (const FTVXPCurveRow* Row : Rows)
	{
		if (Row && Row->XPToNext > 0)
		{
			MaxLevel = FMath::Max(MaxLevel, Row->Level + 1);
		}
	}
	return MaxLevel;
}

const FTVEffectSpec* UTVCombatLibrary::FindEffectOfClass(const FTVAbilityRow& Row, const UClass* EffectClass)
{
	if (!EffectClass)
	{
		return nullptr;
	}
	for (const FTVEffectSpec& Spec : Row.Effects)
	{
		// Compare by path so the class does not have to be loaded (native classes always are).
		const FSoftObjectPath Path = Spec.EffectClass.ToSoftObjectPath();
		if (Path == FSoftObjectPath(EffectClass))
		{
			return &Spec;
		}
		if (const UClass* Loaded = Spec.EffectClass.Get())
		{
			if (Loaded->IsChildOf(EffectClass))
			{
				return &Spec;
			}
		}
	}
	return nullptr;
}

// ---------------------------------------------------------------------------------------------------------------------
// Actors
// ---------------------------------------------------------------------------------------------------------------------

UTVAbilitySystemComponent* UTVCombatLibrary::GetTVASC(const AActor* Actor)
{
	if (!Actor)
	{
		return nullptr;
	}
	if (const ATVCharacterBase* Character = Cast<ATVCharacterBase>(Actor))
	{
		return Character->GetTVAbilitySystemComponent();
	}
	if (const ATVPlayerState* PS = Cast<ATVPlayerState>(Actor))
	{
		return PS->GetTVAbilitySystemComponent();
	}
	return Cast<UTVAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor));
}

bool UTVCombatLibrary::IsAlive(const AActor* Actor)
{
	const ATVCharacterBase* Character = Cast<ATVCharacterBase>(Actor);
	return Character && IsValid(Character) && !Character->IsDead();
}

bool UTVCombatLibrary::IsPlayerSide(const AActor* Actor)
{
	return Actor && (Actor->IsA<ATVPlayerCharacter>() || Actor->IsA<ATVPlayerState>());
}

bool UTVCombatLibrary::IsHostile(const AActor* A, const AActor* B)
{
	if (!A || !B || A == B)
	{
		return false;
	}
	const bool bAPlayer = IsPlayerSide(A);
	const bool bBPlayer = IsPlayerSide(B);
	const bool bAMob = A->IsA<ATVMobCharacter>();
	const bool bBMob = B->IsA<ATVMobCharacter>();
	return (bAPlayer && bBMob) || (bAMob && bBPlayer);
}

int32 UTVCombatLibrary::GetCombatLevel(const AActor* Actor)
{
	if (const ATVCharacterBase* Character = Cast<ATVCharacterBase>(Actor))
	{
		return FMath::Max(1, Character->GetCharacterLevel());
	}
	if (const ATVPlayerState* PS = Cast<ATVPlayerState>(Actor))
	{
		return FMath::Max(1, PS->Level);
	}
	return 1;
}

bool UTVCombatLibrary::HasLineOfSight(const AActor* From, const AActor* To)
{
	if (!From || !To)
	{
		return false;
	}
	const APawn* ToPawn = Cast<APawn>(To);
	const FVector Target = ToPawn ? ToPawn->GetPawnViewLocation() : To->GetActorLocation();
	return HasLineOfSightToLocation(From, Target);
}

bool UTVCombatLibrary::HasLineOfSightToLocation(const AActor* From, const FVector& Location)
{
	UWorld* World = From ? From->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}
	const APawn* FromPawn = Cast<APawn>(From);
	const FVector Start = FromPawn ? FromPawn->GetPawnViewLocation() : From->GetActorLocation();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TVLineOfSight), /*bTraceComplex*/ false, From);
	// Only static world geometry blocks sight; pawns and dynamic props do not.
	FHitResult Hit;
	return !World->LineTraceSingleByObjectType(Hit, Start, Location, FCollisionObjectQueryParams(ECC_WorldStatic), Params);
}

void UTVCombatLibrary::GetHostilesInRadius(const AActor* Instigator, const FVector& Center, float Radius, TArray<AActor*>& OutActors)
{
	UWorld* World = Instigator ? Instigator->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}
	const float RadiusSq = FMath::Square(Radius);
	for (TActorIterator<ATVCharacterBase> It(World); It; ++It)
	{
		ATVCharacterBase* Character = *It;
		if (!IsAlive(Character) || !IsHostile(Instigator, Character))
		{
			continue;
		}
		if (FVector::DistSquared(Character->GetActorLocation(), Center) <= RadiusSq)
		{
			OutActors.AddUnique(Character);
		}
	}
}

void UTVCombatLibrary::GetFriendliesInRadius(const AActor* Instigator, const FVector& Center, float Radius, TArray<AActor*>& OutActors)
{
	UWorld* World = Instigator ? Instigator->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}
	const float RadiusSq = FMath::Square(Radius);
	auto AddIfInside = [&](AActor* Actor)
	{
		if (IsAlive(Actor) && FVector::DistSquared(Actor->GetActorLocation(), Center) <= RadiusSq)
		{
			OutActors.AddUnique(Actor);
		}
	};

	if (const ATVPlayerCharacter* Player = Cast<ATVPlayerCharacter>(Instigator))
	{
		AddIfInside(const_cast<ATVPlayerCharacter*>(Player));
		ATVPlayerState* PS = Player->GetTVPlayerState();
		UTVPartySubsystem* Party = UTVPartySubsystem::Get(Instigator);
		if (PS && Party)
		{
			for (ATVPlayerState* Member : Party->GetPartyMembers(PS))
			{
				if (Member)
				{
					AddIfInside(Member->GetPawn());
				}
			}
		}
		return;
	}

	if (Instigator->IsA<ATVMobCharacter>())
	{
		for (TActorIterator<ATVMobCharacter> It(World); It; ++It)
		{
			AddIfInside(*It);
		}
	}
}

bool UTVCombatLibrary::IsInFrontOf(const AActor* Defender, const AActor* Attacker, float HalfAngleDeg)
{
	if (!Defender || !Attacker)
	{
		return false;
	}
	const FVector ToAttacker = (Attacker->GetActorLocation() - Defender->GetActorLocation()).GetSafeNormal2D();
	if (ToAttacker.IsNearlyZero())
	{
		return true;
	}
	const FVector Forward = Defender->GetActorForwardVector().GetSafeNormal2D();
	return FVector::DotProduct(Forward, ToAttacker) >= FMath::Cos(FMath::DegreesToRadians(HalfAngleDeg));
}

float UTVCombatLibrary::DistanceBetween(const AActor* A, const AActor* B)
{
	if (!A || !B)
	{
		return TNumericLimits<float>::Max();
	}
	return FVector::Dist(A->GetActorLocation(), B->GetActorLocation());
}

// ---------------------------------------------------------------------------------------------------------------------
// Formulas (Docs/BALANCE.md section 1)
// ---------------------------------------------------------------------------------------------------------------------

float UTVCombatLibrary::HastePercent(float HasteRating)
{
	return FMath::Max(0.f, HasteRating) / TVCombatTuning::RatingPerPercent;
}

float UTVCombatLibrary::ComputeGCD(float HasteRating)
{
	const float Scaled = TVCombatTuning::GlobalCooldown / (1.f + HastePercent(HasteRating) / 100.f);
	return FMath::Max(TVCombatTuning::GlobalCooldownFloor, Scaled);
}

float UTVCombatLibrary::ComputeCastTime(float BaseCastTime, float HasteRating)
{
	if (BaseCastTime <= 0.f)
	{
		return 0.f;
	}
	return BaseCastTime / (1.f + HastePercent(HasteRating) / 100.f);
}

float UTVCombatLibrary::ComputeCritChance(float CritRating, float BonusFraction)
{
	const float Chance = TVCombatTuning::BaseCritChance + (FMath::Max(0.f, CritRating) / TVCombatTuning::RatingPerPercent) / 100.f + BonusFraction;
	return FMath::Clamp(Chance, 0.f, 1.f);
}

float UTVCombatLibrary::ComputeArmorMitigation(float Armor, int32 AttackerLevel)
{
	if (Armor <= 0.f)
	{
		return 0.f;
	}
	const float Denominator = Armor + TVCombatTuning::ArmorPerLevel * FMath::Max(1, AttackerLevel) + TVCombatTuning::ArmorConstant;
	return FMath::Clamp(Armor / Denominator, 0.f, TVCombatTuning::MaxMitigation);
}

float UTVCombatLibrary::GetStatValue(const UAbilitySystemComponent* ASC, ETVStat Stat)
{
	switch (Stat)
	{
	case ETVStat::Stamina:   return GetAttr(ASC, UTVAttributeSet_Stats::GetStaminaAttribute(), 0.f);
	case ETVStat::Strength:  return GetAttr(ASC, UTVAttributeSet_Stats::GetStrengthAttribute(), 0.f);
	case ETVStat::Agility:   return GetAttr(ASC, UTVAttributeSet_Stats::GetAgilityAttribute(), 0.f);
	case ETVStat::Intellect: return GetAttr(ASC, UTVAttributeSet_Stats::GetIntellectAttribute(), 0.f);
	case ETVStat::Spirit:    return GetAttr(ASC, UTVAttributeSet_Stats::GetSpiritAttribute(), 0.f);
	case ETVStat::Crit:      return GetAttr(ASC, UTVAttributeSet_Stats::GetCritAttribute(), 0.f);
	case ETVStat::Haste:     return GetAttr(ASC, UTVAttributeSet_Stats::GetHasteAttribute(), 0.f);
	case ETVStat::Armor:     return GetAttr(ASC, UTVAttributeSet_Stats::GetArmorAttribute(), 0.f);
	default:                 return 0.f;
	}
}

float UTVCombatLibrary::GetHitDamage(const AActor* Actor)
{
	const ATVMobCharacter* Mob = Cast<ATVMobCharacter>(Actor);
	return Mob ? Mob->GetHitDamage() : 0.f;
}

float UTVCombatLibrary::ComputeAbilityMagnitude(const AActor* Source, const FTVAbilityRow& Row)
{
	if (Source && Source->IsA<ATVMobCharacter>())
	{
		// Mob damage abilities: multiple of the current hit damage.
		return Row.BaseMagnitude * GetHitDamage(Source);
	}
	const UTVAbilitySystemComponent* ASC = GetTVASC(Source);
	float Magnitude = Row.BaseMagnitude;
	if (Row.CoefficientStat != ETVStat::None)
	{
		Magnitude += GetStatValue(ASC, Row.CoefficientStat) * Row.Coefficient;
	}
	if (Row.SlotKind == ETVAbilitySlot::Ultimate)
	{
		Magnitude *= GetAttr(ASC, UTVAttributeSet_Combat::GetUltimatePowerMultiplierAttribute(), 1.f);
	}
	return FMath::Max(0.f, Magnitude);
}

FText UTVCombatLibrary::FormatAbilityTooltip(const FTVAbilityRow& Row, float Magnitude)
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("Magnitude"), FText::AsNumber(FMath::RoundToInt(Magnitude)));
	Args.Add(TEXT("Cooldown"), FText::AsNumber(FMath::RoundToInt(Row.Cooldown)));
	return FText::Format(Row.Tooltip, Args);
}

// ---------------------------------------------------------------------------------------------------------------------
// Server pipeline
// ---------------------------------------------------------------------------------------------------------------------

float UTVCombatLibrary::DealDamage(AActor* Source, AActor* Target, float Amount, float ThreatMultiplier, bool bCanCrit)
{
	if (!Target || !Target->HasAuthority() || Amount <= 0.f || !IsAlive(Target))
	{
		return 0.f;
	}
	UTVAbilitySystemComponent* TargetASC = GetTVASC(Target);
	UTVAbilitySystemComponent* SourceASC = GetTVASC(Source);
	UAbilitySystemComponent* SpecMaker = SourceASC ? SourceASC : TargetASC;
	if (!TargetASC || !SpecMaker)
	{
		return 0.f;
	}

	float Final = Amount * GetAttr(SourceASC, UTVAttributeSet_Combat::GetDamageDoneMultiplierAttribute(), 1.f);
	bool bCrit = false;
	if (bCanCrit && SourceASC)
	{
		const float Chance = ComputeCritChance(GetStatValue(SourceASC, ETVStat::Crit),
			GetAttr(SourceASC, UTVAttributeSet_Combat::GetCritChanceBonusAttribute(), 0.f));
		bCrit = FMath::FRand() < Chance;
		if (bCrit)
		{
			Final *= TVCombatTuning::CritMultiplier;
		}
	}

	FGameplayEffectContextHandle Context = SpecMaker->MakeEffectContext();
	if (!SourceASC && Source)
	{
		Context.AddInstigator(Source, Source);
	}
	FGameplayEffectSpecHandle SpecHandle = SpecMaker->MakeOutgoingSpec(UGE_TV_Damage::StaticClass(), 1.f, Context);
	if (!SpecHandle.IsValid())
	{
		return 0.f;
	}
	SpecHandle.Data->SetSetByCallerMagnitude(TVSetByCaller::Damage, Final);
	SpecHandle.Data->SetSetByCallerMagnitude(TVSetByCaller::Threat, ThreatMultiplier);
	if (bCrit && TVTags::Hit_Crit().IsValid())
	{
		SpecHandle.Data->AddDynamicAssetTag(TVTags::Hit_Crit());
	}
	SpecMaker->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	return Final;
}

float UTVCombatLibrary::DealHeal(AActor* Source, AActor* Target, float Amount, float ThreatMultiplier, bool bCanCrit)
{
	if (!Target || !Target->HasAuthority() || Amount <= 0.f || !IsAlive(Target))
	{
		return 0.f;
	}
	UTVAbilitySystemComponent* TargetASC = GetTVASC(Target);
	UTVAbilitySystemComponent* SourceASC = GetTVASC(Source);
	UAbilitySystemComponent* SpecMaker = SourceASC ? SourceASC : TargetASC;
	if (!TargetASC || !SpecMaker)
	{
		return 0.f;
	}

	float Final = Amount * GetAttr(SourceASC, UTVAttributeSet_Combat::GetHealingDoneMultiplierAttribute(), 1.f);
	bool bCrit = false;
	if (bCanCrit && SourceASC)
	{
		const float Chance = ComputeCritChance(GetStatValue(SourceASC, ETVStat::Crit),
			GetAttr(SourceASC, UTVAttributeSet_Combat::GetCritChanceBonusAttribute(), 0.f));
		bCrit = FMath::FRand() < Chance;
		if (bCrit)
		{
			Final *= TVCombatTuning::CritMultiplier;
		}
	}

	FGameplayEffectContextHandle Context = SpecMaker->MakeEffectContext();
	FGameplayEffectSpecHandle SpecHandle = SpecMaker->MakeOutgoingSpec(UGE_TV_Heal::StaticClass(), 1.f, Context);
	if (!SpecHandle.IsValid())
	{
		return 0.f;
	}
	SpecHandle.Data->SetSetByCallerMagnitude(TVSetByCaller::Heal, Final);
	SpecHandle.Data->SetSetByCallerMagnitude(TVSetByCaller::Threat, ThreatMultiplier);
	if (bCrit && TVTags::Hit_Crit().IsValid())
	{
		SpecHandle.Data->AddDynamicAssetTag(TVTags::Hit_Crit());
	}
	SpecMaker->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	return Final;
}

FActiveGameplayEffectHandle UTVCombatLibrary::ApplyEffectSpec(AActor* Source, AActor* Target, const FTVEffectSpec& EffectSpec,
	const FTVAbilityRow& Row, float AbilityMagnitude)
{
	FActiveGameplayEffectHandle Result;
	UClass* EffectClass = EffectSpec.EffectClass.LoadSynchronous();
	if (!EffectClass || !EffectClass->IsChildOf(UGameplayEffect::StaticClass()))
	{
		UE_LOG(LogTV, Warning, TEXT("Ability %s: effect class '%s' not found"), *Row.AbilityId.ToString(), *EffectSpec.EffectClass.ToString());
		return Result;
	}
	const UTVGameplayEffect* Def = Cast<UTVGameplayEffect>(EffectClass->GetDefaultObject());
	if (Def && Def->Role == ETVEffectRole::Parameter)
	{
		return Result;
	}

	AActor* Receiver = (Def && Def->bAppliesToSource) ? Source : Target;
	if (!Receiver || !Receiver->HasAuthority() || !IsAlive(Receiver))
	{
		return Result;
	}
	UTVAbilitySystemComponent* ReceiverASC = GetTVASC(Receiver);
	UTVAbilitySystemComponent* SourceASC = GetTVASC(Source);
	if (!ReceiverASC)
	{
		return Result;
	}

	// Duration: 0 -> class default; negative -> "until removed" (one hour).
	float Duration = EffectSpec.Duration;
	if (Duration == 0.f)
	{
		Duration = Def ? Def->DefaultDuration : 5.f;
	}
	if (Duration < 0.f)
	{
		Duration = 3600.f;
	}

	const float DataMagnitude = EffectSpec.Magnitude;
	float Magnitude = DataMagnitude;
	const ETVMagnitudeMode Mode = Def ? Def->MagnitudeMode : ETVMagnitudeMode::Raw;
	switch (Mode)
	{
	case ETVMagnitudeMode::OnePlus:
		Magnitude = 1.f + DataMagnitude;
		break;
	case ETVMagnitudeMode::Negate:
		Magnitude = -DataMagnitude;
		break;
	case ETVMagnitudeMode::TimesHitDamage:
		Magnitude = DataMagnitude * ((Source && Source->IsA<ATVMobCharacter>()) ? GetHitDamage(Source) : AbilityMagnitude);
		break;
	case ETVMagnitudeMode::AbilityMagnitudePerTick:
		if (!FMath::IsNearlyEqual(DataMagnitude, 1.f) && DataMagnitude != 0.f)
		{
			UE_LOG(LogTV, Warning, TEXT("Ability %s: HoT tick interval %.2f not supported, using 1 s"), *Row.AbilityId.ToString(), DataMagnitude);
		}
		Magnitude = AbilityMagnitude;
		break;
	case ETVMagnitudeMode::TotalOverDuration:
		Magnitude = DataMagnitude / FMath::Max(1.f, FMath::RoundToFloat(Duration));
		break;
	case ETVMagnitudeMode::DrainFractionOfMaxResource:
		if (ReceiverASC->GetResourceType() != ETVResource::Mana)
		{
			return Result;
		}
		Magnitude = -(DataMagnitude <= 1.f
			? DataMagnitude * GetAttr(ReceiverASC, UTVAttributeSet_Vitals::GetMaxResourceAttribute(), 0.f)
			: DataMagnitude);
		break;
	default:
		break;
	}

	// ---- Hooks that act instead of (or before) the effect ----
	const ETVEffectHook Hook = Def ? Def->Hook : ETVEffectHook::None;
	switch (Hook)
	{
	case ETVEffectHook::AbsorbShield:
	{
		const float MaxHealth = GetAttr(ReceiverASC, UTVAttributeSet_Vitals::GetMaxHealthAttribute(), 0.f);
		const float Shield = DataMagnitude <= 1.f ? DataMagnitude * MaxHealth : DataMagnitude;
		const FGameplayAttribute ShieldAttr = UTVAttributeSet_Combat::GetAbsorbShieldAttribute();
		if (ReceiverASC->HasAttributeSetForAttribute(ShieldAttr))
		{
			ReceiverASC->SetNumericAttributeBase(ShieldAttr, ReceiverASC->GetNumericAttributeBase(ShieldAttr) + Shield);
		}
		break;
	}
	case ETVEffectHook::Knockback:
	{
		if (ACharacter* Character = Cast<ACharacter>(Receiver))
		{
			FVector Dir = Source ? (Receiver->GetActorLocation() - Source->GetActorLocation()).GetSafeNormal2D() : -Receiver->GetActorForwardVector();
			if (Dir.IsNearlyZero())
			{
				Dir = -Receiver->GetActorForwardVector();
			}
			// Airtime about 0.6 s with Z 300 cm/s; horizontal speed covers DataMagnitude cm.
			const float AirTime = 0.6f;
			Character->LaunchCharacter(Dir * (DataMagnitude / AirTime) + FVector(0.f, 0.f, 300.f), true, true);
		}
		return Result;
	}
	case ETVEffectHook::Interrupt:
		ReceiverASC->InterruptCast(Source, false);
		return Result;
	case ETVEffectHook::Dispel:
	{
		const int32 Count = FMath::Max(1, FMath::RoundToInt(DataMagnitude));
		FGameplayTagContainer DispelTags;
		if (TVTags::Debuff_Magic().IsValid()) { DispelTags.AddTag(TVTags::Debuff_Magic()); }
		if (TVTags::Debuff_Curse().IsValid()) { DispelTags.AddTag(TVTags::Debuff_Curse()); }
		if (!DispelTags.IsEmpty())
		{
			for (int32 i = 0; i < Count; ++i)
			{
				const TArray<FActiveGameplayEffectHandle> Handles = ReceiverASC->GetActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(DispelTags));
				if (Handles.Num() == 0)
				{
					break;
				}
				ReceiverASC->RemoveActiveGameplayEffect(Handles[0]);
			}
		}
		return Result;
	}
	case ETVEffectHook::SocialAggro:
	{
		ATVMobCharacter* Caller = Cast<ATVMobCharacter>(Source);
		if (!Caller)
		{
			return Result;
		}
		AActor* Enemy = Caller->GetCombatTarget();
		if (!Enemy && Caller->GetThreatComponent())
		{
			Enemy = Caller->GetThreatComponent()->GetTopThreatActor();
		}
		TArray<AActor*> Friends;
		GetFriendliesInRadius(Caller, Caller->GetActorLocation(), FMath::Max(Row.RadiusCm, 500.f), Friends);
		for (AActor* Friend : Friends)
		{
			if (Friend != Caller && Enemy)
			{
				if (ATVMobAIController* AI = GetMobAI(Friend))
				{
					AI->EngageTarget(Enemy);
				}
			}
		}
		return Result;
	}
	case ETVEffectHook::SummonAdds:
	{
		FString TagString = EffectSpec.SetByCallerTag.ToString();
		FString MobIdString;
		if (!TagString.Split(TEXT("TV.Spawn."), nullptr, &MobIdString) || MobIdString.IsEmpty())
		{
			UE_LOG(LogTV, Warning, TEXT("Ability %s: SummonAdds needs SetByCallerTag TV.Spawn.<MOB_ID>"), *Row.AbilityId.ToString());
			return Result;
		}
		SummonAdds(Source, FName(*MobIdString), FMath::Max(1, FMath::RoundToInt(DataMagnitude)));
		return Result;
	}
	case ETVEffectHook::Taunt:
	{
		if (ATVMobCharacter* Mob = Cast<ATVMobCharacter>(Receiver))
		{
			if (UTVThreatComponent* Threat = Mob->GetThreatComponent())
			{
				Threat->Taunt(Source, DataMagnitude > 0.f ? DataMagnitude : 1.1f, Duration);
			}
			if (ATVMobAIController* AI = GetMobAI(Mob))
			{
				AI->EngageTarget(Source);
			}
		}
		break;
	}
	default:
		break;
	}

	if (Def && Def->DurationPolicy == EGameplayEffectDurationType::Instant && Def->Modifiers.Num() == 0)
	{
		return Result;
	}

	UAbilitySystemComponent* SpecMaker = SourceASC ? static_cast<UAbilitySystemComponent*>(SourceASC) : static_cast<UAbilitySystemComponent*>(ReceiverASC);
	FGameplayEffectContextHandle Context = SpecMaker->MakeEffectContext();
	FGameplayEffectSpecHandle SpecHandle = SpecMaker->MakeOutgoingSpec(EffectClass, 1.f, Context);
	if (!SpecHandle.IsValid())
	{
		return Result;
	}
	FGameplayEffectSpec& Spec = *SpecHandle.Data.Get();
	Spec.SetSetByCallerMagnitude(TVSetByCaller::Duration, Duration);
	Spec.SetSetByCallerMagnitude(TVSetByCaller::Magnitude, Magnitude);
	Spec.SetSetByCallerMagnitude(TVSetByCaller::BreakOnDamageAfter, Hook == ETVEffectHook::BreakOnDamage ? DataMagnitude : 0.f);
	if (EffectSpec.SetByCallerTag.IsValid())
	{
		Spec.SetSetByCallerMagnitude(EffectSpec.SetByCallerTag, Magnitude);
	}
	if (Def)
	{
		for (const FName& TagName : Def->GrantedTagNames)
		{
			const FGameplayTag Tag = TVTags::Get(*TagName.ToString());
			if (Tag.IsValid())
			{
				Spec.DynamicGrantedTags.AddTag(Tag);
			}
		}
		if (Def->bIsDebuff && TVTags::Debuff().IsValid())
		{
			Spec.DynamicGrantedTags.AddTag(TVTags::Debuff());
		}
	}
	// The ability tag on the effect lets the UI map buffs/debuffs to the ability icon.
	if (Row.AbilityTag.IsValid() && Def && Def->DurationPolicy != EGameplayEffectDurationType::Instant)
	{
		Spec.DynamicGrantedTags.AddTag(Row.AbilityTag);
	}
	if (Row.AbilityTag.IsValid())
	{
		Spec.AddDynamicAssetTag(Row.AbilityTag);
	}

	Result = SpecMaker->ApplyGameplayEffectSpecToTarget(Spec, ReceiverASC);
	return Result;
}

void UTVCombatLibrary::ApplyRowEffects(AActor* Source, AActor* Target, const FTVAbilityRow& Row, float AbilityMagnitude)
{
	for (const FTVEffectSpec& Effect : Row.Effects)
	{
		ApplyEffectSpec(Source, Target, Effect, Row, AbilityMagnitude);
	}
}

void UTVCombatLibrary::ResolveIncomingDamage(UAbilitySystemComponent* TargetASC, const FGameplayEffectSpec& Spec, float RawDamage)
{
	if (!TargetASC || RawDamage <= 0.f)
	{
		return;
	}
	ATVCharacterBase* Target = Cast<ATVCharacterBase>(TargetASC->GetAvatarActor());
	if (!Target || !Target->HasAuthority() || Target->IsDead())
	{
		return;
	}

	const FGameplayEffectContextHandle& Context = Spec.GetContext();
	AActor* Source = Context.GetEffectCauser();
	if (!Source)
	{
		Source = Context.GetInstigator();
	}
	if (ATVPlayerState* SourcePS = Cast<ATVPlayerState>(Source))
	{
		Source = SourcePS->GetPawn();
	}
	UAbilitySystemComponent* SourceASC = Context.GetInstigatorAbilitySystemComponent();

	if (TVTags::State_Evading().IsValid() && TargetASC->HasMatchingGameplayTag(TVTags::State_Evading()))
	{
		Target->MulticastCombatText(0.f, ETVCombatTextKind::Immune, Source);
		return;
	}

	const bool bPeriodic = Spec.GetPeriod() > 0.f || (TVTags::Hit_Periodic().IsValid() && Spec.GetDynamicAssetTags().HasTagExact(TVTags::Hit_Periodic()));
	const bool bCrit = TVTags::Hit_Crit().IsValid() && Spec.GetDynamicAssetTags().HasTagExact(TVTags::Hit_Crit());

	// Dodge (direct hits only).
	const float Dodge = GetAttr(TargetASC, UTVAttributeSet_Combat::GetDodgeChanceAttribute(), 0.f);
	if (!bPeriodic && Dodge > 0.f && FMath::FRand() < Dodge)
	{
		Target->MulticastCombatText(0.f, ETVCombatTextKind::Dodge, Source);
		return;
	}

	float Damage = RawDamage;

	// Armor.
	const float ArmorPct = GetAttr(TargetASC, UTVAttributeSet_Combat::GetArmorPctModifierAttribute(), 0.f);
	const float Armor = FMath::Max(0.f, GetStatValue(TargetASC, ETVStat::Armor) * (1.f + ArmorPct));
	Damage *= 1.f - ComputeArmorMitigation(Armor, GetCombatLevel(Source));

	// Damage taken modifiers.
	Damage *= GetAttr(TargetASC, UTVAttributeSet_Combat::GetDamageTakenMultiplierAttribute(), 1.f);
	const float FrontalMod = GetAttr(TargetASC, UTVAttributeSet_Combat::GetFrontalDamageTakenModAttribute(), 0.f);
	if (FrontalMod != 0.f && Source && IsInFrontOf(Target, Source, TVCombatTuning::FrontalHalfAngleDeg))
	{
		Damage *= FMath::Max(0.f, 1.f + FrontalMod);
	}
	Damage = FMath::Max(0.f, Damage);
	const float DamageForThreat = Damage;

	// Absorb shield.
	const FGameplayAttribute ShieldAttr = UTVAttributeSet_Combat::GetAbsorbShieldAttribute();
	float Absorbed = 0.f;
	if (TargetASC->HasAttributeSetForAttribute(ShieldAttr))
	{
		const float Shield = TargetASC->GetNumericAttributeBase(ShieldAttr);
		if (Shield > 0.f)
		{
			Absorbed = FMath::Min(Shield, Damage);
			Damage -= Absorbed;
			TargetASC->SetNumericAttributeBase(ShieldAttr, Shield - Absorbed);
			if (Shield - Absorbed <= KINDA_SMALL_NUMBER && TVTags::Buff_Absorb().IsValid())
			{
				TargetASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(TVTags::Buff_Absorb()));
			}
		}
	}

	// Health.
	const FGameplayAttribute HealthAttr = UTVAttributeSet_Vitals::GetHealthAttribute();
	const float OldHealth = TargetASC->GetNumericAttribute(HealthAttr);
	const float NewHealth = FMath::Max(0.f, OldHealth - Damage);
	if (Damage > 0.f)
	{
		TargetASC->SetNumericAttributeBase(HealthAttr, NewHealth);
	}

	// Threat and engagement.
	const float SpecThreat = Spec.GetSetByCallerMagnitude(TVSetByCaller::Threat, false, 1.f);
	if (ATVMobCharacter* MobTarget = Cast<ATVMobCharacter>(Target))
	{
		if (Source && IsHostile(MobTarget, Source))
		{
			const float SourceThreatMult = GetAttr(SourceASC, UTVAttributeSet_Combat::GetThreatMultiplierAttribute(), 1.f);
			if (UTVThreatComponent* Threat = MobTarget->GetThreatComponent())
			{
				Threat->AddThreat(Source, DamageForThreat * SpecThreat * SourceThreatMult);
			}
			if (ATVMobAIController* AI = GetMobAI(MobTarget))
			{
				AI->EngageTarget(Source);
			}
		}
	}
	else if (ATVMobCharacter* MobSource = Cast<ATVMobCharacter>(Source))
	{
		if (UTVThreatComponent* Threat = MobSource->GetThreatComponent())
		{
			Threat->AddThreat(Target, 0.f);
		}
	}

	// Rage: taken scales with health fraction; dealt is flat per direct hit.
	const float MaxHealth = FMath::Max(1.f, GetAttr(TargetASC, UTVAttributeSet_Vitals::GetMaxHealthAttribute(), 1.f));
	AddRage(TargetASC, DamageForThreat / MaxHealth * TVCombatTuning::RageFromDamageTakenScale);
	if (!bPeriodic && SourceASC)
	{
		AddRage(SourceASC, TVCombatTuning::RageFromHitDealt);
	}

	if (UTVAbilitySystemComponent* TargetTV = Cast<UTVAbilitySystemComponent>(TargetASC))
	{
		TargetTV->NotifyCombatActivity();
		TargetTV->NotifyDamageTakenForCasts();
	}
	if (UTVAbilitySystemComponent* SourceTV = Cast<UTVAbilitySystemComponent>(SourceASC))
	{
		SourceTV->NotifyCombatActivity();
	}

	if (Damage > 0.f)
	{
		if (ATVPlayerCharacter* PlayerTarget = Cast<ATVPlayerCharacter>(Target))
		{
			if (UTVCombatEventsSubsystem* Events = UTVCombatEventsSubsystem::Get(Target))
			{
				Events->BroadcastPlayerDamaged(PlayerTarget->GetTVPlayerState(), Damage);
			}
		}
		BreakRootsOnDamage(TargetASC);
	}

	if (Absorbed > 0.f && Damage <= 0.f)
	{
		Target->MulticastCombatText(Absorbed, ETVCombatTextKind::Absorb, Source);
	}
	else
	{
		Target->MulticastCombatText(Damage, bCrit ? ETVCombatTextKind::CritDamage : ETVCombatTextKind::Damage, Source);
	}

	if (NewHealth <= 0.f && Damage > 0.f)
	{
		Target->HandleDeath(Source);
	}
}

void UTVCombatLibrary::ResolveIncomingHeal(UAbilitySystemComponent* TargetASC, const FGameplayEffectSpec& Spec, float RawHeal)
{
	if (!TargetASC || RawHeal <= 0.f)
	{
		return;
	}
	ATVCharacterBase* Target = Cast<ATVCharacterBase>(TargetASC->GetAvatarActor());
	if (!Target || !Target->HasAuthority() || Target->IsDead())
	{
		return;
	}
	const FGameplayEffectContextHandle& Context = Spec.GetContext();
	AActor* Source = Context.GetEffectCauser();
	if (ATVPlayerState* SourcePS = Cast<ATVPlayerState>(Source ? Source : Context.GetInstigator()))
	{
		Source = SourcePS->GetPawn();
	}
	UAbilitySystemComponent* SourceASC = Context.GetInstigatorAbilitySystemComponent();
	const bool bCrit = TVTags::Hit_Crit().IsValid() && Spec.GetDynamicAssetTags().HasTagExact(TVTags::Hit_Crit());

	const float Heal = RawHeal * GetAttr(TargetASC, UTVAttributeSet_Combat::GetHealingReceivedMultiplierAttribute(), 1.f);
	const FGameplayAttribute HealthAttr = UTVAttributeSet_Vitals::GetHealthAttribute();
	const float Health = TargetASC->GetNumericAttribute(HealthAttr);
	const float MaxHealth = GetAttr(TargetASC, UTVAttributeSet_Vitals::GetMaxHealthAttribute(), Health);
	const float Effective = FMath::Clamp(Heal, 0.f, FMath::Max(0.f, MaxHealth - Health));
	if (Effective > 0.f)
	{
		TargetASC->SetNumericAttributeBase(HealthAttr, Health + Effective);
	}
	Target->MulticastCombatText(Heal, bCrit ? ETVCombatTextKind::CritHeal : ETVCombatTextKind::Heal, Source);

	if (Source && IsPlayerSide(Source) && Effective > 0.f)
	{
		const float SpecThreat = Spec.GetSetByCallerMagnitude(TVSetByCaller::Threat, false, 1.f);
		const float SourceThreatMult = GetAttr(SourceASC, UTVAttributeSet_Combat::GetThreatMultiplierAttribute(), 1.f);
		AddHealingThreat(Source, Target, Effective, SpecThreat * SourceThreatMult);
		if (UTVAbilitySystemComponent* SourceTV = Cast<UTVAbilitySystemComponent>(SourceASC))
		{
			// Healing a fighting ally puts the healer in combat.
			if (UTVAbilitySystemComponent* TargetTV = Cast<UTVAbilitySystemComponent>(TargetASC))
			{
				if (TargetTV->IsInCombat())
				{
					SourceTV->NotifyCombatActivity();
				}
			}
		}
	}
}

void UTVCombatLibrary::AddHealingThreat(AActor* Healer, AActor* HealedTarget, float EffectiveHeal, float ThreatMultiplier)
{
	if (!Healer || !HealedTarget || EffectiveHeal <= 0.f)
	{
		return;
	}
	TArray<ATVMobCharacter*> Mobs;
	UTVThreatComponent::FindMobsEngagedWith(HealedTarget, Mobs);
	if (Mobs.Num() == 0)
	{
		return;
	}
	const float PerMob = TVCombatTuning::HealingThreatFactor * EffectiveHeal * ThreatMultiplier / Mobs.Num();
	for (ATVMobCharacter* Mob : Mobs)
	{
		if (UTVThreatComponent* Threat = Mob ? Mob->GetThreatComponent() : nullptr)
		{
			Threat->AddThreat(Healer, PerMob);
			if (ATVMobAIController* AI = GetMobAI(Mob))
			{
				AI->EngageTarget(Healer);
			}
		}
	}
}

void UTVCombatLibrary::SummonAdds(AActor* Summoner, FName MobId, int32 Count)
{
	ATVMobCharacter* Boss = Cast<ATVMobCharacter>(Summoner);
	UWorld* World = Summoner ? Summoner->GetWorld() : nullptr;
	if (!Boss || !World || !Boss->HasAuthority() || Count <= 0)
	{
		return;
	}
	const FTVMobRow* AddRow = FindMobRow(MobId);
	if (!AddRow)
	{
		UE_LOG(LogTV, Warning, TEXT("SummonAdds: mob row %s not found"), *MobId.ToString());
		return;
	}
	const bool bStatic = ATVMobAIController::ProfileFromName(AddRow->BehaviorTreeId) == ETVMobAIProfile::Static;
	UTVBossDriverComponent* Driver = Boss->GetBossDriver();
	if (bStatic)
	{
		// Shard pillars: min(Count, party size).
		int32 PartySize = Driver ? Driver->GetEngagedPartySize() : 0;
		if (PartySize <= 0 && Boss->GetThreatComponent())
		{
			PartySize = Boss->GetThreatComponent()->NumLivingPlayers();
		}
		Count = FMath::Min(Count, FMath::Max(1, PartySize));
	}

	AActor* Enemy = Boss->GetCombatTarget();
	if (!Enemy && Boss->GetThreatComponent())
	{
		Enemy = Boss->GetThreatComponent()->GetTopThreatActor();
	}

	const float Ring = bStatic ? 800.f : 350.f;
	const int32 Level = FMath::Clamp(Boss->GetCharacterLevel(), AddRow->LevelMin, FMath::Max(AddRow->LevelMin, AddRow->LevelMax));
	for (int32 i = 0; i < Count; ++i)
	{
		const float Angle = (2.f * PI * i) / Count + FMath::FRandRange(-0.2f, 0.2f);
		const FVector Offset(FMath::Cos(Angle) * Ring, FMath::Sin(Angle) * Ring, 0.f);
		const FTransform Where(Boss->GetActorRotation(), Boss->GetActorLocation() + Offset);
		ATVMobCharacter* Add = ATVMobCharacter::SpawnMob(World, Boss->GetClass(), MobId, Level, Where,
			Boss->GetHealthScale(), Boss->GetDamageScale());
		if (!Add)
		{
			continue;
		}
		Add->SetSummoner(Boss);
		Add->SetHomeLocation(Boss->GetHomeLocation() + Offset);
		if (Driver)
		{
			Driver->RegisterSpawnedAdd(Add);
		}
		if (!bStatic && Enemy)
		{
			if (ATVMobAIController* AI = GetMobAI(Add))
			{
				AI->EngageTarget(Enemy);
			}
		}
	}
}
