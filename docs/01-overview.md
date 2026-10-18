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
| 2 | `0x33F880` | `bl` cave | Title: load saved progress, then go to *WritingData* instead of the NEX download |
| 3 | `0x34111C` | `nop` | Title: never enter *DataSalvage* (it needs the server) |
| 4 | `0x34140C` | `mov r0,#0x39` | Title: skip the NEX upload of recovery plays |
| 5 | `0x3412EC` | `mov r0,#0x39` | Title: local-data validation path goes to final setup |
| 6 | `0x2EFB70`–`0x2EFB80` | 5 instructions | Hall: set network mode to 2 and jump to the "login OK" branch |
| 7 | `0x2DFD2C`, `0x2DFD48` | `mov r0,#10` | Crane: read `Bonus_FreePlaySum` as 10 |
| 8 | `0x304E88` | `mov r0,#1` | Practice Catcher: start request returns *success* |
| 9 | `0x2E1490` | `bl` cave | Crane: end-of-session save returns *success* and exports won badges to the HOME Menu |
| 10 | `0x305640` | `mov r0,#1` | Practice Catcher: result save returns *success* |
| 10b | `0x3125A0` | `bl` cave | Tutorial crane (first play): end-of-session save, same as patch 9 |
| 11 | `0x2B034C` → `0x3D79B0` | branch + 25-instruction cave | Remap the clock into the week of the cached schedule |
| 12 | `0x2CDDB4` → `0x3D7A20` | call + cave | After each `savedata` write, save the progress blob to `extdata:/reclawed` |
| 13 | (in the caves of 9, 10b and the boot load) | | Leave *new player* mode (network mode 0) once the player owns a badge |

Patch 11's and 12's caves live in the zero padding at the end of `.text` (`0x3D79AC`–`0x3D8000`), which is mapped executable.

Details for each are on the following pages.
