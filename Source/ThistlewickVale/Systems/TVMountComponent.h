// Mount ownership and mounted state on ATVPlayerState (GDD section 11, DATA_SCHEMAS section 7).
// GA_TV_SummonMount (Combat) handles the 1.5 s cast, then calls ServerSetMounted(true) on the server.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"
#include "TVMountComponent.generated.h"

class ATVPlayerState;
class APlayerState;
class APawn;
class ACharacter;
class UMeshComponent;
class USceneComponent;
class UGameplayAbility;
class UTVMountDataAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTVOnMountedChanged, bool, bIsMounted);

UCLASS(ClassGroup = (TV), meta = (BlueprintSpawnableComponent))
class THISTLEWICKVALE_API UTVMountComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTVMountComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "TV|Mount")
	bool OwnsMount(FName MountId) const { return OwnedMounts.Contains(MountId); }

	/** Server: adds a mount (quest reward, save load). */
	void GrantMount(FName MountId);

	const TArray<FName>& GetOwnedMounts() const { return OwnedMounts; }

	/**
	 * Server only (authority check inside). true mounts the first owned mount (or ActiveMountId) unless in the dungeon;
	 * false dismounts. Not a client RPC on purpose: mounting must go through the summon ability's cast.
	 */
	void ServerSetMounted(bool bMounted);

	/** Dismount key (IMC_TV_Mounted). Clients can always ask to get off. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TV|Mount")
	void ServerRequestDismount();

	UFUNCTION(BlueprintPure, Category = "TV|Mount")
	bool IsMounted() const { return bMounted; }

	/** Server: the player entered the dungeon instance. Dismounts and blocks mounting until NotifyLeftDungeon. */
	void NotifyEnteredDungeon();
	void NotifyLeftDungeon();
	bool IsInDungeon() const { return bInDungeon; }

	const UTVMountDataAsset* GetMountData(FName MountId) const;

	UPROPERTY(BlueprintAssignable, Category = "TV|Mount")
	FTVOnMountedChanged OnMountedChanged;

	/** Every mount in the game. Defaults to the import script's DA_Mount_Dawnstag. */
	UPROPERTY(EditDefaultsOnly, Category = "TV|Mount")
	TArray<TSoftObjectPtr<UTVMountDataAsset>> MountCatalog;

protected:
	UFUNCTION() void OnRep_Mounted();
	UFUNCTION() void HandlePawnSet(APlayerState* Player, APawn* NewPawn, APawn* OldPawn);

	ATVPlayerState* GetTVPlayerState() const;
	bool HasAuthority() const;

	/** Tag + visuals; runs on server and clients. Speed is server-only (attribute replicates). */
	void ApplyMountedState();
	void ApplySpeed(bool bOn);
	void ApplyTag(bool bOn);
	void SpawnMountVisual();
	void DestroyMountVisual();

	void BindServerEvents();
	void UnbindServerEvents();
	void HandlePlayerDamaged(ATVPlayerState* Player, float Amount);
	void HandleAbilityActivated(UGameplayAbility* Ability);
	void HandleAbilityFailed(const UGameplayAbility* Ability, const FGameplayTagContainer& FailureTags);

	UPROPERTY(Replicated)
	TArray<FName> OwnedMounts;

	UPROPERTY(ReplicatedUsing = OnRep_Mounted)
	bool bMounted = false;

	UPROPERTY(Replicated)
	FName ActiveMountId;

	bool bInDungeon = false;
	bool bTagApplied = false;
	bool bSpeedApplied = false;
	float SpeedBaseBeforeMount = 1.f;

	FDelegateHandle DamagedHandle;
	FDelegateHandle AbilityActivatedHandle;
	FDelegateHandle AbilityFailedHandle;

	UPROPERTY(Transient) TObjectPtr<UMeshComponent> MountMesh;
	TWeakObjectPtr<ACharacter> RiddenBy;
	TWeakObjectPtr<USceneComponent> RiderOriginalParent;
	FTransform RiderOriginalRelative;
	TWeakObjectPtr<UClass> RiderOriginalAnimClass;
};
