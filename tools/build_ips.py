#!/usr/bin/env python3
"""
build_ips.py - assemble a patch definition (TOML) into a Luma3DS code.ips

The patch itself lives in patches/*.toml; this script only reads, assembles and
packs it. You never need to edit this file to change the patch.

Usage:
    python3 tools/build_ips.py                                  # patches/eur-v1.3.1.toml -> build/EUR/...
                                                                # (game without update: build/<REGION>-noupdate/...)
    python3 tools/build_ips.py --set plays=25                   # override a [vars] value
    python3 tools/build_ips.py --spotpass path/to/boss.sarc     # play YOUR SpotPass week
    python3 tools/build_ips.py --check code.bin                 # verify against YOUR decompressed .code
    python3 tools/build_ips.py --patch patches/other.toml --out somewhere/code.ips

The patch's settings block says which machines the game plays (installer/
source/ipscfg.h). As built it selects the game's built-in machines, which work
on any console. With --spotpass, the week is read from your own boss.sarc
(tools/spotpass_week.py, the same rules as the installer app) and the patch
plays that week instead. The installer app does this on the console by itself.

Patch entries:
    address        where the bytes go (virtual address in the .code)
    asm            ARM assembly; may use ${var}
    original       optional: the instruction expected there (checked by --check)
    require_empty  optional: the bytes there must be zero (padding)
    dead_code      optional: { size, sha256 } of a function the game never runs,
                   whose bytes the patch replaces (checked by --check)

Requires Python 3.11+ (tomllib) and keystone-engine (see requirements.txt).
"""
import argparse
import hashlib
import os
import string
import sys

try:
    import tomllib
except ModuleNotFoundError:
    sys.exit('Python 3.11 or newer is required (tomllib).')
try:
    from keystone import Ks, KsError, KS_ARCH_ARM, KS_MODE_ARM
except ImportError:
    sys.exit('keystone-engine is missing: pip install -r requirements.txt')

from patch_meta import build_name    # tools/patch_meta.py: build/<name>/ folder per patch
import spotpass_week                 # tools/spotpass_week.py: the week in a boss.sarc

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_PATCH = os.path.join(ROOT, 'patches', 'eur-v1.3.1.toml')
WEEK_MS = 7 * 24 * 60 * 60 * 1000       # the clock remap folds time into one week
SETTINGS_MAGIC = b'RCLWCFG1'            # the settings block: magic, anchor (u64 ms), mode (u32)
MODE_SPOTPASS = 0


def parse_value(raw):
    """Turn a --set string into int / str."""
    try:
        return int(raw, 0)
    except ValueError:
        return raw


def resolve_vars(vars_):
    """Check the [vars] values and add derived ones used by the asm (week length)."""
    v = dict(vars_)
    if 'plays' in v and not (isinstance(v['plays'], int) and 1 <= v['plays'] <= 255):
        sys.exit('plays must be an integer between 1 and 255')
    v['week_ms'] = WEEK_MS
    return {k: str(x) for k, x in v.items()}


def subst(text, vars_, where):
    try:
        return string.Template(text).substitute(vars_)
    except KeyError as e:
        sys.exit(f'{where}: unknown variable ${{{e.args[0]}}}')


def assemble(ks, src, addr, where):
    try:
        enc, _ = ks.asm(src, addr)
    except KsError as e:
        sys.exit(f'{where}: assembly error: {e}')
    if not enc:
        sys.exit(f'{where}: assembled to nothing')
    return bytes(enc)


def build_ips(records, base):
    out = bytearray(b'PATCH')
    for addr, data in sorted(records):
        off = addr - base
        if not 0 <= off < 0x1000000:
            sys.exit(f'{addr:#x}: outside IPS range')
        for i in range(0, len(data), 0xFFFF):
            chunk = data[i:i + 0xFFFF]
            out += (off + i).to_bytes(3, 'big') + len(chunk).to_bytes(2, 'big') + chunk
    return bytes(out + b'EOF')


def set_spotpass_week(records, week):
    """Write anchor = `week` and the SpotPass mode into the one settings block
    among `records` (found by its magic, as the installer does)."""
    hits = [(i, at) for i, (_, data) in enumerate(records)
            for at in range(0, len(data) - 19, 4) if data[at:at + 8] == SETTINGS_MAGIC]
    if len(hits) != 1:
        sys.exit(f'--spotpass: expected one settings block in the patch, found {len(hits)}')
    i, at = hits[0]
    addr, data = records[i]
    block = SETTINGS_MAGIC + spotpass_week.date_ms(week).to_bytes(8, 'little') + MODE_SPOTPASS.to_bytes(4, 'little')
    records[i] = (addr, data[:at] + block + data[at + 20:])


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--patch', default=DEFAULT_PATCH, help='patch definition (TOML)')
    ap.add_argument('--set', action='append', default=[], metavar='KEY=VALUE', help='override a [vars] value')
    ap.add_argument('--check', metavar='CODE_BIN', help='decompressed .code to verify original instructions')
    ap.add_argument('--out', help='output path (default: build/luma/titles/<title_id>/code.ips)')
    ap.add_argument('--spotpass', metavar='BOSS_SARC',
                    help="your game's boss.sarc: play its SpotPass week instead of the built-in machines")
    a = ap.parse_args()

    week = None
    if a.spotpass:
        try:
            week, days, machines = spotpass_week.read_week(a.spotpass)
        except (OSError, spotpass_week.SpotPassError) as e:
            sys.exit(f'--spotpass: {e}')
        print(f'SpotPass week from {week.isoformat()}: {machines} machines on {days} of 7 days')
        if days < 7:
            print(f'  warning: only {days} days of that week have machines; the others will be empty')

    with open(a.patch, 'rb') as f:
        spec = tomllib.load(f)
    meta, patches = spec.get('meta', {}), spec.get('patch', [])
    base = meta.get('code_base', 0x100000)
    if not patches:
        sys.exit(f'{a.patch}: no [[patch]] entries')

    vars_ = dict(spec.get('vars', {}))
    for kv in a.set:
        if '=' not in kv:
            sys.exit(f'--set expects KEY=VALUE, got "{kv}"')
        k, raw = kv.split('=', 1)
        if k not in vars_:
            sys.exit(f'--set: unknown variable "{k}" (known: {", ".join(vars_)})')
        vars_[k] = parse_value(raw)
    vars_ = resolve_vars(vars_)

    ks = Ks(KS_ARCH_ARM, KS_MODE_ARM)
    code = open(a.check, 'rb').read() if a.check else None
    records, seen, errors = [], [], 0

    for i, p in enumerate(patches):
        where = f'patch #{i + 1} @ {p.get("address", "?"):#x}' if 'address' in p else f'patch #{i + 1}'
        if 'address' not in p or 'asm' not in p:
            sys.exit(f'{where}: "address" and "asm" are required')
        addr = p['address']
        data = assemble(ks, subst(p['asm'], vars_, where), addr, where)
        desc = subst(p.get('description', ''), vars_, where)
        dead = p.get('dead_code')
        if dead is not None and len(data) > dead['size']:
            sys.exit(f'{where}: {len(data)} bytes, more than the {dead["size"]}-byte function it replaces')

        for s, e in seen:
            if addr < e and s < addr + len(data):
                sys.exit(f'{where}: overlaps another patch at {s:#x}')
        seen.append((addr, addr + len(data)))

        if code is not None:
            cur = code[addr - base: addr - base + len(data)]
            if p.get('require_empty') and any(cur):
                errors += 1
                print(f'  MISMATCH {addr:#x}: expected empty padding')
            elif dead is not None:
                if hashlib.sha256(code[addr - base: addr - base + dead['size']]).hexdigest() != dead['sha256'].lower():
                    errors += 1
                    print(f'  MISMATCH {addr:#x}: dead function bytes differ (sha256)')
            elif 'original' in p:
                orig = assemble(ks, subst(p['original'], vars_, where), addr, where)
                if code[addr - base: addr - base + len(orig)] != orig:
                    errors += 1
                    print(f'  MISMATCH {addr:#x}: expected "{p["original"]}"')

        records.append((addr, data))
        print(f'{addr:#010x}  {len(data):4d} B  {desc}')

    if errors:
        sys.exit(f'\n{errors} mismatch(es): wrong game region or version? Nothing written.')

    if week is not None:
        set_spotpass_week(records, week)

    region = meta.get('region', 'unknown')
    out = a.out or os.path.join(ROOT, 'build', build_name(meta), 'luma', 'titles', meta.get('title_id', 'unknown'), 'code.ips')
    ips = build_ips(records, base)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, 'wb') as f:
        f.write(ips)
    shown = ', '.join(f'{k}={vars_[k]}' for k in spec.get('vars', {}))
    machines = f'SpotPass week from {week.isoformat()}' if week else 'built-in machines'
    print(f'\nWrote {out} ({region}, {len(ips)} bytes; {shown}; {machines})')


if __name__ == '__main__':
    main()
