#!/usr/bin/env python3
"""
make_manifest.py - list the built patches for the installer app (patches.txt).

Reads every patches/*.toml, takes the code.ips that build_ips.py wrote for it
(build/<region>/luma/titles/<title id>/code.ips) and writes to build/patches/:

    patches.txt               the manifest the app reads (format below)
    patch-<REGION>-<c><v>.ips one copy of each patch, named after the game code
                              it fits, e.g. patch-EUR-u5136.ips

These files are attached to every release (the app downloads the latest ones)
and bundled in the app itself (used when the console is offline).

patches.txt:
    reclawed-patches 1 <release>
    <base title id> <u|b><version> <region> <file> <size> <sha256>

Usage:
    python3 tools/make_manifest.py                  # release name "dev"
    python3 tools/make_manifest.py --release v0.4.0

Requires Python 3.11+ (tomllib). Run tools/build_ips.py (or build_all.sh) first.
"""
import argparse
import glob
import hashlib
import os
import re
import shutil
import sys

try:
    import tomllib
except ModuleNotFoundError:
    sys.exit('Python 3.11 or newer is required (tomllib).')

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FORMAT = 1
CODES = {'update': 'u', 'base': 'b'}


def entry_for(toml_path, build_dir):
    """Return (sort key, manifest line, source ips, file name) for one patch definition."""
    with open(toml_path, 'rb') as f:
        meta = tomllib.load(f).get('meta', {})
    where = os.path.relpath(toml_path, ROOT)

    region, tid = meta.get('region', ''), meta.get('title_id', '')
    code, version = meta.get('code_title'), meta.get('code_version')
    if not re.fullmatch(r'[A-Z]{1,7}', region):
        sys.exit(f'{where}: [meta] region must be upper-case letters, got "{region}"')
    if not re.fullmatch(r'[0-9A-Fa-f]{16}', tid):
        sys.exit(f'{where}: [meta] title_id must be 16 hex digits')
    if code not in CODES:
        sys.exit(f'{where}: [meta] code_title must be "update" or "base"')
    if not isinstance(version, int) or not 0 <= version <= 0xFFFF:
        sys.exit(f'{where}: [meta] code_version must be a title version (0-65535)')

    ips = os.path.join(build_dir, region, 'luma', 'titles', tid, 'code.ips')
    if not os.path.isfile(ips):
        sys.exit(f'{where}: {os.path.relpath(ips, ROOT)} not found, run tools/build_ips.py first')
    data = open(ips, 'rb').read()

    c = CODES[code]
    name = f'patch-{region}-{c}{version}.ips'
    line = f'{tid.upper()} {c}{version} {region} {name} {len(data)} {hashlib.sha256(data).hexdigest()}'
    return (tid.upper(), c, version), line, ips, name


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--release', default='dev', help='release name written in the header (e.g. v0.4.0)')
    ap.add_argument('--build', default=os.path.join(ROOT, 'build'), help='build folder of build_ips.py')
    a = ap.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9._-]{1,31}', a.release):
        sys.exit('--release: letters, digits, ".", "_", "-" only (max 31)')

    tomls = sorted(glob.glob(os.path.join(ROOT, 'patches', '*.toml')))
    if not tomls:
        sys.exit('No patch definitions in patches/')
    entries = sorted(entry_for(t, a.build) for t in tomls)
    keys = [e[0] for e in entries]
    if len(set(keys)) != len(keys):
        sys.exit('Two patches target the same game code (title id + version)')

    out = os.path.join(a.build, 'patches')
    shutil.rmtree(out, ignore_errors=True)
    os.makedirs(out)
    for _, _, ips, name in entries:
        shutil.copyfile(ips, os.path.join(out, name))
    with open(os.path.join(out, 'patches.txt'), 'w', newline='\n') as f:
        f.write('# Badge Arcade Reclawed patches, read by the installer app.\n')
        f.write('# <base title id> <u|b><version> <region> <file> <size> <sha256>\n')
        f.write(f'reclawed-patches {FORMAT} {a.release}\n')
        for _, line, _, _ in entries:
            f.write(line + '\n')
    print(f'Wrote {os.path.relpath(out, ROOT)}/patches.txt ({len(entries)} patches, release {a.release})')


if __name__ == '__main__':
    main()
