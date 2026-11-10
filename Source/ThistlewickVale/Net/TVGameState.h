// Replicated world state: world flags (e.g. the Ember Crown relit -> Lanternrest glow), shared by every client.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Net/TVNetTypes.h"
#include "TVGameState.generated.h"

UCLASS()
class THISTLEWICKVALE_API ATVGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ATVGameState();

	UFUNCTION(BlueprintPure, Category = "World")
	bool HasWorldFlag(FName Flag) const;

	/** Server only. Adds the flag, replicates it and persists it to Saved/World/WorldFlags.json. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "World")
	void SetWorldFlag(FName Flag);

	/** Server only. Mostly for testing. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "World")
	void ClearWorldFlag(FName Flag);

	UFUNCTION(BlueprintPure, Category = "World")
	const TArray<FName>& GetWorldFlags() const { return WorldFlags; }

	/** Fires on server and clients whenever the flag list changes (UI / environment materials listen). */
	UPROPERTY(BlueprintAssignable, Category = "World")
	FTVOnWorldFlagsChanged OnWorldFlagsChanged;

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_WorldFlags)
	TArray<FName> WorldFlags;

	UFUNCTION()
	void OnRep_WorldFlags();

private:
	void LoadFlagsFromDisk();
	void SaveFlagsToDisk() const;
	static FString GetFlagsFilePath();
};
