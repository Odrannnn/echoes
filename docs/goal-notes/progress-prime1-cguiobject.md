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
