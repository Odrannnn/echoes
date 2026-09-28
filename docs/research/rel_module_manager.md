# The REL module manager: what is built, and the one input that is missing

## What exists, and is verified

`platform/rel.cpp` (710 lines) and `platform/include/port_rel.h` implement the whole module
runtime, and it is **verified against the real thing**: `tests/port_rel_real.cpp` links every module
in a directory in dependency order, and `orig/G2ME01/files/RelProd/` holds all **86** retail modules
to run it against. The public API is complete for the job:

```cpp
Module Load(const void* data, size_t size, GuestArena& arena);
bool  LinkModule(RelModuleHeader* header, void* bss);
bool  UnlinkModule(RelModuleHeader* header);
bool  Probe(const void* data, size_t size, ImageInfo& info);
size_t ModuleCount();  RelModuleHeader* ModuleAt(size_t i);  void ResetModules();
```

**As of 2026-09-26 `platform/rel.cpp` is compiled into the port** (`mp_platform`). It had been in
neither `mp_platform` nor `files.cmake` — only in the two test executables — so the module loader was
in no binary the port produces. Adding it changed nothing else: the probe goes to 128 files with 0
failures and `tools/link_check.sh` is unchanged at 548 undefined and 0 duplicate definitions, so it
introduces no collision with the game's own symbols.

## The manager's update closure is written (2026-09-28)

`src/MetroidPrime/PortModuleManager.cpp` (port-only, `files.cmake`) holds retail's per-frame
closure, from the asm: `fn_801F05D0` walks `rstl::map<rstl::string, SModuleRecord*>` and erases
records `fn_80213650` says are finished; `fn_80213838` is the state machine (0 reading, 1 linked,
2 cancelling, 3 unloaded); `fn_80213960` reads (`CDvdFile`, `CMemory::Alloc`, `SyncRead`),
`fn_802136A0` links, `fn_802137C0` unlinks, `fn_80213A64` releases, `fn_80213AFC` is the deleting
dtor, `fn_8033EDF4`/`fn_8033EDA8`/`fn_8033EE2C` are the debugger registry (head `lbl_80419CA8`).
Link and unlink call `port::modules::Prolog/Epilog` (`platform/compiled_modules.cpp`) instead of
`OSLinkFixed` + the image's prolog, because the image is PowerPC; a module that is not compiled in
aborts with a message. **Still missing: `fn_801F03C4`**, which inserts records, so the map is
empty at runtime. The section below describes the disc-module route, which this does not settle.

## The missing input, measured rather than assumed

**Nothing calls the runtime.** `port_rel.h` is included by `platform/rel.cpp` and mentioned in
`platform/README.md`; no other file in `platform/` or `src/` includes it. So the port can load a
module and never asks.

The blocker is not the missing caller, it is that **the caller cannot be written: nothing in this
repository says which modules exist, where they are on the disc, or in what order to load them.**
That is the retail module descriptor table, and it is not here. Measured, three places:

| where looked | result |
| --- | --- |
| `build/G2ME01/main.dol` (byte-identical to retail, sha1 `6ef9b491…`) | **2 of 86** module names appear, as the incidental strings `ElitePirate.rel` and `Tweaks.rel` — not a table. Searched both as bare names and as `<Name>.rel`; the bare-name search finds 0, which is how a first pass can wrongly conclude the table is absent from the DOL |
| `config/G2ME01/` | `config.yml`, `splits.txt`, `symbols.txt`, `build.sha1`, `rels/`. No module list, no `rel.json`, no `*.lcf` |
| `orig/G2ME01/` | `files/RelProd/` (the 86 extracted modules) and `sys/`. **No disc image, and no record of any module's disc offset** |

`ImageInfo` in `port_rel.h` is a *single module's* header — version, section count, module id, bss
size, fixed size. There is no DOL-wide enumeration anywhere in the runtime, and none is possible
from what the tree holds.

### The order is already derived, and needs no image

`tools/gen_module_order.py` reads each module's own id from its header and its import table, and
topologically sorts the graph. **86 modules ordered, 27 of them with dependencies, maximum depth 2,
0 cycles, 0 import ids that do not resolve, and 0 ordering violations when every edge is
re-verified against the files.** The result is `docs/research/rel_module_order.md`, and
`tools/gate.sh` fails if it goes stale.

Two things that measurement corrected, both of which would otherwise have been shipped as a
confident answer:

- **A module imports itself.** `Tweaks.rel`'s import table is `{id 82, id 0}`, and 82 is `Tweaks`'
  own `moduleId` — a module lists itself for the relocations applied to its own image. Leaving that
  self-edge in makes all 86 modules one-node cycles and the sort finds nothing ready, which is
  exactly what it reported on the first run.
- **Seven swarm modules sort before `SwarmBasics`, and that is correct.** It looks wrong, because
  `SwarmBasics` owns the `CAi`/`CPatterned` layer they are built on — but `BacteriaSwarm`,
  `Glowbug`, `Metaree`, `Puffer`, `Shrieker` and `IngSnatchingSwarm` import only themselves and the
  DOL, so they have no dependency to respect. `FlyerSwarm`, which genuinely imports `SwarmBasics`
  (id 80), lands at 66 against `SwarmBasics`' 55. **Naming the suspicious case and then measuring it
  is the only reason this is a finding rather than a bug report.**

So what is left for the manager is exactly one thing: reading each module's bytes. Order and
dependency are solved; offsets are not, and offsets are what a disc image (or a shipped table)
provides.

### What would unblock it

**A G2ME01 disc image.** On disc the module table sits in the image's own module information, which
is why it is absent from the DOL: the DOL is the *host* of the modules, not their directory. With an
image, the manager is: find the table, read each module's name and offset, read the bytes through
Aurora's DVD, `Load` then `LinkModule` in dependency order. The runtime half is done and tested; only
the table lookup is missing.

The alternative, if no image is available, is to **generate the table from
`orig/G2ME01/files/RelProd/` and ship it** — the 86 names are known and `port_rel_real.cpp` already
resolves their dependency order from a directory listing. That gives a working manager without the
image, at the cost of hardcoding retail's module set, which is fine for this game and wrong as a
general loader. **This is a decision, not a measurement, and it has not been taken.**

**What that decision would cost, measured so it can be taken on facts:** the 86 modules are
**3.5 MB** in total (86 files; largest `DarkSamus` 188 KB, `Tweaks` 168 KB, `DigitalGuardian`
152 KB). Embedded, they would roughly triple a port binary and would have to be shipped as data
alongside it. Read from the disc at runtime they cost nothing in the binary and stay correct if the
module set ever changes. That asymmetry is the whole of the argument, and it is why the disc route
is the one the port should take — but the shipping route is the one that can be built and tested
today, and the tension between those two facts is the decision.

### Also unfinished, and independent of the above

- **`OSLinkFixed`'s fixed-address path** (the version-3 `impSize` truncation). Deliberately not
  implemented: no module needing it is known for these games and it cannot be tested without one.
  Aurora declares `OSLink`/`OSLinkFixed` in `extern/aurora/include/dolphin/os/OSModule.h` and
  implements neither.
- **Feeding it real disc reads.** The tests read from a directory; nothing reads from the DVD yet.

## What this costs the port today

Not the first frame, immediately. Fourteen modules are compiled into `mp_game` and registered by
`platform/compiled_modules.cpp`, so Tweaks, CannonBall, ForgottenObject and their neighbours do
initialise. The other ~71 are not in the binary and cannot be, so every entity, script object and
GUI element they provide is missing. **The port will not reach a playable front-end until the
manager exists**, which is why this is on the critical path even though it is not the next thing to
write.
