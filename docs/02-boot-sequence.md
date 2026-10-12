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

## Notes

- `0x2A` and the `ServiceClosed` scene are a built-in "arcade closed" mode with only the badge box, still reachable in the code. The decision uses a sum of owned-badge counts (`0x239E44` on `[0x42BE68] + 0x71A54`).
- The NNID check matters: save data and HOME badge data are bound to the NNID, and a mismatch triggers *"your save data has been deleted"* (`ErrorApplet` `NnidConflict` / `NnidBadgeConflict`).
