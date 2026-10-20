#!/usr/bin/env python3
"""Tells which SpotPass week a boss.sarc holds.

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
import xml.etree.ElementTree as ElementTree

EPOCH = datetime.date(2000, 1, 1)  # 3DS DateTime: ms since 2000-01-01
HEADER_MAX = 256 * 1024  # real ones: ~2-4 KB
SCHEDULE_MAX = 1024 * 1024  # real ones: ~220 KB
SCHEDULE_NAME = b'Schedule.xml'
WEEK_DAYS = 7

# SARC layout (little-endian, as Badge Arcade writes it):
#   0x00 "SARC", 0x04 header size, 0x06 BOM FF FE, 0x08 file size, 0x0C data offset
#   0x14 "SFAT", 0x18 header size 0x0C, 0x1A node count, 0x1C hash key
#   0x20 nodes of 16 bytes: name hash, attributes (0x01000000 | name offset / 4),
#        data start, data end (both relative to the data offset)
#   then "SFNT", 0x08, 0, and the zero-terminated names, 4-byte aligned.
SARC_HEADER_SIZE = 0x14
SFAT_NODES = 0x20
SFAT_NODE_SIZE = 16
SFNT_HEADER_SIZE = 8
NODE_HAS_NAME = 0x01000000


class SpotPassError(Exception):
    """The file isn't a boss.sarc this tool understands, or it has no week."""


def read_header(f, path):
    """Reads the header part of a SARC: everything before the file data.

    Args:
        f: The archive, open for binary reading.
        path: Its path, for error messages.

    Returns:
        A tuple (header bytes, data offset).

    Raises:
        SpotPassError: It isn't a little-endian SARC, or its header is damaged.
    """
    head = f.read(SARC_HEADER_SIZE)
    if len(head) < SARC_HEADER_SIZE or head[:4] != b'SARC' or head[6:8] != b'\xff\xfe':
        raise SpotPassError(f'{path}: not a SARC archive (is it boss.sarc?)')
    data_offset = struct.unpack_from('<I', head, 0x0C)[0]
    if not 0x28 <= data_offset <= HEADER_MAX:
        raise SpotPassError(f'{path}: damaged SARC header')
    f.seek(0)
    hdr = f.read(data_offset)
    if len(hdr) < data_offset or hdr[0x14:0x18] != b'SFAT':
        raise SpotPassError(f'{path}: damaged SARC header')
    return hdr, data_offset


def find_member(hdr, data_offset, name, path):
    """Finds a member of a SARC by name.

    Args:
        hdr: The header part, as read_header() returns it.
        data_offset: Where the file data starts.
        name: The member's name (bytes).
        path: The archive's path, for error messages.

    Returns:
        A tuple (start, end): the member's bytes in the archive, or None if
        there is no such member.

    Raises:
        SpotPassError: The header is damaged.
    """
    count = struct.unpack_from('<H', hdr, 0x1A)[0]
    names = SFAT_NODES + SFAT_NODE_SIZE * count
    if names + SFNT_HEADER_SIZE > data_offset or hdr[names:names + 4] != b'SFNT':
        raise SpotPassError(f'{path}: damaged SARC header')
    names += SFNT_HEADER_SIZE
    for i in range(count):
        _, attr, start, end = struct.unpack_from('<4I', hdr, SFAT_NODES + SFAT_NODE_SIZE * i)
        if not attr & NODE_HAS_NAME:
            continue
        at = names + (attr & 0xFFFFFF) * 4
        if hdr[at:hdr.find(b'\0', at)] == name:
            return data_offset + start, data_offset + end
    return None


def read_schedule(path):
    """Reads Schedule.xml out of a boss.sarc, without reading the rest.

    Args:
        path: The boss.sarc.

    Returns:
        The bytes of its Schedule.xml.

    Raises:
        OSError: The file can't be opened.
        SpotPassError: It isn't a boss.sarc this tool understands.
    """
    with open(path, 'rb') as f:
        hdr, data_offset = read_header(f, path)
        where = find_member(hdr, data_offset, SCHEDULE_NAME, path)
        if where is None:
            raise SpotPassError(f'{path}: no Schedule.xml inside (is it boss.sarc?)')
        start, end = where
        if not start <= end or end - start > SCHEDULE_MAX:
            raise SpotPassError(f'{path}: Schedule.xml has an implausible size')
        f.seek(start)
        xml = f.read(end - start)
        if len(xml) != end - start:
            raise SpotPassError(f'{path}: file is cut short')
        return xml


def parse_date(text):
    """Parses a Schedule.xml date.

    Args:
        text: A date as "YYYYMMDD" (surrounding blanks allowed), or None.

    Returns:
        The date (datetime.date), or None if `text` isn't a real day of
        2000-2099.
    """
    text = (text or '').strip()
    if len(text) != 8 or not text.isdigit():
        return None
    try:
        d = datetime.date(int(text[:4]), int(text[4:6]), int(text[6:]))
    except ValueError:
        return None
    return d if 2000 <= d.year <= 2099 else None


def schedule_week(xml):
    """Finds the SpotPass week in a Schedule.xml.

    Args:
        xml: The Schedule.xml bytes.

    Returns:
        A tuple (first day, days with machines, machines): the week's first
        day (datetime.date), how many of its 7 days have machines, and how many
        DefaultStage entries fall in those 7 days.

    Raises:
        SpotPassError: It isn't valid XML, or has no dated machines in its week.
    """
    try:
        root = ElementTree.fromstring(xml)
    except ElementTree.ParseError as e:
        raise SpotPassError(f'Schedule.xml is not valid XML: {e}') from e
    items = [(it.findtext('RegexSetName'), parse_date(it.findtext('DateStartText')))
             for it in root.iter('FileItem')]
    week = next((d for s, d in items if s == 'PrizeCollection' and d), None)
    stages = [d for s, d in items if s == 'DefaultStage' and d]
    if week is None and stages:
        week = min(stages)
    if week is None:
        raise SpotPassError('Schedule.xml has no dated machines')
    in_week = [(d - week).days for d in stages if 0 <= (d - week).days < WEEK_DAYS]
    if not in_week:
        raise SpotPassError('Schedule.xml has no machines in its week')
    return week, len(set(in_week)), len(in_week)


def read_week(path):
    """Finds the SpotPass week in a boss.sarc.

    Args:
        path: The boss.sarc.

    Returns:
        A tuple (first day, days with machines, machines), as schedule_week().

    Raises:
        OSError: The file can't be opened.
        SpotPassError: It isn't a usable boss.sarc.
    """
    return schedule_week(read_schedule(path))


def date_ms(d):
    """Converts a date to the game's DateTime.

    Args:
        d: The date (datetime.date).

    Returns:
        Milliseconds from 2000-01-01 00:00 to `d` 00:00.
    """
    return (d - EPOCH).days * 86_400_000


def main():
    """Prints the week of the boss.sarc given on the command line."""
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    try:
        week, days, machines = read_week(sys.argv[1])
    except (OSError, SpotPassError) as e:
        sys.exit(str(e))
    print(f'SpotPass week from {week.isoformat()}: {machines} machines on {days} of 7 days')


if __name__ == '__main__':
    main()
