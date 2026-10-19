# Reclawed installer

A small 3DS app that installs the Badge Arcade Reclawed patch on the console itself:
it copies `code.ips` to the SD card and turns on Luma3DS *Enable game patching*,
so users don't need a PC. It is meant to be installed with FBI by scanning a QR code.

> [!NOTE]
> Work in progress. Right now this is only the app skeleton: it starts, shows its
> version and exits with START. Nothing is written to the SD card yet.

## Building

Requires [devkitPro](https://devkitpro.org/wiki/Getting_Started) with devkitARM and
libctru, plus [makerom](https://github.com/3DSGuy/Project_CTR) and
[bannertool](https://github.com/carstene1ns/3ds-bannertool) on the `PATH` for the CIA.

```bash
cd installer
make        # reclawed-installer.3dsx, for the Homebrew Launcher
make cia    # reclawed-installer.cia, to install with FBI
make clean
```

## Layout

| Path | Contents |
|---|---|
| `source/` | C sources |
| `meta/app.rsf` | makerom settings for the CIA (permissions, services) |
| `meta/icon.png`, `meta/banner.png`, `meta/banner.wav` | HOME Menu icon (48×48), banner (256×128) and banner sound, made from `docs/logo.png` |
| `build/` | build output, never committed. `build/romfs/` is assembled at build time |

The app's title ID is `000400000BADA000` (unique ID `0xBADA0`, product code `CTR-H-RCLW`).
