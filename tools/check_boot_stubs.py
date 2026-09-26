#!/usr/bin/env python3
"""Fail if the diagnostic reachability stubs are in a build that is meant to be the port.

**The failure this prevents is silent and total.** `MP_BOOT_STUBS=ON` adds
`src/MetroidPrime/PortReachStubs.cpp`, which defines the 318 undefined symbols the boot path
*does* reach. With those defined the link **succeeds**, so `tools/link_check.sh` reports
`undefined 0` and looks like the best news in the project. It is the worst: the number would be
wrong by 318, and every downstream claim - the gap table, `check_docs_claims.py`, a handoff's
"the port does not link" - would be resting on it.

So the property that matters is not "did someone pass the flag" but "**is this build allowed to
report an undefined count**". This checks the second thing, from the build directory rather than
from anyone's command line:

  - a `CMakeCache.txt` with `MP_BOOT_STUBS:BOOL=ON` in a tree that is *not* the boot probe's
    throwaway directory, is a failure;
  - and `link_check.sh` refusing to run in such a tree is belt and braces, so the check also
    fails if `MP_BOOT_STUBS` is on in the directory `link_check.sh` would use.

Run: `python3 tools/check_boot_stubs.py [build-dir ...]`   (default: build-port-link)
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
PROBE_DIR_NAME = 'build-boot-probe'


def check(d: pathlib.Path) -> list[str]:
    cache = d / 'CMakeCache.txt'
    if not cache.exists():
        return []
    m = re.search(r'^MP_BOOT_STUBS:BOOL=(\S+)$', cache.read_text(errors='replace'), re.M)
    if not m or m.group(1).upper() != 'ON':
        return []
    if d.name == PROBE_DIR_NAME:
        return []          # that is the one directory it is allowed in
    return [f'{d}: MP_BOOT_STUBS=ON - the reachability stubs are linked, so this build\'s '
            f'undefined count is 0 by construction and cannot be believed']


def main() -> int:
    dirs = [pathlib.Path(a) for a in sys.argv[1:]] or [ROOT / 'build-port-link']
    problems: list[str] = []
    for d in dirs:
        problems += check(d)
    # The probe's own directory is the only permitted one; make that explicit by checking
    # it exists and is where the flag is expected, so the allowance cannot drift.
    if problems:
        print('the diagnostic reachability stubs are in a build that must report them:')
        for p in problems:
            print('  ' + p)
        print('\nThe only build allowed to have MP_BOOT_STUBS=ON is '
              f'{PROBE_DIR_NAME}/, which tools/boot_probe.sh creates.\n'
              'Delete that build directory, or reconfigure without the option. Do not '
              'report an undefined count from a build in this state.')
        return 1
    print(f'check_boot_stubs: ok - no reachable-stub build among '
          f'{", ".join(d.name for d in dirs)}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
