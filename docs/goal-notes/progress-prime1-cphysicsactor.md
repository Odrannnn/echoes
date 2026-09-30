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

---

# progress-prime1-cphysicsactor (lane 5, 2026-09-30) - second attempt

**`tools/goal_check.sh build/goal/item.json` -> `PASS`.** `matched 10259 -> 10261`,
`main/MetroidPrime/CPhysicsActor: 53 -> 55 / 66 functions`, unit fuzzy 91.57% -> 93.36%,
unit matched-code 67.23% -> 69.02%, **0 functions regressed anywhere in the DOL, no `asm` added**.
The unit stays `NonMatching` (as the item says); no `flip_test.sh` was run.

Diff: `src/MetroidPrime/CPhysicsActor.cpp` (+93 / -0) and a new `##` section in
`docs/research/raw_offsets.md`. No header, `configure.py`, `config/` or `files.cmake` change,
and **no class layout was touched** - `CHECK_SIZEOF` for every class involved still holds.

## 1. The seed's premise was wrong, and that is where the two functions came from

The item's `reason` says: read Prime 1's `CPhysicsActor.cpp` and port the eight named functions.
Lane 4 did that and got 3. **That is the whole of what Prime 1 can give**, and it is nearly
exhausted: I re-measured all six remaining named functions and every one of them is at exactly the
score lane 4 recorded, and the diffs are register allocation only (section 4). Prime 1 is a dead
end for this unit.

What is not a dead end is the **`fn_*` part of the unit**, and here **lane 4's section 1 and
section 4 are both wrong**:

> lane 4: "objdiff pairs functions by mangled name, and retail's are placeholders. **None of these
> can raise `matched_functions`** ... `SetCollisionPrimitive` *is* implementable and correct -
> defining it would fill a real hole in the class - but it will sit at 0.00% forever."

**Measured on this tree, that is false.** The repo already has the mechanism, and it is not new:
`src/MetroidPrime/Carve80049244.cpp:146` and `Carve80274774.cpp:29` define a placeholder-named
retail function as `extern "C" void fn_80049244(CIOWinManager* self)`, with `Carve80049244.cpp`'s
own comment explaining why: "Retail's symbol is the unmangled placeholder `fn_80049244` ...
so the definition must not mangle ... The exported name is `fn_80049244`, unmangled, **which is
what objdiff pairs against**." `main.cpp:213` and `CAnimData.cpp:368,372` do the same.

I counted it rather than believing it - **296 of the 5642 placeholder-named functions in
`build/goal/judge/report.base.json` are already at 100%** (they are 2.9% of all functions but 5.6%
of everything already matched). So they pair, they are ordinary work, and four were sitting in
this unit at 0.00%. Two of them are now at 100.00%. Lane 4's conclusion cost this unit two
functions and would have cost every later lane the same.

`SetCollisionPrimitive` is also not hypothetical: `include/MetroidPrime/CPhysicsActor.hpp:121`
declares it and **no TU in the tree defines it**, and `CScriptDebris.o` (0x800D303C) and
`CScriptWater.o` (0x800D9ED4) both call it.

## 2. What landed: two functions, 0.00% -> 100.00%

| function | before | after | retail |
|---|---|---|---|
| `fn_800EA17C` | 0.00% | **100.00%** | 0x800EA17C, 0x3C = 60 B, 15 insns |
| `fn_800EA984` | 0.00% | **100.00%** | 0x800EA984, 0x5C = 92 B, 23 insns |

Both verified byte-for-byte, not just by percentage: I extracted the retail `.text` bytes from
`build/G2ME01/asm/MetroidPrime/CPhysicsActor.s` and compared them to
`powerpc-eabi-objdump -d` of our object, instruction by instruction. `fn_800EA17C` is 15/15
identical. `fn_800EB944` is 25/25 (see section 3). For the two `bl` fields the raw bytes differ
(`48 1E 2A 01` vs `48 00 00 01`) because ours carry a relocation - that is the normal, correct
form and `objdiff` resolves it.

### `fn_800EA17C` = `CPhysicsActor::SetCollisionPrimitive`

`extern "C" void fn_800EA17C(CPhysicsActor* self, const CCollidableAABox& prim)`.

The body copies 32 bytes from `&prim + 8` to `self + 0x238`. How each number was fixed:

- `GetCollisionPrimitive` is `addi r3,r3,0x230` (asm:491) and matches at 100%, so
  **`mCollisionPrimitive` is at 0x230**.
- `CHECK_SIZEOF(CCollidableAABox, 0x28)` with a vtable at +0 puts `CCollisionPrimitive::x4_` at +4
  and `mMaterial` at +8, so **+0x238 is `mCollisionPrimitive.mMaterial`**, and the copy is
  `mMaterial` (8) + `mAabb` (0x18) = 0x20 bytes.
- It **skips the vtable at +0 and `x4_` at +4**. `x4_` is declared in
  `include/Collision/CCollisionPrimitive.hpp:156` and is read and written **nowhere in the tree**
  (grepped `src/` and `include/`).

Three spellings, all built and measured:

| spelling | score |
|---|---|
| `self->SetCollisionPrimitive(prim)` (i.e. `mCollisionPrimitive = prim`, whole object) | 86.67% - one extra `lwz`/`stw` pair, it copies `x4_` |
| a `CAABox`-only store at a hardcoded `+0x238` | 70.57% - wrong: a bare `CAABox` is 0x18 and loses the `mMaterial` half, and the `lfd`/`stfd` pair disappears |
| **a 32-byte same-layout payload struct** (`CMaterialList` + `CAABox`) | **100.00%** |

The payload struct is the `Carve80049244.cpp` convention for exactly this case: a local duplicate
shape, no header edit, no layout change, and `CHECK_SIZEOF` covers the offsets. That is also why
there are 3 raw-offset sites and a new `## src/MetroidPrime/CPhysicsActor.cpp` section in
`docs/research/raw_offsets.md` - kind B debt, named, with the blocker written down. The blocker is
small and I did not take it because it is a header edit outside this item: the member is already in
the header but `private`, and retail's copy is *not* the implicit assignment, so the fix is to
define the already-declared `SetCollisionPrimitive` where it is declared.

### `fn_800EA984` = the angular half of `ClearImpulses`, split out

`extern "C" void fn_800EA984(CPhysicsActor* self)`, body
`*reinterpret_cast<CAxisAngle*>(self + 0x208) = CAxisAngle::Identity(); *(...)(self + 0x1f0) = *that;`

It sits at 0x800EA984, immediately before `ClearImpulses` (0x800EA9E0), and is that function's
angular half as its own out-of-line body: call `CAxisAngle::Identity()`, store at +0x208, reload,
store at +0x1f0. `ClearImpulses` inlines the identical sequence (asm:1127-1148) and matches at
100%, which is what **proves** `mAngularImpulse` = 0x208 and `mMoveAngularImpulse` = 0x1f0 - the
two adjacent `CAxisAngle` members at `CPhysicsActor.hpp:229,231`. `CMorphBall.o` calls it at
0x800CB150.

The one thing that had to be right beyond the obvious: **`CAxisAngle::Identity()` must be an
out-of-line call, not inlined.** The direct spelling with the header's existing
`SetAngularImpulseWR` accessor is impossible for the second member (no accessor exists, it is
private), so both go through the overlay. The first attempt inlined the identity and emitted the
value directly; the call is what produces the `stwu`/`mflr`/`stw r31` prologue and the
`stw`/`lwz` pairs.

## 3. `fn_800EB944`: matched byte-for-byte, then deliberately not landed

**This is the most useful thing in the notes, because it is a real function that reaches 100% and
still cannot be committed here.**

`fn_800EB944` (0x800EB944, 0x64 = 100 B, 25 insns) is the out-of-line destructor chain for
`x254_`, the `CPhysicsActorUnkB` member at `CPhysicsActor.hpp:245`. `__dt__` (0x800EB8C8) calls it
once, at asm:2199. I wrote it and it matches **every byte** (25/25, 100.00%):

```cpp
extern "C" void* fn_800EB944(void* self, short deleting) {
  if (self != nullptr) {
    if (*static_cast< unsigned char* >(self) != 0) {
      fn_800CD460(*reinterpret_cast< void** >(reinterpret_cast< char* >(self) + 4), 1);
    }
    if (deleting > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}
```

Three spellings mattered, each measured:

| what | why |
|---|---|
| `short deleting`, tested `deleting > 0` | retail branches on `extsh. r0,r31` + `ble`, a **sign**-extended halfword. A `bool` parameter measures 93.40% and emits `clrlwi.` + `beq` instead of the `extsh.` |
| `void*` return, **single exit** | retail's epilogue is one `mr r3,r30` before the reloads, so it returns `self`. `void` drops it (24 insns); an early `return self` on the null path adds one back (26 insns). One exit is 25 |
| `fn_800CD460(ptr, 1)` passing a literal `1` | the callee's second parameter is the same sign-tested flag |

**And then `goal_check.sh` failed the gate**, with

```
FAIL  gate.sh
      link_check: STRICT FAIL - regression gate: 251 undefined against a baseline of 250 (GREW)
```

because `fn_800EB944`'s one call, `bl fn_800CD460`, is a **genuinely new undefined symbol**. I
measured this rather than assuming:

- `nm` over all **905** objects in `build/G2ME01/src` finds **no** definition of `fn_800CD460`
  (checked on a stashed clean tree, so the object was not mine).
- It is **not** among the 250 symbols `tools/link_check.sh` already tolerates
  (`build-port-link/link_undefined.txt`), and the clean tree's own `link_check.sh` prints
  "unchanged from baseline (250 undefined, 0 duplicates)".

So the chain is: `fn_800CD460` (0x800CD460, 0x58 B, lives in `CMorphBall.o`, called from three
units - `CPhysicsActor.s:2219`, `CGroundMovement.s:2357`, `auto_03_801F7AD0_text.s:129`) ->
`fn_800CD4B8` (0x800CD4B8, 0x98 B) -> `fn_8033D2F4` (0x8033D2F4, **also undefined**). Defining
`fn_800CD460` here would just move the hole, and it belongs to `CMorphBall.cpp`, not this unit.

So the definition is left out, the full characterisation is in a comment at the definition's
position in the `.cpp`, and it is filed as a `NEW:` item below. **This is the one function in this
unit whose only blocker is another unit's missing symbols.**

## 4. The six named functions: Prime 1 is exhausted, re-measured

Every score below is **this run's** measurement, and each is identical to lane 4's, which is the
useful result: none of them moved, so none of them has a spelling left to try. The diffs are
register allocation and instruction order only - identical instruction multisets. Full per-function
detail is in lane 4's section 6 above and is not repeated; the one correction is below.

| function | this run | note |
|---|---|---|
| `GetMotionVolume` | 97.19% | only the register holding the `0.0f` literal differs, in both `rstl::max_val` calls (retail f0, ours f2) |
| `PredictAngularMotion` | 91.57% | FP allocation across the `CMotionState` argument |
| `CalculateNewVelocityWR_UsingImpulses` | 85.43% | two instructions misplaced |
| `GetTotalForceWR` | 84.62% | one `lfs` a slot early |
| `PredictMotion_Internal` | 74.36% | one opcode: retail `extrwi. r0,r0,1,25`, ours `rlwinm. r0,r0,26,31,31` for `!mAngularEnabled` |
| `GetPrimitiveOffset` | 70.57% | retail interleaves `lfs`/`stfs` through **one** register; ours hoists two loads into f0 and f1. I re-tried lane 4's spellings plus per-component `SetX`/`SetY`/`SetZ` through a named local: all 70.57% |

**Correction to lane 4's section 6, which is a false generalisation and would mislead a later
lane.** It says:

> The recurring shape, in all six, is that **retail keeps the prologue stores (`stw r0`, `stw r31`,
> `mr r31,r3`) at the top ... while mwcceppc 2.7 sinks the prologue stores below the first loads
> and allocates from the top down.**

That is not what the objects show. On `GetMotionVolume` (the function with the largest frame) I
compared the prologues instruction by instruction and **all 11 prologue instructions match in the
same order** - `stwu`, `mflr`, `stw r0`, `stfd f31`, `psq_st f31`, `stfd f30`, `psq_st f30`,
`stfd f29`, `psq_st f29`, `stw r31`, `stw r30` - and only then `lwz r12,0(r4)`. The register
allocation also matches retail's numbering exactly for the first 60 instructions. So there is no
prologue-placement codegen difference to explain; the differences are local to individual
expressions, and the "mwcceppc 2.7 vs GC/2.7" framing has nothing to stand on.

## 5. `fn_800EBD24` (0.00%) - still not reachable, and now I know why

28 bytes, the last function in the unit, and it is a **static constructor**: `lfs f0, <0.0f>` /
`lis r3, <lbl_80410974>` / `stfsu` / `stfs` / `stw r0` / `blr`, writing a 12-byte `CVector3f` to
`lbl_80410974` in `.bss`. It is in `.ctors` (asm:2494).

It cannot be written without **adding a new 12-byte `.bss` object at a fixed address**, and
`lbl_80410974` is not a symbol anything in `src/` or `include/` defines - it is reached by address
from three places in `CCollisionActor.o` (0x80137F2C, 0x80138298, 0x80138654) as a vtable
component. Defining it means claiming a `.bss` range this unit does not own and that a
`splits.txt`/declaration change would have to carry, which is a different kind of change from
anything this item asks for. Left alone deliberately.

## 6. Gates

```
$ export MP_TOOLCHAIN_DIR=/run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrimePort
$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 10259 -> 10261   linked 5039 -> 5039
  ok    check_symbol_names.py
  ok    All:  31.23% fuzzy, 23.55% matched, 11.82% linked (10261 / 28465 functions)
  ok    target rose: main/MetroidPrime/CPhysicsActor: 53 -> 55 / 66 functions
  ok    no asm added
goal_check: PASS progress-prime1-cphysicsactor

$ sha1sum build/G2ME01/main.dol
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010  build/G2ME01/main.dol
$ python3 tools/check_symbol_names.py
checked 505 units; 0 declared names are missing from their object
$ python3 tools/check_decl_order.py --unit MetroidPrime/CPhysicsActor
ok: 1 unit(s) checked, none emits its functions out of retail order
$ python3 tools/check_raw_offsets.py
ok: 160 raw-offset site(s) in 67 file(s), all documented in raw_offsets.md
```

Per-function diff of `build/goal/judge/report.base.json` against `build/report.json`, over all
**28465** functions of the DOL: **0 regressed, 2 improved, 0 new keys**, and the two are
`fn_800EA984 0.0 -> 100.0` and `fn_800EA17C 0.0 -> 100.0`.

`gate.sh` rewrites the derived block of `docs/HANDOFF.md` as a side effect; per the brief I
reverted that edit, so `git status` shows only `src/MetroidPrime/CPhysicsActor.cpp` and
`docs/research/raw_offsets.md`. (`tools/check_docs_claims.py` run on its own therefore reports the
state block as stale against 10261 - that is the driver's rewrite, and `goal_check.sh` passes.)

## 7. Decl order - a trap worth recording, it cost most of this run

The unit **is** permuted on the clean tree - `check_decl_order.py --unit MetroidPrime/CPhysicsActor`
says "would break on a flip" **before** any edit of mine (verified with `git stash`) - yet
`goal_check.sh` passed on it. So the gate only fails when a *new* permutation appears, and adding
two functions made the unit's real permutation newly visible:

```
GATE FAIL: raw-offsets decl-order
  main/MetroidPrime/CPhysicsActor   permuted and not in decl_order.md - add it with a reason
```

and it was **my placement** on top of the pre-existing one. The rule is in the brief (declare
descending by retail offset) but the failure mode is not obvious: **mwcceppc emits in reverse
source order, so the *object* is ascending and the *source* must be descending.** I got the two
backwards twice - `fn_800EA17C` has to sit **between** `MoveCollisionPrimitive` (0x800EA160) and
`GetCollisionPrimitive` (0x800EA1B8) in source, and `fn_800EA984` **after** `ClearImpulses`
(0x800EA9E0) and before `ClearForcesAndTorques` (0x800EAA74). A `struct` definition interleaved
between two function definitions also perturbs it, so the payload struct lives at the top of the
file with its documentation.

A related trap, which cost me a build: **`ninja` will not rebuild after a pure whitespace/reorder
edit through a scripted rewrite if the mtime lands inside the same second**, and the resulting
stale object silently reports the *previous* ordering. `touch` the source before trusting a
`check_decl_order.py` result.

## 8. `NEW:` items

One, and it is the `fn_800EB944` chain of section 3 - a function that reaches 100% and is blocked
only by another unit's missing symbols.

NEW: def-fn-800CD460 | progress | MetroidPrime/Player/CMorphBall | fn_800EB944 in CPhysicsActor.cpp is fully characterised and matches 25/25 bytes, but its only call `bl fn_800CD460` is undefined tree-wide, so defining it grows the link's undefined count 250 -> 251 and fails the gate; fn_800CD460 (0x800CD460, 0x58B) calls fn_800CD4B8 (0x800CD4B8, 0x98B) which calls fn_8033D2F4, also undefined, so the whole chain has to land together. The bodies are 20 and 25 instructions of refcount-bit twiddling (extrwi/rlwimi on bits 26-29 of a byte at +0) plus CMemory::Free - see the full characterisation in the comment at src/MetroidPrime/CPhysicsActor.cpp:78.

Nothing else is filed. The six named functions are codegen walls with the spellings recorded; the
constructor gap (lane 4's section 5) tops out near 94% and would not pair; `fn_800EBD24` needs a
`.bss` claim this unit does not own.

## 9. If you take one thing from this

**When a unit's functions carry retail's placeholder names, they are not unreachable - they are
ordinary work, and the tree already has 296 of them at 100%.** `extern "C"` with `self` as a
first parameter is the mechanism, `Carve80049244.cpp` documents why, and lane 4's "no source we
can write will ever match them by name" is what hid two functions from this unit. Re-measure the
seed's list against the object before believing any part of it, including the parts that look like
settled fact.

---

# progress-prime1-cphysicsactor (lane 3, 2026-09-30) - third attempt

**`tools/goal_check.sh build/goal/item.json` -> `PASS`.** `matched 10485 -> 10486`,
`main/MetroidPrime/CPhysicsActor: 55 -> 56 / 66 functions`, unit fuzzy 93.36% -> 93.69%,
unit matched-code 69.02% -> 69.35%, **0 functions regressed anywhere in the DOL (per-function diff
over all 28465), no `asm` added**. The unit stays `NonMatching` (as the item says); no
`flip_test.sh` was run.

Diff: `src/MetroidPrime/CPhysicsActor.cpp` only (+34 / -0). No header, `configure.py`, `config/`
or `files.cmake` change, and **no class layout was touched** - `CHECK_SIZEOF` is untouched and
`unit_fit.sh` shows the same three pre-existing COMDAT extras.

## 1. What landed: `fn_800EBD24`, 0.00% -> 100.00%

**Lane 5's section 5 is wrong, and it is wrong in the direction that costs a function.** It says:

> `fn_800EBD24` ... cannot be written without **adding a new 12-byte `.bss` object at a fixed
> address** ... Defining it means claiming a `.bss` range this unit does not own and that a
> `splits.txt`/declaration change would have to carry.

**Measured on this tree: no `splits.txt` claim is needed, and no `.bss` claim of any kind.** All
the function needs is for the **symbol to be *defined* in our object under retail's name**, so that
objdiff can resolve the `@ha`/`@l` relocations against the baked retail addresses `3C 60 80 41` /
`D4 03 09 74`. I proved that by defining a 12-byte object named `lbl_80410974` and an
`extern "C" void fn_800EBD24()` that zeroes it: **28/28 instructions, 100.00%, +1.**

Two things were load-bearing and both were measured:

- **the symbol must be defined, not just declared.** With only
  `extern "C" SLbl80410974 lbl_80410974;` the function already matched 100.00%, and the judge then
  failed with `link_check: STRICT FAIL - 251 undefined against a baseline of 250 (GREW)` - the new
  undefined symbol was `lbl_80410974` itself. Adding the definition (uninitialised, so it is a
  common/`.bss` symbol) drops the port's undefined count back to **250, 0 duplicates**.
- **retail's third store is `stw r0, 0x8(r3)`, not `stfs`.** With a `float` third member (both as
  `SLbl80410974` and as a real `CVector3f` with `SetZ(0)`) the compiler emits `stfs f0, 8(r3)` and
  the function measures **77.14%**. The landed struct has `float x; float y; int z;` - that is what
  retail's bytes say. The object is only ever passed by address elsewhere
  (`SMoverData::__ct__(..., lbl_80410974)` at 0x800707F4 and the same in `CScriptActor`,
  `CScriptDoor`, `CScriptDock`, `CScriptPlatform`, `CScriptDebris`, `CCollisionActor`), so the
  member types are not load-bearing for anything else.

Decl order: `fn_800EBD24` is retail's **last** function in the unit (0x800EBD24, `.text:0x2108`,
and it is this unit's only `.ctors` entry), so it sits **first in the source** -
`python3 tools/check_decl_order.py --unit MetroidPrime/CPhysicsActor` -> ok.

`.ctors` remains `claimed 4 / ours 0` in `unit_fit.sh` - the same as on the clean tree (measured),
so nothing regressed. Registering it with `__CTOR_LIST__` would close that gap, but it would put a
real initialiser in the port's startup path for no benefit to the count, so I left it.

## 2. Two false leads in the earlier notes, both measured - do not re-derive them

**(a) `PredictMotion_Internal`'s "one opcode difference" does not exist.** Lane 4 wrote:

> Blocked by one opcode: retail `extrwi. r0,r0,1,25`, ours `rlwinm. r0,r0,26,31,31` ... Changing the
> bitfield from `bool mAngularEnabled : 1` to `uint ... : 1` still produced `rlwinm.`

**Both sides are the same bytes: retail `54 00 D7 FF`, ours `54 00 d7 ff`.** dtk prints that
encoding as `extrwi.` and GNU objdump prints it as `rlwinm.`; it is one instruction, not two, and
the bitfield type has nothing to do with it. (The same pair of mnemonics appears at
`CMorphBall.s:14722`, `54 00 DF FF`.) The **real** remaining difference in that function is
scheduling: ours hoists `lbz r0, 0x168(r4)` + the bit test + `bne` **six instructions** above
`stfd f31` / `fmr f31,f1` / `stw r31` / `mr r31,r4` / `stw r30` / `mr r30,r3`; retail does the test
after them. Everything else in the 39 instructions is identical. Any diff built from
`asm/*.s` vs `objdump` output must compare **bytes**, not mnemonics.

**(b) `CVector3f::operator+`'s named-locals form is load-bearing for a `Matching` unit.** Rewriting
`include/Kyoto/Math/CVector3f.hpp`'s `operator+` from
`float x = ..; float y = ..; float z = ..; return CVector3f(x, y, z);` to
`return CVector3f(lhs.GetX() + rhs.GetX(), lhs.GetY() + rhs.GetY(), lhs.GetZ() + rhs.GetZ());`
breaks the matching build:

```
FAILED: [code=1] build/G2ME01/ok
build/G2ME01/main.dol: FAILED
86 files OK
WARNING: 1 computed checksum(s) did NOT match
```

so it is not a way to move this unit's `operator+`-shaped functions. Reverted.

## 3. `GetPrimitiveTransform`: the correct source is found, and it is 4 instructions from 100%

Not landed (it does not raise the count). Recorded because the next lane should not redo it.
The **fully unnamed** form is the one that reproduces retail's shape:

```cpp
CTransform4f CPhysicsActor::GetPrimitiveTransform() const {
  return CTransform4f::Translate(GetTransform().GetTranslation() + mPrimitiveOffset);
}
```

It emits **27 instructions against retail's 27**, with the same frame (`stwu r1,-0x30`), the same
two stack slots (retail's dead `GetTranslation()` temporary at 0x8..0x10 and the sum at 0x14..0x1c -
the sum is what `addi r4, r1, 0x14` passes to `Translate`), the same six loads in the same order
into f5/f4/f3/f2/f1/f0 and the same three `fadds` with the same operand order. What is left is
**scheduling only**: retail keeps `stw r0,0x34(r1)`, `stw r31,0x2c(r1)`, `mr r31,r3` at positions
3-5 (ours sinks them to 10/14/15) and interleaves `stfs f3,0x8(r1)` before the third `fadds` (ours
groups all three `fadds` first).

**Naming either temporary collapses the frame to 0x20 and drops the score**, which is how the
search narrows: 12 spellings measured, all 55.44% except the unnamed one at **25.93%** - the score
goes *down* because the correct code has the prologue stores in a different place, and objdiff
scores positionally.

| spelling | score |
|---|---|
| `return CTransform4f::Translate(GetTransform().GetTranslation() + mPrimitiveOffset);` | **25.93%** |
| same with `(...)` extra parens / a `CVector3f(...)` wrapper / `mPrimitiveOffset + ...` | 25.93% (byte-identical object) |
| same plus `const CVector3f& o = mPrimitiveOffset;`, `const CPhysicsActor* self = this;`, `const CTransform4f& xf = GetTransform();`, or a dead `const float dead = 0.f;` | 25.93% (byte-identical) |
| `CVector3f vec = GetTransform().GetTranslation() + mPrimitiveOffset; return Translate(vec);` | 55.44% (one slot) |
| `const CVector3f trans = ...; return Translate(trans + mPrimitiveOffset);` | 55.44% (one slot) |
| `CVector3f trans = ...; CVector3f vec = trans + mPrimitiveOffset; return Translate(vec);` (what was in the tree) | 55.44% |
| `CVector3f vec = ...; vec += mPrimitiveOffset; return Translate(vec);` | **70.04%** (27 insns, frame 0x20) |
| `CVector3f a = ...; a += mPrimitiveOffset; return Translate(a);` | 70.04% |

## 4. A TU-local codegen fact that will mislead the next four lanes

`CSwarmBasics::GetOrbitPosition` in `src/MetroidPrime/Enemies/CSwarmBasicsOrbitPosition.cpp` is
**at 100%** with the raw-offset pointer-deref spelling, and its shape is retail's interleaved
`lfs f0 / stfs f0 / lfs f0 / stfs f0 / lfs f0 / stfs f0`. **The identical spelling in
`CPhysicsActor.cpp` and in `CActor.cpp` does not work** - both stay at **70.57%** and emit the
two-loads-then-two-stores shape (`lfs f0 / lfs f1 / stfs f0 / lfs f0 / stfs f1 / stfs f0`).

Measured this run, so nobody repeats it:

| TU | spelling | result |
|---|---|---|
| `CSwarmBasicsOrbitPosition.cpp` | `const CVector3f* pos = (const CVector3f*)((const char*)this + 0x194); return *pos;` | **100.00%** |
| `CPhysicsActor.cpp` | same, offset 0x258 | 70.57% |
| `CActor.cpp`, `GetOrbitPosition` | same, offset 0x54 | 70.57% |
| `CPhysicsActor.cpp`, `GetPrimitiveOffset` | `const CVector3f* o = &mPrimitiveOffset; return *o;` | 70.57% |
| `CPhysicsActor.cpp`, `GetPrimitiveOffset` | `CVector3f out; out.SetX/SetY/SetZ(...); return out;` | 70.57% |
| `CPhysicsActor.cpp`, `GetPrimitiveOffset` | `CVector3f out; out[kDX]/[kDY]/[kDZ] = ...; return out;` | 70.57% |
| `CPhysicsActor.cpp`, `GetPrimitiveOffset` | any variant that goes through `float*` (see below) | 0.00% |

It is **not** the number of functions in the TU: adding 12 extra FP-heavy functions to
`CSwarmBasicsOrbitPosition.cpp` left it at **100.00%** (and moved the symbol from 0x0 to 0x1a4). It
is **not** the class, the offset, the parameter list or the form: three probe functions added to
that TU (`mp_probe_local`, `mp_probe_inline`, `mp_probe_arg`, declared as members and with the
raw-offset, no-local and no-argument forms) **all** emitted the interleaved pattern, while
`CActor::GetOrbitPosition` in the same shape did not. I could not isolate the difference.

Also measured here and worth knowing: **`float*` punning is a trap.** Writing the 12-byte copy as
`const float* s = (const float*)&mPrimitiveOffset; CVector3f out; float* d = (float*)&out;
d[0]=s[0]; d[1]=s[1]; d[2]=s[2]; return out;` makes mwcceppc **stop eliding** `out` into the sret
buffer - it becomes a separate 0x20 stack temp plus a 12-byte copy, and the function drops to
**0.00%**. `fn_42_3A4` in `CScriptMetaree.cpp` and `fn_51_370` in `PuddleSporeAccessors.cpp` get
the interleaved shape because there the destination is a **parameter** pointer, which MWCC never
assumes is a fresh temporary.

## 5. Walls - every score below is this run's measurement

None of these reached 100%. Prime 1's source and the tree's existing source were the starting
point for all of them; the spellings listed are the ones **not** already in lanes 4 and 5.

| function | before | best tried | new spellings measured this run |
|---|---|---|---|
| `GetPrimitiveTransform` | 55.44% | **25.93%** with the correct source (section 3), 70.04% with the highest score | 12, see section 3 |
| `GetMotionVolume` | 97.19% | 97.19% | `0.0f` for the literal; `rstl::max_val(0.f, x)` swapped; `const float z = 0.f` shared by both calls; the max inlined into the `CVector3f(...)`; `const float h/lo` then max; `const` locals - 97.19%, 97.12%, 97.19%, 97.19%, 88.95%, 88.95% |
| `CalculateNewVelocityWR_UsingImpulses` | 85.43% | 85.43% | `mMassRecip * (impulse+moveImpulse) + mVelocity` (83.48%); `(mImpulse+mMoveImpulse) * mMassRecip` (85.43%, byte-identical); `const CVector3f dv = ...` then max (85.43%) |
| `GetTotalForceWR` | 84.62% | 84.62% | `mMomentum + mForce` (84.15%); two `const CVector3f&` locals (84.62%); `CVector3f(mForce) + mMomentum` (84.62%); per-component `SetX/SetY/SetZ` (84.62%) |
| `GetBoundingBox` | 16.30% | 16.30% | `mx` declared before `mn` (16.03%); `mn` before `mx` (16.05%); `GetTranslation() + mPrimitiveOffset` (16.24%); `mBaseBoundingBox.min/.max` direct (**does not compile** - those members are not public) |
| `GetPrimitiveOffset` | 70.57% | 70.57% | section 4 |
| `PredictMotion_Internal` | 74.36% | 74.36% | section 2(a): the "opcode" difference is a disassembler artefact; only the hoisted `lbz`/test/`bne` remains |
| `PredictAngularMotion` | 91.57% | not retried | two prior lanes measured the same 91.57% with identical instruction multisets; 432 bytes of FP allocation across the `CMotionState` return value |
| `__ct__` | 84.22% | not attempted | lane 4's section 5 analysis stands: a 22-instruction missing block, ~94% even with it |

`GetMotionVolume`'s 97.19% is **one register choice**, twice: in both `rstl::max_val` blocks
retail puts the `0.0f` literal in **f0** and the `1.5f` in **f3**, ours puts them in **f2** and
**f0**; every other one of the 148 instructions matches, and the two `stfs`/`fadds` that differ are
consequences of that single choice. `rstl::max_val` is `(a < b) ? b : a` and the compare operands
already match retail (`fcmpo cr0, up, 0.0f`), so it is not the argument order.

`GetTotalForceWR` and `GetPrimitiveOffset` are the closest of all: `GetTotalForceWR` differs by
**one** instruction out of 13 (retail hoists `lfs f3,0x1c4(r4)` above the first `fadds`, ours does
not), and `GetPrimitiveOffset` by two (`retail interleaves load/store through one register, ours
hoists the second load into f1`).

`GetBoundingBox`'s 37 instructions match retail's 37 one for one, including the two stack slots
(max+off at 0x8, min+off at 0x14, `addi r5` hoisted to the top and `addi r4` late - exactly as in
retail). The whole difference is that retail evaluates the **max** first and keeps off.x/off.y/off.z
in f6/f5/f3, while our source (`CAABox(GetMinPoint() + off, GetMaxPoint() + off)`) evaluates min
first. Declaring `mx` before `mn` gets the evaluation order right and then puts the objects in the
wrong slots (16.03%), and naming either one collapses the slot pair - the two requirements are not
simultaneously satisfiable from the source.

## 6. `fn_800EB944`: still blocked, and the blocker is worse than lane 5 said

Re-measured independently, same conclusion. `fn_800CD460`, `fn_800CD4B8` and `fn_8033D2F4` are
undefined in all **909** objects under `build/G2ME01/src`, absent from the 250 tolerated symbols,
and the clean tree's `probe_sources.sh` prints `LINKED (250 undefined, 0 duplicates)`.

One addition to lane 5's chain map: **`fn_8033D2EC`/`fn_8033D2F4` and their four globals sit in an
*unclaimed* gap**, so landing them is a **carve**, not a function definition.
`config/G2ME01/splits.txt:2643` has `Kyoto/Audio/CDSPStreamManager.cpp` ending `.text` at
0x8033D2EC and `.sdata2`/`.sbss` at 0x80419C98, while line 2661 starts
`Kyoto/Animation/CSoundPOINode.cpp` at 0x8033D420; 0x8033D2EC..0x8033D420 and
`lbl_80418C88` / `lbl_80419C98..0x80419CA4` / `lbl_803E05C8` are in the holes. So the whole chain
needs four files, not one.

## 7. Gates

```
$ export MP_TOOLCHAIN_DIR=/run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrimePort
$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 10485 -> 10486   linked 5051 -> 5051
  ok    check_symbol_names.py
  ok    All:  31.77% fuzzy, 24.35% matched, 11.84% linked (10486 / 28465 functions)
  ok    target rose: main/MetroidPrime/CPhysicsActor: 55 -> 56 / 66 functions
  ok    no asm added
goal_check: PASS progress-prime1-cphysicsactor

$ sha1sum build/G2ME01/main.dol
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010  build/G2ME01/main.dol
$ python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json
matched  10485 -> 10486   linked 5051 -> 5051   (+1 functions at 100%, 0 units newly linked)
  +100%    main/MetroidPrime/CPhysicsActor :: fn_800EBD24
no regression
$ ./tools/probe_sources.sh
probe: 754 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)
$ python3 tools/check_symbol_names.py
checked 505 units; 0 declared names are missing from their object
$ python3 tools/check_decl_order.py --unit MetroidPrime/CPhysicsActor
ok: 1 unit(s) checked, none emits its functions out of retail order
$ python3 tools/check_raw_offsets.py
ok: 162 raw-offset site(s) in 69 file(s), all documented in raw_offsets.md
$ ./tools/unit_fit.sh MetroidPrime/CPhysicsActor.cpp
   .text      claimed   8484   ours   8464   retail   8484   SHORT by 20     (clean tree: 8436, SHORT by 48)
```

`gate.sh` rewrites the derived block of `docs/HANDOFF.md` as a side effect; per the brief I
reverted that edit, so `git status` shows only `src/MetroidPrime/CPhysicsActor.cpp`.

`fn_800EBD24` was also verified instruction by instruction, not just by percentage: our object and
`build/G2ME01/asm/MetroidPrime/CPhysicsActor.s:2466-2475` carry the same seven instructions in the
same order (`lfs f0,<0.0f>` / `lis r3,lbl_80410974@ha` / `li r0,0` /
`stfsu f0,lbl_80410974@l(r3)` / `stfs f0,4(r3)` / `stw r0,8(r3)` / `blr`); the `lis`/`stfsu` fields
carry relocations, which is the correct form for an unlinked object and what objdiff resolves.

## 8. `NEW:` items

None filed. The `fn_800EB944` chain (section 6) is lane 5's item and I did not shrink its scope.
`CActor::GetOrbitPosition` looked like a cheap second copy of this item - same shape, same
candidate spelling, and the one spelling known to work is documented in section 4 - so I measured
it: **70.57%, unchanged** (section 4's table), so it is not a candidate and is not filed.

## 9. If you take one thing from this

**A `.bss`-addressing instruction is not the wall it looks like.** Lane 5 wrote off
`fn_800EBD24` because the address "would have to be claimed"; in fact objdiff resolves an
`@ha`/`@l` relocation against a symbol **defined in our own unlinked object**, so the only
requirement is that the symbol exists under retail's name - and the port's undefined count grows
only if you *declare* it without defining it. The second-order lesson is the same one as section
2(a): **compare bytes, not disassembler mnemonics**, in both the retail `.s` and our `objdump`.

## Review rejected run 1 (2026-09-30 20:21:07Z, reviewer worker)

The judge passed this attempt; the reviewer rejected it:

`src/MetroidPrime/CPhysicsActor.cpp:38-41` and `:56` rest on a claim that is false about this tree — `main/auto_08_80410974_bss` is a linked matching-build unit that already defines `lbl_80410974` (12 bytes) and `lbl_80410980` at0x80410974/0x80410980 (`nm build/G2ME01/obj/auto_08_80410974_bss.o`, `main.elf`), so the range is claimed, not an unclaimed gap, and the landed `SLbl80410974 lbl_80410974;` is a second common definition of a symbol the matching link already owns, emitted by a unit that has no `.bss` claim at all (splits.txt:433-437). GNU ld merges commons silently and `.bss` is NOBITS so it never reaches the DOL, meaning the gates cannot detect a shift in `lbl_80410974`/`lbl_80410980` — the 100% score is link-order luck, and files.cmake:929-934 documents exactly this duplicate-definition hazard producing 28 duplicates here. The function body itself is genuine and worth keeping (retail's 7 instructions at asm:2466-2475 are reproduced exactly). An acceptable change keeps the whole gain and follows the repo's own convention for this case (files.cmake:943-945, CScriptRsfAudio.cpp:22-26): leave the MWCC branch as `extern "C" SLbl80410974 lbl_80410974;` so `auto_08_80410974_bss` remains the matching build's owner and objdiff still resolves the `@ha`/`@l` pair, and put the definition in the `#else` host branch so the port link keeps its 250-undefined baseline — plus a comment that says who owns the symbol, rather than claiming nothing does.

Rejected diff: /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal/build/goal/review/progress-prime1-cphysicsactor-L3-1.patch
Review transcript: /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal/build/goal/agent/progress-prime1-cphysicsactor-L3-1-review1-20260930T201541.jsonl

## Fix round 1

The reviewer was right and the comment was false. Re-measured the claim on this tree before
changing anything:

```
$ nm build/G2ME01/obj/auto_08_80410974_bss.o
0000000c C lbl_80410974
00000018 C lbl_80410980
$ nm build/G2ME01/main.elf | grep 804109
80410974 B lbl_80410974
80410980 B lbl_80410980
```

`main/auto_08_80410974_bss` is a linked unit of the matching build and already owns the symbol, so
0x80410974..0x80410998 is a **claimed** range (`config/G2ME01/splits.txt:425` ends
`CDecalManager.cpp`'s `.bss` at 0x80410974; this unit's own split at `:433-437` claims no `.bss`),
not the "unclaimed gap" the old comment called it. The landed definition was therefore a second
common definition of a symbol the matching link already owns - exactly the hazard
`files.cmake:929-934` documents as producing 28 duplicates here, and one no gate can see, because
GNU ld merges commons silently and `.bss` is NOBITS so the shift never reaches the DOL.

**Changed, in `src/MetroidPrime/CPhysicsActor.cpp` only:**

- `lbl_80410974` is now `extern SLbl80410974 lbl_80410974;` under `#ifdef __MWERKS__` and defined
  only in the `#else` host branch - the convention `files.cmake:943-945` and
  `src/MetroidPrime/ScriptObjects/CScriptRsfAudio.cpp:22-26` already use for a loader variable a
  unit's split does not claim `.bss` for. The MWCC branch is the retail source token for token, so
  `auto_08_80410974_bss` stays the matching build's owner; the host branch keeps the port link at
  its 250-undefined baseline, since no dtk split object exists there.
- The comment now says **who owns the symbol**, with the two `nm` commands that prove it, instead
  of claiming nothing defines it. The false sentence ("an unclaimed `.bss` gap ... so nothing else
  in the tree defines or names it") is gone.

Nothing else was touched: `fn_800EBD24`'s body, the `SLbl80410974` layout (`float x; float y;
int z`, which is what retail's `stw` of a word zero requires), its `extern "C"` pairing and its
placement first in the file are all unchanged, and no other hunk of the diff was edited.

**objdiff does not need the definition - the declaration alone is enough to pair the function**,
which is the point of the change: the relocation is what resolves the address, not the storage.
Our object now carries the reference rather than the definition:

```
$ nm build/G2ME01/src/MetroidPrime/CPhysicsActor.o | grep 804109
         U lbl_80410974
$ objdump -dr --disassemble=fn_800EBD24 build/G2ME01/src/MetroidPrime/CPhysicsActor.o
    20f4: c0 00 00 00  lfs     f0,0(0)
    20f8: 3c 60 00 00  lis     r3,0
                    20fa: R_PPC_ADDR16_HA  lbl_80410974
    20fc: 38 00 00 00  li      r0,0
    2100: d4 03 00 00  stfsu   f0,0(r3)
                    2102: R_PPC_ADDR16_LO  lbl_80410974
    2104: d0 03 00 04  stfs    f0,4(r3)
    2108: 90 03 00 08  stw     r0,8(r3)
    210c: 4e 80 00 20  blr
```

Same seven instructions, same order, same relocations, still 28/28 bytes against
`build/G2ME01/asm/MetroidPrime/CPhysicsActor.s:2466-2475`.

**Gates after the fix** (counts are unchanged from the rejected run, so the whole gain is kept):

```
$ ./tools/decomp_build.sh MetroidPrime/CPhysicsActor
All:  31.77% fuzzy, 24.35% matched, 11.84% linked (10486 / 28465 functions)
main/MetroidPrime/CPhysicsActor: 93.69% fuzzy, 69.35% matched (56 / 66 functions)
   fn_800EBD24 100.00%   (report.json: fuzzy_match_percent 100.0, size 28)
$ sha1sum build/G2ME01/main.dol
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
$ ./tools/probe_sources.sh
probe: 754 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)
$ python3 tools/check_symbol_names.py
checked 505 units; 0 declared names are missing from their object
$ python3 tools/check_decl_order.py --unit MetroidPrime/CPhysicsActor
ok: 1 unit(s) checked, none emits its functions out of retail order
$ python3 tools/check_raw_offsets.py
ok: 162 raw-offset site(s) in 69 file(s), all documented in raw_offsets.md
```

`matched 10485 -> 10486` and the target at `56 / 66` are exactly what the judge accepted, and the
250-undefined baseline is intact, so the host-only definition is still doing its job.
