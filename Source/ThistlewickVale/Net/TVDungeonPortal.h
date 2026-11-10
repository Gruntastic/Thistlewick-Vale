// Dungeon portal. Entrance (in L_Thistlewick, bIsExit = false) sends the party into its L_Barrow instance;
// exit (in L_Barrow, bIsExit = true) sends the player back out. Walk into the trigger or interact (F).
// Placed by the Environment Artist as BP_TVDungeonPortal (mesh set in the Blueprint).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TVDungeonPortal.generated.h"

class UBoxComponent;
class UArrowComponent;
class UStaticMeshComponent;
class UPrimitiveComponent;
class ATVPlayerState;

UCLASS()
class THISTLEWICKVALE_API ATVDungeonPortal : public AActor
{
	GENERATED_BODY()

public:
	ATVDungeonPortal();

	/** false: world-side entrance. true: exit inside the dungeon (also the default release point if no TV_DungeonEntrance tag). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal")
	bool bIsExit = false;

	/** Where players arrive when they come out of / into this portal. */
	UFUNCTION(BlueprintPure, Category = "Portal")
	FTransform GetArrivalTransform() const;

	/** Server: use the portal (from overlap or ServerInteract). */
	void Use(ATVPlayerState* Player);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
	TObjectPtr<UBoxComponent> Trigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
	TObjectPtr<UArrowComponent> ArrivalPoint;

	/** Seconds before the same player can use a portal again (stops ping-pong after arrival). */
	UPROPERTY(EditAnywhere, Category = "Portal")
	float ReuseCooldown = 3.f;

	virtual void BeginPlay() override;

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	TMap<TWeakObjectPtr<ATVPlayerState>, double> LastUse;
};
