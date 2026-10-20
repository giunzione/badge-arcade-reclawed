#!/usr/bin/env python3
"""Tests for tools/spotpass_week.py and build_ips.py --spotpass.

Builds small synthetic boss.sarc files (no game data). The week rules are the
same as the installer's (installer/tests/test_spotpass.c), so the cases are too.

Usage:
    python3 tools/test_spotpass_week.py     # exit code 0 when everything passes

The build_ips.py tests need keystone (requirements.txt) and are skipped
without it; the parser tests don't.
"""

import datetime
import os
import struct
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import spotpass_week as sw  # noqa: E402 - after the path change above

SETTINGS_MAGIC = b'RCLWCFG1'
PATCHES = os.path.join(HERE, '..', 'patches')


def make_sarc(members):
    """Builds a little-endian SARC laid out as the game's.

    Args:
        members: (name, bytes) pairs.

    Returns:
        The archive: header, SFAT nodes, SFNT names (4-aligned), and the data
        from a 0x80 boundary, each member 0x80-aligned.
    """
    names, offs = b'', []
    for name, _ in members:
        offs.append(len(names) // 4)
        names += name.encode() + b'\0'
        names += bytes(-len(names) % 4)
    head_len = 0x20 + 16 * len(members) + 8 + len(names)
    data_offset = (head_len + 0x7F) & ~0x7F
    nodes, data = b'', b''
    for (_, blob), off in zip(members, offs, strict=True):
        data += bytes(-len(data) % 0x80)
        nodes += struct.pack('<4I', 0, 0x01000000 | off, len(data), len(data) + len(blob))
        data += blob
    hdr = (b'SARC'
           + struct.pack('<HHIII', 0x14, 0xFEFF, data_offset + len(data), data_offset, 0x100)
           + b'SFAT' + struct.pack('<HHI', 0x0C, len(members), 0x65) + nodes
           + b'SFNT' + struct.pack('<HH', 8, 0) + names)
    return hdr + bytes(data_offset - len(hdr)) + data


def item(regex_set, start, expire='20990101'):
    """Returns one Schedule.xml FileItem of set `regex_set`, from `start`."""
    return (f'<FileItem><DateStartText>{start}</DateStartText>'
            f'<DateExpireText>{expire}</DateExpireText><Comment />'
            f'<RegexSetName>{regex_set}</RegexSetName><Key>k</Key><Value>v</Value></FileItem>')


def schedule(*items):
    """Returns a Schedule.xml (bytes) holding `items`."""
    return ('<?xml version="1.0" encoding="utf-8"?><FileItems><Items>' + ''.join(items)
            + '</Items></FileItems>').encode()


def full_week():
    """Returns a Schedule.xml for 2018-07-13: 3 machines on each of 7 days."""
    days = [f'201807{d:02d}' for d in range(13, 20)]
    return schedule(item('MessageDir', '20180713'),
                    *[item('DefaultStage', d) for d in days for _ in range(3)],
                    item('PrizeCollection', '20180713', '20180720'))


class TempDirTest(unittest.TestCase):
    """A test case with a temporary folder for the files it writes."""

    def setUp(self):
        """Creates the folder."""
        self._tmp = tempfile.TemporaryDirectory()
        self.tmp = self._tmp.name

    def tearDown(self):
        """Removes the folder."""
        self._tmp.cleanup()

    def write(self, name, blob):
        """Writes `blob` to `name` in the folder and returns its path."""
        path = os.path.join(self.tmp, name)
        with open(path, 'wb') as f:
            f.write(blob)
        return path


class WeekRulesTest(unittest.TestCase):
    """schedule_week(), parse_date() and date_ms()."""

    def test_full_week(self):
        """A full week starts on the PrizeCollection entry."""
        self.assertEqual(sw.schedule_week(full_week()), (datetime.date(2018, 7, 13), 7, 21))

    def test_earliest_machine_without_prize_collection(self):
        """Without one, the earliest machine day counts; BonusStage isn't a machine."""
        xml = schedule(item('DefaultStage', '20190112'), item('DefaultStage', '20190110'),
                       item('BonusStage', '20190101'))
        self.assertEqual(sw.schedule_week(xml), (datetime.date(2019, 1, 10), 2, 2))

    def test_machines_outside_the_week(self):
        """Machines outside the 7 days from the start don't count."""
        xml = schedule(item('PrizeCollection', '20180713'), item('DefaultStage', '20180712'),
                       item('DefaultStage', '20180720'), item('DefaultStage', '20180719'))
        self.assertEqual(sw.schedule_week(xml), (datetime.date(2018, 7, 13), 1, 1))

    def test_unusable_schedules(self):
        """Schedules without a usable week are refused."""
        cases = {
            'no machines in the week': schedule(item('PrizeCollection', '20180713')),
            'bad date': schedule(item('DefaultStage', '2018-07-13')),
            'not a real day': schedule(item('DefaultStage', '20230229')),
            'empty': b'<FileItems><Items/></FileItems>',
            'not XML': b'not xml',
        }
        for why, xml in cases.items():
            with self.subTest(why), self.assertRaises(sw.SpotPassError):
                sw.schedule_week(xml)

    def test_date_ms(self):
        """Dates convert to ms since 2000-01-01."""
        self.assertEqual(sw.date_ms(datetime.date(2000, 1, 1)), 0)
        self.assertEqual(sw.date_ms(datetime.date(2018, 7, 13)), 584755200000)
        self.assertEqual(sw.date_ms(datetime.date(2019, 1, 10)), 600393600000)


class SarcTest(TempDirTest):
    """read_week() on archive files."""

    def test_synthetic_boss_sarc(self):
        """Schedule.xml is found among other members."""
        path = self.write('boss.sarc', make_sarc([('post/a.sarc', b'AAAA'),
                                                  ('Schedule.xml', full_week()),
                                                  ('sharc/x.sarc', b'BB')]))
        self.assertEqual(sw.read_week(path), (datetime.date(2018, 7, 13), 7, 21))

    def test_unusable_archives(self):
        """Archives without a readable Schedule.xml are refused."""
        whole = make_sarc([('Schedule.xml', full_week())])
        cases = {
            'no Schedule.xml': make_sarc([('post/a.sarc', b'AAAA')]),
            'name prefix only': make_sarc([('Schedule.xml2', full_week())]),
            'damaged header': b'SARC' + bytes(60),
            'too short': b'SAR',
            'big-endian': whole.replace(b'\xff\xfe', b'\xfe\xff', 1),
            'cut short': whole[:len(whole) - 10],
        }
        for why, blob in cases.items():
            with self.subTest(why), self.assertRaises(sw.SpotPassError):
                sw.read_week(self.write('x.sarc', blob))


class BuildIpsTest(TempDirTest):
    """build_ips.py with and without --spotpass, on every patch definition."""

    def setUp(self):
        """Skips without keystone, which build_ips.py needs."""
        try:
            import keystone  # noqa: F401
        except ImportError:
            self.skipTest('keystone not installed')
        super().setUp()

    def build(self, *args):
        """Runs build_ips.py with `args`; returns the finished process."""
        return subprocess.run([sys.executable, os.path.join(HERE, 'build_ips.py'), *args],
                              capture_output=True, text=True, check=False)

    def test_settings_block(self):
        """Built-in by default; --spotpass writes the week and the SpotPass mode."""
        boss = self.write('boss.sarc', make_sarc([('Schedule.xml', full_week())]))
        for toml in sorted(t for t in os.listdir(PATCHES) if t.endswith('.toml')):
            for extra, want in (((), (0, 1)), (('--spotpass', boss), (584755200000, 0))):
                with self.subTest(toml=toml, spotpass=bool(extra)):
                    out = os.path.join(self.tmp, toml + '.ips')
                    r = self.build('--patch', os.path.join(PATCHES, toml), '--out', out, *extra)
                    self.assertEqual(r.returncode, 0, r.stderr)
                    with open(out, 'rb') as f:
                        ips = f.read()
                    self.assertEqual(ips.count(SETTINGS_MAGIC), 1)
                    at = ips.find(SETTINGS_MAGIC) + len(SETTINGS_MAGIC)
                    self.assertEqual(struct.unpack_from('<QI', ips, at), want)

    def test_bad_spotpass_file(self):
        """A file that isn't a boss.sarc stops the build; nothing is written."""
        out = os.path.join(self.tmp, 'never.ips')
        r = self.build('--spotpass', self.write('x.sarc', b'nope'), '--out', out)
        self.assertNotEqual(r.returncode, 0)
        self.assertFalse(os.path.exists(out))


if __name__ == '__main__':
    unittest.main()
