# 01 – Overview

## Getting the code

1. Dump the **update** title's main content with GodMode9: `A:/title/0004000e/00153600/content/*.app` → *NCCH image options* → *Mount image* → `exefs/.code`. The base title's code is not the one that runs.
2. `.code` is compressed with **backwards LZ (BLZ)**. Decompressed size: `0x357000`.
3. Load it as raw ARM (little-endian, ARMv6K) at base `0x100000`.

## Memory layout (from the exheader)

| Segment | Address | Size |
|---|---|---|
| text | `0x100000` | `0x2D79AC` |
| rodata | `0x3D8000` | `0x4B79C` |
| data | `0x424000` | `0x328F8` (+ bss `0x75068`) |

Relevant services in the exheader: `act:u` (NNID), `boss:U` (SpotPass), `http:C`, `ssl:C`, `soc:U`, `frd:u`, `am:app`.

## Method

There are no symbols. Most of the work went through strings:

- **Dialogue flows.** Every rabbit dialogue is a `talk/<Name>.msflow` file. A pointer table at `0x3D9764` holds 74 of them. Their index is passed to the "play talk" function `0x25A168(talkMgr, index, label)`, so finding `mov r1, #<index>` before that call tells you which code path shows which dialogue.
- **Scene transitions.** Scenes switch through a named event: `0x204E88(scene, {type, "EventName"})`, with names such as `Next`, `Back`, `Crane`, `ServiceClosed`.
- **Game variables.** A list of 256 integers lives at `[0x42C004] + 0x8C`. Getter `0x25A20C`, setter `0x25A084`. A string table at `0x455969` names them: index `0x0C` = `Bonus_FreePlaySum`, `0x17` = `Recovery_FreePlay`, `0x1A` = `ServiceClosed`, and so on.
- **Emulation.** Pure functions, such as date conversion, were run in Unicorn to confirm units.

## Patch list

The source of truth is [`patches/eur-v1.3.1.toml`](../patches/eur-v1.3.1.toml), built by [`tools/build_ips.py`](../tools/build_ips.py). This table summarises it.

| # | Address | Change | Purpose |
|---|---|---|---|
| 1 | `0x33D3C8` | `mov r0,#0x26` | Title: after save-data load go to *LoadingData* instead of *ConnectingNetwork* |
| 2 | `0x33F880` | `mov r0,#0x33` | Title: after local load go to *WritingData* instead of the NEX download |
| 3 | `0x34111C` | `nop` | Title: never enter *DataSalvage* (it needs the server) |
| 4 | `0x34140C` | `mov r0,#0x39` | Title: skip the NEX upload of recovery plays |
| 5 | `0x3412EC` | `mov r0,#0x39` | Title: local-data validation path goes to final setup |
| 6 | `0x2EFB70`–`0x2EFB80` | 5 instructions | Hall: set network mode to 2 and jump to the "login OK" branch |
| 7 | `0x2DFD2C`, `0x2DFD48` | `mov r0,#10` | Crane: read `Bonus_FreePlaySum` as 10 |
| 8 | `0x304E88` | `mov r0,#1` | Practice Catcher: start request returns *success* |
| 9 | `0x2E1490` | `bl` cave | Crane: end-of-session save exports won badges to the HOME Menu, *pending* until they are written |
| 10 | `0x305640` | `mov r0,#1` | Practice Catcher: result save returns *success* |
| 10b | `0x3125A0` | `bl` cave | Tutorial crane (first play): end-of-session save, same as patch 9 |
| 11 | `0x2B034C` → `0x3D79B0` | branch + 25-instruction cave | Remap the clock into the week of the cached schedule (anchor from patch 16) |
| 12 | `0x2CDDB4` → `0x3D7A20` | call + cave | After each `savedata` write, save the progress blob to `extdata:/reclawed` |
| 13 | (in the caves of 9, 10b and the boot load) | | Leave *new player* mode (network mode 0) once the player owns a badge |
| 14 | `0x33F7CC`, `0x2D6398` → cave | branch / call | Load the saved badges once per boot: at title state `0x28`, or at hall entry if the boot skipped `0x28` |
| 15 | `0x25163C` → cave | branch | HOME badge box from the game (Y in the badge book): reload the HOME badge data first, as the in-game badge box does |
| 16 | `0x4237A0` | 20-byte data block | Settings, written by the installer for each console: first day of the SpotPass week, catalog mode |
| 17 | `0x33F60C` → `0x153BC0` | call + cave | Catalog mode 1: skip the two `boss.sarc` loading jobs, so the game plays its built-in machines |

Patch 11's and 12's caves live in the zero padding at the end of `.text` (`0x3D79AC`–`0x3D8000`), which is mapped executable. Patch 16 is in the zero padding at the end of `.rodata` (read-only). Patch 17's cave replaces an SDK worker function the game never runs (checked by SHA-256, see `dead_code` in the TOMLs).

Details for each are on the following pages.
