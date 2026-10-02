# progress-unit-cgamecamera

Unit `MetroidPrime/Cameras/CGameCamera`, DOL unit, stays `NonMatching`.
`build/goal/item.json` said 19/35; re-measured on the clean tree at 19/35 (the reason's
per-function list was accurate). **Result: 19 -> 25 / 35 matched functions**, unit fuzzy
51.97% -> 58.00%, matched code 17.85% -> 32.96%. Global matched 11704 -> 11710.
`./tools/goal_check.sh build/goal/item.json` -> **PASS**.

## What I changed (two files, both under src/ and include/)

Six functions taken to 100%. Each row is before% -> after%, and the spelling that reached it.

| function | before | after | what made the difference |
|---|---|---|---|
| `GetPerspectiveMatrix() const` | 96.07 | **100.00** | `if (mPerspDirty == true)`, not `if (mPerspDirty)` |
| `SetActive(bool)` | 93.33 | **100.00** | `const bool` on the **definition and the declaration** |
| `ConvertToWorldSpace(const CVector3f&) const` | 67.93 | **100.00** | two named locals instead of one returned expression |
| `ResetFovInterpolation(float)` | 24.92 | **100.00** | `SFovInterpolation::Set` moved out of line, **by value** |
| `InterpolateFOV(float,float,float)` | 63.19 | **100.00** | `if (duration <= 0.f)` with `ResetFovInterpolation` first |
| `InterpolateFOV(float,float,float,TUniqueId,CStateManager&)` | 82.78 | **100.00** | same `<= 0.f` inversion, keeping the `const float target` local |

Side effect worth keeping: the constructor rose 81.52% -> 94.54% and
`UpdatePerspective` 65.72% -> 75.29% once `Set` was out of line and its inner test inverted.

The full diff is nine hunks; no `asm`, nothing outside `src/MetroidPrime/Cameras/CGameCamera.cpp`
and `include/MetroidPrime/Cameras/CGameCamera.hpp`.

## Findings that generalise (these cost the build each)

1. **`if (boolBitfield)` and `if (boolBitfield == true)` are different code.** `if (mPerspDirty)`
   emits the recording form `rlwinm.` + `beq`; `== true` emits non-recording
   `rlwinm r0,r0,25,31,31` + `cmplwi r0,1` + `bne`. That is the **whole** remaining 3.9% of
   `GetPerspectiveMatrix`, and `if (true == mPerspDirty)` gives the identical bytes (operand order
   is free). Already documented for a blob flag in `RUNNING_THE_DECOMP.md`; this is a second,
   independent confirmation on a member bitfield. Also measured and **rejected**: `!= false`,
   `static_cast<bool>(...)`, and routing it through a `const uint` local - all four still emit the
   recording form.

2. **`const` on a by-value parameter changes the callee's codegen.** `void CGameCamera::SetActive(bool)`
   cost an extra `clrlwi r4,r4,24` (bool normalisation) that retail does not have; adding `const`
   removes it. It has to be on the **declaration in the header too**, not just the definition -
   I only tested the definition first and it did not move.

3. **`if (x > 0.f) { A } else { B }` and `if (x <= 0.f) { B } else { A }` are different code.**
   MWCC picks the fall-through case from the source order: `<= 0.f`-first gives `bne` over the
   `<= 0` block, `> 0.f`-first gives `ble` into the `else`. Retail's `InterpolateFOV` has
   `fcmpo cr0,f30,f0 ; cror eq,lt,eq ; bne`, i.e. the `<= 0.f`-first spelling. Measured and
   **not** equal to retail: `!(x > 0.f)`, `!(x <= 0.f)`, `0.f >= x`, early-`return` restructuring -
   all land at 90-99%, never 100.

4. **`CVector3f` in a returned expression is not the same as a named local.** Retail's
   `ConvertToWorldSpace` stores `MultiplyOneOverW`'s result to a stack slot and reloads it for the
   transform multiply; `return GetTransform() * ...MultiplyOneOverW(position);` keeps it in place
   and is 22 instructions shorter. Naming **both** the vector and the result (`const CVector3f v`,
   `const CVector3f r`, `return r;`) is 100%. Naming only `v` and returning the multiply is 82.9% -
   the `r` local is load-bearing. This is the same lever as the `*out = CVector3f(a,b,c)` note in
   `RUNNING_THE_DECOMP.md` ~line 5556.

5. **`SFovInterpolation`'s ctor and `Set` are out-of-line in retail, and the last parameter is
   by value.** Both `ResetFovInterpolation` and both `InterpolateFOV`s `bl` a shared 32-byte body,
   and `CGameCamera`'s constructor `bl`s an identical second copy at `0x801b19f8` - two copies
   because mwcceppc emitted the ctor and `Set` separately. `#pragma noinline` on the in-header
   definitions changes nothing (measured). Declaring both in the class and defining them in the
   .cpp is what produces the `bl`, and **`TUniqueId` by value, not `const TUniqueId&`**: the
   by-reference version compiles and scores 88.9% / 81.2%, the by-value one 100% / 87.9%.
   Retail passes the id in `r4` as a stack slot for both, so the source is by value and mwcceppc
   spilled it.

6. **`fn_801B19D8` / `fn_801B19F8` / `__ct__9CMatrix4fFRC9CMatrix4f` are still at 0%** because our
   object names them `Set__Q211CGameCamera17SFovInterpolationFfffff9TUniqueId` and
   `__ct__Q211CGameCamera17SFovInterpolationFfffff9TUniqueId` and objdiff will not pair them with
   retail's unnamed `fn_*` on position alone. They are 32 B each and byte-identical to retail's -
   `unit_fit.sh`-class work for a follow-up, not something this item needed.

## Where I stopped, and the evidence

- **`UpdatePerspective`, 75.29%, 90 of 98 instructions differ.** Inlining the inner `<= 0.f` took
  it 65.72 -> 75.29%. Retail's remaining shape is
  `fcmpo cr0,f1,f0 ; cror eq,gt,eq ; bne` on the `>= 0.00001f` epsilon guard and a plain
  `fcmpo ; blt` at the tail; ours still emits `cror eq,gt,eq ; bne` in both places. Tried:
  `!(AbsF(...) < eps)`, `eps > AbsF(...)`, and hoisting the guard into a `return` - all three land
  at 76.0-76.4% and none reaches 100. The residual is the epsilon comparison, not the control flow.

- **`CMatrix4f::Determinant`, 73.1% best (from 59.09%).** The expression is right; the difference
  is register allocation. Retail evaluates group 1 as `-m12*e + m13*d + m11*f`
  (`fmadds f3,f0,f6,f3` where `f6 = f3 - f6` is the negation) and ours emitted
  `m11*f - m12*e + m13*d`. Writing `m11*f + m13*d - m12*e` gets 60 of 75 differing-instruction
  positions gone (73.1%). Also tried `-m12*e + m13*d + m11*f` (62.5%), `m13*d - m12*e + m11*f`
  (64.6%), and reordering the six minors `a,f,d,e,b,c` (64.1%). 15 instructions still differ and
  they are pure allocation - `fsubs`/`fmsubs` operand order and which minor lands in which
  register. I did **not** apply this one: 73.1% is not a match and the diff would be pure churn.

- **`CMatrix4f::GetInverse` (38.1%, 1004 B), `ValidateCameraTransform` (4.76%, 740 B),
  `GetScanObjectIndicatorPosition` (5.4%, 252 B), `ClearFluidList` (10.9%, 292 B)** - the last
  three are real TODO stubs in the source (`ValidateCameraTransform` even carries the comment
  "TODO: Recover orthonormalization..."). Recovering them is new work, not a spelling fix, and
  filling them in with plausible bodies is exactly what the brief forbids. `GetInverse` is 38.1%
  because it expands the adjugate inline; Prime 1 has no donor for it.

## Verification

```
sha1sum build/G2ME01/main.dol                 -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
python3 tools/check_symbol_names.py           -> checked 515 units; 0 declared names are missing
./tools/goal_check.sh build/goal/item.json    -> goal_check: PASS progress-unit-cgamecamera
                                                  ok  counts: matched 11704 -> 11710  linked 5656 -> 5656
                                                  ok  target rose: CGameCamera: 19 -> 25 / 35
                                                  ok  no asm added
```

`check_decl_order.py --unit MetroidPrime/Cameras/CGameCamera` is clean after the out-of-line
move, so the two new out-of-line definitions did not permute the unit's `.text`.

## Useful commands for the next run

`tools/fast_try.sh` does not work on this unit - its ninja target is
`build/G2ME01/src/MetroidPrime/Cameras/CGameCamera.o`, not `...CGameCamera.cpp.o`. A scratch
harness that rebuilds just that object, runs `objdiff-cli report generate`, and prints
`tools/bytescmp.py`'s per-instruction diff count for a named function iterates a spelling in
about 4 seconds. `tools/bytescmp.py <obj> <needle>` matches the **first** symbol containing the
needle, so `InterpolateFOV__11CGameCameraFfff` picks the 4-arg overload, not the 5-arg one - use
`build/binutils/powerpc-eabi-objdump -d <obj>` with an anchored awk range instead.

`bl` targets: read them out of `build/G2ME01/main.elf`, which holds retail's bytes over this range
(`dtk` fills unclaimed `.text` from the disc). Re-deriving them from the raw DOL by hand is a
trap - the `LI` field is bits 6..29 sign-extended from bit 23, and getting that wrong silently
returns zero callers, which looks exactly like "nobody calls it".
---

# Second run (lane 8, 2026-10-02)

Re-measured on the clean tree: **25/35**, exactly what the previous run's notes said (19 -> 25
had already landed). **Result: 25 -> 29 / 35 matched functions**, unit fuzzy 58.00% -> 64.19%,
matched code 32.96% -> 43.63% (2356 of 5400 bytes). Global matched 12173 -> 12177.
`./tools/goal_check.sh build/goal/item.json` -> **PASS**.

Three files touched, all under `src/`/`include/`, no `asm`:
`src/MetroidPrime/Cameras/CGameCamera.cpp`, `include/MetroidPrime/Cameras/CGameCamera.hpp`,
`include/MetroidPrime/TGameTypes.hpp`.

## What I changed

| function | before | after | what made the difference |
|---|---|---|---|
| `fn_801B19D8` (retail-unnamed, 32 B) | 0.0 | **100.00** | the SFovInterpolation setter, spelled as an `extern "C"` free function |
| `fn_801B19F8` (retail-unnamed, 32 B) | 0.0 | **100.00** | the SFovInterpolation constructor, same treatment |
| `__ct__9CMatrix4fFRC9CMatrix4f` (132 B) | 0.0 | **100.00** | defined in this TU as 16 member initialisers |
| `UpdatePerspective(float, CStateManager&)` | 75.29 | **100.00** | two named locals + `!(x < eps)` instead of `>=` |
| `CMatrix4f::Determinant() const` | 59.09 | **73.11** | group 1's terms reordered; kept (see caveat) |

The previous run left all three 0.00% functions as a follow-up ("byte-identical to retail, but
objdiff will not pair them") and named the exact reason: our object gave them C++-mangled names
(`Set__Q211CGameCamera17SFovInterpolationFfffff9TUniqueId`) and objdiff pairs by name. That is
the whole fix, and it is the first thing to try on any retail-unnamed function.

## Findings that generalise

1. **A retail-unnamed function is only reproducible as a C-linkage free function.** `fn_801B19D8`
   and `fn_801B19F8` are byte-identical 32-byte bodies that mwcceppc emitted twice (constructor
   and setter) and retail named nothing. Written as members they mangle and pair with nothing;
   written as `extern "C" void fn_801B19D8(SFovInterpolation* self, ...)` objdiff pairs them
   immediately, at 100%, **with no change to the body at all**. Which one is which is decided by
   the relocations in the retail object, not by guessing: `objdump -d -r
   build/G2ME01/obj/.../CGameCamera.o` shows `ResetFovInterpolation` and both `InterpolateFOV`s
   relocating to `fn_801B19D8` and `CGameCamera`'s own constructor relocating to `fn_801B19F8`.
   So `fn_801B19D8` is the **setter** and `fn_801B19F8` is the **constructor** - the reverse of
   address order, and the reverse of what the previous run assumed when it named the symbols
   `Set__...` and `__ct__...`.

2. **The record has to move out of the class** to make that work, because the free function needs
   the type. `SFovInterpolation` is now a namespace-scope struct in the header. This is a
   *naming* change only; `CHECK_SIZEOF(CGameCamera, 0x200)` still holds and the field offsets are
   unchanged (mFovInterpolation is still at 0x1E0, verified by the constructor matching).

3. **`TUniqueId` needed a default constructor, and it must have an EMPTY body.**
   `TUniqueId() : value(0) {}` compiles and is **worse**: `li r0,0` plus the store, and
   `CGameCamera`'s constructor drops 94.54% -> 89.18%. `TUniqueId() {}` - leaving `value`
   untouched - is what recovers 94.54%, because the store `fn_801B19F8` makes is then the only
   one. This is a **shared-header change** and the one risk in this diff: it touches 241 objects
   (`ninja -n | wc -l` after touching `TGameTypes.hpp`). Measured harmless - `gate.sh`'s
   per-function report diff fails any function that got worse and found none, and the DOL sha1
   and all 86 REL hashes held. But a future run should know it is there and why.

4. **`!(x < eps)` and `x >= eps` are different code, and retail uses the first.**
   `CMath::AbsF(GetFov() - GetTargetFov()) >= 0.00001f` emits `fcmpo ; cror eq,gt,eq ; bne`.
   Retail has a plain `fcmpo ; blt` and nothing else. `!(... < 0.00001f)` is the spelling that
   produces it: 98.59% -> 99.68%, and after the frame fix it reaches **100%**. The previous run
   recorded that `!(AbsF(...) < eps)` "landed at 76.0-76.4%" - that is not a contradiction, the
   frame was still 80 bytes then (see finding 5), and the guard only reads as `blt` once the
   surrounding allocation matches. **Do not judge a spelling from a run where something else
   is still wrong.** Also measured and rejected: `eps <= x` (98.28%), `x > eps` (99.63%, 1
   non-reloc diff), empty-then/else (identical to `!(x<eps)`, 99.68%), `!CMath::IsEpsilon(...)`
   (identical to `!(x<eps)`).

5. **Two named locals fix `UpdatePerspective`'s register allocation; the difference written
   inline costs a whole stack frame.** The frame is 64 bytes in retail and 80 in ours, and the
   extra 16 is f29's save/restore plus one more spilled pair. Naming `target` and `delta` before
   the `Clamp` is what lets mwcceppc keep the pair in f31/f30 across it: 75.29% -> 98.59%, and
   99.68% with the guard fix. This is the same lever as findings 4 and 17 of the previous run's
   notes, in a different function: **a local's mere existence changes the register count, and
   the count is what the frame size reports.**

6. **`Determinant`'s cofactor groups are not in textbook order, and only group 1 is wrong.**
   Group 1 is `m11 * f + m13 * d - m12 * e`, not `m11 * f - m12 * e + m13 * d`. Worth 22 points
   (55.42% -> 77.47% by objdiff's own measure; 59.09% -> 73.11% as report.json scores it).
   I swept **all four groups** over 18 reorderings each (6 permutations x 3 sign styles) and
   groups 2-4 are already at their best, so the textbook spelling is correct for them. I also
   swept the six **minor declarations** over 7 orders (`abcdef`, `cbadfe`, `aefdcb`, `cafebd`,
   `fabcde`, `acbedf`, `cbdaef`): 55.30-57.51%, i.e. **the declaration order does not matter**,
   because the expressions are inlined and ordered by the return expression. Do not spend
   another run on declaration order.

7. **The 73.11% `Determinant` is kept, unlike the previous run's 73.1%.** It is not a match and
   the diff is not pure churn: it removes a real mismatch (retail evaluates group 1 as
   `fmadds`/`fmsubs` in an order the textbook spelling cannot produce) and no function got worse
   - report.json's fuzzy went **up** for the unit and the global fuzzy went up. The previous run
   declined the identical number as "pure churn"; I think that was the right call at the time
   and the wrong call now only in that the reasoning was about the diff, not the number. Flagging
   it for the reviewer either way: **this is a partial improvement, not a matched function.**

## Where I stopped, and the evidence

- **`GetInverse` (1004 B, 38.10%) is a register-allocation wall.** Retail keeps 17 FP registers
  live and spills 9 pairs; we keep fewer and the whole body permutes. It is the same code as
  `Determinant` (same 18 minors, same `invDet` scale, same `bl Determinant` and
  `bl __ct__9CMatrix4fF...`), so the group-1 finding is in scope, but it does not reach it.
  Measured and **rejected**: applying the group-1 reordering (42.98% for the "divide" variant,
  31.44% for the multiply trailing form, which does not compile), and hoisting all 16 output
  cells into named locals `c0..c15` (31.44% - **identical** to inline, so hoisting changes
  nothing here, unlike finding 5). What is left is genuinely different codegen, not a spelling.
  This is the obvious next target on this unit and I do not have a lead on it.

- **`ValidateCameraTransform` (740 B, 4.76%), `GetScanObjectIndicatorPosition` (252 B, 5.40%),
  `ClearFluidList` (292 B, 10.90%)** are unchanged real TODO stubs, exactly as the previous run
  found. `ValidateCameraTransform` still carries the "TODO: Recover orthonormalization" comment.
  Recovering them is new work with no donor, and filling them with plausible bodies is what the
  brief forbids. Prime 1 (`prime-ref/`) has **no** `CMatrix4f::GetInverse` or `::Determinant`
  either - I checked `prime-ref/src/Kyoto/Math/CMatrix4f.cpp` and there is no donor for either.

- **`CGameCamera`'s constructor is at 94.54%, 6 instructions short**, and all 6 are `sdata`
  relocations (`lbl_8041CDE8` = 0.f, `lbl_80418650`, `CModelDataNull__10CModelDataFv`,
  `kInvalidUniqueId`) that we fill from a different address. Masking the reloc field, the
  constructor is **0 instructions different** in 110 (`tools/bytescmp.py` with the reloc field
  masked agrees). Those are pool addresses outside this unit's range and cannot be moved from
  here; 92.64% -> 94.54% was the only part available.

## Verification

```
sha1sum build/G2ME01/main.dol              -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
python3 tools/check_symbol_names.py        -> checked 525 units; 0 declared names are missing
python3 tools/check_decl_order.py --unit MetroidPrime/Cameras/CGameCamera
                                           -> ok: none emits its functions out of retail order
./tools/goal_check.sh build/goal/item.json -> goal_check: PASS progress-unit-cgamecamera
                                                ok  counts: matched 12173 -> 12177  linked 5860 -> 5860
                                                ok  target rose: CGameCamera: 25 -> 29 / 35
                                                ok  no asm added
```

`check_decl_order` matters here and it is worth saying why: three new definitions
(`fn_801B19F8`, `fn_801B19D8`, `CMatrix4f`'s copy ctor) were added to a unit whose functions
must be declared **descending by retail offset**, and the two SFovInterpolation bodies sit at the
*end* of retail's `.text` while their call sites are at the front. Getting that backwards
permutes the object with objdiff still at 100%.

## Notes for the next run

- **`tools/fast_try.sh <unit>` does work on this unit** - the previous run's note that it does
  not is wrong, or was wrong for that tree. `fast_try.sh MetroidPrime/Cameras/CGameCamera`
  rebuilds and re-reports in **under a second** when nothing changed. The target is
  `build/G2ME01/src/.../CGameCamera.o` and fast_try already uses exactly that name.
- **A scratch harness is still worth building.** `build/tools/objdiff-cli diff -1 <ours> -2
  build/G2ME01/obj/.../CGameCamera.o -o - --format json` gives per-function `match_percent` and
  per-instruction `diff_kind`, and lets you split the differences into *with-relocation* and
  *without*. That split is the most useful signal in this whole problem: at 99.68% `UpdatePerspective`
  looked like a near-miss, and the 6 remaining entries were **all** relocation-target names, i.e.
  the function was byte-identical. It turns "99.68% so close" into "0 real differences, and here
  is exactly what the 6 were".
- **Watch out for a greedy regex when scripting a sweep.** Two of my sweep scripts silently
  replaced the wrong span (a `.*?0\.00001f.*?` that matched the *first* `else if`, and a
  final write that truncated the file to 9 lines). Both produced plausible-looking numbers
  (0.0000% across the board) rather than an error. **Restore the file from a saved copy and
  re-measure after any scripted sweep**, and treat "every variant scored the same" as a script
  bug until proven otherwise.
- `tools/bytescmp.py <obj> <needle>` counts relocations as differences. For this unit that
  overstates every number by ~6 instructions. Mask the reloc field (keep the opcode and the
  non-relocated operands) before believing a "N instructions differ" figure.

WALL: CMatrix4f::GetInverse 38.10% - register allocation, not spelling; 17 FP registers live in retail vs fewer in ours, and hoisting all 16 output cells into named locals is byte-for-byte identical to inlining them (31.44% both), so the next attempt needs a different structural idea, not a reordering.
WALL: CMatrix4f::Determinant 73.11% - all four cofactor groups swept over 18 reorderings each (groups 2-4 are already optimal) and the six minor declarations over 7 orders (no effect); the residual is which minor lands in which register, not the expression.

---

# Third run (lane 8, 2026-10-02)

Re-measured on the clean tree: **29/35**, i.e. the previous run's 25 -> 29 had already landed, so
`item.json`'s "25/35" was stale. **Result: 29 -> 31 / 35 matched functions**, unit fuzzy
64.19% -> 83.27%, matched code 43.63% -> 53.70%. Global matched 12213 -> 12215.
`./tools/goal_check.sh build/goal/item.json` -> **PASS**.

One file touched, `src/MetroidPrime/Cameras/CGameCamera.cpp`, no `asm`, no header change in the
end (see finding 4 - the header experiment was reverted).

## What I changed

| function | before | after | what made the difference |
|---|---|---|---|
| `ClearFluidList(CStateManager&)` | 10.90 | **100.00** | recovered body; **`int` loop index**, not `uint` (finding 1) |
| `GetScanObjectIndicatorPosition(const CStateManager&) const` | 5.40 | **100.00** | recovered body; the manager field is `mBallCamera` (findings 2-4) |
| `ValidateCameraTransform(const CTransform4f&, const CTransform4f&)` | 4.76 | **76.65** | full body recovered from the object; the frame/local allocation still differs |

Both 100%s are real matches (objdiff, 252 and 292 bytes). The `ValidateCameraTransform` body is a
genuine recovery - every callee is a plain `bl` - but it is **not** a match and is flagged as such
in a comment in the source.

## Findings that generalise

1. **`uint` vs `int` on a vector index changes MWCC's induction-variable split.** Retail's
   `ClearFluidList` walks a stack copy with a running pointer *and* a separate index
   (`lhz r0,0(r30)`; `addi r30,r30,2`; `addi r29,r29,1`; `cmpw r29,r31`; `blt`). With
   `for (uint i = 0; i < v.size(); ++i)` MWCC emits `lhzx r0,base,byteoffset` plus *two* counters
   and never strength-reduces: **89.40%**. With `int i` the same source is **100.00%**. Swept, all
   one run:

   | spelling | score |
   |---|---|
   | `uint i = 0; i < fluids.size(); ++i` | 89.40 |
   | `uint i = 0, n = fluids.size(); i < n; ++i` | 89.53 |
   | `uint i = 0; i < fluids.mCount; ++i` | 89.40 |
   | `int i = 0; i < fluids.size(); ++i` | **100.00** |
   | `int i = 0; i < fluids.size(); i++` | **100.00** |
   | `int i = 0, n = fluids.size(); i < n; ++i` | **100.00** |
   | `uint i = 0; i < fluids.size(); ++i` with `fluids.data()[i]` | 99.18 |
   | `const TUniqueId* it = v.begin(); it != v.end(); ++it` | 80.77 |
   | iterator + index together (`++it, ++i`) | 99.18 |

   **`v.mCount` and `v.size()` are identical here, so the type of the index is the whole
   difference.** Worth trying on any loop that is a few percent short with an `lhzx`.

2. **A `const` copy of a `rstl::reserved_vector` is what forces retail's inlined copy
   constructor.** Retail's `ClearFluidList` copies the fluid list onto the stack (an
   eight-halfword unrolled body, `srwi.`/`mtctr`/`bdnz`) and then walks the *copy*. The source is a
   `const` local initialised from `GetFluidList()`; iterating `GetFluidList()` directly emits no
   copy at all. Range-based `for` is not available - **MWCC 2.7 rejects it outright**
   (`Error: '(' expected` on `for (TUniqueId id : fluids)`), which is why the index loop is the
   only spelling to try; `grep -rn ': [a-z]*)' src` finds no range-for in the whole tree.

3. **The two `sth r0,8(r1)` / `sth r0,12(r1)` in `ClearFluidList` are not a mystery.** They are
   `this+8` = `CEntity::mUniqueId`, stored once for the by-value `TUniqueId` returned by
   `GetUniqueId()` and once for the by-value `TUniqueId` argument of
   `CScriptTrigger::RemoveInhabitant`. Writing `water->RemoveInhabitant(GetUniqueId(), mgr)`
   unchanged reproduces both.

4. **`rstl::vector` is 16 bytes, not 12 - `rmemory_allocator` occupies a word.** This is what
   makes `CCameraManager`'s field at 0x1C be **`mBallCamera`**, and it is what
   `GetScanObjectIndicatorPosition` dispatches on. Walking the ABI settled the whole function:

   - `CVector3f` is returned through a **hidden pointer in r3** for a member function - proved by
     `CGameCamera::ConvertToWorldSpace` (matched, 100%): `mr r29,r3` then `stfs f0,0(r29)`. So in
     `GetScanObjectIndicatorPosition` r3 = return slot, r4 = `this`, r5 = the manager.
   - The three exits are `lwz r4,<obj>` ; `lwz r12,0(r4)` ; `lwz r12,92(r12)` ; `bctrl` with
     (r3 = return slot, r4 = obj, r5 = mgr). Offset 0x5C is `GetScanObjectIndicatorPosition` in
     **both** `CActor`'s and `CGameCamera`'s tables (`objdump -r -j .data` of either retail object
     puts it at vtable offset 0x5C, and the vptr is `&__vt__[0]`), so the callee really is
     `obj->GetScanObjectIndicatorPosition(mgr)`.
   - The watched object is `mWatchedObject`: `CActor` is `CHECK_SIZEOF(CActor, 0x158)` and retail
     loads `lhz 344(r3)` - the first field past `CActor`.
   - Naming `CCameraManager`'s 0x1C slot a `CGameCamera*` gave **99.97%**, one `lwz` off
     (`32(r4)` vs `28(r4)`); `BallCamera()` is **100.00%**.

5. **`CameraManager(m)->x` does not compile in MWCC 2.7.** `CameraManager(m).x` does. The same
   expression with `->` fails with `pointer/array required` *pointing at the `(` of
   `CameraManager(`*, which reads like a bad declaration. Every other camera TU spells it with a
   dot (`CameraManager(mgr).UpdateCameraTriggers(...)`), which is probably why nobody has hit it.
   Two other MWCC 2.7 limits hit in the same function: an inline member body **cannot read a member
   declared later in the class** ("pointer/array required" again - define it after the class), and
   calling a member on a pointer to a **forward-declared** class needs the full definition in this
   TU (`illegal use of incomplete struct/union/class 'CBallCamera'`).

6. **`!(a < eps) || !(b < eps) || !(c < eps)` is the spelling retail's branch shape implies.**
   Retail emits `bge, bge, blt` for one three-term test. Three `>=` give `bge, bge, bge`. Written
   as a negated `&&` chain (`!(a && b && c)`) the first two are "branch to the body when not-less"
   and the last is "branch past the body when less", which is exactly retail's shape. Measured in
   `ValidateCameraTransform`: `>= 2.f` cost 3 points (73.76% vs 76.65%).

7. **`CMath::Limit` is `AbsF(v) > h ? h * Sign(v) : v`, and retail's `fsel` + `fmuls` is exactly
   that**, with `f0` still holding `h`. Reading `fsel f1,f2,f0,f1` backwards is the trap: objdump
   prints `fD, fA, fB, fC`, and `CMath::FastFSel`'s own inline asm is `fsel out, v, h, l`, so
   **the condition is the *second* printed operand** and the two results are the third and fourth.
   `CMath.hpp` already had `Limit`; nothing had to be added.

## Where I stopped, and the evidence

- **`ValidateCameraTransform`, 76.65% - the body is recovered, the stack frame is not.** Retail's
  frame is 0x120 bytes holding **nine** 12-byte vector slots (8, 20, 32, 44, 56, 68, 80, 92, 104),
  two `CTransform4f` temporaries (116, 164) and `xf` at 212; ours is 0x100 with **seven** slots, the
  temporaries at 92 and 140 and `xf` at 188. Every stack-offset instruction therefore differs.
  Retail materialises two `CVector3f` temporaries that are **written and never read** - `up`
  (built at 0x38 from `m20, m21, m22`, only `.z` is tested) and `right` (built at 0x20 from
  `m00, m10, m20`, only `.z` is tested) - and our compiler forwards both constructions away, which
  is where the two missing slots go. Nothing tried this run makes them materialise:

  | spelling | score | frame |
  |---|---|---|
  | three named `const CVector3f` locals in block 1 | 63.55 | 256 |
  | non-const `up` / `right` | 76.65 | 256 |
  | `const CVector3f flat = ...DropZ()` (no dead store) | 75.72 | 272 |
  | `CTransform4f look = ...LookAt(...); xf = look;` per block | 77.63 | 352 |
  | `xf.m03 = newXf.m03;` etc. before the return (retail does three `stfs`) | 75.72 | 272 |
  | `- 0.f` on both block-4 tests (retail emits `fsubs` against 0.f) | 76.65 | 256 |

  Also decoded but not reproduced: retail returns
  `CTransform4f(xf.m00 ... xf.m22, newXf.m03, newXf.m13, newXf.m23)` - it re-stores `newXf`'s
  translation into the local's translation slots immediately before the returning copy-construct,
  which is not what `return xf;` does here.

- **`CMatrix4f::Determinant` 73.11% and `CMatrix4f::GetInverse` 38.10% are untouched.** Both were
  WALLed by the second run (all four cofactor groups swept over 18 reorderings each, the six minor
  declarations over 7 orders; `GetInverse`'s 16 output cells hoisted into named locals is
  byte-for-byte identical to inlining them). Re-reading `Determinant`'s object this run shows the
  residual is a straight register-allocation permutation of the same 79 instructions - every
  `fmsubs`/`fmuls` is present in both, only the FPR assignment differs - so the two old `WALL:`
  lines stand and I add nothing to them.

- **`CGameCamera`'s constructor is still 94.54%** with all six remaining differences being `sdata`
  relocation targets outside this unit's range, as the second run measured. Not reachable from
  here.

## Verification

```
sha1sum build/G2ME01/main.dol                    -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/probe_sources.sh                         -> 751 files, 0 failed; LINKED (288 undefined, 0 dup)
python3 tools/check_symbol_names.py              -> checked 525 units; 0 declared names are missing
python3 tools/check_decl_order.py --unit MetroidPrime/Cameras/CGameCamera
                                                -> ok: none emits its functions out of retail order
./tools/goal_check.sh build/goal/item.json      -> goal_check: PASS progress-unit-cgamecamera
                                                   ok  counts: matched 12213 -> 12215  linked 5860 -> 5860
                                                   ok  target rose: CGameCamera: 29 -> 31 / 35
                                                   ok  no asm added
```

`tools/unit_fit.sh MetroidPrime/Cameras/CGameCamera.cpp`: `.text` 4216 -> **5388** of retail's 5400
(SHORT by 12, was SHORT by 1184), `.data` 140 of 144 (unchanged). One new extra symbol,
`__dt__Q24rstl29reserved_vector<9TUniqueId,4>Fv` (60 B), from the `ClearFluidList` copy - a weak
COMDAT both linkers discard; the two that were already there (`__dt__16CActorParametersFv`,
`GetHealthInfo__6CActorCFv`) are unchanged. The DOL sha1 is byte-identical to before the change,
which is what a correct change to an already-linked unit looks like.

`docs/HANDOFF.md`'s state block was rewritten by `check_docs_claims.py` during the build (matched
12213 -> 12215, DOL units 10665 -> 10667). Nothing in it was hand-edited.

## Notes for the next run

- **`tools/fast_try.sh MetroidPrime/Cameras/CGameCamera` is the loop** - a clean rebuild plus a
  per-function report in well under a second. `.tmp/opencode/try.sh` wraps it and
  `.tmp/opencode/insndiff.py <fn-substring>` prints an aligned instruction-level diff of one
  function against retail; both are under the gitignored `.tmp/`, so they never reach the diff.
- **Read the pool constants out of the retail DOL, not from memory.** `tools/dol_read.py <addr>
  <len> build/G2ME01/main.dol` prints `f32`, which is what turned `1.f / 1e-5f / -1.f / 0.999f /
  0.01f / 2.f` into recognisable expressions instead of six mystery `lbl_8041CDFx`s.
- **`rstl::vector` at 16 bytes will bite again.** Any field-offset argument over a `CCameraManager`
  or `CActor` member needs the allocator member counted; the header's `CHECK_SIZEOF` only gives the
  total.
- Only four functions are left on this unit: the constructor (6 `sdata` relocation differences,
  unreachable), `Determinant` and `GetInverse` (both register-allocation walls, two previous runs),
  and `ValidateCameraTransform` (frame allocation, above). A run that wants this unit's last
  matched function should start from the frame, not the expressions.

## Review rejected run 24 (2026-10-01 23:40:31Z, reviewer worker)

The judge passed this attempt; the reviewer rejected it:

The two 100% matches (`ClearFluidList`, `GetScanObjectIndicatorPosition`) are real and verified against the retail object, but the third change replaces a live-port stub with a body that provably mis-passes two values: at `CGameCamera.cpp:229,232` retail's `IsMagnitudeSafe`/`LookAt` take the block-3 `flat` vector (`r3 = r5 = r1+104`, written only at 0x801b0d20–0x28), not `up2` (stored dead at r1+20/24/28), and at `:235` retail re-stores `newXf`'s m03/m13/m23 into the local (0x801b0e44–0x801b0e60) before the returning copy-construct, which `return xf;` drops — so `CFirstPersonCamera.cpp:135` and `CInterpolationCamera.cpp:294` would receive a transform whose translation is (0,0,0) whenever the LookAt branch fires. An acceptable change keeps the two 100% bodies exactly as they are and either spells the last block as `flat.IsMagnitudeSafe()` / `LookAt(Zero, flat, Up)` with `xf.m03 = newXf.m03; xf.m13 = newXf.m13; xf.m23 = newXf.m23;` before the return, or reverts `ValidateCameraTransform` to its old stub body and notes the two deviations for the next attempt.

Rejected diff: /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal/build/goal/review/progress-unit-cgamecamera-L8-24.patch
Review transcript: /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal/build/goal/agent/progress-unit-cgamecamera-L8-24-review1-20261001T233348.jsonl

## Fix round 1 (worker, 2026-10-02)

Both objections to `ValidateCameraTransform` were real; I re-derived both from the retail object
(`build/G2ME01/obj/MetroidPrime/Cameras/CGameCamera.o`) before changing anything, and made only
those two changes. The two 100% bodies are byte-identical to what was reviewed.

1. **The last block takes `flat`, not `up2`.** Retail's `IsMagnitudeSafe` at 0x8f0 is reached with
   `r3 = r1+104` and its `LookAt` at 0x920 with `r5 = r1+104` - the same slot, and r1+104 is the one
   block 3 built `flat` in at 0x818-0x828. The (m02, m12, m22) triple that retail stores at
   r1+20/24/28 before the second `fcmpo` is written and never read again, so `up2` is dead except for
   its own `.z` test. Ours now passes `flat` to both callees; our object emits `r3 = r1+68` and
   `r5 = r1+68`, the same slot block 3 wrote.
2. **The tail re-stores `newXf`'s translation.** 0x944-0x960 does `lfs f2,44(r30)` /
   `lfs f1,28(r30)` / `lfs f0,12(r30)` (r30 = `newXf`, so m23/m13/m03) and stores them into the
   local's r1+0x100/0xf0/0xe0 before the returning copy-construct at 0x964 - so the returned
   translation is `newXf`'s whichever branch fired, which is what stops `CFirstPersonCamera.cpp:135`
   and `CInterpolationCamera.cpp:294` from seeing (0,0,0) when a `LookAt` branch runs.
   **`CTransform4f::m03/m13/m23` are `private`**, so the reviewer's `xf.m03 = newXf.m03;` spelling
   does not compile ("illegal access to protected/private member"); the compiling equivalent is
   `xf.SetTranslation(newXf.GetTranslation())`, which emits the same three loads from `r30` and
   three stores into the local (verified in our object at 0x96c-0x980).

**Measured**

- `ValidateCameraTransform` **76.65% -> 79.87%** (`tools/fast_try.sh`, `build/report.json`); the
  frame also shrank 0x100 -> 0xf0, because block 4 no longer needs a vector of its own.
- The unit's matched count is unchanged and still the two new ones: `report.json` shows
  `31/35`, with only the constructor (94.54%), `Determinant` (73.11%), `GetInverse` (38.10%) and
  `ValidateCameraTransform` below 100% - `ClearFluidList` and `GetScanObjectIndicatorPosition` are
  still at 100%.
- `./tools/goal_check.sh build/goal/item.json` -> **PASS progress-unit-cgamecamera**;
  `ok counts: matched 12213 -> 12215`, `ok target rose: main/MetroidPrime/Cameras/CGameCamera:
  29 -> 31 / 35`, `ok no asm added`, and gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs
  claims, port probe) clean.
- `python3 tools/check_raw_offsets.py` -> `ok: 167 raw-offset site(s) in 71 file(s), all documented
  in raw_offsets.md`.

Still 4 functions short of a flip, and `ValidateCameraTransform` is still short only of the frame
allocation (retail's nine 12-byte vector slots versus our seven); see "Where I stopped" above.

---

# Fourth run (lane 2, 2026-10-02)

Re-measured on the clean tree: **31/35**, i.e. the third run's 29 -> 31 plus its fix round had both
landed (`ValidateCameraTransform` was at 79.87%). **Result: 31 -> 32 / 35 matched functions**,
`ValidateCameraTransform` **79.87% -> 100.00%** (740 B, byte-identical), unit fuzzy 83.71% ->
86.47%, matched code 2900 -> 3640 of 5400. Global matched 12286 -> 12287.
`./tools/goal_check.sh build/goal/item.json` -> **PASS**.

One file touched, `src/MetroidPrime/Cameras/CGameCamera.cpp`, plus two new `#include`s in it
(`Kyoto/Math/CloseEnough.hpp` and `"float.h"`, exactly what Prime 1's `CGameCamera.cpp` includes).
No header change, no `asm`. `docs/HANDOFF.md`'s state block was rewritten by
`check_docs_claims.py` during the build (12286 -> 12287, DOL units 10738 -> 10739); nothing in it
was hand-edited.

**The previous three runs' "frame allocation" blocker is gone.** Retail's 0x120-byte frame with
nine 12-byte vector slots is reproduced exactly; it was never a frame-size problem, it was a
*spelling* problem that showed up as one.

## What made the difference, in order of what it was worth

1. **Chain the accessor; do not bind it to a named `const CVector3f`.** 79.87% -> **86.90%**, and
   the frame 0xf0 -> **0x120**, retail's exact size. Retail stores three `CVector3f` locals that
   are written and **never read again** (`up` at r1+56, `right` at r1+32, `up2` at r1+20 - counted
   by grepping every `N(r1)` reference in the retail disassembly, one `stfs` per component and no
   load). `const CVector3f up = xf.GetUp(); ... up.GetZ()` gives MWCC nothing to keep and it
   forwards the whole vector away; `xf.GetUp().GetZ()` is a *temporary*, whose slot it keeps, so
   all three dead stores appear and every stack offset lines up. Measured, all in one sweep:

   | spelling | score | frame |
   |---|---|---|
   | `const CVector3f up/right/up2` locals + `.GetZ()` (what the tree had) | 79.87 | 0xf0 |
   | non-const `CVector3f` locals + `.GetZ()` | 79.87 | 0xf0 |
   | `const` locals + `up[kDZ]` (const `operator[]`) | 79.87 | 0xf0 |
   | non-const locals + `up[kDZ]` (address-taken `operator[]`) | 79.87 | 0xf0 |
   | `const CVector3f& upRef = up;` to force the address to escape | 82.92 | 0x100 |
   | chained `xf.GetUp().GetZ()`, one local (`up`) still named | 83.69 | 0x110 |
   | **chained `xf.GetUp().GetZ()` / `xf.GetRight().GetZ()`, no locals** | **86.90** | **0x120** |

   Nothing to do with `operator[]`: MWCC inlines it either way. The lever is *temporary vs named
   local*, nothing else.

2. **Both `bool` tests want the condition positive.** 86.90% -> **95.75%**. Retail emits
   `clrlwi. r0,r3,24 ; beq <xf = oldXf>`, i.e. it branches *away* from the `xf = oldXf` copy and
   falls through into the `LookAt`. We emitted `bne <LookAt>`, i.e. the negation-first spelling
   `if (!flat.CanBeNormalized()) { xf = oldXf; } else { LookAt }`. Both are the same program, but
   MWCC's block layout follows the source order, and every address inside the two blocks moves
   when the order does. Written as `if (flat.CanBeNormalized()) { LookAt } else { xf = oldXf; }`,
   once in block 3 (91.34%) and once in block 4 (91.30%). Swapping the *blocks* instead is wrong:
   block 3 before block 4 scores 100.00%, the reverse 80.86%.

3. **`close_enough` keeps the `x - 0.f` that MWCC otherwise folds.** 95.75% -> **100.00%**.
   Retail's block 4 is `AbsF(right.GetZ() - 0.f) < 0.01f` and does emit `fsubs f0,f1,f2` with
   `f2 = lbl_8041CDE8 = 0.f`. Written as `CMath::AbsF(xf.GetRight().GetZ() - 0.f) < 0.01f` MWCC
   **folds the subtraction away** - no `fsubs`, identical bytes to the version without it, and the
   same 95.75% (verified by disassembling, not by score). Called as
   `!close_enough(xf.GetRight().GetZ(), 0.f, 0.01f) && close_enough(xf.GetUp().GetZ(), 0.f, 0.01f)`
   - i.e. `AbsF(a - b) < eps` from `include/Kyoto/Math/CloseEnough.hpp:27` - MWCC inlines the
   header body **late enough that the literal `0.f` is no longer folded**. This is the general
   lesson: *an inline wrapper can protect an operation the optimizer would fold in your own
   expression*, and `close_enough` is exactly the wrapper retail's source used, so reaching for it
   is not a workaround but the reconstruction.

   Measured equivalent: `close_enough(x, 0.f, 0.01f)` and `close_enough(x, 0.f)` both give
   100.00% (the epsilon is an unrelocated pool constant, so objdiff cannot tell them apart - I kept
   the explicit `0.01f` because that is the value retail actually compares against). Block 1 is
   unaffected either way: `close_enough(mag, 1.f, FLT_EPSILON * 1000.f)`,
   `close_enough(mag, 1.f, 0.0001192093f)`, `close_enough(mag, 1.f)` and the hand-written
   `CMath::AbsF(mag - 1.f) < 1.19209e-4f` all give 100.00%; I kept `close_enough` with
   `FLT_EPSILON * 1000.f` because that is retail's literal.

## Two *bugs* the previous runs shipped

**The source comment's constant table was wrong, and so was the code built from it.** Three runs
had "lbl_8041CDEC = 1.f, lbl_8041CDF4 = 1e-5f, lbl_8041CDF8 = -1.f, lbl_8041CDFC = 0.999f,
lbl_8041CE00 = 0.01f and lbl_8041CE04 = 2.f" - a pool label run off by one after CDFC. Read out of
the DOL with `tools/dol_read.py`, the truth is:

```
8041CDE8  0x00000000  0.f
8041CDEC  0x3f800000  1.f
8041CDF4  0x38fa0000  0.0001192093      <- FLT_EPSILON * 1000.f, not 1e-5f
8041CDF8  0xbf800000 -1.f
8041CDFC  0x3f7fbe77  0.999
8041CE00  0xbe4ccccd -0.2               <- not 0.01f
8041CE04  0x3c23d70a  0.01              <- not 2.f
8041CE08  0x40000000  2.f
```

So the port was running `if (up.GetZ() < 0.01f)` where retail runs `< -0.2f`, and
`AbsF(right.GetZ()) < 2.f` where retail runs `AbsF(right.GetZ() - 0.f) < 0.01f`. objdiff cannot
see any of that - the constants are unrelocated pool references, so every one of those spellings
scored the same. Prime 1's `prime-ref/src/MetroidPrime/Cameras/CGameCamera.cpp:594-628` is the
donor and it agrees with the DOL exactly (`xfCpy.GetUp()[kDZ] < -0.2f`,
`close_enough(..., FLT_EPSILON * 1000.f)`); the previous runs had the donor available and did not
read it. **The block-4 dead `up2` triple is real, not the mis-pass the reviewer caught** - the
object now passes `r3 = r1+104` and `r5 = r1+104` (block 3's `flat`) to `IsMagnitudeSafe` and
`LookAt`, and still re-stores `newXf`'s m03/m13/m23 before the returning copy-construct, so
`CFirstPersonCamera.cpp:135` and `CInterpolationCamera.cpp:294` cannot see (0,0,0).

**STALE: the third run's "Retail materialises two `CVector3f` temporaries our compiler forwards
away... nothing tried this run makes them materialise" is not a wall, it is the chained-call
spelling above.** Same for the second run's frame notes. Two runs of frame experiments were spent
because nobody tried removing the locals.

## Where the unit still stands (measured this run, no spellings tried on these)

- **`CMatrix4f::Determinant` 73.11%, `CMatrix4f::GetInverse` 38.10%** - the two previous runs'
  `WALL:` lines are about register allocation of the 18 minors and are *not* contradicted by
  anything I measured; I only re-read their scores. Do not spend another run on the cofactor
  ordering; if someone picks these up, the untried idea is the same one that just worked here -
  change the *shape of the expression* (which value lives where, what is a named local and what is
  a temporary), not the order of the terms.
- **`CGameCamera`'s constructor 94.54%** - unchanged. `objdiff diff` (relocation-aware) says the
  whole constructor is byte-identical; the six differences `report.json` counts are resolved
  `sdata`/pool addresses outside this unit, so it is a link-layout artefact, not a source problem.
  **That is a measurement, not a wall** - I did not try to move the pools.

## Verification

```
sha1sum build/G2ME01/main.dol                    -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
python3 tools/check_symbol_names.py              -> checked 525 units; 0 declared names are missing
python3 tools/check_decl_order.py --unit MetroidPrime/Cameras/CGameCamera
                                                -> ok: none emits its functions out of retail order
python3 tools/check_docs_claims.py               -> docs claims agree with the tree
./tools/goal_check.sh build/goal/item.json      -> goal_check: PASS progress-unit-cgamecamera
                                                   ok  counts: matched 12286 -> 12287  linked 5863 -> 5863
                                                   ok  target rose: CGameCamera: 31 -> 32 / 35
                                                   ok  no asm added
```

`tools/unit_fit.sh MetroidPrime/Cameras/CGameCamera.cpp`: `.text` 5392 -> **5464** against retail's
5400 claim - the 32 matched functions now fill the claim exactly (our `ValidateCameraTransform` is
0x708..0x9ec = 740 B, retail's 740) and the 64 bytes over are the same three weak COMDAT copies
that were there before (`__dt__16CActorParametersFv`, `__dt__reserved_vector<9TUniqueId,4>`,
`GetHealthInfo__6CActorCFv`), which both linkers discard. `.data` 140 of 144, unchanged.

## Notes for the next run

- **Read the pool constants out of the DOL before writing them down, and read Prime 1's source
  before writing them down.** Both were available; the constant table was wrong for three runs and
  cost a semantic bug that objdiff scored identically at every value.
- **Two MWCC rules this run paid for, both general:**
  1. *a named `const CVector3f` local whose only use is `.GetZ()` is optimized to nothing; the same
     access chained onto the call that produced it keeps its stack slot.* Worth trying on any
     function whose frame is short of retail's by whole 12- or 16-byte slots.
  2. *`if (!cond) A else B` and `if (cond) B else A` are different code*, because MWCC lays the
     blocks out in source order. Read the branch polarity off the object (`beq` to the `else` body
     vs `bne` to the `then` body) instead of assuming.
  3. *an inline wrapper can save an operation from constant folding.* `close_enough(a, 0.f, eps)`
     keeps `a - 0.f`; `CMath::AbsF(a - 0.f)` does not.
- **`.tmp/opencode/l2_try.py` + `l2_score.sh`** (gitignored) are this unit's harness: `l2_try.py
  <variants.py>` swaps the whole function body for each spelling, rebuilds one object, prints
  score + frame size + an object hash, and restores the file. **Print the object hash** - during
  this run two "identical" variants turned out to have different hashes, and the frame/score alone
  cannot tell a real change from a no-op.
- Only three functions are left on this unit: the constructor (pool-address artefact in
  `report.json`), `Determinant` and `GetInverse`. A run that wants 33/35 should start from the
  *expression shape* in `Determinant`, not from the cofactor order.
