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
