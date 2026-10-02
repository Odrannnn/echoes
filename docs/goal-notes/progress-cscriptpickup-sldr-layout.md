# progress-cscriptpickup-sldr-layout

Target: `main/MetroidPrime/ScriptObjects/CScriptPickup` (kind `progress`).

**Result: the unit did not gain a matched function. It is still 8 / 17.** Two functions got
measurably better and nothing got worse; the diff is kept because every part of it is correct
and measured. Details and the corrected premise are below, so the next run does not repeat this.

## The item's premise is wrong - SLdrPickup's member order is already retail's

The reason says the member order is "wrong by 4 bytes from the first property" and that this is
the whole remaining diff of `LoadPickup`. Re-measured, that is not the case.

`SLdrPickup` in `include/MetroidPrime/ScriptLoader/SLdrPickup.hpp` reproduces retail's layout
**byte for byte**. Proof: the 26 property stores in retail's `LoadPickup` case bodies land on
exactly the offsets our object uses (the diff is only a constant 4 in the *stack frame base*,
608 vs 604 - see below). The full table, read out of `tools/dis.sh 0x800B389C 0x818` (the
binary-search switch, each `beq` body naming the member it writes) and cross-checked against
`__ct__10SLdrPickupFv` at 0x800B40B4:

| property tag | member | offset |
|---|---|---|
| 0x255a4580 | editorProperties | 0x00 (60 B) |
| 0x3a3e03ba | collisionSize | 0x3C |
| 0x2e686c2a | collisionOffset | 0x48 |
| 0xa02ff0c4 | itemToGive (SLdrPlayerItem) | 0x54 |
| 0x28c71b54 | capacityIncrease | 0x58 |
| 0x165ab069 | itemPercentageIncrease | 0x5C |
| 0x94af1445 | amount | 0x60 |
| 0xf7fbaaa5 | respawnTime | 0x64 |
| 0xc80fc827 | pickupEffectLifetime | 0x68 |
| 0x32dc67f6 | lifetime | 0x6C |
| 0x56e3ceef | fadetime | 0x70 |
| 0xc27ffa8f | model (CAssetId) | 0x74 |
| 0xe260b08c | animationInformation | 0x78 (12 B) |
| 0x7e397fed | actorInformation | 0x84 (120 B) |
| 0x192b0e70 | echoInformation | 0xFC (20 B) |
| 0xe585f166 | activationDelay | 0x110 |
| 0xa9fe872a | pickupEffect (CAssetId) | 0x114 |
| 0xe10bcb96 | absoluteValue | 0x118 |
| 0xce33239f | calculateVisibility | 0x119 |
| 0x2de4a294 | canHomeByDefault | 0x11A |
| 0xa6ea280d | autoHomeRange | 0x11C |
| 0xc2b11cfd | delayUntilHome | 0x120 |
| 0x2db59fcf | homingSpeed | 0x124 |
| 0x961c0d17 | autoSpin | 0x128 |
| 0xa755eb02 | blinkOut | 0x129 |
| 0x850115e4 | orbitOffset | 0x12C (12 B) |

`sizeof` 0x138 = 312. `SLdrEchoParameters` is 20 B, not 24: `fn_8023B55C` (its ctor) writes
bytes 0-1, word 4 and floats 8/12/16, and `__dt__18SLdrEchoParametersFv` follows.
`SLdrPlayerItem` really is one `int` (`LoadTypedefSLdrPlayerItem` at 0x8023E0C4 is 0x18 bytes:
read int, store, return - no property loop), so the 28 bytes at 0x58-0x73 really are separate
`SLdrPickup` members and the tree is right to declare them that way.

**The 4 bytes are `LoadPickup`'s stack frame, not the struct.** Retail puts the `SLdrPickup`
local at `r1+608`, ours at `r1+604`, and everything from there up is shifted by 4. Retail's local
block is `[uid 56][uid 60][TAreaId 64][? 68][CVector3f 72,84,96][SEchoParameters 108]
[CVector3f 124][CAABox 136,160,184][CTransform4f 208,256,304][CActorParameters 352]
[optional 444/448][optional 524/528][bool 604]`; ours is the same objects in a different order
with one 4-byte local fewer (`SEchoParameters` lands at 68, before the three switch temporaries
instead of after). So retail has exactly one more 4-byte local than we do in the 0x40-0x88
range, and retail stores `info.GetAreaId()` twice (64 and 68) where we store it once and pass
`&64`. That is MWCC's local-slot allocation, not a header. Do not spend another item on the
header.

## What I changed, and what it measured

`include/MetroidPrime/ScriptLoader/SLdrPickup.hpp`, `SLdrAnimationParameters.hpp`,
`CScriptPickup.hpp`, `src/MetroidPrime/ScriptLoader/SLdrStructMembers.cpp`,
`src/MetroidPrime/ScriptObjects/CScriptPickup.cpp`:

1. **Dropped `~SLdrPickup()`.** Retail has no `__dt__10SLdrPickupFv`; `LoadPickup` destroys the
   members inline, in reverse declaration order (echo, actor, animation, player item, editor
   properties). The implicit destructor does exactly that. Our declared dtor was an out-of-line
   call to the stub in `SLdrStructMembers.cpp`.
2. **Declared `~SLdrAnimationParameters()`.** Retail has `__dt__23SLdrAnimationParametersFv`
   (0x802422A8, the usual self-null boilerplate). Ours was trivial and elided, so both cleanup
   paths in `LoadPickup` were 3 instructions short.
3. **`LoadEditorTransform(...).GetRotation()`** instead of `CTransform4f(LoadEditorTransform(...))`
   in the `collisionSize == Zero` branch. Retail calls `GetRotation__12CTransform4fCFv` there.
4. **Added `GetPosition_800B44A4`**, retail's 116-byte free function at 0x800B44A4, which the
   object did not define at all. It is `pickup.GetTranslation() + pickup.GetTransform()
   .Rotate(pickup.GetOrbitOffset())` - note the position comes from `CActor::mPosition`
   (0x54), *not* from `GetTransform().GetTranslation()`, which reads m03/m13/m23 at 0x30/0x40/0x50.
   It needs `extern "C"`: as a `static` free function MWCC emits
   `GetPosition_800B44A4__FRC13CScriptPickup` and as a `static` member
   `GetPosition_800B44A4__13CScriptPickupFRC13CScriptPickup`, so objdiff cannot pair it with
   retail's unmangled name. Nothing in the DOL calls it.

Measured, objdiff on the unit:

| function | before | after |
|---|---|---|
| `LoadPickup` (2072 B) | 91.36% | **94.50%** |
| `GetPosition_800B44A4` (116 B) | absent, 0% | **98.83%** |
| unit fuzzy | 42.41% | 44.24% |
| unit matched_functions | 8 / 17 | 8 / 17 (98.83% is not 100%) |

`GetPosition_800B44A4` is 4 bytes short and that is all. Everything else matches, including the
prologue, `Rotate__12CTransform4fCFRC9CVector3f`, the register assignment of the sums and the
`fadds` operands.

## Walls

**`WALL: GetPosition_800B44A4 98.83%` - 4 bytes of MWCC scheduling.** The two remaining
differences are a swapped `lfs` pair (retail loads `pos.z` into f2 then `rot.z` into f1; we load
`rot.z` into f1 then `pos.z` into f2 - the `fadds f0, f2, f1` is identical, only the order of two
independent loads differs) and a swapped epilogue pair (retail `lwz r31` before `lwz r0`, we the
other way). Spellings tried, all measured:

| spelling | score |
|---|---|
| `GetTranslation() + Rotate(...)`, one line | 98.83% |
| `orbit` named local, then `GetTranslation() + orbit` | 98.83% |
| `pos` as a const ref local declared first, `pos + orbit` | 98.83% |
| `pos` as a const ref local, `Rotate` inline in the return | 98.83% |
| `Rotate(...) + GetTranslation()` (operands swapped) | 98.34% |
| `CVector3f(pos.GetX()+orbit.GetX(), ...)` spelled out | 97.62% |
| `CVector3f pos = ...; pos += Rotate(...); return pos;` | 35.44% |
| `CVector3f out = ...; out = Rotate(...) + out; return out;` | 35.44% |

**`LoadPickup` is 5.5% short and cannot be finished from this unit.** Three causes, all measured
from the side-by-side diff:

- `rstl::optional_object<T>`'s copy constructor is header-inline
  (`include/rstl/optional_object.hpp:20`), so MWCC inlines it and emits `addic.`/`beq` +
  `__ct__10CModelDataFRC10CModelData` plus a redundant `stb`/`lbz` of the valid flag into a
  separate bool local. Retail calls
  `__ct__Q24rstl29optional_object<10CModelData>FRCQ24rstl29optional_object<10CModelData>` out of
  line, and we never emit that symbol at all. Fixing it means moving the copy ctor out of line
  for every `optional_object` in the tree - a global change, not this item's.
- The local-slot order and the extra 4-byte local described above.
- One `bl` names `LoadModelData__FRC9CVector3fUiRC23SLdrAnimationParametersb` where retail names
  `fn_8023A0B8`; same signature, same arguments, different symbol.

**`__ct__10SLdrPickupFv` (328 B, 0%) is not reachable in one item.** The good news is the nested
layout it needs is already right: `SLdrLightParameters` is 64 B with `ambientColor` (CColor) at
+20, which is +152 inside `SLdrActorParameters` at +132 exactly as the ctor stores it, and
`SLdrActorParameters` is 120 B (its `visor` runs 96-103, so the `int` the ctor writes with value
15 at +232 is `visor.visorFlags` at +100). What is missing is that retail's ctor *calls* four
nested constructors that we name differently - `fn_8023E118` (SLdrPlayerItem), `fn_802422E4`
(SLdrAnimationParameters), `fn_8023F534` (SLdrActorParameters), `fn_8023B55C`
(SLdrEchoParameters) - and those four live at 0x8023xxxx, which **no entry in
`config/G2ME01/splits.txt` claims**, so they are in an unclaimed gap and are scored nowhere. So
matching the 328-byte ctor needs, in order: rename those four to retail's names, then move
`SLdrPickup::SLdrPickup()` out of `SLdrStructMembers.cpp` into `CScriptPickup.cpp` (splits.txt
puts 0x800B40B4 in this unit) and write its member-initializer list with the exact SDA float
constants. That is a chain of at least two items, not one.

## Gates

All run from this worktree with `MP_TOOLCHAIN_DIR` set:

- `sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`
- `./tools/probe_sources.sh` -> `749 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)`
- `python3 tools/check_symbol_names.py` -> `checked 503 units; 0 declared names are missing`
- `./tools/decomp_build.sh` -> `All: 30.97% fuzzy, 23.23% matched, 11.76% linked
  (726 / 2041 files)`, `10041 / 28465 functions` - unchanged from the baseline
- all 86 RELs `cmp`-equal to `orig/G2ME01/files/RelProd/`
- `python3 tools/check_docs_claims.py` -> `docs claims agree with the tree`
- `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CScriptPickup` -> `ok`
- per-function diff of `build/report.json` against a baseline report built from the reverted
  tree: **0 functions worse, 0 lost**, 2 better (`GetPosition_800B44A4` 0 -> 98.83,
  `LoadPickup` 91.36 -> 94.50)

No `asm`, no `configure.py` or `splits.txt` change, no commit.

`GetTouchBounds` (196 B, 43.16%) has the same shape of problem and is untouched by this item:
the same eight FP loads and three sums in a different order, and our compiler hoists them
*above* the `stw r0`/`stw r31`/`mr r31, r3` prologue while retail keeps them after. Same
instruction count, same offsets, same registers - only the schedule differs. Worth knowing
before anyone tries it.

One thing AGENTS.md has stale: it quotes `probe_sources.sh` as "329 files", it now prints 749.

---

# Run 2 (lane 9, 2026-10-02): CScriptPickup 9 -> 11 / 17, goal_check PASS

Tree was clean (run 1's diff was not kept). Re-measured: `GetTouchBounds` 97.98%, `GetPosition_800B44A4` absent.
Both are now **100%**. Diff: `CScriptPickup.cpp` (+7/-1), `CScriptPickup.hpp` (+1).

- **`GetTouchBounds`**: `CVector3f off = GetTranslation();` (a **by-value copy**, not `const CVector3f&`)
  then `CAABox(mTouchBounds.GetMinPoint() + off, mTouchBounds.GetMaxPoint() + off)` -> 100.0%.
  The `const&` form, `GetTranslation()` twice, named min/max locals (both orders), float-component
  locals (all 72 order permutations, <=77%), `off + bounds` (97.57%) all stayed at 97.6-98.0%
  (only the FP load schedule differed). The by-value copy changes the scheduling; that is the whole fix.
- **`GetPosition_800B44A4`**: `extern "C" CVector3f GetPosition_800B44A4(const CScriptPickup&)` returning
  `pickup.GetTranslation() + pickup.GetTransform().Rotate(pickup.GetOrbitOffset())` -> **100.0%**
  (run 1's 98.83% wall is superseded; same spelling there probably differed in accessors). Needs
  `GetOrbitOffset()` added to the header (`mOrbitOffset`, +0x1c4). Placed after `fn_800B4518`, so
  decl order stays descending (`check_decl_order.py` ok). Variants `b`,`c`,`g` (named rot, named xf)
  also 100%; component-wise spellings 98.6%; `rot + pos` 98.76%; copying `pos` first 22.9% (hoists loads, spills f29-f31).
- Did not redo run 1's SLdrPickup dtor / `SLdrAnimationParameters` dtor / `LoadPickup` changes (LoadPickup is 93.78%
  on this tree; still blocked by optional_object copy ctor, local slot order, `fn_8023A0B8` name, as in run 1).

Gates: `./tools/goal_check.sh build/goal/item.json` -> PASS (gate.sh incl. DOL sha1 and 86 RELs, 9 -> 11 / 17,
12699 -> 12701 total, check_symbol_names ok, no asm). No commit.

Remaining in unit: ctor 64.12%, Think 1.49%, Touch 69.85%, LoadPickup 93.78%, ShowAllKeysCollectedAlert and
`__ct__10SLdrPickupFv` still absent (the latter needs the four nested SLdr ctor renames, see run 1).
