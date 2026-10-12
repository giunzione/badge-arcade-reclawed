# 05 – HOME badges

## Files

HOME Menu badges live in shared extdata `0x000014D1`. Badge Arcade mounts it as the `BADGE:` archive.

### `BadgeMngFile.dat` (`0xD4A8` bytes): the registry

| Offset | Size | Field |
|---|---|---|
| `0x04` | u32 | Set count |
| `0x08` | u32 | Unique badge count |
| `0x10` | u32 | `0xFFFFFFFF` |
| `0x18` | u32 | Total badges (sum of quantities) |
| `0x1C` | u32 | NNID principal ID |
| `0x3E8` | 1000 × `0x28` | Badge entries |
| `0xA028` | 100 × `0x30` | Set entries |
| `0xB2E8` | 360 × `0x18` | Badges placed on the HOME Menu |

Badge entry (offsets from `0x3EC + i × 0x28`): `id` u32, `setId` u32, `dataSlot` u16, `sid` u16, (2), `quantity` u16, (4), `titleIdLow` u32, `titleIdHigh` u32 ×2 (shortcut target, `0xFFFFFFFF` = none).

Set entry (from `0xA038 + i × 0x30`): `setId` u32, `dataSlot` u32, (4), `uniqueBadges` u32, `totalBadges` u32, `firstBadgeIndex` u32.

Badge entries have to be **ordered by set**: a set covers `firstBadgeIndex … +uniqueBadges`.

### `BadgeData.dat` (`0xF4DF80` bytes): names and images

| Offset | Content |
|---|---|
| `0x000000 + slot × 0x8A0` | Set names, 16 languages × `0x8A` UTF-16 |
| `0x035E80 + slot × 0x8A0` | Badge names |
| `0x250F80 + slot × 0x2000` | Set icons |
| `0x318F80 + slot × 0x2800` | Badge 64×64 (RGB565 + A4) |
| `0xCDCF80 + slot × 0xA00` | Badge 32×32 |

Format source: AntiMach's *Advanced Badge Editor*.

### The "wiped badges" case

Switching NNID or network clears the **registry** only. Names and images stay in `BadgeData.dat`, so they can be re-listed. That's what `tools/restore_badges.py` does: it matches badges to sets by name (`"<set name> badge"`).

## Badge ownership: why won badges are lost

The badges you own were **stored on Nintendo's servers**. At boot the game downloaded them through NEX (*DownloadOwnBadge*) into the object at `[0x42BE68]`, and states `0x33`/`0x35` exported them from there into `BADGE:` through the manager at `[0x42BE3C]`:

- `0x2412F4(mgr, 1)`: *export all prizes*
- `0x241204(mgr)`: *export own prizes*

Offline, the in-memory list starts empty. Won badges are added to it during the session (`0x258D70`). Online they were then synced to the server; offline the persistence patch below keeps them in a local file.

## Debug menu

A developer menu table at `0x42D4B0` (entries are `{handler, 0, name, 0}`) is still present. Notable entries:

| Name | Handler | Notes |
|---|---|---|
| Export All Prizes | `0x3008DC` | = state `0x33` |
| Export All Own Prizes | `0x3008F0` | = state `0x35` |
| `[OwnBadge] Memory -> SD` | `0x300BA4` | Serializes own-badge data with `0x24A63C`. The SD write was removed |
| `[OwnBadge] SD -> Memory` | `0x300B88` | Stub: allocates and frees a buffer |
| Load Boss Data from SD | `0x300674` | |
| Delete Management File | `0x300888` | |

## Local persistence

The server never held anything special: it stored one **signed progress blob** that the game itself produced, and handed it back at boot.

| Function | Role |
|---|---|
| `0x24A63C(game, &out, &len)` | Builds the blob at `game+0x418`: format version, two counters (`+0x71A38`, `+0x71A3C`), the own-badge list (`[game+0x71A54]`, MessagePack with the keys `PrizeNodes`, `PrizeCounts`, `OwnedBookPages`), two lists of id/value pairs, then a 32-byte HMAC-SHA256 (`0x24BA10`, key in rodata) |
| `0x239E78(game, buf, len)` | The inverse: checks the HMAC, restores the counters, the list (`0x24A844`) and the pairs. Called by the title after the NEX download (`0x33FBF4`) |
| `0x24A594(mgr, req, buf, len)` | Queues a full upload. Offline it is **not** reached during normal play: winning a badge only adds it to the in-memory list (`0x255E04`), and the end of a crane session sends a small result request, not the blob |

The patch keeps the blob in a file in the game's own extdata:

- **Save:** every time the save manager writes `savedata` (`bl 0x2CE850` at `0x2CDDB4`), a cave builds the blob with `0x24A63C` and writes `[len][blob]` to `extdata:/reclawed`.
- **Load:** title state `0x28` (where the NEX download used to start) reads the file and passes it to `0x239E78`, then continues to *WritingData* (`0x33`). States `0x33`/`0x35` export the list to the HOME Menu at boot.
- **HOME export after a session:** online, the request status function `0x253798` itself exported the prizes (`0x2412F4([0x42BE3C], 0)`) once the server confirmed the end-of-session request. The patch replaces that status call at `0x2E1490` with a cave that runs the same export and reports success. The export is queued as a task: in practice the badge box shows the new badges after the badge book has been opened once, or after the next launch. Badges are placed in the order they were won, not grouped by set.

Three details only showed up on hardware:

1. **The game saves before it loads.** `savedata` is written during boot, before state `0x28`. Saving the blob then would overwrite the file with an empty list. Saves are skipped until the load has run; the flag lives in the unused tail of the `.bss` page (`0x4CBFF0`), which is zero at boot.
2. **Unwritten extdata blocks don't read back.** A file created at a fixed size and only partly written fails to read. The whole file (`0x49000` bytes) is written every time.
3. **The upload path is the wrong hook** (see `0x24A594` above).

File helpers (members of the archive returned by `0x25B6EC([0x450790], 2)`, same calls as the `savedata` writer `0x2CE850`):

| Address | Call |
|---|---|
| `0x25E12C` / `0x25DFD8` | Guard: mounts `extdata:` if needed, unmounts on release |
| `0x270EC4` | Open (`mode` 0 read, 1 write) |
| `0x25B09C` | Create `extdata:/<name>` with a fixed size (`[0x42C140]` is the save manager) |
| `0x2727F0` / `0x270DC8` | Read / write |
| `0x272908` | Close |

The blob is built in the upload buffer at `game+0x418`; the word before it is borrowed for the length header and restored. The game's extdata allows 16 files and uses four (`savedata`, `addSaveData`, `boss.sarc`, `allbadge.sarc`). A missing or damaged file fails the HMAC check and the game starts with an empty list.

### Save data layout notes

- `savedata` (`0xE4D8` bytes) is loaded into the save manager at `+0x88` and written from a second copy at `+0xE560` (`0x2CE3xx` / `0x2CE850`).
- `addSaveData` (`0x1400` bytes): version (`3`), 100 four-byte per-day entries (year−2000, month, day, count; `0x253644`), and a set of up to 1024 ids at `+0x198` with its count at `+0x1198` (add `0x258B4C`, remove `0x258A3C`, contains `0x258CE4`). It has no room for the badge list.
