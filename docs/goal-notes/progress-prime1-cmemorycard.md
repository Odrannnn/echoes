# progress-prime1-cmemorycard

`kind: progress`, target `MetroidPrime/CMemoryCard`. The unit stays `NonMatching`; `flip_test.sh`
was deliberately **not** run - a `progress` item is judged on `report.json`'s per-function exact
matches, and the unit is nowhere near flipping (10/56 functions, 47.81% fuzzy, 43.88% matched
code; the remaining 46 functions are 0% `fn_*` auto units from `auto_*.o` stubs and out of scope
for this item). Prime 1's `src/MetroidPrime/CMemoryCard.cpp` was the starting point, adapted to this
repo's headers, member names and the extra Echoes fields; nothing was copied from Prime 1's
headers and no class layout was changed.

## Result

`main/MetroidPrime/CMemoryCard`: **4/56 -> 10/56** functions at 100%,
matched code 8.78% -> 43.88%, fuzzy 36.01% -> 47.81%.
`All:` 30.61% fuzzy / 22.74% matched / 11.74% linked (9939) -> 30.71% / 22.92% / 11.74% (9970).

`tools/report_diff.py build/goal/judge/report.base.json build/report.json` prints
`matched 9964 -> 9970  linked 4896 -> 4896  (+6 functions at 100%, 0 units newly linked)` and
`no regression`. `tools/goal_check.sh build/goal/item.json` prints `PASS`.

Per function, from `build/report.json` against the judge's baseline
`build/goal/judge/report.base.json`:

| function | before | after | Prime 1's source |
| --- | --- | --- | --- |
| `__ct__11CMemoryCardFv` | 96.33% | **100%** | Prime 1's, with the Echoes `mDarkWorldNameId` and 40-not-16 reserves; one measured edit - a local reference to the new vector (below) |
| `InitializePump__22CSaveWorldIntermediateFv` | 12.95% | **100%** | Prime 1's body plus Echoes' `IGetDarkStringTableAssetId`, `mLayerNames`, `mAreaLayerNameOffsets`; one measured edit, `push_back_unsafe` (below) |
| `InitializePump__11CMemoryCardFv` | 94.62% | **100%** | Prime 1's `if (world.InitializePump()) {...} else { done = false; }` shape; two measured edits, the `if` shape and `push_back_unsafe` (below) |
| `HasSaveWorldMemory__11CMemoryCardCFUi` | 45.12% | **100%** | Prime 1's, but the `AUTO(it, ...)` becomes a named `const_iterator` (below) |
| `GetFrontEndName__16CSaveWorldMemoryCFv` | 48.35% | **100%** | **not** Prime 1's - Echoes indexes `GetString(count < 4 ? 0 : 3)`, Prime 1 always returns 0. Only the branch shape needed tuning (below) |
| `GetDarkFrontEndName__16CSaveWorldMemoryCFv` | 48.35% | **100%** | Echoes-only twin of the above; same edit |
| `GetAreaAndWorldIdForSaveId__11CMemoryCardCFUi` | 87.08% | 87.08% | unchanged - not in the item, and three spellings tried, none better (below) |
| `MergeEnvironmentVariables` | 73.36% | 86.20% | Echoes-only (not in Prime 1); one measured edit, `push_back_unsafe` |

**All 5 functions named in `item.json` are at 100%.** Nothing anywhere got worse. Two functions
outside the item's list came along for free (`GetDarkFrontEndName`, and `MergeEnvironmentVariables`
which only needed the same `push_back_unsafe` edit). The diff adds no `asm` and touches exactly one
file: `src/MetroidPrime/CMemoryCard.cpp`. No header, `configure.py`, `config/` or `files.cmake`
change was needed.

## The four measurements that produced the result

### 1. `push_back_unsafe`, not `push_back` - four sites

Every one of these functions reserves (or is already known to have capacity) and then appends, and
retail's loop body has **no capacity check** - it computes `data + count * size`, stores the element
and bumps the count, with nothing between. `rstl::vector::push_back` emits
`if (count == capacity) reserve(capacity ? capacity*2 : 4);` first, which was worth 12 instructions
per site and is what kept these functions from reaching 100%.

| site | before | after |
| --- | --- | --- |
| `CSaveWorldIntermediate::InitializePump`, `mAreaIds` (after `reserve(areaCount)`) | 91.72% | **100%** |
| `CMemoryCard::InitializePump`, `mScanStates` (after `reserve(...)`) | 94.62% | 100% (with the `if` fix) |
| `MergeEnvironmentVariables`, `destination` (after `reserve(...)`) | 73.36% | 86.20% |
| `CMemoryCard::InitializePump`, `mWorldInter->push_back` in the ctor | 96.33% | unchanged (has a real check) |

`push_back_unsafe` is the honest spelling here, not a trick to dodge a check: each site is
immediately preceded by a `reserve` for a count the loop provably cannot exceed, and the ctor site
keeps the checked `push_back` because retail's does check there.

### 2. `GetFrontEndName` / `GetDarkFrontEndName` - Prime 1's source does not apply

Prime 1's is `return mWorldName->GetObject()->GetString(0);` - no count test at all. Echoes' is
`GetString(GetStringCount() < 4 ? 0 : 3)` and that is what this repo already had. The measured diff
was that the existing `if (!valid() || GetObject() == nullptr) return nullptr;` spelling made mwcc
**compile the ternary branchlessly** (`subfc`/`srwi`/`andc`, 9 instructions where retail has 2
branches). Two edits fixed it, and both are the ones Prime 1 already uses for the same shape:

- invert the guard to the positive form (`if (valid() && GetObject() != nullptr) { ... }
  return nullptr;`), which puts retail's `return nullptr` block at the *end* with two `beq` into it
  instead of two short branches over it - 48.35% -> 99.70%;
- spell the test as `if (count >= 4) return GetString(3); return GetString(0);` rather than
  `< 4 ? 0 : 3`, which is what makes the compiler branch instead of computing a select -
  99.70% -> **100%**.

### 3. `CMemoryCard::InitializePump` - the loop's `if` is positive in retail

The body was already right; the shape was not. The existing source was
`if (!world.InitializePump()) { done = false; continue; }` followed by an unconditional block.
Retail loads the result and does `clrlwi. r0, r3, 24; beq` - i.e. the *taken* path is the body. That
is Prime 1's spelling exactly (`if (world.InitializePump()) { ... } else { done = false; }`), and
adopting it took the function 94.62% -> 95.60%; combined with `push_back_unsafe`, **100%**.

### 4. Two small local-reference edits

- **ctor**: `rstl::vector<CSaveWorldIntermediate>& worlds = *mWorldInter;` (Prime 1's line 18),
  used for both `reserve` and the loop's `push_back`. Without it the compiler reloaded
  `mWorldInter` from `this` before the second `reserve` (`lwz r3, 0x1c(r29)`) where retail holds it
  in `r31`. 96.33% -> **100%**.
- **`HasSaveWorldMemory`**: Prime 1's `AUTO(it, rstl::find_by_key(...))` written out as a named
  `const_iterator`, then `it != mMemoryWorlds.end()`. The one-liner compiled to a form where the
  `end` computation was hoisted *above* the `find_by_key` call; retail computes it after. 45.12% ->
  **100%**.

## What I tried that did not help (so the next run does not repeat it)

- **`GetAreaAndWorldIdForSaveId` stays at 87.08%.** Not in the item's list, and the residual is
  purely stack-slot allocation: retail's frame is `0x30` and it spills `begin`, `end`, the found
  iterator and the pair in a different order than we do (retail keeps the outer iterator in `r8`
  and the area begin in `r9`; we keep the areas' base in a register and the found iterator in a
  stack slot). Three spellings tried, none better: (a) `const rstl::vector<uint>& areas` + inline
  `find` - 87.08%, the current source; (b) `find(it->second.mAreaIds.begin(),
  it->second.mAreaIds.end(), saveId)` with no `areas` reference - **83.20%, worse**; (c) named
  `begin`/`end` iterators - 87.65%, +0.57% but three extra lines for a register-allocation
  coincidence. Reverted to (a). This is the same class of wall as the notes' `WALL:` rule: the
  remaining diff is register allocation, not semantics.
- `TAreaId(i)`'s double spill (`stw r28, 0xc(r1)` *and* `stw r28, 0x8(r1)`) in
  `CSaveWorldIntermediate::InitializePump` matched once the `push_back_unsafe` edit landed, so it
  needed no separate action. Do not "fix" it.
- Inlining `rstl::lower_bound` / `rstl::find` by hand: never needed. Prime 1's plain call
  spellings matched retail's out-of-line `find_by_key` / `binary_find` calls exactly.

## What is still not done, and why it is not this item

The other 46 functions in the unit are `fn_*` at 0% and are **not** in this unit's `.cpp`: they
are the `auto_*`/`auto_03_*` object stubs in `configure.py` that `check_raw_offsets.py` files under
this address range (`fn_80176A88`, `fn_80177220`, `fn_80178BE0`, `fn_80178C74`, `fn_80178ECC`, the
`CDummyWorld`/`rstl::vector` out-of-lines and the `__ct__`/`__dt__` pairs). They are separate units
and a separate carve; touching them would be a `configure.py` + `splits.txt` + `files.cmake`
change, which this item does not call for. The `CMemoryCard` unit as carved cannot flip while those
live in other objects.

## Gates

    sha1sum build/G2ME01/main.dol                 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010  (matches)
    ./tools/probe_sources.sh                      749 files, 0 failed, 0 errors
    python3 tools/check_symbol_names.py           503 units, 0 missing names
    ./tools/decomp_build.sh                       All: 30.71% / 22.92% / 11.74% (9970 / 28465)
    ./tools/link_check.sh                         unchanged from baseline (250 undefined, 0 duplicates)
    ./tools/goal_check.sh build/goal/item.json    PASS progress-prime1-cmemorycard

No undefined symbol was added: the three `#include`s the body needs
(`MetroidPrime/CWorldLayerState.hpp`, `MetroidPrime/IGameArea.hpp`,
`MetroidPrime/Player/CGameState.hpp`) are all headers, and every callee reached is either a virtual
call through an existing vtable slot or an already-declared method, so the port's undefined count
and its symbol list are byte-identical to the baseline.

## Codegen rules this unit confirmed (for the notes of the next item)

- **`push_back_unsafe` when the append is capacity-proven.** Four sites, each worth 12 instructions.
  Retail's unchecked append is `data + count*size; store; count+1` and nothing else.
- **A `?:` over a comparison compiles to a branchless select under `-O4,p`; an `if`/`return` pair
  compiles to real branches.** Retail's two-instruction `cmpwi`/`blt` pair is only reachable from the
  statement form. This bit `GetFrontEndName` twice.
- **Retail puts a function's early-exit block last when the guard is positive** (`clrlwi.; beq` over
  the body to a trailing `return`), and first when the guard is negative. Rewriting the guard's
  polarity moves that block and is worth several percent on its own.
- **An `AUTO(it, expr)` in Prime 1 is often a real named local, not a convenience.** Here the named
  `const_iterator` and the named `&worlds` reference each changed the emitted code; the one-liners
  reload from `this` where retail keeps the value in a register.
- **Echoes' `CMemoryCard` is a superset of Prime 1's, not a rename of it.** Five members are new
  (`mDarkWorldNameId`, `mLayerNames`, `mAreaLayerNameOffsets`, and the two dark-world token checks),
  and Prime 1's `GetFrontEndName` has no count test where Echoes' does. Porting Prime 1's source
  verbatim would have *lowered* this unit.
