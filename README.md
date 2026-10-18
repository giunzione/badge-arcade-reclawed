<p align="center">
  <img src="docs/logo.png" alt="Badge Arcade Reclawed" width="420">
</p>

# Badge Arcade Reclawed

[![Build patch](https://github.com/giunzione/badge-arcade-reclawed/actions/workflows/build.yml/badge.svg)](https://github.com/giunzione/badge-arcade-reclawed/actions/workflows/build.yml)

An offline revival patch for **Nintendo Badge Arcade** on the Nintendo 3DS.

Nintendo shut down the game’s servers on 8 April 2024, and the game has been unplayable ever since.

This project is a small [Luma3DS](https://github.com/LumaTeam/Luma3DS) code patch that removes the server dependency. \
You can enter the arcade again, play the crane machines and the Practice Catcher, and get a different set of machines every day of the week, all without an internet connection.

> [!NOTE]
> This project contains **no Nintendo code or assets**. It ships only an IPS patch (our own instructions) and helper scripts. \
> You need your own copy of the game and your own SpotPass data, dumped from your own console.

## Why?

Every now and then someone online asks whether Badge Arcade can be played again, and the answer was always “not yet”. \
I wanted to see how far a purely client-side approach could go, with no server and no network setup, just a patch you drop on your SD card.

---

## Status

| Feature | Status |
|---|---|
| Boot without servers or NNID login | ✅ Working |
| Arcade hall | ✅ Working |
| Crane machines (10 free plays per visit) | ✅ Working |
| Practice Catcher (bonus machine) | ✅ Working |
| Daily machine rotation, with the real system clock | ✅ Working |
| *Extra:* restoring old badges to the HOME Menu badge box | ✅ Optional side tool, [`tools/restore_badges.py`](tools/restore_badges.py) |
| Keeping newly won badges after closing the game | ✅ Working. Stored in the game's extdata as `reclawed`, see [docs/05-home-badges.md](docs/05-home-badges.md) |
| Theme shop, Miiverse gallery, info corner | ❔ Untested |

The original game stored your collection on Nintendo's servers. The patch keeps it in a local file instead, and sends the badges you win to the HOME Menu badge box at the end of each crane session.

### Starting from scratch

If you never played Badge Arcade, or your save data is gone, the game starts with the rabbit's tutorial as if it were your first visit. Since v0.2.1 this works offline too: the tutorial crane saves your badge, and once it's done the game treats you as a returning player instead of sending you back to the tutorial.

This case was found and tested by a French 3DS fan who reached out right after the first release. Thank you!

> [!NOTE]
> So far the patch has been tested on **two consoles** (a returning player and a brand-new save). Setups different from these, for example another SpotPass week in `boss.sarc` or no NNID, haven't been tried yet. \
> If you try it, please [open an issue](https://github.com/giunzione/badge-arcade-reclawed/issues) and say whether it worked, with your console model and Luma3DS version.

### Planned

- **Badges in the HOME Menu right away.** Today they appear in the badge box after you open the badge book once, or at the next launch. The export is already triggered at the end of each session; it only needs to run without waiting for the badge book.
- **No more fixed schedule date.** The patch maps the clock onto the week stored in the `boss.sarc` it was built for. Reading that date from the game's own data at boot would make it work with any SpotPass week, without custom builds.
- **Faster loading**, if possible. Some of the wait at boot may come from steps meant for the online service. To be investigated, no promises.
- **Other regions** (USA, JPN), with help from people who own those versions.

---

## Requirements

- A Nintendo 3DS / 2DS family console running **Luma3DS** custom firmware
- **Nintendo Badge Arcade, EUR, version 1.3.1**. Base title ID `0004000000153600`, plus its update.
  Other regions are **not supported**, because the patch addresses differ.
- Your console must still have the game's SpotPass data in its extdata (`boss.sarc`).
  This is the case if the game was opened at least once while the servers were up.
- The console must be on its **Nintendo Network ID** account. If you use [Pretendo](https://pretendo.network) through Nimbus, switch back to **Nintendo** before launching the game.

---

## Installation

1. **Back up first** (recommended, not required). Use [Checkpoint](https://github.com/BernardoGiordano/Checkpoint) to back up the game's extdata. If you have badges in your HOME Menu badge box, also dump them with [Simple Badge Injector](https://github.com/AntiMach/simple-badge-injector): the patched game still runs its own badge-update step at boot, so it's worth keeping a copy. Neither tool is needed for the patch to work.
2. Download the zip for your region from the [latest release](https://github.com/giunzione/badge-arcade-reclawed/releases/latest) (e.g. `badge-arcade-reclawed-EUR-….zip`) and extract it to the root of your SD card, so that you get `luma/titles/<title id>/code.ips`.
3. Open the Luma3DS configuration menu (hold <kbd>SELECT</kbd> while powering on) and enable **"Enable game patching"**.
4. Launch Nintendo Badge Arcade.

> [!TIP]
> The game takes **a long time to load**, especially on the title screen. Don't panic: it isn't frozen, just give it a moment.

To uninstall, delete `code.ips`. The installed game is never modified.

### Building from source

The patch is defined as data in [`patches/eur-v1.3.1.toml`](patches/eur-v1.3.1.toml): address, ARM instructions and a description for each change. [`tools/build_ips.py`](tools/build_ips.py) turns it into an `.ips`. Requires Python 3.11+.

```bash
pip install -r requirements.txt
python3 tools/build_ips.py                              # defaults from the TOML [vars]
python3 tools/build_ips.py --set plays=25               # override a variable
python3 tools/build_ips.py --set anchor_date=2018-07-20 # if your boss.sarc covers another week
```

The output goes to `build/<region>/luma/titles/<title id>/code.ips` (for EUR: `build/EUR/luma/titles/0004000000153600/code.ips`). If you dumped and decompressed your own `.code`, add `--check code.bin` to verify every original instruction before writing.

---

## Extra: restoring your old badges to the HOME Menu

> [!TIP]
> **This step is entirely optional and unrelated to the patch.** The patch works without it. It's a small side tool that came out of the research, included because it may help someone. Skip it if your badge box is fine.

If your HOME Menu badge box is empty but you used to own badges, they might still be there and simply not be listed. Switching NNID or network (for example to Pretendo) wipes the badge **registry** (`BadgeMngFile.dat`) but leaves the badge **images** (`BadgeData.dat`) in place.

```bash
python3 tools/restore_badges.py <dump_folder> <output_folder> --dry-run   # preview
python3 tools/restore_badges.py <dump_folder> <output_folder>
```

Copy the two files from `<output_folder>` to `sd:/3ds/SimpleBadgeInjector/` and inject them with Simple Badge Injector.

Limitations: quantities reset to 1, and HOME Menu shortcut badges become normal badges. All Pokémon badges end up in a single "Pokémon" set, because their names don't tell the regions apart.

---

## FAQ

**The rabbit asks if my 3DS is brand new.**
The game can't find a Nintendo Network ID. You are probably on a Pretendo account: switch back to Nintendo with Nimbus.

**The hall is empty or has no machines.**
Your extdata doesn't contain `boss.sarc`. The patch can't provide machine data, because that data belongs to Nintendo.

**Can you add other regions?**
Possibly. It needs the `.code` of that region's v1.3.1 update to locate the same functions. Contributions are welcome: add a `patches/<region>-v1.3.1.toml` with its own `[meta] region` and `title_id`, and CI will build it and ship it as a separate zip.

**I won a badge but it's not in the HOME Menu badge box.**
Open the badge book in the game once, then check the badge box again. If you close the game first, the badges are added the next time you launch it.

**The badges in the HOME Menu are in a strange order.**
They are placed in the order you won them, not grouped by set. Nothing is lost, you can move them around in the badge box.

**Only 10 plays? Why not 99?**
The claw machines won’t let you leave while you still have unspent plays, so burning through 99 of them becomes a chore fast (trust me, I tried!)

**How does it work?**
See the [technical documentation](docs/).

---

## Disclaimer

This is an unofficial fan project, not affiliated with or endorsed by Nintendo. Nintendo Badge Arcade and all related names and assets are property of Nintendo. Use at your own risk and always keep backups.

This project is a proof of concept, developed iteratively with the help of AI. \
The idea, direction and all testing on real hardware are mine; the reverse engineering, patches and documentation were produced with an AI model.

## Credits

- [3dbrew](https://www.3dbrew.org/wiki/Nintendo_Badge_Arcade): SpotPass and RomFS documentation
- [DaniElectra](https://github.com/DaniElectra/badge-arcade-server): earlier server research
- [AntiMach](https://github.com/AntiMach/advanced-badge-editor): Advanced Badge Editor, whose source documents the HOME badge file format
- The [Pretendo Network](https://pretendo.network) team
- A French 3DS fan, for finding and testing the first-boot case

## License

[MIT](LICENSE), covering the patch and scripts in this repository only.
If you build on this work or share it, a credit or a link back to this repository is appreciated!
