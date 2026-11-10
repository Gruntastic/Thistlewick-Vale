#include "Net/TVPlayerController.h"

#include "ThistlewickVale.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/DataTable.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "GameFramework/Character.h"
#include "GameFramework/SpringArmComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"
#include "Core/TVProjectSettings.h"
#include "Combat/TVPlayerState.h"
#include "Combat/TVCharacterBase.h"
#include "Combat/TVTargetingComponent.h"
#include "Combat/TVAbilitySystemComponent.h"
#include "Systems/TVSaveSubsystem.h"
#include "Systems/TVMountComponent.h"
#include "Systems/TVInteractable.h"
#include "Net/TVInputConfig.h"
#include "Net/TVGameMode.h"
#include "Net/TVPartySubsystem.h"
#include "Net/TVPartyInfo.h"
#include "Net/TVDungeonManager.h"
#include "Net/TVDungeonPortal.h"
#include "Net/TVCharacterFilePeek.h"
#include "Net/TVNetPlayerAccess.h"

namespace
{
	const FName MountSummonAbilityId(TEXT("ABL_MOUNT_SUMMON"));
	const FName DefaultClassId(TEXT("CLS_WARRIOR"));

	bool IsInteractable(const AActor* Actor)
	{
		return Actor && (Actor->IsA<ATVDungeonPortal>() || Actor->GetClass()->ImplementsInterface(UTVInteractable::StaticClass()));
	}

	/**
	 * INTERFACE-REQUEST (Systems, ITVInteractable): INTERFACES.md names the interface but not its function. This calls
	 * the first UFUNCTION named Interact / OnInteract / ExecuteInteract by reflection and fills every object parameter
	 * with the controller, pawn or player state, whichever matches the parameter class. Please confirm the signature
	 * (suggested: UFUNCTION(BlueprintNativeEvent) void Interact(ATVPlayerController* Instigator)) and this can become
	 * a direct ITVInteractable::Execute_Interact call.
	 */
	bool CallInteractByReflection(AActor* Target, ATVPlayerController* PC)
	{
		static const FName Names[] = { TEXT("Interact"), TEXT("OnInteract"), TEXT("ExecuteInteract") };
		for (const FName& Name : Names)
		{
			UFunction* Function = Target->FindFunction(Name);
			if (!Function)
			{
				continue;
			}
			TArray<uint8, TInlineAllocator<64>> Buffer;
			Buffer.SetNumZeroed(FMath::Max<int32>(Function->ParmsSize, 1));
			uint8* Params = Buffer.GetData();
			for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
			{
				It->InitializeValue_InContainer(Params);
			}
			UObject* Candidates[] = { PC, PC->GetPawn(), PC->PlayerState.Get() };
			for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
			{
				FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(*It);
				if (!ObjectProperty || It->HasAnyPropertyFlags(CPF_ReturnParm))
				{
					continue;
				}
				for (UObject* Candidate : Candidates)
				{
					if (Candidate && Candidate->IsA(ObjectProperty->PropertyClass))
					{
						ObjectProperty->SetObjectPropertyValue_InContainer(Params, Candidate);
						break;
					}
				}
			}
			Target->ProcessEvent(Function, Params);
			for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
			{
				It->DestroyValue_InContainer(Params);
			}
			return true;
		}
		return false;
	}
}

ATVPlayerController::ATVPlayerController()
{
	Targeting = CreateDefaultSubobject<UTVTargetingComponent>(TEXT("Targeting"));
	ActionBar.Init(NAME_None, NumActionSlots);
	bShowMouseCursor = true;
}

void ATVPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ATVPlayerController, ActionBar, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ATVPlayerController, Party, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ATVPlayerController, bLoggedIn, COND_OwnerOnly);
}

ATVPlayerState* ATVPlayerController::GetTVPlayerState() const
{
	return GetPlayerState<ATVPlayerState>();
}

// ================================================================================================ lifecycle

void ATVPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		ApplyMappingContexts();
		SetShowMouseCursor(true);
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(true);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
		TryAutoLoginFromCommandLine();
	}
}

void ATVPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutosaveTimer);
	}
	Super::EndPlay(EndPlayReason);
}

void ATVPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (!bAutoLoginSent)
	{
		AutoLoginRetryTime += DeltaTime;
		if (AutoLoginRetryTime > 0.5f)
		{
			AutoLoginRetryTime = 0.f;
			TryAutoLoginFromCommandLine();
		}
	}

	UpdateMountedContext();

	if (bAutoRun)
	{
		if (APawn* MyPawn = GetPawn())
		{
			const FRotator YawRotation(0.f, GetControlRotation().Yaw, 0.f);
			MyPawn->AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), 1.f);
		}
	}
}

// ================================================================================================ login

void ATVPlayerController::TryAutoLoginFromCommandLine()
{
	if (bAutoLoginSent || !IsLocalController())
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const ENetMode NetMode = World->GetNetMode();
	FString Name, ClassString, Account;
	const TCHAR* CommandLine = FCommandLine::Get();
	const bool bHasName = FParse::Value(CommandLine, TEXT("TVName="), Name);
	FParse::Value(CommandLine, TEXT("TVClass="), ClassString);
	FParse::Value(CommandLine, TEXT("TVAccount="), Account);

	if (NetMode == NM_Client)
	{
		// The URL (?Name=?Class=) usually logged us in already on the server; ServerLogin is then ignored.
		if (!bHasName)
		{
			bAutoLoginSent = true; // nothing on the command line: wait for the login UI / TVLogin
			return;
		}
	}
	else
	{
		// Standalone / listen server (PIE, -game on L_Thistlewick): only in the MMO game mode, not on L_Entry.
		if (!World->GetAuthGameMode<ATVGameMode>())
		{
			bAutoLoginSent = true;
			return;
		}
		if (!bHasName)
		{
			Name = TEXT("Adventurer");
		}
	}
	if (!GetPlayerState<ATVPlayerState>())
	{
		return; // retry from PlayerTick once the player state exists
	}
	bAutoLoginSent = true;
	const FName ClassId = ClassString.IsEmpty() ? DefaultClassId : FName(*ClassString);
	UE_LOG(LogTV, Log, TEXT("%s Auto login from command line: name=%s class=%s"), TV_NET_LOG_PREFIX, *Name, *ClassId.ToString());
	ServerLogin(Account, Name, ClassId);
}

void ATVPlayerController::ServerLogin_Implementation(const FString& Account, const FString& CharacterName, FName ClassId)
{
	if (bLoggedIn)
	{
		return; // already logged in from the travel URL
	}
	ATVGameMode* GameMode = GetWorld()->GetAuthGameMode<ATVGameMode>();
	if (!GameMode)
	{
		ClientLoginResult(false, TEXT("This map does not accept logins."));
		return;
	}
	FString Error;
	if (!GameMode->LoginPlayer(this, Account, CharacterName, ClassId, Error))
	{
		UE_LOG(LogTV, Warning, TEXT("%s Login refused for %s: %s"), TV_NET_LOG_PREFIX, *CharacterName, *Error);
		ClientLoginResult(false, Error);
	}
}

void ATVPlayerController::ClientLoginResult_Implementation(bool bSuccess, const FString& Message)
{
	UE_LOG(LogTV, Log, TEXT("%s Client login result: %s (%s)"), TV_NET_LOG_PREFIX, bSuccess ? TEXT("OK") : TEXT("FAILED"), *Message);
	OnLoginResult.Broadcast(bSuccess, Message);
	OnChatMessage.Broadcast(ETVChatChannel::System, FString(), Message);
}

void ATVPlayerController::ConnectToServer(const FString& Address, const FString& CharacterName, FName ClassId, const FString& Account)
{
	const FString SafeName = ATVGameMode::SanitizeName(CharacterName);
	const FString SafeAccount = Account.IsEmpty() ? SafeName : ATVGameMode::SanitizeName(Account);
	const FString Url = FString::Printf(TEXT("%s?Name=%s?Class=%s?Account=%s"), *Address, *SafeName, *ClassId.ToString(), *SafeAccount);
	UE_LOG(LogTV, Log, TEXT("%s Connecting: %s"), TV_NET_LOG_PREFIX, *Url);
	ClientTravel(Url, TRAVEL_Absolute);
}

void ATVPlayerController::SetPendingUrlLogin(const FString& Account, const FString& CharacterName, FName ClassId)
{
	PendingUrlAccount = Account;
	PendingUrlName = CharacterName;
	PendingUrlClass = ClassId;
}

bool ATVPlayerController::GetPendingUrlLogin(FString& OutAccount, FString& OutName, FName& OutClass) const
{
	if (PendingUrlName.IsEmpty() || PendingUrlClass.IsNone())
	{
		return false;
	}
	OutAccount = PendingUrlAccount;
	OutName = PendingUrlName;
	OutClass = PendingUrlClass;
	return true;
}

void ATVPlayerController::HandleLoginComplete(const FString& Account, const FString& CharacterName, FName ClassId, bool bExistingCharacter, const FTVCharacterFilePeek& Peek)
{
	AccountName = Account;
	bLoggedIn = true;

	if (Peek.ActionBar.Num() == NumActionSlots)
	{
		ActionBar = Peek.ActionBar;
	}
	else
	{
		BuildDefaultActionBar(ClassId);
	}
	OnRep_ActionBar();
	OnRep_LoggedIn();
	ForceNetUpdate();

	GetWorldTimerManager().SetTimer(AutosaveTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { SaveCharacterNow(TEXT("autosave")); }),
		AutosaveInterval, true);
}

void ATVPlayerController::OnRep_LoggedIn()
{
	if (bLoggedIn)
	{
		bAutoLoginSent = true;
	}
}

void ATVPlayerController::SaveCharacterNow(const TCHAR* Reason)
{
	if (!HasAuthority() || !bLoggedIn)
	{
		return;
	}
	ATVPlayerState* PS = GetTVPlayerState();
	UTVSaveSubsystem* Save = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTVSaveSubsystem>() : nullptr;
	if (!PS || !Save)
	{
		UE_LOG(LogTV, Warning, TEXT("%s Save skipped (%s): no player state or save subsystem"), TV_NET_LOG_PREFIX, Reason);
		return;
	}
	// INTERFACE-REQUEST (Systems, UTVSaveSubsystem): write "actionBar" from
	// Cast<ATVPlayerController>(PS->GetPlayerController())->GetActionBar() (10 entries, null for empty).
	const bool bSaved = Save->SaveCharacter(PS);
	UE_LOG(LogTV, Log, TEXT("%s Saved %s (%s): %s"), TV_NET_LOG_PREFIX, *TVNet::GetCharacterName(PS), Reason, bSaved ? TEXT("ok") : TEXT("FAILED"));
}

// ================================================================================================ action bar

void ATVPlayerController::BuildDefaultActionBar(FName ClassId)
{
	ActionBar.Init(NAME_None, NumActionSlots);

	// DT_Classes.StartingAbilities [{AbilityId, UnlockLevel, DefaultSlot}] read by reflection so Net does not depend on
	// the Combat row header. Slots are treated as 0-based; out-of-range slots go to the first free slot.
	// VERIFY-5.8 / contract: DATA_SCHEMAS 2 does not say whether DefaultSlot is 0- or 1-based.
	const UTVProjectSettings* Settings = UTVProjectSettings::Get();
	const UDataTable* Classes = Settings ? Settings->LoadTable(TEXT("Classes")) : nullptr;
	const uint8* Row = Classes ? Classes->FindRowUnchecked(ClassId) : nullptr;
	const UScriptStruct* RowStruct = Classes ? Classes->GetRowStruct() : nullptr;
	if (!Row || !RowStruct)
	{
		return;
	}
	const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(RowStruct->FindPropertyByName(TEXT("StartingAbilities")));
	const FStructProperty* Inner = ArrayProperty ? CastField<FStructProperty>(ArrayProperty->Inner) : nullptr;
	if (!Inner)
	{
		return;
	}
	const FNameProperty* IdProperty = CastField<FNameProperty>(Inner->Struct->FindPropertyByName(TEXT("AbilityId")));
	const FIntProperty* SlotProperty = CastField<FIntProperty>(Inner->Struct->FindPropertyByName(TEXT("DefaultSlot")));
	if (!IdProperty)
	{
		return;
	}
	FScriptArrayHelper Helper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(Row));
	TArray<FName> Unplaced;
	for (int32 Index = 0; Index < Helper.Num(); ++Index)
	{
		const uint8* Element = Helper.GetRawPtr(Index);
		const FName AbilityId = IdProperty->GetPropertyValue_InContainer(Element);
		const int32 Slot = SlotProperty ? SlotProperty->GetPropertyValue_InContainer(Element) : INDEX_NONE;
		if (ActionBar.IsValidIndex(Slot) && ActionBar[Slot].IsNone())
		{
			ActionBar[Slot] = AbilityId;
		}
		else
		{
			Unplaced.Add(AbilityId);
		}
	}
	for (const FName& AbilityId : Unplaced)
	{
		const int32 Free = ActionBar.IndexOfByKey(NAME_None);
		if (Free != INDEX_NONE)
		{
			ActionBar[Free] = AbilityId;
		}
	}
}

void ATVPlayerController::ServerSetActionSlot_Implementation(int32 SlotIndex, FName AbilityId)
{
	if (!ActionBar.IsValidIndex(SlotIndex))
	{
		return;
	}
	ActionBar[SlotIndex] = AbilityId;
	OnRep_ActionBar();
}

bool ATVPlayerController::ActivateActionSlot(int32 SlotIndex)
{
	if (!ActionBar.IsValidIndex(SlotIndex) || ActionBar[SlotIndex].IsNone())
	{
		return false;
	}
	UTVAbilitySystemComponent* ASC = TVNet::GetTVAbilitySystem(GetTVPlayerState());
	return ASC && ASC->TryActivateAbilityById(ActionBar[SlotIndex]);
}

void ATVPlayerController::OnRep_ActionBar()
{
	OnActionBarChanged.Broadcast();
}

// ================================================================================================ interaction / death

void ATVPlayerController::ServerInteract_Implementation(AActor* Target)
{
	APawn* MyPawn = GetPawn();
	ATVPlayerState* PS = GetTVPlayerState();
	if (!bLoggedIn || !IsValid(Target) || !MyPawn || !PS || TVNet::IsPawnDead(MyPawn))
	{
		return;
	}
	// Generous server-side range check (client picks within InteractRange; allow for large actors and lag).
	if (FVector::Dist(MyPawn->GetActorLocation(), Target->GetActorLocation()) > InteractRange * 1.5f)
	{
		return;
	}
	if (ATVDungeonPortal* Portal = Cast<ATVDungeonPortal>(Target))
	{
		Portal->Use(PS);
		return;
	}
	if (Target->GetClass()->ImplementsInterface(UTVInteractable::StaticClass()))
	{
		if (!CallInteractByReflection(Target, this))
		{
			UE_LOG(LogTV, Warning, TEXT("%s %s implements ITVInteractable but no Interact UFUNCTION was found"), TV_NET_LOG_PREFIX, *GetNameSafe(Target));
		}
	}
}

void ATVPlayerController::ServerReleaseSpirit_Implementation()
{
	if (ATVGameMode* GameMode = GetWorld()->GetAuthGameMode<ATVGameMode>())
	{
		GameMode->ReleasePlayer(this);
	}
}

AActor* ATVPlayerController::FindInteractCandidate() const
{
	const APawn* MyPawn = GetPawn();
	if (!MyPawn)
	{
		return nullptr;
	}
	const FVector Origin = MyPawn->GetActorLocation();
	if (Targeting)
	{
		AActor* Target = Targeting->GetCurrentTarget();
		if (IsInteractable(Target) && FVector::Dist(Origin, Target->GetActorLocation()) <= InteractRange)
		{
			return Target;
		}
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TVInteract), false, MyPawn);
	GetWorld()->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity,
		FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects), FCollisionShape::MakeSphere(InteractRange), Params);
	AActor* Best = nullptr;
	double BestDistance = TNumericLimits<double>::Max();
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (!IsInteractable(Actor))
		{
			continue;
		}
		const double Distance = FVector::DistSquared(Origin, Actor->GetActorLocation());
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = Actor;
		}
	}
	return Best;
}

// ================================================================================================ input

void ATVPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!InputConfig)
	{
		InputConfig = UTVInputConfig::Create(this);
	}
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent);
	if (!Input)
	{
		UE_LOG(LogTV, Error, TEXT("%s InputComponent is not a UEnhancedInputComponent; check Config/DefaultInput.ini"), TV_NET_LOG_PREFIX);
		return;
	}
	const UTVInputConfig* C = InputConfig;
	Input->BindAction(C->Move, ETriggerEvent::Triggered, this, &ATVPlayerController::HandleMove);
	Input->BindAction(C->Look, ETriggerEvent::Triggered, this, &ATVPlayerController::HandleLook);
	Input->BindAction(C->OrbitHold, ETriggerEvent::Started, this, &ATVPlayerController::HandleOrbitStarted);
	Input->BindAction(C->OrbitHold, ETriggerEvent::Completed, this, &ATVPlayerController::HandleOrbitCompleted);
	Input->BindAction(C->Zoom, ETriggerEvent::Triggered, this, &ATVPlayerController::HandleZoom);
	Input->BindAction(C->Jump, ETriggerEvent::Started, this, &ATVPlayerController::HandleJumpStarted);
	Input->BindAction(C->Jump, ETriggerEvent::Completed, this, &ATVPlayerController::HandleJumpCompleted);
	Input->BindAction(C->AutoRun, ETriggerEvent::Started, this, &ATVPlayerController::HandleAutoRun);
	Input->BindAction(C->TabTarget, ETriggerEvent::Started, this, &ATVPlayerController::HandleTabTarget);
	Input->BindAction(C->TargetSelf, ETriggerEvent::Started, this, &ATVPlayerController::HandleTargetSelf);
	for (int32 Index = 0; Index < C->TargetParty.Num(); ++Index)
	{
		Input->BindAction(C->TargetParty[Index], ETriggerEvent::Started, this, &ATVPlayerController::HandleTargetParty, Index);
	}
	Input->BindAction(C->ClickSelect, ETriggerEvent::Started, this, &ATVPlayerController::HandleClickSelect);
	for (int32 Index = 0; Index < C->ActionSlots.Num(); ++Index)
	{
		Input->BindAction(C->ActionSlots[Index], ETriggerEvent::Started, this, &ATVPlayerController::HandleActionSlot, Index);
	}
	Input->BindAction(C->Interact, ETriggerEvent::Started, this, &ATVPlayerController::HandleInteract);
	Input->BindAction(C->Mount, ETriggerEvent::Started, this, &ATVPlayerController::HandleMountKey);
	Input->BindAction(C->ToggleBags, ETriggerEvent::Started, this, &ATVPlayerController::HandleToggleBags);
	Input->BindAction(C->ToggleCharacter, ETriggerEvent::Started, this, &ATVPlayerController::HandleToggleCharacter);
	Input->BindAction(C->ToggleQuestLog, ETriggerEvent::Started, this, &ATVPlayerController::HandleToggleQuestLog);
	Input->BindAction(C->ToggleAbilityBook, ETriggerEvent::Started, this, &ATVPlayerController::HandleToggleAbilityBook);
	Input->BindAction(C->ToggleMenu, ETriggerEvent::Started, this, &ATVPlayerController::HandleToggleMenu);
	Input->BindAction(C->OpenChat, ETriggerEvent::Started, this, &ATVPlayerController::HandleOpenChat);

	ApplyMappingContexts();
}

void ATVPlayerController::ApplyMappingContexts()
{
	if (bMappingApplied || !InputConfig)
	{
		return;
	}
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer) : nullptr;
	if (Subsystem)
	{
		Subsystem->AddMappingContext(InputConfig->DefaultContext, 0);
		bMappingApplied = true;
	}
}

void ATVPlayerController::UpdateMountedContext()
{
	if (!bMappingApplied || !InputConfig)
	{
		return;
	}
	const UTVMountComponent* Mount = TVNet::GetMount(GetTVPlayerState());
	const bool bMounted = Mount && Mount->IsMounted();
	if (bMounted == bMountedContextActive)
	{
		return;
	}
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer) : nullptr;
	if (!Subsystem)
	{
		return;
	}
	if (bMounted)
	{
		Subsystem->RemoveMappingContext(InputConfig->DefaultContext);
		Subsystem->AddMappingContext(InputConfig->MountedContext, 0);
	}
	else
	{
		Subsystem->RemoveMappingContext(InputConfig->MountedContext);
		Subsystem->AddMappingContext(InputConfig->DefaultContext, 0);
	}
	bMountedContextActive = bMounted;
}

void ATVPlayerController::HandleMove(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	APawn* MyPawn = GetPawn();
	if (!MyPawn)
	{
		return;
	}
	if (Axis.Y < 0.f)
	{
		bAutoRun = false; // backing up cancels auto-run
	}
	const FRotator YawRotation(0.f, GetControlRotation().Yaw, 0.f);
	const FRotationMatrix Matrix(YawRotation);
	MyPawn->AddMovementInput(Matrix.GetUnitAxis(EAxis::X), Axis.Y);
	MyPawn->AddMovementInput(Matrix.GetUnitAxis(EAxis::Y), Axis.X);
}

void ATVPlayerController::HandleLook(const FInputActionValue& Value)
{
	if (!bOrbiting)
	{
		return;
	}
	const FVector2D Axis = Value.Get<FVector2D>();
	AddYawInput(Axis.X);
	// VERIFY-5.8: Mouse2D Y is positive when moving up; negated here for non-inverted look. Flip if it feels inverted.
	AddPitchInput(-Axis.Y);
}

void ATVPlayerController::HandleOrbitStarted()
{
	bOrbiting = true;
}

void ATVPlayerController::HandleOrbitCompleted()
{
	bOrbiting = false;
}

void ATVPlayerController::HandleZoom(const FInputActionValue& Value)
{
	const float Delta = Value.Get<float>();
	APawn* MyPawn = GetPawn();
	USpringArmComponent* Arm = MyPawn ? MyPawn->FindComponentByClass<USpringArmComponent>() : nullptr;
	if (Arm)
	{
		Arm->TargetArmLength = FMath::Clamp(Arm->TargetArmLength - Delta * ZoomStep, MinZoom, MaxZoom);
	}
}

void ATVPlayerController::HandleJumpStarted()
{
	if (ACharacter* MyCharacter = GetPawn<ACharacter>())
	{
		MyCharacter->Jump();
	}
}

void ATVPlayerController::HandleJumpCompleted()
{
	if (ACharacter* MyCharacter = GetPawn<ACharacter>())
	{
		MyCharacter->StopJumping();
	}
}

void ATVPlayerController::HandleAutoRun()
{
	bAutoRun = !bAutoRun;
}

void ATVPlayerController::HandleTabTarget()
{
	if (Targeting)
	{
		Targeting->CycleTarget();
	}
}

void ATVPlayerController::HandleTargetSelf()
{
	if (Targeting && GetPawn())
	{
		Targeting->SetTarget(GetPawn());
	}
}

void ATVPlayerController::HandleTargetParty(int32 PartyIndex)
{
	if (!Targeting || !Party)
	{
		return;
	}
	int32 Seen = 0;
	for (const FTVPartyMemberInfo& Info : Party->GetMemberInfos())
	{
		if (!Info.PlayerState || Info.PlayerState.Get() == PlayerState.Get())
		{
			continue;
		}
		if (Seen++ == PartyIndex)
		{
			if (APawn* MemberPawn = Info.PlayerState->GetPawn())
			{
				Targeting->SetTarget(MemberPawn);
			}
			return;
		}
	}
}

void ATVPlayerController::HandleClickSelect()
{
	if (!Targeting || bOrbiting)
	{
		return;
	}
	FHitResult Hit;
	if (GetHitResultUnderCursor(ECC_Visibility, false, Hit) && Cast<ATVCharacterBase>(Hit.GetActor()))
	{
		Targeting->SetTarget(Hit.GetActor());
	}
}

void ATVPlayerController::HandleActionSlot(int32 SlotIndex)
{
	ActivateActionSlot(SlotIndex);
}

void ATVPlayerController::HandleInteract()
{
	if (AActor* Candidate = FindInteractCandidate())
	{
		ServerInteract(Candidate);
	}
}

void ATVPlayerController::HandleMountKey()
{
	ATVPlayerState* PS = GetTVPlayerState();
	const UTVMountComponent* Mount = TVNet::GetMount(PS);
	if (Mount && Mount->IsMounted())
	{
		ServerDismount();
		return;
	}
	if (UTVAbilitySystemComponent* ASC = TVNet::GetTVAbilitySystem(PS))
	{
		ASC->TryActivateAbilityById(MountSummonAbilityId);
	}
}

void ATVPlayerController::ServerDismount_Implementation()
{
	if (UTVMountComponent* Mount = TVNet::GetMount(GetTVPlayerState()))
	{
		Mount->ServerSetMounted(false);
	}
}

// ================================================================================================ chat

void ATVPlayerController::SendSystemMessage(const FString& Message)
{
	ClientReceiveChat(ETVChatChannel::System, FString(), Message);
}

void ATVPlayerController::ClientReceiveChat_Implementation(ETVChatChannel Channel, const FString& Sender, const FString& Message)
{
	static const TCHAR* ChannelNames[] = { TEXT("Say"), TEXT("Party"), TEXT("System") };
	const TCHAR* ChannelName = ChannelNames[FMath::Clamp(static_cast<int32>(Channel), 0, 2)];
	UE_LOG(LogTV, Log, TEXT("%s Chat [%s] %s%s%s"), TV_NET_LOG_PREFIX, ChannelName, *Sender, Sender.IsEmpty() ? TEXT("") : TEXT(": "), *Message);
	OnChatMessage.Broadcast(Channel, Sender, Message);
#if !UE_BUILD_SHIPPING
	// Visible before the chat widget exists.
	if (GEngine && !OnChatMessage.IsBound())
	{
		const FColor Color = Channel == ETVChatChannel::Party ? FColor(120, 180, 255) : Channel == ETVChatChannel::System ? FColor::Yellow : FColor::White;
		GEngine->AddOnScreenDebugMessage(-1, 8.f, Color, FString::Printf(TEXT("[%s] %s%s%s"), ChannelName, *Sender, Sender.IsEmpty() ? TEXT("") : TEXT(": "), *Message));
	}
#endif
}

void ATVPlayerController::ServerChat_Implementation(const FString& InLine)
{
	if (!bLoggedIn)
	{
		return;
	}
	// Simple flood guard: at most 6 lines per 3 seconds.
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastChatTime > 3.f)
	{
		LastChatTime = Now;
		ChatBurst = 0;
	}
	if (++ChatBurst > 6)
	{
		SendSystemMessage(TEXT("You are sending messages too quickly."));
		return;
	}
	const FString Line = InLine.TrimStartAndEnd().Left(255);
	if (Line.IsEmpty())
	{
		return;
	}
	if (Line.StartsWith(TEXT("/")))
	{
		ExecuteChatCommand(Line);
	}
	else
	{
		BroadcastSay(Line);
	}
}

void ATVPlayerController::ServerSay_Implementation(const FString& Message)
{
	if (bLoggedIn)
	{
		BroadcastSay(Message);
	}
}

void ATVPlayerController::ServerPartyChat_Implementation(const FString& Message)
{
	if (bLoggedIn)
	{
		BroadcastParty(Message);
	}
}

void ATVPlayerController::BroadcastSay(const FString& InMessage)
{
	const FString Message = InMessage.TrimStartAndEnd().Left(255);
	if (Message.IsEmpty())
	{
		return;
	}
	const FString Sender = TVNet::GetCharacterName(GetTVPlayerState());
	const APawn* MyPawn = GetPawn();
	UE_LOG(LogTV, Log, TEXT("%s [Say] %s: %s"), TV_NET_LOG_PREFIX, *Sender, *Message);
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ATVPlayerController* Other = Cast<ATVPlayerController>(It->Get());
		if (!Other || !Other->IsLoggedIn())
		{
			continue;
		}
		const APawn* OtherPawn = Other->GetPawn();
		const bool bInRange = Other == this
			|| (MyPawn && OtherPawn && FVector::Dist(MyPawn->GetActorLocation(), OtherPawn->GetActorLocation()) <= SayRange);
		if (bInRange)
		{
			Other->ClientReceiveChat(ETVChatChannel::Say, Sender, Message);
		}
	}
}

void ATVPlayerController::BroadcastParty(const FString& InMessage)
{
	const FString Message = InMessage.TrimStartAndEnd().Left(255);
	ATVPlayerState* PS = GetTVPlayerState();
	const UTVPartySubsystem* Parties = UTVPartySubsystem::Get(this);
	if (Message.IsEmpty() || !Parties)
	{
		return;
	}
	if (!Parties->GetParty(PS))
	{
		SendSystemMessage(TEXT("You are not in a party."));
		return;
	}
	const FString Sender = TVNet::GetCharacterName(PS);
	UE_LOG(LogTV, Log, TEXT("%s [Party] %s: %s"), TV_NET_LOG_PREFIX, *Sender, *Message);
	for (ATVPlayerState* Member : Parties->GetPartyMembers(PS))
	{
		if (ATVPlayerController* MemberPC = Member ? Cast<ATVPlayerController>(Member->GetPlayerController()) : nullptr)
		{
			MemberPC->ClientReceiveChat(ETVChatChannel::Party, Sender, Message);
		}
	}
}

void ATVPlayerController::ExecuteChatCommand(const FString& Line)
{
	FString Command = Line;
	FString Rest;
	Line.Split(TEXT(" "), &Command, &Rest);
	Command = Command.ToLower();
	Rest = Rest.TrimStartAndEnd();

	ATVPlayerState* PS = GetTVPlayerState();
	UTVPartySubsystem* Parties = UTVPartySubsystem::Get(this);
	FString Result;

	if (Command == TEXT("/s") || Command == TEXT("/say"))
	{
		BroadcastSay(Rest);
	}
	else if (Command == TEXT("/p") || Command == TEXT("/party"))
	{
		BroadcastParty(Rest);
	}
	else if (Parties && (Command == TEXT("/invite") || Command == TEXT("/inv")))
	{
		Parties->InviteByName(PS, Rest, Result);
	}
	else if (Parties && Command == TEXT("/accept"))
	{
		Parties->Accept(PS, Result);
	}
	else if (Parties && Command == TEXT("/decline"))
	{
		Parties->Decline(PS, Result);
	}
	else if (Parties && Command == TEXT("/leave"))
	{
		Parties->Leave(PS, Result);
	}
	else if (Parties && Command == TEXT("/kick"))
	{
		Parties->KickByName(PS, Rest, Result);
	}
	else if (Parties && (Command == TEXT("/leader") || Command == TEXT("/promote")))
	{
		Parties->PromoteByName(PS, Rest, Result);
	}
	else if (Command == TEXT("/release"))
	{
		ServerReleaseSpirit_Implementation();
	}
	else if (Command == TEXT("/leavedungeon"))
	{
		ServerLeaveDungeon_Implementation();
	}
	else if (Command == TEXT("/enterdungeon"))
	{
		ServerDebugEnterDungeon_Implementation();
	}
	else if (Command == TEXT("/who"))
	{
		TArray<FString> Names;
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			const ATVPlayerController* Other = Cast<ATVPlayerController>(It->Get());
			if (Other && Other->IsLoggedIn())
			{
				Names.Add(TVNet::GetCharacterName(Other->GetTVPlayerState()));
			}
		}
		Result = FString::Printf(TEXT("Online (%d): %s"), Names.Num(), *FString::Join(Names, TEXT(", ")));
	}
	else
	{
		Result = TEXT("Commands: /s /p /invite <name> /accept /decline /leave /kick <name> /leader <name> /release /leavedungeon /who");
	}
	if (!Result.IsEmpty())
	{
		SendSystemMessage(Result);
	}
}

// ================================================================================================ party

void ATVPlayerController::ServerPartyInvite_Implementation(const FString& PlayerName) { ExecuteChatCommand(FString(TEXT("/invite ")) + PlayerName); }
void ATVPlayerController::ServerPartyAccept_Implementation() { ExecuteChatCommand(TEXT("/accept")); }
void ATVPlayerController::ServerPartyDecline_Implementation() { ExecuteChatCommand(TEXT("/decline")); }
void ATVPlayerController::ServerPartyLeave_Implementation() { ExecuteChatCommand(TEXT("/leave")); }
void ATVPlayerController::ServerPartyKick_Implementation(const FString& PlayerName) { ExecuteChatCommand(FString(TEXT("/kick ")) + PlayerName); }
void ATVPlayerController::ServerPartyPromote_Implementation(const FString& PlayerName) { ExecuteChatCommand(FString(TEXT("/leader ")) + PlayerName); }

void ATVPlayerController::ClientPartyInviteReceived_Implementation(const FString& InviterName)
{
	UE_LOG(LogTV, Log, TEXT("%s Party invite received from %s"), TV_NET_LOG_PREFIX, *InviterName);
	OnPartyInviteReceived.Broadcast(InviterName);
}

void ATVPlayerController::SetParty(ATVPartyInfo* NewParty)
{
	if (Party != NewParty)
	{
		Party = NewParty;
		ForceNetUpdate();
		OnRep_Party();
	}
}

void ATVPlayerController::OnRep_Party()
{
	OnPartyChanged.Broadcast();
}

void ATVPlayerController::NotifyPartyInfoUpdated()
{
	OnPartyChanged.Broadcast();
}

// ================================================================================================ dungeon

void ATVPlayerController::ServerLeaveDungeon_Implementation()
{
	ATVDungeonManager* Dungeons = ATVDungeonManager::Get(this);
	ATVPlayerState* PS = GetTVPlayerState();
	if (Dungeons && PS && Dungeons->IsInsideDungeon(PS))
	{
		Dungeons->LeaveDungeon(PS);
	}
}

void ATVPlayerController::ServerDebugEnterDungeon_Implementation()
{
#if UE_BUILD_SHIPPING
	SendSystemMessage(TEXT("Use the Barrow portal in the Ruined Hills."));
#else
	if (ATVDungeonManager* Dungeons = ATVDungeonManager::Get(this))
	{
		Dungeons->EnterDungeon(GetTVPlayerState());
	}
#endif
}

void ATVPlayerController::ClientLoadDungeonInstance_Implementation(const FString& LevelPackage, const FString& InstanceName, FVector Location)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() != NM_Client)
	{
		return; // listen server / standalone: the server world already loaded it
	}
	if (const TWeakObjectPtr<ULevelStreamingDynamic>* Existing = ClientDungeonInstances.Find(InstanceName))
	{
		if (Existing->IsValid())
		{
			(*Existing)->SetShouldBeLoaded(true);
			(*Existing)->SetShouldBeVisible(true);
			return;
		}
	}
	// Same package path and name override as the server (TVDungeonManager.cpp), so the package names match and the
	// client's visibility report lines up with the server's level.
	ULevelStreamingDynamic::FLoadLevelInstanceParams Params(World, LevelPackage, FTransform(FRotator::ZeroRotator, Location));
	Params.OptionalLevelNameOverride = &InstanceName;
	Params.bInitiallyVisible = true;
	bool bSuccess = false;
	ULevelStreamingDynamic* Streaming = ULevelStreamingDynamic::LoadLevelInstance(Params, bSuccess);
	if (bSuccess && Streaming)
	{
		ClientDungeonInstances.Add(InstanceName, Streaming);
		UE_LOG(LogTV, Log, TEXT("%s Client loading dungeon instance %s (%s)"), TV_NET_LOG_PREFIX, *InstanceName, *Streaming->GetWorldAssetPackageName());
	}
	else
	{
		UE_LOG(LogTV, Error, TEXT("%s Client could not load dungeon instance %s from %s"), TV_NET_LOG_PREFIX, *InstanceName, *LevelPackage);
	}
}

void ATVPlayerController::ClientUnloadDungeonInstance_Implementation(const FString& InstanceName)
{
	TWeakObjectPtr<ULevelStreamingDynamic> Streaming;
	if (ClientDungeonInstances.RemoveAndCopyValue(InstanceName, Streaming) && Streaming.IsValid())
	{
		Streaming->SetIsRequestingUnloadAndRemoval(true);
		UE_LOG(LogTV, Log, TEXT("%s Client unloaded dungeon instance %s"), TV_NET_LOG_PREFIX, *InstanceName);
	}
}

// ================================================================================================ console commands

void ATVPlayerController::TVLogin(const FString& CharacterName, const FString& ClassId, const FString& Account)
{
	ServerLogin(Account, CharacterName, ClassId.IsEmpty() ? DefaultClassId : FName(*ClassId));
}

void ATVPlayerController::TVConnect(const FString& Address, const FString& CharacterName, const FString& ClassId)
{
	ConnectToServer(Address.IsEmpty() ? FString(TEXT("127.0.0.1:7777")) : Address, CharacterName, ClassId.IsEmpty() ? DefaultClassId : FName(*ClassId));
}

void ATVPlayerController::TVChat(const FString& Line) { ServerChat(Line); }
void ATVPlayerController::TVSay(const FString& Message) { ServerSay(Message); }
void ATVPlayerController::TVParty(const FString& Message) { ServerPartyChat(Message); }
void ATVPlayerController::TVInvite(const FString& PlayerName) { ServerPartyInvite(PlayerName); }
void ATVPlayerController::TVAccept() { ServerPartyAccept(); }
void ATVPlayerController::TVDecline() { ServerPartyDecline(); }
void ATVPlayerController::TVLeave() { ServerPartyLeave(); }
void ATVPlayerController::TVKick(const FString& PlayerName) { ServerPartyKick(PlayerName); }
void ATVPlayerController::TVLeader(const FString& PlayerName) { ServerPartyPromote(PlayerName); }
void ATVPlayerController::TVRelease() { ServerReleaseSpirit(); }
void ATVPlayerController::TVLeaveDungeon() { ServerLeaveDungeon(); }
void ATVPlayerController::TVEnterDungeon() { ServerDebugEnterDungeon(); }
void ATVPlayerController::TVSetSlot(int32 SlotNumber, const FString& AbilityId) { ServerSetActionSlot(SlotNumber - 1, FName(*AbilityId)); }
void ATVPlayerController::TVSlot(int32 SlotNumber) { ActivateActionSlot(SlotNumber - 1); }

void ATVPlayerController::TVPartyStatus()
{
	if (!Party)
	{
		UE_LOG(LogTV, Log, TEXT("%s Party status: not in a party"), TV_NET_LOG_PREFIX);
		return;
	}
	UE_LOG(LogTV, Log, TEXT("%s Party status: id %d, %d members"), TV_NET_LOG_PREFIX, Party->GetPartyId(), Party->GetMemberInfos().Num());
	for (const FTVPartyMemberInfo& Info : Party->GetMemberInfos())
	{
		UE_LOG(LogTV, Log, TEXT("%s   %s%s %s L%d HP %.0f%% Res %.0f%%%s"), TV_NET_LOG_PREFIX, *Info.Name, Info.bIsLeader ? TEXT(" (leader)") : TEXT(""),
			*Info.ClassId.ToString(), Info.Level, Info.HealthPercent * 100.f, Info.ResourcePercent * 100.f, Info.bIsInDungeon ? TEXT(" [dungeon]") : TEXT(""));
	}
}
