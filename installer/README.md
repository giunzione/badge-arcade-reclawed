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
| Nintendo Badge Arcade | it isn't installed |
| Patch | there is no patch for the exact game version installed (region + update version, or the base game's version without the update). The first button then becomes **Report** |
| Machines | never. Which SpotPass week the game's `boss.sarc` holds, or that the built-in machines will be used (see [The SpotPass week and the machines](#the-spotpass-week-and-the-machines)) |
| Patch on the SD | never. It's compared with the patch the app would install, using the record the app keeps next to it (see [Updates](#updates)) |
| `config.ini` | it's missing or not in a format the app fully understands (see below) |

The bottom screen has the controls (D-Pad and <kbd>A</kbd>, or touch):

- **Install** (when nothing blocks it), for the detected game:
  1. the patch is checked (IPS structure) and copied to
     `sd:/luma/titles/<title id>/code.ips`, with a small record next to it
     (`reclawed.txt`, see [Updates](#updates));
  2. in `sd:/luma/config.ini`, `enable_game_patching` is set to `1`. The original
     file is first saved to `sd:/3ds/badge-arcade-reclawed/config.ini.bak`.

  With **Restart after install** ticked (the default) the console restarts after a
  3-second countdown, which can be cancelled.
- **Machines** switches between the **SpotPass week** and the game's **built-in** machines.
  It's only offered when `boss.sarc` is usable; without it the patch always uses the built-in ones.
  It starts on what the installed patch uses. Switching makes **Install** rewrite the patch.
- **Uninstall** deletes the patch (and its record) from the SD, after a confirmation. Game patching stays
  on, since other mods may rely on it.
- **Restart** restarts the console, which is when Luma3DS applies the changes.
- **Exit** (or <kbd>START</kbd>) closes the app.

Don't save Rosalina's settings between installing and restarting: Rosalina writes back
the options Luma booted with, which would turn game patching off again.

### Where the patches come from

At start the app looks for the newest patches online, in a background thread:

1. it downloads `patches.txt` from the [latest release](https://github.com/giunzione/badge-arcade-reclawed/releases/latest), a list of
   every patch with the game version it fits, its size and its SHA-256;
2. if your game's version is listed, it downloads that patch from the same release and
   checks its size and hash.

If that works, the latest release's patch is used (*"Patch from v0.x.y (online)"*), so a
new or fixed patch reaches everyone without reinstalling the app. If the console is
offline, GitHub can't be reached or the download fails, the app uses the copy of the same
files built into it (*"Built-in patch from v0.x.y"*), and says why the online check failed.
HTTPS certificates are checked against Mozilla's root list (`certs/`); a console with a
wrong date and time fails that check, and the app suggests fixing the clock.

When no patch fits the installed version, **Report** shows a QR code. Scanned with a phone,
it opens a new GitHub issue with the region, title ID, game version, app version and
Luma3DS version already filled in (`.github/ISSUE_TEMPLATE/game-version.yml`).

The known releases (title IDs for EUR, USA and JPN) are listed in `source/titles.c`.
Which versions have a patch is decided only by `patches/*.toml`: each one names the game
code it fits (`code_title`, `code_version`), and `tools/make_manifest.py` turns the built
patches into `patches.txt`.

### The SpotPass week and the machines

The patch folds the console's clock into one week of SpotPass data, so it has to know which
week the console has. Every patch carries a 20-byte **settings block** (magic `RCLW` `CFG1`,
the week's first day, the catalog mode; `source/ipscfg.h`). The app fills it in before
writing `code.ips`. The values in the release file are only defaults.

1. It opens the game's SD extdata (`extdata_id` in `source/titles.c`) and reads `boss.sarc`'s
   header and its `Schedule.xml` (`source/spotpass.c`). The whole 25 MB archive is never read.
2. The week starts on the `PrizeCollection` entry's `<DateStartText>` (else the earliest
   `DefaultStage`). The app also counts the days that have machines.

| `boss.sarc` | Shown | The patch gets |
|---|---|---|
| a full week | *SpotPass week from 2018-07-13* | that day as its anchor, mode 0 |
| fewer than 7 days with machines | *SpotPass week 2018-07-13 (5 days only)* (warning) | the same |
| missing | *No SpotPass data: built-in machines* | mode 1: the game's built-in machines |
| damaged or without a schedule | e.g. *boss.sarc damaged: built-in machines* (warning) | mode 1 |
| fine, but **Machines** set to built-in | *Built-in machines (your choice)* | mode 1 |

A patch from before the settings block (an old release) shows *Old patch: SpotPass week can't
be set*: it keeps its fixed week.

### Updates

Every install also writes `sd:/luma/titles/<title id>/reclawed.txt` (Luma3DS ignores it):

```
# Badge Arcade Reclawed: written by the installer app, do not edit.
reclawed-installed 1 v0.4.0 u5136 <SHA-256 of code.ips>
```

At the next start the app compares the `code.ips` on the SD with the patch it would
install now (the latest release's, or its built-in one when offline):

| On the SD | Shown | Install |
|---|---|---|
| the same patch, byte for byte | *Patch installed and up to date* | nothing to do |
| the app's patch from an older release | *Update available: v0.4.0 -> v0.5.0*, button **Update** | replaces it |
| the app's patch from a newer release (e.g. offline with an older app) | *Newer patch on SD (v0.5.0), kept* | never downgrades it |
| the app's patch for another game version (e.g. installed before the game's update) | *Patch for another game version on SD* | replaces it |
| the same patch with other settings (another SpotPass week, or **Machines** switched) | *Patch settings will change* | rewrites it. A newer patch kept on the SD gets its settings rewritten too |
| a `code.ips` the app didn't write: no record, or the record's hash doesn't match (custom build, manual copy, edited file) | *Unknown patch on SD (asks to replace)* | asks first |

Opening the app is enough to see whether a newer patch exists. Release tags compare as
numbers (`v0.10.0` > `v0.9.0`); a pre-release (`v0.5.0-beta`) counts as older than the same
version without a suffix. Patches copied by hand from a release zip show as up to date when
they match the current release, and as unknown otherwise.

### Why the config edit is so careful

If Luma3DS can't parse `config.ini` it resets **all** its options to their defaults.
So the app never rewrites the file: it finds the single `0` of `enable_game_patching`
in the `[boot]` section and overwrites that one byte, then reads the file back to check.
It refuses to touch a file it doesn't fully understand (no `[meta]` version 3.x, the
key missing or repeated, a value other than `0`/`1`), and it doesn't touch Luma setups
that keep their files on the internal memory instead of the SD card.

## Building

Requires [devkitPro](https://devkitpro.org/wiki/Getting_Started) with devkitARM,
libctru and citro2d/citro3d (`3ds-dev`), libcurl (`pacman -S 3ds-curl`, which brings mbedTLS and zlib), plus [makerom](https://github.com/3DSGuy/Project_CTR) and
[bannertool](https://github.com/carstene1ns/3ds-bannertool) on the `PATH` for the CIA.

The app bundles the patch manifest from `../build/patches/`, so build it first:

```bash
python3 tools/build_ips.py       # from the repo root: the patch(es)
python3 tools/make_manifest.py   # build/patches/: patches.txt + patch-*.ips
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

### Testing in an emulator

Test builds (never release them; their version ends in `-fake` / `-test` and the top
screen says *TEST BUILD*) simulate what an emulator can't provide, and log every check
result to `sd:/3ds/badge-arcade-reclawed/test-log.txt`:

| Switch | Effect |
|---|---|
| `FAKE_GAME=1` | pretend Badge Arcade EUR v1.3.1 is installed |
| `FAKE_VERSION=n` | with `FAKE_GAME`: pretend update version `n` instead (`0` = no update), e.g. one with no patch |
| `ONLINE_BASE=url` | look for releases at `url` instead of GitHub |

[`tests/fake_release.py`](tests/fake_release.py) stands in for GitHub releases, with one
scenario per online case (newer patch online, version not listed, missing file, wrong hash,
unreadable list, list too big, 404, slow server, untrusted HTTPS certificate):

```bash
make clean && make FAKE_GAME=1 ONLINE_BASE=http://127.0.0.1:8080
python3 tests/fake_release.py newer      # then start the .3dsx in Azahar
```

Azahar passes the app's network traffic to the PC, so `127.0.0.1` is the PC itself.

For the SpotPass checks, put a `boss.sarc` in Azahar's emulated extdata, at
`sdmc/Nintendo 3DS/<32 zeros>/<32 zeros>/extdata/00000000/00001536/user/boss.sarc` (EUR).
The test log then also shows `spotpass=… first=… catalog=…`.

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
| `source/manifest.c` | reads `patches.txt`, finds the patch for a game version, checks a file's hash; the online/built-in choice |
| `source/online.c`, `source/net.c` | the background download from the latest release (libcurl, 3DS only) |
| `source/report.c` | the pre-filled GitHub issue link for the Report QR code |
| `source/sha256.c` | SHA-256, to verify patches |
| `vendor/qrcodegen/` | QR code generator (Nayuki, MIT), unchanged; see its README |
| `certs/` | Mozilla's root certificates for HTTPS; see its README |
| `source/installer.c` | the install actions (copy patch and its record, enable game patching, uninstall) |
| `source/installed.c` | the record next to the patch (`reclawed.txt`): format, release order, how the SD patch compares |
| `source/luma_config.c` | reads and edits the text of `config.ini` |
| `source/luma_system.c` | asks the running Luma3DS for its version and options (3DS only) |
| `source/fileops.c`, `source/ips.c` | file helpers and the IPS check |
| `source/ipscfg.c` | the patch's settings block: find, read and write it, compare two patches apart from it |
| `source/spotpass.c` | `boss.sarc`: the SARC lookup, the week in `Schedule.xml`, dates |
| `tests/` | PC unit tests for everything except `main.c`, `ui.c`, `probe.c`, `online.c`, `net.c` and `luma_system.c` |
| `gfx/` | images for the top screen, converted by tex3ds at build time: the Reclawed logo (made from `docs/logo.png`) and the small Divisione3DSAntimog "powered by" logo |
| `meta/app.rsf` | makerom settings for the CIA (permissions, services) |
| `meta/icon.png`, `meta/banner.png`, `meta/banner.wav` | HOME Menu icon (48×48), banner (256×128) and banner sound, made from `docs/logo.png` |
| `build/` | build output, never committed. `build/romfs/` gets the patch manifest and certificates at build time |

The app's title ID is `000400000BADA000` (unique ID `0xBADA0`, product code `CTR-H-RCLW`).
