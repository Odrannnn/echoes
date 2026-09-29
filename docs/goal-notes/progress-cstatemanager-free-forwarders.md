# progress-cstatemanager-free-forwarders

Target `MetroidPrime/CStateManager` (`main/MetroidPrime/CStateManager`, `NonMatching`).
**79 -> 90 of 239 matched functions, +11.** The unit stays `NonMatching`; `flip_test.sh` was
not run to decide anything.

## What landed

The nine bare one-`bl` forwarders named in the item's `reason`, plus the layer under each of
them and, in family 1, the `rstl::vector<float>` destructor that belongs to this unit. All
twelve are in `src/MetroidPrime/CStateManager.cpp`.

| retail | offset | bytes | score |
| --- | --- | --- | --- |
| `fn_80043180` | 0xCF80 | 32 | 100.00% |
| `fn_800431A0` | 0xCFA0 | 36 | 100.00% |
| `fn_800431C4` | 0xCFC4 | 112 | 100.00% |
| `fn_80043234` | 0xD034 | 132 | 100.00% |
| `__dt__Q24rstl36vector<f,Q24rstl17rmemory_allocator>Fv` | 0xD0B8 | 84 | 100.00% |
| `fn_800434CC` | 0xD2CC | 32 | 100.00% |
| `fn_800434EC` | 0xD2EC | 36 | 100.00% |
| `fn_80043510` | 0xD310 | 84 | 100.00% |
| `fn_80043688` | 0xD488 | 32 | 100.00% |
| `fn_800436A8` | 0xD4A8 | 36 | 100.00% |
| `fn_800436CC` | 0xD4CC | 80 | 100.00% |
| `fn_8004371C` | 0xD51C | 160 | **77.68%** (see below) |

`fn_8004371C` is 160 bytes and scores 77.68%, so it is **not** counted. The eleven counted are
the other eleven. `fn_800391B4` was already in the source and already 100%; it is not part of
the +11.

## The three spellings that decided the leaves

All three are load-bearing and none is guessable from the disassembly alone. Measured, one at a
time, with `tools/fast_try.sh MetroidPrime/CStateManager`:

1. **The destructor flag is a `short`, not an `int`.** MWCC's deleting-destructor tail is
   `extsh. r0,r31 ; ble`, which sign-extends the low half. An `int` parameter gives
   `cmpwi r31,0` and the function drops. Every leaf here takes `short flag`.
2. **The leaf returns a pointer, not `void`.** MWCC destructors end `mr r3,r30` and return
   `this`. Declared `void`, the trailing `mr r3,r30` is gone. This is what fixed
   `fn_80043234`: it was 96.97% (byte-identical but for that one instruction) and reached
   100.00% by changing the return type alone. Same for the three leaves.
3. **The forwarders pass `-1`**, which is MWCC's `kDestructorFlagNone` - the non-deleting
   value - and is what produces the caller's `li r4,-1`.

## `~vector()` is not inlined, and that is what writes 0x800432B8

The thing that made this whole family reachable: `<rstl/vector.hpp>` spells `~vector()` in the
header, and it looks like a call site written through the template would inline and emit
nothing for objdiff to pair - the same trap `fn_8003C0C4`/`fn_8003C054` document. It does not.
mwcceppc at `-inline deferred` still emits the out-of-line copy under the retail symbol's own
name, and the `bl` is a `R_PPC_REL24 __dt__Q24rstl36vector<f,Q24rstl17rmemory_allocator>Fv`.

Measured directly: a probe function whose only body is `v->~vector()` compiles to
`li r4,-1 ; bl __dt__Q24rstl42vector<6CToken,...>Fv` with a relocation, and the object gains a
`W __dt__Q24rstl42vector<6CToken,Q24rstl17rmemory_allocator>Fv` weak definition.

**No header change, no `asm`, no `__attribute__` is needed.** This is the cheapest spelling of
these three families in the repo, and it costs nothing in the port's link.

## The three objects, as layouts

- family 1 (`fn_800431C4`): three `rstl::vector`s at 0x3C, 0x2C, 0x1C, destroyed in descending
  offset order, the middle one `vector<float>`. `SVectorOwner3` is 0x4C bytes and the three
  `addi r3,r30,off` come from the layout. Spelled as vectors, not as a raw block, because
  `fn_80043234` reads a count at +4 and the buffer at +12.
- family 2 (`fn_80043510`): one `rstl::vector<CToken>` and nothing else, which is why the
  element destructor gets r3 untouched - no `addi` for a member at a non-zero offset.
- family 3 (`fn_800436CC`): a counted array of **44-byte** records at +4 with the count at +0,
  each holding a flag pointer at +0x24 (whose first byte is the flag) and a `CToken*` at +0x28.
  Walked by index: `li r29,0` is the counter, `addi r30,r30,44` steps, `cmpw r29,r0` against
  `lwz r0,0(r28)` is the test.

## `fn_80043234`: the vector's own iterators, not raw pointers

First written as a raw pointer walk it measured 84.61%. The residue was entirely the frame:
retail has `stwu r1,-32` and four spills (`stw r3,20(r1) ; stw r3,8(r1) ; stw r0,16(r1) ;
stw r0,12(r1)`) where the raw-pointer form had `-16` and none. Those are the two
`pointer_iterator`s `begin()` and `end()` each holding - the vector pointer twice, the end
pointer twice. Replacing the loop with `rstl::destroy(self->begin(), self->end())` reproduced
the frame and the spills exactly: 84.61% -> 96.97%, and the return type took it to 100.00%.

**The lesson worth keeping: before concluding that a residue is register allocation, check
whether the frame size is part of it.** A 16-byte frame against retail's 32 is a different
*program shape* (two iterators rather than two pointers), not a different allocation of the same
one, and no amount of rewording the pointer walk would have produced those four stores.

## What is left, and why `fn_8004371C` stopped at 77.68%

`fn_8004371C` is the only one of the twelve below 100%, and the item's `reason` did not name it
(it named only the leaves). Retail's tail over the 44-byte records:

```
80043748:  cmplwi  r30,0          ; the record pointer, tested for null
8004374c:  beq     end
80043750:  addic.  r3,r30,36      ; r3 = rec + 0x24
80043754:  beq     end
80043758:  lbz     r0,0(r3)       ; the flag byte
8004375c:  cmplwi  r0,0
80043760:  beq     end
80043764:  lwz     r31,40(r30)    ; the CToken*
80043768:  cmplwi  r31,0
8004376c:  beq     end
80043770:  beq     80043780       ; <-- redundant: skips only the ~CToken
80043774:  mr      r3,r31
80043778:  li      r4,0           ; <-- flag 0, not -1
8004377c:  bl      __dt__6CTokenFv
80043780:  mr      r3,r31
80043784:  bl      Free__7CMemoryFPCv
```

Two things are unresolved and both are codegen, not layout:

- **The `beq` at 0x80043770** re-tests the token pointer MWCC has just proved non-null, and this
  time the target is the `Free` rather than the loop end. A `continue`-shaped guard on
  `rec.x24`/`rec.x24[0]`/`rec.x28` emits the three `beq`s to `end` and none of this. A
  `&&`-chain in the condition would be the natural source of a duplicated test, but three
  spellings of that (`||` in one condition, `if (a) if (b) if (c)`, `&&`) all collapse to the
  same three branches here. Not measured to a wall - not attempted, because the second
  difference below dominates.
- **`li r4,0`, not `li r4,-1`.** The `~CToken()` call takes flag **0**; the leaves take -1.
  This is the one place in the unit where the flag is not the non-deleting value, and
  `~CToken()` spelled as a call gives -1. `CToken`'s out-of-line destructor is a *different*
  entry point at 0x8030154C from the `__dt__6CTokenFv` the leaves reach, so this may be an
  unlock/release helper rather than the destructor. Until that is resolved the `bl` target and
  the flag disagree with what the rest of the family does.

`fn_8004371C` at 77.68% is worth one more run against those two; it is a real function at a real
address and nothing about it is a wall.

## Measured, and gates

```
./tools/fast_try.sh MetroidPrime/CStateManager
  main/MetroidPrime/CStateManager: 14.33% fuzzy, 10.02% matched code, 90/239 functions
```

Baseline was 79/239, re-measured from `build/report.json` at the start of this run, not
recalled.

```
./tools/gate.sh            -> GATE FAIL: docs        (stale HANDOFF state block only)
MP_GATE_DOCS_WRITE=1 ./tools/gate.sh
  -> GATE PASS  58abc50+2 changed
  matched  9421 -> 9432   linked 4764 -> 4764   (+11 functions at 100%, 0 units newly linked)
  configure ok / ninja + build.sha1 ok / hashes vs config.yml ok / report ok
  per-function diff ok / module wiring ok / dol_read ok / docs claims ok
  gs offsets ok / raw offsets ok / decl order ok / files.cmake ok / module order ok
  port probe ok / port link gap ok / reach stubs ok
```

The first `gate.sh` run fails only because `docs/HANDOFF.md`'s state block is one commit stale;
the goal loop's judge runs `gate.sh` with `MP_GATE_DOCS_WRITE=1`, which rewrites the derived
counts from the tree and passes. **`docs/HANDOFF.md` was reverted and is untouched by this
change** - the driver commits it with the notes, and the judge rewrites the counts.

`probe_sources.sh` is the reason the families had to land whole:

```
probe: 733 files, 0 failed, 0 errors; link: LINKED (254 undefined, 0 duplicates)
link_check: unique undefined symbols 254 / duplicate definitions 0
link_check: unchanged from baseline (254 undefined, 0 duplicates)
```

254 before, 254 after, against `docs/research/port_link_baseline.txt` (254). The item's
`reason` recorded 313 -> 317 for the forwarders alone; the baseline has moved since, and the
count is the same problem: a forwarder whose callee is only *declared* asks the host linker for
a symbol nothing defines. The three families above are each closed to their leaf, and each leaf's
own callees (`CMemory::Free`, `CToken::~CToken`, the vector destructors) are real functions the
port already has.

Also clean: `python3 tools/check_symbol_names.py` -> 0 declared names missing;
`python3 tools/check_decl_order.py` -> `ok: 933 unit(s) checked, 28 permuted, all 28 accounted
for in decl_order.md` (CStateManager was already in that list before this change, at
`docs/research/decl_order.md:40`, and stays permuted - the new functions are declared
descending by retail offset within the new block, but the unit as a whole was already listed and
is still in the same state).

## Ranges claimed

None. This is a DOL unit; no `splits.txt` or `configure.py` change, no carve.

## Files

- `src/MetroidPrime/CStateManager.cpp` - the new block sits after the `fn_80039B1C` forwarder
  (around line 124 onward), replacing the comment that said the three forwarders were held back.
  Three includes added: `Kyoto/Alloc/CMemory.hpp`, `Kyoto/CToken.hpp` (and `Kyoto/Graphics/
  CModel.hpp` was already there).

## No NEW:

Nothing new is blocked. `fn_8004371C` is a spelling question inside the unit this item already
targets, not a separate unit, so it does not warrant a `NEW:` line.
