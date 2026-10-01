# progress-unit-ceulerangles - MetroidPrime/CEulerAngles

`src/MetroidPrime/CEulerAngles.cpp` only. One function touched in passing: none - the only edit
outside the new code is the `FromQuaternion` body, and `docs/HANDOFF.md`'s state block was
rewritten by `tools/sync_state_block.py` when `tools/gate.sh` ran inside the judge.

## Result

**3 -> 5 of 6 functions matched**; the whole tree went 11828 -> 11830 matched of 28465.
`./tools/goal_check.sh build/goal/item.json` prints `PASS progress-unit-ceulerangles`, with
`target rose: main/MetroidPrime/CEulerAngles: 3 -> 5 / 6 functions`, `no asm added`, and the
full `gate.sh` clean (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe).

Per function, from `build/report.json` after the change:

| function | before | after |
| --- | --- | --- |
| `__sinit_CEulerAngles_cpp` (24 B) | 100% | 100% |
| `FromTransform__12CEulerAnglesFRC12CTransform4f` (60 B) | 100% | 100% |
| `sqrt__Ff` (32 B) | 100% | 100% |
| `FromMatrix__12CEulerAnglesFRC9CMatrix3f` (300 B) | **0.00%** | **100%** |
| `msl_sqrtf__Ff` (228 B) | **0.00%** | **100%** |
| `FromQuaternion__12CEulerAnglesFRC11CQuaternion` (252 B) | 92.68% | 99.52% |

Unit: 39.01% -> 99.87% fuzzy, 12.95% -> 71.88% of code. `.sdata2` is 40/40 bytes claimed and
matches 80% of them; the residue is `1.0f` and `2.0f` sitting in each other's pool slot (below).
`python3 tools/check_decl_order.py --unit MetroidPrime/CEulerAngles` says `ok: 1 unit(s) checked,
none emits its functions out of retail order`, and `tools/unit_fit.sh` says `no extra functions:
our object defines only what the retail unit object does` (it does report `.bss claimed 12 ours 0
SHORT by 12`, which is pre-existing: `sIdentity` is `common` and lands in `.sbss`).

The unit stays `NonMatching`; `flip_test.sh` was not run, per the `progress` item.

## The three pieces

### 1. `FromMatrix`, 0% -> 100%

It was declared in the header and never defined, so retail's bytes came from dtk. Prime 1's
`src/MetroidPrime/CEulerAngles.cpp` is the donor for the logic and it ports across almost as-is,
because the arithmetic and the three `atan2` call sites fall straight out of
`./tools/dis.sh 0x8001D430 0x380`.

One thing is **not** Prime 1's spelling and is load-bearing: the negations. Prime 1 assigns
`roll = -atan2(...)` into a `double`. Retail's asm has `frsp` on each result and only then a
`fneg`, i.e. the `atan2` result is demoted to `float` and the sign is applied outside the call.
So the three are `const float` and the `return` is `CEulerAngles(-roll, -pitch, -yaw)`.
Measured: negating inside the `atan2` is 2 differing instructions worse; `double` locals are
worse again.

`close_enough(sq, 0.f)` is the repo's own overload and its default `Real32::Epsilon()` is the
`1e-5f` at `s2:-32180`; the `0.f` argument is the `s2:-32188` that `fsubs` subtracts. Both come
out right with no change.

`tools/try_batch.py src/MetroidPrime/CEulerAngles.cpp MetroidPrime/CEulerAngles FromMatrix`:
`base` **0 differing instrs (MATCH)**, `roll-first` 20+, `neg-inside` 23+, `prime1-double` 20+,
`ce-after` 35.

### 2. `msl_sqrtf`, 0% -> 100%

The biggest find of the run, and it is a repo fact rather than a decompilation problem:
**`libc/math.h` already gates retail's exact `fpclassify` behind a macro.** Lines 121-172:

```c
#if (!defined(__MWERKS__) || __MWERKS__ >= 0x2407) && !defined(MSL_OLD_FP_CLASSIFY)
#define FP_NAN 0
#define FP_INFINITE 1
... FP_ZERO 3, FP_NORMAL 4, FP_SUBNORMAL 2   /* and this switch has case 0 first */
#else
#define FP_NAN 1
#define FP_INFINITE 2
#define FP_ZERO 3
#define FP_NORMAL 4
#define FP_SUBNORMAL 5                         /* and this switch has 0x7f800000 first */
#endif
```

Retail's tail at 0x8001D6E8 is `__fpclassifyf` inlined with `case 0x7f800000` first and
`li r0,1` / `li r0,2` / `li r0,3` / `li r0,5` / `li r0,4` - that is the **else** branch, i.e. the
pre-2.4.7 numbers. Without `#define MSL_OLD_FP_CLASSIFY` the best spelling was 15 differing
instructions with `li r0,0` / `li r0,1`; with it, `0`.

The other half: retail's slow path loads its threshold with `lfd` and not `lfs`
(`c8 02 82 60 lfd f0,s2:-32160`, the trailing `0.0` pool word at 0x8041A620), so the source
compares against the **double** literal `0.0`. `x < 0.0f` is 2 instructions worse than
`x < 0.0`. That is the whole difference, and it is worth recording because nothing in the
disassembly explains it - reading the displacement as a float constant is what makes the
instruction look wrong.

The fast path is the `sqrtf` body `libc/math.h` already carries at lines 202-220, with two
differences from it: drop `volatile float y` (retail has no `stfs`/`lfs` round trip, just
`fmul f1,f1,f0` / `frsp f1,f1` / `b`) and use `__frsqrte(static_cast<double>(x))` directly.
With the repo's `volatile` spelling the fast path still matches instruction for instruction; the
two extra `stfs`/`lfs` are all that is left of it.

**`MSL_OLD_FP_CLASSIFY` is defined only in this translation unit**, before the first include,
because that include is the one that reaches `math.h`. Nothing else in the tree changes, which
is why `check_docs_claims.py` and the report diff are both clean. Any other unit that turns out
to need retail's old fpclassify numbers has the same one-line fix waiting; the default branch in
`libc/math.h` must not be flipped, it is what every other unit is built against.

Also: the unit's two `sqrt` symbols are `msl_sqrtf__Ff` and `sqrt__Ff`, which is just
mwcceppc's mangling of `msl_sqrtf(float)` and `sqrt(float)`. They were previously spelled
`extern "C" float msl_sqrtf__Ff(float)` / `extern "C" float sqrt__Ff(float)`, i.e. the
pre-mangled identifier pasted into C with `extern "C"` to keep the symbol. Writing the real
declarations and dropping `extern "C"` produces the same two symbols and lets `FromMatrix` call
plain `sqrt(...)`, which picks the `float` overload and calls `sqrt__Ff` - the same symbol retail
calls. `extern "C" float sqrt(float)` over `double sqrt(double)` also compiles, but the mangled
spelling is the honest one.

### 3. `FromQuaternion`, 92.68% -> 99.52%

Same 67 instructions as retail; the diff is register allocation, not logic.

Retail's product order, read off the asm, is: the `sx sy sz` group, then
`yy xx zz wx yz wy xz wz xy`. Prime 1's order (`zz xx wz yy xy wy xz yz wx`) is **worse** here
(35 vs 19 differing instructions), and hoisting `const CVector3f& vector = quat.GetVector()`
is worse still (40 vs 35). Declaring the products in retail's own instruction order is what
took it from 92.68% to 99.52%.

What is left is two instructions: retail puts `yy` in f11 and `xx` in f3, this puts `xx` in f11
and `yy` in f3, which also swaps the operands of the three `fadds`. Both order the same 12
`fmuls` the same way and the same registers otherwise - the allocator picks f3, the register
`scale` just died in, for the first product where retail picks f11.

WALL: CEulerAngles::FromQuaternion 99.52% - the last two instructions are an f11/f3 swap between
`yy` and `xx`; the logic and the 12-product order already match retail and 70-odd other
spellings did not move the allocator.

### The spellings already tried for `FromQuaternion` (do not repeat)

`tools/try_batch.py`, 6 batches, 78 variants, counted as differing instructions against the
retail object. Current source is `ctrl`/19 in the table below.

- product order: retail's asm order **19**; Prime 1's order 35; five other orderings 33-38.
- group-1 order (`sx sy sz` / `sz sy sx` / `sy sx sz` / `sx sz sy` / `sz sx sy` / `sy sz sx`) x
  group-2 order: best 19, worst 27. `sz sy sx` = retail's instruction order and is **not** the
  best, 20.
- `const CVector3f& vector = quat.GetVector();` hoisted: +5 to +6, never better.
- `const` on all 12 products, on `scale` and `magnitudeSquared`, or on all of them: 19, no
  change.
- naming the nine matrix entries separately before the `CMatrix3f(...)` call: 19, no change.
- `if (magnitudeSquared > 0.f) scale = 2.f / magnitudeSquared;` with a `0.f` seed instead of
  the ternary: 22. `0.f < magnitudeSquared ? ... : ...`: 21.
- FromMatrix: negations inside `atan2` 23, `double` locals 20, `roll` computed first 20,
  `close_enough` tested positive-first 35.

## Left, and not left as a guess

`FromQuaternion` is the only function still short, and it is a register swap. The `.sdata2` slot
order is also still off by a transposition that no spelling moved: retail's pool is
`1.0f 0.0f 2.0f 1e-5f 0.5 3.0 0.0` and ours is `2.0f 0.0f 1.0f 1e-5f 0.5 3.0 0.0`. `1.0f` and
`2.0f` are used only in `FromQuaternion`, `0.0f` first at 0x34 then `2.0f` at 0x4c then `1.0f`
at 0x58 in both. It costs 8 bytes of the unit's 80% data match and nothing else, and it is not
worth another lane: there is no `NEW:` line here on purpose, since a requeue of this unit for two
instructions is a restatement of the item and the `WALL:` line above is the signal the driver
wants instead.
