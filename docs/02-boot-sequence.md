# 02 – Boot sequence

The title scene is a state machine with **61 states** (`0x00`–`0x3C`). The current state is stored at `this + 0x1BC`, and the dispatcher at `0x33C9B8` jumps through a table at `0x33C9DC`.

## Main path (original game)

| State | Handler | What it does |
|---|---|---|
| `0x00`–`0x01` | `0x3434D4`, `0x3432FC` | Title screen, wait for input or timeout |
| `0x02` | `0x3434EC` | Read the NNID through `act`. Empty → `0x03`, present → `0x04` |
| `0x03` | | Dialogue `Start_NeedNNID` ("Is your 3DS brand new?") |
| `0x04` | `0x33D36C` | Load save data. OK → `0x05`, error → `0x1D` (`Start_SaveExtDataFailed`) |
| `0x05`–`0x06` | | "Connecting to Nintendo Network…": AC connect, then wireless off → `0x08`, no internet → `0x07` |
| `0x09`–`0x14` | | Login, version check, SpotPass (`playinfo`, `data`, `allbadge`) |
| `0x17`, `0x2C` | | "Downloading NEX data": own badges and free plays |
| `0x26` | `0x33F5A4` | **LoadingData**: five async local loads (`boss.sarc`, `allbadge.sarc`, …) |
| `0x27` | `0x34122C` | Validate local data. OK → `0x05`, missing → `0x2A` |
| `0x28` | `0x33F7CC` | After the load. Emergency notice → `0x29`, otherwise → `0x2B` (NEX) |
| `0x2A` | `0x33D864` | **Service end**. If the player owns badges → `ServiceClosed` scene, otherwise `Start_ServiceEnd` and exit |
| `0x2F`, `0x37` | | "Uploading NEX data" |
| `0x33` | `0x340990` | **WritingData**: HOME badge manager, *export all prizes* |
| `0x34` | `0x3410E4` | Badge version check. May go to *DataSalvage* `0x1F` |
| `0x35` | `0x33EBBC` | **UpdatingBadgeData**: HOME badge manager, *export own prizes* |
| `0x36` | `0x34137C` | Pending recovery plays → `0x37` (upload), else → `0x39` |
| `0x39` | `0x33FD4C` | **Final setup**: schedule, stats, play report, then scene event `Crane`/`Next` |

Error dialogues by state: `0x07` Start_Disconnected, `0x08` Start_DisableWireless, `0x0C` Start_DisableServer, `0x0D` Start_VersionUp, `0x32` Start_NEXMntStarted.

## Patched path

```
0x02 NNID ─► 0x04 save data ─► 0x26 LoadingData ─► 0x28 ─► 0x33 WritingData
     ─► 0x34 version check (no salvage) ─► 0x35/0x36 ─► 0x39 final setup ─► hall
```

Every state touching the network is skipped. The local-only states that build the HOME badge data (`0x33`–`0x36`) are kept.

## LoadingData and the catalog mode

*LoadingData* (`0x26`, `0x33F5A4`) runs five jobs one after the other. Their status words are at scene `+0x3FC`, `+0x400`, `+0x404`, `+0x408` and `+0x40C` (0 idle, 1 running, 2 done, anything else → state `0x1D`, the SD-card error). Each job's callback sets its own word:

| Job | Callback | What it does |
|---|---|---|
| 1 (`+0x3FC`) | `0x3423A0` | opens `boss.sarc` and `allbadge.sarc` from extdata as archives 0 and 1. Fails with **3** when they're missing |
| 2 (`+0x400`) | `0x3428BC` | main schedule, `0x24DD70(gGameData[+0x719F8], …, archive 0)`: `boss.sarc`'s `Schedule.xml` (empty if archive 0 isn't open) |
| 3 (`+0x404`) | `0x3429CC` | default schedule, same with archive 5 (RomFS) into `gGameData[+0x719FC]`: valid 2000–2030, 30 built-in machines |
| 4 (`+0x408`) | `0x3432CC` | prize catalog setup `0x248038` (the machines' resources) |
| 5 (`+0x40C`) | `0x341940` | the message flows named in the main schedule (`Boss/StartUp.msflow`, …). Fails with **3** when there are none |

Jobs 1 and 5 are why a console without `boss.sarc` stops at the SD-card error.

The installer writes a **catalog mode** into the patch (settings block, patch 16):

- **0, SpotPass week** (default when `boss.sarc` is usable): nothing changes, and the game plays the cached week with the clock remapped to it ([04 – Time](04-time.md)).
- **1, built-in**: patch 17 replaces the first status read in *LoadingData* (`ldr r0, [r4, #0x3fc]` at `0x33F60C`) with a call to a cave. While a status is still 0, the cave marks jobs 1 and 5 as done. Archives 0 and 1 then stay closed and the main schedule stays empty, so the game runs on the default schedule and the built-in catalog `pc.sarc`. That's the same as a console that never downloaded SpotPass data. [badge-arcade-offline](https://github.com/GrouchoGlasses42/badge-arcade-offline) sets the same two status words in its own start-up code.

Side effect: without `allbadge.sarc`, the collection viewer may not show badges that only came with SpotPass data. Their ownership stays in the save.

The cave runs on every frame of the state and only touches a status that is still 0, so mode 0 behaves exactly as before. Not verified: a reload path, `0x240864` (callers `0x300904`, `0x30EA64`), reads both schedules again from archives 0 and 5. It's gated by the same extdata file check as job 1 (`0x26FCE0`), and offline it hasn't been seen running.

## Notes

- `0x2A` and the `ServiceClosed` scene are a built-in "arcade closed" mode with only the badge box, still reachable in the code. The decision uses a sum of owned-badge counts (`0x239E44` on `[0x42BE68] + 0x71A54`).
- The NNID check matters: save data and HOME badge data are bound to the NNID, and a mismatch triggers *"your save data has been deleted"* (`ErrorApplet` `NnidConflict` / `NnidBadgeConflict`).
