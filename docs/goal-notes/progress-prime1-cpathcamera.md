# progress-prime1-cpathcamera — MetroidPrime/Cameras/CPathCamera

`kind: progress`, unit stays `NonMatching`. **matched_functions 5/16 -> 9/16**; global
`9908 -> 9912` (`All: 30.49566% -> 30.505743%` fuzzy, `total_functions` still 28465). No
function anywhere got worse (`tools/report_diff.py` prints `no regression`). No `asm` added.

## What I did

I did **not** port Prime 1's `CPathCamera.cpp` at all. Prime 1's class is a different
class: it owns a `mSpline` directly, its `MoveAlongSpline` returns a `CTransform4f`, and
its `AvoidDoorCollisions(CStateManager&)` has no `xf` parameter. Echoes splits the
settings into `CScriptPathCamera` and the runtime into `CPathCamera`, so the item's
"Echoes' engine is a fork of it" is true at the level of the *algorithm*, not of the
source. Every function I touched I read out of the retail disassembly
(`tools/dis.sh 0x801B31A8 0x1DC0`) and wrote against **this repo's** headers. The Prime 1
read was useful for exactly one thing, below.

Four functions reached 100%, and three more rose substantially without flipping.

| function | before | after |
|---|---|---|
| `__ct__11CPathCamera...` | 97.39% | **100%** |
| `CalculateLookAtDistance` | 99.00% | **100%** |
| `UpdateFov` | 52.76% | **100%** |
| `AvoidDoorCollisions` | 10.49% | **100%** |
| `CalculatePositionDistance` | 78.47% | 85.81% |
| `MoveAlongSpline` | 61.80% | 81.77% |
| `__sinit_CPathCamera_cpp` | 0.00% | 44.27% |
| `Reset` | 2.88% | 2.88% (untouched) |
| `Think` | 0.70% | 0.70% (untouched) |
| `GetScanObjectIndicatorPosition` | 6.20% | 6.20% (untouched) |
| `UpdateOrientation` | 0.44% | 0.44% (untouched) |

## The one thing Prime 1 was needed for: `__sinit`

`__sinit_CPathCamera_cpp` is 0% because the unit has **no static data at all** in our
source, so the function is not emitted. Retail's `__sinit` (0x801b4ef0, 120 bytes) builds
one `CMaterialFilter` on the stack and stores it to a `.data` address, having first filled
two `.sbss` words (0x804192B0..0x804192BF) via `__shl2i` — i.e. it constructs two
`CMaterialList`s and then a `CMaterialFilter` from them. That is exactly Prime 1's
`kLineOfSightIncludeList` / `kLineOfSightExcludeList` / `kLineOfSightFilter` triple
(`prime-ref/src/MetroidPrime/Cameras/CPathCamera.cpp:13-16`), so I added the same three
statics. Prime 1's are `kMT_Solid` / `kMT_ProjectilePassthrough`; the material constants
are **not** recoverable from the retail bytes (see the blocked section), so I used this
repo's own `kMT_Unknown59` / `kMT_NoPlatformCollision`. That is a guess in a value, not in
structure — see the caveat.

## The three spelling rules that actually moved functions

These are the reusable findings. All three are codegen rules, not a lesson or a wall.

**1. `0u` on an `int` count is what produces `cmplwi`, not `cmpwi`.** Retail's
`CalculateLookAtDistance` and every other count test in the unit is `cmplwi rX,0`; ours
was `cmpwi`. `GetControlPointCount()` returns `int`, so `== 0` gives `cmpwi` and
`== 0u` gives `cmplwi`. Changing *only* that one character took
`CalculateLookAtDistance` from 99.00% to 100%. It is a whole unit-wide idiom: every
`GetControlPointCount()`/`GetKnotCount()` comparison in this file now carries the `u`
suffix.

**2. Branch polarity in the source is what picks `beq` vs `bne`.** `UpdateFov` was
stuck at 52.76% purely because I had written the condition the "obvious" way round.
Retail branches *forward over* the uncommon path; writing the same test with the arms
swapped (`!=` first, `else` after) put the `beq` where retail has it and matched it.
The rule: when a function's prologue/epilogue and every block already line up and one
`b` is inverted, invert the *source* condition rather than the branch.

**3. Retail re-derives values instead of holding them — do not "help" it.** In
`MoveAlongSpline` retail computes `camera + 544` (`GetSpeedControlSpline`) twice rather
than keeping a reference, and it copies `GetTranslation()` into locals before the null
check. Hoisting either into a C++ local made the score *worse* (81.77% -> 62.61% on one
attempt). Introducing `const CVector3f ret = GetTranslation();` at the top instead took it
61.80% -> 81.77%. So: **when retail recomputes, write it recomputed; when retail
pre-copies, pre-copy it in source.**

**4. `rstl::min_val` is not how retail writes a min.** In `CalculatePositionDistance`
the closed-loop "nearest" is a hand-rolled `if`, and the sign test is written `!(a > b)`
rather than `a <= b`. `min_val` gave 83.98%; the explicit `if` gave 85.16%; `!(a > b)`
gave 85.81%. The `cror eq,lt,eq` that retail emits for `<=` on a float pair is the
compiler's spelling of `!(a > b)`.

**5. `string_l`, not `string`, for a constructor argument.** The ctor was 97.39% and the
only difference was `rstl::string("Path Camera")` vs `rstl::string_l("Path Camera")`:
retail calls `string_l__4rstlFPCc` (0x802ff418), we were emitting the
`basic_string(char const*, int, allocator)` constructor. One word, 97.39% -> 100%. Worth
grepping for across the repo: any `rstl::string(<literal>)` in a constructor is suspect.

## Verified

```
sha1sum build/G2ME01/main.dol        6ef9b491d0cc08bc81a124fdedb8bfaec34d0010   (retail)
./tools/decomp_build.sh              All: 30.505743% fuzzy, 9912 / 28465 functions
./tools/probe_sources.sh             749 files, 0 failed, 0 errors; LINKED (250 undef, 0 dups)
python3 tools/check_symbol_names.py  503 units; 0 declared names missing
python3 tools/check_decl_order.py    ok, none out of retail order
all 86 RELs                          cmp-equal to orig/G2ME01/files/RelProd/
config.yml hash re-check             ok (all 86 + main.dol)
tools/report_diff.py                 matched 9908 -> 9912, "no regression"
```

`tools/flip_test.sh` was not run: the unit is 3412 bytes of `.text` against retail's
7616 (`unit_fit.sh` says `SHORT by 4204`), so it cannot flip. Per the item, `progress`
is judged on `report.json`, which is what the numbers above are.

## Blocked, with the evidence

**`Reset` (1580 B, 2.88%) and `Think` (572 B, 0.70%) are the item's two named
functions and I did not move them.** They are the only two that need a *new* algorithm
rather than a spelling fix, and both are large. `Think` at retail 0x801b3b4c calls
`MoveAlongSpline`, `AvoidDoorCollisions` and `UpdateOrientation` in sequence, so it
cannot be written until `UpdateOrientation` is known — and `UpdateOrientation`
(904 B, 0.44%) resolves an active `CScriptCameraHint` and does quaternion look-at
damping that I did not finish. Chasing `Think` first would have been the wrong order.
`Reset` likewise is 1580 bytes of spline/door-collision side selection.

**`__sinit` is at 44.27% and I could not close it, and the blocker is a shared header.**
Our `__sinit` emits the right *shape* (two `__shl2i` list constructions then the filter
store) but the compiler picks different registers and does the two list constructions
sequentially rather than interleaved. The cause is `CMaterialList::Add` in
`include/Collision/CMaterialList.hpp`, which does `value |= u64(1) << material` — a
read-modify-write. Retail's sequence initialises each list to `0x1` (the `li r4,1` before
the first `__shl2i` at 0x801b4efc) and stores the shift result *directly*, with no load
and no `or`, because in retail the value is known to be `1 << material` and the list is
fresh. `CScriptDebris` has the same pattern and is at 100% with the *existing* header,
which is what tells me the header is usable for the 2-material form and that only the
1-material form diverges. Changing `Add` to a direct store would fix `__sinit` here but
I did not do it: `CMaterialList` is shared by many units and a change to it can move an
unrelated function in any of them, which the reviewer would rightly reject as out of
scope for this item. The three spellings I tried and their scores, so the next run skips
them: `CMaterialList(kMT_X)` (enum ctor) **44.27%**, `CMaterialList(u64(1) << kMT_X)`
**34.30%** (the compiler constant-folds it to `lis`), `static const` on all three
statics **no change from non-const**. The material constants themselves are not
recoverable: `__sinit` loads the shift amounts from `.sdata` at 0x80418678 and 0x8041867c,
whose *values* are the two `EMaterialTypes` the retail source passed, and those two words
are shared with other units, so I read them as a hint and not as a measurement.

**`GetScanObjectIndicatorPosition` (1408 B, 6.20%) is a stub and stays one.** I read
the first 120 bytes of it: it calls the ball camera's scan-indicator position
virtually, then branches on `mFlags` bit 26 and bit 24, then a camera-hint height
override, then a perpendicular-distance spline evaluation. It is a large function with
its own `fn_801B9480` helper that I would also have to identify. Not a bounded slice.

## NEW:

NEW: CPathCamera | match | MetroidPrime/Cameras/CPathCamera | `__sinit_CPathCamera_cpp` needs `CMaterialList::Add` to be a direct store rather than a read-modify-write, and `Reset`/`Think`/`GetScanObjectIndicatorPosition`/`UpdateOrientation` remain unported (0.7-6.2% each) - a shared-header change plus four large functions is more than one item
