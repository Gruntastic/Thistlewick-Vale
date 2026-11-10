// Shared enums and structs from Docs/DATA_SCHEMAS.md. Owned by the lead; frozen after Phase 1.
// Domain row structs (abilities, mobs, items, quests...) live with their owning engineer and include this file.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "TVTypes.generated.h"

UENUM(BlueprintType)
enum class ETVRole : uint8 { Tank, Healer, DPS };

UENUM(BlueprintType)
enum class ETVResource : uint8 { None, Rage, Mana, Focus };

UENUM(BlueprintType)
enum class ETVStat : uint8
{
	None,
	// Primary
	Stamina, Strength, Agility, Intellect, Spirit,
	// Secondary
	Crit, Haste, Armor
};

UENUM(BlueprintType)
enum class ETVArmorFamily : uint8 { None, Plate, Leather, Robe };

UENUM(BlueprintType)
enum class ETVWeaponType : uint8 { None, SwordShield, Staff, MaceTome, BowDaggers };

UENUM(BlueprintType)
enum class ETVEquipSlot : uint8 { None, Head, Chest, Legs, Boots, MainHand, OffHand };

UENUM(BlueprintType)
enum class ETVSetTier : uint8 { None, T1_Wanderers, T2_ValeGuard, T3_HollowCrown };

/** Number of rolled stats equals the numeric value (Common = 1 ... Epic = 4). */
UENUM(BlueprintType)
enum class ETVRarity : uint8 { None = 0, Common = 1, Uncommon = 2, Rare = 3, Epic = 4 };

UENUM(BlueprintType)
enum class ETVAbilitySlot : uint8 { Builder, Spender, Utility, Defensive, Ultimate, MobAbility };

UENUM(BlueprintType)
enum class ETVTargetType : uint8 { Enemy, Friendly, Self, Ground, PartyAoE };

UENUM(BlueprintType)
enum class ETVMobTier : uint8 { Normal, Elite, WorldElite, MiniBoss, Boss };

UENUM(BlueprintType)
enum class ETVQuestObjectiveType : uint8 { Kill, Collect, Interact, Defend, Event };

UENUM(BlueprintType)
enum class ETVQuestState : uint8 { NotStarted, Active, ReadyToTurnIn, Completed };

UENUM(BlueprintType)
enum class ETVAssetType : uint8
{
	SkeletalMesh, StaticMesh, Skeleton, Material, MaterialInstance, Texture, Icon,
	AnimSequence, AnimMontage, AnimBlueprint, Niagara, Sound, Music, Widget, Map
};

UENUM(BlueprintType)
enum class ETVAssetStatus : uint8 { Planned, Placeholder, Generated, Cleaned, Imported, Final, FailedKeptPlaceholder };

USTRUCT(BlueprintType)
struct FTVRolledStat
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Item") ETVStat Stat = ETVStat::None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Item") int32 Value = 0;

	bool operator==(const FTVRolledStat& Other) const { return Stat == Other.Stat && Value == Other.Value; }
};

/** One concrete dropped item. Stats are derived on the server from Seed and can be re-derived to validate. */
USTRUCT(BlueprintType)
struct FTVItemInstance
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Item") FName ItemDefId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Item") ETVSetTier SetTier = ETVSetTier::None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Item") ETVEquipSlot Slot = ETVEquipSlot::None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Item") ETVRarity Rarity = ETVRarity::Common;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Item") int32 ItemLevel = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Item") int32 Seed = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Item") TArray<FTVRolledStat> RolledStats;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Item") FGuid InstanceGuid;

	bool IsValid() const { return !ItemDefId.IsNone() && InstanceGuid.IsValid(); }
};

/** DT_AssetManifest row. Row name = manifest ID. */
USTRUCT(BlueprintType)
struct FTVAssetManifestRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Manifest") ETVAssetType AssetType = ETVAssetType::StaticMesh;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Manifest") TSoftObjectPtr<UObject> Asset;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Manifest") ETVAssetStatus Status = ETVAssetStatus::Planned;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Manifest") int32 TriCount = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TV|Manifest") int32 Budget = 0;
};
