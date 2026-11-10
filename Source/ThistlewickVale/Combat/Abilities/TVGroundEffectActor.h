// Server-driven ground zone: a trap (Snare Trap: arms, then applies the row's effects to the first hostile to enter)
// or a pool (Crownfire: damages hostiles inside every second). Replicated so clients can show the VFX/decal.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/TVCombatRows.h"
#include "TVGroundEffectActor.generated.h"

UENUM(BlueprintType)
enum class ETVGroundEffectMode : uint8 { Trap, Pool };

UCLASS()
class THISTLEWICKVALE_API ATVGroundEffectActor : public AActor
{
	GENERATED_BODY()

public:
	ATVGroundEffectActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server, right after spawning. */
	void InitTrap(AActor* InSource, const FTVAbilityRow& InRow, float InRadius, float InLifetime, float InArmTime, float InAbilityMagnitude);
	void InitPool(AActor* InSource, const FTVAbilityRow& InRow, float InRadius, float InLifetime, float InTickDamage);

	UFUNCTION(BlueprintPure, Category = "TV|Ground") float GetRadius() const { return Radius; }
	UFUNCTION(BlueprintPure, Category = "TV|Ground") ETVGroundEffectMode GetMode() const { return Mode; }
	UFUNCTION(BlueprintPure, Category = "TV|Ground") FName GetVfxId() const { return VfxId; }
	UFUNCTION(BlueprintPure, Category = "TV|Ground") bool IsArmed() const { return bArmed; }

	/** Clients: spawn the visual for VfxId (art hook). */
	UFUNCTION(BlueprintImplementableEvent, Category = "TV|Ground") void OnZoneVisualsChanged();

protected:
	UFUNCTION() void OnRep_Visuals();
	void ServerTick();
	void Arm();

	UPROPERTY(ReplicatedUsing = OnRep_Visuals) ETVGroundEffectMode Mode = ETVGroundEffectMode::Pool;
	UPROPERTY(ReplicatedUsing = OnRep_Visuals) float Radius = 300.f;
	UPROPERTY(ReplicatedUsing = OnRep_Visuals) FName VfxId;
	UPROPERTY(ReplicatedUsing = OnRep_Visuals) bool bArmed = false;

	TWeakObjectPtr<AActor> Source;
	FTVAbilityRow Row;
	float TickDamage = 0.f;
	float AbilityMagnitude = 0.f;
	FTimerHandle TickTimer;
	FTimerHandle ArmTimer;
};
