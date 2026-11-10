# Systems automation tests

The tests are compiled into the game module from `Source/ThistlewickVale/Systems/Tests/` (guarded by `WITH_DEV_AUTOMATION_TESTS`, so they are in Editor and Development builds and not in Shipping). They build their item and stat-pool rows in code, so they do not need imported data tables.

| Test | Checks |
|---|---|
| `ThistlewickVale.Systems.Loot.SameSeedSameItem` | Same seed, rarity and level always give the same item |
| `ThistlewickVale.Systems.Loot.DifferentSeedsDiffer` | Different seeds give different items |
| `ThistlewickVale.Systems.Loot.StatCountAndUniqueness` | Stat count = rarity, no duplicate stats, first stat primary, values in range and at least 1 |
| `ThistlewickVale.Systems.Loot.ValidatorRejectsTampering` | `ValidateItem` accepts real rolls and rejects changed values, stats, rarity, level, slot, seed or extra stats |
| `ThistlewickVale.Systems.Loot.EpicOnlyWhenAllowed` | Epic only drops from tables that list it |
| `ThistlewickVale.Systems.Save.RoundTripJson` | Save data, then JSON, then save data again is lossless and uses the DATA_SCHEMAS section 8 keys |
| `ThistlewickVale.Systems.Save.RoundTripFile` | The same through a file on disk (`Saved/Automation/`, deleted afterwards) |
| `ThistlewickVale.Systems.Save.RejectsTamperedItems` | A forged item is removed on load, along with its equipment entry |
| `ThistlewickVale.Systems.Save.SafeNamesAndVersion` | File names are cleaned; unknown `schemaVersion` and bad JSON are refused |

## Run headless (Windows, from the project folder)

```bat
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%CD%\ThistlewickVale.uproject" ^
  -ExecCmds="Automation RunTests ThistlewickVale.Systems; Quit" ^
  -unattended -nopause -nosplash -NullRHI -log -ReportExportPath="%CD%\Saved\Automation\Reports"
```

The exit code is non-zero if a test fails. The log has a `Test Completed. Result={Success|Fail}` line per test, and the JSON report goes to `Saved/Automation/Reports/index.json`.

## Run in the editor

Open **Tools > Session Frontend > Automation** (or **Tools > Test Automation**), filter on `ThistlewickVale.Systems`, tick the tests and click **Start Tests**.

## Importing the data first (not needed for these tests)

```bat
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%CD%\ThistlewickVale.uproject" ^
  -run=pythonscript -script="%CD%\Tools\ue\import_data.py"
```
