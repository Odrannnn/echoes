# Vendored runtime snapshots

Aurora and MusyX are tracked source snapshots, not submodules. A normal clone
contains the port patches; no private or unpublished dependency commits need to
be fetched. Keep their license files and source notices when updating them.

| Directory | Upstream | Snapshot imported into this repository |
| --- | --- | --- |
| `musyx` | <https://github.com/AxioDL/musyx> | Unmodified `ec4697520f70bf1a5f361413c1b3d4292ecf7b90`, the commit upstream PrimeDecomp/echoes pins as a submodule |
| `aurora` | <https://github.com/encounter/aurora> | Local port fork `d39404c803d28657c94159a8f44addffdd85a781`, based on port snapshot `32c926ca0f59994249ba4dae5d606a66f8bbd87f` |
| `musyx-port` | <https://github.com/AxioDL/musyx> | Local SDL3 port fork `dc8bed3d6b253113a9f0c7bed57378f91b8dc788`, based on port snapshot `ab6d648668f2c6871eef2c079e8edccf51d7f8ec` |

These commit IDs record provenance; they are not download requirements. Existing
developer checkouts retain their old submodule object databases under
`.git/modules/extern/` for historical comparisons.

The `aurora` and `musyx-port` snapshots were taken from the Metroid Prime port
(`../MetroidPrimePort/extern`) rather than upstream, because the port patches
listed below are SDK-level and game-agnostic. Diff against upstream before
updating them, and keep the patches listed here with them.

Both snapshots carry MIT top-level license files. The earlier recompilation
project's DolRecomp/ModernGekko dependencies are separate and are not linked here.
Retain and review individual source notices as well as the top-level licenses.

Port-specific changes include deferred ARQ callbacks, FIFO draw-sync tokens,
bounded/validated ARAM copies, paused event-pump servicing, Windows runtime DLL
packaging, PC audio stream buffers, pointer-width corrections, atomic muting,
and synchronous voice retirement before releasing group resources.

Aurora still obtains its transitive dependencies through its provider system;
versions are pinned in `aurora/cmake/AuroraDependencyVersions.cmake`. Native CI
builds the vendored source and does not run `git submodule update`.

## Two MusyX copies

`extern/musyx` is what the matching build compiles: upstream's `configure.py` builds the MusyX
runtime objects from `extern/musyx/src`, and only that exact commit reproduces the retail DOL.
Leave it unmodified. `extern/musyx-port` is the port's fork, used only by CMake
(`add_subdirectory(extern/musyx-port)`). The two cannot be one tree yet: upstream MusyX has since
replaced the PC backend with its own (`sndPCOpenAudio`/`sndPCRender`, `hw_pc_sdl.c`), and a
`git merge` of `ec46975` into the port fork conflicts in 13 files, including the whole of `hw_pc.c`.
Moving the port onto upstream's PC API is separate work.
