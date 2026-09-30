# progress-prime1-cguiobject — `GuiSys/CGuiObject` 15/21 -> 18/21 matched (2026-09-30)

**Result: 3 new exactly-matched functions, no function anywhere got worse, no `asm`.**
The unit stays `NonMatching` (it is far too big to flip: `unit_fit.sh` reports `.text` SHORT by
904 bytes), so `flip_test.sh` was not run and is not the criterion here.

Measured, not recalled (`build/report.json`, after the change):

```
main/GuiSys/CGuiObject: 68.14893% fuzzy, 18 / 21 functions
All:  30.74% fuzzy, 22.93% matched, 11.74% linked (9983 / 28465 functions)
```

Baseline re-measured by reverting the file and rebuilding: `30.73% ... 9980 / 28465`, unit 15/21.
`build/G2ME01/main.dol` is `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` before and after.
`probe_sources.sh` -> `749 files, 0 failed, 0 errors`; `check_symbol_names.py` -> `0 missing`;
`unit_fit.sh` -> `no extra functions`. Only `src/GuiSys/CGuiObject.cpp` changed.

The class layout is **not** Prime 1's and must not be made so: Echoes' `CGuiObject` is 0x74 with a
vtable (4) + `mLocalXF` (0x04) + `mWorldXF` (0x34) + `mWorldTransformValid` (0x64) + `mParent`
(0x68) + `mChild` (0x6C) + `mNextSibling` (0x70). Prime 1 has no vtable and no validity flag; its
`RecalculateTransforms` is eager, Echoes' is lazy. **Echoes' `CGuiObject` is not a rename of Prime
1's engine code** — Prime 1's source is a good guide for *spelling*, not for *structure*.

## Per function: before %, after %, and what Prime 1's source did

| function | before | after | Prime 1's source |
|---|---|---|---|
| `RotateW2O` | 72.28% | **100.00%** | Prime 1 has no `RotateW2O`; the body is a 1-liner here already. Needed only a named local (below). |
| `RotateTranslateW2O` | 80.68% | **100.00%** | matched almost unchanged once `StupidSubtract` was inlined by hand and the result bound to a named local. |
| `SetO2WTransform` | 96.72% | **100.00%** | matched unchanged except for binding the parent transform to a named `const CTransform4f&`. |
| `AddChildObject` | 66.74% | 81.71% | Prime 1's structure is right and lifts the unit; its last two lines still do not match (see below). |
| `GetWorldTransform` | 14.04% | 14.04% | not attempted - retail inlines its own recursion 10 deep. |
| `RecalculateTransforms` | 23.33% | 23.33% | not attempted - Prime 1's eager version is **wrong** here; retail inlines its own recursion 8 deep. |

### The one rule that covers three of them: MWCC keeps a named local's frame slot

`RotateW2O` and `RotateTranslateW2O` return a 12-byte `CVector3f`, which both compilers return
through a hidden sret pointer in `r3`. `return <call returning CVector3f>;` lets MWCC pass the
*sret slot itself* straight down as the callee's sret, so the frame is one temp smaller and the
return is a tail store. **Retail always materialises the callee's result in a stack temp and then
copies `f0/f1/f2` into the sret**, so the source has to name the local:

```cpp
CVector3f CGuiObject::RotateW2O(const CVector3f& vec) const {
  const CVector3f result = GetWorldTransform().TransposeRotate(vec);
  return result;                      // 72.28% -> 100.00%
}
```

Same for `RotateTranslateW2O`: the `CVector3f` from `TransposeRotate` must be a named local at
`r1+20` while the subtraction's temporary sits at `r1+8`, i.e. the subtraction must be an **unnamed**
full-expression temporary (created during codegen, so it takes the lower slot) and the result must
be a **named** local. That is why `delta` as a named local (99.78%, slots swapped) and `translation`
as a `const CVector3f&` (89.24%, a stack slot appears) are both wrong, and this is right:

```cpp
const CTransform4f& world = GetWorldTransform();
const CVector3f translation = world.GetTranslation();
const CVector3f result =
    world.TransposeRotate(CVector3f(vec.GetX() - translation.GetX(), vec.GetY() - translation.GetY(),
                                    vec.GetZ() - translation.GetZ()));
return result;
```

The other half of `RotateTranslateW2O`: MWCC emits a `CVector3f` constructor's arguments in
**reverse** source order, and the free `operator-` in `include/Kyoto/Math/CVector3f.hpp` (which has
three named `float` locals) emits `y, z, x` instead. The explicit three-argument constructor emits
`z, y, x`, which is what retail has. **So `a - b` and `CVector3f(a.GetX() - b.GetX(), a.GetY() -
b.GetY(), a.GetZ() - b.GetZ())` are not the same code here**, and the explicit form is the one that
matches. Prime 1's `StupidSubtract` free function is the right idea for the wrong reason.

`SetO2WTransform` was 4 bytes short and nothing else: retail has `mr r0,r3 ; mr r4,r0` between the
`GetWorldTransform` call and the `GetQuickInverse` call, we had `mr r4,r3`. Binding the call result
to a named `const CTransform4f&` produces the extra `mr r0` with no stack traffic (MWCC does not
spill reference locals). Chained `mParent->GetWorldTransform().GetQuickInverse()` does not. Note
this is the *same* source shape that `RotateW2O` has (`GetWorldTransform().TransposeRotate(vec)`,
one `mr`), so the difference is the named reference, not the chaining.

### `AddChildObject`: 66.74% -> 81.71%, and what is left

Prime 1's structure is right and is what retail does; only the tail differs. What landed:

* `CTransform4f worldLocalXf = CTransform4f::Identity();` - retail copy-constructs from
  `sIdentity__12CTransform4f` (0x804173D4) into a frame slot, which Prime 1's line 104 has and the
  two-argument `CTransform4f(rotation, position)` form here did not. The two-argument form also
  called `__ml__9CMatrix3fCFRC9CVector3f` **twice**; retail calls it once.
* `const CVector3f pos = tmpMtx * position;` then a **twelve-float** `CTransform4f` constructor
  assigned into `worldLocalXf` (retail's stores are the mtx's 9 floats in member order followed by
  `pos.x/y/z`, at `r1+260`, then `__as__12CTransform4f` into `r1+344`).
* `m0/m1/m2` as `const CVector3f&` locals, and `m2` **before** `m1` before `m0` - Prime 1's order,
  and retail's evaluation order (retail's first `fdivs` is `1.0f / |column 2|`). Writing them
  inline in the `CMatrix3f` arguments scores 81.61% instead of 81.71%: the same code, so either
  spelling is fine, but the named form is Prime 1's.

What is still open, with the evidence:

1. **Six `CVector3f` temporaries retail builds and we do not** (retail `r1+8..r1+76`, 18 `stfs`
   pairs, dead stores). They are the results of `tmpMtx.GetColumn(0/1/2)` read out of the matrix
   memory in the order `z, y, x` (row Z, row Y, row X) twice. Retail's twelve-float constructor then
   takes its 9 arguments **out of registers**, not out of those slots. Writing
   `tmpMtx.GetColumn(0).GetX(), ...` (Prime 1's exact spelling) makes MWCC emit **nine out-of-line
   calls** to `CMatrix3f::GetColumn(int)` - the header's `int` overload has a `switch` and
   `-inline deferred,noauto` will not inline it - and the function falls to **45.41%**. The
   `Get00()..Get22()` spelling above avoids the calls and is the best measured result.
2. **Every frame offset is shifted.** Retail's stack objects, highest first: `Identity` local 344,
   `tmpMtx` 308, the twelve-float ctor temp 260, the `CMatrix3f * CVector3f` result 212, `position`
   200, the three `Magnitude` argument temps 188/176/164, the six column temps 152..92, the six
   `GetColumn` temps 76..8. Ours has a different *set* of objects (no `GetColumn` temps, and ours
   puts the `__ml__` result at `r1+8`), and MWCC's offsets are not in source order (the matrix at
   308 is created after `position` at 200). **Do not spend a run on frame offsets alone** - the set
   of temporaries has to be right first, and item 1 is blocked.
3. Retail's twelve-argument load order is `m22, m12, m02, m21, m11, m01, m20, m10, m00` then
   `pos.z, pos.y, pos.x`; we load `pos` first and then read six arguments back out of our own
   ctor temporary. The `GetColumn` calls in item 1 are the likely cause of the different grouping.

## Not attempted, and why: the two recursive functions

`GetWorldTransform` (696 bytes, 14.04%) and `RecalculateTransforms` (288 bytes, 23.33%) are
**correct as written** - the source in the tree already reproduces retail's semantics exactly - and
what is missing is MWCC inlining each function into itself, which the flags do not do:

* Retail `RecalculateTransforms` sets `mWorldTransformValid = 0` then walks `mChild` **8 levels
  deep by hand** (each level: `stb 0,100(rX) ; lwz rY,108(rX) ; b test`), reaches level 9, and only
  there calls itself in a real `bl` inside the `mNextSibling` loop. Our build emits the loop with a
  `bl` and no inlining at all (72 bytes vs 288). That is a recursion unroll, and this unit is
  compiled `-O4,p -inline deferred,noauto -pragma "inline_max_size(125)"` (`configure.py`
  `cflags_retro`), which will not inline a 288-byte self-call.
* Retail `GetWorldTransform` walks `mParent` 10 levels deep the same way, with each level's
  `mWorldXF = parentWorld * mLocalXF` in its own 48-byte stack temp (`r1+8, 56, 104, ... 392`), and
  calls itself once at level 10. Same conclusion.

Both would have to be hand-written as 8- and 10-level nestings to be even in the right shape, and
MWCC's register assignment for them (`r22`-`r31` held live across the whole descent, and `r0` for
the zero flag in the first six levels but `r31` in the last three) is the unroller's, not the
source's. Prime 1's `RecalculateTransforms` is the **wrong algorithm** for Echoes (eager, no
validity flag) - do not port it.

WALL: CGuiObject::GetWorldTransform 14.04% - retail is a 10-deep hand-inlined walk of mParent; mwcceppc with -inline deferred,noauto does not unroll self-recursion, and the unroller's r23-r31 assignment is not reachable from source
WALL: CGuiObject::RecalculateTransforms 23.33% - same: retail inlines the self-call 8 deep before falling back to a real bl in the mNextSibling loop

## Codegen facts worth keeping (measured, not inferred)

* MWCC emits a `CVector3f`/`CTransform4f` constructor's arguments in **reverse** source order, and
  a constructor whose arguments are three named `float` locals (the `operator-` in
  `CVector3f.hpp`) emits `y, z, x` where a literal three-argument constructor emits `z, y, x`.
  Same values, different order, 80.68% -> 100.00%.
* A named `const CVector3f` local costs a frame slot and forces the callee's sret there; the same
  value as an unnamed full-expression temporary takes the lower slot instead. Two named locals are
  allocated in reverse declaration order. This is what decides `r1+8` vs `r1+20` in every
  `CVector3f`-returning function here.
* A named `const T&` local bound to a call result emits **no** stack traffic but does emit one
  extra `mr` (through `r0`) before the value is used as an argument.
* `compile_commands.json` in this tree is **stale and misleading**: it shows a `clang --target=
  powerpc-eabi` command, but the real rule in `build.ninja` is `mwcc_sjis` with
  `mwcceppc.exe GC/2.7`. Read `build.ninja`, not `compile_commands.json`, before reasoning about
  codegen.

## Not filed as `NEW:`

The two recursive functions are a measured wall, not a queueable target: with this unit's flags
mwcceppc will not unroll a self-call, so no source spelling reaches 100% and filing them would cost
a lane an hour for nothing. `AddChildObject`'s remaining gap needs a `CMatrix3f::GetColumn(int)`
that MWCC inlines, which is a header/flag question, not a source question for this unit - recorded
here instead.

---

# Second run: 18/21 -> **19/21** matched (2026-09-30, lane 5)

**Result: `RecalculateTransforms` reached 100.00%. `AddChildObject` 81.71% -> 99.15%,
`GetWorldTransform` 14.04% -> 79.17%. No function anywhere got worse, no `asm`, judge PASS.**

The previous run's `WALL:` lines on the two recursive functions were **wrong about the
mechanism**: retail's hand-unrolled descent is reachable from source, it just needs the
declaration marked `inline` and this unit's `inline_max_size` raised. Both `WALL:` lines are
withdrawn - see the measurements below.

Measured, not recalled (`build/report.json` after the change):

```
main/GuiSys/CGuiObject: 94.96% fuzzy, 51.60% matched code, 19 / 21 functions
All:  31.55% fuzzy, 24.08% matched, 11.83% linked (10388 / 28465 functions)
main.dol sha1 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010 (unchanged)
probe_sources.sh -> 752 files, 0 failed, 0 errors; check_symbol_names.py -> 0 missing
unit_fit.sh -> .text claimed 3008 ours 3008 retail 3008 "fits"; no extra functions
```

Baseline re-measured from the clean tree: `10387 / 28465`, unit 18/21, and `unit_fit.sh`
reported `.text` **SHORT by 904 bytes**. It now **fits** - that is the single clearest sign
the object is structurally right. (`.data` is still SHORT by 4, the 8-byte `sdata2` fits.)

`./tools/goal_check.sh build/goal/item.json` -> `PASS`, with
`ok target rose: main/GuiSys/CGuiObject: 18 -> 19 / 21 functions`.

## Files changed (only these two)

* `include/GuiSys/CGuiObject.hpp` - `inline` added to the `GetWorldTransform` and
  `RecalculateTransforms` declarations.
* `src/GuiSys/CGuiObject.cpp` - `#pragma inline_max_size(450)` at the top; `AddChildObject`
  and `GetWorldTransform` respelled as below.

No `configure.py` change, no `config/` change, no `asm`.

## Per function: before %, after %, and what produced it

| function | before | after | what changed |
|---|---|---|---|
| `RecalculateTransforms` | 23.33 | **100.00** | `inline` on the declaration + `inline_max_size(450)`. No source change at all. |
| `AddChildObject` | 81.71 | 99.15 | four source changes, all listed below. 45 bytes of the 760 still differ. |
| `GetWorldTransform` | 14.04 | 79.17 | `inline` + `inline_max_size(450)` + swapping the two arms of the `mParent` test. |

### 1. The recursion: `inline` on the declaration, then `inline_max_size`

`-inline deferred,noauto` means *do not inline automatically*; a function is still inlined when
its **declaration is marked `inline`**. Neither recursive member was, so mwcceppc emitted a
72-byte loop with a single `bl`. Adding `inline` to the two declarations in
`include/GuiSys/CGuiObject.hpp` makes the self-call inlinable, and the depth is then governed by
`-pragma "inline_max_size(N)"` (the project default is 125, `configure.py` line 283):

| `inline_max_size` | `RecalculateTransforms` | `GetWorldTransform` |
|---|---|---|
| 125 (project default, `inline` absent) | 23.33% (72 B, no unroll) | 14.04% (no unroll) |
| `inline` present, 125 | 49.36% (3 levels) | 29.29% |
| `inline` present, 200 | 68.31% | 32.94% |
| `inline` present, 300 | 87.19% | 53.36% |
| `inline` present, 400 | **100.00%** | 66.18% |
| `inline` present, **450** | **100.00%** | **71.62%** |
| `inline` present, 500 / 550 / 600 / 700 / 800 / 1500 / 4000 | 100.00% | 71.62% (flat) |

`450` is the smallest value in the plateau, so that is what the pragma says. **`unroll_factor`
is not the lever**: `#pragma unroll_factor(8)`, `(10)` and `(16)` all leave the unit at exactly
the no-pragma numbers (74.17% fuzzy, 49.36% / 29.29%). Raising `inline_max_size` alone, with
the declarations left un-`inline`d, does nothing at all (23.33% / 14.04% at 500 and at 900) -
**both halves are required.**

Retail's `RecalculateTransforms` is 288 bytes and unrolls nine deep (`r3`, then `r22, r30,
r29, r28, r27, r26, r25, r24, r23`) before the tenth level becomes a real `bl`; ours at 450
unrolls the same nine deep and is byte-exact, prologue (`stmw r22,8(r1)`) and all.

**Why this is a code fact and not a header hack:** the `inline` keyword changes no layout and
no symbol (the functions are still emitted out of line, still at retail's addresses, still the
only definitions - `unit_fit.sh` reports no extra functions). `CGuiObject.hpp` is included by
only two files (`src/GuiSys/CGuiObject.cpp` and `include/GuiSys/CGuiWidget.hpp`), and the full
build after the change has **zero** functions worse anywhere, so no other unit moved.

`GetWorldTransform` is a different animal and is still open - see the last section.

### 2. `AddChildObject`: 81.71% -> 99.15%

Four changes, in decreasing order of effect. Everything else in the function was already right
(the previous run's `position`, `scale`, `sIdentity` copy and `__ml__` findings all hold, and
all of them still match byte-for-byte).

**(a) `tmpMtx.GetColumn(kD?)` spelled out nine times, not `Get00()..Get22()` or named locals.**
The previous run tried `GetColumn(0)` - the `int` overload - and measured 45.41%: it has a
`switch` and `-inline deferred,noauto` will not inline it, so nine real calls appear. The
`EDim` overloads inline perfectly, and retail reads the matrix **strided**
(`GetColumn(kDZ)` reads `+8, +20, +32`), which only the `EDim` overloads do. Written out nine
times, mwcceppc materialises a CVector3f temporary per call and reuses three frame slots
(`r1+8/20/32`, 18 dead stores) - exactly retail. `Get00()..Get22()` reads them contiguously and
produces no temporary at all.
* 81.71% -> **90.93%**, frame 336 -> 416.

**(b) `column * (1.f / scale.GetX())`, not `(1.f / scale.GetX()) * column`.**
Same values, same three `fmuls`, but with the scalar first the two nested class temporaries are
folded into one and the six CVector3f frame slots retail builds (`r1+92..163`: three raw
`GetColumn` results and three scaled ones, the ctor's `r4/r5/r6` pointing at the *scaled* set)
are lost. With the vector first, the frame is exactly 448 bytes like retail's and the six slots
land in the right places. The `EDim` overload of `operator*` is the one to spell.
* 90.93% -> **96.42%**, frame 416 -> 448.

**(c) the three scaled columns inline in the `CMatrix3f` constructor call**, not bound to named
`const CVector3f&` locals. With the locals, the frame slots are handed out in the reverse order
(col0 lowest) and retail has col2 lowest; inlining puts creation order = evaluation order and
the assignment matches exactly.
* 96.42% -> **96.99%**.

**(d) the `atEnd` sibling walk as `for (;;) { ... if (next == nullptr) { ...; break; } ... }`.**
The plain `while (last->mNextSibling != nullptr) { last = last->mNextSibling; }` produces the
same instructions but mwcceppc **rotates the loop** so the body is the loop head:

```
retail  lwz r0,next(r3); cmplwi; bne L_cont;  stw child,next(r3); b end
        L_cont: mr r3,r0; b top
ours    b top; L_cont: mr r3,r0; top: lwz r0,next(r3); cmplwi; bne L_cont; ...
```

Prime 1's `do { ... } while (true)` and the `for(;;)` form both keep the test at the top.
* 96.99% -> **99.15%**, and the whole 760-byte body is now byte-identical except 45 bytes.

Dead ends, all measured this run (do not repeat): named `const CVector3f& m0/m1/m2` locals
passed to the ctor (96.42%, slots reversed); named `const CVector3f c0/c1/c2` columns for the
12-float ctor (87.43%, frame 368); `const CVector3f` **by value** for `pos` (90.93%);
inline-into-ctor *with* the scalar-first multiply (92.98%); named inverse-scale floats
`invX/invY/invZ` (90.93%); `scale` before `position` (83.13%); `scale[kDX]` (96.27%);
`tmpMtx.GetColumn(k)[k]` (97.96%); `pos.GetParent()->GetWorldTransform()` (96.15%);
`const CGuiObject* parent = child->GetParent()` as a separate local (98.31%); `pos` as
`const CVector3f&` (99.09%); `tmpMtx` as `const CMatrix3f&` (98.25%); a named
`const CTransform4f childXf` temporary (99.15%, identical); `pos[0]/[1]/[2]` instead of
`GetX/GetY/GetZ` (99.16%, one byte better and the same register assignment);
`-1.f * GetTranslation()` instead of `GetTranslation() * -1.f` (99.15%, byte-identical).

### 3. `AddChildObject`'s last 45 bytes: FPR numbering only

Everything else is exact. The remaining diff is entirely which floating-point register holds
each of the twelve values of the 12-float `CTransform4f` temporary at `r1+260` (and the same
values in the nine dead `GetColumn` temporaries at `r1+8..76`):

```
value    retail  ours      value    retail  ours
m22      f5     f3        m00      f1     f11
m12      f4     f4        pos.y   f10    f2
m02      f3     f5        pos.z    f11    f1
m21      f7     f6        pos.x    f0     f0
m11      f6     f7        m20      f9     f9
m01      f2     f8        m10      f8     f10
```

The **load order, the store order, the frame offsets and every mnemonic already match** - the
twelve loads are issued in the same sequence (`340, 328, 316, 336, 324, 312, 332, 320, 308,
84, 88, 80`) and the eighteen stores into the `GetColumn` temporaries and twelve into the
`CTransform4f` temporary are in the same order with the same values. Only the physical register
number differs, and it is a single contiguous allocator decision made for the whole basic block:
retail hands f1..f9 to the nine matrix values and f0/f10/f11 to `pos`, ours hands f3..f11 to
the matrix and f0/f1/f2 to `pos`. About thirty spellings of the 12-argument constructor did not
move it (all listed in the dead-end list above, plus the accessor variants `GetX/GetY/GetZ`,
`operator[]`, and a mixed `GetRow` spelling), so this is very likely not reachable from source
with the same twelve arguments. **Do not spend a run on frame offsets or offsets alone** - the
previous run's advice, now with the offsets actually right.

## `GetWorldTransform`: 14.04% -> 79.17%, still open (no WALL: - it moved twice this run)

`inline` + `inline_max_size(450)` gets the ten-level `mParent` descent with retail's exact
prologue (`stwu r1,-480(r1)`, `stmw r23,444(r1)`, `mr r26,r3`) and 696 bytes, the same size as
retail. On top of that, the two arms of the inner test have to be the other way round:

```cpp
if (mParent != nullptr) {          // 79.17%
  mWorldXF = mParent->GetWorldTransform() * mLocalXF;
  mWorldTransformValid = true;
} else {
  return mLocalXF;
}
```

with `if (mParent == nullptr) { return mLocalXF; }` first it is 71.62%: retail's ten levels
**branch over** the no-parent case to reach the `mWorldTransformValid = true` store, so the
multiply has to be the fall-through. `mWorldTransformValid = true;` before the assignment is
51.68%; the two statements in the other order 76.53%; hoisting the `mWorldTransformValid` test
to an early `return mWorldXF;` 53.14%.

**What is left (108 of 696 bytes), and it is one shape, not register allocation.** At every
level retail ends its multiply block with a `b` that jumps *past* the no-parent case into a
separate `li r0,1 ; stb r0,100(rX)` block, which then falls through into the next level's
body:

```
retail   ... bl __as__ ; b <setvalid_i> ; <null case: addi r4,this+4 ; b ...> ; li r0,1 ; stb r0,100(this) ; <body_{i+1}>
ours     ... bl __as__ ; li r0,1 ; stb r0,100(this) ; b <body_{i+1}> ; <null case: addi r4,this+4 ; b ...>
```

Ours keeps the validity store in the same basic block as the `__as__` call, retail splits it
out. The instruction sequence is identical - only the position of the `b` and the resulting
branch displacements (`b +0xc` vs `b +0x8`, 8 bytes per level, 10 levels) differ. That is a
block-layout choice; I did not find a source spelling that produces it, and I am **not**
recording a `WALL:` for it, because a run that only tries the obvious restructurings will not
reach 100% either. If a future attempt wants it, the thing to look for is a source form where
`mWorldTransformValid = true` is not textually in the same statement group as the assignment -
the pragma/`inline` work above already proved this function's shape is source-reachable.

## Codegen facts added this run (measured)

* `-inline deferred,noauto` will inline a self-recursive function if the **declaration** is
  marked `inline`; `-pragma "inline_max_size(N)"` then sets the unroll depth, and the depth is
  flat from 450 to 4000 on both functions. `unroll_factor` does nothing. Both halves are needed.
* `operator*(const CVector3f&, float)` and `operator*(const float, const CVector3f&)` are not
  the same code here: the vector-first form keeps the CVector3f temporary materialised in a
  frame slot, the scalar-first form folds it away. 96.42% vs 90.93% in `AddChildObject`.
* An `EDim` overload of an inline accessor is inlined; the same class's `int` overload is not,
  and if it has a `switch` it becomes nine real calls. `GetColumn(kDZ)` yes, `GetColumn(0)` no.
* mwcceppc **rotates** a `while (p->next != nullptr) p = p->next;` loop so the body is the head;
  the same loop written `for (;;) { next = ...; if (next == nullptr) { ...; break; } ...; }` is
  not rotated. 96.99% vs 99.15%, 28 bytes.
* A spelling repeated N times produces N temporaries but mwcceppc reuses slots: nine
  `tmpMtx.GetColumn(k?)` calls yield 18 stores into three 12-byte slots, all dead.
