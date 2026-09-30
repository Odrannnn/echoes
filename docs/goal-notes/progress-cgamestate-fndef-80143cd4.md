# progress-cgamestate-fndef-80143cd4

Lane 4, `wt-mp2-goal-L4`, head `c966f3e`. **The item passed.** `fn_80143CD4` now matches retail
**exactly**: 0 differing instructions of 20, 80 bytes, which is retail's own size, and
`main/MetroidPrime/Player/CGameState` went **101 -> 102 / 116** functions. The unit stays
`NonMatching` (no flip attempted, as a `progress` item requires). Three files, 38 insertions.

## Measured, in the order the judge reads it

    ./tools/goal_check.sh build/goal/item.json
      ok    no judge-owned path touched
      ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
      ok    counts: matched 10048 -> 10049   linked 4918 -> 4918
      ok    check_symbol_names.py
      ok    All:  30.99% fuzzy, 23.27% matched, 11.78% linked (10049 / 28465 functions)
      ok    target rose: main/MetroidPrime/Player/CGameState: 101 -> 102 / 116 functions
      ok    no asm added
    goal_check: PASS progress-cgamestate-fndef-80143cd4

    ./tools/decomp_build.sh MetroidPrime/Player/CGameState
      main/MetroidPrime/Player/CGameState: 87.85% fuzzy, 60.61% matched (102 / 116 functions)
      (was 87.40886% / 101 / 116 on this head)

    sha1sum build/G2ME01/main.dol   6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
    python3 tools/check_symbol_names.py   checked 504 units; 0 declared names are missing
    ./tools/probe_sources.sh   750 files, 0 failed, 0 errors; LINKED (250 undefined, 0 duplicates)
    python3 tools/check_decl_order.py --unit MetroidPrime/Player/CGameState
      ok: 4 unit(s) checked, none emits its functions out of retail order

I also checked the blast radius directly: disassembling every defined symbol in the probe
object against ninja's `CGameState.o`, **only `fn_80143CD4` differs**, and the two objects
have the same symbol set. The 64-byte shrink shifts every later function but permutes nothing,
and the header change to `rstl/reserved_vector.hpp` is access-specifier-only.

## The probe (re-created; the previous run's `.tmp/` copy did not survive `git clean`)

    $ time ./.tmp/opencode/probe_gs.sh src/MetroidPrime/Player/CGameState.cpp .tmp/opencode/probe_gs.o
    real 0m0.32s
    $ cmp .tmp/opencode/probe_gs.o build/G2ME01/src/MetroidPrime/Player/CGameState.o
    (identical)

`build.ninja`'s exact `mwcc_sjis` cflags - including `-i extern/musyx/include`, the four
`-DMUSY_*` defines and `-pragma "inline_max_size(125)"` that `tools/probe_cc.sh` omits - through
wibo + sjiswrap + mwcceppc. A driver next to it substitutes a candidate body between
`fn_80143CD4`'s real signature braces and scores it with `tools/bytescmp.py`, i.e. by
**differing instructions, not by objdiff's percentage**. Every number below came from it; the
whole ladder is 90 candidate compiles, about 35 s. Both live under gitignored `.tmp/`, so the
diff stays clean - and, again, `git clean` destroys them. The queued
`progress-cgamestate-probe-tool` item already asks for this to become a `tools/` script; I did
not re-file it.

## The ladder (all measured on this head; spellings so the next run skips them)

`differing / total instructions`, and the object's size:

| spelling | score | note |
|---|---|---|
| `resize(src->size())` + a hand field loop (what was there) | 20/36, 144 B | a frame and two out-of-line calls |
| `resize` + `(*self)[i] = (*src)[i]` | 20/36, 144 B | identical object |
| `uninitialized_copy_n` over the real `SPlayerConfig` | 15/16, 64 B | no null guard, 2 word moves per element |
| local `Shadow {int; Elem[4]}` + `uninitialized_copy_n`, `Elem` a plain `{uint;bool;bool}` | 12/18, 72 B | the previous run's best; **2 instructions short** |
| the same, but a hand-written field loop instead of `uninitialized_copy_n` | 16/18, 72 B | field-wise copy yes, wrong loop shape |
| **`uninitialized_copy_n` + a user-provided copy constructor on the element** | 11/20, **80 B** | retail's size; the copy is field-wise |
| the same with the loop written out by hand instead of calling `uninitialized_copy_n` | 7/20, 80 B | |
| ...with `it` declared before `cur` | 6/20, 80 B | cursors now r5/r4, scratch r3 |
| **...plus the function returns `*self`** | 11/20, 80 B | cursors flip to r5=src / r6=dst |
| **...with `cur` declared before `it`** | **0/20, 80 B** | exact |
| `return self;` + plain `rstl::uninitialized_copy_n(src->data(), self->mCount, self->data())` | **0/20, 80 B** | exact, and this is the form that shipped - no locals needed |

Three things had to be true at once. Each is a codegen rule, not a spelling preference, and
each is recorded in a comment at the code it explains.

**1. The element must not be trivially copyable, or mwcceppc merges the copy into two word
moves.** Retail's loop body is six instructions - `lwz`/`lbz`/`stw`/`lbz`/`stb`/`stb` - for an
8-byte `{uint, bool, bool}`. With the implicit copy constructor, `construct_impl`'s
`new (dest) T(src)` inlines to `lwz`+`lwz`+`stw`+`stw`, which is 18 instructions and 72 bytes
for the whole function. One user-provided copy constructor on `CGMFrontEnd::SPlayerConfig` is
enough to stop the merge. (A user-provided copy *assignment* is not: `construct_impl` goes
through the copy **constructor**, and `2-userctor-assign` scored the same 11/20 as
`1-userctor` while `8-userctor-assign-loop` fell back to 16/18.) The null guard
`cmplwi r5,0`/`beq` inside the loop is that same placement new - it is free with this shape and
costs 4 extra instructions if you also write `if (cur)` by hand (`q6-guard-dst`, 12/21).

**2. The count is stored before the copy and read back out of `self`.** Retail does
`stw r0,0(r3)` at 0x80143CE0 and then `lwz r0,0(r3)` at 0x80143CE4, so the loop bound is
`self->mCount`, not `src->mCount`. `uninitialized_copy_n(src->data(), self->mCount,
self->data())` is what produces the reload; taking `n` from `src` instead (`B-ucn-srcn`)
scored 15/17 and 64 bytes. It also means retail's `operator=` stores the count *first* and has
no `if (this != &other)`, which is not what `include/rstl/reserved_vector.hpp` currently says -
see the caveat below.

**3. The function returns `*self`, and that is what fixes the registers.** This was the whole
last 11 instructions and it is the least obvious of the three. Nothing in retail's 80 bytes
needs a return value - the epilogue is a bare `blr` - but returning `*self` (which is what a C++
`operator=` does, and what `rstl::vector::operator=` in this repo already returns) keeps
`self` **live in r3 all the way to the `blr`**. That removes r3 from the allocator's pool, and
the whole loop allocation shifts up by one: the scratch register for `lbz` lands in r4 instead
of r3, which frees r4, which pushes the destination cursor to r5 and the source cursor to r6.
Declared `void`, the identical body allocates cursors in r4/r5 and the scratch in r3 - six
instructions of pure register numbering off, and `bytescmp` reports 6/20 where the return value
reports 11/20 with the two cursors the other way round. Worth remembering generally: **in
MWCC, a function's incoming parameter registers are reusable as temporaries only once they
die, so anything that extends a parameter's live range to the epilogue renumbers the whole
body.**

## Files touched

- `src/MetroidPrime/Player/CGameState.cpp:832-855` - `fn_80143CD4` rewritten as
  `reserved_vector::operator=`'s three statements, returning `SFrontEndPlayerConfigs&`. The
  comment records retail's addresses and why each of the three details is load-bearing. Same
  `extern "C"` name, so the symbol is unchanged; the caller in `CGMFrontEnd`'s copy
  constructor is untouched and its bytes are unchanged.
- `include/MetroidPrime/Player/CGMFrontEnd.hpp:21-35` - `SPlayerConfig` gets a user-provided
  copy constructor, with the reason. `mPlayers` is only ever touched in `CGameState.cpp`, so
  this cannot move another unit.
- `include/rstl/reserved_vector.hpp:13-21` - `mCount`/`mData` moved from `private` to
  `public`, matching `rstl::vector`, which already has all its members public for exactly this
  reason (retail's out-of-line template `operator=` can only be claimed by writing it out under
  an `extern "C"` name, and that code needs the members). Access is codegen-neutral and the
  full-tree build confirms it.

## Caveats and things the next run should know

- **`reserved_vector::operator=` itself is still wrong** and I did not touch it, because it is
  shared and out of scope for this item. `include/rstl/reserved_vector.hpp:103-110` still reads
  `if (this != &other) { destroy_elements(); uninitialized_copy(...); mCount = other.mCount; }`,
  where `fn_80143CD4` shows retail doing `mCount = other.mCount; uninitialized_copy_n(...);` with
  no self-check. Now that the members are public, changing it is a one-line edit, but it would
  alter every unit that assigns a `reserved_vector`, and this item was not the place to measure
  that. Worth its own item.
- `SPlayerConfig`'s user-provided copy constructor **suppresses the implicit default
  constructor**. Nothing default-constructs it - the full 2043-file build is clean - but a later
  change that needs `SPlayerConfig()` will have to add one, and it must not be
  `= default` in a way that reintroduces the implicit trivial copy.
- `rstl::reserved_vector::resize(int, const T& = T())` still mentions `T()`; it compiles today
  only because no instantiation of `resize` for `SPlayerConfig` omits the second argument. The
  copy constructor does not break it, but it is one call away from breaking.

## Sub-100% in this unit, re-measured on this head (for whoever picks it up next)

    __ct__18CPersistentOptionsFR16CBitStreamReader   95.52%  776 B
    PutTo__18CPersistentOptionsCFR16CBitStreamWriter 94.35%  600 B
    __ct__11CWorldStateFR16CBitStreamReaderUiR...     97.38%  568 B
    PutTo__11CWorldStateCFR16CBitStreamWriterRC...     97.30%  148 B
    __ct__10CGameStateFR16CBitStreamReader            84.14% 1668 B
    PutTo__10CGameStateFR16CBitStreamWriter           91.90%  876 B
    __ct__11CGMFrontEndFRC11CGMFrontEnd               88.86%  140 B
    __sinit_CGameState_cpp                            63.35%   80 B
    fn_801466F4                                       66.63%  172 B
    StartGameFromFrontEnd__Fv                         59.35%  784 B
    fn_801465EC / fn_80146338 / LoadGameFileState / __dt__11CGMFrontEndFv   0.00%

The four `mr r5,r31` dead-move walls in the previous runs' notes are unchanged and still
unreachable. `AddVariable` is at 100% from the L9 retry and is not in this list.

## New queue items

None. The only candidate is the `progress-cgamestate-probe-tool` item the previous run already
filed for the throwaway probe harness; filing it twice would just duplicate a queued item. The
register-allocation rule in point 3 above is a lesson, which the brief says to put here rather
than in the queue.
