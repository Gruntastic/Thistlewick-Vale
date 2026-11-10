// Player state: owns the ASC + attribute sets (survive pawn death) and the Systems engineer's components.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "TVPlayerState.generated.h"

class UAbilitySystemComponent;
class UTVAbilitySystemComponent;
class UTVAttributeSet_Vitals;
class UTVAttributeSet_Stats;
class UTVAttributeSet_Combat;
class UTVInventoryComponent;
class UTVEquipmentComponent;
class UTVQuestComponent;
class UTVMountComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTVOnLevelChanged, int32, OldLevel, int32, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTVOnXPChanged, int32, XP, int32, XPToNext);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTVOnGoldChanged, int32, Gold);
/** Named to avoid Net/TVNetTypes.h FTVOnActionBarChanged. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTVOnPlayerActionBarChanged);

UCLASS()
class THISTLEWICKVALE_API ATVPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ATVPlayerState(const FObjectInitializer& ObjectInitializer);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UTVAbilitySystemComponent* GetTVAbilitySystemComponent() const { return AbilitySystem; }

	UFUNCTION(BlueprintPure, Category = "TV|Player") UTVInventoryComponent* GetInventory() const { return Inventory; }
	UFUNCTION(BlueprintPure, Category = "TV|Player") UTVEquipmentComponent* GetEquipment() const { return Equipment; }
	UFUNCTION(BlueprintPure, Category = "TV|Player") UTVQuestComponent* GetQuests() const { return Quests; }
	UFUNCTION(BlueprintPure, Category = "TV|Player") UTVMountComponent* GetMount() const { return Mount; }

	/** Server. Sets class/level/xp, base stats, grants abilities, fills vitals and the default action bar if empty. */
	void InitCharacter(FName InClassId, int32 InLevel, int32 InXP);
	/** Server. Levels up through DT_XPCurve (cap = last row + 1). */
	void AddXP(int32 Amount);
	/** Server. Negative allowed; clamps at 0. */
	void AddGold(int32 Amount);
	void SetCharacterName(const FString& InName) { CharacterName = InName; }

	UFUNCTION(BlueprintPure, Category = "TV|Player") FName GetClassId() const { return ClassId; }
	UFUNCTION(BlueprintPure, Category = "TV|Player") const FString& GetCharacterName() const { return CharacterName; }
	UFUNCTION(BlueprintPure, Category = "TV|Player") int32 GetLevel() const { return Level; }
	UFUNCTION(BlueprintPure, Category = "TV|Player") int32 GetXP() const { return XP; }
	UFUNCTION(BlueprintPure, Category = "TV|Player") int32 GetGold() const { return Gold; }
	UFUNCTION(BlueprintPure, Category = "TV|Player") int32 GetXPToNext() const;

	/** 10 ability ids (NAME_None = empty), saved by Systems as "actionBar". */
	UFUNCTION(BlueprintPure, Category = "TV|Player") const TArray<FName>& GetActionBar() const { return ActionBar; }
	/** Server (save loader) or via ServerSetActionBarSlot from the owning client. */
	void SetActionBar(const TArray<FName>& InActionBar);
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TV|Player") void ServerSetActionBarSlot(int32 SlotIndex, FName AbilityId);

	/** Server: health/resource refilled (rage 0), death and combat state cleared, combo stacks reset. Pawn untouched. */
	void ReviveAtFullHealth();

	UPROPERTY(BlueprintAssignable, Category = "TV|Player") FTVOnLevelChanged OnLevelChanged;
	UPROPERTY(BlueprintAssignable, Category = "TV|Player") FTVOnXPChanged OnXPChanged;
	UPROPERTY(BlueprintAssignable, Category = "TV|Player") FTVOnGoldChanged OnGoldChanged;
	UPROPERTY(BlueprintAssignable, Category = "TV|Player") FTVOnPlayerActionBarChanged OnActionBarChanged;

	static constexpr int32 NumActionSlots = 10;

protected:
	UFUNCTION() void OnRep_Level(int32 OldLevel);
	UFUNCTION() void OnRep_XP();
	UFUNCTION() void OnRep_Gold();
	UFUNCTION() void OnRep_ActionBar();
	void FillDefaultActionBar();
	void SetLevelInternal(int32 NewLevel);

	UPROPERTY(VisibleAnywhere, Category = "TV|Player") TObjectPtr<UTVAbilitySystemComponent> AbilitySystem;
	UPROPERTY() TObjectPtr<UTVAttributeSet_Vitals> VitalsSet;
	UPROPERTY() TObjectPtr<UTVAttributeSet_Stats> StatsSet;
	UPROPERTY() TObjectPtr<UTVAttributeSet_Combat> CombatSet;

	UPROPERTY(VisibleAnywhere, Category = "TV|Player") TObjectPtr<UTVInventoryComponent> Inventory;
	UPROPERTY(VisibleAnywhere, Category = "TV|Player") TObjectPtr<UTVEquipmentComponent> Equipment;
	UPROPERTY(VisibleAnywhere, Category = "TV|Player") TObjectPtr<UTVQuestComponent> Quests;
	UPROPERTY(VisibleAnywhere, Category = "TV|Player") TObjectPtr<UTVMountComponent> Mount;

	UPROPERTY(ReplicatedUsing = OnRep_ActionBar) TArray<FName> ActionBar;

public:
	// Public on request of Systems and Net (they read these directly). Write through InitCharacter/AddXP/AddGold on the server.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "TV|Player") FName ClassId;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "TV|Player") FString CharacterName;
	UPROPERTY(ReplicatedUsing = OnRep_Level, BlueprintReadOnly, Category = "TV|Player") int32 Level = 1;
	UPROPERTY(ReplicatedUsing = OnRep_XP, BlueprintReadOnly, Category = "TV|Player") int32 XP = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Gold, BlueprintReadOnly, Category = "TV|Player") int32 Gold = 0;
};
