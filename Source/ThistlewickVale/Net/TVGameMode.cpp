#include "Net/TVGameMode.h"

#include "ThistlewickVale.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Core/TVProjectSettings.h"
#include "Combat/TVPlayerState.h"
#include "Combat/TVCharacterBase.h"
#include "Systems/TVSaveSubsystem.h"
#include "Net/TVGameState.h"
#include "Net/TVPlayerController.h"
#include "Net/TVDungeonManager.h"
#include "Net/TVPartySubsystem.h"
#include "Net/TVNetPlayerAccess.h"
#include "Net/TVNetTypes.h"

ATVGameMode::ATVGameMode()
{
	DefaultPawnClass = ATVPlayerCharacter::StaticClass();
	PlayerStateClass = ATVPlayerState::StaticClass();
	PlayerControllerClass = ATVPlayerController::StaticClass();
	GameStateClass = ATVGameState::StaticClass();
	DungeonManagerClass = ATVDungeonManager::StaticClass();
	bStartPlayersAsSpectators = false;
}

void ATVGameMode::StartPlay()
{
	Super::StartPlay();

	if (!ATVDungeonManager::Get(this))
	{
		FActorSpawnParameters Params;
		Params.Name = TEXT("TVDungeonManager");
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		GetWorld()->SpawnActor<ATVDungeonManager>(DungeonManagerClass ? DungeonManagerClass.Get() : ATVDungeonManager::StaticClass(), FTransform::Identity, Params);
	}
	UE_LOG(LogTV, Log, TEXT("%s Server ready on map %s (net mode %d)"), TV_NET_LOG_PREFIX, *GetWorld()->GetMapName(), static_cast<int32>(GetNetMode()));
}

FString ATVGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
	// Base handles ?Name= (player name) and the start spot.
	const FString Result = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);

	if (ATVPlayerController* PC = Cast<ATVPlayerController>(NewPlayerController))
	{
		const FString Name = UGameplayStatics::ParseOption(Options, TEXT("Name"));
		const FString ClassString = UGameplayStatics::ParseOption(Options, TEXT("Class"));
		FString Account = UGameplayStatics::ParseOption(Options, TEXT("Account"));
		if (Account.IsEmpty())
		{
			Account = Name;
		}
		if (!ClassString.IsEmpty() && !Name.IsEmpty())
		{
			PC->SetPendingUrlLogin(Account, Name, FName(*ClassString));
		}
	}
	return Result;
}

void ATVGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	ATVPlayerController* PC = Cast<ATVPlayerController>(NewPlayer);
	if (!PC)
	{
		return;
	}
	FString Account, Name;
	FName ClassId;
	if (PC->GetPendingUrlLogin(Account, Name, ClassId))
	{
		FString Error;
		if (!LoginPlayer(PC, Account, Name, ClassId, Error))
		{
			PC->ClientLoginResult(false, Error);
		}
	}
	else
	{
		UE_LOG(LogTV, Log, TEXT("%s Connection accepted; waiting for ServerLogin (no ?Class= on the URL)"), TV_NET_LOG_PREFIX);
	}
}

bool ATVGameMode::IsValidClassId(FName ClassId)
{
	if (ClassId.IsNone())
	{
		return false;
	}
	if (const UTVProjectSettings* Settings = UTVProjectSettings::Get())
	{
		if (const UDataTable* Classes = Settings->LoadTable(TEXT("Classes")))
		{
			return Classes->GetRowMap().Contains(ClassId);
		}
	}
	// Table not imported yet: accept the four slice classes.
	static const FName Known[] = { TEXT("CLS_WARRIOR"), TEXT("CLS_MAGE"), TEXT("CLS_CLERIC"), TEXT("CLS_RANGER") };
	for (const FName& Id : Known)
	{
		if (Id == ClassId)
		{
			return true;
		}
	}
	return false;
}

FString ATVGameMode::SanitizeName(const FString& In)
{
	FString Out;
	for (const TCHAR Char : In.TrimStartAndEnd())
	{
		if (FChar::IsAlnum(Char) && Out.Len() < 16)
		{
			Out.AppendChar(Char);
		}
	}
	return Out;
}

bool ATVGameMode::LoginPlayer(ATVPlayerController* PC, const FString& InAccount, const FString& InName, FName ClassId, FString& OutError)
{
	if (!PC || PC->IsLoggedIn())
	{
		OutError = TEXT("Already logged in.");
		return false;
	}
	ATVPlayerState* PS = PC->GetPlayerState<ATVPlayerState>();
	if (!PS)
	{
		OutError = TEXT("No player state.");
		return false;
	}
	const FString Name = SanitizeName(InName);
	FString Account = SanitizeName(InAccount);
	if (Account.IsEmpty())
	{
		Account = Name;
	}
	if (Name.Len() < 2)
	{
		OutError = TEXT("Character names need 2-16 letters or digits.");
		return false;
	}
	if (!IsValidClassId(ClassId))
	{
		OutError = FString::Printf(TEXT("Unknown class '%s'."), *ClassId.ToString());
		return false;
	}
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const ATVPlayerController* Other = Cast<ATVPlayerController>(It->Get());
		if (Other && Other != PC && Other->IsLoggedIn() && Other->GetAccountName() == Account
			&& TVNet::GetCharacterName(Other->GetPlayerState<ATVPlayerState>()).Equals(Name, ESearchCase::IgnoreCase))
		{
			OutError = FString::Printf(TEXT("%s is already logged in."), *Name);
			return false;
		}
	}

	// Load or create (DATA_SCHEMAS 8: no auth, "name + class").
	UTVSaveSubsystem* Save = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTVSaveSubsystem>() : nullptr;
	const bool bExisting = Save && Save->LoadCharacter(PS, Account, Name);
	if (!bExisting)
	{
		PS->InitCharacter(ClassId, 1, 0);
		if (Save)
		{
			Save->RegisterAccount(PS, Account); // so SaveCharacter knows the file name
		}
	}
	TVNet::SetCharacterName(PS, Name);
	PC->HandleLoginComplete(Account, Name, bExisting ? TVNet::GetClassId(PS) : ClassId, bExisting);

	// Spawn: saved location (UTVSaveSubsystem::GetSavedSpawn only answers when the save's map is this map),
	// unless it is inside dungeon instance space; else the Lanternrest bind stone.
	FTransform SpawnTransform = GetBindStoneTransform();
	FTransform Saved;
	const ATVDungeonManager* Dungeons = ATVDungeonManager::Get(this);
	if (bExisting && Save && Save->GetSavedSpawn(PS, Saved)
		&& !(Dungeons && Dungeons->IsLocationInInstanceSpace(Saved.GetLocation())))
	{
		SpawnTransform = FTransform(FRotator(0.f, Saved.Rotator().Yaw, 0.f), Saved.GetLocation() + FVector(0.0, 0.0, 50.0));
	}
	RestartPlayerAtTransform(PC, SpawnTransform);

	UE_LOG(LogTV, Log, TEXT("%s Login OK account=%s name=%s class=%s %s at %s"), TV_NET_LOG_PREFIX, *Account, *Name,
		*TVNet::GetClassId(PS).ToString(), bExisting ? TEXT("loaded") : TEXT("new"), *SpawnTransform.GetLocation().ToString());
	PC->ClientLoginResult(true, FString::Printf(TEXT("Welcome %s, %s."), bExisting ? TEXT("back") : TEXT("to Thistlewick Vale"), *Name));
	return true;
}

bool ATVGameMode::PlayerCanRestart_Implementation(APlayerController* Player)
{
	// No pawn until the character is logged in; LoginPlayer spawns it explicitly.
	const ATVPlayerController* PC = Cast<ATVPlayerController>(Player);
	return PC && PC->IsLoggedIn() && Super::PlayerCanRestart_Implementation(Player);
}

AActor* ATVGameMode::FindBindStone() const
{
	UWorld* World = GetWorld();
	APlayerStart* Fallback = nullptr;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		if (It->GetLevel() != World->PersistentLevel)
		{
			continue; // ignore anything inside dungeon instances
		}
		if (It->PlayerStartTag == BindStoneTag)
		{
			return *It;
		}
		if (!Fallback)
		{
			Fallback = *It;
		}
	}
	return Fallback;
}

AActor* ATVGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (AActor* BindStone = FindBindStone())
	{
		return BindStone;
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}

FTransform ATVGameMode::GetBindStoneTransform() const
{
	if (const AActor* BindStone = FindBindStone())
	{
		return FTransform(FRotator(0.f, BindStone->GetActorRotation().Yaw, 0.f), BindStone->GetActorLocation());
	}
	UE_LOG(LogTV, Warning, TEXT("%s No PlayerStart in the persistent level; spawning at the origin"), TV_NET_LOG_PREFIX);
	return FTransform(FVector(0.0, 0.0, 300.0));
}

void ATVGameMode::RespawnPlayerAt(ATVPlayerController* PC, const FTransform& Where)
{
	if (!PC || !PC->IsLoggedIn())
	{
		return;
	}
	if (APawn* OldPawn = PC->GetPawn())
	{
		PC->UnPossess();
		OldPawn->Destroy();
	}
	RestartPlayerAtTransform(PC, Where);
	TVNet::RestoreVitalsAfterRespawn(PC->GetPlayerState<ATVPlayerState>());
}

void ATVGameMode::ReleasePlayer(ATVPlayerController* PC, bool bForce)
{
	ATVPlayerState* PS = PC ? PC->GetPlayerState<ATVPlayerState>() : nullptr;
	if (!PS || !PC->IsLoggedIn())
	{
		return;
	}
	if (!bForce && !TVNet::IsPlayerDead(PS))
	{
		PC->SendSystemMessage(TEXT("You are not dead."));
		return;
	}

	// GDD 5: release to the Lanternrest bind stone; inside the dungeon, to the dungeon entrance.
	FTransform Where;
	const ATVDungeonManager* Dungeons = ATVDungeonManager::Get(this);
	const bool bInDungeon = Dungeons && Dungeons->GetEntranceTransformFor(PS, Where);
	if (!bInDungeon)
	{
		Where = GetBindStoneTransform();
	}
	RespawnPlayerAt(PC, Where);
	UE_LOG(LogTV, Log, TEXT("%s %s released to %s"), TV_NET_LOG_PREFIX, *TVNet::GetCharacterName(PS), bInDungeon ? TEXT("the dungeon entrance") : TEXT("the bind stone"));
}

void ATVGameMode::Logout(AController* Exiting)
{
	ATVPlayerController* PC = Cast<ATVPlayerController>(Exiting);
	ATVPlayerState* PS = PC ? PC->GetPlayerState<ATVPlayerState>() : nullptr;
	if (PC && PS && PC->IsLoggedIn())
	{
		if (ATVDungeonManager* Dungeons = ATVDungeonManager::Get(this))
		{
			Dungeons->HandleLogout(PS); // moves the pawn outside first so the saved location is in the zone
		}
		PC->SaveCharacterNow(TEXT("logout"));
		if (UTVPartySubsystem* Parties = UTVPartySubsystem::Get(this))
		{
			Parties->HandleLogout(PS);
		}
		UE_LOG(LogTV, Log, TEXT("%s Logout %s"), TV_NET_LOG_PREFIX, *TVNet::GetCharacterName(PS));
	}
	Super::Logout(Exiting);
}

void ATVGameMode::SaveAllPlayers()
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ATVPlayerController* PC = Cast<ATVPlayerController>(It->Get()))
		{
			if (PC->IsLoggedIn())
			{
				PC->SaveCharacterNow(TEXT("shutdown"));
			}
		}
	}
}

void ATVGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SaveAllPlayers();
	Super::EndPlay(EndPlayReason);
}
