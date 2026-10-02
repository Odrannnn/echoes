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

---

# Ninth run (2026-10-02), lane 2 (wt-mp2-goal-L2) — PASS, 32 -> 33 / 44

## Re-measurement first: the tree is past both earlier runs' notes

HEAD is `3d674363 progress: progress-prime1-ctargetreticles` — the *other* item on this same
unit, which landed `UpdateNextLockOnGroup` at 100% four hours ago. `tools/fast_try.sh
MetroidPrime/CTargetReticles` on this clean tree gives **32/44, 30.00% fuzzy, 23.92% matched
code**, so this file's "30/44" and that file's eighth-run "31/44" are both history. The seven
spellings of `Draw__17CTargetingManager` and `UpdateNextLockOnGroup` described above are in
`HEAD`; `CalculateRadiusWorld` and the twelve `// TODO:` stubs are not.

## What landed: `CalculateOrbitZoneReticlePosition`, 99.68% -> **100.00%** (380 B)

One line in `src/MetroidPrime/CTargetReticles.cpp`:

```diff
-      CCast::LtoF(player->GetTweakPlayer()->GetOrbitZoneHeight(CPlayer::kZI_Targeting));
+      static_cast< float >(player->GetTweakPlayer()->GetOrbitZoneHeight(CPlayer::kZI_Targeting));
```

That is the whole change. It retires this file's `WALL:` line and the sibling item's
"the remaining delta is scheduling only".

### The rule behind it: `CCast::LtoF` is not the same expression as `static_cast<float>`

`CCast::LtoF` (`include/Kyoto/Basics/CCast.hpp:61`) is
`inline float LtoF(int in) { return static_cast< float >(in); }` — a **separate inline
function**, and this unit compiles with `-inline deferred,noauto`, so it is materialised from
the inlining stage instead of from the expression tree. MW's float register allocator then
hands **f2 to the `int`->`double` temporary and f3 to the `224.0f` numerator**; retail has the
other way round (`lfd f3` for the pool, `lfs f2` for 224.0f, `fsubs f3,f1,f3`,
`fdivs f31,f2,f3`). Written as a cast applied to the call expression itself, the same
allocator hands out **f3 to the temporary and f2 to the numerator** and the function is
byte-identical.

Measured with `tools/fast_try.sh` on this tree, all with the rest of the body unchanged:

| spelling of the conversion | score |
| --- | --- |
| `CCast::LtoF(<call>)` into a named `const float` (as found) | 99.68% |
| `static_cast<float>(<call>)` into a named `const float` | **100.00%** (kept) |
| `(float)<call>` into a named `const float` | 100.00% (same code as `static_cast`) |
| `224.f / (float)<call>` inline, no `halfExtY` local | 100.00% (same code) |
| `224.f / CCast::LtoF(<call>)` inline, no `halfExtY` local | 99.68% |
| `const int h = <call>; static_cast<float>(h)` | 99.68% |
| `const int h = <call>; (float)h` | 99.68% |
| `const CTweakPlayer* tw = ...GetTweakPlayer(); static_cast<float>(tw->GetOrbitZoneHeight(...))` | 99.68% |
| `const u32 h = ...; static_cast<float>(h)` | 99.68% |

So it is **not** the cast syntax: `(float)`, `static_cast<float>` and `CCast::LtoF` are three
different codegens. The conversion has to be applied **directly to the call expression**;
routing it through a function (even an `inline` one that does nothing) or through an `int`
local gives 99.68%.

### Correction to the eighth run of the sibling item, and to this file's oracle claim

Both files record that "a standalone `mwcceppc` run produces retail's register assignment for
every one of these shapes, so the difference is not in the float expression tree - it is
something in the surrounding translation unit". **That is wrong**, and it is what kept the
previous two runs on the wrong question. Re-measured this run: a scratch TU containing only
this function plus the unit's own includes, compiled with the unit's exact `cflags` from
`build.ninja` (GC/2.7, `-O4,p -inline deferred,noauto ...`), emits the **full-TU** assignment
(`lfd f2` for the pool, `lfs f3` for 224.0f) for every one of the seven shapes tried - it does
not reproduce retail either. The standalone TU is therefore an excellent *fast* oracle (2s
against 8s) but it is not a different context for this question: the difference was in the
expression tree the whole time, in the cast.

Command shape, for whoever wants the oracle again:

```
wibo $MP_TOOLCHAIN_DIR/build/compilers/GC/2.7/mwcceppc.exe <cflags from build.ninja> \
     -o /tmp/oracle.o /tmp/oracle.cpp
build/binutils/powerpc-eabi-objdump -d -r /tmp/oracle.o | grep -E 'lfd|lfs |fsubs|fdivs'
```

(the `-pragma "cats off"` and `-pragma "warn_notinlined off"` arguments must stay **one**
argument each; splitting them on whitespace makes the compiler read `off"` as a filename).

### Ten more spellings measured, all 99.68% (do not repeat)

All measured in-tree with `fast_try.sh`, player local and call order unchanged:
`const float numerator = 224.f;` declared first in the function, before the camera;
the same declared between `fovHalf` and `halfExtY`; `float dist = 224.f; dist = dist/halfExtY;`
(and the `dist /= halfExtY` form, which loads 224.0f into f31 instead); `static const float`
function-local numerator; `const float d0 = 224.f/halfExtY; float dist = d0;`; non-`const`
`halfExtY`; `224.0` (a *double* literal) as the numerator, which is 97.53%; the `tan` argument
hoisted into `tanArg` after the divide; the player local moved after `fovHalf` (82.20%) and
removed entirely (82.20%) - both confirm the earlier runs' call-order finding.

## Verdict, measured on the tree as it stands

`./tools/goal_check.sh build/goal/item.json` -> **PASS**:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12531 -> 12532   linked 5903 -> 5903
  ok    check_symbol_names.py
  ok    All:  35.35% fuzzy, 29.19% matched, 12.92% linked (12532 / 28465 functions)
  ok    target rose: main/MetroidPrime/CTargetReticles: 32 -> 33 / 44 functions
  ok    no asm added
goal_check: PASS progress-unit-ctargetreticles
```

The unit stays `NonMatching` (11 functions below 100%), so no `flip_test.sh`. Diff: one line
of `src/MetroidPrime/CTargetReticles.cpp` plus the comment above the function, which said
"four instructions still out" and now records the cast rule instead. No header, no
`configure.py`, no `config/`, no `files.cmake`, no new undefined symbol (a `static_cast` emits
no call), no `asm`.

## `DrawOrbitZoneGroup` (724 B, 0.55%) is now fully mapped, and it is blocked on the port

Nobody had disassembled this one. It is **not** an undiscoverable Echoes-only shape; the whole
body reads out of retail 0x800AD258..0x800AD52C as:

1. `const CIdList& ids = mgr.GetPlayer(mPlayerIndex)->GetPlayerState()->GetIds();`
   (`GetIds__12CPlayerStateCFv` at 0x8008485C is `addi r3,r3,56; blr` - the `CIdList` is at
   `CPlayerState`+0x38; count at +4, `TUniqueId*` at +0xC, the loop is
   `for (TUniqueId* it = ids.mItems; it != ids.mItems + ids.mCount; ++it)` with a 2-byte step).
2. `if (ids.mCount != 0) { gpRender->vtable[28](); CGraphics::SetTevOp(0, "..."); CGraphics::SetTevOp(1, <-.bss>); CGraphics::SetDepthWriteMode(kCompare_Lequal?, false, false); }`
   - the three setup calls are inside the `count != 0` guard and are issued once, before the
   loop. `SetDepthWriteMode(0, 3, 0)` at the top and `(1, 3, 1)` after the loop is the matching
   restore.
3. Per id: `TCastToPtr<CUnknown63>(mgr.GetObjectById(id))`, skip if null;
   `CTexture* tex = fn_80232C20(actor)` (0x80232C20 = `lbz 0x164; beq -> 0; lwz 0x160` = the
   `optional_object<TCachedToken<CUnknown63Obj>>` at +0x158), skip if null.
4. `CVector3f half = fn_80232B34(actor)` (0x80232B34, sret in r3, `this` in r4 - a
   `CVector3f`-returning member; it reads the actor's floats at +0x174/+0x178/+0x17C/+0x180 and
   the `.sdata` pair at -18416/-18432).
5. `CVector3f pos(actor->f54, actor->f58, actor->f5C);` - six `lfs`/`stfs` into the frame,
   **two** of them via `lwz`/`stw` word copies (MW turns the copy of that `CVector3f` into
   integer moves).
6. `float s = CalculateClampedScale(pos, actor->f168, actor->f16C, actor->f170, mgr, mPlayerIndex);`
   - the three arguments come off the actor in the order 0x168, 0x16C, 0x170 (`lfs f3`/`f2`/`f1`).
7. `gpRender->SetModelMatrix(CTransform4f(rotation * CVector3f(s, 0.f, 0.f, 0.f, 0.f, s), pos));`
   - the six-float "scale vector" is a plain `lfs`/`stfs` run, and `__ml__9CMatrix3fCFRC9CMatrix3f`
   is `CMatrix3f::operator*(const CMatrix3f&, const CVector3f&)`.
8. `tex->Load(0, 1)` (0x802C5908 = `CTexture::Load`), `CGraphics::StreamBegin(152)`,
   `CGraphics::StreamColor(CColor::White())`, then **four** `StreamTexcoord`/`StreamVertex`
   pairs from the frame's `CVector3f`s at +12/+24/+36/+48 (texcoords from `half`'s four
   components in the order f31/f28, f30/f28, f29/f28, f29/f30) and `StreamEnd()`.

**Why it cannot land yet:** step 3 needs `TCastToPtr<10CUnknown63>__FP7CEntity` and steps 3-4
need `fn_80232C20`/`fn_80232B34` - none of the three is in
`docs/research/port_link_baseline.txt` (291 symbols; it holds `TCastToPtr<CActor>(CEntity*)`,
`TCastToPtr<CPlayer>(CEntity&)` and five pointer-overload casts, and nothing else). Writing the
body adds three undefined symbols and `tools/link_check.sh --strict` fails on growth, which
fails `goal_check.sh`'s `gate.sh` - the same wall as `CalculateRadiusWorld`, one class wider.
`CUnknown63` itself is declared only inside `src/MetroidPrime/TypesMatch.cpp`, which
`files.cmake` excludes from the host build, so it is not even a type this TU can name.
Getting it needs the class plus the two members in a header the port build sees, i.e. more
than one item; I did not attempt the body.

NEW: port-tcastto-sandwormeye-reference-overload | port | TCastToPtr<12CSandwormEye>__FR7CEntity | the port resolves six `TCastToPtr<T>` casts but not this one (it is absent from the 291-entry baseline), so `CCompoundTargetReticle::CalculateRadiusWorld` - whose body is already written and measured at 96.81% - cannot be decompiled at all; `TCastTo.hpp` only declares the two overloads, so the fix is one explicit instantiation of the `CEntity&` overload next to the eight `TypesMatch` bodies in `src/MetroidPrime/PortGlobals.cpp`, and the same class id as `CAST_TO_IMPL(CSandwormEye, kET_SandwormEye)` at `src/MetroidPrime/TypesMatch.cpp:812`.

## Not attempted, and why

- **`CalculateRadiusWorld` (576 B, 1.35%)** - untouched, for the reason above. The body is
  written down (96.11% -> 96.81% with the six named `CAABox` floats, the `(maxZ, maxY)`
  `min_val` argument order and the explicit `case 2:`), and two runs agree the residue is an
  `f1`/`f2` accumulator swap.
- **`DrawGrapplePoint` / `DrawGrappleGroup` (572/648 B)** - untouched; the sibling item's eighth
  run mapped both and the blockers are `CScriptGrapplePoint`'s layout at +0x153/+0x184 and a
  virtual `GetOrbitPosition` this tree's `CActor` does not model. Confirmed this run that the
  `TCastToPtr<19CScriptGrapplePoint>__FR7CEntity` **reference** overload is likewise absent from
  the baseline (only the `CEntity*` one is there), so both are blocked on the same port gap.
- **`__ct__` (2280 B, 59.40%)**, `Update` (2524 B), `UpdateCurrLockOnGroup` (2628 B),
  `DrawCurrLockOnGroup` (7128 B), `DrawNextLockOnGroup` (2484 B), `DrawSeeker` (1172 B),
  `DrawScanTargetGroup` (1116 B) - untouched, all at their measured positions. Each is an
  Echoes-only body of 1000+ bytes with no donor; not one item's work.
