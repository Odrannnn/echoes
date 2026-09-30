# progress-prime1-cscriptpickup — `MetroidPrime/ScriptObjects/CScriptPickup`

Unit stays `NonMatching`. `matched_functions` for the unit went **8 -> 9**; the tree total went
**9997 -> 9998** of 28465 (`build/report.json`, `All: 30.79% fuzzy, 23.04% matched`, up from
30.78%). The unit itself went 42.41% -> 52.46% fuzzy.

## Gate (all measured after the change)

    sha1sum build/G2ME01/main.dol   6ef9b491d0cc08bc81a124fdedb8bfaec34d0010   (unchanged)
    ./tools/probe_sources.sh        749 files, 0 failed, 0 errors; link: LINKED (249 undefined, 0 duplicates)
    python3 tools/check_symbol_names.py   checked 503 units; 0 declared names are missing
    ./tools/decomp_build.sh         All: 30.79% fuzzy, 23.04% matched, 11.74% linked (9998 / 28465)
    all 86 RELs cmp-equal to orig/G2ME01/files/RelProd/ (0 differing) and 0/86 sha1 mismatches
                                    against config/G2ME01/config.yml

Only `build/G2ME01/src/MetroidPrime/ScriptObjects/CScriptPickup.o` was rebuilt (ninja `[1/1]`), and
the three other TUs that include `CScriptPickup.hpp` (`TypesMatch.cpp`,
`CScriptPickupGenerator.cpp`, `PortGlobals.cpp`) emit nothing for the added inline accessor, so no
other unit moved. Modules stayed at 1411/11739.

## Diff

- `src/MetroidPrime/ScriptObjects/CScriptPickup.cpp` — `AcceptScriptMsg` body, new
  `ShowAllKeysCollectedAlert` body, new `extern "C" GetPosition_800B44A4`, one added include.
- `include/MetroidPrime/ScriptObjects/CScriptPickup.hpp` — one added public accessor
  `GetOrbitOffset()` (needed because retail's helper is a free function, not a member).

No `asm` added, no `configure.py` / `splits.txt` / `files.cmake` change, no init removed.

## Per function (from `build/report.json`)

| function | before | after |
|---|---|---|
| `AcceptScriptMsg` | 12.69% | **100%** (new match) |
| `GetPosition_800B44A4` | not emitted (0 B) | 98.83% (116 B, 8 bytes short) |
| `ShowAllKeysCollectedAlert` | not emitted (0 B) | 96.76% (680 B) |
| `Think` | 1.49% | 1.49% (untouched) |
| `Touch` | 69.85% | 69.85% (untouched) |
| `GetTouchBounds` | 43.16% | 43.16% (untouched) |
| ctor | 64.12% | 64.12% |
| `LoadPickup` | 91.36% | 91.36% |

## Prime 1's source did not help on any of the three named functions

Read `/run/media/.../prime-ref/src/MetroidPrime/ScriptObjects/CScriptPickup.cpp` (212 lines) in full.
It is the GC/1.3.2 ancestor and shares almost nothing with Echoes' version:

- `Think`: Prime 1 is a `CPhysicsActor` with `mDelayTimer`/`mPossibility` and no echo parameters,
  no respawn, no orbit, no `mAutoHomeRange`/`mHomingSpeed`. Echoes' `Think` is 3320 bytes with two
  per-player loops, FOV math and an orbit/spin transform. Nothing was reusable.
- `Touch`: Prime 1 takes no `CStateManager&` player index, calls `InitializePowerUp` /
  `IncrPickUp` / `DeleteObjectRequest` and `SendScriptMsgs(kSS_Arrived, mgr, kSM_None)`. Echoes'
  `Touch` is 1292 bytes: `MaskUIdNumPlayers`, `AddPowerUp`/`ReInitializePowerUp`, a respawn branch,
  `ShowAllKeysCollectedAlert`, the "all pickups found" message and the visor switch.
- `GetTouchBounds`: Prime 1 is one line, `return CPhysicsActor::GetBoundingBox();`. Echoes' is
  196 bytes building a `CAABox` from `mTouchBounds` offset by `mPosition`.
- `AcceptScriptMsg`: Prime 1's signature is `AcceptScriptMsg(EScriptObjectMessage, TUniqueId,
  CStateManager&)`; Echoes' is `(CStateManager&, const CScriptMsg&)`. No overlap.

So all four bodies above were derived from `build/G2ME01/main.elf` with `tools/dis.sh`, not from
Prime 1. Prime 1's `ShowAllKeysCollectedAlert` does not exist.

## What `AcceptScriptMsg` actually does (measured, now 100%)

```cpp
switch (msg.GetMessage()) {
case kSM_Activate: mTransformZ = GetTranslation().GetZ(); break;      // 0x5c -> 0x194
case kSM_XCRT:     if (mgr.fn_80036200().GetUnk14_24()) mUnk3 = true; break;  // bit 0 of 0x1d0
case kSM_XDelete:  if (!mEnableTractorTest)
                     SendScriptMsgs(kSS_Dead, mgr, kInvalidUniqueId, kSM_None);
                   sUnkPickupId = kInvalidUniqueId; break;
default: break;
}
CActor::AcceptScriptMsg(mgr, msg);
```

Two things had to be right and both are ordering/semantics, not formatting:

1. **The case bodies must be declared in retail's layout order** (Activate, XCRT, XDelete). MWCC
   emits the bodies in *source* order inside a function, and the object at 12.69% had them in the
   order I first wrote them. Reordering took it to 67.66%; the remaining bytes were the
   `mUnknownProp`/`mUnk3` mix-up below, after which it is byte-identical.
2. The `kSM_XCRT` case sets **`mUnk3`**, not `mUnknownProp`. See the bitfield lesson.

`fn_80036200()` is `CScriptObjectLoaderHelper&` at `mgr + 0x3EC0`; the bit read is that class's
`x14_24` at +0x14, reached through the existing `GetUnk14_24()` accessor, so the call stays a real
`bl` as in retail.

## Codegen lesson worth keeping: MWCC `: 1` bitfields are MSB-first

`CScriptPickup`'s trailing `bool ... : 1` run is one byte at 0x1D0. I calibrated it by compiling a
throwaway probe function that stored and then tested each named field. The mapping is **reverse of
the declaration order**:

    bit 7 mUnknownProp   bit 6 mGenerated   bit 5 mInTractor   bit 4 mAbsoluteValue
    bit 3 mEnableTractorTest   bit 2 mAutoSpin   bit 1 mUnk2   bit 0 mUnk3
    byte 1 bit 7 mBlinkOut

and the printed objdump operand is not the same number in the two forms:

- store, `rlwimi r0,r3,<B>,<31-B>,<31-B>` — the first printed operand **is** the bit index.
- test, `rlwinm. r0,r0,<32-B>,31,31`, except bit 7 which is `clrlwi. r0,r0,31`.

Reading the printed number as the bit index in a *test* is off by `32 - B`, which is how I first
wrote `mUnknownProp` where retail writes `mUnk3`. `fn_800B4518` (already 100%) is the calibration
anchor: `mUnknownProp` there is the `rlwimi ...,7,24,24`.

## `GetPosition_800B44A4` — 98.83%, 8 bytes of scheduling

Retail `0x800b44A4`, 116 bytes, **no callers anywhere in the DOL** (`objdump -d main.elf |
grep 800b44a4` finds only the definition), so it is dead code retail's compiler still emitted.
MWCC emitted ours even as an unused function, but a `static` one gets the parameter type appended
to the symbol (`GetPosition_800B44A4__FPC13CScriptPickup`); `extern "C"` is what produces retail's
unmangled `GetPosition_800B44A4`.

Body is `mPosition + mTransform.Rotate(mOrbitOffset)` (Rotate at 0x802C8F60, `mTransform` at +0x24,
`mOrbitOffset` at +0x1C4). Spellings tried, all 116 bytes, all 8 real bytes differing in one place
— retail loads `mPosition.z` (0x5C) *before* the rotated `.z` (0x10(r1)) and we do the reverse:

| spelling | score |
|---|---|
| `GetTranslation() + GetTransform().Rotate(GetOrbitOffset())` | 98.83% |
| `const CVector3f& pos = ...; CVector3f orbit = ...; return pos + orbit;` | 98.83% |
| `const CTransform4f& xf; const CVector3f& orbit; return GetTranslation() + xf.Rotate(orbit);` | 98.83% |
| `CVector3f orbit = ...; return orbit + GetTranslation();` | worse (operand order flipped, 21 B differ) |
| `CVector3f pos = ...; CVector3f rot = ...; return pos + rot;` | worse (156 B, CVector3f copy inlined) |

The header needed one added accessor, `GetOrbitOffset()`, because retail's helper is a free
function and `mOrbitOffset` is private.

## `ShowAllKeysCollectedAlert` — 96.76%, the switch dispatch is the only difference

Retail `0x800b41FC`, 680 bytes. Every `GetItemAmount` chain, all four string literals
(`STRG_AllTempleKeysFound` / `AllSandKeysFound` / `AllSwampKeysFound` / `AllCliffsKeysFound`, at
0x803A7E27/3F/55/6C), the virtual `GetResourceIdByName` and the
`QueueMessage(GetHUDMessageFrameCount() + 1, id, 0.f)` tail are byte-identical to ours.

What is left is the shape of the dispatch. Retail compares against **six** values — 38, 32, 29, 35,
101, 41 — using only `bge` + `b`, e.g.

    cmpwi r6,38 ; bge A ; cmpwi r6,32 ; bge B ; cmpwi r6,29 ; bge case29 ; b default
    A: cmpwi r6,101 ; bge default ; cmpwi r6,41 ; bge default ; b case38
    B: cmpwi r6,35 ; bge case35 ; b case32

That is a binary search over the case set `{29, 32, 35, 38, 41, 101}` where 41 and 101 fall into
`default`. Getting 41 and 101 into the set was necessary and was worth 90.76% -> 96.76%: the first
attempt guessed 67 (`kIT_PersistentCounter1`, actually 0x43) and the correct value is
`kIT_HealthRefill` (0x29 = 41) plus `kIT_TempleKey4` (0x65 = 101). Both forms were tried:

- `case kIT_HealthRefill: case kIT_TempleKey4: default: break;` — 96.76%
- `case kIT_HealthRefill: case kIT_TempleKey4: break;` (no `default:` label) — identical codegen

MWCC still emits `beq <case body>` for every pivot where retail emits only `bge`, and our function
is 684 bytes against retail's 680. Without the extra two labels MWCC picks a *jump table*
(`addi r0,r6,-29 ; cmplwi r0,9 ; slwi ; lwzx ; bctr`), 648 bytes. I did not find a spelling that
produces retail's `bge`-only lowering.

## `Touch` — 69.85%, blocked on a class that does not exist in this repo

The missing 320 bytes (ours 972, retail 1292) is exactly the commented-out `CExplosion` block at
0x800B47F8-0x800B493C. Retail does `operator new(384)`, copy-constructs the `CToken` from
`mPickupParticleDesc`, `mgr.AllocateUniqueId()`, builds
`CEntityInfo(GetCurrentAreaId(), CEntity::NullConnectionList, kInvalidEditorId)`,
`rstl::string_l("Explosion - Pickup Effect")` (0x803A7E84), `GetTransform()`, `0`,
`CVector3f(1,1,1)`, `CColor::White()`, then calls the ctor at 0x800532D4 and `mgr.AddObject`.

There is no `CExplosion.hpp` in this tree, so restoring it means declaring `fn_800532D4` with a
signature guessed from 11 register/stack arguments (`CColor` and an `int -1` are passed on the
stack at r1+8 and r1+12, and r1+0 is never written, so the real signature has an argument I could
not identify). I did not do it: it is a speculative declaration of an unnamed symbol, and Touch
would still not reach 100% on the strength of it alone.

## `GetTouchBounds` — 43.16%, pure scheduling

Already correct semantically: retail builds `CAABox(min + mPosition, max + mPosition)` where
`mTouchBounds` is at 0x1A4 and `mPosition` at 0x54. Our object is the right size (196 B) and the
same 49 instructions, but retail computes the whole max vector first (loading `mPosition.x`,
`box[432]`, `mPosition.y`, `box[436]`, ...) and lays the two temporaries out with arg1 at r1+20 and
arg2 at r1+8, while MWCC interleaves min and max per component and lays them out the other way
round. Three spellings tried, all 43.16% / 196 B:

- `const CVector3f& off = GetTranslation(); return CAABox(GetMinPoint()+off, GetMaxPoint()+off);` (the tree's original)
- `CVector3f hi = max+off; CVector3f lo = min+off; return CAABox(lo, hi);` — max computed first, as retail does; still interleaved
- `CVector3f off = GetTranslation();` (value copy instead of a reference) — same

## `LoadPickup` — 91.36%, and it is blocked on the wrong `SLdrPickup` layout

This is the largest single win left in the unit (2072 B) and it is not a scheduling problem: every
differing run is a **4-byte member offset** that is already off by the time the first property is
read, and stays off. Retail's `LoadPickup` writes `sldrPickup + 0x260`, `0x264`, `0x268` where ours
writes `0x25C`, `0x260`, `0x264`; the same -4 appears at every later property.

Retail's `__ct__10SLdrPickupFv` (0x800B40B4, 328 B) gives the real layout, and it is **not** the
declaration order in `include/MetroidPrime/ScriptLoader/SLdrPickup.hpp` (which is marked
"Generated by scripts/generate_script_loaders.py. Review before integration."):

    capacityIncrease 0x38   collisionSize 0x3C   collisionOffset 0x48
    amount 0x58  itemPercentageIncrease 0x5C  pickupEffectLifetime 0x60  respawnTime 0x64
    lifetime 0x68  fadetime 0x6C  activationDelay 0x70  <sub-ctor at 0x54> <word 0x74 = -1>
    <sub-ctor at 0x78> <sub-ctor at 0x84> <sub-ctor at 0xFC> <word 0x114 = -1>
    CColor at 0x98   <int 15 at 0xE8>   <float 0 at 0x110>  <floats 0 at 0x118,0x11C>
    <float at 0x124>  <bytes 0,1,0 at 0x118..0x11A>  <bytes 0,0 at 0x128,0x129>

versus the header's order (editorProperties, collisionSize, collisionOffset, itemToGive,
capacityIncrease, itemPercentageIncrease, amount, respawnTime, pickupEffectLifetime, lifetime,
fadetime, model, animationInformation, actorInformation, echoInformation, activationDelay, ...).
`SLdrPickup.hpp` is included by only two TUs, `src/MetroidPrime/ScriptObjects/CScriptPickup.cpp`
and `src/MetroidPrime/ScriptLoader/SLdrStructMembers.cpp`, so a reorder is contained to those two
units — but it is a real header change and I ran out of budget before measuring it.

`__ct__10SLdrPickupFv` itself (328 B, currently not emitted at all) would follow from the same
layout work.

## NEW

NEW: progress-cscriptpickup-sldr-layout | progress | MetroidPrime/ScriptObjects/CScriptPickup | SLdrPickup's member order is wrong by 4 bytes from the first property, which is the whole remaining diff of LoadPickup (2072 B, 91.36%) and blocks its 328-byte ctor; retail's order is in __ct__10SLdrPickupFv at 0x800B40B4 and the header only has two including TUs.

## Retry on L9 — 2026-09-30

Fresh report before edits: `CScriptPickup` was 8/17 functions, 42.41% fuzzy; tree total 10097/28465. I remeasured the three seeded Prime 1 names on this tree and did not re-run the already documented Prime 1 source experiment:

| function | before -> after | Prime 1 source result (from earlier measured experiment) |
|---|---:|---|
| `Think` | 1.49% -> 1.49% | Did not help: Prime 1's physics/timer implementation is not Echoes' orbit/FOV/homing body. |
| `Touch` | 69.85% -> 69.85% | Did not help: signatures and pickup/respawn/visor behavior differ; Echoes also has the explosion path. |
| `GetTouchBounds` | 43.16% -> 43.16% | Did not help: Prime 1 returns the base bounding box; Echoes builds translated touch bounds. |

Progress kept: implemented the measured `AcceptScriptMsg` switch (Activate, XCRT, XDelete, then base call), which moved it 12.69% -> 100.00%. A separate new spelling/ABI experiment made the `SLdrPickup` destructor declaration host-only (`TARGET_PC`): retail has no `__dt__10SLdrPickupFv`, while the host still defines its destructor. `LoadPickup` improved 91.36% -> 94.39% (still 2072 bytes, not exact). `fast_try.sh MetroidPrime/ScriptObjects/CScriptPickup` measured the final unit at 9/17, 45.25% fuzzy.

Correction to the earlier `NEW: ...sldr-layout` hypothesis: a temporary probe compiled with the MWCC target flags measured `sizeof(SLdrPickup)==0x138` and its current field offsets as editorProperties 0x00, collisionSize 0x3C, collisionOffset 0x48, itemToGive 0x54, capacityIncrease 0x58, itemPercentageIncrease 0x5C, amount 0x60, respawnTime 0x64, pickupEffectLifetime 0x68, lifetime 0x6C, fadetime 0x70, model 0x74, animationInformation 0x78, actorInformation 0x84, echoInformation 0xFC, activationDelay 0x110, pickupEffect 0x114, booleans 0x118..0x11A, autoHomeRange 0x11C, delayUntilHome 0x120, homingSpeed 0x124, autoSpin/blinkOut 0x128/0x129, orbitOffset 0x12C. These agree with retail's `__ct__10SLdrPickupFv` at 0x800B40B4 and `LoadPickup` member writes. The apparent -4 is the local object's stack base (retail r1+608, ours r1+604), not a member offset: do not reorder this struct on this evidence.

Verification: `python3 tools/check_decl_order.py --unit main/MetroidPrime/ScriptObjects/CScriptPickup` passed. `./tools/goal_check.sh build/goal/item.json` passed: matched 10097 -> 10098, linked 4918 unchanged, target 8 -> 9/17, no asm; gate, symbol names, and all report/DOL/REL/docs/probe checks passed. No `WALL:`: this run did not measure repeated alternative spellings to a scheduling wall.
