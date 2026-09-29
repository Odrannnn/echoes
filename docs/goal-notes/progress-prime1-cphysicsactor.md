# progress-prime1-cphysicsactor (lane 4, 2026-09-30)

**`tools/goal_check.sh build/goal/item.json` -> `PASS`.** `matched 9668 -> 9671`,
`main/MetroidPrime/CPhysicsActor: 50 -> 53 / 66 functions`, unit fuzzy 89.89% -> 91.57%,
unit matched-code 58.75% -> 67.23%, **0 functions regressed anywhere in the DOL, no `asm` added**.
The unit stays `NonMatching` (as the item says); no `flip_test.sh` was run.

Diff: `src/MetroidPrime/CPhysicsActor.cpp` only (+9 / -8). Prime 1's
`/run/media/odran/Leo/projects/Restored-projects/Chatgpt/prime-ref/src/MetroidPrime/CPhysicsActor.cpp`
was read for every function; no header from it was copied, and **no class layout was changed** -
the one thing I tried there (the bitfield type, below) made things worse and was reverted.

## 1. Re-measure first: the seed's list is stale

`item.json` names 12 unmatched functions sharing a Prime 1 name. Re-measured against
`build/goal/judge/report.base.json` (the judge's own baseline, not the seed's text):

- `__dt__13CPhysicsActorFv` is **already at 100.00%** in the baseline. It is not a target.
- The remaining 11 are as listed. The four `fn_*` functions (`fn_800EBD24`, `fn_800EB944`,
  `fn_800EA984`, `fn_800EA17C`, all 0.00%) carry retail's placeholder names, so **no source we
  can write will ever match them by name** - see section 4. They are not reachable and are not
  in the 9 functions worked below.

So the real target list is 9 functions, and 3 of the 9 came to 100%.

## 2. The three that matched (each needed a small edit to Prime 1's source)

| function | before | after | Prime 1's source |
|---|---|---|---|
| `GetMoveToORImpulseWR` | 72.86% | **100.00%** | **needed a small edit.** Prime 1 writes `return (1.f / d) * (GetMass() * impulse);`. We had `(GetMass() * impulse) / d`. Same values, different multiply/divide order, and retail wants Prime 1's order. |
| `GetRotateToORAngularMomentumWR` | 79.37% | **100.00%** | **needed a small edit.** Prime 1 builds a whole rotated quaternion and takes `acos` of *that* as a `double`: `const CQuaternion rotated(q.GetScalar(), GetTransform().Rotate(q.GetVector())); const double ac = acos(rotated.GetScalar()); ... static_cast<float>(ac) * 2.0f ...`. We had a `CVector3f rotated` and `acos(q.GetScalar())` as a `float`. The `double` acos and the `static_cast` are load-bearing. |
| `PredictLinearMotion` | 97.33% | **100.00%** | **needed a small edit, all cosmetic except one part.** Prime 1 writes `CVector3f sum = mForce + mMomentum;` inline; we called the header's `GetConstantTotalForceWR()` (same expression, but a different call shape). Also Prime 1's `CVector3f velocity = CVector3f(CalculateNewVelocityWR_UsingImpulses());` and its `0.f` (not `0.0f`) spelling. Any one of the three alone was not enough - I only got 100% with all of them. |

**Lesson for the next Prime-1 lane: Prime 1's source is a strong prior, not an answer.** Three of
nine needed an edit, and in one case (below) Prime 1's source is *wrong* for Echoes.

## 3. Prime 1's `GetBoundingBox` is wrong for Echoes - do not copy it

Prime 1 has

```cpp
CVector3f off = mPrimitiveOffset + GetTransform().GetTranslation();
```

Echoes does **not**. `CActor::mTransform` is at 0x24 and `mPosition` at 0x54, and
`CTransform4f::GetTranslation()` reads `m03`/`m13`/`m23` at **0x30/0x40/0x50**, while
`CActor::GetTranslation()` returns `mPosition` at **0x54/0x58/0x5c**. Retail's
`GetBoundingBox` loads 0x54/0x58/0x5c, so our existing `GetTranslation()` was already right and
Prime 1's version is wrong. I applied Prime 1's version, measured 16.24-16.27% against our
16.30%, and reverted. (The real blocker there is unrelated - see section 5.)

## 4. What the four `fn_*` functions are (characterised so nobody re-derives them)

From `build/G2ME01/asm/MetroidPrime/CPhysicsActor.s`, in retail offset order:

- `fn_800EBD24` (28 B, offset 0x2120): returns `{0.f, 0.f, 0}` by value - a `CVector3f`-ish
  static or a `CAABox`/`CVector3f` static initialiser. No caller inside this unit.
- `fn_800EB944` (100 B, between `__dt__` and `__ct__`): the `CMotionState`-or-`CPhysicsState`
  destructor helper - `Free(p)` guarded by two `beq`/`ble` tests, returning `r30`. 2 branches
  deep in the class destructor chain.
- `fn_800EA984` (92 B, right before `ClearImpulses`): **`mAngularImpulse = CAxisAngle::Identity();
  mMoveAngularImpulse = mAngularImpulse;`** (writes 0x208..0x213, then reloads and writes
  0x1f0..0x1fb). It is the angular half of `ClearImpulses` split out into its own function.
- `fn_800EA17C` (60 B, between `MoveCollisionPrimitive` and `GetCollisionPrimitive`): a 32-byte
  copy from `&arg + 8` to `0x238` - i.e. **`SetCollisionPrimitive(const CCollidableAABox&)`**,
  which our header already declares at line 121 and the .cpp never defines.
  `sizeof(CCollidableAABox) == 0x20` and `mCollisionPrimitive` is at 0x238, so the layouts agree.

**None of these can raise `matched_functions`**: objdiff pairs functions by mangled name, and
retail's are placeholders. `SetCollisionPrimitive` *is* implementable and correct - defining it
would fill a real hole in the class - but it will sit at 0.00% forever. It was deliberately left
alone here because this item's diff must only raise a count.

## 5. The constructor is missing a real 22-instruction block (measured, not attempted)

`__ct__` is 84.22% (892 B) and stays there. Retail's constructor has ~22 instructions our source
does not produce, at the end of the body, after `mNumTicksPartialUpdate`:

```
lwz  r0, 0x8(r30)        ; StepData::unk  (the int at +8)
clrlwi. r0, r0, 31
beq  .L_800EBCBC         ; skip if the sign bit is clear
li   r3, 0x3c            ; 60
lis/addi r4, mskNullBox__6CAABox
li   r5, 0x0
bl   __nw__FUlPCcPCc     ; operator new(0x3c)
mr.  r4, r3
beq  .L_800EBCBC
lis/addi r4, mskNullBox__6CAABox
lhz  r7, 0x0(r25)        ; this->mId
li   r5, 0x1 / li r6, 0x2
bl   fn_80258A9C         ; unknown, (ptr, mskNullBox, 1, 2, id)
neg/or/srwi r0, r0, r4   ; (p != 0)
stb  r0, 0x2c4(r31)      ; our CPhysicsActorUnkB x254_ / void* unk2
stw  r4, 0x2c8(r31)
```

So the class has a lazily-heap-allocated 60-byte member (`x254_` flag + `unk2` pointer at
0x2c4/0x2c8) that retail constructs from `mskNullBox` when `StepData::unk`'s sign bit is set.
Also note retail initialises `mLastFloorPlaneNormal`'s flag byte at 0x2a4 to 0, which our
initialiser list does not spell. Even with this block the function would land near 94%, not
100%, so it would not raise the count. Not attempted; recording it because the next
CPhysicsActor-sized item will otherwise re-derive it. `fn_80258A9C` is still unidentified.

## 6. Measured walls - the remaining 6 are register allocation / scheduling only

**Prime 1's source was already what we have, unchanged, for all six.** For every one the
instruction *multiset* is identical to retail's and only the register numbers and the order of
independent instructions differ. The recurring shape, in all six, is that **retail keeps the
prologue stores (`stw r0`, `stw r31`, `mr r31,r3`) at the top and reuses the lowest free FP
register from f0 up, while mwcceppc 2.7 sinks the prologue stores below the first loads and
allocates from the top down.** Retail is GC/2.7 too (Echoes), so this is not a compiler-version
story - it is a codegen wall for every source spelling I tried, not a missing line of C++.

Spelling -> score (each built and measured; `GetPrimitiveTransform`'s "before" is 25.93%):

| function | before | best tried | spellings tried (score) |
|---|---|---|---|
| `GetPrimitiveTransform` | 25.93% | **55.44%** (kept) | Prime 1 identical 25.93; operand swap `offset + trans` 25.93; one local `vec` 55.44; two locals `trans`,`vec` 55.44; two locals with the sum inline 55.44; `const CVector3f& trans` 25.93. Retail keeps **two** stack objects (the raw `GetTransform().GetTranslation()` at 0x8 and the sum at 0x14, frame 0x30) and ours keeps one; no spelling produced both. |
| `PredictAngularMotion` | 91.57% | 91.57% | Prime 1 identical 91.57; split `orient`/`angMom` locals 71.78; split `vi` out of `v1` 89.44. Retail allocates the CMotionState argument floats f3,f2,f1,f0 then reuses f0; ours uses f6,f5,f4,f3 then f2,f1 - i.e. ours keeps f0-f5 live where retail does not. |
| `GetMotionVolume` | 97.19% | 97.19% | Prime 1 identical 97.19; `max_val(0.f, x)` 97.16; hand-written ternary 97.16. The only difference is which register holds the `0.0f` literal in each of the two `rstl::max_val` calls (retail f0, ours f2). `rstl::max_val` is `(a < b) ? b : a` and matches everywhere else. |
| `CalculateNewVelocityWR_UsingImpulses` | 85.43% | 85.43% | Prime 1 identical 85.43; split into `imp`/`dv` 85.43; operand order swapped 83.48. Exactly two instructions misplaced. |
| `PredictMotion_Internal` | 74.36% | 74.36% | Prime 1's `const CMotionState&` + by-value-getter spelling 7.67 (**much worse** - it deletes both `CMotionState` copy-ctor calls that retail makes); `== false` 74.36; `0 ==` 74.36; `!(bool)` 74.36; inverted `if` 63.85. Blocked by one opcode: retail `extrwi. r0,r0,1,25`, ours `rlwinm. r0,r0,26,31,31` for `!mAngularEnabled` (bit 25 of the byte at 0x168). Changing the bitfield from `bool mAngularEnabled : 1` to `uint mAngularEnabled : 1` (layout identical, and the member is used only in this TU) still produced `rlwinm.` and dropped the unit to 91.51%, so it was reverted. |
| `GetTotalForceWR` | 84.62% | 84.62% | Prime 1 identical 84.62; local `total` 84.62; `GetConstantTotalForceWR()` 84.62; `mMomentum + mForce` 84.15; explicit per-component adds 80.77. 11 of 13 instructions match; one `lfs` is one slot early in ours. |
| `GetPrimitiveOffset` | 70.57% | 70.57% | Prime 1 identical 70.57; `return CVector3f(mPrimitiveOffset)` 70.57; local `off` 70.57; per-component ctor 70.57; `const CVector3f&` deref 70.57. Retail copies 12 bytes with **one** temp register interleaved (`lfs/stfs/lfs/stfs/...`); ours hoists two loads and uses f0 and f1. |
| `GetBoundingBox` | 16.30% | 16.30% | ours 16.30; Prime 1's `GetTransform().GetTranslation()` 16.24 (**wrong semantics**, section 3); split `mn`/`mx` locals 16.05; `trans + offset` 16.24; explicit per-component 16.30. Retail puts the off-sum in f6/f5/f4 and reuses them, ours in f5/f4 and a different store order; retail also emits `addi r5,r1,0x8` at the top, ours in the middle. |

No `WALL:` line is filed: this is a `progress` item and it passed, and the driver parks `WALL:`
items for review. The scores above are the record.

## 7. No `NEW:` items filed

Nothing found raises a count. The `fn_*` names are unreachable (section 4), the constructor gap
(section 5) tops out around 94% and would not pair, and the six scheduling walls (section 6) are
codegen, not missing C++.

## 8. How this was verified

```
$ export MP_TOOLCHAIN_DIR=/run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrimePort
$ ./tools/decomp_build.sh MetroidPrime/CPhysicsActor
All:  29.90% fuzzy, 21.70% matched, 11.74% linked (9671 / 28465 functions)
main/MetroidPrime/CPhysicsActor: 91.57% fuzzy, 67.23% matched (53 / 66 functions)

$ sha1sum build/G2ME01/main.dol
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010  build/G2ME01/main.dol
$ ./tools/probe_sources.sh
probe: 744 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)
$ python3 tools/check_symbol_names.py
checked 502 units; 0 declared names are missing from their object
$ python3 tools/check_decl_order.py --unit MetroidPrime/CPhysicsActor
ok: 1 unit(s) checked, none emits its functions out of retail order
$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 9668 -> 9671   linked 4894 -> 4894
  ok    check_symbol_names.py
  ok    All:  29.90% fuzzy, 21.70% matched, 11.74% linked (9671 / 28465 functions)
  ok    target rose: main/MetroidPrime/CPhysicsActor: 50 -> 53 / 66 functions
  ok    no asm added
goal_check: PASS progress-prime1-cphysicsactor
```

`gate.sh` rewrites the derived block of `docs/HANDOFF.md` as a side effect; per the brief I
reverted that edit, so `git status` shows only `src/MetroidPrime/CPhysicsActor.cpp`.

**Tooling note that saved this item a lot of time:** the per-function side-by-side is
`build/G2ME01/asm/MetroidPrime/CPhysicsActor.s` (retail, dtk) against
`build/binutils/powerpc-eabi-objdump -dr --disassemble=<mangled> build/G2ME01/src/MetroidPrime/CPhysicsActor.o`.
`objdiff-cli diff -p . -u main/<unit> -o -` works but prints one enormous JSON blob for the
whole unit regardless of the symbol you pass. Note also that `objdump` mis-decodes MWCC's
`psq_st`/`lq` pairs as `xxsel`/`xsmsubmsp`; the bytes (`f3 .. .. ..`) are the same, so those are
display noise, not diffs.
