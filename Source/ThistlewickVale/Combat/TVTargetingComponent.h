// Tab targeting on the player controller (GDD section 5): Tab cycles living hostiles within 40 m inside a 90 degree cone
// in front of the camera, nearest first; click/F-keys call SetTarget. The target is mirrored to the server (ability
// target + the pawn's replicated CombatTarget) so abilities and target-of-target use the same actor.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TVTargetingComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTVOnTargetChanged, AActor*, NewTarget);

UCLASS(ClassGroup = (TV), meta = (BlueprintSpawnableComponent))
class THISTLEWICKVALE_API UTVTargetingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTVTargetingComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure, Category = "TV|Targeting") AActor* GetCurrentTarget() const { return CurrentTarget; }
	/** Local (owning client): next hostile in the cone, nearest first; wraps around. */
	UFUNCTION(BlueprintCallable, Category = "TV|Targeting") void CycleTarget();
	/** Local or server. Null clears. */
	UFUNCTION(BlueprintCallable, Category = "TV|Targeting") void SetTarget(AActor* NewTarget);
	UFUNCTION(BlueprintCallable, Category = "TV|Targeting") void ClearTarget() { SetTarget(nullptr); }

	UPROPERTY(BlueprintAssignable, Category = "TV|Targeting") FTVOnTargetChanged OnTargetChanged;

	UPROPERTY(EditDefaultsOnly, Category = "TV|Targeting") float TabRangeCm = 4000.f;
	UPROPERTY(EditDefaultsOnly, Category = "TV|Targeting") float TabConeHalfAngleDeg = 45.f;
	/** Targets further than this are dropped automatically. */
	UPROPERTY(EditDefaultsOnly, Category = "TV|Targeting") float MaxKeepRangeCm = 6000.f;

protected:
	UFUNCTION(Server, Reliable) void ServerSetTarget(AActor* NewTarget);
	UFUNCTION() void OnRep_CurrentTarget();
	void ApplyTarget(AActor* NewTarget);
	APawn* GetControlledPawn() const;
	bool GetViewPoint(FVector& OutLocation, FVector& OutForward) const;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentTarget) TObjectPtr<AActor> CurrentTarget;
};
