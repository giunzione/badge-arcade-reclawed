#!/usr/bin/env python3
"""Draws the QR code that FBI scans to install the installer app.

The URL points at the newest *full* release (GitHub's "latest" skips
pre-releases), and the release workflow always uploads the CIA under the same
name, so the image only needs regenerating if the repository moves.

Usage:
    python3 tools/make_qr.py                      # docs/install-qr.png for this repo
    python3 tools/make_qr.py --url URL --out file.png

Requires segno (pip install segno). Not needed to build the patch or the app.
"""

import argparse
import os
import sys

try:
    import segno
except ImportError:
    sys.exit('segno is missing: pip install segno')

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_URL = ('https://github.com/giunzione/badge-arcade-reclawed/'
               'releases/latest/download/reclawed-installer.cia')


def main():
    """Parses the command line and writes the PNG."""
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--url', default=DEFAULT_URL, help='what the QR code points to')
    ap.add_argument('--out', default=os.path.join(ROOT, 'docs', 'install-qr.png'),
                    help='output PNG')
    a = ap.parse_args()

    # Error correction M and a quiet zone of 4 modules: easy for the 3DS camera.
    qr = segno.make(a.url, error='m', micro=False)
    qr.save(a.out, scale=8, border=4, dark='#000000', light='#ffffff')
    print(f'Wrote {a.out} ({qr.version}, {a.url})')


if __name__ == '__main__':
    main()
