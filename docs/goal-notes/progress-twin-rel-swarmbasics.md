# progress-twin-rel-swarmbasics

`kind: progress`, `target: module:SwarmBasics`. Landed as `Matching`:
`src/MetroidPrime/Enemies/CSwarmBasicsLeafCache.cpp` claiming
`config/G2ME01/rels/SwarmBasics/splits.txt`'s `.text 0x00005B90..0x00005C8C`, 0xFC = 252 bytes,
four functions. **module:SwarmBasics `matched_functions` 12 -> 16** (194 functions in the module);
global matched 13307 -> 13311, linked 6355 -> 6359. `tools/goal_check.sh build/goal/item.json`
printed `goal_check: PASS`.

## The four functions, before and after

All four are new to this unit, so the "before" is the same as the "after" being claimed: none
was in any `Matching` unit of the module before this change, and `build/report.json` listed them
as unmatched entries of `SwarmBasics/auto_00_00004280_text`. Per function, as the item asks:

| function | bytes | before | after | twin's source matched unchanged? |
| --- | --- | --- | --- | --- |
| `fn_80_5B90` | 104 | unmatched | **100.00%** | yes - `fn_80248EA4`, `src/WorldFormat/CMetroidAreaCollider.cpp:917` |
| `fn_80_5BF8` | 32 | unmatched | **100.00%** | yes - `fn_80248F0C` / `fn_80248DBC`, same file |
| `fn_80_5C18` | 40 | unmatched | **100.00%** | yes - `fn_80248F2C` / `fn_80248DDC`, same file |
| `fn_80_5C40` | 76 | unmatched | **100.00%** | no - its shape twin is `__ct__Q212CAreaOctTree4NodeFRCQ212CAreaOctTree4Node` and this tree does not have that member-wise copy anywhere; written from `CAreaOctTree::Node`'s layout |

Unit: `SwarmBasics/MetroidPrime/Enemies/CSwarmBasicsLeafCache` - 4/4 functions, 100.00% fuzzy,
100.00% matched code. `mw_version="GC/2.7"` per object (a REL's default is GC/1.3.2, which
measured **8/14** on the same source; GC/2.7 gives 14/14 on the fourteen I first wrote).

## What the four are

One `rstl::reserved_vector< CAreaOctTree::Node, 64 >` chain. The element is `CAreaOctTree::Node`
(`NESTED_CHECK_SIZEOF(CAreaOctTree, Node, 0x24)`, `include/WorldFormat/CAreaOctTree.hpp:91`):
`fn_80_5C40` is its copy constructor, `fn_80_5C18` is `rstl::construct_impl< Node >`, `fn_80_5BF8`
is `rstl::construct< Node >` and `fn_80_5B90` is `uninitialized_copy_n` over it. The 0x24 stride
is in the bytes twice (`addi r31,r31,0x24` / `addi r30,r30,0x24`) and agrees with this module's
own `push_back` at 0x5A5C (`mulli r0,r0,36`). The whole neighbourhood is the octree-leaf-cache
machinery `CAreaCollisionCache` and `CMetroidAreaCollider::COctreeLeafCache` are built from, which
is why it is here: `CSwarmBasics` embeds a `CAreaCollisionCache`.

Both real classes exist and are laid out and size-checked, but their members are private, so a
local struct with the members **named** was used instead of raw offsets -
`tools/check_raw_offsets.py` fails the gate on a raw offset in a file it has no section for, and
the file now has zero raw-offset sites.

## Why the claim is 0x5B90..0x5C8C and not the rest of the run - the expensive part

I first wrote **fourteen** functions and got 12 of them to 100.00%. The claim could not be the
whole run, and the reasons are measured, not guessed.

### 1. Two of the fourteen are a register-allocation wall

`fn_80_5AC0` (140 B) is **98.00%** and `fn_80_58EC` (132 B) is **95.85%**. In both, the only
difference is register allocation: the same 35 instructions in the same order, with the count in
`r6` and the cursor in `r3` where this build puts them in `r5`/`r6` (`fn_80_58EC`: retail
outer-count `r6` / cursor `r5` / counter `r8`, mine `r4` / `r6` / `r7`). Seven spellings were
tried and all left both scores **unchanged**: a local bound for `mCount`; `rstl::destroy` vs
`rstl::destroy_impl`; `&mData[i]` vs `mData + i`; an explicit element cursor (worse, 59.57%);
a `data()` accessor; and `mw_version="GC/1.3.2"` instead of GC/2.7. So the variable is not the
source, and both stay unclaimed rather than going into the link with different registers.

WALL: fn_80_5AC0 98.00% - register allocation only; 7 source spellings tried in this run, none moved it
WALL: fn_80_58EC 95.85% - register allocation only; same 7 spellings, none moved it

### 2. `fn_80_5C8C` and `fn_80_5B4C` reach 100% and are still not in the claim

Both are exact, and both are blocked by something else:

* **`fn_80_5B4C` (0x5B4C, 68 B)** - extending the claim down to it makes **`DigitalGuardian`,
  `ElitePirate`, `Lumite` and `SandBoss`** come out of `dtk rel make` with different bytes: four
  module hashes fail while `main.dol`'s sha1 holds and the per-function diff is clean. Measured
  by narrowing the claim one function at a time:

  | claim | result |
  | --- | --- |
  | `0x5B90..0x5C8C` (landed) | **87 files OK** |
  | `0x5BF8..0x5C8C` | 4 RELs FAILED (DigitalGuardian, ElitePirate, Lumite, SandBoss) |
  | `0x5B4C..0x5C8C` | the same 4 |
  | `0x5C18..0x5C8C` | **87 files OK** |
  | `0xB088..0xB0F0` (same `auto_00_00004280_text` run, 0x8f0 earlier) | **87 files OK** |

  So it is this particular range and not "any new claim in this module". It is also not the
  object's bytes: the same four fail with the object marked **`NonMatching`** and its `.o` out
  of the link, and they **pass** with a one-line comment added to `CSwarmBasicsHooks.cpp` -
  which re-runs the same global `dtk rel make` and matches. **All 86 `.rel` files come out of
  one `makerel` rule in `build.ninja`**, so any module's link re-runs all of them; that is why a
  claim in SwarmBasics can move DigitalGuardian at all.
* **`fn_80_5C8C` (0x5C8C, 88 B)** - exact, but it calls `fn_80_589C`, whose own callee
  `fn_80_58EC` is wall #1 above. It cannot be claimed separately from what it calls:
  `tools/probe_sources.sh` runs `tools/link_check.sh --strict`, which fails when the port's
  undefined count grows, and a `Matching` object calling a symbol nothing defines is exactly one
  new undefined name.

### 3. The five modules that import from SwarmBasics

`FlyerSwarm`, `IngBlobSwarm`, `MetareeSwarm`, `PlantScarabSwarm` and `EmperorIngStage3` all
import from module 80 (read off their own `impOffset` tables). A first attempt at the whole
0x589C..0x5CE4 range had an object that referenced `CMemory::Free`, and those five modules'
12 import-stub addends all moved (`FlyerSwarm`: `0x5efc -> 0x5d1c`, `0x146b -> 0x3469`, ...).
**The landed claim's object has no undefined symbol at all** - `fn_80_5C18` -> `fn_80_5C40`,
`fn_80_5BF8` -> `fn_80_5C18`, `fn_80_5B90` -> `fn_80_5BF8`, and `fn_80_5C40` calls nothing - and
those five stay byte-identical.

## Spelling notes for the next lane (no `NEW:` for these - they are lessons)

* `float mAabb[6]` as a struct member copies as **words** (nine `lwz`/`stw`); six named float
  scalars give retail's nine `lfs`/`stfs`/`lwz` with `f1`/`f0` alternating. 0.00% -> 100.00% on
  `fn_80_5C40` from that one change.
* `new (dest) SNode(src)` emits a **weak COMDAT copy** of the copy constructor into the object,
  which a fixed-width claim has no room for. The `if (dest != nullptr) fn_80_5C40(dest, src);`
  spelling - plus `#pragma dont_inline on` around the four definitions - gives the eight
  instructions retail has, with no fifth symbol.
* A struct's **first two words as one aggregate** (`self->mHead = src.mHead;`) give retail's
  `lwz/lwz/stw/stw`; two separate member assignments give `lwz/stw/lwz/stw`. That is
  `fn_80_5A64`'s only difference and it moved that function 94.87% -> 100.00%.
* A destructor that hands its callee the **address** of a member (`addi r3,r30,0x18`) is not
  `free(p.mPtr)` - that compiles to `lwz r3,24(r30)`. Passing `&self->mPtr` reproduces it.
* `fn_80_5A64` **returns** its receiver even though the last instruction before `blr` is
  `addi r1,0x10`: `mr r3,r30` sits at 0x5AA0, before the final `stb`. Reading only the tail
  makes a function look `void`.

## Verification

    ./tools/decomp_build.sh -r      ->  87 files OK  (0 computed checksums did NOT match)
    build/report.json               ->  SwarmBasics/MetroidPrime/Enemies/CSwarmBasicsLeafCache
                                         4/4 functions, 100.00% fuzzy, 100.00% matched code
                                       module:SwarmBasics matched_functions 12 -> 16 / 194
    sha1sum build/G2ME01/main.dol   ->  6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
    python3 tools/check_decl_order.py --unit MetroidPrime/Enemies/CSwarmBasicsLeafCache.cpp -> ok
    ./tools/goal_check.sh build/goal/item.json
      ok  gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
      ok  counts: matched 13307 -> 13311   linked 6355 -> 6359
      ok  target rose: module:SwarmBasics: 12 -> 16 / 194 functions
      ok  no asm added
      goal_check: PASS progress-twin-rel-swarmbasics

Files touched: `configure.py` (+1 `Object(Matching, ...)`, line 2015),
`config/G2ME01/rels/SwarmBasics/splits.txt` (+3), `files.cmake` (+1),
`src/MetroidPrime/Enemies/CSwarmBasicsLeafCache.cpp` (new).

## NEW

NEW: claim-range-hazard-swarmbasics | match | SwarmBasics | claiming .text 0x5B4C..0x5B90 or 0x5BF8..0x5C18 of module 80's auto_00_00004280_text breaks the hashes of DigitalGuardian, ElitePirate, Lumite and SandBoss although their own objects and main.dol are byte-identical (0x5B90..0x5C8C and 0xB088..0xB0F0 in the same run are clean); the four functions that would fill the range, fn_80_5B4C/fn_80_5C8C and the 0x5AC0/0x58EC pair below them, are otherwise at 98-100%.