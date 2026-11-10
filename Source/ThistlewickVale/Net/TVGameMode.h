// Server game mode for L_Thistlewick: login (name + class), spawn at saved location or bind stone, release/respawn.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TVGameMode.generated.h"

class ATVPlayerController;
class ATVPlayerState;
class ATVDungeonManager;

UCLASS()
class THISTLEWICKVALE_API ATVGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATVGameMode();

	//~ AGameModeBase (signatures checked against 5.8.3 GameModeBase.h)
	virtual void StartPlay() override;
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal = TEXT("")) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual bool PlayerCanRestart_Implementation(APlayerController* Player) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Server. Loads the character through UTVSaveSubsystem (or creates it with ATVPlayerState::InitCharacter),
	 * then spawns the pawn at the saved location or the Lanternrest bind stone. Returns false with OutError on bad input.
	 */
	bool LoginPlayer(ATVPlayerController* PC, const FString& Account, const FString& CharacterName, FName ClassId, FString& OutError);

	/** Server. Release after death: dungeon entrance when inside an instance, Lanternrest bind stone otherwise. */
	void ReleasePlayer(ATVPlayerController* PC, bool bForce = false);

	/** Server. Respawns the player at a transform (destroys the old pawn) and restores vitals. */
	void RespawnPlayerAt(ATVPlayerController* PC, const FTransform& Where);

	/** Lanternrest bind stone: APlayerStart tagged "BindStone" in the persistent level, else any player start. */
	FTransform GetBindStoneTransform() const;

	/** Saves every logged-in player (used on shutdown). */
	void SaveAllPlayers();

	static bool IsValidClassId(FName ClassId);
	static FString SanitizeName(const FString& In);

	/** Map name the save system records for the overworld. */
	static const TCHAR* OverworldMapName() { return TEXT("L_Thistlewick"); }

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Spawning")
	FName BindStoneTag = TEXT("BindStone");

	UPROPERTY(EditDefaultsOnly, Category = "Spawning")
	TSubclassOf<ATVDungeonManager> DungeonManagerClass;

	AActor* FindBindStone() const;
};
