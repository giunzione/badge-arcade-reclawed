# 03 – Hall and cranes

## Hall network sub-state

The hall keeps a second state at `[hall + 0x18C] + 8`. Values `0x44`–`0x4E` are a small connection machine, driven by `0x2EFB14`:

| Value | Meaning |
|---|---|
| `0x44` | Start AC connection |
| `0x45` | Wait for connection |
| `0x46`–`0x48` | NEX login |
| `0x49`–`0x4A` | NEX request (sets network mode, see below) |
| `0x4B` | **Success**: rabbit plays `boss/default/HallAd` |
| `0x4C` / `0x4D` / `0x4E` | Wireless off / no internet / server error |

The handler at `0x2D58A8` turns `0x4C`–`0x4E` into the dialogues `Hall_DisableWireless`, `Hall_Disconnected` and `Hall_DisableServer`.

**Network mode.** The value at `[0x42BE68] + 0x71A04` is set to `2` by state `0x49` through `0x24760C`. The rest of the hall logic (`0x2D56AC`) only runs normally when it equals 2. Otherwise it keeps sending the hall back to `0x44`, which loops forever if you skip only the login. Patch 6 therefore writes `2` there and then jumps to the success branch at `0x2EFDCC`.

**New players.** Before that, on entering the hall (`0x2D6398`), a network mode of `0` means *new player*: the hall switches to sub-state `0x43` and plays the introduction (`Introduction_Day01_*`). Online, the download handlers (`0x33FC10` at boot, `0x30EB94` in game) set the mode to `2` once the downloaded own-badge list is not empty (`0x239E44` returns the total count). Offline nothing downloads, so a fresh save would replay the tutorial forever. The persistence caves apply the same rule after loading the saved badges at boot and at the end of every crane session, including the tutorial crane, whose save request is checked separately at `0x3125A0`.

## Server request status

Every server operation is polled through one function:

```
status = 0x253798(requestMgr, requestType)
  0 = pending, 1 = success, anything else = error
```

| Call site | Type | Context | Error dialogue |
|---|---|---|---|
| `0x2E086C` | 7 | Crane eShop purchase | `GamePlayPre_Disconnected` |
| `0x2E1490` | `0x61` / `0x800` | Crane end-of-session save | `GamePlayEnd_RetrySavingToServer` |
| `0x304E88` | `0x81` | Practice Catcher start | `Training_Start_Disconnected` |
| `0x305640` | `0x101` | Practice Catcher result | `Training_Result_Disconnected` |

Patches 8–10 replace the call with `mov r0, #1`. The game then follows its own success path.

## Free plays

On crane entry (`0x2DFC28`) the game checks these, in order:

1. `Recovery_FreePlay` (`0x17`): plays left from an interrupted session
2. `Bonus_FreePlaySum` (`0x0C`): plays won in the Practice Catcher, capped at 99
3. Otherwise → paid mode (eShop, "€1 for 5 plays")

Patch 7 makes step 2 always read as 10. Every crane visit therefore starts the game's own *free play* session, and a session can't be left halfway through.

## Game variables (partial)

| Index | Name |
|---|---|
| `0x00` | `Crane_PlayCrane` |
| `0x0A`–`0x0C` | `Bonus_FreePlayByWin`, `Bonus_FreePlayByLose`, `Bonus_FreePlaySum` |
| `0x0E`–`0x10` | `Hall_StartUpTalkDone`, `Hall_ReturnHallDone`, `Hall_StartUpToFree` |
| `0x17`–`0x18` | `Recovery_FreePlay`, `Recovery_Penalty` |
| `0x19` | `Info_Entering` |
| `0x1A` | `ServiceClosed` |

The full name list is a comma-separated string at `0x455969`.
