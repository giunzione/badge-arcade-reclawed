#!/usr/bin/env bash
# Build every patches/*.toml and package one zip per region, plus the patch
# manifest the installer app reads (patches.txt + patch-<REGION>-<code>.ips).
# Usage: .github/scripts/build_all.sh [release]
#   -> dist/badge-arcade-reclawed-<REGION>[-release].zip, dist/patches.txt, dist/patch-*.ips
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
  [ -z "${seen[$region]:-}" ] || { echo "::error file=$f::region $region already defined in ${seen[$region]}"; exit 1; }
  seen[$region]=$f
  echo "::group::$region ($f)"
  python tools/build_ips.py --patch "$f"
  echo "::endgroup::"
  (cd "build/$region" && zip -qr "../../dist/badge-arcade-reclawed-${region}${suffix}.zip" luma)
done

python tools/make_manifest.py --release "${1:-dev}"
cp build/patches/* dist/

ls -l dist
