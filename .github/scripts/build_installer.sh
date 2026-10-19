#!/usr/bin/env bash
# Build the installer app (.3dsx and .cia) with the patch manifest already built
# in build/patches/ (run build_all.sh, or tools/build_ips.py + tools/make_manifest.py).
#
# Meant for the devkitpro/devkitarm image, which has devkitARM, libctru and
# citro2d but not libcurl or the CIA tools: 3ds-curl comes from devkitPro's
# repository; makerom and bannertool are downloaded (pinned versions, checked
# against SHA-256) when they aren't on the PATH.
#
# Usage: .github/scripts/build_installer.sh   -> dist/reclawed-installer.{cia,3dsx}
set -euo pipefail

# makerom 0.18.4, not 0.19.0: the 0.19.0 Linux build needs glibc 2.38 and the
# devkitPro image is Debian bookworm (glibc 2.36). Both behave the same here.
MAKEROM_URL=https://github.com/3DSGuy/Project_CTR/releases/download/makerom-v0.18.4/makerom-v0.18.4-ubuntu_x86_64.zip
MAKEROM_SHA256=dd596854718c195c6e3229286be485b122921715555af8ae5cf8e9a465d9f970
BANNERTOOL_URL=https://github.com/Epicpkmn11/bannertool/releases/download/v1.2.2/bannertool.zip
BANNERTOOL_SHA256=e4259c08fe8944ebadd5f4b96f9a8603e5427338074cfc46323fbbc3410d51ed

tools="${RUNNER_TEMP:-/tmp}/ctr-tools"
mkdir -p "$tools"

# fetch <url> <sha256> <zip member> <name>: download, verify, extract one binary.
fetch() {
  local zip="$tools/$4.zip"
  curl -fsSL --retry 3 -o "$zip" "$1"
  echo "$2  $zip" | sha256sum -c -
  unzip -o -j -q "$zip" "$3" -d "$tools"
  chmod +x "$tools/$4"
}

# libcurl + mbedTLS for the online patch check: in devkitPro's repository, not in the image.
if [ ! -f "${DEVKITPRO:-/opt/devkitpro}/portlibs/3ds/lib/libcurl.a" ]; then
  dkp-pacman -Sy --noconfirm --needed 3ds-curl
fi

command -v makerom >/dev/null || fetch "$MAKEROM_URL" "$MAKEROM_SHA256" makerom makerom
command -v bannertool >/dev/null || fetch "$BANNERTOOL_URL" "$BANNERTOOL_SHA256" linux-x86_64/bannertool bannertool
export PATH="$tools:$PATH"

make -C installer cia
mkdir -p dist
cp installer/reclawed-installer.cia installer/reclawed-installer.3dsx dist/
ls -l dist/reclawed-installer.*
