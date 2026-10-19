# Reclawed installer

A small 3DS app that installs the Badge Arcade Reclawed patch on the console itself:
it copies `code.ips` to the SD card and turns on Luma3DS *Enable game patching*,
so users don't need a PC. It is meant to be installed with FBI by scanning a QR code.

> [!NOTE]
> Work in progress. The app installs the patch and enables game patching, but it
> doesn't check the installed game yet (region, version), and the interface is plain text.

## What it does

The top screen shows the Luma3DS version, whether game patching is active, what
`config.ini` says, and whether each bundled patch is already on the SD card.
Press <kbd>A</kbd> to install:

1. Every bundled patch is checked (IPS structure) and copied to
   `sd:/luma/titles/<title id>/code.ips`. A different version already there is replaced.
2. In `sd:/luma/config.ini`, `enable_game_patching` is set to `1`. The original
   file is first saved to `sd:/3ds/badge-arcade-reclawed/config.ini.bak`.

Then restart the console. Don't save Rosalina's settings before restarting: Rosalina
writes back the options Luma booted with, which would turn game patching off again.

### Why the config edit is so careful

If Luma3DS can't parse `config.ini` it resets **all** its options to their defaults.
So the app never rewrites the file: it finds the single `0` of `enable_game_patching`
in the `[boot]` section and overwrites that one byte, then reads the file back to check.
It refuses to touch a file it doesn't fully understand (no `[meta]` version 3.x, the
key missing or repeated, a value other than `0`/`1`), and it doesn't touch Luma setups
that keep their files on the internal memory instead of the SD card.

## Building

Requires [devkitPro](https://devkitpro.org/wiki/Getting_Started) with devkitARM and
libctru, plus [makerom](https://github.com/3DSGuy/Project_CTR) and
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

`make compile_commands` does a clean build that also writes `build/compile_commands.json`
for clangd and other editors.

## Layout

| Path | Contents |
|---|---|
| `source/main.c` | text UI and the install flow |
| `source/installer.c` | the two install actions (copy patch, enable game patching) |
| `source/luma_config.c` | reads and edits the text of `config.ini` |
| `source/luma_system.c` | asks the running Luma3DS for its version and options (3DS only) |
| `source/fileops.c`, `source/ips.c` | file helpers and the IPS check |
| `tests/` | PC unit tests for everything except `main.c` and `luma_system.c` |
| `meta/app.rsf` | makerom settings for the CIA (permissions, services) |
| `meta/icon.png`, `meta/banner.png`, `meta/banner.wav` | HOME Menu icon (48×48), banner (256×128) and banner sound, made from `docs/logo.png` |
| `build/` | build output, never committed. `build/romfs/` gets the patches at build time |

The app's title ID is `000400000BADA000` (unique ID `0xBADA0`, product code `CTR-H-RCLW`).
