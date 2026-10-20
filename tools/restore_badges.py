#!/usr/bin/env python3
"""Rebuilds the HOME Menu badge registry (BadgeMngFile.dat).

The registry is rebuilt from the badges still physically present in
BadgeData.dat. Use case: the registry was wiped (all slots 0xFF) but the names
and images are still in BadgeData.dat, so the HOME badge box shows nothing.

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

import os
import struct
import sys

MNG_SIZE, DATA_SIZE = 0xD4A8, 0xF4DF80
LANG_EN = 1  # EUR slot order: JP, EN, FR, DE, IT, ES, ...
LANGS = 16
NAME_SIZE = 0x8A  # one language's name, UTF-16LE
NAME_STRIDE = 0x8A0  # all languages of one slot
BADGE_NAMES = 0x35E80
BADGE_IMAGES = 0x318F80  # 64 px images
BADGE_IMAGE_SIZE = 0x2800
MAX_BADGES, MAX_SETS, MAX_PLACED = 1000, 100, 360
MNG_BADGES = 0x3EC  # badge entries (0x28 each), from their id field
MNG_BADGE_SIZE = 0x28
MNG_SETS = 0xA038  # set entries (0x30 each), from their id field
MNG_SET_SIZE = 0x30
NO_SHORTCUT = 0xFFFFFFFF
BADGE_SUFFIX = ' badge'


def name_at(data, off, lang=LANG_EN):
    """Reads one language of a name in BadgeData.dat.

    Args:
        data: The BadgeData.dat bytes.
        off: Where the slot's names start.
        lang: The language index.

    Returns:
        The name, up to its terminator, with line breaks as spaces.
    """
    raw = data[off + lang * NAME_SIZE: off + (lang + 1) * NAME_SIZE]
    return raw.decode('utf-16le', 'replace').split('\0')[0].replace('\r\n', ' ').strip()


def set_name_bytes(text):
    """Encodes a set name for BadgeData.dat.

    Args:
        text: The name.

    Returns:
        NAME_SIZE bytes: the UTF-16LE name, cut to fit with its terminator.
    """
    b = text.encode('utf-16le')[:NAME_SIZE - 2]
    return b + b'\0' * (NAME_SIZE - len(b))


def empty_mng(nnid):
    """Builds an empty registry, as the HOME Menu writes it.

    Args:
        nnid: The NNID field to keep from the old registry.

    Returns:
        The BadgeMngFile.dat bytes (bytearray), with no sets and no badges.
    """
    m = bytearray()
    m += b'\0' * 0x10 + b'\xff' * 4 + b'\0' * 0x3D4
    for _ in range(MAX_BADGES):
        m += b'\0' * 4 + b'\xff' * 0xA + b'\0' * 0xA + b'\xff' * 8 + b'\0' * 8
    for _ in range(MAX_SETS):
        m += (b'\xff' * 8 + b'\0' * 4 + struct.pack('<H', 0x2710) + b'\0' * 2 + b'\xff' * 0xC
              + b'\0' * 8 + b'\xff' * 4 + b'\0' * 8)
    for _ in range(MAX_PLACED):
        m += b'\0' * 4 + b'\xff' * 0xA + b'\0' * 2 + b'\xff' * 8
    assert len(m) == MNG_SIZE
    struct.pack_into('<I', m, 0x1C, nnid)
    return m


def read_inputs(src):
    """Reads and checks the two files of a badge dump.

    Args:
        src: The folder holding BadgeData.dat and BadgeMngFile.dat.

    Returns:
        A tuple (BadgeData.dat as a bytearray, the NNID field of the registry).

    Exits the program with a message if the registry isn't empty.
    """
    with open(os.path.join(src, 'BadgeData.dat'), 'rb') as f:
        data = bytearray(f.read())
    with open(os.path.join(src, 'BadgeMngFile.dat'), 'rb') as f:
        mng_in = f.read()
    assert len(data) == DATA_SIZE and len(mng_in) == MNG_SIZE, 'unexpected file sizes'

    old_sets, old_unique = struct.unpack_from('<II', mng_in, 4)
    if old_sets or old_unique:
        sys.exit(f'Registry is not empty ({old_sets} sets, {old_unique} badges): '
                 'refusing to overwrite.')
    return data, struct.unpack_from('<I', mng_in, 0x1C)[0]


def present_badges(data):
    """Lists the badges physically present: contiguous slots with a name and an image.

    Args:
        data: The BadgeData.dat bytes.

    Returns:
        (slot, name) pairs. Slots with a name but no image are skipped, with a
        warning.
    """
    badges = []
    for slot in range(MAX_BADGES):
        n = name_at(data, BADGE_NAMES + slot * NAME_STRIDE)
        img = data[BADGE_IMAGES + slot * BADGE_IMAGE_SIZE:
                   BADGE_IMAGES + (slot + 1) * BADGE_IMAGE_SIZE]
        if not n:
            break
        if not any(img):
            print(f'warning: badge slot {slot} has a name but no image, skipped')
            continue
        badges.append((slot, n))
    return badges


def present_sets(data):
    """Lists the badge sets with a name: (slot, name) pairs, contiguous from slot 0."""
    sets = []
    for slot in range(MAX_SETS):
        n = name_at(data, slot * NAME_STRIDE)
        if not n:
            break
        sets.append((slot, n))
    return sets


def group_badges(badges, set_slots):
    """Assigns each badge to a set by its name ("<set name> badge").

    Args:
        badges: (slot, name) pairs, from present_badges().
        set_slots: (slot, name) pairs, from present_sets().

    Returns:
        A tuple (groups, renamed): set slot -> badge slots, and the sets that
        take the badges of an ambiguous name (e.g. "Pokemon badge" for all
        regions), with the name they get: set slot -> new name.

    Exits the program with a message if a badge matches no set at all.
    """
    by_name = {n: s for s, n in set_slots}
    groups = {}  # set slot -> [badge slots]
    fallback = {}  # badge-name prefix -> [(badge slot, candidate set slots)]
    for slot, n in badges:
        prefix = n[:-len(BADGE_SUFFIX)] if n.endswith(BADGE_SUFFIX) else n
        if prefix in by_name:
            groups.setdefault(by_name[prefix], []).append(slot)
            continue
        cands = [s for s, sn in set_slots if sn.startswith(prefix)]
        if len(cands) == 1:
            groups.setdefault(cands[0], []).append(slot)
        else:
            fallback.setdefault(prefix, []).append((slot, cands))

    # Ambiguous groups (e.g. Pokemon): use the first candidate set slot and rename it.
    renamed = {}
    for prefix, items in fallback.items():
        cands = items[0][1]
        if not cands:
            sys.exit(f'no set found for badges named "{prefix} badge"')
        target = cands[0]
        groups.setdefault(target, []).extend(s for s, _ in items)
        renamed[target] = prefix
    return groups, renamed


def build_registry(nnid, order, groups):
    """Builds the registry for the grouped badges.

    Args:
        nnid: The NNID field to keep.
        order: The set slots to register, in order.
        groups: Set slot -> badge slots.

    Returns:
        The BadgeMngFile.dat bytes (bytearray).
    """
    unique = sum(len(groups[s]) for s in order)
    mng = empty_mng(nnid)
    struct.pack_into('<II', mng, 0x04, len(order), unique)
    struct.pack_into('<I', mng, 0x18, unique)
    i = 0
    for set_id, s in enumerate(order, start=1):
        first = i
        for slot in groups[s]:
            e = MNG_BADGES + i * MNG_BADGE_SIZE
            struct.pack_into('<IIHH', mng, e, i + 1, set_id, slot, 0)  # id, set id, data slot, sid
            struct.pack_into('<H', mng, e + 0x0E, 1)  # quantity
            struct.pack_into('<IIII', mng, e + 0x14, *[NO_SHORTCUT] * 4)  # not a HOME shortcut
            i += 1
        se = MNG_SETS + (set_id - 1) * MNG_SET_SIZE
        struct.pack_into('<II', mng, se, set_id, s)
        struct.pack_into('<III', mng, se + 0x0C, len(groups[s]), len(groups[s]), first)
    return mng


def main():
    """Parses the command line, prints the plan and (unless --dry-run) writes the files."""
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    dry = '--dry-run' in sys.argv
    if len(args) != 2:
        sys.exit(__doc__)
    src, dst = args
    data, nnid = read_inputs(src)
    badges = present_badges(data)
    set_slots = present_sets(data)
    groups, renamed = group_badges(badges, set_slots)

    order = [s for s, _ in set_slots if s in groups]
    unique = sum(len(groups[s]) for s in order)
    print(f'NNID field kept: {nnid:#010x}')
    names = dict(set_slots)
    for s in order:
        print(f'  set slot {s:2d}: {len(groups[s]):3d} badges  {renamed.get(s, names[s])}')
    unused = [n for s, n in set_slots if s not in groups]
    if unused:
        print('  set slots left unused:', unused)
    print(f'total: {len(order)} sets, {unique} badges')
    if dry:
        return

    mng = build_registry(nnid, order, groups)
    for s, label in renamed.items():
        for lang in range(LANGS):
            o = s * NAME_STRIDE + lang * NAME_SIZE
            data[o:o + NAME_SIZE] = set_name_bytes(label)

    os.makedirs(dst, exist_ok=True)
    with open(os.path.join(dst, 'BadgeMngFile.dat'), 'wb') as f:
        f.write(mng)
    with open(os.path.join(dst, 'BadgeData.dat'), 'wb') as f:
        f.write(data)
    print('written to', dst)


if __name__ == '__main__':
    main()
