# Cross-agent interface ledger (Phase 1)

Engineers work in parallel and cannot see each other's code. These names, header paths and signatures are fixed; reference the other side by `#include` of the listed header even though it is not in your worktree. The lead merges and compiles. If you need something that is not listed, add a `// INTERFACE-REQUEST:` comment and mention it in your report; do not invent a class in someone else's folder.

All paths are under `Source/ThistlewickVale/`. All classes use `THISTLEWICKVALE_API`.

## Combat Engineer owns `Combat/`
- `Combat/TVAbilitySystemComponent.h` — `UTVAbilitySystemComponent : UAbilitySystemComponent`
  - `void SetEquipmentStats(const TMap<ETVStat, float>& Totals)` server; replaces the single infinite "equipment" effect.
  - `FActiveGameplayEffectHandle ApplyBonusEffect(TSubclassOf<UGameplayEffect> Effect, float Magnitude)` / `void RemoveBonusEffect(FActiveGameplayEffectHandle)` for set bonuses.
  - `void GrantAbilitiesForClass(FName ClassId, int32 Level)`; `bool TryActivateAbilityById(FName AbilityId)`.
  - `float GetCooldownRemaining(FName AbilityId) const`, `float GetGCDRemaining() const` (UI).
- `Combat/TVAttributeSets.h` — `UTVAttributeSet_Vitals`, `UTVAttributeSet_Stats` as in DATA_SCHEMAS §3.
- `Combat/TVPlayerState.h` — `ATVPlayerState : APlayerState, IAbilitySystemInterface`
  - Creates the ASC, attribute sets, and these default subobjects from Systems: `UTVInventoryComponent`, `UTVEquipmentComponent`, `UTVQuestComponent`, `UTVMountComponent` (getters `GetInventory()`, `GetEquipment()`, `GetQuests()`, `GetMount()`).
  - Replicated: `FName ClassId`, `FString CharacterName`, `int32 Level`, `int32 XP`, `int32 Gold`.
  - `void InitCharacter(FName ClassId, int32 Level, int32 XP)` server; `void AddXP(int32)`; `void AddGold(int32)`; delegate `OnLevelChanged`.
- `Combat/TVCharacterBase.h` — `ATVCharacterBase : ACharacter, IAbilitySystemInterface`; `ATVPlayerCharacter`; `ATVMobCharacter`
  - Visuals come from manifest IDs. If the manifest asset is a `UStaticMesh` (placeholder) it goes on a `UStaticMeshComponent* PlaceholderMesh`; if `USkeletalMesh`, on the character mesh.
  - `ATVMobCharacter::InitFromMobRow(FName MobId, int32 Level, float HealthScale, float DamageScale)`; `FName GetMobId() const`.
  - `bool IsDead() const`; multicast death; `FName GetDisplayNameId()`.
- `Combat/TVCombatEventsSubsystem.h` — `UTVCombatEventsSubsystem : UWorldSubsystem` (server)
  - `DECLARE_MULTICAST_DELEGATE_TwoParams(FTVOnMobKilled, ATVMobCharacter* /*Mob*/, const TArray<ATVPlayerState*>& /*EligiblePlayers*/)` → `OnMobKilled`.
  - `DECLARE_MULTICAST_DELEGATE_TwoParams(FTVOnPlayerDamaged, ATVPlayerState*, float)` → `OnPlayerDamaged` (mount dismount).
- `Combat/TVTargetingComponent.h` — `UTVTargetingComponent : UActorComponent` on the player controller; `AActor* GetCurrentTarget() const`, `void CycleTarget()`, `void SetTarget(AActor*)`, delegate `OnTargetChanged`.
- `Combat/AI/TVSpawnPoint.h` — `ATVSpawnPoint : AActor` with `FName MobId; int32 Count; float RespawnTime; int32 Level`; `void SetScaling(float Health, float Damage)`; `void SpawnNow()`; `void SetActiveSpawning(bool)`.
- Mount summon ability `GA_TV_SummonMount` (row `ABL_MOUNT_SUMMON`) calls `UTVMountComponent::ServerSetMounted(true)`. Tag `TV.State.Mounted` blocks all other abilities.
- Movement speed: character movement max speed = base × `MoveSpeedMultiplier` attribute.

## Systems Engineer owns `Systems/`
- `Systems/TVItemRows.h` — `FTVItemDefRow`, `FTVStatPoolRow`, `FTVLootTableRow`; `Systems/TVLootLibrary.h` — `UTVLootLibrary` static `RollItem`, `ValidateItem`, `RollLoot(FName LootTableId, int32 Seed) -> TArray<FTVItemInstance>`.
- `Systems/TVInventoryComponent.h` — `UTVInventoryComponent` (20 slots, replicated to owner): `bool AddItem(const FTVItemInstance&)`, `bool RemoveItem(FGuid)`, `const TArray<FTVItemInstance>& GetItems() const`, delegate `OnInventoryChanged`.
- `Systems/TVEquipmentComponent.h` — `UTVEquipmentComponent`: `ServerEquip(FGuid)`, `ServerUnequip(ETVEquipSlot)`, `const FTVItemInstance* GetEquipped(ETVEquipSlot) const`, `int32 GetSetPieceCount(ETVSetTier) const`, delegate `OnEquipmentChanged`; pushes totals to `UTVAbilitySystemComponent::SetEquipmentStats`; equipped visuals replicated to everyone as manifest IDs.
- `Systems/TVQuestComponent.h` — `UTVQuestComponent`: `ServerAcceptQuest(FName)`, `ServerTurnInQuest(FName)`, `ETVQuestState GetQuestState(FName) const`, `void NotifyKill(FName MobId)`, `void NotifyCollect(FName ItemOrNodeId)`, `void NotifyInteract(FName TargetId)`, `void NotifyEvent(FName EventId)`, delegate `OnQuestsChanged`.
- `Systems/TVMountComponent.h` — `UTVMountComponent`: `bool OwnsMount(FName) const`, `void GrantMount(FName)`, `void ServerSetMounted(bool)`, `bool IsMounted() const` (replicated), delegate `OnMountedChanged`.
- `Systems/TVSaveSubsystem.h` — `UTVSaveSubsystem : UGameInstanceSubsystem` (server): `bool LoadCharacter(ATVPlayerState*, const FString& Account, const FString& CharacterName)` (false = new character), `bool SaveCharacter(ATVPlayerState*)`.
- `Systems/TVLootDropSubsystem.h` — listens to `OnMobKilled`, rolls personal loot per eligible player, grants XP/gold, notifies quests.
- `Systems/TVInteractable.h` — `ITVInteractable` + `ATVQuestNode` (glowmoss, lanterns, chests), `ATVNpc` (quest giver/vendor).

## Network & Dungeon Engineer owns `Net/` and `Config/DefaultEngine.ini`, `Tools/run_*.bat`
- `Net/TVGameMode.h` — `ATVGameMode`; `Net/TVGameState.h` — `ATVGameState` (world flags replicated: `bool HasWorldFlag(FName) const`, `void SetWorldFlag(FName)`).
- `Net/TVPlayerController.h` — `ATVPlayerController`: creates `UTVTargetingComponent`; `ServerLogin(const FString& Account, const FString& CharacterName, FName ClassId)`; periodic and logout save through `UTVSaveSubsystem`; `ServerInteract(AActor*)` forwards to `ITVInteractable`.
- `Net/TVPartySubsystem.h` — `UTVPartySubsystem : UWorldSubsystem` (server): `TArray<ATVPlayerState*> GetPartyMembers(const ATVPlayerState*) const` (returns just the player when solo), invite/accept/leave/kick; `Net/TVPartyInfo.h` — `ATVPartyInfo` replicated to members.
- `Net/TVDungeonManager.h` — `ATVDungeonManager`: per-party instance of `L_Barrow`, `void EnterDungeon(ATVPlayerState* Leader)`, `void LeaveDungeon(ATVPlayerState*)`, `static void GetPartyScaling(int32 PartySize, float& OutHealth, float& OutDamage)` (formula in GDD §7); `Net/TVDungeonPortal.h`.

## UI Engineer (Phase 2) owns `UI/`
Reads only: the delegates and getters above. No gameplay logic in widgets.
