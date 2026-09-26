# A host-only rename of a REL module's entry points perturbs 8 REL images, and nothing else
# reports it

## The result, stated first

Applying a `#ifdef __MWERKS__` rename to the `RELMain`/`RELExit` of 15 reimplemented REL modules
leaves **every game object byte-identical**, leaves **`main.dol`'s sha1 intact**, and leaves
**78 of the 86 REL hashes intact** — and changes **8 of them**. The per-function diff shows
nothing. Reverted; the 16 units stay out of the port build.

This is recorded because the failure is invisible to every check a person would naturally reach
for first, and because two rounds of reasoning about it were wrong before the data settled it.

## What the rename is

```cpp
#ifdef __MWERKS__
#define MP_TWEAKS_MAIN RELMain
#define MP_TWEAKS_EXIT RELExit
#else
#define MP_TWEAKS_MAIN mp_relmain_tweaks
#define MP_TWEAKS_EXIT mp_relexit_tweaks
#endif

extern "C" void MP_TWEAKS_MAIN() { TweaksInit(); }
```

MWCC still compiles `RELMain`, so the game objects should be unchanged — and they are. The point of
this file is that "should be" turned out to be true for the objects and false for the modules.

## The measurements, in the order they were taken, including the two that were wrong

| # | experiment | result | what it seemed to say |
| --- | --- | --- | --- |
| 1 | apply to all 17 files, `decomp_build.sh` | **8 of 86 RELs FAILED** (`Kralee`, `OctapedeSegment`, `Parasite`, `PillBug`, `ScriptPlayerActor`, `ScriptPlayerTurret`, `WallCrawler`, `WallWalker`) | the rename is unsafe |
| 2 | per-file bisect, stashing one file at a time | **every file a "culprit"** | meaningless — see below |
| 3 | compare one object's bytes before/after | **byte-identical** | the rename is safe |
| 4 | coarse isolation: everything *except* `files.cmake` | 8 failures; `files.cmake` alone | `files.cmake` is innocent, the renames are not |
| 5 | is `__MWERKS__` even defined by this build? | **yes** — `#error "MWERKS IS DEFINED"` fires | not a missing-macro problem |
| 6 | apply to **two** files, rebuild | 87 files OK | the rename is safe |
| 7 | apply to all, force `rm *.rel main.elf` and rebuild | **8 failures** | the rename is unsafe |
| 8 | control: clean rebuild at HEAD, no renames | 87 files OK | the clean rebuild is innocent |
| 9 | compare **all 15** renamed objects, HEAD vs renamed | **all byte-identical** | the objects really are unchanged |
| 10 | diff a failing REL against a good one | same size (17,728), differing from byte 13640 in offset-like values (`0xE4`→`0xE0`, `4`→`0`), no strings there | the perturbation is real and localised |

Experiments 3 and 6 were the two that misled. **3** was taken while the tree was in a state where
ninja had not rebuilt everything, so it compared a stale object. **6** could not fail: the RELs are
relinked from `main.elf`, and if every object is unchanged the DOL is unchanged, so the modules are
never relinked and no hash can move. **A test that cannot fail proves nothing** — the same lesson
as the two tools in this project that shipped as empty stubs.

## Why 8 and not 86

The documented rule is that *any* change to the linked DOL shifts every REL, because `makerel` runs
`dtk rel make` with `build/G2ME01/main.elf` as its first input. Here the DOL is **unchanged** — its
sha1 holds — and yet 8 modules move. So this is not that mechanism. The difference is confined to
one offset region of `Kralee.rel` and consists of small offset-like deltas, which points at
something in the module link that depends on more than the DOL's bytes.

Three of the eight map to files that were renamed (`CScriptPlayerActor.cpp`,
`CScriptPlayerTurretRel.cpp`, `CScriptWallCrawler.cpp`). The other five — `Kralee`, `Parasite`,
`PillBug`, `OctapedeSegment`, `WallWalker` — have **no renamed source at all**, which is the part
that does not fit a simple "this file changed its output" story and is why this is unresolved
rather than merely inconvenient.

## The working hypotheses, for whoever picks it up

1. **Something non-deterministic in the module link.** The five unmoved-source modules are the clue:
   if a renamed file's object is byte-identical, the only way its module changes is if the module
   link reads something that is not in the object — a timestamp, a build id, an ordering that
   depends on directory iteration order, or a `ldscript.lcf` include list whose order shifts when
   files are touched. **Grep the module's `ldscript.lcf` and `build.sha1` for anything that is not
   content-addressed.**
2. **Staleness, one level up.** `build.sha1` is a recorded list, and a clean `rm *.rel` plus a
   rebuild is the only way to be sure what is being compared. Every conclusion above rests on a
   `rm`-then-rebuild, and any future bisect must do the same or it will lie again.
3. **The rename is not the variable.** It has never been shown that a *different*, provably
   inert change to those same 15 files does not also move 8 modules. **That is the experiment to run
   first**: add a comment line to each of the 15 files, change nothing else, and check the hashes.
   If that also moves 8, the rename is exonerated and the real cause is whatever touches those
   files.

## Why the 16 units stay out until this is answered

`files.cmake` compiles them, and their `RELMain`/`RELExit` collide — 28 duplicate definitions over
four distinct symbols in the last measurement. The rename fixes that and costs 8 module hashes.
Trading a known 28 duplicates for 8 unknown hash failures is not a trade; leaving them out is a
known, bounded, documented state. The tool that writes the rename is
`tools/rename_module_entries.py` and it is `--check`ed by nothing yet, because until this is
explained there is nothing safe for it to enforce.

## The transferable lesson

**Three checks said this change was fine** — every object byte-identical, the DOL hash intact, 78 of
86 module hashes intact — **and it was not fine.** The per-function diff is not a safety net for
module-level work, and neither is a mostly-green hash run. When a change touches a REL module, the
only evidence is the full 86-hash set on a forced clean rebuild, and the experiment has to be
designed so that it *can* fail.

## Attempt 2 (2026-09-26): an internal linker error, and a diagnosis NOT YET MEASURED

A second attempt renamed 12 files (the set now in `files.cmake`) and also gave six loader
variables an initialiser. The host link went from 331 to 326 undefined symbols with 0 duplicates,
and then mwldeppc failed on `ScriptRsfAudio` and `ScriptPlayerProxy` with
`internal linker error: File: 'ELF_linker.c' Line: 5083` and no diagnostic.

**Where the two failing modules differ from the rest - and the wrong answer first.** The first
explanation offered was the splits: that every other edited unit's split claims a `.bss` range so
the unit owns its loader variable, while `CScriptRsfAudio` and `CScriptPlayerProxy` claim only
`.text`, so their loaders come from dtk's split and the patch's `= 0` added a second definition.
**That is wrong, and it is measurable: both failing modules' `splits.txt` claim a `.bss` range,
exactly as the working `ScriptCannonBall`'s does - one line each, `.bss type:bss align:8`.** The
split is not what distinguishes them. Recorded because the reasoning is plausible, the data is one
command away, and a plausible story that is never measured is what this project's own
`PROCESS_LESSONS.md` is about.

**The discriminator that does hold is `Matching` vs `NonMatching`, and it is the whole constraint.**

| unit | state in `configure.py` | loader at HEAD |
| --- | --- | --- |
| `CScriptRsfAudio` | `Matching` | `extern FScriptLoader lbl_65_bss_0;` |
| `CScriptPlayerProxy` | `Matching` | `extern FScriptLoader lbl_62_bss_0;` |
| `CScriptCoinRel`, `CScriptPufferRel`, `CScriptRiftPortal`, `CScriptSafeZone`, `CScriptWallCrawler`, `CScriptMetaree`, `ScriptGuiSetup`, `CFlyerSwarmRel`, `CScriptPlayerActorMain` | `Matching` | same shape |
| `CScriptSkyRipple` | `NonMatching` | `FScriptLoader REL_loader_SkyRipple;` |
| `CScriptCannonBall` | `NonMatching` | - |

**The two that failed are the two that were `Matching` *and* declared their loader `extern`.** Ten
of the twelve edited units are `Matching`, so mwcceppc's output for them has to reproduce retail's
bytes exactly or the module hash breaks - and a host-side *definition* of a variable retail's object
already defines is exactly that kind of change. `CScriptSkyRipple` and `CScriptCannonBall` are
`NonMatching`, their objects are not held to retail's bytes, and **`CScriptCannonBall` and `Tweaks`
are the only two module TUs that were in `files.cmake` before this change** - which is not a
coincidence and is the fact the first attempt missed while using them as the control.

**So the fix is a shape, not a value: under `__MWERKS__` the source must be HEAD's text token for
token, and every change lives in the `#else` branch.** For a `Matching` unit that declares its
loader `extern`, that means the host supplies the definition:

```cpp
#ifdef __MWERKS__
extern FScriptLoader lbl_65_bss_0;
#else
FScriptLoader lbl_65_bss_0 = 0;
#endif
```

The host still needs a definition, because there is no dtk split object in the host build at all.
And it still must be an *initialised* definition: with no initialiser and no other TU defining it,
the linker binds the symbol as a `FUNC` and places it in read-only `.text`, and the module's own
store into it segfaults. That is the same bug as `REL_loader_CannonBall`, which could be fixed
freely precisely because it is `NonMatching`.

**`CScriptCannonBall` was also the wrong control to diff against**, for the same reason: it is
`NonMatching`, so its object is not in the module link and nothing it does to itself can move a
module hash. Diffing it against a failing file compares a linked unit with an unlinked one.

**The fix, and its result.** The shape above is in the tree, and it is built and measured:

    86/86 modules match config.yml, 0 differ
    sha1sum build/G2ME01/main.dol  ->  6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
    tools/gate.sh                  ->  GATE PASS
    matched 3186 -> 3186, linked 1802 -> 1802
    tools/link_check.sh            ->  326 undefined (was 331), 0 duplicate definitions

Twelve module TUs are in the host build, the registry calls their real entry points, and five link
symbols are closed. **The two attempts that failed are the two that put the change in the shared
part of the source instead of the `#else` branch** - attempt 1 across all 17 files, which moved 8
module hashes, and attempt 2 on the two units that also changed a `Matching` unit's loader from
`extern` to a definition, which is what produced mwldeppc's internal linker error.

Two smaller things that cost time and are worth not repeating:

* **MWCC GC/2.7 is C++98 and `nullptr` is not available in every TU.** It is defined in
  `include/dolphin/types.h` as `NULL`, so a TU that includes it compiles and one that does not does
  not. The first error was `undefined identifier 'nullptr'` from `CScriptCoinRel.cpp`. Use `0`,
  which is what this tree already does for the same purpose
  (`CFlyerSwarmRel.cpp:7`, `FScriptLoader REL_loader_FlyerSwarm = 0;`).
* **A module's own entry points are not the only collisions.** Beyond `RELMain`/`RELExit`,
  `SetFuncPtrs()` is defined by both `CScriptRiftPortal` and `ScriptGuiSetup`, and
  `__ct__10CModelDataFv` by both `CScriptSkyRipple` and `CScriptScriptStreamedMovie` - and the
  latter was *already in the host build*, so its name could not be changed. Three names, not one.
* **`CScriptPlayerActorMain` must define no exit symbol at all.** Retail's module has a prolog and
  no epilog; defining one collides with `CScriptPlayerActor.o`'s real `RELExit`, and
  `port::modules::ShutdownAll` already skips a null shutdown.
* **Never report a hash result from a build that aborted before relinking.** Attempt 1's "DOL
  intact" reading came from exactly that, and it is why 8 modules nearly passed.
