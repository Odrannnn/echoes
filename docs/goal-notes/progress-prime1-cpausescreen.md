# progress-prime1-cpausescreen

`kind: progress`, target `MetroidPrime/CPauseScreen` (unit stays `NonMatching`; `flip_test.sh` not
run, as the item says).

## Result

`matched_functions` for `main/MetroidPrime/CPauseScreen`: **4 -> 6** (of 84). Project-wide
`matched_functions` **9977 -> 9979**. No function anywhere got worse (measured by diffing every
`(unit, function)` fuzzy-match percentage in `build/report.base.json` against `build/report.json`:
0 worse, 2 better).

## What I changed

One file, `src/MetroidPrime/CPauseScreen.cpp`, two lines: the `looped` argument of the two
`CSfxManager::SfxStart` calls in `SetPanSound` and `SetZoomSound` went from `true` to `false`.

## Per function, before % -> after %

| function | before | after | note |
|---|---|---|---|
| `SetPanSound__12CPauseScreenFb` | 99.97 | **100.00** | matched |
| `SetZoomSound__12CPauseScreenFb` | 99.97 | **100.00** | matched |
| every other function in the unit | unchanged | unchanged | 0 worse, verified |

## Prime 1's source was not usable for this unit

The item's `reason` points at Prime 1's `src/MetroidPrime/CPauseScreen.cpp` as a reference. It is a
**different class**, not an older version of the same one:

- Prime 1's `CPauseScreen` is the pause-subscreen container. Its constructor takes
  `(int subscreen, const CDependencyGroup& suitDgrp, const CDependencyGroup& ballDgrp)` and it owns
  `rstl::auto_ptr<CPauseScreenBase> mScreens[2]`, `mStrgPauseScreen` ("STRG_PauseScreen") and a
  `FRME_PauseScreen` frame. It switches between LogBook / Options / Inventory.
- Echoes' `CPauseScreen` is the scan-network screen. Its constructor takes no arguments, `CHECK_SIZEOF`
  is 0x56c, and it owns `CScanTree mScanTree`, eleven model slots, twelve history rows and a
  `CQuitGameScreen`.

The four names the seeder paired across the two (`InitializeFrameGlue`, `CheckLoadComplete`, `Draw`,
`__dt__`) are name collisions between unrelated functions, and the seeder itself measured **0 with the
same size**. So there was no source to adapt; I worked from the target's own disassembly instead.

## The two matches, and how they were found

Both were at 99.97% - one 4-byte instruction away. objdiff's per-function JSON has `left` = retail
and `right` = our build (verified by disassembling both objects directly, not assumed).

```
=== SetPanSound__12CPauseScreenFb  ours=(...,'144',99.97)  retail=(...,'144',99.97)
**   17 retail=li r9, 0x0   ours=li r9, 0x1
```

`r9` is `SfxStart`'s 6th argument, `bool looped`
(`include/Kyoto/Audio/CSfxManager.hpp:195`). Retail passes `0`. The other 35 instructions were
already identical, so this was the whole difference. Same for `SetZoomSound`.

**Lesson (general, not GameCube-specific):** when two functions sit at 99.9%, do not read them as
"nearly done, probably a register-allocation detail". Filter the diff to instructions that are *not*
branches and not symbol names, and the residue is almost always a single wrong literal. Branch
targets differ in every such function and are pure address noise.

## What is blocked, with the evidence

### `__dt__12CPauseScreenFv` - 98.38%, and the remaining 1.6% is not source-tunable

The destructor is a compiler-generated member teardown. Three real differences remain, none of which
a source edit reaches:

1. **Two outlined teardowns.** Retail calls `fn_80038F0C` and `fn_8020D820`; we emit weak inline
   copies of `~rstl::vector<CLight>` and `~rstl::list<rstl::pair<bool,TToken<CTexture>>>`. Retail's
   linker kept a different copy. Same shape as the `construct.hpp` comment at line 65 ("Declared
   before its inline definition ... so an out-of-line copy is weak rather than local") - that trick
   was applied to `destroy_impl` but these two instantiations still land differently.
2. **`x1fc_` element type.** Retail destroys `__dt__Q24rstl36vector<i,...>` (`vector<int>`); we emit
   `vector<unsigned>`. The header has `rstl::vector<CAssetId> x1fc_` and `CAssetId` is `typedef uint`
   (`include/Kyoto/SObjectTag.hpp:10`). Changing the member's type to `int` is a real fix but it
   changes a class layout, which the item forbids without a measured diff at every use, and `x1fc_`
   has no user in the unit to confirm against.
3. **`mModelTokens` teardown.** Retail loads each `optional_object<CToken>` and calls
   `__dt__6CTokenFv` with `r4 = -1` and no null test; we emit a `cmplwi`/`beq` null guard and `r4 = 0`.
   This is `optional_object`'s destructor shape (`include/rstl/optional_object.hpp:22`, whose
   `clear()` is deliberately commented out with the note "Makes ~CScriptHudMemo match"). Changing it
   here would move every other `optional_object` user.

I did not file a `NEW:` for this: the blocker is three independent codegen questions, not one
reachable function, and none of the three is a single spelling away.

### The three functions the item named

`InitializeFrameGlue` (0.14%, 2940 bytes), `CheckLoadComplete` (0.15%, 3848 bytes) and
`Draw` (0.26%, 1512 bytes) are `// TODO:` stubs in this repo. Their retail bodies are 1-4 KB each and
reference `CTweakGui` accessors, `CScanTree`, the GUI widget tree and the model pipeline. Prime 1's
versions of the same three names are unrelated code (see above), so they give no starting point. This
is a several-day job per function, not one item.

### `SetFog` (2.70%, 148 bytes) - fully decoded, but its callees are unnamed

This one I *could* read completely off the disassembly, and the body is short:

```
if (enabled) {
  CColor& c = <fn_802161C8>();   // lwz r3,0(r3); addi r3,r3,0x5a8  -> a CColor at tweak+0x5a8
  float startz = <fn_802161D4>(); // lfs f1,0x5a4(r3)
  float endz   = <fn_802161E0>(); // lfs f1,0x5a0(r3)
  CGraphics::SetFog(kRFM_PerspRevExp, endz, startz, c);   // li r3,0x2
} else {
  CGraphics::SetFog(kRFM_None, <lbl_8041D794>, <lbl_8041D794>, CColor::Black());  // li r3,0x0
}
```

The blocker is the three accessors. `fn_802161C8/D4/E0` live in `main/auto_03_80213CB8_text` and have
**no GC name**; that object has 30 named `CTweakGui` accessors but not these three, and
`src/MetroidPrime/Tweaks/` has no `CTweakGui::` definition at all - `CAutoMapper.cpp:1723` already
calls `GetMapBackgroundCycleTime()` as an undefined symbol that resolves into that auto unit. So
writing this body means inventing three names in a unit that is `NonMatching` and unnamed, i.e.
guessing identifiers the linker cannot check. That is the "plausible-looking stand-in" the brief
warns about, so I left it alone and recorded it instead.

## Gates (all re-run after the change)

```
sha1sum build/G2ME01/main.dol   -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010   (expected)
./tools/probe_sources.sh        -> 749 files, 0 failed, 0 errors; LINKED (250 undefined, 0 duplicates)
python3 tools/check_symbol_names.py -> 503 units, 0 missing
./tools/decomp_build.sh         -> All: 30.73% fuzzy, 22.92% matched, 11.74% linked (9979/28465)
all 86 RELs                     -> cmp-identical to orig/G2ME01/files/RelProd/
```

`All:` did not fall (9977 -> 9979 matched functions). The diff adds no `asm`, and no initialisation
was removed.

## Notes for the next run

- The helper I used is at `.tmp/opencode/od.py` (gitignored, not part of the diff). It runs
  `objdiff-cli diff -p . -u main/MetroidPrime/CPauseScreen -o - <symbol>` and prints a
  retail-vs-ours side-by-side. **Left is retail, right is ours** - the opposite of the usual
  intuition, and getting it backwards sends you after phantom bugs.
- Retail symbols for unnamed functions can be disassembled straight out of
  `build/G2ME01/obj/<unit>.o`; the DOL itself is not objdump-readable (`file format not
  recognized`), so use the split object.
- Two lines of the 84 functions in this unit are within one instruction of 100% and both are now
  taken. The next cheapest candidates by size are `fn_80205BE4` (32 B), `fn_8020AFFC` (40 B),
  `fn_80205C04` (40 B) - but all are unnamed retail functions with no declaration to write against.

---

# Second run (lane 2, 2026-10-01)

`kind: progress`, target `MetroidPrime/CPauseScreen`, unit stays `NonMatching`; `flip_test.sh` not
run, as the item says.

## Result

`matched_functions` for `main/MetroidPrime/CPauseScreen`: **6 -> 9** (of 84). Project-wide
`matched_functions` **11506 -> 11509**; `All:` fuzzy 32.96% (unchanged), matched code 25.81% ->
25.82%. No function anywhere got worse (measured by diffing every `(unit, function)`
fuzzy-match percentage in `build/report.base.json` against `build/report.json`): **0 worse, 4
better, 0 new, 0 removed**.

`./tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS progress-prime1-cpausescreen`**.

## STALE: the previous run's `SetFog` blocker - the accessors are named after all

The last run wrote (notes lines 110-117) that `SetFog`'s three `CTweakGui` accessors "live in
`main/auto_03_80213CB8_text` and have **no GC name** ... `src/MetroidPrime/Tweaks/` has no
`CTweakGui::` definition at all", and that writing the body "means inventing three names in a unit
that is `NonMatching` and unnamed".

**That is no longer true on this tree, and the blocker was never real.** Re-measured:

- `powerpc-eabi-objdump -dr build/G2ME01/obj/MetroidPrime/CPauseScreen.o | grep GetLogBook` and
  the retail disassembly of `SetFog__12CPauseScreenCFb` name all three callees:
  `GetLogBookFogColor__9CTweakGuiCFv`, `GetLogBookFogFar__9CTweakGuiCFv`,
  `GetLogBookFogNear__9CTweakGuiCFv`.
- `include/MetroidPrime/Tweaks/CTweakGui.hpp:254-256` **already declares** all three, and
  `src/MetroidPrime/Tweaks/CTweakGui.cpp:602-606` already defines them. So they were written
  between the two runs (or the last run read the wrong unit). The linker can check every one of
  them; nothing had to be invented.

The same applies to `GetDefaultModelPosition`, which calls
`GetLogBookModelXOffset__9CTweakGuiCFv` and `GetLogBookModelZOffset__9CTweakGuiCFv` - both
declared at `include/MetroidPrime/Tweaks/CTweakGui.hpp:75-76`. **Lesson: a "no GC name" claim
about a symbol is cheap to re-measure and expensive to inherit** - `objdump -dr` on the split
object prints the relocation's target symbol, so the question "is this nameable?" costs one
command. Do not conclude a symbol is unnameable from a previous run's note; check
`nm`/`objdump -r` on `build/G2ME01/obj/<unit>.o` first.

Also **STALE**: the seeder's premise that this is Prime 1's pause-subscreen class. The last run
correctly refuted that (Echoes' `CPauseScreen` is the scan-network screen). Confirmed again: the
four names the seeder paired across the two are collisions between unrelated functions, and the
reference source at `prime-ref/src/MetroidPrime/CPauseScreen.cpp` is a different class with a
different constructor and different members. **Do not read Prime 1's source for this unit.**

## What I changed

Three files, all in this unit's own pair:

- `include/MetroidPrime/CPauseScreen.hpp:136` - `rstl::vector< CAssetId > x1fc_` becomes
  `rstl::vector< int >` (see the destructor section; it removes one objdiff
  `DIFF_ARG_MISMATCH` but does **not** move the reported percentage).
- `src/MetroidPrime/CPauseScreen.cpp:3-7` - added `#include "MetroidPrime/Tweaks/CTweakGui.hpp"`.
- `src/MetroidPrime/CPauseScreen.cpp` - filled in `SetFog`, `GetDefaultModelPosition`,
  `GetModelPosition`, `EnsureTextureLoaded` (they were `// TODO:` stubs returning `false` /
  `CVector3f::Zero()`).

## Per function, before % -> after %

| function | before | after | Prime 1's source |
|---|---|---|---|
| `SetFog__12CPauseScreenCFb` | 2.70 | **100.00** | n/a - unrelated name collision |
| `GetDefaultModelPosition__12CPauseScreenFv` | 17.17 | **100.00** | n/a - unrelated name collision |
| `GetModelPosition__12CPauseScreenCFv` | 8.51 | **100.00** | n/a - unrelated name collision |
| `EnsureTextureLoaded__12CPauseScreenFRC6CToken` | 2.64 | 92.26 | n/a - unrelated name collision |
| every other function in the unit | unchanged | unchanged | 0 worse, verified |

Prime 1's source was **not used at all**: all four bodies were read off this unit's own
disassembly (`build/G2ME01/obj/MetroidPrime/CPauseScreen.o`, disassembled per symbol).

## The three exact matches, and how each was found

`objdiff-cli diff -p . -u main/MetroidPrime/CPauseScreen -o - <symbol>` emits **JSON**, not text.
`.tmp/opencode/od2.py` (gitignored) parses it and prints a retail-vs-ours table. **Left is
retail, right is ours** - the opposite of the usual intuition.

All four are the same shape of problem: a short body whose only differences are literals and
branch polarity.

### `SetFog` - `li r3, 0x2` and the wrong enumerator name

First attempt, `kRFM_PerspRevExp`, gave 99.84% with exactly two diffs:

```
DIFF_ARG_MISMATCH R|li r3, 0x2                    O| li r3, 0x6
DIFF_ARG_MISMATCH R|lfs f1, lbl_8041D794@sda21    O| lfs f1, @1010@sda21
```

`GX_FOG_PERSP_REVEXP = 6` (`include/dolphin/gx/GXEnum.h:521`) but retail passes **2**, which is
`GX_FOG_PERSP_LIN` -> `kRFM_PerspLin`. `kRFM_PerspLin` gives 100.00%. The second diff was sda
*label naming* only (same 0.0f value) and vanished with it.

### `GetDefaultModelPosition` - read straight off the disassembly, 17.17% -> 100%

Retail (`0xbc..0x114`, 92 bytes) is 20 instructions: two `CTweakGui` calls and three `stfs`.

```
CVector3f CPauseScreen::GetDefaultModelPosition() {
  return CVector3f(gpTweakGui->GetLogBookModelXOffset(), 0.f,
                   gpTweakGui->GetLogBookModelZOffset());
}
```

Note the **call order is Z first, then X** (`fmr f31,f1` parks Z, then X is loaded and stored at
+0), which is what MWCC's right-to-right evaluation produces for a three-argument constructor.
Trying X first would have swapped the two `stfs`.

### `GetModelPosition` - two chained `CVector3f::Lerp`s, 8.51% -> 100%

188 bytes of float code, fully determined once the member offsets are known. Retail reads
`0x4b8`, `0x500`, `0x4d4`, `0x4d8`, `0x4dc` off `this` and four SDA constants
`lbl_8041D794/798/79C/7A0` = **0.0, 1.0, -0.6, -0.5** (read with
`python3 tools/dol_read.py 0x8041D790 0x20 orig/G2ME01/sys/main.dol` - the DOL is big-endian and
the file offset for a vaddr is not `vaddr - base`; use the tool).

Those offsets are this header's own layout, confirmed by compiling an offset probe with the
toolchain's own compiler (`.tmp/opencode/off.o`):

```
0x4a8 mQuitScreen  0x4b0 mLeftStickIcon  0x4b4 mRightStickIcon  0x4b8 mLegendHiddenAmount
0x4bc x4bc_        0x4c0 x4c0_          0x4c4 mSelectionDelay  0x4c8 mSelectionHighlight
0x4cc mLeftRepeat  0x4d0 mRightRepeat    0x4d4 mModelPan        0x4e0 mModelCenterOffset
0x4ec mModelScale  0x4f8 mModelPitch     0x4fc mModelYaw       0x500 mModelZoomAmount
0x504 mModelFade   0x508 x508_           0x524 mLights         0x534 mActorLights
0x538 mModelTransform                        sizeof = 0x56c
```

**The members are private, so `offsetof` from a probe file does not compile** ("illegal access to
protected/private member"). `#define private public` before the include works, but the cheapest
route is `include/static_assert.hpp`'s `CHECK_OFFSETOF(cls, member, n)`, which is a no-op off
`__MWERKS__` and an array-size check on.

The body is two lerps, and the register-level reading is worth recording because the first guess
was wrong:

```
defaultPos = GetDefaultModelPosition()          // (x = XOffset, y = 0, z = ZOffset)
hiddenPos  = defaultPos + (0.0, -0.6, -0.5)
pos        = Lerp(defaultPos, hiddenPos, mLegendHiddenAmount)
return     = Lerp(pos, mModelPan, mModelZoomAmount)
```

`CVector3f::Lerp` (`include/Kyoto/Math/CVector3f.hpp:52`) is already exactly the shape retail
emits (`a*inv + b*v` per component, `inv = 1-v`), so **reusing the existing helper is what makes
it match** - hand-rolling the same arithmetic does not. The result's `.x` is built from
`defaultPos.z` and `.y` from `defaultPos.x`: the default position is **swizzled**, not used
straight. That is the trap here; reading the three `lfs` off `8(r1)/12(r1)/16(r1)` in order and
assuming `x,y,z` gives a body that scores 0.

The four SDA constants come out as `@1067..@1070` instead of `lbl_8041D79x`; the *values* are
identical and objdiff still scores 100.00%, so the label name is not part of the match.

### `EnsureTextureLoaded` - 2.64% -> 92.26%, four spellings measured

Retail (212 bytes) is a TXTR type test, a `CToken` copy, a `GetBitmapDataStatus()` switch and a
`TryReloadBitmapData()`. The type test is the giveaway for the spelling:

```
lwz r5, 0(r4)        ; token.mObjRef
lwz r3, 0xc(r5)      ; mObjRef->mObjTag.type  (offset 0xc in CObjectReference)
subis r0, r3, 0x5458 ; cmplwi r0, 0x5452      ; == 'TXTR'
```

`0x12` into `CObjectReference` is `mObjTag`, so this is `token.GetTag().type` - and
`CToken::GetTag()` is `const`, so it works on the `const CToken&` parameter. Getting this wrong
(`token.GetObj()->GetType()`, as I first wrote it) does not compile: `CToken::GetObj()` is
non-const and `IObj` has no `GetType`.

The four spellings, in the order tried (each is a *cumulative* edit, not a fresh start):

| spelling | score |
|---|---|
| `if (type != 'TXTR') return true;` + `else if` chain + `return x ? true : false` | 69.55% |
| `... if (type != 'TXTR') return true;` + `else if` chain + `if (x) return true; return false;` | 83.23% |
| + two independent `if (status == 1)` / `if (status == 2)`, not `else if` | 88.83% |
| + `if (type == 'TXTR') { ... } return true;` (invert the outer test) | 90.25% |
| + `if (!x) return false; else return true;` | 92.26% |
| + wrap the two status tests in `if (status != 0)` | **92.26%** (best) |
| final: `return x;` instead of the `if/else` | 86.13% (worse - do not) |

The two that mattered are the **inverted outer test** (it turns retail's `bne` into ours) and
**two separate `if`s rather than `else if`** (retail falls through from the `status == 1` call
into the `status == 2` test, which an `else if` cannot express). The last 7.7% is instruction
scheduling: retail loads `token.mObjRef` *after* the prologue and we hoist it above, and two
branch destinations differ. That residue is not worth more spellings - `bne 0x9034` vs
`bne 0x548` is address noise.

## What is still blocked, with the evidence

### `__dt__12CPauseScreenFv` - 98.38%, and it **cannot** reach 100% from source

Re-measured. Ten instructions of 259 differ, in three independent groups:

1. **`bl fn_8020D820` vs `bl __dt__Q24rstl69list<pair<bool,TToken<CTexture>>, ...>Fv`**
   (1 instruction, 1 of the 6 `DIFF_ARG_MISMATCH`).
   **This is unreachable and is the reason the destructor can never be 100%.** Retail's object
   *defines* `fn_8020D820` at `0xb8d4`, 156 bytes - byte-for-byte the `~list` destructor
   (`powerpc-eabi-objdump -dr --disassemble=fn_8020D820`, same 156-byte shape: `mr. r28,r3`,
   `lwz r31,4(r28)` walk, `__dt__6CTokenFv` + `Free`). The difference is only that retail's
   symbol is **unnamed in `config/G2ME01/symbols.txt`**, so objdiff prints `fn_8020D820` and
   scores the `bl` as an argument mismatch. There is no C++ spelling that emits the name
   `fn_8020D820`, and the fix would be a `config/` edit, which the item forbids. **Do not spend
   a run on this.**
2. **`optional_object<CToken>` element teardown, twice** (2 `DIFF_INSERT` + 2 `li r4` mismatches,
   at `mLeftStickIcons` and `mRightStickIcons`).
   Retail's inlined `~reserved_vector<TToken<T>,9>` emits, per element,
   `mr r3, elem; li r4, -1; bl __dt__6CTokenFv` with **no null test**. Ours emits
   `cmplwi elem,0; beq skip; mr r3, elem; li r4, 0x0; bl __dt__6CTokenFv` - a null test on an
   inline-storage address that can never be null, and the *non*-deleting flag. That comes from
   `reserved_vector::destroy_elements()` -> `destroy_impl` -> `in->~T()` in
   `include/rstl/reserved_vector.hpp:120` and `include/rstl/construct.hpp:70`.
   **Not attempted**: both headers are shared by every class with a `reserved_vector` /
   `optional_object` member, and `include/rstl/optional_object.hpp:22` already carries the note
   "Makes ~CScriptHudMemo match" on the commented-out `clear()`. Changing the shared destroy path
   to chase 4 instructions in one destructor is a bad trade, and it cannot reach 100% anyway
   because of (1).
3. **`x1fc_`'s element type - FIXED, and it is worth recording that it bought nothing.**
   Retail destroys `__dt__Q24rstl36vector<i, ...>`; we emitted
   `__dt__Q24rstl37vector<Ui, ...>`. Changing `rstl::vector< CAssetId >` (`CAssetId` is
   `typedef uint`, `include/Kyoto/SObjectTag.hpp:10`) to `rstl::vector< int >` in this unit's
   own header **removes that `DIFF_ARG_MISMATCH`** - measured - and **leaves the reported
   percentage at exactly 98.384315**. The bytes are identical either way (the call is a reloc to
   a weak symbol), so objdiff's fuzzy score cannot see it. `CHECK_SIZEOF(CPauseScreen, 0x56c)`
   and every offset are unaffected (`int` and `uint` are both 4-byte, 4-aligned) - verified by
   the offset probe above. Kept because it is a measured fix, but **a future run should not
   expect it to move a number.**

### `__ct__12CPauseScreenFv` - 70.70%, 2560 bytes, 460 of 693 instructions differ

The single largest target in the unit and still a multi-session job. Two structural facts from
the diff, neither of which is a spelling:

- Retail hoists **string-literal addresses out of `.rodata` into `.sdata`**
  (`lwz r5, lbl_8041D70C@sda21` for `"TXTR_ScanNetworkSelected"`), where we emit
  `lis r3, @stringBase0@ha; addi r5, r3, @stringBase0@l; addi r5, r5, 0x19`. That is a codegen
  difference in how the string pool is addressed, and it shifts every stack slot in the function
  (`stwu r1, -0x100` vs `-0xd0`, and 0x94 vs 0x78 for the first temp). **Fixing the address form
  is a prerequisite for the rest; until it does, every offset in the function is wrong and no
  amount of member reordering will help.**
- The `// TODO:` at `src/MetroidPrime/CPauseScreen.cpp:105` is real work: retail reads
  `LogbookLegendVisible` and then populates and locks the two stick-icon sets from player
  tweaks, which this body does not do. **Filling in a body that still omits that is a stub that
  announces itself only in a comment - do not.**

### `RestoreTextures` (236 B, 1.69%) and `TouchVisibleNodes` (348 B, 1.15%)

Both are fully decodable and both are blocked on the same thing: retail calls `fn_8020AE24`
(a 64-byte list-node helper with **no GC name**, in the same "only retail" bucket as `fn_8020D820`)
and `RestoreTextures` additionally reads `CTexture` bytes at `+0x8` and `+0xa` as bitfields
(`lbz r3,8(r30)`; `lbz r0,0xa(r31); rlwinm. r0,r0,27,31,31`) that no accessor in
`include/Kyoto/Graphics/CTexture.hpp` exposes. Writing it would mean naming an unnamed helper and
poking a private bitfield by offset - the "plausible-looking stand-in" the brief warns about. Left
as TODOs.

### The 41 `fn_*` functions retail defines and we do not

`powerpc-eabi-nm` on both objects: retail has 84 `.text` symbols, we have 72. The 41 retail-only
names are all `fn_8020D054..fn_8020D9D8` (the out-of-line weak copies of `~rstl::vector<CLight>`,
`~TCachedToken<CTexture>`, `~reserved_vector<TToken<CTexture>,9>`, `~list<pair<bool,TToken<CTexture>>>`,
...), plus `fn_80205BAC`..`fn_8020CE88`. They are **the same code under the mangled names we do
emit** (each was checked: `fn_8020D820` == our `~list`, 156 bytes each), so they are the
*unnamed-in-config* half of issue (1) above and **not** separate work. The other direction - 32
symbols only we define, all `__dt__<template>` instantiations plus `GetResourceIdByName` - is the
same thing from our side. **Consequence: this unit's `.text` can never be byte-identical, so
`Matching` is out of reach for it regardless of the bodies.** Worth knowing before anyone
spends a run trying to flip it.

## Gates (all re-run after the change)

```
sha1sum build/G2ME01/main.dol        -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010   (expected)
./tools/probe_sources.sh             -> 753 files, 0 failed, 0 errors; LINKED (250 undefined, 0 duplicates)
python3 tools/check_symbol_names.py  -> checked 515 units; 0 missing
./tools/decomp_build.sh              -> All: 32.96% fuzzy, 25.82% matched, 12.18% linked (11509/28465)
main/MetroidPrime/CPauseScreen       -> 8.91% fuzzy, 2.35% matched code (9 / 84 functions)
all 86 RELs                          -> cmp-identical to orig/G2ME01/files/RelProd/  (via goal_check's gate.sh)
./tools/goal_check.sh build/goal/item.json -> goal_check: PASS progress-prime1-cpausescreen
```

`All:` did not fall (11506 -> 11509 matched). The diff adds no `asm`, adds and removes no
function symbol anywhere, and removes no initialisation.

`docs/HANDOFF.md` shows as modified: that is `goal_check`'s own derived-count rewrite, which the
item's instructions say to expect.

## Notes for the next run

- `.tmp/opencode/od2.py <symbol> [unit] [--all]` - the objdiff per-function table. **Left is
  retail, right is ours.** `--all` prints every instruction, not just the differing ones.
- `.tmp/opencode/off.cpp` + the toolchain's own `mwcceppc.exe` (needs `$MP_TOOLCHAIN_DIR/build/tools/wibo`)
  dumps member offsets into `.data`, where `objdump -s -j .data` reads them. Needs
  `#define private public` around the header include.
- The cheapest remaining target is `EnsureTextureLoaded` at 92.26%; the residue is scheduling, so
  only try it if you have already read the 56-instruction table and have a specific hypothesis.
- Do **not** try to flip this unit. The 41-symbol asymmetry above means `.text` cannot match.
- Do **not** re-litigate Prime 1's `CPauseScreen.cpp`; it is a different class (see above).
- Read `restoreToARAM` (`bool`, at `rstl::pair<bool, TToken<CTexture>>` + 0x8, which retail
  `lbz r3,8(r30)`s) before writing `RestoreTextures` - it is the flag `EnsureTextureLoaded`'s
  `TrackTexture` call passes, and it is already named.
