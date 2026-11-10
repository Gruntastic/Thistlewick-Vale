#include "Net/TVPartyInfo.h"

#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Combat/TVPlayerState.h"
#include "Net/TVNetPlayerAccess.h"
#include "Net/TVDungeonManager.h"
#include "Net/TVPlayerController.h"

ATVPartyInfo::ATVPartyInfo()
{
	bReplicates = true;
	bAlwaysRelevant = false;
	bOnlyRelevantToOwner = false;
	SetReplicatingMovement(false);
	SetNetUpdateFrequency(2.f);
	SetMinNetUpdateFrequency(1.f);
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f;
	SetHidden(true);
}

void ATVPartyInfo::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATVPartyInfo, PartyId);
	DOREPLIFETIME(ATVPartyInfo, MemberInfos);
	DOREPLIFETIME(ATVPartyInfo, Leader);
}

bool ATVPartyInfo::IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const
{
	// Legacy replication passes the connection's PlayerController as RealViewer.
	// VERIFY-5.8: Iris ignores IsNetRelevantFor. Config/DefaultEngine.ini forces net.Iris.UseIrisReplication=0 (also the
	// 5.8.3 default in IrisConfig.cpp). If Iris is enabled later, replace this with a per-connection filter
	// (UReplicationSystem::SetConnectionFilter / AddToConnectionFilter) keyed on the members' connections.
	const APlayerController* ViewerPC = Cast<APlayerController>(RealViewer);
	if (!ViewerPC)
	{
		if (const AController* ViewerController = Cast<AController>(RealViewer))
		{
			return IsMember(ViewerController->PlayerState);
		}
		return false;
	}
	return IsMember(ViewerPC->PlayerState);
}

bool ATVPartyInfo::IsMember(const APlayerState* PlayerState) const
{
	if (!PlayerState)
	{
		return false;
	}
	if (HasAuthority())
	{
		for (const TObjectPtr<ATVPlayerState>& Member : Members)
		{
			if (Member == PlayerState)
			{
				return true;
			}
		}
		return false;
	}
	for (const FTVPartyMemberInfo& Info : MemberInfos)
	{
		if (Info.PlayerState == PlayerState)
		{
			return true;
		}
	}
	return false;
}

void ATVPartyInfo::InitParty(int32 InPartyId, ATVPlayerState* InLeader)
{
	PartyId = InPartyId;
	Leader = InLeader;
	Members.Reset();
	if (InLeader)
	{
		Members.Add(InLeader);
	}
	ServerRefreshMemberInfos();
}

void ATVPartyInfo::ServerAddMember(ATVPlayerState* Member)
{
	if (Member && !Members.Contains(Member))
	{
		Members.Add(Member);
		ServerRefreshMemberInfos();
	}
}

void ATVPartyInfo::ServerRemoveMember(ATVPlayerState* Member)
{
	if (Members.Remove(Member) > 0)
	{
		if (Leader == Member)
		{
			Leader = Members.Num() > 0 ? Members[0].Get() : nullptr;
		}
		ServerRefreshMemberInfos();
	}
}

void ATVPartyInfo::ServerSetLeader(ATVPlayerState* NewLeader)
{
	if (NewLeader && Members.Contains(NewLeader))
	{
		Leader = NewLeader;
		ServerRefreshMemberInfos();
	}
}

void ATVPartyInfo::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority())
	{
		ServerRefreshMemberInfos();
	}
}

void ATVPartyInfo::ServerRefreshMemberInfos()
{
	Members.RemoveAll([](const TObjectPtr<ATVPlayerState>& Member) { return !IsValid(Member); });

	const ATVDungeonManager* Dungeons = ATVDungeonManager::Get(this);
	TArray<FTVPartyMemberInfo> NewInfos;
	NewInfos.Reserve(Members.Num());
	for (const TObjectPtr<ATVPlayerState>& Member : Members)
	{
		FTVPartyMemberInfo& Info = NewInfos.AddDefaulted_GetRef();
		Info.PlayerState = Member;
		Info.Name = TVNet::GetCharacterName(Member);
		Info.ClassId = TVNet::GetClassId(Member);
		Info.Level = TVNet::GetCharacterLevel(Member);
		Info.HealthPercent = TVNet::GetHealthPercent(Member);
		Info.ResourcePercent = TVNet::GetResourcePercent(Member);
		Info.bIsInDungeon = Dungeons && Dungeons->IsInsideDungeon(Member);
		Info.bIsLeader = (Member == Leader);
		Info.bIsDead = TVNet::IsPlayerDead(Member);
	}

	// Only dirty the property when something changed, so idle parties cost nothing on the wire.
	bool bChanged = NewInfos.Num() != MemberInfos.Num();
	for (int32 Index = 0; !bChanged && Index < NewInfos.Num(); ++Index)
	{
		const FTVPartyMemberInfo& A = NewInfos[Index];
		const FTVPartyMemberInfo& B = MemberInfos[Index];
		bChanged = A.PlayerState != B.PlayerState || A.Name != B.Name || A.ClassId != B.ClassId || A.Level != B.Level
			|| !FMath::IsNearlyEqual(A.HealthPercent, B.HealthPercent, 0.005f)
			|| !FMath::IsNearlyEqual(A.ResourcePercent, B.ResourcePercent, 0.005f)
			|| A.bIsInDungeon != B.bIsInDungeon || A.bIsLeader != B.bIsLeader || A.bIsDead != B.bIsDead;
	}
	if (bChanged)
	{
		MemberInfos = MoveTemp(NewInfos);
		ForceNetUpdate();
		OnRep_MemberInfos();
	}
}

void ATVPartyInfo::OnRep_MemberInfos()
{
	OnPartyInfoChanged.Broadcast();

	// Tell local controllers so their OnPartyChanged fires too (UI binds to the controller).
	if (UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			ATVPlayerController* PC = Cast<ATVPlayerController>(It->Get());
			if (PC && PC->IsLocalController() && PC->GetParty() == this)
			{
				PC->NotifyPartyInfoUpdated();
			}
		}
	}
}
