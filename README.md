<p align="center">
  <img src="docs/logo.png" alt="Badge Arcade Reclawed" width="420">
</p>

# Badge Arcade Reclawed

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
| Keeping newly won badges after closing the game | ❌ Not yet. See [docs/05-home-badges.md](docs/05-home-badges.md) |
| Theme shop, Miiverse gallery, info corner | ❔ Untested |

Badges you win **are not kept** yet. The original game stored your collection on Nintendo's servers, and making it persistent locally is the next milestone.

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
2. Download the latest release and copy `luma/titles/0004000000153600/code.ips` to the root of your SD card.
3. Open the Luma3DS configuration menu (hold <kbd>SELECT</kbd> while powering on) and enable **"Enable game patching"**.
4. Launch Nintendo Badge Arcade.

To uninstall, delete `code.ips`. The installed game is never modified.

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
Possibly. It needs the `.code` of that region's v1.3.1 update to locate the same functions.

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

## License

[MIT](LICENSE), covering the patch and scripts in this repository only.
