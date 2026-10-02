# progress-unit-ctargetreticles

`kind: progress`, `target: MetroidPrime/CTargetReticles`, lane 8, 2026-10-02.
`./tools/goal_check.sh build/goal/item.json` -> **PASS** (re-run at the end, on the tree as
committed):

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12469 -> 12470   linked 5863 -> 5863
  ok    check_symbol_names.py
  ok    All:  35.25% fuzzy, 29.04% matched, 12.90% linked (12470 / 28465 functions)
  ok    target rose: main/MetroidPrime/CTargetReticles: 30 -> 31 / 44 functions
  ok    no asm added
goal_check: PASS progress-unit-ctargetreticles
```

The unit stays `NonMatching` (it is 10 540 bytes short of retail's 29 220, and
`unit_fit.sh` lists 15 COMDAT weak copies it emits that retail's object does not), so no
`flip_test.sh` was run - the item is judged on `report.json`'s per-function exact matches.

## Re-measurement first: the item's `reason` was stale

`item.json` said "27/44 functions match". On the clean tree `build/report.json` said **30/44**,
and `tools/fast_try.sh MetroidPrime/CTargetReticles` agreed:

```
main/MetroidPrime/CTargetReticles: 25.93% fuzzy, 19.82% matched code, 30/44 functions
```

14 functions were below 100%. The 30 that already matched are the file-local `fn_800B2*`
helpers, the two `Touch` functions, `COrbitPointMarker`'s six, `CTargetReticleRenderState`'s
three, `CTargetingManager`'s ctor/`CheckLoadComplete`/`Update`, and
`CCompoundTargetReticle::DrawCrosshairs`/`Draw`/`UpdateOrbitZoneGroup`/`GetDesiredReticleState`.

## What landed

### 1. `Draw__17CTargetingManagerCFRC13CStateManagerb` - **22.04% -> 100.00%** (336 B)

This is the item's matched function (30 -> 31). It was a two-line stub; the body is in
`src/MetroidPrime/CTargetReticles.cpp` and the recipe is in the comment above it. The parts
that were not obvious:

- `CColor::White()` already returns `const CColor&` in this repo, so
  `CGraphics::SetAmbientColor(CColor::White())` passes r3 straight through - no temporary.
- `GetCurrentCameraTransform` is called into a temporary and **copy-constructed** into the
  local that `SetViewPointMatrix` gets (retail: `__ct__12CTransform4fFRC12CTransform4f`
  between the call and the use). `const CTransform4f camXf = ...` produces exactly that.
- The two viewport floats are `static_cast<float>` of `int`, and MW expands that as
  `xoris`/`lis 0x4330`/`stw`/`stw`/`lfd`/`lfd`/`fsubs` against the pool constant
  `0x4330000080000000` at `_SDA2_BASE_ - 29464` - not a reinterpret. `CCast::LtoF` is
  `static_cast<float>(int)` here and that is what emits it. (This is the same idiom the
  already-matched `CPlayer::WithinOrbitScreenEllipse` uses, at 99.8%.)
- **The order of the two viewport locals is load-bearing.** `CViewport` is
  `{mLeft, mTop, mWidth, mHeight, mHalfWidth, mHalfHeight}`, so `mWidth` is +0x08 and
  `mHeight` +0x0C, and retail loads `mHeight` **first** (into f31) and `mWidth` second
  (into f30), then `SetPerspective` copies them into f3 and f2. `COrbitPointMarker::Draw`
  (100% already) does the same thing, so I copied its shape verbatim:
  `vpHeight` then `vpWidth`, both before the `gpRender->SetPerspective` call, and
  `curCam->GetFov()` is called *after* them.
- `gpRender->SetPerspective` is the 5-argument `IRenderer` overload at vtable +0x5C
  (`lfs f4,460(r28)` / `lfs f5,464(r28)` are `CGameCamera::mZnear`/`mZfar`).
- `CTargetingManager::mOrbitPointMarker` is at +0x2B0 and `mTargetReticle` at +0x4.

Nothing was tried and rejected for this function: the first spelling after reading the
disassembly scored 100.00%.

### 2. `CalculateOrbitZoneReticlePosition__22CCompoundTargetReticleCFRC13CStateManagerb` - **81.00% -> 99.68%** (380 B)

Not a match, but a real structural fix and the reason the item is nearly a second match. The
whole shape of the function is the *order* MW evaluates in. Retail calls `GetCurrentCamera`,
then loads `mgr.GetPlayer(mPlayerIndex)` into r31, then calls `GetFov`, and only then
`GetTweakPlayer`/`GetOrbitZoneHeight`. So the player pointer must be a **named local
evaluated between the camera and the fov term**; inlined, or declared after the fov term, MW
moves the `GetFov` call earlier and the score falls.

Four instructions are still out, and they are a pure float-register swap: retail puts the
`224.0f` numerator of the `fdivs` in **f2** and the `int`->`double` temporary of the
`(float)CCast::LtoF(...)` in **f3**; ours has f2 and f3 exchanged (`lfd f2,-29464` where
retail has `lfd f3,-29464`, and the two `fdivs` operands with them). Everything else -
prologue, GPR allocation, call order, epilogue - is byte-identical.

18 spellings, all measured this run with `tools/fast_try.sh`:

| spelling | score |
| --- | --- |
| as found (`halfExtY` inline, fov inline in the `tan` call) | 81.00% |
| `fovHalf` hoisted, player expression still inlined | 82.20% |
| `halfExtY` declared **before** `fovHalf` (changes the call order) | 88.98% |
| everything inlined into the `return` expression | 56.96% |
| `224.f / halfExtY / tan(...)` as one expression | 75.09% |
| `const float tanT = tan(...); 224.f / halfExtY / tanT` | 75.09% |
| `const double halfExtY` + `static_cast<float>` around the division | 97.53% |
| **named `player` local between the camera and the fov term** | **99.68%** (kept) |
| the same, `const float numerator = 224.f; dist = numerator / halfExtY` | 99.68% |
| the same, `224.f / CCast::LtoF(player->...)` inline (no `halfExtY` local) | 99.68% |
| the same, `const float tanArg = fovHalf * (1.f/360.f) * (2.f*M_PIF)` | 99.68% |
| the same, `dist = dist / tan(...)` instead of `dist /= tan(...)` | 99.68% |
| the same, `const CGameCamera& cam = *mgr.GetCameraManager(...)->GetCurrentCamera(...)` | 99.68% |
| the same, `const int orbitHeight = ...; 224.f / CCast::LtoF(orbitHeight)` | 99.68% |
| the same, `const float scaled = 224.f/halfExtY; float dist = scaled;` | 99.68% |
| the same, `const float fovArg = fovHalf * (1.f/360.f); tan(fovArg * (2.f*M_PIF))` | 99.68% |
| the same, non-`const` `fovHalf` / `halfExtY` / `dist` | 99.68% |
| conversion and division sharing one variable (`float dist = CCast::LtoF(...); dist = 224.f/dist;`) | 99.68%, but structurally *worse* (r26, two extra `mr`s, `fovHalf` in f4) |
| `rstl::min_val(min_val(z,y), maxX-minX)` / `max_val` outer arguments swapped | 96.53% |

A standalone `mwcceppc` experiment (compiled outside the tree with the same flags, a
structurally identical stub of the function) produces retail's register assignment for
**every** one of these shapes, so the difference is not in the float expression tree - it is
something in the surrounding translation unit that MW's allocator reacts to. I did not find
it.

WALL: CalculateOrbitZoneReticlePosition__22CCompoundTargetReticleCFRC13CStateManagerb 99.68% - 4 instructions left, a pure f2/f3 float-register swap between `224.0f` and the `int`->`double` temp; 18 spellings tried, all 99.68% or worse.

## What I reached but could not land

### `CalculateRadiusWorld__22CCompoundTargetReticleCFRC6CActorRC13CStateManager` - **1.35% -> 96.11%** (576 B), then reverted

It was `// TODO: ... return 1.f;`. Prime 1's decomp is a direct donor and the Echoes retail is
the same function with one extra step. I got it to **96.11%** and then had to take it back out
again, for a reason worth recording:

**The body needs a `TCastToConstPtr<CSandwormEye>` call, and that raises the port's undefined
count, which fails the strict link gate.** The chain is
`TCastToConstPtr<T>(const CEntity&)` -> `TCastToPtr<T>(const_cast<CEntity&>)`
(`include/MetroidPrime/TCastTo.hpp:21`), and retail's `CalculateRadiusWorld` does exactly
that: `mr r3,r30; bl TCastToPtr<12CSandwormEye>__FR7CEntity`. For the DOL that resolves -
`src/MetroidPrime/TypesMatch.cpp:812` has `CAST_TO_IMPL(CSandwormEye, kET_SandwormEye)` and
the symbol is in `build/G2ME01/main.elf`. For the **port** it does not:
`files.cmake:968` records that `src/MetroidPrime/TypesMatch.cpp` is excluded from the host
build (its `uchar x_pad0[0x2f0 - sizeof(CPhysicsActor)]` underflows on a 64-bit host) and
that only six TypesMatch bodies live in `platform/PortGlobals.cpp` instead. So the call added
a 292nd undefined symbol and `tools/link_check.sh --strict` failed, which fails
`goal_check.sh`'s `gate.sh`:

```
FAIL  gate.sh
      link_check: STRICT FAIL - regression gate: 292 undefined against a baseline of 291 (GREW), 0 duplicate(s)
```

Confirmed by set-differencing the two lists:
`diff <(sed 's/^sym //' docs/research/port_link_baseline.txt | sort -u) <(sort -u build-port-link/link_undefined.txt)`
names exactly one addition, `CSandwormEye* TCastToPtr<CSandwormEye>(CEntity&)`. The fix is one
line in `platform/PortGlobals.cpp`, which is **not** this item's target, so I reverted the body
rather than widen the diff; the stub function keeps a comment pointing here.

The recipe, so the next run does not have to re-derive it (all measured):

1. Prime 1's body, adapted: `optional_object<CAABox>` from `actor.GetTouchBounds()`,
   `? *touchBounds : CAABox(actor.GetAimPosition(mgr, 0.f), actor.GetAimPosition(mgr, 0.f))`,
   then the `switch` on `gpTweakTargeting->GetTargetRadiusMode()` and the
   `radius > 0.f ? radius : 1.f` tail. **1.35% -> 62.79%.** `class CSandwormEye;` forward-
   declared at file scope is enough - the template is declared but not defined in this TU, so
   the call is external and links.
2. **The six `CAABox` floats must be named locals** (`minX`, `minY`, `minZ`, `maxX`, `maxY`,
   `maxZ`) declared *before* the switch. Retail loads all six into f26..f31 up front; written
   inline, MW keeps them in the case arms and the frame comes out 144 bytes instead of 224.
   **62.79% -> 96.11%.**
3. **`rstl::min_val(a, b)` is `(b < a) ? b : a` and MW evaluates the *second* argument first**,
   so the inner pair has to be written `(maxZ, maxY)` to emit `maxY` first, and the outer one
   `(maxX, minX-extent)`. `(maxY, maxZ)` scores 96.11%, `(maxZ, maxY)` 96.81%.
4. **The switch needs an explicit `case 2:` in front of `default:`** - retail emits the dead
   `b` at 0x800AD048 that a third explicit label produces. **96.11% -> 96.81%.**
5. Still 8 instructions out, and again a register-allocation difference: retail holds `0.5f`
   in f0 across the outer `min_val`, so the x extent lands in f2; MW reuses f0 instead.
   Tried: `0.5f * min(...)`, `min(...) * 0.5f`, `radius *= 0.5f` as a second statement, a
   named `extent` local, a named `scale` local, swapped outer arguments - 96.53% to 96.81%,
   none moved it.
6. `CAABox` is `{min, max}` and `optional_object::m_valid` is `ATTRIBUTE_ALIGN(4)`, i.e. at
   +0x18 for a 24-byte `CAABox` - which is the flag retail reads at `r1+80` after
   `GetTouchBounds` writes to `r1+56`.

WALL: CalculateRadiusWorld__22CCompoundTargetReticleCFRC6CActorRC13CStateManager 96.11% (reached, not landed) - 8 register-allocation instructions out, and the body cannot land at all until the port defines `TCastToPtr<12CSandwormEye>__FR7CEntity`; the strict link gate forbids the 292nd undefined symbol.

## Things the next run should not have to learn

- **MW's `(float)(int)` on this target is an 8-instruction double round-trip**, not a
  reinterpret: `xoris rX,rX,0x8000` / `lis rY,0x4330` / `stw rY,low` / `stw rX,high` /
  `lfd` / `lfd` / `fsubs` against the pool constant `0x4330000080000000`
  (`_SDA2_BASE_ - 29464`, = 2^52 + 2^31). I confirmed this by compiling
  `float f(int i){return static_cast<float>(i);}` with `mwcceppc` outside the tree - it emits
  exactly that sequence and the same 8-byte constant. `CCast::LtoF` is `static_cast<float>`,
  which is why it appears in `CalculateOrbitZoneReticlePosition`, `WithinOrbitScreenEllipse`
  and `CTargetingManager::Draw` alike. Do not "fix" it into a bit cast.
- **`M_PIF` is a `float`** (`include/Kyoto/Math/CMath.hpp:16`,
  `3.14159265358979323846f`), so `(2.f * M_PIF)` folds to the *float* constant 6.2831855 and
  loads with `lfs`, not `lfd`. This decides the float/double shape of the whole `tan`
  argument chain in `CalculateOrbitZoneReticlePosition`.
- **A standalone `mwcceppc` run is a fast oracle for register allocation.** Build.ninja's
  `cflags` block for any unit, run `wibo mwcceppc.exe -c <flags>`, then
  `build/binutils/powerpc-eabi-objdump -d -r` the object: 2 seconds a variant against 8 for
  `fast_try.sh`, and it answers "is this a tree-shape problem or a context problem?" - which
  is how I established that the remaining `CalculateOrbitZoneReticlePosition` diff is *not*
  reachable from the float expression.

## Not attempted

The other 12 unmatched functions are all `// TODO:` stubs of 300-7128 bytes
(`Update__22CCompoundTargetReticle` 2524 B, `UpdateCurrLockOnGroup` 2628 B,
`DrawCurrLockOnGroup` 7128 B, `DrawNextLockOnGroup` 2484 B, `DrawSeeker` 1172 B,
`DrawScanTargetGroup` 1116 B, `UpdateNextLockOnGroup` 860 B, `DrawOrbitZoneGroup` 724 B,
`DrawGrappleGroup` 648 B, `DrawGrapplePoint` 572 B, and
`__ct__22CCompoundTargetReticleFRC13CStateManageri` 2280 B at 59.40%). None is a
one-item-sized job from a donor; the smallest of them is still four to six times
`CTargetingManager::Draw`.
