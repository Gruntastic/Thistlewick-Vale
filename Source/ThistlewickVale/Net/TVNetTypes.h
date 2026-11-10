// Shared networking types for the Net module: chat channels, party member rows, UI delegates.
#pragma once

#include "CoreMinimal.h"
#include "TVNetTypes.generated.h"

class ATVPlayerState;

/** Log prefix used by every Net log line so the smoke test can grep for it (see Tests/Net/README.md). */
#define TV_NET_LOG_PREFIX TEXT("[TVNet]")

UENUM(BlueprintType)
enum class ETVChatChannel : uint8
{
	Say,
	Party,
	System
};

/** One row of the party frame. Replicated inside ATVPartyInfo to party members only. */
USTRUCT(BlueprintType)
struct FTVPartyMemberInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Party")
	TObjectPtr<ATVPlayerState> PlayerState = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Party")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Party")
	FName ClassId;

	UPROPERTY(BlueprintReadOnly, Category = "Party")
	int32 Level = 1;

	/** 0..1 */
	UPROPERTY(BlueprintReadOnly, Category = "Party")
	float HealthPercent = 1.f;

	/** 0..1 */
	UPROPERTY(BlueprintReadOnly, Category = "Party")
	float ResourcePercent = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "Party")
	bool bIsInDungeon = false;

	UPROPERTY(BlueprintReadOnly, Category = "Party")
	bool bIsLeader = false;

	UPROPERTY(BlueprintReadOnly, Category = "Party")
	bool bIsDead = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTVOnUIToggle);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FTVOnChatMessage, ETVChatChannel, Channel, const FString&, Sender, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTVOnPartyInvite, const FString&, InviterName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTVOnPartyChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTVOnActionBarChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTVOnLoginResult, bool, bSuccess, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTVOnWorldFlagsChanged);
