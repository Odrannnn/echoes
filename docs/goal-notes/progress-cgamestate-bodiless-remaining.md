# progress-cgamestate-bodiless-remaining

`kind: progress` - `MetroidPrime/Player/CGameState`, stays `NonMatching`. No carve, no
`configure.py` change, no `config/` change, no `asm`, nothing committed. Branch head `97d3797`
(`progress-cgamestate-loadcompressed`), lane 7.

## Result: the gate is clean, but the unit's matched count did not rise, so this item FAILS

`matched_functions` is **94 / 116 before and after**. `tools/goal_check.sh`'s `target_rose` test
needs it to rise strictly, so the judge will reject the item. What did land is one real
improvement to an existing body, and a wall that is now *characterised* rather than guessed at:

| | before | after |
| --- | --- | --- |
| `__ct__11CWorldStateFR16CBitStreamReaderUiRC18CWorldSaveGameInfo` | 97.38028% | **99.29578%** |
| unit `fuzzy_match_percent` | 83.19763 | **83.25729** |
| unit `matched_functions` | 94 / 116 | 94 / 116 (unchanged) |
| unit `matched_code` | 10060 / 18236 | 10060 / 18236 (unchanged) |
| unit `total_code` | 18236 | 18236 (unchanged - nothing was appended) |
| global `matched` / `linked` | 9932 / 4895 | 9932 / 4895 |

```
./tools/gate.sh build/goal/judge/report.base.json
  configure ok / ninja + build.sha1 ok / hashes vs config.yml ok / report ok
  per-function diff  matched 9932 -> 9932  linked 4895 -> 4895  (+0 at 100%, 0 newly linked)
  module wiring ok / dol_read ok / docs claims ok / gs offsets ok / raw offsets ok
  decl order ok / files.cmake ok / module order ok / port probe ok / port link gap ok
  GATE PASS  97d3797+1 changed
sha1sum build/G2ME01/main.dol        6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
python3 tools/check_symbol_names.py  503 units, 0 declared names missing
python3 tools/check_decl_order.py --unit MetroidPrime/Player/CGameState   ok
python3 tools/check_docs_claims.py   docs claims agree with the tree
```

No `NEW:` lines: everything below is either a measured wall (the brief says spellings and scores
go in the notes) or a lesson about the toolchain (a lesson does not raise a count).

## The item's figures are stale, as usual - re-measured

`item.json` repeated attempt 1's `NEW:` line: "21 functions still have no body in 9 runs
(2848 bytes)". At this head it is **11 bodiless functions**, and the sub-100% set is 11 more.
`total_functions` 116, `matched_functions` 94, `fuzzy 83.25729`, `matched_code 10060/18236`.

Bodiless (objdiff `fuzzy_match_percent: null`), with retail size:

| address | name | bytes |
| --- | --- | --- |
| 0x80142288 | `fn_80142288` | 76 |
| 0x801422D4 | `fn_801422D4` | 108 |
| 0x80142760 | `fn_80142760` | 124 |
| 0x80142944 | `fn_80142944` | 104 |
| 0x801435D4 | `LoadGameFileState__10CGameStateFPCv` | 488 |
| 0x80143B94 | `__dt__11CGMFrontEndFv` | 180 |
| 0x80143C40 | `__ct__11CGMFrontEndFRC11CGMFrontEnd` | 140 |
| 0x80143CD4 | `fn_80143CD4` | 80 |
| 0x80144818 | `fn_80144818` | 200 |
| 0x80146338 | `fn_80146338` | 440 |
| 0x801465EC | `fn_801465EC` | 264 (item says skip; it is a measured regswap wall) |

Sub-100% with a body: `PutTo__10CGameState` 91.90, `StartGameFromFrontEnd__Fv` 59.35,
`__ct__10CGameStateFR16CBitStreamReader` 84.14, `PutTo__11CWorldStateC` 97.30,
`__ct__11CWorldStateF(reader)` 99.30, `PutTo__18CPersistentOptionsC` 94.35,
`__ct__18CPersistentOptionsF(reader)` 95.52, `AddVariable` 87.78, `fn_801466F4` 66.63,
`fn_8014680C` 92.69, `__sinit_CGameState_cpp` 63.35.

## What landed: `kInvalidAreaId` is a load, retail materialises the value

`src/MetroidPrime/Player/CGameState.cpp:455-461`, `CWorldState::CWorldState(CBitStreamReader&,
CAssetId, const CWorldSaveGameInfo&)`.

`include/MetroidPrime/TGameTypes.hpp:15` declares `extern const TAreaId kInvalidAreaId`, so naming
it is a **load**; retail's constructor materialises the constant instead - `li r7,-1` in the
prologue (0x801450CC), stored to `+0x04` at 0x801450F0, and zeroed to the three `rc_ptr` slots
around it in the order 0/4/8. The old body produced `lwz r5,0(r0)` for the area id and pushed the
`-1` into the wrong member, which also cost the store order. `mAreaId(TAreaId(-1))` reproduces all
of it: 97.38028% -> 99.29578%, and nothing else in the unit moved (`matched_code` and
`total_code` both unchanged, so no other function changed size).

**Generalisable: an `extern const` in a header is a memory read in every use.** `kInvalidAssetId`
is a `#define` (include/Kyoto/SObjectTag.hpp:8) and so is fine; `kInvalidAreaId` is not. Worth
sweeping the headers for other `extern const` sentinels before blaming a body for a stray `lwz`.

## The wall that blocks both `CWorldState` functions: this mwcceppc never emits a dead `mr r5`

This is the single most useful thing measured here, and it is a **toolchain** fact, not a spelling.

`CWorldState::PutTo` (97.30%) is **one instruction** from 100%: retail has
`mr r5,r31` at 0x80145044 before `bl CWorldLayerState::PutTo`, and
`CWorldLayerState::PutTo` takes **one** argument (`PutTo__16CWorldLayerStateCFR16CBitStreamWriter`,
0x801721FC, and its own body never reads r5). The same dead move appears in
`__ct__11CWorldStateF(reader)`, where retail has `mr r5,r31` at 0x8014523C before
`bl __ct__16CWorldLayerStateFR16CBitStreamReader` - also a one-argument constructor. Both
functions' residues are exactly that, nothing else.

I probed whether any source shape makes the compiler produce it. Standalone probe compiled with
the unit's exact ninja flags:

```c
extern "C" void g1(void* p);          // one argument
extern "C" void g2(void* a, void* b, void* c);
extern "C" void p4(void* r3, void* r4, void* r5) {
  g2(r4, r5, r3);   // r5 is an argument here
  g1(r4);           // and r5 is still live afterwards
  g1(r5);
}
```

mwcceppc 2.7 emits `mr r3,r30` and `bl` - **no `mr r5`** - for the middle call, and the same with
the value live across it. Three more probes (the callee as a class constructor, a two-argument
callee, a one-argument call with nothing after it) all behave identically.

So: **this compiler only materialises the argument registers the callee's prototype needs.**
Retail's compiler emitted a redundant third-argument move, which means retail's
`mwcceppc` build differs from `MetroidPrimePort/build/compilers/GC/2.7/mwcceppc.exe`, or retail's
`CWorldLayerState` declarations had an extra (defaulted) parameter that its symbol table does not
record. Either way it is not reachable from source, and both functions are parked until someone
resolves it. Re-measure before spending a lane: if a different `mw_version` is ever tried on this
unit, these two are the canary.

## `fn_80143CD4` (0x80143CD4, 80 B) - 14 spellings, 6 of 20 instructions left

Identified: it is `rstl::reserved_vector< CGMFrontEnd::SPlayerConfig, 4 >::operator=`, the only
thing `CGMFrontEnd`'s copy constructor calls (`addi r3,r31,32` / `addi r4,r7+32` at 0x80143C80 /
0x80143C88). The member is at `CGMFrontEnd+0x20`: `mCount` at `+0x00`, four 8-byte
`SPlayerConfig`s at `+0x04`, `0x20 + 0x24 == 0x44 == sizeof(CGMFrontEnd)`.

What retail does, all of it reproduced by the best spelling (below): the count is stored **first**
from a read of the *source* (0x80143CD4 `lwz r0,0(r4)`, 0x80143CE0 `stw r0,0(r3)`), the loop
counter is re-read out of the *destination* (0x80143CE4), there is **no `this != &other` test**,
the loop is `mtctr` / `cmpwi r0,0` / `beqlr` / body / `bdnz`, and each element is copied member by
member - the 4-byte player selection, then the two 1-byte flags, leaving the two padding bytes
untouched - behind the library's own null precondition on the destination (`cmplwi r5,0` / `beq`,
0x80143CF4). `rstl::uninitialized_copy_n`'s `remaining` counter is what produces the `mtctr`/`bdnz`
pair; the repo's copy has no null precondition, which is why the test is written by hand here.

Best spelling, 80 bytes against retail's 80:

```c
extern "C" void fn_80143CD4(SFrontEndPlayerConfigs* self, const SFrontEndPlayerConfigs* src) {
  self->mCount = src->mCount;
  const CGMFrontEnd::SPlayerConfig* from = src->mItems;
  CGMFrontEnd::SPlayerConfig* to = self->mItems;
  for (int remaining = self->mCount; remaining != 0; --remaining, ++from, ++to) {
    if (to != nullptr) {
      to->mPlayerSelection = from->mPlayerSelection;
      to->mRumbleEnabled = from->mRumbleEnabled;
      to->x5_ = from->x5_;
    }
  }
}
```

The six that differ are one register apart and all follow from one fact: **retail's source pointer
is in r6 where every one of our 14 spellings puts it in r4**, and retail uses r4 as the byte
scratch where we use r3. Retail's loop needs four registers (r4 scratch, r5 dest, r6 src, r7 a
one-shot for the first load) and ours needs three. Tried, all 80/80 bytes and 6 differing:
declaration order of `from`/`to` both ways; `const`-qualified pointers; `if (to)`; `if (&to[0])`;
`if (to && from)`; a `for` with the increments in the header, a `while`, and a `while` with
`++from, ++to, --remaining` in that order; three locals for the three fields; the two 1-byte
fields assigned before the 4-byte one; `src` and `self` reached through named locals; an
index-based loop (84 bytes, worse); `rstl::construct(&to[0], from[0])`; `remaining > 0` instead of
`!= 0` (gives `blelr` instead of `beqlr`, so `!= 0` is right); `unsigned` instead of `int`
(`cmplwi` instead of `cmpwi`, so `int` is right). Left **unwritten**: a body at 70% that buys
nothing is worse than no body in a diff.

## `fn_80142288` / `fn_801422D4` (0x80142288 + 0x801422D4, 184 B) - the `erase` pair, 20+ spellings

Identified: the out-of-line `erase(iterator)` / `erase(iterator, iterator)` of
`rstl::vector< rstl::pair< CAssetId, TEditorId > >`, called by
`CPersistentOptions::SetCinematicState` when the state is being cleared (0x80142200-0x80142210:
`r0 = 16(r1)`, `r3 = r1+12`, `r4 = this+0x18`, `r5 = r1+8`, `stw r0,8(r1)`). `fn_801422D4` is
`destroy(first, last)` plus the tail shift: it computes `(elem - data) / 8` with
`subf` / `srawi r0,r0,3` / `addze`, runs `while (src != data + count*8)` copying 8 bytes at a
time, writes the new count (unchanged - the shift and the count cancel, which is why it looks
wrong at first read), and stores the element pointer through the first argument.

`fn_80142288` is the one-argument overload: it builds `[elem+8, elem+8, elem]` in a three-word
frame local and passes `&range[2]` and `&range[1]`, the array trick `fn_801466F4` already
documents. Our best is 72 bytes against retail's 76 - **the whole `mRtl` frame is missing**:

- retail has `stw r31,28(r1)` / `mr r31,r3` / `lwz r31,28(r1)`, a *dead* save of the first
  argument. Reproducing it requires the function to return a **class type** (so mwcceppc uses an
  sret pointer and parks it in r31) **with exactly two real parameters**, because with three the
  sret shifts them to r4/r5/r6 and retail has `elem` in r5. Spelled as
  `TIter fn_80142288(SGameStateBlock* self, const TIter* elem)` returning
  `fn_801422D4(self, &range[2], &range[1])`, the frame and the parameter registers come out right.
- 15 of the 18 instructions still differ, for two reasons: the register assignment is two off
  (our `next` lands in r0 where retail has r7), and **retail reads `*elem` twice** (0x80142290
  `lwz r6,0(r5)` for the `+ 8`, 0x8014229C `lwz r0,0(r5)` for `range[2]`) where mwcceppc 2.7
  common-subexpresses the two reads into one. Both reads precede every store, so it is not an
  aliasing invalidation; the compiler simply re-materialises instead of keeping a copy.

Tried: `void*` / `void**` / `void*&` / `void* const*` first parameter; `void*` vs `void* const`
vs a 4-byte struct pointer; a local alias of the out parameter; the three-word array built with
one local, with `range[1] = range[0]`, with the three stores in the order 0/1/2, 0/2/1 and 2/0/1;
`elem[0]` and `elem->mItem` spellings; a 3- and a 4-element array; `const` array; the return type
`void` / `void*` / a 4-byte class / `rstl::pointer_iterator<...>`; `return f(...)` and a bare
`f(...)`. Best 72/76. Left **unwritten**: see above.

## Also established, so nobody re-derives it

- **`CGameStateEnvVarManager`'s map is not what blocks `fn_80145B90`/`fn_8014601C`.** The earlier
  notes blamed the epilogue reload order; this run's probe result is the same class of problem -
  a compiler artifact, not a spelling - so those two stay parked with the epilogue wall, as the
  item says.
- **`fn_80142760` (the 36-byte element's copy) is still correctly refused.** Nine words with three
  refcount increments interleaved after the 4th, 6th and 9th store, over a 36-byte element no
  header types. Hand-writing it from raw offsets is exactly the plausible stand-in the brief
  forbids, and it gates `fn_801466F4`/`fn_8014680C`. The item's own note stands.
- **`fn_801465EC` skipped as instructed** (measured 93.79% regswap wall in an earlier run).
- **`__ct__11CGMFrontEndFRC11CGMFrontEnd` (140 B) is the cheapest thing left in principle** - a
  member-wise copy with two vtable stores (0x803B0D68 then 0x803B8470, both relocation fields, so
  objdiff ignores their values) and one `bl` to `fn_80143CD4`. It needs `mPlayers` assigned through
  a named out-of-line `operator=`, because `rstl::reserved_vector::operator=` is `inline` in
  include/rstl/reserved_vector.hpp:103 and would otherwise be inlined. Changing that header is a
  shared-header change with a large blast radius and was not attempted.

## For the next lane

1. Two of the eleven sub-100% bodies are now within one instruction, and both are blocked by the
   same *toolchain* fact (the dead `mr r5` before a one-argument call, proved above). Do not
   re-spell them; re-measure only if a different `mw_version` is tried on this unit.
2. The cheapest remaining real work in this unit is `__ct__11CGMFrontEndFRC11CGMFrontEnd` (140 B,
   one `bl` away from `fn_80143CD4`), and it needs a decision about
   `include/rstl/reserved_vector.hpp` that belongs on the orchestrator, not in a lane.
3. `fn_80143CD4` and `fn_80142288`/`fn_801422D4` are 6 and 15 instructions away respectively;
   both residues are register assignment, and both were measured over 14 and 20+ spellings. The
   two blockers above (a register the allocator will not give, and a common-subexpression the
   compiler will not keep twice) are not source-shape problems in anything I tried.

Files touched: `src/MetroidPrime/Player/CGameState.cpp` only, +4/-1
(`CWorldState::CWorldState(CBitStreamReader&, CAssetId, const CWorldSaveGameInfo&)`'s member
initializer plus a comment citing the retail instruction). Nothing else in the tree changed; the
scratch harness lived in the gitignored `.tmp/opencode` and has been deleted.

## Re-run on `b7e66c0` (2026-09-30): one exact range-erase body landed

The old figures above were the prior lane's state. Re-measured this worktree before editing:
`main/MetroidPrime/Player/CGameState` was **98 / 116** (`fuzzy_match_percent` 85.961395,
`matched_code` 10564 / 18236); global matched was **9989 / 28465**. The four functions from the
earlier 94/116 measurement are already in the current branch, so I did not redo that work.

Added `fn_801422D4` to `src/MetroidPrime/Player/CGameState.cpp`. It directly copies the tail range
of `rstl::vector<rstl::pair<CAssetId,TEditorId>>`, updates `mCount`, and returns the first
iterator. The two separate locals `start` and `newCount` are necessary: they produce retail's
`addze r6` / destination calculation / `mr r9,r6` register sequence. `./tools/lanediff.sh
MetroidPrime/Player/CGameState fn_801422D4` is empty, and the function disappeared from the
unmatched list. The already-exact `fn_80142288` source remains the one-argument template call; its
compiler-generated two-argument instantiation is still mangled, while this body is also emitted
under retail's otherwise-unmapped name for objdiff pairing.

Also wrote the related `fn_80143CD4` copy helper and `CGMFrontEnd` copy constructor, without a
shared-header edit. They now score **98.00% / 88.86%** respectively but are not exact. The helper's
only residue is the source pointer in r4 rather than retail's r6. The constructor still has the
reserved-vector default-count store and different register assignment; no count credit is claimed
for either body. These changes are in the same previously identified CGMFrontEnd cluster.

Afterward, `build/report.json` measures the target at **99 / 116**, fuzzy 87.66572, matched code
10672 / 18236, and global matched at **9990 / 28465**. Verification:

```text
./tools/goal_check.sh build/goal/item.json
  PASS progress-cgamestate-bodiless-remaining
  target rose: 98 -> 99 / 116
  matched 9989 -> 9990; linked 4896 -> 4896; no regressions; no asm added
sha1sum build/G2ME01/main.dol
  6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
git diff --check
  clean
```

The judge's `MP_GATE_DOCS_WRITE=1` regenerated the derived HANDOFF counts for its gate; I restored
that generated docs edit per the item prompt. A bare gate without the judge's docs-write mode
therefore reports only the expected stale HANDOFF counts (`matched 9990`, DOL 8579). No `NEW:`
line: the remaining frontend/register issues are already characterized above. Nothing committed.

## Final safety correction (same run)

The 98% `fn_80143CD4` score in the section immediately above came from reinterpreting the private
`mPlayers` member as an unrelated count-plus-array struct. I removed that type-punning after
checking the review criteria. The current helper uses the actual
`rstl::reserved_vector<CGMFrontEnd::SPlayerConfig, 4>` type and its public `resize`, `size`, and
indexing operations; it is semantically a copy, but now scores 0.00%. The copy constructor remains
88.86%. Do not restore the overlay merely to recover fuzzy score.

After that safety edit, re-ran `./tools/goal_check.sh build/goal/item.json`: **PASS**, target
**98 -> 99 / 116**, global **9989 -> 9990**, linked unchanged, no regressions, no asm. The exact
`fn_801422D4` body and its empty `lanediff` are unchanged; final target figures are fuzzy 87.235794
and matched code 10672 / 18236. The judge regenerated `docs/HANDOFF.md` during its docs-write gate;
I restored it again so only the requested source edit remains in L9.
