#include "Net/TVEntryGameMode.h"

#include "GameFramework/GameStateBase.h"
#include "Net/TVPlayerController.h"

ATVEntryGameMode::ATVEntryGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = ATVPlayerController::StaticClass();
	GameStateClass = AGameStateBase::StaticClass();
}

bool ATVEntryGameMode::PlayerCanRestart_Implementation(APlayerController* Player)
{
	return false;
}

void ATVEntryGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	if (NewPlayer)
	{
		NewPlayer->SetShowMouseCursor(true);
	}
}
