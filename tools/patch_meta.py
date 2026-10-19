#!/usr/bin/env python3
"""
patch_meta.py - shared naming for patch definitions (patches/*.toml)

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
    """Folder / zip name for a patch's [meta] table."""
    region = meta.get('region', 'unknown')
    return f'{region}-noupdate' if meta.get('code_title') == 'base' else region


if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    with open(sys.argv[1], 'rb') as f:
        print(build_name(tomllib.load(f).get('meta', {})))
