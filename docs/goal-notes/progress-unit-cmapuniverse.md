# progress-unit-cmapuniverse — `MetroidPrime/CMapUniverse`, 33/40 → 37/40

**Result: PASS** (`tools/goal_check.sh build/goal/item.json`, run twice on the final tree).
Four functions in the unit went from unmatched to 100%; nothing anywhere got worse.

| function | before | after |
|---|---|---|
| `rstl::swap<CMapUniverse::CMapObjectSortInfo>` | 63.76% | **100.00%** |
| `rstl::iter_swap<pointer_iterator<CMapObjectSortInfo,…>>` | 63.67% | **100.00%** |
| `rstl::__sort3<CMapObjectSortInfo, CMapObjectSortInfoGreaterThan>` | 86.31% | **100.00%** |
| `rstl::__insertion_sort<pointer_iterator<CMapObjectSortInfo,…>>` | 60.94% | **100.00%** |

Unit: `matched_functions 33 → 37 / 40`, `.text` fuzzy 92.81% → 95.45%.
Global: `All: 33.45% fuzzy, 26.51% matched` → `33.46% / 26.52%` (11800 → 11804), linked
unchanged at 5727 (correct: the unit stays `NonMatching`).

## The change

One edit, in `include/MetroidPrime/CMapUniverse.hpp:24-32`: `CMapObjectSortInfo` gets a
user-provided `operator=`. Nothing else. No source change to `CMapUniverse.cpp`, no change to
`rstl::swap`/`iter_swap`/`__sort3`/`__insertion_sort` (their template spellings are already
right — see below).

## Why: mwcc gives a local aggregate a stack home, and `operator=` is what removes it

All four functions failed the same way, and it was not a logic error. Retail's `swap`
(0x8015607C, 100 B) is a **frameless** function that parks `a`'s six fields in `f1` + `r5..r9`,
copies `b` into `a` field by field, then writes the registers into `b`. Our build produced the
identical instruction sequence **plus a `stwu r1,-32(r1)` frame** and five *dead* stores into
`8(r1)..28(r1)` (`rstl::swap` at `.tmp` probe output, and `rstl::iter_swap`, `__sort3`,
`__insertion_sort` likewise). Dead stores mean the local had a stack home it did not need.

A user-provided copy constructor does **not** fix it; a user-provided assignment operator does.
Measured with standalone probes compiled through the project's own mwcceppc flags
(`include/rstl/algorithm.hpp`'s `swap` body is `T tmp(a); a = b; b = tmp;`):

| swapped type (24 B: `float` + 5 `int`) | frame? |
|---|---|
| plain POD | yes |
| user-provided **copy ctor** only | yes |
| user-provided **assignment** only | **no** — byte-identical to retail |
| user-provided copy ctor **and** assignment | **no** |

The member types matter too: a POD *member* with a user-provided copy ctor (like `CColor`, which
has `CColor(const CColor& other) : mRgba(other.mRgba) {}`) forces the frame back on. So
`CMapObjectSortInfo` needs `operator=` on itself, and a copy ctor alone is not enough — I tried
that first and it changed nothing (still 33/40).

This is a *type* property, not a template property: `rstl::swap<pair<w, CGlyph>>`,
`swap<CElementGen::CTexturedParticleListItem>` and the `CTransitionDatabaseGame` instantiations
were already 100% in this tree with the same `rstl::swap` body, so the template is not the thing
to change. Retail's 5-field `swap<CTexturedParticleListItem>` (0x802DC7D4) *does* have a frame and
matches us, so this is not "retail never spills" — it is this one type.

## Left unmatched, and why

- **`GetMapWorldDataByWorldId` — 84.69%, WALL.** Retail hoists `lwz r5,28(r3)` (the vector's
  `mItems`) into the preheader and keeps the byte offset in `r7`; mwcc sinks the same load into
  the loop, reuses `r0` for it and puts the offset in `r6`. Same instruction sequence otherwise.
  The tail (`lwz r3,28(r3); blr` for `mWorldDatas[0]`) is identical in both, so the hoisted value
  is loop-only — this is mwcc's LICM, not scheduling I can reach from the source.
  ~35 spellings tried with `tools/try_batch.py`, ranked by differing instructions: best 5 of 16
  (`*(mWorldDatas.data() + i)`, `mWorldDatas.data()[i]`, `mWorldDatas.data()[i]` with an `int n`
  local, and the same with the compare flipped); the shipped `GetMapWorldData(i)` form is 7.
  Rejected: hoisting the pointer into a named local (11-13, mwcc turns it into a pointer walk),
  `begin()/end()` iterator loop (14), pointer walk with an end pointer (14), `unsigned i` (6, but
  it makes the bound `cmplwi` where retail has `cmpwi`), `int n` + explicit `if (n != 0)` guard
  (7-9), `do/while` (12), `while` (10), `capacity()` as the bound (8).
  *Do not re-try the pointer-hoisting family; it is a dead end.*
- **`CMapUniverse::CMapUniverse(CInputStream&, uint)` — 93.11%.** Not a logic difference: the
  instruction sequence is identical instruction-for-instruction and only the callee-saved
  register *numbering* differs. Retail: `version→r31, in→r30, this→r29, &mHexagonToken→r28`
  (parameters in reverse order, then the local). Ours: `this→r31, &mHexagonToken→r30,
  version→r29, in→r28`. mwcc hands out r31, r30, r29, r28 in declaration order, and this
  translation unit does not declare those four variables differently — I could not find a
  spelling that reorders them.
- **`Draw(...)` — 84.02%.** Ours is 1820 B against retail's 1812 B and 452 of 455 instructions
  differ, so this is a real spelling difference in a 1812-byte function, not register
  allocation. Out of reach for one item; it wants its own session.

## Notes

- `tools/probe_cc.sh` does not compile under `src/MetroidPrime/CMapUniverse.cpp`: its flag list
  is missing `-i extern/musyx/include` and the three `-DMUSY_*` defines that `build.ninja` has,
  so `CAudioSys.hpp` → `musyx/musyx.h` fails. `tools/bytesdiff.sh` therefore cannot be used on
  any unit that reaches `CAudioSys.hpp`. Copy the rule from the unit's own `build.ninja` entry.
- `tools/try_batch.py` works on this unit unchanged and restores the source; it counts differing
  instructions, which is the right ranking for a pure register-allocation difference.
- `tools/goal_check.sh` runs its gate with `MP_GATE_DOCS_WRITE=1`, so **running the judge rewrites
  `docs/HANDOFF.md`'s derived counts** (11800→11804, 10252→10256). That is the judge's own write,
  not mine, and it must stay: reverting it would make `check_docs_claims.py` fail on the next run.
- `tools/goal_check.sh` verifies that no judge-owned path was touched, and it *did not* flag
  `.tmp/opencode/` (gitignored at `.gitignore:53`) where my probe script lives.

## Nothing filed as `NEW:`

All three remaining functions are real units' worth of work but not new *items*: they are the same
unit, already queued as this item's target, and re-filing them would double-count.

WALL: GetMapWorldDataByWorldId__12CMapUniverseFUi 84.69% - mwcc sinks the loop-invariant
`mItems` load into the loop where retail hoists it; 35 spellings tried, best leaves 5 of 16
instructions differing and they are all register numbering.
