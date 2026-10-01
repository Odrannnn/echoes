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