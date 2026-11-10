# Net smoke test: dedicated server + 2 clients

Checks login, spawning, replication between clients, say/party chat, party invite and accept, the party frame data,
dungeon instancing (enter, scaling, leave, unload) and release after death. It runs on the PC with the launcher
engine (5.8.4), so the server runs from the editor binary with `-server`.

Every Net log line starts with `[TVNet]` (category `LogTV`). Logs are in `Saved/Logs/`:
`TV_Server.log`, `TV_Client_Ash.log`, `TV_Client_Bryn.log`.

## 0. One-time setup
1. `Tools\build_editor.bat` must finish with exit code 0. The build needs the Combat and Systems code merged
   (Net includes `Combat/...` and `Systems/...` headers).
2. Create the placeholder maps, unless the Environment Artist's maps are already in place:
   `UnrealEditor.exe ThistlewickVale.uproject -ExecutePythonScript="<repo>\Tools\ue\create_maps.py"`
   This creates `/Game/MMO/Maps/L_Entry`, `L_Thistlewick` (1 km ground, a `PlayerStart` tagged `BindStone`, and a
   Barrow entrance portal 30 m east of spawn) and `L_Barrow` (a test floor, a `TV_DungeonEntrance` target point and an exit portal).
3. Optional: delete `Saved/Characters/Ash_Ash.json` and `Bryn_Bryn.json` to test new characters.

## 1. Launch
`Tools\run_2_clients.bat` (or `run_server.bat`, wait for the server, then `run_client.bat Ash CLS_WARRIOR` and
`run_client.bat Bryn CLS_CLERIC 127.0.0.1:7777 1290`). Set `UE_ROOT` if the engine is not in
`C:\Program Files\Epic Games\UE_5.8`.

Expected, server log:
```
[TVNet] Server ready on map L_Thistlewick (net mode 1)
[TVNet] Login OK account=Ash name=Ash class=CLS_WARRIOR new at X=... Y=... Z=...
[TVNet] Login OK account=Bryn name=Bryn class=CLS_CLERIC new at ...
```
(`loaded` instead of `new` on the second run.) Client logs: `[TVNet] Client login result: OK (Welcome to Thistlewick Vale, Ash.)`.

Pass: both windows show a character at the bind stone. Each client sees the other player move (WASD, right mouse orbit,
wheel zoom, Space jump, R auto-run).

## 2. Chat (client console: press `~`)
Arguments with spaces need quotes.
- Ash: `TVSay "hello vale"`. Server: `[TVNet] [Say] Ash: hello vale`. Both client logs: `[TVNet] Chat [Say] Ash: hello vale`.
- `TVChat "/who"`. Ash's log: `[TVNet] Chat [System] Online (2): Ash, Bryn`.

## 3. Party
- Ash: `TVInvite Bryn` (or `TVChat "/invite Bryn"`). Server: `[TVNet] Party invite Ash -> Bryn`.
  Bryn's log: `[TVNet] Party invite received from Ash` and a System line asking for /accept.
- Bryn: `TVAccept`. Server: `[TVNet] Party 1 formed, leader Ash` and `[TVNet] Party 1: Bryn joined (2/5)`.
- Both: `TVPartyStatus`. Each log lists 2 members with class, level, HP % and resource %, with Ash marked `(leader)`.
- Ash: `TVParty "pull in 3"`. Both clients log `[TVNet] Chat [Party] Ash: pull in 3`. Use `/p` in TVChat for the same result.
- Pass: in Bryn's log the party data replicates (TVPartyStatus is not empty). A third client that is not in the party never
  receives the ATVPartyInfo actor; check with `TVPartyStatus` on it, which prints "not in a party".

## 4. Dungeon instance
- Stand near each other (within 30 m). Ash walks into the portal 30 m east of spawn (or types `TVEnterDungeon`, which works only outside Shipping builds).
  Server:
  ```
  [TVNet] Dungeon instance L_Barrow_TVInst1 loading at X=600000.000 Y=0.000 Z=-200000.000 for party key 1 (package /Game/MMO/Maps/L_Barrow_TVInst1)
  [TVNet] Dungeon instance L_Barrow_TVInst1 ready
  [TVNet] Dungeon instance L_Barrow_TVInst1 scaled for party size 2: health x0.650 damage x0.70 (N spawn points)
  [TVNet] Ash entered dungeon instance L_Barrow_TVInst1 (party key 1, 1 inside)
  [TVNet] Bryn entered dungeon instance L_Barrow_TVInst1 (party key 1, 2 inside)
  ```
  Client logs: `[TVNet] Client loading dungeon instance L_Barrow_TVInst1 (/Game/MMO/Maps/L_Barrow_TVInst1)`.
  A warning `did not report ... visible within 20 s; teleporting anyway` means the client's level name did not match
  the server's. Record it as a bug and include both logs.
- Pass: both players stand on the Barrow floor and see each other. `TVPartyStatus` shows `[dungeon]` for both.
- Isolation, optional with a third client `run_client.bat Cade CLS_MAGE 127.0.0.1:7777 640 400`: Cade enters solo
  and gets `L_Barrow_TVInst2` at X=1100000. Cade never sees Ash or Bryn, and they never see Cade.

## 5. Death and release
- Kill a character, or use any debug damage the Combat Engineer provides. Then `TVRelease`.
  Inside the instance, the server logs `[TVNet] Ash released to the dungeon entrance`; outside it logs `... released to the bind stone`.
  `TVRelease` while alive prints `You are not dead.`
- Wipe: when everyone inside is dead for 5 s, the server logs `[TVNet] Party wiped in L_Barrow_TVInst1; resetting the instance`,
  and both players respawn at the entrance.

## 6. Leave and unload
- Both walk into the exit portal, or type `TVLeaveDungeon`. Server: `[TVNet] Ash left dungeon instance L_Barrow_TVInst1 (1 still inside)`, then
  `... (0 still inside)`, then `[TVNet] Dungeon instance L_Barrow_TVInst1 is empty; unloading in 60 s`, and about 60 s later
  `[TVNet] Dungeon instance L_Barrow_TVInst1 unloaded`. Clients: `[TVNet] Client unloaded dungeon instance L_Barrow_TVInst1`.

## 7. Save and logout
- Wait 60 s. Server: `[TVNet] Saved Ash (autosave): ok` (`FAILED` means UTVSaveSubsystem returned false).
- Close Bryn's window. Server: `[TVNet] Saved Bryn (logout): ok`, then `[TVNet] Party 1: Bryn has gone offline`, then `[TVNet] Party 1 disbanded`, then `[TVNet] Logout Bryn`.
- Run `run_client.bat Bryn CLS_CLERIC` again. Server: `Login OK ... name=Bryn ... loaded at` the saved location.
  A player who logs out inside the dungeon is moved outside before the save and returns at the Barrow entrance in the zone.

## Known limitations
- Every member's client also loads its party's L_Barrow instance, because UE does not replicate dynamically created
  streaming levels. A client never loads another party's instance.
- Players and mobs in the persistent level are kept apart by distance. Instances are 5 km apart and 2 km below the zone,
  well beyond the 150 m default cull distance. Mobs spawn into the instance's own level once ATVSpawnPoint uses
  OverrideLevel (an INTERFACE-REQUEST).
- Relevancy uses legacy replication. `net.Iris.UseIrisReplication=0` in DefaultEngine.ini; Iris would need connection filters.
