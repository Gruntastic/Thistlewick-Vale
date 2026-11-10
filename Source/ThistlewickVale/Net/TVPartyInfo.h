// One replicated actor per party. Replicated only to its members (IsNetRelevantFor), refreshed by the server twice a second.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Net/TVNetTypes.h"
#include "TVPartyInfo.generated.h"

class ATVPlayerState;

UCLASS(NotPlaceable)
class THISTLEWICKVALE_API ATVPartyInfo : public AActor
{
	GENERATED_BODY()

public:
	ATVPartyInfo();

	//~ AActor (IsNetRelevantFor signature checked against 5.8.3 Actor.h)
	virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Party")
	const TArray<FTVPartyMemberInfo>& GetMemberInfos() const { return MemberInfos; }

	UFUNCTION(BlueprintPure, Category = "Party")
	ATVPlayerState* GetLeader() const { return Leader; }

	UFUNCTION(BlueprintPure, Category = "Party")
	int32 GetPartyId() const { return PartyId; }

	UFUNCTION(BlueprintPure, Category = "Party")
	bool IsMember(const APlayerState* PlayerState) const;

	/** Fires on clients (and server) when member rows change. */
	UPROPERTY(BlueprintAssignable, Category = "Party")
	FTVOnPartyChanged OnPartyInfoChanged;

	// ---------------------------------------------------------------- Server API (UTVPartySubsystem only)
	void InitParty(int32 InPartyId, ATVPlayerState* InLeader);
	void ServerAddMember(ATVPlayerState* Member);
	void ServerRemoveMember(ATVPlayerState* Member);
	void ServerSetLeader(ATVPlayerState* NewLeader);
	void ServerRefreshMemberInfos();
	const TArray<TObjectPtr<ATVPlayerState>>& GetMembers() const { return Members; }

protected:
	UPROPERTY(Replicated)
	int32 PartyId = 0;

	UPROPERTY(ReplicatedUsing = OnRep_MemberInfos)
	TArray<FTVPartyMemberInfo> MemberInfos;

	UPROPERTY(ReplicatedUsing = OnRep_MemberInfos)
	TObjectPtr<ATVPlayerState> Leader;

	/** Server-authoritative membership list. Not replicated; MemberInfos carries what clients need. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ATVPlayerState>> Members;

	UFUNCTION()
	void OnRep_MemberInfos();
};
