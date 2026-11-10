// Player controller: login, Enhanced Input (built in C++ at runtime), targeting, action bar, interact, chat, party
// and dungeon RPCs. UI keys are exposed as delegates for the UI Engineer.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "Net/TVNetTypes.h"
#include "TVPlayerController.generated.h"

class UTVTargetingComponent;
class UTVInputConfig;
class ATVPartyInfo;
class ATVPlayerState;
class ULevelStreamingDynamic;

UCLASS()
class THISTLEWICKVALE_API ATVPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ATVPlayerController();

	static constexpr int32 NumActionSlots = 10;

	//~ APlayerController / AActor
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Targeting")
	UTVTargetingComponent* GetTargeting() const { return Targeting; }

	// ---------------------------------------------------------------- Login
	/** Client -> server. Ignored once logged in. Account defaults to CharacterName when empty. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Login")
	void ServerLogin(const FString& Account, const FString& CharacterName, FName ClassId);

	UFUNCTION(Client, Reliable)
	void ClientLoginResult(bool bSuccess, const FString& Message);

	UPROPERTY(BlueprintAssignable, Category = "Login")
	FTVOnLoginResult OnLoginResult;

	/** Entry map / UI: travel to a server with name and class on the URL (?Name=Ash?Class=CLS_WARRIOR). */
	UFUNCTION(BlueprintCallable, Category = "Login")
	void ConnectToServer(const FString& Address, const FString& CharacterName, FName ClassId, const FString& Account = TEXT(""));

	UFUNCTION(BlueprintPure, Category = "Login")
	bool IsLoggedIn() const { return bLoggedIn; }

	/** Server-side bookkeeping, called by ATVGameMode. */
	void SetPendingUrlLogin(const FString& Account, const FString& CharacterName, FName ClassId);
	bool GetPendingUrlLogin(FString& OutAccount, FString& OutName, FName& OutClass) const;
	void HandleLoginComplete(const FString& Account, const FString& CharacterName, FName ClassId, bool bExistingCharacter);
	const FString& GetAccountName() const { return AccountName; }

	/** Server. Saves through UTVSaveSubsystem. Called every AutosaveInterval seconds and on logout. */
	void SaveCharacterNow(const TCHAR* Reason);

	// ---------------------------------------------------------------- Interaction / death
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Interaction")
	void ServerInteract(AActor* Target);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Death")
	void ServerReleaseSpirit();

	// ---------------------------------------------------------------- Action bar
	/** The action bar lives on ATVPlayerState::ActionBar (10 slots, Combat). This returns a copy for UI convenience. */
	UFUNCTION(BlueprintPure, Category = "ActionBar")
	TArray<FName> GetActionBar() const;

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "ActionBar")
	void ServerSetActionSlot(int32 SlotIndex, FName AbilityId);

	/** Local: activates the ability in a slot (0-9) through UTVAbilitySystemComponent::TryActivateAbilityById. */
	UFUNCTION(BlueprintCallable, Category = "ActionBar")
	bool ActivateActionSlot(int32 SlotIndex);

	UPROPERTY(BlueprintAssignable, Category = "ActionBar")
	FTVOnActionBarChanged OnActionBarChanged;

	// ---------------------------------------------------------------- Chat and party
	/** Client -> server. A line starting with '/' is a command (/invite name, /leave, /p text ...); otherwise say. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Chat")
	void ServerChat(const FString& Line);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Chat")
	void ServerSay(const FString& Message);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Chat")
	void ServerPartyChat(const FString& Message);

	UFUNCTION(Client, Reliable)
	void ClientReceiveChat(ETVChatChannel Channel, const FString& Sender, const FString& Message);

	/** Server helper: system line to this player only. */
	void SendSystemMessage(const FString& Message);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Party") void ServerPartyInvite(const FString& PlayerName);
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Party") void ServerPartyAccept();
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Party") void ServerPartyDecline();
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Party") void ServerPartyLeave();
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Party") void ServerPartyKick(const FString& PlayerName);
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Party") void ServerPartyPromote(const FString& PlayerName);

	UFUNCTION(Client, Reliable)
	void ClientPartyInviteReceived(const FString& InviterName);

	UFUNCTION(BlueprintPure, Category = "Party")
	ATVPartyInfo* GetParty() const { return Party; }

	/** Server: set by UTVPartySubsystem. */
	void SetParty(ATVPartyInfo* NewParty);

	/** Local: called by ATVPartyInfo::OnRep when the member rows change. */
	void NotifyPartyInfoUpdated();

	UPROPERTY(BlueprintAssignable, Category = "Chat") FTVOnChatMessage OnChatMessage;
	UPROPERTY(BlueprintAssignable, Category = "Party") FTVOnPartyInvite OnPartyInviteReceived;
	UPROPERTY(BlueprintAssignable, Category = "Party") FTVOnPartyChanged OnPartyChanged;

	// ---------------------------------------------------------------- Dungeon
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Dungeon")
	void ServerLeaveDungeon();

	/** Server -> client: load the same L_Barrow instance locally (same package name as on the server). */
	UFUNCTION(Client, Reliable)
	void ClientLoadDungeonInstance(const FString& LevelPackage, const FString& InstanceName, FVector Location);

	UFUNCTION(Client, Reliable)
	void ClientUnloadDungeonInstance(const FString& InstanceName);

	// ---------------------------------------------------------------- UI toggles (UI Engineer binds widgets here)
	UPROPERTY(BlueprintAssignable, Category = "UI") FTVOnUIToggle OnToggleBags;
	UPROPERTY(BlueprintAssignable, Category = "UI") FTVOnUIToggle OnToggleCharacter;
	UPROPERTY(BlueprintAssignable, Category = "UI") FTVOnUIToggle OnToggleQuestLog;
	UPROPERTY(BlueprintAssignable, Category = "UI") FTVOnUIToggle OnToggleAbilityBook;
	UPROPERTY(BlueprintAssignable, Category = "UI") FTVOnUIToggle OnToggleMenu;
	UPROPERTY(BlueprintAssignable, Category = "UI") FTVOnUIToggle OnOpenChat;

	// ---------------------------------------------------------------- Console commands (testable before UI exists)
	UFUNCTION(Exec) void TVLogin(const FString& CharacterName, const FString& ClassId, const FString& Account);
	UFUNCTION(Exec) void TVConnect(const FString& Address, const FString& CharacterName, const FString& ClassId);
	UFUNCTION(Exec) void TVChat(const FString& Line);
	UFUNCTION(Exec) void TVSay(const FString& Message);
	UFUNCTION(Exec) void TVParty(const FString& Message);
	UFUNCTION(Exec) void TVInvite(const FString& PlayerName);
	UFUNCTION(Exec) void TVAccept();
	UFUNCTION(Exec) void TVDecline();
	UFUNCTION(Exec) void TVLeave();
	UFUNCTION(Exec) void TVKick(const FString& PlayerName);
	UFUNCTION(Exec) void TVLeader(const FString& PlayerName);
	UFUNCTION(Exec) void TVPartyStatus();
	UFUNCTION(Exec) void TVRelease();
	UFUNCTION(Exec) void TVLeaveDungeon();
	UFUNCTION(Exec) void TVEnterDungeon();
	UFUNCTION(Exec) void TVSetSlot(int32 SlotNumber, const FString& AbilityId);
	UFUNCTION(Exec) void TVSlot(int32 SlotNumber);
	// The same requests the UI and keys send (Net/TVDevCommands.cpp), so scripts can drive a client without a UI.
	UFUNCTION(Exec) void TVInteract();
	UFUNCTION(Exec) void TVAcceptQuest(const FString& QuestId);
	UFUNCTION(Exec) void TVTurnInQuest(const FString& QuestId);
	UFUNCTION(Exec) void TVTabTarget();
	UFUNCTION(Exec) void TVMount();

	// ---------------------------------------------------------------- Dev cheats (server-checked; refused in Shipping)
	// Documented in Tests/Net/README.md. Each sends ServerDevCommand; the server logs "[TVNet] [Dev] ...".
	UFUNCTION(Exec) void TVDamageSelf(float Amount);
	UFUNCTION(Exec) void TVKillTarget();
	UFUNCTION(Exec) void TVSetLevel(int32 NewLevel);
	UFUNCTION(Exec) void TVGrantItem(const FString& ItemDefId, const FString& Rarity, int32 ItemLevel);
	UFUNCTION(Exec) void TVGrantMount(const FString& MountId = TEXT("MNT_DAWNSTAG"));
	UFUNCTION(Exec) void TVCompleteQuest(const FString& QuestId);
	UFUNCTION(Exec) void TVTeleport(float X, float Y, float Z);
	/** Targets the nearest living mob with this MobId (or any mob for "any"). */
	UFUNCTION(Exec) void TVTargetMob(const FString& MobId);
	/** Server logs level, XP, gold, health, dead/mounted/dungeon state, quest progress and item count. */
	UFUNCTION(Exec) void TVStatus();
	/** Server logs every bag and equipped item with its ValidateItem result. */
	UFUNCTION(Exec) void TVListItems();
	/** Client logs the players and mobs this client can see (instance isolation checks). */
	UFUNCTION(Exec) void TVVisible();

protected:
	/** Development shortcut for the smoke test: enter the dungeon without walking to the portal. Disabled in Shipping. */
	UFUNCTION(Server, Reliable)
	void ServerDebugEnterDungeon();

	/** Dev cheat dispatcher (Net/TVDevCommands.cpp). Refused when the server is a Shipping build. */
	UFUNCTION(Server, Reliable)
	void ServerDevCommand(const FString& Command);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Targeting")
	TObjectPtr<UTVTargetingComponent> Targeting;

	UPROPERTY(Transient)
	TObjectPtr<UTVInputConfig> InputConfig;

	UPROPERTY(ReplicatedUsing = OnRep_Party)
	TObjectPtr<ATVPartyInfo> Party;

	UPROPERTY(ReplicatedUsing = OnRep_LoggedIn)
	bool bLoggedIn = false;

	UFUNCTION() void OnRep_Party();
	UFUNCTION() void OnRep_LoggedIn();

	UPROPERTY(EditDefaultsOnly, Category = "Save")
	float AutosaveInterval = 60.f;

	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	float InteractRange = 600.f;

	UPROPERTY(EditDefaultsOnly, Category = "Chat")
	float SayRange = 6000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float MinZoom = 200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float MaxZoom = 1600.f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float ZoomStep = 80.f;

private:
	// Input handlers
	void HandleMove(const FInputActionValue& Value);
	void HandleLook(const FInputActionValue& Value);
	void HandleOrbitStarted();
	void HandleOrbitCompleted();
	void HandleZoom(const FInputActionValue& Value);
	void HandleJumpStarted();
	void HandleJumpCompleted();
	void HandleAutoRun();
	void HandleTabTarget();
	void HandleTargetSelf();
	void HandleTargetParty(int32 PartyIndex);
	void HandleClickSelect();
	void HandleActionSlot(int32 SlotIndex);
	void HandleInteract();
	void HandleMountKey();
	void HandleToggleBags() { OnToggleBags.Broadcast(); }
	void HandleToggleCharacter() { OnToggleCharacter.Broadcast(); }
	void HandleToggleQuestLog() { OnToggleQuestLog.Broadcast(); }
	void HandleToggleAbilityBook() { OnToggleAbilityBook.Broadcast(); }
	void HandleToggleMenu() { OnToggleMenu.Broadcast(); }
	void HandleOpenChat() { OnOpenChat.Broadcast(); }

	void ApplyMappingContexts();
	void UpdateMountedContext();
	void TryAutoLoginFromCommandLine();

	// -TVExec="cmd;wait 5;cmd" / -TVExecFile=<path>: console commands queued on the client after login (not in Shipping).
	void StartDevExecQueue();
	void RunNextDevExec();
	void HandleDevCommand(const FString& Command); // server side of ServerDevCommand
	void DevLog(const FString& Message) const;
	TArray<FString> DevExecQueue;
	FTimerHandle DevExecTimer;
	bool bDevExecStarted = false;
	AActor* FindInteractCandidate() const;
	void ExecuteChatCommand(const FString& Line);
	void BroadcastSay(const FString& Message);
	void BroadcastParty(const FString& Message);
	ATVPlayerState* GetTVPlayerState() const;

	// Server state
	FString AccountName;
	FString PendingUrlAccount;
	FString PendingUrlName;
	FName PendingUrlClass;
	FTimerHandle AutosaveTimer;
	float LastChatTime = -100.f;
	int32 ChatBurst = 0;

	// Local state
	bool bMappingApplied = false;
	bool bMountedContextActive = false;
	bool bAutoRun = false;
	bool bOrbiting = false;
	bool bAutoLoginSent = false;
	float AutoLoginRetryTime = 0.f;
	TMap<FString, TWeakObjectPtr<ULevelStreamingDynamic>> ClientDungeonInstances;
};
