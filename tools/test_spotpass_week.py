#!/usr/bin/env python3
"""
test_spotpass_week.py - tests for tools/spotpass_week.py and build_ips.py --spotpass.

Builds small synthetic boss.sarc files (no game data). The week rules are the
same as the installer's (installer/tests/test_spotpass.c), so the cases are too.

Usage:
    python3 tools/test_spotpass_week.py     # exit code 0 when everything passes

build_ips.py needs keystone (requirements.txt); the parser tests don't.
"""
import os
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import spotpass_week as sw  # noqa: E402

SETTINGS_MAGIC = b'RCLWCFG1'
failures = 0


def check(cond, what):
    global failures
    if not cond:
        failures += 1
        print(f'  FAIL {what}')


def make_sarc(members):
    """A little-endian SARC laid out as the game's: header, SFAT, SFNT, data."""
    names, offs = b'', []
    for name, _ in members:
        offs.append(len(names) // 4)
        names += name.encode() + b'\0'
        names += bytes(-len(names) % 4)
    head_len = 0x20 + 16 * len(members) + 8 + len(names)
    data_offset = (head_len + 0x7F) & ~0x7F
    nodes, data = b'', b''
    for (name, blob), off in zip(members, offs):
        data += bytes(-len(data) % 0x80)
        nodes += struct.pack('<4I', 0, 0x01000000 | off, len(data), len(data) + len(blob))
        data += blob
    hdr = (b'SARC' + struct.pack('<HHIII', 0x14, 0xFEFF, data_offset + len(data), data_offset, 0x100)
           + b'SFAT' + struct.pack('<HHI', 0x0C, len(members), 0x65) + nodes
           + b'SFNT' + struct.pack('<HH', 8, 0) + names)
    return hdr + bytes(data_offset - len(hdr)) + data


def item(regex_set, start, expire='20990101'):
    return (f'<FileItem><DateStartText>{start}</DateStartText><DateExpireText>{expire}</DateExpireText>'
            f'<Comment /><RegexSetName>{regex_set}</RegexSetName><Key>k</Key><Value>v</Value></FileItem>')


def schedule(*items):
    return ('<?xml version="1.0" encoding="utf-8"?><FileItems><Items>' + ''.join(items)
            + '</Items></FileItems>').encode()


def full_week():
    days = [f'201807{d:02d}' for d in range(13, 20)]
    return schedule(item('MessageDir', '20180713'), *[item('DefaultStage', d) for d in days for _ in range(3)],
                    item('PrizeCollection', '20180713', '20180720'))


def write(tmp, name, blob):
    path = os.path.join(tmp, name)
    with open(path, 'wb') as f:
        f.write(blob)
    return path


def test_week_rules():
    import datetime
    D = datetime.date
    w = sw.schedule_week(full_week())
    check(w == (D(2018, 7, 13), 7, 21), f'full week: {w}')

    # No PrizeCollection: the earliest machine day, in any order; BonusStage isn't a machine.
    w = sw.schedule_week(schedule(item('DefaultStage', '20190112'), item('DefaultStage', '20190110'),
                                  item('BonusStage', '20190101')))
    check(w == (D(2019, 1, 10), 2, 2), f'no PrizeCollection: {w}')

    # Machines outside the 7 days from the start don't count.
    w = sw.schedule_week(schedule(item('PrizeCollection', '20180713'), item('DefaultStage', '20180712'),
                                  item('DefaultStage', '20180720'), item('DefaultStage', '20180719')))
    check(w == (D(2018, 7, 13), 1, 1), f'outside the week: {w}')

    for bad, why in ((schedule(item('PrizeCollection', '20180713')), 'no machines in the week'),
                     (schedule(item('DefaultStage', '2018-07-13')), 'bad date'),
                     (schedule(item('DefaultStage', '20230229')), 'not a real day'),
                     (b'<FileItems><Items/></FileItems>', 'empty'),
                     (b'not xml', 'not XML')):
        try:
            sw.schedule_week(bad)
            check(False, f'{why}: accepted')
        except sw.SpotPassError:
            pass

    check(sw.date_ms(D(2000, 1, 1)) == 0, 'epoch')
    check(sw.date_ms(D(2018, 7, 13)) == 584755200000, '2018-07-13 in ms')
    check(sw.date_ms(D(2019, 1, 10)) == 600393600000, '2019-01-10 in ms')


def test_sarc(tmp):
    import datetime
    path = write(tmp, 'boss.sarc', make_sarc([('post/a.sarc', b'AAAA'), ('Schedule.xml', full_week()),
                                              ('sharc/x.sarc', b'BB')]))
    check(sw.read_week(path) == (datetime.date(2018, 7, 13), 7, 21), 'read_week on a synthetic boss.sarc')

    for name, blob, why in (('nosched.sarc', make_sarc([('post/a.sarc', b'AAAA')]), 'no Schedule.xml'),
                            ('prefix.sarc', make_sarc([('Schedule.xml2', full_week())]), 'name prefix only'),
                            ('junk.sarc', b'SARC' + bytes(60), 'damaged header'),
                            ('short.sarc', b'SAR', 'too short'),
                            ('be.sarc', make_sarc([('Schedule.xml', full_week())]).replace(b'\xff\xfe', b'\xfe\xff', 1),
                             'big-endian')):
        try:
            sw.read_week(write(tmp, name, blob))
            check(False, f'{why}: accepted')
        except sw.SpotPassError:
            pass
    # Cut short after the header: the schedule can't be read.
    whole = make_sarc([('Schedule.xml', full_week())])
    try:
        sw.read_week(write(tmp, 'cut.sarc', whole[:len(whole) - 10]))
        check(False, 'cut short: accepted')
    except sw.SpotPassError:
        pass


def test_build_ips(tmp):
    try:
        import keystone  # noqa: F401
    except ImportError:
        print('  (skipped build_ips.py --spotpass: keystone not installed)')
        return
    boss = write(tmp, 'boss.sarc', make_sarc([('Schedule.xml', full_week())]))
    for toml in sorted(os.listdir(os.path.join(HERE, '..', 'patches'))):
        if not toml.endswith('.toml'):
            continue
        for spotpass, want in ((None, (0, 1)), (boss, (584755200000, 0))):
            out = os.path.join(tmp, toml + '.ips')
            args = [sys.executable, os.path.join(HERE, 'build_ips.py'), '--patch',
                    os.path.join(HERE, '..', 'patches', toml), '--out', out]
            if spotpass:
                args += ['--spotpass', spotpass]
            r = subprocess.run(args, capture_output=True, text=True)
            check(r.returncode == 0, f'{toml} {"--spotpass " if spotpass else ""}build: {r.stderr.strip()}')
            if r.returncode:
                continue
            ips = open(out, 'rb').read()
            at = ips.find(SETTINGS_MAGIC) + 8
            check(ips.count(SETTINGS_MAGIC) == 1 and struct.unpack_from('<QI', ips, at) == want,
                  f'{toml}: settings block {struct.unpack_from("<QI", ips, at)} != {want}')
    # A file that isn't a boss.sarc stops the build, nothing written.
    out = os.path.join(tmp, 'never.ips')
    r = subprocess.run([sys.executable, os.path.join(HERE, 'build_ips.py'), '--spotpass',
                        write(tmp, 'x.sarc', b'nope'), '--out', out], capture_output=True, text=True)
    check(r.returncode != 0 and not os.path.exists(out), 'bad --spotpass file refused')


def main():
    with tempfile.TemporaryDirectory() as tmp:
        test_week_rules()
        test_sarc(tmp)
        test_build_ips(tmp)
    print('spotpass_week: ok' if not failures else f'spotpass_week: {failures} failed')
    sys.exit(1 if failures else 0)


if __name__ == '__main__':
    main()
