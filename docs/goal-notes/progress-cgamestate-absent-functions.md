# progress-cgamestate-absent-functions

Lane 2, `wt-mp2-goal-L2`, head `fc61ccb`. **The tree is back to clean**: `./tools/goal_check.sh
build/goal/item.json` prints `goal_check: FAIL progress-cgamestate-absent-functions - progress item
changed nothing under src/ or include/`, and that is the correct verdict for a reverted tree.
Nothing reached 100%, so the unit's matched count did not move (103 / 116 before and after,
10070 project-wide). No commit.

What follows is the disassembly-derived structure of all four absent functions, the spellings
measured, and the two walls. The next run should not re-disassemble anything.

## Correction to the item's premise: there are **four** absent functions, not three

`build/report.json` lists 0.00% for `fn_801465EC` (0x801465EC, 264 B), `fn_80146338` (0x80146338,
440 B), `LoadGameFileState__10CGameStateFPCv` (0x801435F4, 488 B) **and** `__dt__11CGMFrontEndFv`
(0x80143B94, 180 B). The last is not in the `reason`; it is the fourth and, as measured below, the
one with the clearest path to 100% - the only obstacle is that the obvious fix makes the object
*worse* in a different way.

All four are genuinely absent: `powerpc-eabi-nm build/G2ME01/src/MetroidPrime/Player/CGameState.o`
has `U fn_801465EC` and no definition of the other three. **Do not read
`build/G2ME01/obj/MetroidPrime/Player/CGameState.o`** - it is a stale Sep-28 pre-sync object that
*does* define all four, and it will tell you the wrong thing.

Also worth knowing before starting: `src/MetroidPrime/Player/CGameStateBlockReserve.cpp` is a
**ready-made body for `fn_801465EC` already in the tree**, listed in `files.cmake:871` but **not in
`configure.py`**, so it is never compiled. It is not a duplicate-definition hazard for anything
written in `CGameState.cpp`. Its byte-copy loop is the 93.79% spelling (below).

## A general measurement: **`bl` targets in the DOL cannot be resolved**

`.tmp/opencode/ret2.py` (see "Tools" at the end) decodes a function out of the DOL and resolves its
`bl` displacements against `config/G2ME01/symbols.txt`. The targets it prints for an external call
are **linker placeholders and land outside every section in `tools/bytescmp.py:SECTIONS`**: for the
100%-matched `reserve__Q24rstl63vector<CHintOptions::SHintState, rmemory_allocator>::reserve(int)`
at 0x801464F0, whose `bl` at 0x80146580 computes to **0x80765DA4** - 0x80765DA4 is not in the DOL
(`.text` is 0x80003840..0x803A5494). So this is not a bug in the decoder and not specific to the
absent functions. Retail's real answers are in retail's relocations, and
`tools/dump_fn_relocs.sh`'s own header says those are gone once a unit claims the range. **Callee
identity for a claimed function has to come from the source tree, not from the DOL.**

## `fn_801465EC` (0x801465EC, 0x108 = 264 B) - `rstl::vector<unsigned char>::reserve`

    stwu r1,-48(r1) / mflr r0 / stw r0,52(r1) / stw r31,44 / stw r30,40 / mr r30,r4
    stw r29,36 / mr r29,r3
    lwz r0,8(r3) ; cmpw r30,r0 ; ble return          <- signed: size <= cap skips the grow
    mr r3,r30 ; bl <allocate__Q24rstl17rmemory_allocatorFi>
    lwz r5,12(r29) ; mr r31,r3 ; lwz r0,4(r29) ; mr r4,r31
    stw r5,16(r1) ; add r0,r5,r0 ; cmplw r5,r0 ; stw r0,8(r1) ; subf r3,r5,r0
    stw r0,12(r1) ; stw r5,20(r1) ; beq <0xdc>
    srwi. r0,r3,3 ; mtctr r0 ; beq <0xc4>           <- 8-byte chunk loop, 0x70..0xb8
    andi. r3,r3,7 ; beq <0xdc> ; mtctr r3            <- byte tail, 0xc8..0xd8
    lwz r3,12(r29) ; bl <CMemory::Free>
    stw r31,12(r29) ; stw r30,8(r29) ; epilogue

`x04_count` is a **byte** size in this instance (no `* 36`, unlike the 36-byte block's
`fn_801466F4` at 0x801466F4). The copy is a pointer-range byte loop that mwcceppc's loop-idiom
transform turns into the `srwi. r0,r3,3` / `mtctr` / 8x `lbz`-`stb` / `bdnz` chunk loop plus the
`andi. r3,r3,7` byte tail - **not** a `memcpy` call, and not the `uninitialized_copy_n` that makes
`fn_80004AA0` 100% (that one takes a count, not a range; see `CGameStateBlockCopyCtor.cpp`'s
header).

### Spellings measured (all compiled through `.tmp/opencode/probe_gs.sh`, 0.3 s each)

The compile is byte-identical to ninja's: `score.py` on the ninja object and on the probe object
both report `248B vs retail 264B  62 vs 66 instrs` for the same source. objdiff % is from
`./tools/fast_try.sh`.

| spelling | size / instrs | objdiff |
|---|---|---|
| `uchar* to; const uchar* from; const uchar* end = from + count; for (; from != end; ++from, ++to) *to = *from;` | 248 B / 62 | **93.79%** |
| same, `while (from != end) { *to++ = *from++; }` | 248 B / 62 | - |
| same, `uint n = end - from; while (n) { *to++ = *from++; --n; }` | 248 B / 62 | - |
| same, bounded on `to` (`to != to + count`) | 248 B / 62 | - |
| `rstl::uninitialized_copy(src, src + count, buffer)` (3-arg range) | 248 B / 62 | - |
| `rstl::uninitialized_copy_n(src, (int)count, buffer)` | 236 B / 59 | 86.86% |
| `for (uint i = 0; i != count; ++i, ++to) *to = from[i];` (index) | 152 B / 38 | - |
| `memcpy(buffer, self->x0c_data, count)` -> `__memcpy` | 112 B / 28 | 41.21% |
| `(memcpy)(buffer, self->x0c_data, count)` | 112 B / 28 | - |
| `CMemory::Copy(...)` | compile error | - |
| `while (n) { c = min(8,n); for (i<c) to[i]=from[i]; ... }` (nested) | 324 B / 81 | - |
| `void* range[4]` + a local `static` byte-copy helper taking `(&range[3], &range[1], buffer)` | 144 B / 36 | - |

`memcpy` cannot be made to inline here: `include/Kyoto/MemoryCopy.hpp:9` does
`#define memcpy(dest, src, size) __memcpy(...)` under `__MWERKS__`, and `(memcpy)(...)` reaches the
library function, so both are a `bl`. **Retail's inline is not a `memcpy` call at all** - it is the
loop-idiom transform, which the first row already reproduces instruction for instruction from 0x54
to the end.

### The wall: four dead stores, and they are the whole difference

Retail's 0x44/0x50/0x58/0x5C are `stw r5,16(r1)`, `stw r0,8(r1)`, `stw r0,12(r1)`, `stw r5,20(r1)`
with `r5` = source and `r0` = source+count. **They are dead**: nothing after 0x5C reloads 8/12/16/20
(`r1)`, and the loop runs off `r5`/`r4`. They are also why retail's frame is 48 bytes
(`stwu r1,-48(r1)`, LR at 52) against our 32 - the four outgoing-argument slots are all that is
extra, and the other 62 instructions match.

Their values are `(end, end, src, src)` in slots `(8, 12, 16, 20)` emitted in the order
16, 8, 12, 20. That is the signature of a four-argument call whose body was inlined and whose
argument-setup stores the cleanup pass then failed to remove, so **the source is a four-argument
helper, not a bare loop**. The two four-argument shapes available in `rstl/` are a range pair twice
over, and neither is a member of `rstl`: `uninitialized_copy` is `(It, It, T)` and
`uninitialized_copy_n` is `(S, int, D)`. The one four-argument helper I wrote (the `range[4]` row
above, the `fn_801466F4`/`fn_8014680C` shape) was **outlined**, not inlined, so it produced a `bl`
instead - `-inline auto` with `inline_max_size(125)` did not take it.

WALL: fn_801465EC 93.79% - 4 dead outgoing-arg stores of (end,end,src,src) at 8/12/16/20(r1) that
a 4-argument inlined helper's arg setup would leave; no `rstl/` 4-arg helper, and a local
4-arg one gets outlined not inlined.

## `__dt__11CGMFrontEndFv` (0x80143B94, 0xB4 = 180 B) - the one with a clear path, blocked by the
object, not by the source

Retail's shape, in full: `mr. r31,r3 ; beq <0x9c>` (this to r31, **the flag left in r4**), the
derived vtable store, `addic. r0,r31,32 / beq` + the empty-body 8-wide-unrolled loop, then
`cmplwi r31,0 / beq`, the **base** vtable store, `extsh. r0,r4 / ble <0x9c> / mr r3,r31 /
bl <__dt__9CGameModeFv>`, and the epilogue. **One call.**

### The finding: retail's is the D0 that forwards the flag, and the base D0 does the delete

`__dt__9CGameModeFv` is at 0x80004798, 0x48 bytes, and is the same family: `mr. r31,r3 / beq /
lis / extsh. r0,r4 / addi / stw 0(r31) / ble / bl <operator delete>`. So the chain is
`CGMFrontEnd` D0 -> `CGameMode` D0(flag) -> delete, and **`CGMFrontEnd`'s own 180 bytes free
nothing**. mwcceppc's generated D0 for the same class does the opposite: it calls the base with
`li r4,0` and then `CMemory::Free(this)` itself. Two calls, 46 instructions against retail's 45, and
the loop sits 8 bytes later. That is a **different call sequence, not register allocation** - no
`extern "C"` spelling can paper over it, because the only source that emits this member-destroy
loop is the compiler's own.

`CGMFrontEnd::~CGMFrontEnd() {}` written out-of-line in `CGameState.cpp` between
`StartGameFromFrontEnd` (0x80143884) and the copy ctor (0x80143C48) is **83.00%** and lifts the unit
from 87.93% to 90.11% fuzzy. MWERKS 2.7 rejects `= default` outright ("declaration syntax error").

### Why it was reverted anyway - the object gets worse in a different way

Defining the destructor makes mwcceppc emit **an extra function and an extra section**:

* `__dt__Q24rstl49reserved_vector<Q211CGMFrontEnd13SPlayerConfig,4>Fv` (weak) - a function retail's
  unit does not define, so `tools/unit_fit.sh` fails;
* `__vt__11CGMFrontEnd` moves from `U` to `D __vt__11CGMFrontEnd` in **`.data`**, and this unit
  claims no `.data` at all (`config/G2ME01/splits.txt:672`: `.text .ctors .rodata .bss .sdata
  .sdata2`). The `CGameStateBlockDtor.cpp` header records the same trap for `files.cmake`.

So the fix is not "write the destructor", it is "write the destructor **and** find the spelling
whose D0 forwards the flag instead of freeing" - and I did not find that spelling.

### Measurements of the hand-written alternatives (all worse, for the record)

`extern "C" void __dt__11CGMFrontEndFv(CGMFrontEnd* self, int flag)` with hand-written vptr stores
(`*reinterpret_cast<u32*>(self) = reinterpret_cast<u32>(__vt__11CGMFrontEnd);`, with
`extern "C" char __vt__11CGMFrontEnd[];`) and a hand-written loop:

* the flag/`this` allocation came out right (`mr. r31,r3` retained, `extsh. r0,r4` in place) but
  the **loop was removed by mwcc** - the body is empty, so `for (int i = 0; i != count; ++i)
  players[i].~SPlayerConfig();` collapsed to `lwz r0,32(r3) ; mtctr r0 ; cmpwi r0,0 ; beq ;
  bdnz` - **3.53%**;
* a pointer-bounded `for (; p != p + count; ++p) p->~SPlayerConfig();` gave 92 B / 23 instrs -
  **10.56%**, also collapsed;
* `mPlayers` is **private**, so an `extern "C"` body can only reach it through raw offsets.
  `rstl::reserved_vector`'s **`int mCount` is the first word**
  (`include/rstl/reserved_vector.hpp:25`), so `CGMFrontEnd+0x20` is the count and `+0x24` the data -
  which is why retail's `addic. r0,r31,32` null-test is on the count's own address and is never
  taken.

`rstl::destroy_impl` is not the loop's source either: `include/rstl/construct.hpp:87` returns at
once for a trivially destructible value type, and the loop is the whole point.

## `fn_80146338` (0x80146338, 0x1B8 = 440 B) - the option map's rbtree insert

`stmw r26,8(r1)` frame, 32 bytes, r26-r31 all live. Signature from
`src/MetroidPrime/Player/CPersistentOptionsMapInsert.cpp:77`:
`fn_80146338(SMapInsert* out /*r3*/, SMap* tree /*r4*/, SMapNode* root /*r5*/,
const SMapEntry* entry /*r6*/)`. `CPersistentOptionsMap.hpp:24-37` has the tree layout. Six calls:
three to the node constructor (`fn_80008CE0`, 0x80008CE0, 136 B, by that file's header), two to a
string compare, one to the tree insert at 0x178 (`addi r3,r31,8`, `mr r4,r27`, count+1 in r5 and
r6). Shape: the `root == 0` arm builds a node, stores it at `tree+0x10`, `tree->x04_count++`, then
mirrors it to `tree+0x08` and `tree+0x0C`; otherwise a descent from 0x88 comparing the key, then a
rebalance that walks down one side replacing nulls with fresh nodes (0xd4/0x128 arms each test
`r28->x00` / `r28->x04` for null and fix up `tree+8` / `tree+12` when the replaced node was the
extreme), then `tree->x04_count++` and the `tree+8` insert. All three `stb r0,8(r30)` are *different*
`.sdata` bytes (`lbz -31135/-31134/-31133(r13)`) - three distinct `false` constants, not one.
Not attempted; the descents plus the rebalance are 110 instructions of exact register allocation.

## `LoadGameFileState__10CGameStateFPCv` (0x801435F4, 0x1E8 = 488 B)

Declared at `include/MetroidPrime/Player/CGameState.hpp:116`, returning
`GameFileStateInfo` by value through `r29`; called from
`src/MetroidPrime/CMemoryCardDriver.cpp:443,456`. Structure: a **1712-byte frame**, `li r5,4096` +
`bl` (a `CMemoryInStream` at 28(r1)), `CBitStreamReader` at 16(r1), then **seven** `bl`s of the
same reader with immediates `32, 32, 1, 1, 32, 32` - the two `1`s each followed by
`neg r0,r3 / or r0,r0,r3 / srwi r0,r0,31 / stb r0,80(r1)` and `81(r1)`, i.e. two booleans read as
one **bit**; the last two 32-bit reads land in r31/r3 and are stored to `8(r1)`/`12(r1)`, with
`li r0,0` stores to 8 and 12 first. Then `addi r3,r1,88` for the rest: `lfd f0,8(r1)` and
`stfd f0,48(r1)`, three `bl`s (`li r4,42` on the second) writing `60(r1)`, `64(r1)`, `68(r1)`,
`72(r1)`, a `lwz r0,1492(r1)` / `cmpwi r0,0` that branches over a `lfs f1,-25096(r2)`, and the
asset-id path `lis r3,17200 / xoris r0,r0,32768 / stw / xoris r4,r4,32768 / lfd f2,-25088(r2) /
stw / lfs f3,-25092(r2) / fsubs / fdivs / fmuls`. Then the `GameFileStateInfo` is written through
`r29` (`0(r29)`, `8(r29)`, `16(r29)`, `24(r29)`, `32(r29)`), a `bl` with `li r4,-1`, and two
trailing calls - one of them building `0x8000_0000 + 0x0D5C` out of `lis r4,-32709` +
`addi r0,r4,3420` and storing it to 28(r1). 122 instructions and 14 call sites with unresolvable
targets; not attempted.

## Tools left in `.tmp/opencode/` (gitignored)

* `ret.py <vaddr> <size>` - disassemble a retail function out of the DOL
  (`tools/bytescmp.py`'s reader, so retail even inside a claimed range).
* `ret2.py <vaddr> <size> [--raw]` - the same, plus `b`/`bl` targets resolved against
  `config/G2ME01/symbols.txt`, and the raw instruction word. The target resolution is **wrong for
  external calls**, as measured above; it is still the right tool for intra-function branches.
* `one.py <variants> <i> <unit> <sym> <addr> <size> [lo] [hi] [--diff]` - splice one `%%%`-separated
  variant into the tree, run `fast_try.sh` and the disassembly, then **restore the tree in a
  `finally`**. Safer than L7's `sweep2.py` for anything that ends in a build you keep.
* `score.py`, `sweep2.py`, `probe_gs.sh`, `raw.py`, `patch.py` - carried over from lane 7,
  re-verified here: `probe_gs.sh`'s object is byte-identical to ninja's for the same source.

## Queue

No `NEW:` lines. The three functions in the `reason` are this item, and the fourth
(`__dt__11CGMFrontEndFv`) is a member of the same unit and the same item - filing either would be a
restatement of the item, which the brief forbids.

---

# Run on lane 9 (`wt-mp2-goal-L9`, head 782065a9) - `fn_80146338` matched

STALE: `fn_801465EC` (`reserve__Q24rstl37vector<Uc,...>Fi`, 264 B) and `CFrontEndGameMode::~CFrontEndGameMode`
(`__dt__17CFrontEndGameModeFv`, 180 B, the notes' `__dt__11CGMFrontEndFv`) are both 100.0 in this tree's
`build/report.json` (measured). The earlier walls on them were overtaken upstream; do not retry them.
Note the report uses the retail names above, not `fn_801465EC`/`CGMFrontEnd`.

## Done: `fn_80146338` 0.00% -> 100.00% (unit 105 -> 106 / 116, project 13038 -> 13039)

`goal_check.sh` PASS. Source: `src/MetroidPrime/Player/CGameState.cpp`, between `fn_801465A8` and
`CEnvironmentVariable::GetBitCount` (descending order; `check_decl_order.py` ok). Uses the local
`SMap/SMapNode/SMapEntry/SMapInsert` from `CPersistentOptionsMap.hpp` plus `rstl::rbtree_rebalance`.

Spelling: root==0 arm builds the node, then `++count`, `x08 = x0c = root`, result `{root, &x08}`; else a
`while (created == nullptr)` loop with `less = fn_800273B4(&tree->x01_compare_this, entry->key, cur->x10_key)`,
`if (!less && !fn_800273B4(..., cur->x10_key, entry->key))` -> found, then left/right arms creating the node with
colour 1 and updating `x08_header`/`x0c_rightmost` when `cur` was the extreme.

Measured:
* First draft with literal `true/false/true` results: 93.82%. The only diff was `li r0,1` in place of
  retail's `lbz r0,-31135/-31134/-31133(r13)`.
* **Three file-scope non-const statics** `static bool sInsertedFirst = true; sFound = false; sInserted = true;`
  (in that order) give 100% - retail's three `.sdata` bytes. This is the whole trick.
* Port gate: the call to `fn_80008CE0` and `fn_800273B4` grew the port's undefined count 287 -> 289 (gate.sh
  `link-gap` FAIL). The port keeps the option map in a host `rstl::map` (`PortCPersistentOptionsMap.cpp`) and
  never calls `fn_80146338`, so the block is under `#ifndef TARGET_PC`. gate then passes.

## Still absent in this unit (measured 0%): `LoadGameFileState__10CGameStateFPCv` (488 B), `push_back` for
`vector<CWorldState>` (56 B), `erase` for `vector<pair<uint,TEditorId>>` (76 B). Not attempted this run; the
`LoadGameFileState` structure is in the earlier section above.

---

# Run on lane 5 (`wt-mp2-goal-L5`, head 47a515d9) - unit 108 -> 110 / 116, project 13133 -> 13135

`goal_check.sh` **PASS**. Nothing committed. Both functions below are the item's own (its `reason`
names them), so no `NEW:` line is filed.

Overtaken upstream (not this item's verdict, which is PASS): `fn_801465EC` is now
`reserve__Q24rstl37vector<Uc,...>Fi` and the `__dt__11CGMFrontEndFv` wall is now
`__dt__17CFrontEndGameModeFv`; both are 100% on this tree (measured in `build/report.json`). Do not
retry either. Same correction as lane 9's note.

## A measurement that corrects this file's premise: a missing `fuzzy_match_percent` means
## **paired at 0%**, not "absent"

Earlier sections here (and lane 2's) read a function with no `fuzzy_match_percent` in
`build/report.json` as one our object does not define. It is not. `build/tools/objdiff-cli diff -p .
-u main/MetroidPrime/Player/CGameState -o f.json --format json` prints both symbol tables, and for
`push_back__...vector<11CWorldState...>` it showed `left: size 56, match_percent 0.0` paired with
`right: size 144, match_percent 0.0`. The report simply omits the key at 0.0, and
`tools/fast_try.sh` prints those as `0.00%`. So "0.00%" means *defined but wrong*, which is a
different and usually cheaper problem than "not defined". Run the `objdiff-cli diff` once before
concluding a function is absent.

## Landed: `reserve__Q24rstl48vector<11CWorldState,...>Fi` 71.37% -> **100%** (172 B)

`src/MetroidPrime/Player/CGameState.cpp:120-155`. The `extern "C" fn_801466F4` carve was
**byte-identical to retail** (measured with `tools/bytescmp.py`: 4 differing instructions of 43, all
four `bl` relocations) and still scored nothing, because retail's symbol at 0x801466F4 is the
mangled instantiation. The fix is the recipe in `docs/RUNNING_THE_DECOMP.md` (line ~959) for a
non-inline template member: a **non-inline explicit specialization**, spelled where reverse source
order puts it.

    template <>
    void rstl::vector< CWorldState >::reserve(int newSize) { ... }

mwcceppc 2.7 accepts it (`reserve` is declared out of line in `rstl/vector.hpp`) and emits it
**strong `T`** at the source position - `nm` puts it at 0x5e6c, between `fn_801465A8` (0x5e28) and
`fn_801467A0` (0x5f18), which is retail's order. It also removes the weak 180-byte generic
instantiation and its local `uninitialized_copy<pointer_iterator<CWorldState>,CWorldState*>` (104 B)
from the object.

**The one spelling detail that is load-bearing:** the body must re-read `mItems` at each use rather
than hoisting it into a local. With `uchar* const first = reinterpret_cast<uchar*>(mItems);` the
function is 176 bytes at 84.91%: the hoisted pointer takes a *callee-saved* register (`r30`) because
it is live across the `fn_8014680C` call, retail keeps `this` in `r29` / the parameter in `r30` and
uses volatile `r0`/`r6`, and ours needs a fifth register (`stw r28,32(r1)`). The generic `reserve`
in `rstl/vector.hpp` re-reads its members, which is why the carve was exact and the first
specialization was not.

## Landed: the 56-byte append at 0x801426E0, 0% -> **100%**, by renaming the retail symbol

The eighth upstream sync renamed 0x801426E0 in `config/G2ME01/symbols.txt` to
`push_back__Q24rstl48vector<11CWorldState,...>FRC11CWorldState`. Our object emitted that name
(weak, 144 bytes - the *growth-checking* `push_back` from `rstl/vector.hpp`, too big for
`inline_max_size(125)` so both callers `bl` it) and could not pair. The carve `fn_801426E0` in the
same file is byte-identical to retail's 56 bytes (`bytescmp.py`: 1 differing instruction of 14, the
`bl`) and paired with nothing.

One line in `config/G2ME01/symbols.txt` (reported as intended changes, per the brief):

    - push_back__Q24rstl48vector<11CWorldState,Q24rstl17rmemory_allocator>FRC11CWorldState = .text:0x801426E0; // type:function size:0x38
    + fn_801426E0 = .text:0x801426E0; // type:function size:0x38

This is the mechanism `docs/RUNNING_THE_DECOMP.md` line 176 documents. It is safe here: nothing in
`main.dol` or in any of the 86 `config/G2ME01/rels/*/symbols.txt` names the old symbol (`grep` over
`config/` returns only that one line), `main.dol` still hashes `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`,
and `python3 tools/check_symbol_names.py` reports 0 missing names.

### The in-source route, measured, and why it is not the one to take

The specialization **is** expressible - the earlier "mwcceppc cannot express one" wall is only half
true. All of this measured this run:

| spelling | result |
|---|---|
| `template<> void rstl::vector<CWorldState>::push_back(...)` with the member **in the class body** | `object '...push_back(...)' redefined` (reproduced) |
| declare the specialization early, **define it after the first use** | same `object redefined`, so a declaration alone does not count - the definition must precede the first use |
| same, with `push_back`'s body moved out of the class in `rstl/vector.hpp` (kept `inline`) | **compiles** |
| ... with `#pragma dont_inline on` / `reset` around it | compiles; **no effect** - still inlined |
| ... with `__declspec(noinline)` | `illegal type qualifier(s)` |
| ... with no caller at all (call sites rewritten to `fn_801426E0`) | **not emitted** - mwcceppc drops an unreferenced explicit specialization |

and the blocking fact: **the 56-byte body is always inlined.** Before the specialization this unit
paired `StateForWorld` at 100% because both callers emit a `bl`; with the specialization, mwcceppc
inlines 56 bytes into both (`StateForWorld` 100% -> 90.17%, and the stream constructor
84.14% -> 83.80%), and no standalone `push_back__...` symbol exists to pair at all. There is no
in-source spelling that emits it out of line, so renaming the retail symbol is the whole fix. The
`rstl/vector.hpp` edit was reverted; the header is untouched in the final diff.

## Measured after the change

* unit `main/MetroidPrime/Player/CGameState`: **108 -> 110 / 116** functions,
  fuzzy 93.03466% -> **93.611755%**, matched code 12816 -> 13044 B. `.rodata` / `.sdata` /
  `.sdata2` / `.ctors` percentages unchanged.
* project: 13133 -> **13135** matched functions, fuzzy 37.11898% -> 37.120586%, linked 6225
  (unchanged), 844/2169 units.
* `tools/goal_check.sh`: PASS, every line ok. `main.dol` sha1 unchanged, 86 REL hashes ok,
  `probe_sources.sh` 840 files 0 failed / link LINKED (286 undefined), `unit_fit.sh` unchanged at
  86 extras / 9364 B over, `check_decl_order.py` ok.
* `tools/gate.sh` rewrote the two derived counts in `docs/HANDOFF.md` (13133 -> 13135 and
  11488 -> 11490); that is the gate's own edit, not mine.

## Not attempted, and the one measurement worth keeping

Still unmatched in this unit (measured): `LoadGameFileState__10CGameStateFPCv` 488 B at **0%** (the
structure is in lane 2's section above; 14 call sites, a 1712-byte frame, too big for one item),
`StartGameFromFrontEnd__Fv` 784 B 60.11%, `__ct__10CGameStateFR16CBitStreamReader` 1668 B 84.14%,
`PutTo__18CPersistentOptionsCFR16CBitStreamWriter` 600 B 94.35%,
`__ct__18CPersistentOptionsFR16CBitStreamReader` 776 B 95.52%,
`PutTo__10CGameStateFR16CBitStreamWriter` 876 B 96.47%.

`PutTo__10CGameStateFR16CBitStreamWriter` is the only one close enough to be worth a look: its
side-by-side (`.tmp/opencode/sbs.py`, retail 0x801431DC) is **one register out** for the whole
body - retail uses r22/r24/r25 where ours uses r23/r25/r26 - with every other difference a `bl`
relocation objdiff ignores. That is the callee-saved allocation, i.e. one live value too many
somewhere in a 876-byte function, not a spelling difference. One spelling only, so this is **not** a
`WALL:` - it is a lead.

## Review rejected run 1 (2026-10-02 19:12:42Z, reviewer worker)

The judge passed this attempt; the reviewer rejected it:

`src/MetroidPrime/Player/CGameState.cpp:141-156` makes a guest-layout body the live `rstl::vector<CWorldState>::reserve` on the 64-bit PC port as well as in the matching build. It hardcodes 36-byte elements (`allocate(newSize * 36)`, `mCount * 36`, `fn_8014680C`'s 36-byte stride) while the host's `CWorldState` is 64 bytes (measured in the port's own object: `StateForWorld` strides `0x40`, `fn_80142760` writes to `+0x38`, the pre-change host `reserve` is `shl $0x6`/`add $0x40`); being a strong definition, it overrides the weak sizeof-based instantiation for every caller, and `CWorld.cpp`/`CMemoryCard.cpp` call `StateForWorld` (`PORT_NOTES.md:483`), so `mWorldStates.reserve()` overflows a 36·n buffer by 28 bytes per element. Acceptable change: keep the guest body that scores 100% but put it behind `#ifndef TARGET_PC` and give the host the sizeof-based body the header's template already has — the shape `src/MetroidPrime/CEntity.cpp:18` and `src/MetroidPrime/Player/CStaticInterference.cpp:13` use — so the matching gain survives.

Rejected diff: /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal/build/goal/review/progress-cgamestate-absent-functions-L5-1.patch
Review transcript: /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal/build/goal/agent/progress-cgamestate-absent-functions-L5-1-review1-20261002T190139.jsonl

## Fix round 1 (2026-10-02)

One correction, exactly the one the review asked for: the 36-byte guest body of
`rstl::vector< CWorldState >::reserve` now lives under `#ifndef TARGET_PC`, and the `#else` arm is a
second explicit specialization of the same member carrying `include/rstl/vector.hpp`'s own body
verbatim (`mAllocator.allocate(newData, newSize)` / `uninitialized_copy(begin(), end(), newData)` /
`destroy` / `mAllocator.deallocate`), i.e. the spelling `src/MetroidPrime/CEntity.cpp:17` and
`src/MetroidPrime/Player/CStaticInterference.cpp:12` already use. `src/MetroidPrime/Player/CGameState.cpp:151-196`;
the comment above it records why the two arms differ. Nothing else in the diff was touched.

Measured after the fix (not asserted, measured):

* **Host**: `build-port-link` object builds; `nm` shows `_ZN4rstl6vectorI11CWorldStateNS_17rmemory_allocatorEE7reserveEi`
  as `T` (strong) and `objdump` shows the sizeof-based body — `shl $0x6,%edi` for
  `newSize * sizeof(CWorldState)`, `add $0x40,%rax` in the copy loop, signed guard
  `cmp %esi,0x8(%rdi); jl`. So `mWorldStates.reserve()` allocates 64·n on the host again instead of
  36·n.
* **No new link gap**: the host body references `CWorldState::~CWorldState()`, which is already on
  the accepted list (`docs/research/port_link_gap_list.md:49`, `_ZN11CWorldStateD1Ev`) — it is the
  generic template's own reference, present whenever the header body is instantiated.
* **Guest unchanged**: `tools/decomp_build.sh` clean; unit `main/MetroidPrime/Player/CGameState`
  110 / 116, fuzzy **93.611755%** — identical to the pre-fix figure — with
  `reserve__Q24rstl48vector<11CWorldState,...>Fi` (172 B) at **100.0%**, `fn_801426E0` (56 B) and
  `fn_80146338` (440 B) also 100.0. Project 13135 matched functions, 37.120586% fuzzy, `All:` line
  `37.12% / 30.56% / 13.50% (13135 / 28465)` — unchanged.
* `tools/goal_check.sh build/goal/item.json` **PASS**, every line ok. `main.dol`
  `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, `check_symbol_names.py` 0 missing,
  `check_decl_order.py` ok, `check_raw_offsets.py` ok (176 sites / 76 files). No commit.
