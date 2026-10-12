#!/usr/bin/env python3
"""
restore_badges.py - rebuild the HOME Menu badge registry (BadgeMngFile.dat)
from the badges still physically present in BadgeData.dat.

Use case: the registry was wiped (all slots 0xFF) but names/images are still
in BadgeData.dat, so the HOME badge box shows nothing.

Format (from AntiMach's Advanced Badge Editor):
  BadgeMngFile.dat (0xD4A8)
    0x04 u32 set count, 0x08 u32 unique badges, 0x18 u32 total badges, 0x1C u32 NNID
    0x3E8 + i*0x28 : 1000 badge entries
    0xA028 + i*0x30: 100 set entries
    0xB2E8 + i*0x18: 360 placed-badge entries (left empty)
  BadgeData.dat (0xF4DF80)
    set names   : slot*0x8A0 (16 langs x 0x8A, UTF-16LE)
    badge names : 0x35E80 + slot*0x8A0
    set images  : 0x250F80 + slot*0x2000
    badge 64px  : 0x318F80 + slot*0x2800, badge 32px: 0xCDCF80 + slot*0xA00

Badges are matched to sets by name ("<set name> badge"). Badges whose set
can't be determined from the name (all Pokemon regions share "Pokemon badge")
are grouped into a single set.

Usage: restore_badges.py <input_dir> <output_dir> [--dry-run]
"""
import os, struct, sys

MNG_SIZE, DATA_SIZE = 0xD4A8, 0xF4DF80
LANG_EN = 1  # EUR slot order: JP, EN, FR, DE, IT, ES, ...


def name_at(data, off, lang=LANG_EN):
    raw = data[off + lang * 0x8A: off + (lang + 1) * 0x8A]
    return raw.decode('utf-16le', 'replace').split('\0')[0].replace('\r\n', ' ').strip()


def set_name_bytes(text):
    b = text.encode('utf-16le')[:0x88]
    return b + b'\0' * (0x8A - len(b))


def empty_mng(nnid):
    m = bytearray()
    m += b'\0' * 0x10 + b'\xff' * 4 + b'\0' * 0x3D4
    for _ in range(1000):
        m += b'\0' * 4 + b'\xff' * 0xA + b'\0' * 0xA + b'\xff' * 8 + b'\0' * 8
    for _ in range(100):
        m += b'\xff' * 8 + b'\0' * 4 + struct.pack('<H', 0x2710) + b'\0' * 2 + b'\xff' * 0xC + b'\0' * 8 + b'\xff' * 4 + b'\0' * 8
    for _ in range(360):
        m += b'\0' * 4 + b'\xff' * 0xA + b'\0' * 2 + b'\xff' * 8
    assert len(m) == MNG_SIZE
    struct.pack_into('<I', m, 0x1C, nnid)
    return m


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    dry = '--dry-run' in sys.argv
    if len(args) != 2:
        sys.exit(__doc__)
    src, dst = args
    data = bytearray(open(os.path.join(src, 'BadgeData.dat'), 'rb').read())
    mng_in = open(os.path.join(src, 'BadgeMngFile.dat'), 'rb').read()
    assert len(data) == DATA_SIZE and len(mng_in) == MNG_SIZE, 'unexpected file sizes'

    old_sets, old_unique = struct.unpack_from('<II', mng_in, 4)
    if old_sets or old_unique:
        sys.exit(f'Registry is not empty ({old_sets} sets, {old_unique} badges): refusing to overwrite.')
    nnid = struct.unpack_from('<I', mng_in, 0x1C)[0]

    # badges physically present (contiguous slots with a name and an image)
    badges = []
    for slot in range(1000):
        n = name_at(data, 0x35E80 + slot * 0x8A0)
        img = data[0x318F80 + slot * 0x2800: 0x318F80 + (slot + 1) * 0x2800]
        if not n:
            break
        if not any(img):
            print(f'warning: badge slot {slot} has a name but no image, skipped')
            continue
        badges.append((slot, n))

    set_slots = []
    for slot in range(100):
        n = name_at(data, slot * 0x8A0)
        if not n:
            break
        set_slots.append((slot, n))

    # assign badges to sets
    by_name = {n: s for s, n in set_slots}
    groups = {}          # set slot -> [badge slots]
    fallback = {}        # badge-name prefix -> [badge slots] (ambiguous)
    for slot, n in badges:
        prefix = n[:-len(' badge')] if n.endswith(' badge') else n
        if prefix in by_name:
            groups.setdefault(by_name[prefix], []).append(slot)
        else:
            cands = [s for s, sn in set_slots if sn.startswith(prefix)]
            if len(cands) == 1:
                groups.setdefault(cands[0], []).append(slot)
            else:
                fallback.setdefault(prefix, []).append((slot, cands))

    # ambiguous groups (e.g. Pokemon): use the first candidate set slot and rename it
    renamed = {}
    for prefix, items in fallback.items():
        cands = items[0][1]
        if not cands:
            sys.exit(f'no set found for badges named "{prefix} badge"')
        target = cands[0]
        groups.setdefault(target, []).extend(s for s, _ in items)
        renamed[target] = prefix

    order = [s for s, _ in set_slots if s in groups]
    unique = sum(len(groups[s]) for s in order)

    print(f'NNID field kept: {nnid:#010x}')
    for s in order:
        label = renamed.get(s, dict(set_slots)[s])
        print(f'  set slot {s:2d}: {len(groups[s]):3d} badges  {label}')
    unused = [n for s, n in set_slots if s not in groups]
    if unused:
        print('  set slots left unused:', unused)
    print(f'total: {len(order)} sets, {unique} badges')
    if dry:
        return

    mng = empty_mng(nnid)
    struct.pack_into('<II', mng, 0x04, len(order), unique)
    struct.pack_into('<I', mng, 0x18, unique)
    i = 0
    for set_id, s in enumerate(order, start=1):
        first = i
        for slot in groups[s]:
            e = 0x3EC + i * 0x28
            struct.pack_into('<IIHH', mng, e, i + 1, set_id, slot, 0)    # id, set id, data slot, sid
            struct.pack_into('<H', mng, e + 0x0E, 1)                        # quantity
            struct.pack_into('<IIII', mng, e + 0x14, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF)  # no shortcut
            i += 1
        se = 0xA038 + (set_id - 1) * 0x30
        struct.pack_into('<II', mng, se, set_id, s)
        struct.pack_into('<III', mng, se + 0x0C, len(groups[s]), len(groups[s]), first)

    for s, label in renamed.items():
        for lang in range(16):
            o = s * 0x8A0 + lang * 0x8A
            data[o:o + 0x8A] = set_name_bytes(label)

    os.makedirs(dst, exist_ok=True)
    open(os.path.join(dst, 'BadgeMngFile.dat'), 'wb').write(mng)
    open(os.path.join(dst, 'BadgeData.dat'), 'wb').write(data)
    print('written to', dst)


if __name__ == '__main__':
    main()
