# fix-stage-change-extern

`kind: progress`, target `MetroidPrime/Player/CGameState`, stays `NonMatching`.

**Unit 98 -> 99 of 116 matched functions. Global `build/report.json` matched 9986 -> 9987, linked
unchanged at 4896.** `tools/goal_check.sh build/goal/item.json` -> **PASS**:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 9986 -> 9987   linked 4896 -> 4896
  ok    check_symbol_names.py
  ok    All:  30.75% fuzzy, 22.98% matched, 11.74% linked (9987 / 28465 functions)
  ok    target rose: main/MetroidPrime/Player/CGameState: 98 -> 99 / 116 functions
  ok    no asm added
goal_check: PASS fix-stage-change-extern
```

## The item's `reason` is stale; it was already fixed before this run

The reason describes a `tools/run_goal.sh:446` pathspec defect and names CGameState only because
that is where the run which found it was working. **The pathspec is already fixed** in this tree's
history, in `e6916a5` ("tools: goal loop stages extern/musyx and extern/musyx-port"):

```
$ sed -n '444,447p' tools/run_goal.sh
  ( cd "$WT" && git add -A -- src include config docs configure.py files.cmake CMakeLists.txt \
      extern/musyx extern/musyx-port ) || true
```

`ef9e308` ("fix: commit the MusyX stream.c guards match-stream flipped against") is the commit that
landed the four `MUSY_VERSION` guards. The build links and reproduces retail exactly, so there is
no unblock left to do and **no `NEW:` line is filed for it** - the blocker it described is gone.
What the judge actually tests for a `progress` item is `target_rose` on CGameState, so that is
what this run did.

**Worth flagging to the driver:** an item whose stated `reason` is un-actionable by an agent
(agents may not edit `tools/`) is better dropped or re-queued against a real target than re-issued
as a `progress` item with an unrelated unit attached. Two consecutive runs of this id have now
spent their budget on CGameState rather than on the reason.

## What changed: `fn_8014680C`, 92.69% -> 100.00%

One function, three lines of body, `src/MetroidPrime/Player/CGameState.cpp:66-88` only. Retail's
`fn_8014680C` (0x8014680C, 104 bytes) is the move half of `fn_801466F4`'s grow.

**The one thing that decides it: the loop bound is re-read from `end` on every iteration, and
that needs `end` to be a `void**`.** Retail's guard is `lwz r0,0(r29)` / `cmplw r31,r0`
(0x80146848-0x8014684C) with `r29` holding the `end` *pointer* live across the whole loop, so the
`fn_80142718` call is taken to be able to write through `*end`. Written `void* const*` - both
parameters const, which is what this file had - mwcceppc hoists that load above the loop and
emits `cmplw r29,r31` against a register instead. Making **only `end`** a `void**` restores the
reload; making `begin` one too is what breaks the register assignment (see the table).

**The second thing: the input cursor is declared before the output cursor.** Retail reserves r31
for `in` and r30 for `out` and can then place the `lwz r31,0(r3)` immediately after the
`stw r31,28(r1)` prologue spill, which is retail's instruction order. Any other order rotates
those five prologue instructions and costs 4-11 instructions of objdiff for identical semantics.

Final body:

```cpp
extern "C" void* fn_8014680C(void* const* begin, void** end, void* dst) {
  uchar* in = static_cast< uchar* >( *begin );
  uchar* out = static_cast< uchar* >(dst);
  for (; in != static_cast< uchar* >( *end ); in += 36, out += 36) {
    fn_80142718(out, in);
  }
  return out;
}
```

### Every spelling measured (all 104 bytes unless noted; "differing" = `tools/bytescmp.py`)

| spelling of the two pointers / the cursor declarations | differing | score |
| --- | --- | --- |
| **`void* const* begin, void** end`; `in` then `out`; empty for-init** | **1 of 26** | **100.00%** |
| `void** begin, void** end`; `in` then `out`; empty for-init | 6 of 26 | 88.46% |
| `void** begin, void** end`; `in` then `out`; `out += 36` in the body | 9 of 26 | - |
| `void** begin, void** end`; `in` then `out`; guard spelled `*end != in` | 7 of 26 | - |
| `void** begin, void* const* end` (no reload) | 19 of 25, 100 B | 88.46% |
| `void** begin, void** end`; `out` then `in`; empty for-init | 15 of 25, 100 B | 88.46% |
| `void** begin, void** end`; `out` then `in` as the for-init | 11 of 26 | 88.46% |
| `void** begin, void** end`; `in` then `out`; `while` loop | 15 of 25, 100 B | - |
| `void* const* begin, void* const* end`; `out` then `in` as the for-init (**HEAD**) | 15 of 25, 100 B | 92.69% |

The single remaining "differing" instruction in the winning spelling is the `bl` field at +0x30
(`4bffbedd` retail vs an unresolved relocation in ours); objdiff ignores relocation fields, which
is why the function scores 100.00%.

An earlier dead end worth not repeating: making the *forward declaration* of `fn_80142718` take a
non-const `void* src` cannot work at all - `extern "C"` cannot overload, so the definition at the
bottom of the file has to change too, and the build fails. The reload is controlled by the
**parameter types of `fn_8014680C`**, not by anything about `fn_80142718`.

## Verification

```
sha1sum build/G2ME01/main.dol                       6ef9b491d0cc08bc81a124fdedb8bfaec34d0010  (retail, exact)
./tools/decomp_build.sh                             All: 30.75% fuzzy, 22.98% matched, 11.74% linked
                                                    (9987 / 28465 functions)
./tools/goal_check.sh build/goal/item.json          PASS  (output at the top of this file)
python3 tools/check_symbol_names.py                 checked 503 units; 0 declared names are missing
python3 tools/check_decl_order.py --unit MetroidPrime/Player/CGameState
                                                    ok: 4 unit(s) checked, none emits its functions
                                                    out of retail order
python3 tools/bytescmp.py ... CGameState.o fn_8014680C 0x8014680C 0x68
                                                    1 differing instruction of 26 (104 bytes ours
                                                    vs 104 retail) - the bl relocation only
```

Per-function diff against the judge's baseline, computed rather than recalled
(`python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json`):

```
matched  9986 -> 9987   linked 4896 -> 4896   (+1 functions at 100%, 0 units newly linked)
  +100%    main/MetroidPrime/Player/CGameState :: fn_8014680C
no regression
```

Files touched: `src/MetroidPrime/Player/CGameState.cpp` only - 3 body lines rewritten plus the
comment above them. `docs/HANDOFF.md` was rewritten by the judge's own
`check_docs_claims.py --write` inside `goal_check.sh`, not by hand. No header change, no
`configure.py`, no `build/goal/` file, no `tools/` file, no `extern/`, no `.s` file.

Note for the next run: `src/MetroidPrime/Player/CGameState.cpp` is **not** in `files.cmake`, so it
is not compiled or linked by the port. A new undefined symbol introduced here cannot move the
port's link gap, which is what makes the `extern "C"` declaration style in this file safe.

## Measured, not landed - spellings and scores for whoever takes them

### `fn_80143CD4` (0x80143CD4, 80 B): 96.25%, one register allocation away

This is `rstl::reserved_vector<CGMFrontEnd::SPlayerConfig, 4>::reserved_vector(const&)`, called
from `CGMFrontEnd`'s copy constructor at 0x80143CB8 with `&mPlayers` of both objects (`mPlayers`
is at +0x20; `CHECK_SIZEOF(CGMFrontEnd, 0x44)` = 32 + 4 + 4*8 confirms it). **Both
`__ct__11CGMFrontEndFRC11CGMFrontEnd` and `__dt__11CGMFrontEndFv` are undefined in
`CGameState.o`** and retail's bytes for them are filled in by dtk's asm
(`build/G2ME01/asm/MetroidPrime/Player/CGameState.s:2023` and `:2079`), so this unit's report has
**no `fuzzy_match_percent` at all** for `fn_80143CD4`, `__ct__11CGMFrontEnd` and `__dt__CGMFrontEnd`
- objdiff has nothing to pair them with. Defining `fn_80143CD4` under an `extern "C"` name is what
makes it pairable, exactly as `fn_80142288` was.

Getting to 96.25% needed three findings, all of them real and reusable:

1. **The count is written and then re-read.** Retail does `stw r0,0(r3)` then `lwz r0,0(r3)` then
   `mtctr`. That is the constructor's member-initialiser store followed by the body reading the
   member again as `uninitialized_copy_n`'s `n` argument - not a local.
2. **The loop is a counted `bdnz` with a null test on the destination cursor inside it**
   (`cmplwi r5,0` / `beq` at 0x80143CF4), and the tree's `rstl::construct` reproduces both
   (`rstl/construct.hpp`'s `RSTL_PRECONDITION` compiles away, but the placement-new null check
   does not). Spelled as a hand-written `if (out) { ... }` the loop gets **unrolled four times**
   (204 bytes) - it is `rstl::construct(out, *in)` inside the counted loop that keeps the shape.
3. **The 8-byte element is copied member-wise** (`lwz` word, `lbz` +4, `stw` word, `lbz` +5,
   `stb` +4, `stb` +5), not as two words. Getting that needs a copy that is *not* a block move:
   `rstl::uninitialized_copy_n` over `CGMFrontEnd::SPlayerConfig` emits two `lwz`/`stw` pairs and
   sits at 12 differing instructions. A same-layout view struct whose copy constructor is
   user-provided reproduces retail's six instructions.

Scores for the spellings, all 80 bytes unless noted: three `rstl::construct` calls per element
**204 B (unrolled)**; an index-based range **96 B**; `do { } while (--remaining)` **72 B, no null
test**; `rstl::uninitialized_copy_n` over the view's array member 11 of 20; over
`uchar mData[32]` 12 of 18 (**72 B**); explicit counted loop + `rstl::construct`, `in` then `out`
**6 of 20 = 96.25%**; the same with `out` then `in` 7 of 20.

The 6 remaining differences are **purely register numbering**: ours puts the source cursor in r4
and reuses r3 for the byte temporary; retail puts the source cursor in r6, keeps r3 unused, and
reuses r4 (the dead `src` parameter) for the byte temporary. The instruction sequence, the
schedule, the sizes and the loop shape are already identical. Nothing I tried moved the
allocation, and I did not find a spelling that keeps r3 occupied across the loop without adding
code, so **this function is not landed** - it is a measured wall, and no spelling for it is
claimed to exist.

`__ct__11CGMFrontEnd` (140 B) and `__dt__CGMFrontEndFv` (180 B) are **not** worth attempting the
same way: both are almost entirely compiler-generated (two vtable fix-ups at 0x80304470 and
0x8030BD68, the base-dtor chain, the inline `destroy_elements` expansion, the `CMemory::Free`
tail call), and neither vtable symbol exists in this tree, so writing them by hand would be
transcription rather than decompilation.

### `PutTo__11CWorldStateCFR16CBitStreamWriterRC18CWorldSaveGameInfo` (148 B): 4 bytes short

Ours is 144 bytes against retail's 148, and the whole difference is one instruction: retail has
`mr r5,r31` at 0x80145044 before the last `bl`, ours reuses the `r5` left over from the
`CMapWorldInfo` call. The last call is `CWorldLayerState::PutTo`, at 0x801721FC, which
`config/G2ME01/symbols.txt:6086` names **`PutTo__16CWorldLayerStateCFR16CBitStreamWriter`** -
two parameters - and whose 240-byte body ignores `r5` entirely. `src/MetroidPrime/CWorldLayerState.cpp`
implements exactly that 2-parameter signature and is **`Matching` at 100%**.

So retail's caller materialises `saveWorld` into `r5` for a callee that does not take it, and
there is no C++ spelling of a 2-parameter member call that emits that. The obvious fix - adding
`const CWorldSaveGameInfo&` to `CWorldLayerState::PutTo` - would rename the symbol to
`...CFR16CBitStreamWriterRC18CWorldSaveGameInfo`, objdiff would then fail to pair it against
retail's 2-parameter name, and the `Matching` `CWorldLayerState` unit would drop to 14/15, i.e.
**`linked` would fall and the judge would fail the item**. There is exactly one call site of
0x801721FC in the whole binary (measured: `objdump -d build/G2ME01/main.elf | grep 'bl 801721fc'`
returns one line), so there is no second call site to cross-check the arity against, and
`tools/dump_fn_relocs.sh 0x80144FD4` reports "no object in build/G2ME01/obj defines it" - the
retail relocations are gone now that this unit claims the range.

I did not land a `extern "C"` forward declaration with a 3-parameter signature for retail's
2-parameter symbol, because that is an arity the tree has no evidence for and the item is already
judged. Recorded here because it is one instruction and the next run can decide.

### `fn_801466F4` (172 B): 66.63%, and re-reading the members made it worse

Retail re-reads `self->x04_count` and `self->x0c_data` from memory after each call
(0x80146760-0x8014676C) instead of keeping them in registers, so its prologue uses r29-r31 where
ours uses r27-r31 (`stmw`/`lmw`). Spelling the member reads again at the call sites
(`fn_801467A0(static_cast< uchar* >(self->x0c_data), static_cast< uchar* >(self->x0c_data) +
self->x04_count * 36)`) does produce the reloads but drops the function to **27 differing
instructions of 42** against the current **31 of 36** (144 bytes against retail's 172). Two
variants measured, differing only in whether `range[2]`/`range[3]` are written from `range[2]` or
from `self->x0c_data` again: identical, 27 of 42. Not attempted further.

### Not attempted

`__sinit_CGameState_cpp` (80 B, 63.35%): ours is 68 bytes against 80, and retail's is a
six-value `stw` run off three `lis` bases with a different schedule, not a small fix.
`__ct__11CWorldStateFR16CBitStreamReaderUi` (568 B, 97.38%) and `__ct__18CPersistentOptions`
(776 B, 95.52%): ours is 124 and 600-ish bytes against 568 and 776 - the whole tail is
misaligned, not a couple of instructions. `StartGameFromFrontEnd` (784 B, 59.35%),
`__ct__10CGameStateFR16CBitStreamReader` (1668 B, 84.14%), `PutTo__10CGameState` (876 B, 91.90%),
`AddVariable` (196 B, 87.78%): read the disassembly, not attempted. No `WALL:` line is claimed
for any of them.

`NEW: progress-cgamestate-putto-worldlayer-arity | progress | MetroidPrime/Player/CGameState | PutTo__11CWorldState is 4 bytes short because retail passes saveWorld in r5 to the 2-param CWorldLayerState::PutTo, and the honest fix (a 3rd parameter) would un-match the Matching CWorldLayerState unit`
