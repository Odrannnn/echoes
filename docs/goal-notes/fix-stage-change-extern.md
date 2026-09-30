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

---

# Run 3 (2026-09-30, lane 3) - target `MetroidPrime/CAnimData`, +1 function, PASS

`kind: progress`, target `MetroidPrime/CAnimData` (the driver re-queued the id with this target;
the two earlier runs worked `MetroidPrime/Player/CGameState`, which is why they never touched the
reason). **Unit 73 -> 74 of 216 matched functions. Global `build/report.json` 10395 -> 10396,
`linked` 5048 -> 5048. `./tools/goal_check.sh build/goal/item.json` -> PASS:**

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 10395 -> 10396   linked 5048 -> 5048
  ok    check_symbol_names.py
  ok    All:  31.57% fuzzy, 24.11% matched, 11.83% linked (10396 / 28465 functions)
  ok    target rose: main/MetroidPrime/CAnimData: 73 -> 74 / 216 functions
  ok    no asm added
goal_check: PASS fix-stage-change-extern
```

## The `reason` is stale again, measured a third time on this tree

Re-measured, not recalled: `tools/run_goal.sh`'s `stage_change` is

```
$ sed -n '450,452p' tools/run_goal.sh
  ( cd "$WT" && git add -A -- src include config docs configure.py files.cmake CMakeLists.txt \
      extern/musyx extern/musyx-port ) || true
```

so the `extern/` pathspec defect it describes was fixed in `e6916a5` and the `MUSY_VERSION` guards
landed in `ef9e308`; the build links and reproduces retail exactly (`sha1sum` below). The item as
described is not actionable by an agent anyway - the file is in `tools/`, which we may not edit.
**Third run in a row to reach that conclusion; recommend the driver drop or re-scope this id.**

## The change: one line, `include/MetroidPrime/CAnimData.hpp:232`

```diff
-  uchar mUniformScale : 1;
+  bool mUniformScale : 1;
```

`SetModelScale__9CAnimDataFRC9CVector3f` (0x80025D74, 92 B) goes **81.74% -> 100.00%**, 92 bytes
against retail's 92, with 1 differing instruction of 23 - the `lfs f1` **relocation field** at
+0x0C, which objdiff ignores. `src/MetroidPrime/CAnimData.cpp` is byte-for-byte unchanged; the
whole fix is the *declared type* of the bitfield. No `asm`, no `configure.py`, no `files.cmake`.

### Why: mwcc treats a 1-bit `uchar` bitfield and a 1-bit `bool` bitfield as different types

Retail's tail (0x80025DB4-0x80025DCC) is

```
lbz r0, 0x2ad(r3) / rlwimi r0, r5, 7, 24, 24 / stb r0, 0x2ad(r3)      ; mUniformScale = uniform
lbz r0, 0x2ad(r3) / extrwi r0, r0, 1, 24 / stb r0, 0x2f1(r3)          ; mPose.SetUniformScale(mUniformScale)
```

`extrwi r0, r0, 1, 24` is "read back a value that is already 0 or 1" - no `!= 0` normalisation - and
the `rlwimi` inserts a register that is known normalised. With `uchar : 1` mwcceppc models the field
as an arbitrary byte: it masks before inserting (`clrlwi r4, r0, 24`, which is a **no-op bug for a
0-or-1 value - the emitted code stores 0 into the flag and reads back an unrelated bit**) and
normalises on read (`rlwinm r4, r0, 25, 31, 31` / `neg` / `or` / `srwi`), which is 4 extra
instructions and 16 extra bytes. Declaring the field `bool : 1` is the only change needed to get
both of retail's forms.

**Do not blanket-apply this to the sibling flags.** Measured: converting all ten 1-bit fields at
0x2ac-0x2ad (`mAnimating`, `mLoop`, `mAligningPos`, `x2ac_27_`, `x2ac_28_`,
`mAnimationJustStarted`, `mPoseBuilt`, `mAnimatedScale`, `x2ad_25_`, plus `mUniformScale`) to
`bool : 1` leaves the matched count at 74 and **drops `__ct__9CAnimData...` from 87.45% to 84.78%**.
Reverted; only `mUniformScale` is a `bool` in retail. `AddAnimatedScale` (which writes
`mAnimatedScale`) is at 100% with `uchar : 1` and retail's `rlwimi r0, r3, 0, 31, 31` there is the
signature of a plain integer bitfield, so the two really are different.

### Every spelling measured for `SetModelScale` (92 B retail; "%" = objdiff fuzzy)

| spelling | % |
| --- | --- |
| **`bool mUniformScale : 1;`, body unchanged** | **100.00** |
| `bool mUniformScale : 1;`, `bool uniform = ...; mUniformScale = uniform; mPose.SetUniformScale(mUniformScale);` | 100.00 |
| `bool mUniformScale : 1;`, same but passing the local to `SetUniformScale` | 91.09 |
| `uchar : 1`, `bool` local, pass the **local** | 91.09 |
| `uchar : 1`, `bool` local, read the member back | 91.09 |
| `uchar : 1`, body unchanged (**HEAD**) | 81.74 |
| `uchar : 1`, `uchar uniform = ...` local | 84.78 |
| `uchar : 1`, `SetUniformScale(uchar)` param + read-back | 81.09 (96 B) |
| `uchar : 1`, `mUniformScale = c1 && c2 ? true : false;` | 72.83 |
| `uchar : 1`, `mUniformScale = false; if (c1 && c2) mUniformScale = true;` | 61.52 |
| `uchar : 1`, `if (c1 && c2) { mUniformScale = true; mPose.SetUniformScale(true); } else { ... }` | 66.09 |
| `uchar : 1`, `mPose.SetUniformScale(uniform = ...)` as the argument | 81.09 |

Reading the member back is required: passing the local drops the `lbz`/`extrwi` pair and 2
instructions (21 vs 23). `bool` local + read-back is the spelling that keeps r5 as the value
register; without the local the compiler reuses r0 and re-emits the mask.

## Verification (all re-measured on this tree)

```
git diff --stat                        include/MetroidPrime/CAnimData.hpp | 2 +-
                                      (docs/HANDOFF.md's 2 lines are the judge's own
                                       check_docs_claims.py --write inside goal_check.sh)
sha1sum build/G2ME01/main.dol          6ef9b491d0cc08bc81a124fdedb8bfaec34d0010  (retail, exact)
./tools/decomp_build.sh                All: 31.57% fuzzy, 24.11% matched, 11.83% linked
                                       main/MetroidPrime/CAnimData: 26.63% fuzzy (74 / 216)
./tools/goal_check.sh build/goal/item.json   PASS  (output at the top of this section)
./tools/probe_sources.sh               752 files, 0 failed, 0 errors; LINKED (250 undefined, 0 duplicates)
python3 tools/check_symbol_names.py    checked 505 units; 0 declared names are missing
python3 tools/check_decl_order.py --unit MetroidPrime/CAnimData
                                      ok: 1 unit checked, none emits its functions out of retail order
./tools/unit_fit.sh MetroidPrime/CAnimData.cpp
                                      107 functions present in ours but not in the retail unit
                                      object, 11284 bytes (pre-existing; the unit is not a flip
                                      candidate - 216 retail functions against 323 of ours)
python3 tools/bytescmp.py build/G2ME01/src/MetroidPrime/CAnimData.o SetModelScale 0x80025D74 0x5C
                                      1 differing instruction of 23 (92 B ours vs 92 retail):
                                      the lfs relocation only
python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json
                                      matched 10395 -> 10396  linked 5048 -> 5048
                                        +100%  main/MetroidPrime/CAnimData :: SetModelScale__9CAnimDataFRC9CVector3f
                                      no regression
```

`CAnimData.hpp` is a header of a `NonMatching` unit, so nothing else in the tree can see the
changed declaration except `src/MetroidPrime/CAnimData.cpp` (`grep -rn mUniformScale` finds no
other use of `CAnimData::mUniformScale`; the `DolphinCSkinnedModel.cpp` and
`CPoseAsTransforms_Linear.cpp` hits are different classes' members of the same name).
`CHECK_SIZEOF(CAnimData, 0x5b8)` still holds - `bool : 1` is one bit, like `uchar : 1`.

## Measured, not landed - for whoever takes CAnimData next

### `fn_8002E95C` (0x8002E95C, 44 B): 41.82%, and it is a **block copy**, not a copy ctor

Retail is five `lfd`/`stfd` pairs, `0x0`..`0x28` - a straight 40-byte move of the whole
`CPASAnimInfo` (0x28, 5 doubles). Ours is a `stwu`/`mflr` frame around a call to a copy
constructor: `CPASAnimInfo.hpp:13` declares a **user-provided** copy ctor
`CPASAnimInfo(const CPASAnimInfo& other) : mId(other.GetAnimId()), mParms(other.mParms) {}`, so
the object is not trivially copyable and the compiler must call the `reserved_vector` copy ctor.
For retail to block-copy, `CPASAnimInfo` has to be trivially copyable, which means its
`rstl::reserved_vector` member's copy ctor has to be trivial - and that contradicts
`rstl::vector<CPASAnimInfo>::vector(const rstl::vector<CPASAnimInfo>&)`
(`__ct__Q24rstl49vector<12CPASAnimInfo,...>FRC...`, 0x8002DFC4, **already 100% matched**), which
per-element-calls this very function. So the two facts cannot both come from the same
reconstruction of `rstl::reserved_vector`. Not attempted: making it trivially copyable would mean
changing the semantics of every `reserved_vector` copy in the tree, which is not one item's work
and is where the interesting risk is. Recording it because it is measured from the disassembly
and the two matching/unmatching facts, not guessed.

### The four `construct_impl` thunks retail emits and we do not

`fn_8002DDA4`, `fn_8002DBC8`, `fn_8002D9E4`, `fn_8002E4B8` (32 B each) are all the same
prologue + one `bl` + epilogue: a **thunk** to `construct_impl<CBoolPOINode>`,
`<CParticlePOINode>`, `<CSoundPOINode>`, `<CPASAnimState>` - all four of which we *do* emit and
match at 100%. They show as `None`/`0.00%` in `report.json` because our object does not define
them at all, so objdiff has nothing to pair. They are the signature of the target being used as a
**function pointer / template argument with a different type** (mwcc's calling-convention thunk),
not of a call. Reproducing them needs the specific use that takes the address; not attempted, and
no `WALL:` is claimed for them.

### Also present, not attempted (fuzzy from `build/report.json`)

`InitializeEffects` 49.23%, `AdvanceIgnoreParticles` 48.13%, `BuildTransitionTree` 17.48%,
`AdvanceAdditiveAnim` 21.38%, `SetKeepJSPose` 40.02% (348 B),
`__ct__9CAnimData...` 87.45% (the largest single gap in the unit; the one the `bool : 1` sweep
made worse), `AdvanceAdditiveAnims` 4.43% (1288 B), `DoAdvance` 4.41%, `Advance` 2.65%,
`BuildAnimationTree` 5.58%, `GetAnimationDuration` 0.91%. Nothing here is claimed to be reachable
and no `NEW:` is filed: for every one of them I have no spelling that reached 100%, so filing them
would spend a lane on a guess.
