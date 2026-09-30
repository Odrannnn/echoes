# cmorphball-getswooshtoworld-rvo

`MetroidPrime/Player/CMorphBall` **50 -> 51 matched functions** (158 total), unit fuzzy
**17.758139% -> 17.78871%**, unit `matched_code` 3944 -> 4136 of 66600. Global
`matched 10041 -> 10042`, `linked 4908 -> 4908`, `All: 30.97% fuzzy, 23.23% matched,
11.76% linked (10042 / 28465 functions)`. The unit stays `NonMatching`.

One function, one file: `src/MetroidPrime/Player/CMorphBall.cpp` (+8/-4, comment included). No
header, no class layout, no `configure.py` / `splits.txt` / `files.cmake`, no `asm`, no docs.

This is the item `cmorphball-getballtoworld-inline` filed as its `NEW:` line. Its recipe was
correct and it took one edit.

## What retail does

`GetSwooshToWorld__10CMorphBallCFv`, target `0x800CB7F4`, 192 bytes. Retail's object is
`build/G2ME01/obj/MetroidPrime/Player/CMorphBall.o` (objdiff's `target_path`) - the branch that
matters, because `build/G2ME01/src/...` is *ours*. Ours was 168 bytes, frame 224; retail is 192
bytes, frame 272. The only difference was the tail: retail materialises the **second** product
and then copy-constructs it into sret, ours RVO'd that `operator*` straight into sret.

Call sequence, from the relocations, identical on both sides except for the tail:

    GetBallRadius(this)                                   # f1, then folded into the z add
    RotateY(sret=+0x18,  &CRelAngle(mBallTiltAngle))     # +0x18 = 24
    GetRotation(sret=+0x48, this+0xD28)                   # +0x48 = 72, mSurfaceToWorld
    Translate(sret=+0x78, &CVector3f@+0x0C)              # +0x78 = 120
    __ml__(sret=+0xA8, +0x78, +0x48)                     # +0xA8 = 168  <- real temp already
    __ml__(sret=+0xD8, +0xA8, +0x18)                     # +0xD8 = 216  <- RETAIL ONLY
    __ct__(sret, +0xD8)                                                  <- RETAIL ONLY

Note retail *does* call `GetBallRadius()` here (`0x800CE9C8`), while reading `mBallTiltAngle`
(`this+0x30` = 48) directly into a `CRelAngle` temp at `this+8`. That is the opposite of
`GetBallToWorld` (`0x800CB764`), where retail reads `mRadius` inline and never calls out. Our
source already had `GetBallRadius()` in the right place, so the position build was never the
problem - only the RVO was.

## The edit

`return A * Q * R;` -> `return CTransform4f((A * Q) * R);`, with the existing expression split
over lines and a comment recording the retail address. Wrapping the **outer** product is what
matters: the argument to the copy constructor is then a prvalue that has to exist as a real
object, so mwcceppc puts it at `this+216` and emits the `__ct__`. The inner product needed no
help - it is an argument to a real call, so it was already a real temp at `this+168` in both.

Do **not** reach for a named local instead. `const CTransform4f t = A * Q * R; return t;` is
NRVO'd (that was spelling 2 in the `GetBallToWorld` table, 91.47%).

## Verified

- `GetSwooshToWorld__10CMorphBallCFv` **89.395836% -> 100.0%**; the two disassembly dumps are now
  identical instruction for instruction - 55 lines each, the only differences being the absolute
  `bl` targets. Done with `objdump -dr` on both objects, offsets stripped, relocations compared
  name by name (all seven match, including the `__ct__` that was missing).
- Per-function diff of the whole report before/after (`build/report.json`, **28465** scored
  functions both sides): **0 worse, 1 better, 0 new, 0 gone**. The one better entry is
  `GetSwooshToWorld__10CMorphBallCFv`. The unit's `.text` grew 24 bytes (the three extra
  instructions), and no other function in the object moved in the report.
- `MP_GATE_DOCS_WRITE=1 ./tools/gate.sh build/goal/judge/report.base.json` -> **GATE PASS
  3ce3a93+2 changed**, all 16 steps `ok` (configure, ninja+build.sha1, hashes vs config.yml,
  report, per-function diff `matched 10041 -> 10042 linked 4908 -> 4908 (+1 functions at 100%, 0
  units newly linked)`, module wiring, dol_read, docs claims, gs/raw offsets, decl order,
  files.cmake, module order, port probe, port link gap, reach stubs). Baseline was recorded at
  `3ce3a93`, this worktree's HEAD. I reverted the two derived-count lines in `docs/HANDOFF.md`
  that `gate.sh --write` rewrote, so the diff is the one source file.
- `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
- `./tools/probe_sources.sh` -> `749 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0
  duplicates)`.
- `python3 tools/check_symbol_names.py` -> `checked 503 units; 0 declared names are missing`.
- `python3 tools/check_decl_order.py --unit main/MetroidPrime/Player/CMorphBall` ->
  `would break on a flip`, and the output is **byte-identical** before and after the edit
  (checked by stashing). The unit is 51/158 and is not going near a flip.
- `./tools/unit_fit.sh MetroidPrime/Player/CMorphBall.cpp` -> `50 function(s) present in ours but
  not in the retail unit object, 4800 bytes total`, unchanged from the previous item - all
  COMDAT weak copies and template destructors.

## The general lesson, for whoever hits it again

**RVO bites on a returned class prvalue, and MWCC will take it.** Any function that ends in
`return <big-class expression>;` where the expression is a call returning by sret is at risk:
mwcceppc hands the callee's sret slot straight to the caller's return slot, drops the final
copy constructor, and shrinks the frame by one object. Retail, in the cases measured here, does
**not** do that: it materialises the result and then emits `__ct__(sret, tmp)`. The tell is a
frame that is one object short and a missing `__ct__` at the tail. The fix is to wrap the
returned expression in the class type - `return CTransform4f(expr);` - which makes the temporary
a real argument to the copy constructor. It has now taken one function from 89.40% and one from
85.92% to 100% with the same one-token change, so **check every class-returning function in a
unit you touch for a tail `__ct__` that we do not emit** before assuming the remaining gap is
register allocation.

No `NEW:` line: the four other near-miss functions in this unit were checked and are a different
problem each (`GetRenderBounds` 93.88% is `CAABox`/`AccumulateBounds`, not RVO; `SpinToSpeed`
84.77% has a missing 16-byte temp - frame 80 vs our 64 - not a missing copy;
`InitializeWakeEffects` 99.73% and `CreateBallShadow` 99.97% have byte-identical prologues and
epilogues, so their gap is in the body; `UpdateIceBreakEffect` 99.99% likewise). I did not
measure a spelling for any of them, so filing them would be a restatement, not work.
