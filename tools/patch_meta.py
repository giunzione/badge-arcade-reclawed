#!/usr/bin/env python3
"""Shared naming for patch definitions (patches/*.toml).

One region can have several patches: one for the updated game and one for the
game without its update. They share the title ID (Luma loads the patch from the
base title's folder), so they are built into separate folders:

    code_title = "update"  ->  build/<REGION>/            (e.g. build/EUR/)
    code_title = "base"    ->  build/<REGION>-noupdate/   (e.g. build/EUR-noupdate/)

The same name is used for the release zip: badge-arcade-reclawed-<name>-<tag>.zip.

Usage (from a shell script):
    python3 tools/patch_meta.py patches/eur-v1.3.1.toml     # prints the build name
"""

import sys

try:
    import tomllib
except ModuleNotFoundError:
    sys.exit('Python 3.11 or newer is required (tomllib).')


def build_name(meta):
    """Returns the build folder and zip name for a patch.

    Args:
        meta: The patch's [meta] table, as a dict.

    Returns:
        The region (e.g. "EUR") for a patch of the updated game, or the region
        with "-noupdate" (e.g. "EUR-noupdate") for one of the base game.
    """
    region = meta.get('region', 'unknown')
    return f'{region}-noupdate' if meta.get('code_title') == 'base' else region


def main():
    """Prints the build name of the patch definition given on the command line."""
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    with open(sys.argv[1], 'rb') as f:
        print(build_name(tomllib.load(f).get('meta', {})))


if __name__ == '__main__':
    main()
