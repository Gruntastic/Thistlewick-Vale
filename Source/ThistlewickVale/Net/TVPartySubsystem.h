// Server-side party bookkeeping: invite, accept, decline, leave, kick, promote; max 5 members.
// Exists on clients too (world subsystems are per world) but every mutating call is a no-op there.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TVPartySubsystem.generated.h"

class ATVPlayerState;
class ATVPartyInfo;
class APlayerState;

UCLASS()
class THISTLEWICKVALE_API UTVPartySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static constexpr int32 MaxPartySize = 5;
	static constexpr float InviteTimeoutSeconds = 60.f;

	static UTVPartySubsystem* Get(const UObject* WorldContext);

	//~ UWorldSubsystem
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

	/** Party members including Player; just Player when solo (INTERFACES.md). */
	TArray<ATVPlayerState*> GetPartyMembers(const ATVPlayerState* Player) const;

	ATVPartyInfo* GetParty(const ATVPlayerState* Player) const;

	/** Stable key per party: PartyId (> 0) when grouped, -(PlayerId + 1) when solo. Used by the dungeon manager. */
	int32 GetPartyKey(const ATVPlayerState* Player) const;

	bool IsLeader(const ATVPlayerState* Player) const;

	/** Character name lookup among connected players (case-insensitive). */
	ATVPlayerState* FindPlayerByName(const FString& Name) const;

	// All return false and fill OutMessage on failure; on success OutMessage is a confirmation for the caller.
	bool Invite(ATVPlayerState* Inviter, ATVPlayerState* Invitee, FString& OutMessage);
	bool InviteByName(ATVPlayerState* Inviter, const FString& InviteeName, FString& OutMessage);
	bool Accept(ATVPlayerState* Invitee, FString& OutMessage);
	bool Decline(ATVPlayerState* Invitee, FString& OutMessage);
	bool Leave(ATVPlayerState* Player, FString& OutMessage);
	bool Kick(ATVPlayerState* Leader, ATVPlayerState* Target, FString& OutMessage);
	bool KickByName(ATVPlayerState* Leader, const FString& TargetName, FString& OutMessage);
	bool PromoteLeader(ATVPlayerState* Leader, ATVPlayerState* NewLeader, FString& OutMessage);
	bool PromoteByName(ATVPlayerState* Leader, const FString& NewLeaderName, FString& OutMessage);

	/** Called by ATVGameMode::Logout. */
	void HandleLogout(ATVPlayerState* Player);

private:
	struct FPendingInvite
	{
		TWeakObjectPtr<ATVPlayerState> Inviter;
		double TimeSeconds = 0.0;
	};

	bool IsServer() const;
	void RemoveFromParty(ATVPlayerState* Player, ATVPartyInfo* Party, const FString& Reason);
	void DisbandParty(ATVPartyInfo* Party);
	void AssignPartyToController(ATVPlayerState* Player, ATVPartyInfo* Party) const;
	void NotifyMembers(const ATVPartyInfo* Party, const FString& Message, const ATVPlayerState* Except = nullptr) const;
	void NotifyPlayer(const ATVPlayerState* Player, const FString& Message) const;
	void NotifyDungeonKeyChange(ATVPlayerState* Player, int32 OldKey) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ATVPartyInfo>> Parties;

	/** Invitee -> pending invite. */
	TMap<TWeakObjectPtr<ATVPlayerState>, FPendingInvite> PendingInvites;

	int32 NextPartyId = 1;
};
