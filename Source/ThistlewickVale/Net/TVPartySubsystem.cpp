#include "Net/TVPartySubsystem.h"

#include "ThistlewickVale.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Combat/TVPlayerState.h"
#include "Net/TVPartyInfo.h"
#include "Net/TVPlayerController.h"
#include "Net/TVDungeonManager.h"
#include "Net/TVNetPlayerAccess.h"
#include "Net/TVNetTypes.h"

UTVPartySubsystem* UTVPartySubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UTVPartySubsystem>() : nullptr;
}

bool UTVPartySubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

void UTVPartySubsystem::Deinitialize()
{
	Parties.Reset();
	PendingInvites.Reset();
	Super::Deinitialize();
}

bool UTVPartySubsystem::IsServer() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_Client;
}

TArray<ATVPlayerState*> UTVPartySubsystem::GetPartyMembers(const ATVPlayerState* Player) const
{
	TArray<ATVPlayerState*> Result;
	if (const ATVPartyInfo* Party = GetParty(Player))
	{
		for (const TObjectPtr<ATVPlayerState>& Member : Party->GetMembers())
		{
			if (IsValid(Member))
			{
				Result.Add(Member);
			}
		}
	}
	if (Result.Num() == 0 && Player)
	{
		Result.Add(const_cast<ATVPlayerState*>(Player));
	}
	return Result;
}

ATVPartyInfo* UTVPartySubsystem::GetParty(const ATVPlayerState* Player) const
{
	if (!Player)
	{
		return nullptr;
	}
	for (const TObjectPtr<ATVPartyInfo>& Party : Parties)
	{
		if (IsValid(Party) && Party->GetMembers().Contains(Player))
		{
			return Party;
		}
	}
	return nullptr;
}

int32 UTVPartySubsystem::GetPartyKey(const ATVPlayerState* Player) const
{
	if (const ATVPartyInfo* Party = GetParty(Player))
	{
		return Party->GetPartyId();
	}
	return Player ? -(Player->GetPlayerId() + 1) : 0;
}

bool UTVPartySubsystem::IsLeader(const ATVPlayerState* Player) const
{
	const ATVPartyInfo* Party = GetParty(Player);
	return Party ? Party->GetLeader() == Player : true;
}

ATVPlayerState* UTVPartySubsystem::FindPlayerByName(const FString& Name) const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState || Name.IsEmpty())
	{
		return nullptr;
	}
	for (APlayerState* PS : GameState->PlayerArray)
	{
		ATVPlayerState* TVPS = Cast<ATVPlayerState>(PS);
		if (TVPS && TVNet::GetCharacterName(TVPS).Equals(Name, ESearchCase::IgnoreCase))
		{
			return TVPS;
		}
	}
	return nullptr;
}

bool UTVPartySubsystem::Invite(ATVPlayerState* Inviter, ATVPlayerState* Invitee, FString& OutMessage)
{
	if (!IsServer() || !Inviter)
	{
		OutMessage = TEXT("Party commands run on the server.");
		return false;
	}
	if (!Invitee)
	{
		OutMessage = TEXT("No player by that name is online.");
		return false;
	}
	if (Invitee == Inviter)
	{
		OutMessage = TEXT("You cannot invite yourself.");
		return false;
	}
	ATVPartyInfo* Party = GetParty(Inviter);
	if (Party && Party->GetLeader() != Inviter)
	{
		OutMessage = TEXT("Only the party leader can invite.");
		return false;
	}
	if (Party && Party->GetMembers().Num() >= MaxPartySize)
	{
		OutMessage = FString::Printf(TEXT("Your party is full (%d)."), MaxPartySize);
		return false;
	}
	if (GetParty(Invitee))
	{
		OutMessage = FString::Printf(TEXT("%s is already in a party."), *TVNet::GetCharacterName(Invitee));
		return false;
	}

	FPendingInvite& Pending = PendingInvites.FindOrAdd(Invitee);
	Pending.Inviter = Inviter;
	Pending.TimeSeconds = GetWorld()->GetTimeSeconds();

	const FString InviterName = TVNet::GetCharacterName(Inviter);
	if (ATVPlayerController* InviteePC = Cast<ATVPlayerController>(Invitee->GetPlayerController()))
	{
		InviteePC->ClientPartyInviteReceived(InviterName);
		InviteePC->SendSystemMessage(FString::Printf(TEXT("%s invites you to a party. Type /accept or /decline."), *InviterName));
	}
	UE_LOG(LogTV, Log, TEXT("%s Party invite %s -> %s"), TV_NET_LOG_PREFIX, *InviterName, *TVNet::GetCharacterName(Invitee));
	OutMessage = FString::Printf(TEXT("You invited %s to your party."), *TVNet::GetCharacterName(Invitee));
	return true;
}

bool UTVPartySubsystem::InviteByName(ATVPlayerState* Inviter, const FString& InviteeName, FString& OutMessage)
{
	return Invite(Inviter, FindPlayerByName(InviteeName), OutMessage);
}

bool UTVPartySubsystem::Accept(ATVPlayerState* Invitee, FString& OutMessage)
{
	if (!IsServer() || !Invitee)
	{
		return false;
	}
	FPendingInvite Pending;
	if (!PendingInvites.RemoveAndCopyValue(Invitee, Pending))
	{
		OutMessage = TEXT("You have no pending party invite.");
		return false;
	}
	ATVPlayerState* Inviter = Pending.Inviter.Get();
	if (!Inviter || GetWorld()->GetTimeSeconds() - Pending.TimeSeconds > InviteTimeoutSeconds)
	{
		OutMessage = TEXT("That party invite has expired.");
		return false;
	}
	if (GetParty(Invitee))
	{
		OutMessage = TEXT("You are already in a party.");
		return false;
	}

	ATVPartyInfo* Party = GetParty(Inviter);
	if (Party && Party->GetMembers().Num() >= MaxPartySize)
	{
		OutMessage = TEXT("That party is full.");
		return false;
	}
	if (Party && Party->GetLeader() != Inviter)
	{
		OutMessage = TEXT("The inviter is no longer the party leader.");
		return false;
	}

	const int32 InviterOldKey = GetPartyKey(Inviter);
	const int32 InviteeOldKey = GetPartyKey(Invitee);
	if (!Party)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Party = GetWorld()->SpawnActor<ATVPartyInfo>(ATVPartyInfo::StaticClass(), FTransform::Identity, Params);
		if (!Party)
		{
			OutMessage = TEXT("Could not create the party.");
			return false;
		}
		Party->InitParty(NextPartyId++, Inviter);
		Parties.Add(Party);
		AssignPartyToController(Inviter, Party);
		NotifyDungeonKeyChange(Inviter, InviterOldKey);
		UE_LOG(LogTV, Log, TEXT("%s Party %d formed, leader %s"), TV_NET_LOG_PREFIX, Party->GetPartyId(), *TVNet::GetCharacterName(Inviter));
	}

	Party->ServerAddMember(Invitee);
	AssignPartyToController(Invitee, Party);
	NotifyDungeonKeyChange(Invitee, InviteeOldKey);

	const FString Name = TVNet::GetCharacterName(Invitee);
	NotifyMembers(Party, FString::Printf(TEXT("%s joins the party."), *Name), Invitee);
	UE_LOG(LogTV, Log, TEXT("%s Party %d: %s joined (%d/%d)"), TV_NET_LOG_PREFIX, Party->GetPartyId(), *Name, Party->GetMembers().Num(), MaxPartySize);
	OutMessage = FString::Printf(TEXT("You joined %s's party."), *TVNet::GetCharacterName(Party->GetLeader()));
	return true;
}

bool UTVPartySubsystem::Decline(ATVPlayerState* Invitee, FString& OutMessage)
{
	FPendingInvite Pending;
	if (!IsServer() || !Invitee || !PendingInvites.RemoveAndCopyValue(Invitee, Pending))
	{
		OutMessage = TEXT("You have no pending party invite.");
		return false;
	}
	NotifyPlayer(Pending.Inviter.Get(), FString::Printf(TEXT("%s declines your party invite."), *TVNet::GetCharacterName(Invitee)));
	OutMessage = TEXT("Invite declined.");
	return true;
}

bool UTVPartySubsystem::Leave(ATVPlayerState* Player, FString& OutMessage)
{
	ATVPartyInfo* Party = IsServer() ? GetParty(Player) : nullptr;
	if (!Party)
	{
		OutMessage = TEXT("You are not in a party.");
		return false;
	}
	RemoveFromParty(Player, Party, TEXT("leaves the party"));
	OutMessage = TEXT("You left the party.");
	return true;
}

bool UTVPartySubsystem::Kick(ATVPlayerState* Leader, ATVPlayerState* Target, FString& OutMessage)
{
	ATVPartyInfo* Party = IsServer() ? GetParty(Leader) : nullptr;
	if (!Party)
	{
		OutMessage = TEXT("You are not in a party.");
		return false;
	}
	if (Party->GetLeader() != Leader)
	{
		OutMessage = TEXT("Only the party leader can remove members.");
		return false;
	}
	if (!Target || Target == Leader || !Party->GetMembers().Contains(Target))
	{
		OutMessage = TEXT("That player is not in your party.");
		return false;
	}
	NotifyPlayer(Target, TEXT("You have been removed from the party."));
	RemoveFromParty(Target, Party, TEXT("was removed from the party"));
	OutMessage = FString::Printf(TEXT("%s removed."), *TVNet::GetCharacterName(Target));
	return true;
}

bool UTVPartySubsystem::KickByName(ATVPlayerState* Leader, const FString& TargetName, FString& OutMessage)
{
	return Kick(Leader, FindPlayerByName(TargetName), OutMessage);
}

bool UTVPartySubsystem::PromoteLeader(ATVPlayerState* Leader, ATVPlayerState* NewLeader, FString& OutMessage)
{
	ATVPartyInfo* Party = IsServer() ? GetParty(Leader) : nullptr;
	if (!Party || Party->GetLeader() != Leader)
	{
		OutMessage = TEXT("Only the party leader can do that.");
		return false;
	}
	if (!NewLeader || !Party->GetMembers().Contains(NewLeader))
	{
		OutMessage = TEXT("That player is not in your party.");
		return false;
	}
	Party->ServerSetLeader(NewLeader);
	NotifyMembers(Party, FString::Printf(TEXT("%s is now the party leader."), *TVNet::GetCharacterName(NewLeader)));
	OutMessage = TEXT("Leader changed.");
	return true;
}

bool UTVPartySubsystem::PromoteByName(ATVPlayerState* Leader, const FString& NewLeaderName, FString& OutMessage)
{
	return PromoteLeader(Leader, FindPlayerByName(NewLeaderName), OutMessage);
}

void UTVPartySubsystem::HandleLogout(ATVPlayerState* Player)
{
	if (!IsServer() || !Player)
	{
		return;
	}
	PendingInvites.Remove(Player);
	if (ATVPartyInfo* Party = GetParty(Player))
	{
		RemoveFromParty(Player, Party, TEXT("has gone offline"));
	}
}

void UTVPartySubsystem::RemoveFromParty(ATVPlayerState* Player, ATVPartyInfo* Party, const FString& Reason)
{
	const int32 OldKey = Party->GetPartyId();
	const bool bWasLeader = Party->GetLeader() == Player;
	Party->ServerRemoveMember(Player);
	AssignPartyToController(Player, nullptr);
	NotifyDungeonKeyChange(Player, OldKey);

	const FString Name = TVNet::GetCharacterName(Player);
	UE_LOG(LogTV, Log, TEXT("%s Party %d: %s %s"), TV_NET_LOG_PREFIX, OldKey, *Name, *Reason);
	NotifyMembers(Party, FString::Printf(TEXT("%s %s."), *Name, *Reason));

	if (Party->GetMembers().Num() <= 1)
	{
		DisbandParty(Party);
	}
	else if (bWasLeader && Party->GetLeader())
	{
		NotifyMembers(Party, FString::Printf(TEXT("%s is now the party leader."), *TVNet::GetCharacterName(Party->GetLeader())));
	}
}

void UTVPartySubsystem::DisbandParty(ATVPartyInfo* Party)
{
	const int32 PartyId = Party->GetPartyId();
	TArray<TObjectPtr<ATVPlayerState>> Remaining = Party->GetMembers();
	for (const TObjectPtr<ATVPlayerState>& Member : Remaining)
	{
		Party->ServerRemoveMember(Member);
		AssignPartyToController(Member, nullptr);
		NotifyPlayer(Member, TEXT("Your party has been disbanded."));
		NotifyDungeonKeyChange(Member, PartyId);
	}
	Parties.Remove(Party);
	Party->Destroy();
	UE_LOG(LogTV, Log, TEXT("%s Party %d disbanded"), TV_NET_LOG_PREFIX, PartyId);
}

void UTVPartySubsystem::AssignPartyToController(ATVPlayerState* Player, ATVPartyInfo* Party) const
{
	if (ATVPlayerController* PC = Player ? Cast<ATVPlayerController>(Player->GetPlayerController()) : nullptr)
	{
		PC->SetParty(Party);
	}
}

void UTVPartySubsystem::NotifyPlayer(const ATVPlayerState* Player, const FString& Message) const
{
	if (ATVPlayerController* PC = Player ? Cast<ATVPlayerController>(Player->GetPlayerController()) : nullptr)
	{
		PC->SendSystemMessage(Message);
	}
}

void UTVPartySubsystem::NotifyMembers(const ATVPartyInfo* Party, const FString& Message, const ATVPlayerState* Except) const
{
	if (!Party)
	{
		return;
	}
	for (const TObjectPtr<ATVPlayerState>& Member : Party->GetMembers())
	{
		if (Member != Except)
		{
			NotifyPlayer(Member, Message);
		}
	}
}

void UTVPartySubsystem::NotifyDungeonKeyChange(ATVPlayerState* Player, int32 OldKey) const
{
	if (ATVDungeonManager* Dungeons = ATVDungeonManager::Get(this))
	{
		Dungeons->HandlePartyKeyChanged(Player, OldKey, GetPartyKey(Player));
	}
}
