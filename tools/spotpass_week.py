#!/usr/bin/env python3
"""
spotpass_week.py - which SpotPass week a boss.sarc holds.

Same rules as the installer app (installer/source/spotpass.c), so a patch
built by hand with build_ips.py --spotpass gets the week the app would pick:

    - boss.sarc is a SARC archive; only its header and its Schedule.xml are
      read, not the whole ~25 MB file;
    - the week starts on the <DateStartText> of the PrizeCollection entry,
      else on the earliest DefaultStage start;
    - the DefaultStage entries (one per machine and day) in the 7 days from
      there are counted.

Usage (on its own, to see what a file holds):
    python3 tools/spotpass_week.py path/to/boss.sarc

Standard library only.
"""
import datetime
import struct
import sys
import xml.etree.ElementTree as ET

EPOCH = datetime.date(2000, 1, 1)   # 3DS DateTime: ms since 2000-01-01
HEADER_MAX = 256 * 1024             # real ones: ~2-4 KB
SCHEDULE_MAX = 1024 * 1024          # real ones: ~220 KB


class SpotPassError(Exception):
    """The file isn't a boss.sarc this tool understands, or has no week."""


def read_schedule(path):
    """The bytes of Schedule.xml inside the boss.sarc at `path`."""
    with open(path, 'rb') as f:
        head = f.read(0x14)
        if len(head) < 0x14 or head[:4] != b'SARC' or head[6:8] != b'\xff\xfe':
            raise SpotPassError(f'{path}: not a SARC archive (is it boss.sarc?)')
        data_offset = struct.unpack_from('<I', head, 0x0C)[0]
        if not 0x28 <= data_offset <= HEADER_MAX:
            raise SpotPassError(f'{path}: damaged SARC header')
        f.seek(0)
        hdr = f.read(data_offset)
        if len(hdr) < data_offset or hdr[0x14:0x18] != b'SFAT':
            raise SpotPassError(f'{path}: damaged SARC header')
        count = struct.unpack_from('<H', hdr, 0x1A)[0]
        names = 0x20 + 16 * count
        if names + 8 > data_offset or hdr[names:names + 4] != b'SFNT':
            raise SpotPassError(f'{path}: damaged SARC header')
        names += 8
        for i in range(count):
            _, attr, start, end = struct.unpack_from('<4I', hdr, 0x20 + 16 * i)
            if not attr & 0x01000000:
                continue
            at = names + (attr & 0xFFFFFF) * 4
            if hdr[at:hdr.find(b'\0', at)] != b'Schedule.xml':
                continue
            if not start <= end or end - start > SCHEDULE_MAX:
                raise SpotPassError(f'{path}: Schedule.xml has an implausible size')
            f.seek(data_offset + start)
            xml = f.read(end - start)
            if len(xml) != end - start:
                raise SpotPassError(f'{path}: file is cut short')
            return xml
    raise SpotPassError(f'{path}: no Schedule.xml inside (is it boss.sarc?)')


def parse_date(text):
    """'YYYYMMDD' -> date, or None."""
    text = (text or '').strip()
    if len(text) != 8 or not text.isdigit():
        return None
    try:
        d = datetime.date(int(text[:4]), int(text[4:6]), int(text[6:]))
    except ValueError:
        return None
    return d if 2000 <= d.year <= 2099 else None


def schedule_week(xml):
    """(first day, days with machines, machines) of a Schedule.xml."""
    try:
        root = ET.fromstring(xml)
    except ET.ParseError as e:
        raise SpotPassError(f'Schedule.xml is not valid XML: {e}')
    items = [(it.findtext('RegexSetName'), parse_date(it.findtext('DateStartText')))
             for it in root.iter('FileItem')]
    week = next((d for s, d in items if s == 'PrizeCollection' and d), None)
    stages = [d for s, d in items if s == 'DefaultStage' and d]
    if week is None and stages:
        week = min(stages)
    if week is None:
        raise SpotPassError('Schedule.xml has no dated machines')
    in_week = [(d - week).days for d in stages if 0 <= (d - week).days < 7]
    if not in_week:
        raise SpotPassError('Schedule.xml has no machines in its week')
    return week, len(set(in_week)), len(in_week)


def read_week(path):
    """(first day, days with machines, machines) of the boss.sarc at `path`."""
    return schedule_week(read_schedule(path))


def date_ms(d):
    """Milliseconds since 2000-01-01 00:00, the game's DateTime."""
    return (d - EPOCH).days * 86_400_000


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    try:
        week, days, machines = read_week(sys.argv[1])
    except (OSError, SpotPassError) as e:
        sys.exit(str(e))
    print(f'SpotPass week from {week.isoformat()}: {machines} machines on {days} of 7 days')


if __name__ == '__main__':
    main()
