# Reclawed installer

A small 3DS app that installs the Badge Arcade Reclawed patch on the console itself:
it copies `code.ips` to the SD card and turns on Luma3DS *Enable game patching*,
so users don't need a PC. It is meant to be installed with FBI by scanning a QR code.

> [!NOTE]
> Work in progress: the app works but has only been tested in an emulator so far.

## What it does

At start the app checks the console and lists the result on the top screen, one colored
dot per check (green = ok, blue = will be done, yellow = warning, red = blocks the install):

| Check | Blocks the install when |
|---|---|
| Luma3DS | Luma keeps its files on the internal memory (CTRNAND) instead of the SD. If Luma isn't detected at all (e.g. in an emulator) it's only a warning |
| Nintendo Badge Arcade | it isn't installed, or only a region with no bundled patch is (today only EUR has one) |
| Update | the v1.3.1 update is missing or a different version is installed |
| Patch on the SD | never: the same patch shows as installed, a different `code.ips` is replaced |
| `config.ini` | it's missing or not in a format the app fully understands (see below) |

The bottom screen has the controls (D-Pad and <kbd>A</kbd>, or touch):

- **Install** (when nothing blocks it), for the detected game:
  1. the bundled patch is checked (IPS structure) and copied to
     `sd:/luma/titles/<title id>/code.ips`;
  2. in `sd:/luma/config.ini`, `enable_game_patching` is set to `1`. The original
     file is first saved to `sd:/3ds/badge-arcade-reclawed/config.ini.bak`.

  With **Restart after install** ticked (the default) the console restarts after a
  3-second countdown, which can be cancelled.
- **Uninstall** deletes the patch from the SD, after a confirmation. Game patching stays
  on, since other mods may rely on it.
- **Restart** restarts the console, which is when Luma3DS applies the changes.
- **Exit** (or <kbd>START</kbd>) closes the app.

Don't save Rosalina's settings between installing and restarting: Rosalina writes back
the options Luma booted with, which would turn game patching off again.

The known releases (title IDs and the v1.3.1 update version for EUR, USA and JPN) are
listed in `source/titles.c`. A region becomes supported as soon as a patch for it is
added under `patches/`: the app bundles every built patch.

### Why the config edit is so careful

If Luma3DS can't parse `config.ini` it resets **all** its options to their defaults.
So the app never rewrites the file: it finds the single `0` of `enable_game_patching`
in the `[boot]` section and overwrites that one byte, then reads the file back to check.
It refuses to touch a file it doesn't fully understand (no `[meta]` version 3.x, the
key missing or repeated, a value other than `0`/`1`), and it doesn't touch Luma setups
that keep their files on the internal memory instead of the SD card.

## Building

Requires [devkitPro](https://devkitpro.org/wiki/Getting_Started) with devkitARM,
libctru and citro2d/citro3d (`3ds-dev`), plus [makerom](https://github.com/3DSGuy/Project_CTR) and
[bannertool](https://github.com/carstene1ns/3ds-bannertool) on the `PATH` for the CIA.

The app bundles the patches from `../build/`, so build them first (see the main README):

```bash
python3 tools/build_ips.py   # from the repo root
cd installer
make        # reclawed-installer.3dsx, for the Homebrew Launcher
make cia    # reclawed-installer.cia, to install with FBI
make test   # unit tests on the PC (HOSTCC=clang to pick another compiler)
make clean
```

To try the install flow in an emulator without the game, `make clean && make FAKE_GAME=1`
builds a version (marked `-fake`) that pretends Badge Arcade EUR v1.3.1 is installed.

In CI, [`.github/scripts/build_installer.sh`](../.github/scripts/build_installer.sh) runs
the same build in devkitPro's Docker image, fetching makerom and bannertool. Every
release attaches `reclawed-installer.cia` and `.3dsx` with fixed names, so the QR code in
the main README (made with `tools/make_qr.py`) always installs the newest one. GitHub's
"latest" skips pre-releases, so publish installer releases as full releases.

`make compile_commands` does a clean build that also writes `build/compile_commands.json`
for clangd and other editors.

## Layout

| Path | Contents |
|---|---|
| `source/main.c` | input handling and the actions (install, uninstall, restart) |
| `source/ui.c` | drawing both screens with citro2d |
| `source/menu.c` | bottom-screen menu: enabled items, D-Pad movement, touch areas |
| `source/checks.c` | the checks: what blocks the install and what is shown |
| `source/probe.c` | gathers the console state the checks need (3DS only) |
| `source/titles.c` | known Badge Arcade releases (title IDs, v1.3.1 update version) |
| `source/installer.c` | the two install actions (copy patch, enable game patching) |
| `source/luma_config.c` | reads and edits the text of `config.ini` |
| `source/luma_system.c` | asks the running Luma3DS for its version and options (3DS only) |
| `source/fileops.c`, `source/ips.c` | file helpers and the IPS check |
| `tests/` | PC unit tests for everything except `main.c`, `ui.c`, `probe.c` and `luma_system.c` |
| `gfx/` | images for the top screen, converted by tex3ds at build time: the Reclawed logo (made from `docs/logo.png`) and the small Divisione3DSAntimog "powered by" logo |
| `meta/app.rsf` | makerom settings for the CIA (permissions, services) |
| `meta/icon.png`, `meta/banner.png`, `meta/banner.wav` | HOME Menu icon (48×48), banner (256×128) and banner sound, made from `docs/logo.png` |
| `build/` | build output, never committed. `build/romfs/` gets the patches at build time |

The app's title ID is `000400000BADA000` (unique ID `0xBADA0`, product code `CTR-H-RCLW`).
