// Front-end game mode for L_Entry (standalone, before connecting): no pawn, mouse cursor on.
// The UI Engineer's login widget calls ATVPlayerController::ConnectToServer(Address, Name, ClassId).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TVEntryGameMode.generated.h"

UCLASS()
class THISTLEWICKVALE_API ATVEntryGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATVEntryGameMode();

	virtual bool PlayerCanRestart_Implementation(APlayerController* Player) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
};
