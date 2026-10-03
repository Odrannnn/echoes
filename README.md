Metroid Prime 2: Echoes  
[![Build Status]][actions] [![Code Progress]][progress] [![Data Progress]][progress] [![Discord Badge]][discord]
=============

[Build Status]: https://github.com/PrimeDecomp/echoes/actions/workflows/build.yml/badge.svg
[actions]: https://github.com/PrimeDecomp/echoes/actions/workflows/build.yml
<!-- BEGIN progress (tools/update_readme_progress.py) -->
[Code Progress]: https://img.shields.io/badge/Code-31.13%25-blue
[Data Progress]: https://img.shields.io/badge/Data-41.80%25-blue
[DOL Progress]: https://img.shields.io/badge/DOL-48.22%25-blue
[RELs Progress]: https://img.shields.io/badge/RELs-7.24%25-blue
[progress]: #progress
<!-- END progress -->

[Discord Badge]: https://img.shields.io/discord/727908905392275526?color=%237289DA&logo=discord&logoColor=%23FFFFFF
[discord]: https://discord.gg/hKx3FJJgrV

A decompilation of Metroid Prime 2: Echoes.

Supported versions:

| Version | Release |
|---------|---------|
| `G2ME01` | GameCube (USA) |
| `G2MJ01` | GameCube (Japan) |
| `G2MP01` | GameCube (PAL) |
| `R32J01` | Wii: New Play Control! (Japan) |
| `R3ME01` | Wii: Metroid Prime Trilogy (USA) |
| `R3MP01` | Wii: Metroid Prime Trilogy (PAL) |

Progress
--------

[![DOL Progress]][progress] [![RELs Progress]][progress]

Measured on `G2ME01` (the DOL and all 86 RELs) by `./tools/decomp_build.sh`; refreshed with
`python3 tools/update_readme_progress.py`. "Fully linked" counts only code in units that are
`Matching`, i.e. whose own object is in the link and the output still hashes to retail.

<!-- BEGIN progress-table (tools/update_readme_progress.py) -->
| Part | Code | Data | Functions | Fully linked code |
|------|------|------|-----------|-------------------|
| Everything | 31.13% | 41.80% | 13618 / 28465 (47.84%) | 13.99% |
| DOL (main.dol) | 48.22% | 56.51% | 11680 / 16726 (69.83%) | 22.10% |
| RELs (86 modules) | 7.24% | 2.38% | 1938 / 11739 (16.51%) | 2.66% |
| Game code | 54.14% | 60.21% | 12319 / 15005 (82.10%) | 17.74% |
| SDK | 98.79% | 96.25% | 1299 / 1308 (99.31%) | 94.78% |
<!-- END progress-table -->

This repository builds the following DOLs:

```text
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010  build/G2ME01/main.dol
7f24a768f7b1a687adb88e56559ad8637ed80589  build/G2MJ01/main.dol
5a670d5da3d181e86a0df7cf7751c7055eee35fb  build/G2MP01/main.dol
442947ba57dce414917feab0e75a8227690b3e4b  build/R32J01/rs5mp2jpn_p.dol
2375606f4e9429a699cfa02728b0bb1176421226  build/R3ME01/rs5mp2_p.dol
077712e46eb7cf2488942f337d215636edbc972d  build/R3MP01/rs5mp2_p.dol
```

Dependencies
============

Windows:
--------

- Install [ninja](https://github.com/ninja-build/ninja/releases) and add it to `%PATH%`.
- Install [Python 3.9+](https://www.python.org/downloads/) and add it to `%PATH%`.

macOS:
------

- Install Python and ninja:

  ```sh
  brew install python ninja
  ```

- On Apple Silicon, install Rosetta 2 if prompted.

Linux:
------

- Install Python 3.9+ and [ninja](https://github.com/ninja-build/ninja/wiki/Pre-built-Ninja-packages) from your package manager.
- On non-x86 platforms, install Wine with 32-bit x86 support and configure with `--wrapper /path/to/wine`.

Build tools are downloaded automatically, including the compilers and [wibo 1.1.0](https://github.com/decompals/wibo/releases/tag/1.1.0) on macOS and x86 Linux.

Building
========

- Checkout the repository:

  ```sh
  git clone https://github.com/PrimeDecomp/echoes.git
  cd echoes
  ```

- Copy your game's disc image to `orig/G2ME01` (or the appropriate version).
  - Supported formats: ISO (GCM), RVZ, WIA, WBFS, CISO, NFS, GCZ, TGC.
  - Required files are extracted automatically. The image can be deleted after the first build.
- Configure:

  ```sh
  python configure.py
  ```

  For another version, add `--version G2MP01` or similar. Use `python3` if needed.
- Build:

  ```sh
  ninja
  ```

Diffing
========

Open the project directory in [objdiff 3.7.0](https://github.com/encounter/objdiff/releases/tag/v3.7.0) after the first build. Select an object to compare; edits rebuild automatically.
