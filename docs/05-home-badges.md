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

Offline, the in-memory list starts empty. Won badges are added to it during the session, and the stemmario (badge book) can show them. Nothing writes them to local storage, though, so they are gone after a restart.

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

## Planned: local persistence

`0x24A63C` produces the same blob the game uploaded to the server. The plan:

1. Where the game uploaded, serialize the blob and write it to a file on the SD card.
2. At boot, where it downloaded, read that file and feed it to the matching deserializer (not located yet).
3. States `0x33`/`0x35` will then export real ownership data to the HOME Menu with the official images, sets and shortcuts.
