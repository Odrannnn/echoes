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
