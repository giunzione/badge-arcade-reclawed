#!/usr/bin/env bash
# Build every patches/*.toml and package one zip per region, plus the patch
# manifest the installer app reads (patches.txt + patch-<REGION>-<code>.ips).
# Usage: .github/scripts/build_all.sh [release]
#   -> dist/badge-arcade-reclawed-<REGION>[-noupdate][-release].zip, dist/patches.txt, dist/patch-*.ips
#      (build/patches/ holds the same manifest files, for the installer build)
set -euo pipefail
suffix="${1:+-$1}"
rm -rf build dist && mkdir -p dist

shopt -s nullglob
tomls=(patches/*.toml)
[ ${#tomls[@]} -gt 0 ] || { echo "No patch definitions in patches/"; exit 1; }

declare -A seen
for f in "${tomls[@]}"; do
  region=$(python -c 'import sys,tomllib; print(tomllib.load(open(sys.argv[1],"rb")).get("meta",{}).get("region",""))' "$f")
  [ -n "$region" ] || { echo "::error file=$f::[meta] region is missing"; exit 1; }
  # <REGION> for the updated game, <REGION>-noupdate without the update (tools/patch_meta.py)
  name=$(python tools/patch_meta.py "$f")
  [ -z "${seen[$name]:-}" ] || { echo "::error file=$f::$name already defined in ${seen[$name]}"; exit 1; }
  seen[$name]=$f
  echo "::group::$name ($f)"
  python tools/build_ips.py --patch "$f"
  echo "::endgroup::"
  (cd "build/$name" && zip -qr "../../dist/badge-arcade-reclawed-${name}${suffix}.zip" luma)
done

python tools/make_manifest.py --release "${1:-dev}"
cp build/patches/* dist/

ls -l dist
