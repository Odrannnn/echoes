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

---

## Retry on L3 — 2026-10-02 (goal lane 3)

`./tools/goal_check.sh build/goal/item.json` → **`goal_check: PASS progress-prime1-cscriptpickup`**.
The unit's `matched_functions` went **12 -> 13 of 17**; the tree's went **13136 -> 13137 of 28465**
(`linked 6228` unmoved), `All: 37.14% fuzzy, 30.57% matched, 13.50% linked`. The unit's own fuzzy
went 51.45% -> **61.98%**, matched code 14.46% -> **21.36%**. `gate.sh` passed whole
(DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe, port link gap); no asm added;
`python3 tools/check_decl_order.py --unit main/MetroidPrime/ScriptObjects/CScriptPickup` ok;
`python3 tools/check_symbol_names.py`: 585 units, 0 declared names missing.

Note on the tree: the tree had moved a long way since the two earlier runs (10098 -> 13136 matched).
Re-measured first. `docs/HANDOFF.md`'s state block in the working tree is the judge's own
`MP_GATE_DOCS_WRITE=1` rewrite, not an edit of mine.

### Re-measured baseline on this tree, before anything

Unit was **12/17**: only `__ct__13CScriptPickup...` 64.12% (1060 B), `Think` 1.49% (3320 B),
`Touch` 69.85% (1292 B), `ShowAllKeysCollectedAlert` **0.00% (680 B, not implemented at all)**
and `LoadPickup` 97.05% (2072 B) were unmatched. So the earlier notes' "ShowAllKeysCollectedAlert
96.76%" did **not** survive the ninth upstream sync: the header kept the declaration, the `.cpp`
lost the body, and `src/MetroidPrime/PortReachStubs.cpp` still carried `reachstub_343` for it.

### Per function (from `build/report.json`)

| function | before | after | what produced it |
|---|---:|---:|---|
| `ShowAllKeysCollectedAlert` | 0.00% | **100%** (new match, 680 B) | new body, correct 4-group case set |
| `Touch` | 69.85% | **97.53%** (1292 B, same size as retail) | explosion block + 4 spelling fixes |
| `__ct__13CScriptPickup` | 64.12% | 64.12% | untouched |
| `Think` | 1.49% | 1.49% | untouched |
| `LoadPickup` | 97.05% | 97.05% | untouched |
| other 12 | 100% | 100% | untouched |

### `ShowAllKeysCollectedAlert` — 100%, and the earlier case set was simply wrong

**The earlier notes' dispatch analysis (six cases `{29,32,35,38,41,101}`, "MWCC emits an extra
`beq` at every pivot", "I did not find a spelling that produces retail's `bge`-only lowering") was
a misreading of the binary search.** Retail 0x800B41FC is 680 bytes and its case set is **18 values
in four groups**, which is why 6 cases gave 684:

    cmpwi r6,38 ; bge A        A: cmpwi r6,101 ; bge D      cmpwi r6,29 ; bge TEMPLE
    cmpwi r6,32 ; bge B        D: cmpwi r6,107 ; bge END    b END
    B: cmpwi r6,35 ; bge SWAMP  <temple body>
       b SAND                 A also: cmpwi r6,41 ; bge END ; b CLIFFS

    TEMPLE (0x800b4264) checks 29,30,31,101,102,103,104,105,106 -> "STRG_AllTempleKeysFound"
    SAND   (0x800b434c) checks 32,33,34                       -> "STRG_AllSandKeysFound"
    SWAMP  (0x800b43a4) checks 35,36,37                       -> "STRG_AllSwampKeysFound"
    CLIFFS (0x800b43fc) checks 38,39,40                       -> "STRG_AllCliffsKeysFound"

So the case labels are `kIT_TempleKey1..3` + `kIT_TempleKey4..9` (0x1D-0x1F and 0x65-0x6A),
`kIT_AgonKey1..3` (0x20-0x22), `kIT_TorvusKey1..3` (0x23-0x25), `kIT_HiveKey1..3` (0x26-0x28).
MW emits the temple body **once**, reached both from `cmpwi 29; bge` and as the fall-through of
`cmpwi 107; bge END`; there is no `beq` anywhere because the pivot's own body is the fall-through
of its right subtree. Each group's body re-checks *its own* keys with
`GetItemAmount(k, true)` and `break`s out of the switch on the first `<= 0`. The tail is
`if (name) { id = gpResourceFactory->GetResourceIdByName(name)->id;
mgr.QueueMessage(mgr.GetHUDMessageFrameCount() + 1, id, 0.f); }` with `const char* name = nullptr`
hoisted into r31 by the prologue (`li r31,0` before the switch). The four literals are at
0x803A7E27/3F/55/6C, immediately after `"??"(??"` at 0x803A7E20, which is the `rs_new` file string
`Touch` already passes to `operator new`.

Placement matters and is the only other thing this needed: the definition goes **after**
`GetPosition_800B44A4` and **before** `SLdrPickup.inc`, because mwcceppc emits in reverse source
order and retail's order is 0x800b44a4 > 0x800b41fc > 0x800b40b4.

### `Touch` — 69.85% -> 97.53%, and the earlier "blocked on a class that does not exist" is gone

`CExplosion.hpp`/`.cpp` now exist (`git log`: `9099bc04 progress: progress-prime1-cexplosion`), with
exactly the constructor retail calls:
`CExplosion(const TLockedToken<CGenDescription>&, TUniqueId, const CEntityInfo&, const rstl::string&,
const CTransform4f&, uint, const CVector3f&, const CColor&, int)`. Retail's 320 bytes at
0x800B47F8-0x800B493C reproduce with one statement:

    mgr.AddObject(rs_new CExplosion(TLockedToken<CGenDescription>(*mPickupParticleDesc),
        mgr.AllocateUniqueId(),
        CEntityInfo(GetCurrentAreaId(), CEntity::NullConnectionList, true, kInvalidEditorId),
        rstl::string_l("Explosion - Pickup Effect"), GetTransform(), 0,
        CVector3f(1.f, 1.f, 1.f), CColor::White(), -1));

`AddObject` is called **unconditionally** on the `new`'s result (retail jumps straight to it with
r25 when `operator new` returned 0), so there is no `if (explosion)`. `CEntity`'s area id is at +4
(`GetCurrentAreaId()` = `mAreaId`, the vptr is +0); `TAreaId` and `TEditorId` are class types and
so are passed by reference to a caller temporary - that is why retail emits `addi r4,r1,48` and
`addi r7,r1,52` rather than loading the values.

Three further fixes, each measured:

1. **A real behaviour difference, not a spelling.** Retail 0x800b4980 does
   `AddPowerUp(itemType, [this+0x160])` and 0x800b4990 does `IncrPickUp(itemType, [this+0x15C])` -
   two *different* members. Ours passed `mAmount` to both. With `mItemType` at 0x158, `mAmount` is
   0x15C and `mCapacity` 0x160, so the source must be `AddPowerUp(itemType, mCapacity)` followed by
   `IncrPickUp(itemType, mAmount)`; the `else` arm already used `mCapacity`/`mAmount` and already
   matched retail. 69.85% -> 94.75% with this and the explosion block together.
2. `if (!GetActive() || !IsVisible()) return;` compiles to `bne`+`b` (972+4 bytes of branch); two
   separate `if (...) return;` give retail's `beq`/`beq`. 94.75% -> 97.46%.
3. `player->GetUniqueId()` twice makes MW reload `lhz 8(player)`; retail reuses the slot it stored
   at function entry, so hoist `const TUniqueId playerUid = player->GetUniqueId();`. Same run.
4. `if (previousAmount < playerState->GetItemAmount(itemType))` gives `cmpw previous,new; bge skip`;
   retail has `cmpw new,previous; ble skip`. Semantically identical, but `>` in the source puts the
   operands the way MW wants: `if (playerState->GetItemAmount(itemType) > previousAmount)`.
   97.46% -> **97.51%**.

**What is left in `Touch` is one instruction and MW's register numbering.** The two streams are
otherwise *identical instruction for instruction* (verified with a mnemonic-stream diff over
`difflib`: one insert region and one trailing out-of-line function). Retail

    lhz r0,90(r1) ; lbz r4,88(r1) ; clrrwi r0,r0,2 ; lbz r5,89(r1) ; ori r3,r0,1 ;
    lwz r6,92(r1) ; lwz r0,96(r1) ; lfs f1,-29280(r2) ; stw r0,252(r26) ; ...

ours

    lhz r0,90(r1) ; lbz r3,89(r1) ; lbz r4,88(r1) ; clrrwi r0,r0,2 ; lwz r5,92(r1) ;
    ori r0,r0,1 ; lfs f1,0(0) ; stw r28,252(r25) ; ...

That is the `CModelFlags` temp of `SetModelFlags(CModelFlags::AlphaBlended(0.f)
.DepthCompareUpdate(true, false))`. `CModelFlags::x0_` is uninitialised in every constructor, and
retail copies it to `mDrawFlags.x0_` (0xFC) out of a **stack slot at 96(r1)** it never writes, while
MW in this tree keeps it in a **register** (r28) it never writes. That one extra callee-saved
register is what shifts the whole block down by one: ours saves `stmw r22,168(r1)` (10 registers,
r22-r31) where retail saves `stmw r23,188(r1)` (9, r23-r31), and every value in the function is
then off by exactly one register (r22<->r23, r23<->r24, ..., r26<->r27), with r28..r31 shared.
Our object is 1288 bytes against retail's 1292. Frame sizes: 208 against 224.

Spellings tried **this run** for that one instruction, all 1292 B or 1288 B, none moved it:

| spelling | score |
|---|---|
| `SetModelFlags(CModelFlags::AlphaBlended(0.f).DepthCompareUpdate(true, false));` | 97.51% |
| `CModelFlags flags = ...; SetModelFlags(flags);` | 97.51% (byte-identical output) |
| `SetModelFlags(CModelFlags(CModelFlags::kT_Blend, 0.f).DepthCompareUpdate(true, false));` | 97.51% |
| `SetModelFlags(CModelFlags::AlphaBlended(CColor(1.f,1.f,1.f,0.f)).DepthCompareUpdate(true,false));` | 92.04% (worse) |

Do **not** re-try those four. `CModelFlags.hpp` is shared by Matching units, so the fix is not a
header edit here. The way in is more stack pressure in `Touch` forcing MW to spill `x0_`; the
`CModelFlags` temp's other homes (88, 89, 90, 92) are byte-identical to retail's, so the allocator
agrees about everything except the dead word.

### `LoadPickup` — 97.05%, and the earlier `SLdrPickup` layout hypothesis stays dead

The mnemonic streams align 1:1 over 518 instructions with **eight** real differences (I counted
them; `difflib` over the two streams). They are not scheduling-only:

- an extra `stb r0,600(r1)` + `beq` + `addic.` pair of ours after `0x340`,
- a missing `stw r0,68(r1)`,
- `mr r5,r3` where retail has `addi r3,r1,136`,
- ours loads two floats from `904(r1)`/`908(r1)` and stores three; retail loads three words from
  `908`/`912`/`r0` and stores three words at `124`/`128`/`132`.

That last one is a genuine type difference (a vector built from floats vs from words), not
scheduling. `SLdrPickup`'s field order in `include/MetroidPrime/ScriptLoader/SLdrPickup.hpp` still
matches retail's `__ct__10SLdrPickupFv` (the L9 retry measured that with a probe), so
`NEW: progress-cscriptpickup-sldr-layout` from the first run is **superseded - do not act on it**.

### Prime 1's source, per the seeded functions

`Think` and `Touch` are the two functions item.json names. `prime-ref`'s `Touch` has a different
signature (`Touch(CActor&)` vs this tree's `Touch(CActor&, CStateManager&)`) and calls
`InitializePowerUp`/`IncrPickUp`/`DeleteObjectRequest`/`SendScriptMsgs(kSS_Arrived, ...)` with no
respawn, no explosion and no visor switch, so **none** of the four spellings above came from it.
`Think` is untouched at 1.49% (the whole body is still commented out in
`src/MetroidPrime/ScriptObjects/CScriptPickup.cpp:119-197`).

### Two things outside the unit the gate forced, both measured

- `src/MetroidPrime/PortReachStubs.cpp` — `reachstub_343` deleted, because
  `ShowAllKeysCollectedAlert` now has a real body and that file *is* linked by the boot probe
  (`-DMP_BOOT_STUBS=ON`), so the two definitions would collide there. Same hand-retirement the file's
  own header documents for `AllocateRenderer`. The number is left as a gap, as `stub_225`/`stub_232`
  were.
- `docs/research/port_link_gap_list.md` + `docs/research/port_link_gap.md` — **one entry removed and
  one added, net 279 -> 279.** Removed
  `_ZN13CScriptPickup25ShowAllKeysCollectedAlertER13CStateManagerP12CPlayerStateNS2_9EItemTypeE`;
  added `_ZN10CExplosionC1ERK12TLockedTokenI15CGenDescriptionE9TUniqueIdRK11CEntityInfoRKN4rstl12basic_stringIcNS9_11char_traitsIcEENS9_17rmemory_allocatorEEERK12CTransform4fjRK9CVector3fRK6CColori`,
  because `Touch` now calls it and **the port build is driven by `files.cmake`, which does not list
  `src/MetroidPrime/CExplosion.cpp`** (the decomp build does: `configure.py:542`
  `Object(NonMatching, "MetroidPrime/CExplosion.cpp")`, and `build/G2ME01/src/MetroidPrime/
  CExplosion.o` exists). `CFluidPlaneManager.cpp`, the other `CExplosion` caller, is outside
  `files.cmake` for the same reason. The new gap entry closes when `CExplosion.cpp` joins
  `files.cmake`. `port undefined 287` is unmoved.

## NEW

(none filed - what is left in this unit - `Touch`'s last instruction, `LoadPickup`'s eight
differences, `Think`'s 3320-byte body and the 1060-byte constructor - is the same target as this
item, and a restatement of the item is not a `NEW:`.)

No `WALL:`: this run landed a match. The `Touch` remainder is characterised above with the four
spellings that do not reach it.
