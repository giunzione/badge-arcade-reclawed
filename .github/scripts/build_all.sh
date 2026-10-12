#!/usr/bin/env bash
# Build every patches/*.toml and package one zip per region.
# Usage: .github/scripts/build_all.sh [suffix]   -> dist/badge-arcade-reclawed-<REGION>[-suffix].zip
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

ls -l dist
