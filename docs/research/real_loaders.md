# The 86 real REL entity loaders: 1 is writable, 85 are blocked on a class that is not in this tree

Measured 2026-09-26 by lane `e5` at commit `755a0ca`, from `build/G2ME01/main.elf`,
`config/G2ME01/symbols.txt`, `config/G2ME01/splits.txt` and the 86-row table in
`rel_loaders.md`. Reproduce with `python3 tools/analyze_loaders.py` and
`python3 tools/classify_loaders.py` (both added by this lane; each is a few dozen lines and
prints the table below).

## The number that sizes the rest of the project

| | count | bytes |
| --- | --- | --- |
| **not blocked** - the entity class exists here, with a claimed vtable | **1** | 744 |
| **blocked** - the entity class has no unit in this tree | **85** | 76,756 |

85 of the 86 are blocked, and every one of them for the same single reason. That is much
worse than "hard", and it is not a decompilation problem: it says the port is missing
**75 entity classes** (the 85 blocked loaders construct 75 distinct ones), and until they exist
no amount of effort on the loaders themselves moves the number. The one unblocked loader is
`LoadAreaAttributes` (`REAA`, 0x8013C2E4, 744 bytes), whose class `CScriptAreaProperties` is the
only `CScript*` in `include/MetroidPrime/ScriptObjects/` whose vtable is a **named** symbol in
`symbols.txt`.

So the honest size of this work is: **1 function of decompilation, and a class-availability
problem of 75 classes.** The loaders are the *symptom*.

## Why "blocked" is mechanical and not a judgement

A DOL loader's bytes store the entity's **vtable pointer** at object+0 (inside the constructor
it tail-calls) and **call the constructor at a fixed address**. So a loader can only ever be a
`Matching` unit if the class it constructs has both its constructor *and* its vtable defined by
a unit in this tree. That is a property of `config/G2ME01/splits.txt`, not an opinion, and the
test is two address lookups:

1. find the `bl __nw__FUlPCcPCc` / `bl __nwa__FUlPCcPCc` in the loader, and the size in `r3`;
2. the constructor is the next call that stores a `.data` address at `0(obj)` - that address is
   the vtable;
3. look both addresses up in `splits.txt`.

**86 loaders, 85 `new` sites, 76 distinct vtables, 75 of them in a `.data` range no unit
claims.** A `.data` range no unit claims is filled from retail by `dtk`, which is exactly the
statement "this class is not in this tree". `tools/classify_loaders.py` prints
`UNBLOCKED 1 / 86`.

**Step 3 is superseded, and it is the claim that made this look like 75 classes of work.**
Lane `g2` wrote `LoadTimeKeyframe` on 2026-09-26 with **no ctor unit and no vtable claim**,
so the ctor's *range* is irrelevant: what the DOL link needs is the ctor's **name** in
`config/G2ME01/symbols.txt`, and then dtk's object for the ctor supplies the bytes. The same
is true of the vtable. The class is not the blocker; see
`docs/research/missing_classes.md` for the corrected test, the 76-row table, and the four
things that *are* on the critical path. `LoadTimeKeyframe` is at **99.75%** and the
remaining 4 bytes are the register that receives the `operator new` result.

Two things this method recovered that are worth keeping:

- **`__nw__`'s class-name string is `??(??)` in this retail build.** Every `new` site passes
  `"??(??)"`, not the class name, so the usual trick of reading the class out of the `operator
  new` arguments does not work here and a lane that tries it will conclude the class is
  unrecoverable. It is not; the vtable is the handle.
- **The class name is recoverable by hand where it matters.** Retail's symbol table has 190
  `__ct__` symbols and 29 `__vt__` symbols, and they are exactly the classes earlier lanes have
  already written. Two of the 86 name their class outright:
  `__ct__21CScriptAreaPropertiesF9TUniqueIdRC11CEntityInfoffUibUiUiiiffRC6CColor` (0x8013C72C)
  and `__ct__13CScriptEffectF9TUniqueIdRCQ24rstl66basic_string<...>CScriptEffect11ParamStruct
  bbbi` (0x80082950, 0x44C bytes). `CScriptEffect` has **no header** in `include/` even though
  retail names its constructor - so a named constructor is not evidence of a written class, and
  the vtable test is the one to trust.

## The one that is not blocked: `LoadAreaAttributes`, 81.25% -> 93.49%

`REAA`, retail 0x8013C2E4, 744 bytes, frame 144, retail symbol
`LoadAreaProperties__FR13CStateManagerR12CInputStreamRC11CEntityInfo`. Not yet `Matching`, so
**not counted** - this is a ratchet measurement, not a result. What changed, in the order the
measurements came:

1. **The port spelled it wrong.** `ScriptLoader.cpp`'s table said `{'REAA', &LoadAreaAttributes}`
   and nothing defined `LoadAreaAttributes`, so `_Z18LoadAreaAttributesR13CStateManager
   R12CInputStreamRK11CEntityInfo` sat on the link-gap list **while the function retail names was
   already in the tree** under `LoadAreaProperties` in `ScriptObjects/CScriptAreaProperties.cpp`.
   Same defect as the 64 thunks, one function. Fixed: the table now uses retail's spelling.
2. **`SLdrAreaAttributes` must not have a user-declared constructor.** Retail's loader calls
   `__ct__20SLdrEditorPropertiesFv` on the `editorProperties` *member*, in place, and
   `__dt__20SLdrEditorPropertiesFv` on the way out - there is no `__ct__18SLdrAreaAttributesFv`
   in `symbols.txt` because retail's aggregate's pair is implicit. The generated header declared
   one (and `SLdrStructMembers.cpp` defined it to close the host link), which made mwcceppc call
   a symbol that does not exist. Removing the declaration is what moved the local 4 bytes and
   fixed the frame.
3. **The constructor call site had the arguments in the wrong order.** The port passed
   `(density, normalLighting, 0.0f, 0.0f, needSky, darkWorld, environmentEffects, overrideSky,
   phazonDamage, 0, Black())` against a prototype that reads
   `(density, normalLighting, hasSkyBox, isDarkWorld, environmentEffects, skyBoxAssetId,
   phazonDamage, unk1, unk2, unk3, color)`. Because MWCC hands floats and integers to separate
   register sequences, the mis-ordered call still *looked* plausible - it cost 0 in registers and
   produced `li r6,0` for `hasSkyBox`, `needSky` as `environmentEffects`, and a `lfd`/`fsubs`
   round-trip to convert an `int` into the float parameter. **This was a live port bug, not only
   a codegen one.**
4. **`LdrToEntityInfo` takes a non-const `CEntityInfo&` in retail**, so the loader needs
   `const_cast<CEntityInfo&>(info)`. `config/G2ME01/symbols.txt` has exactly one
   `LdrToEntityInfo`, `__FR11CEntityInfoRC20SLdrEditorProperties` at 0x80239BD4, and the port
   header declares the *const* overload - which is why there are two entries on the link-gap
   list. `CScriptCannonBall.cpp`, `CScriptForgottenObject.cpp` and `CScriptSkyRipple.cpp` had
   already worked this out with a local re-declaration; this loader now does the same.
5. **mwcceppc emits independent field stores in source order**, so retail's store order *is* the
   source order: `environmentGroupSound`, `overrideSky`, `editorProperties.unknown_0x5d298a43`,
   `needSky`, `darkWorld`, `environmentEffects`, `density`, `normalLighting`, `phazonDamage`.
   Written in struct order the function was 81.25%; written in retail's order, with the same
   three `li`s landing in the same registers, it was 93.49%.
6. **`CColor::Black()` is a named local; the other two tail values are the `new`'s arguments.**
   Retail copies `Black()`'s four bytes into a local at `r1+24` instead of passing the pointer;
   `mgr.AllocateUniqueId()`'s result lands at `r1+20`; the `SLdrAreaAttributes` is at `r1+28`; and
   the two call temporaries are at `r1+16`. Declaring `CColor color` and letting
   `mgr.AllocateUniqueId()` / `LdrToEntityInfo(...)` be the `new`'s own arguments reproduces
   `r1+16/20/24/28` exactly. Declaring *named locals* for those two as well - the obvious reading
   of "three calls, three stack slots" - moves the whole frame 4 bytes the wrong way, and naming
   the local for the CColor but not for them is the combination that reproduces retail.

### What is left in it, and why it is not being called a result

34 differing instructions, in three groups (`tools/try_batch.py`, ranked by differing
instructions, not by objdiff's byte percentage):

- **8 x `cmpw r4,rX` vs `cmpw r6,rX`.** Retail reads the property id destructively
  (`lwz r4,8(r27)` / `lwz r4,0(r4)`) and puts the reloaded stream pointer in `r3`; mwcceppc here
  keeps the old pointer in `r3`, reloads into `r4` and materialises the id in `r6`. Eight source
  spellings of the two reads (`int`/`uint` id, `u16`/`unsigned` size, `for`/`while`, `u16`/`uint`
  count, `input.Get(kUint32Types)`, declarations split across statements) all produced the same
  34. This is register allocation, not logic.
- **`CColor::Black()` is sunk past `operator new` in retail** (`bl Black` after
  `bl __nw__FUlPCcPCc`, inside the `mr.`/`beq` guard) and hoisted before it here. Four
  instructions.
- **The two `lfs f0,0(0)` relocations** name `lbl_8041C140` / `lbl_8041C144` in retail and a
  section-relative `@445` here. Retail's unit has **two** distinct 0.0f entries in its 8 bytes of
  `.sdata2` and ours CSEs to one. This is the same small-data trap
  `RUNNING_THE_DECOMP.md` records for string literals, and it is the one item that would need the
  unit's `.sdata2` split settled before the last percent is even reachable.

Getting this to 100% is worth it for one reason beyond the one function: it is the only place in
the tree where the **entity-class vtable placement** for a loader-shaped function is exercised
at all, and 85 more depend on getting that right.

## The 85, and the ~75 classes behind them

76 distinct vtables for 85 loaders. Three classes are shared:

| vtable | loaders | `new` size |
| --- | --- | --- |
| `lbl_803B4A88` | `LoadDamageActor`, `LoadFogVolume`, `LoadEnvFxDensityController`, `LoadSilhouette`, `LoadRumbleEffect`, `LoadRadialDamage`, `LoadSpecialFunction`, `LoadSpinner` | 576 |
| `lbl_803B4648` | `LoadWorldLightFader`, `LoadDistanceFog` | 76 |
| `lbl_803B3708` | `LoadDebris`, `LoadDebrisExtended` | 944 |

**Eight loaders construct one class.** So closing `CScriptWhatever lbl_803B4A88` is eight
functions of link gap for one class, and the loader table is the cheapest possible place to
*learn* which class that is: the `new` size, the vtable address and the constructor address are
all that stand between a lane and the class's identity, and this file has all three for all 86.

One loader has no `new` at all: `LoadAIKeyframe` (`AIKF`, 0x800D5708, **56 bytes**, the smallest
of the 86) is a five-instruction wrapper that calls `LoadActorKeyframe` and ORs `0x40` into
`byte(obj + 52)`. It is the only one of the 86 that is not really a decompilation problem at
all - it is blocked only because `LoadActorKeyframe`'s class is missing.

## The 86, in size order

`new` size is the size of the entity object, from `r3` at the `operator new` call. "vtable
claimed by" is the `splits.txt` unit that owns the vtable's `.data` address; **nothing** means
`dtk` fills it from retail, i.e. the class is absent.

| loader | FourCC | addr | size | frame | `new` size | entity constructor | vtable | vtable claimed by |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `LoadAIKeyframe` | `AIKF` | 0x800D5708 | 56 | 16 | - | `(delegates to LoadActorKeyframe)` | `-` | **nothing** |
| `LoadWaypoint` | `WAYP` | 0x80073474 | 288 | 160 | 344 | `fn_80073844` | `lbl_803B20B0` | **nothing** |
| `LoadCameraWaypoint` | `CAMW` | 0x800A5474 | 288 | 160 | 344 | `fn_800A56B8` | `lbl_803B32E0` | **nothing** |
| `LoadSpiderBallAttractionSurface` | `BALS` | 0x800FED74 | 292 | 160 | 384 | `fn_800FEFDC` | `lbl_803B4588` | **nothing** |
| `LoadTargetingPoint` | `TGPT` | 0x8012CA24 | 296 | 160 | 352 | `fn_8012CC14` | `lbl_803B2B30` | **nothing** |
| `LoadTimeKeyframe` | `TKEY` | 0x801F9050 | 320 | 112 | 40 | `fn_801F9474` | `lbl_803B7AE0` | **nothing** |
| `LoadRipple` | `RIPL` | 0x801173A4 | 324 | 112 | 52 | `fn_80117580` | `lbl_803B4D18` | **nothing** |
| `LoadRelay` | `SRLY` | 0x800B8EFC | 340 | 112 | 40 | `fn_800B919C` | `lbl_803B35B8` | **nothing** |
| `LoadSpiderBallWaypoint` | `BALW` | 0x800E8C8C | 344 | 160 | 392 | `fn_800E9B58` | `lbl_803B3988` | **nothing** |
| `LoadGrapplePoint` | `GRAP` | 0x800ED18C | 368 | 256 | 408 | `fn_800ED668` | `lbl_803B3AC0` | **nothing** |
| `LoadCameraShaker` | `CAMS` | 0x800D5128 | 392 | 576 | 296 | `fn_800D5578` | `lbl_803B37A0` | **nothing** |
| `LoadAIJumpPoint` | `AJMP` | 0x8014FDFC | 392 | 160 | 392 | `fn_80150148` | `lbl_803B2E58` | **nothing** |
| `LoadScriptLayerController` | `SLCT` | 0x8022F7C8 | 396 | 128 | 48 | `fn_8022FEEC` | `lbl_803B88D0` | **nothing** |
| `LoadSwitch` | `SWTC` | 0x8014A2B8 | 404 | 112 | 40 | `fn_8014A564` | `lbl_803B52A0` | **nothing** |
| `LoadPathMeshCtrl` | `PMCT` | 0x801EB64C | 412 | 160 | 352 | `fn_801EBA30` | `lbl_803B7818` | **nothing** |
| `LoadMemoryRelay` | `MRLY` | 0x8017645C | 420 | 112 | 40 | `fn_80176734` | `lbl_803B5630` | **nothing** |
| `LoadAreaDamage` | `ADMG` | 0x801E2EAC | 444 | 160 | 124 | `fn_801E374C` | `lbl_803B7640` | **nothing** |
| `LoadWorldLightFader` | `WLIT` | 0x800FF484 | 452 | 144 | 76 | `fn_800FFAF0` | `lbl_803B4648` | **nothing** |
| `LoadControllerAction` | `CNTA` | 0x80149E38 | 476 | 128 | 48 | `fn_8014A1C0` | `lbl_803B5280` | **nothing** |
| `LoadMidi` | `MIDI` | 0x8015C67C | 476 | 128 | 60 | `fn_8015CC78` | `lbl_803B5410` | **nothing** |
| `LoadPointOfInterest` | `POIN` | 0x8010EAE0 | 484 | 176 | 352 | `fn_8010EF4C` | `lbl_803B4BE0` | **nothing** |
| `LoadTimer` | `TIMR` | 0x800866C8 | 500 | 112 | 72 | `fn_80086CB8` | `lbl_803B28F8` | **nothing** |
| `LoadDamageActor` | `DMGA` | 0x80100D54 | 504 | 256 | 576 | `fn_80109A20` | `lbl_803B4A88` | **nothing** |
| `LoadCounter` | `CNTR` | 0x80092310 | 512 | 112 | 52 | `fn_8009288C` | `lbl_803B29A0` | **nothing** |
| `LoadRandomRelay` | `RRLY` | 0x800BA234 | 512 | 112 | 48 | `fn_800BAA5C` | `lbl_803B35F8` | **nothing** |
| `LoadCameraBlurKeyframe` | `BLUR` | 0x800BD63C | 516 | 128 | 56 | `fn_800BD9E4` | `lbl_803B3680` | **nothing** |
| `LoadAIHint` | `AIHT` | 0x8022A0C0 | 524 | 176 | 376 | `fn_8022A408` | `lbl_803B86B8` | **nothing** |
| `LoadPlayerHint` | `HINT` | 0x8010C018 | 528 | 176 | 440 | `fn_8010C418` | `lbl_803B4B30` | **nothing** |
| `LoadPickupGenerator` | `PKGN` | 0x8010C590 | 532 | 144 | 660 | `fn_8010D78C` | `lbl_803B4BB0` | **nothing** |
| `LoadFogVolume` | `FOGV` | 0x80101E10 | 540 | 256 | 576 | `fn_80109A20` | `lbl_803B4A88` | **nothing** |
| `LoadActorRotate` | `AROT` | 0x80109F88 | 540 | 544 | 524 | `fn_8010B054` | `lbl_803B4B10` | **nothing** |
| `LoadAIWaypoint` | `AIWP` | 0x801E8558 | 544 | 176 | 368 | `fn_801E87D8` | `lbl_803B7680` | **nothing** |
| `LoadRepulsor` | `REPL` | 0x802276A8 | 544 | 176 | 360 | `fn_802279B0` | `lbl_803B84F8` | **nothing** |
| `LoadActorKeyframe` | `ACKF` | 0x800D5740 | 548 | 128 | 56 | `fn_800D60C8` | `lbl_803B37C0` | **nothing** |
| `LoadCoverPoint` | `COVR` | 0x800EC298 | 552 | 176 | 400 | `fn_800EC790` | `lbl_803B2DDC` | **nothing** |
| `LoadEnvFxDensityController` | `FXDC` | 0x80101610 | 552 | 192 | 576 | `fn_80109A20` | `lbl_803B4A88` | **nothing** |
| `LoadPlayerStateChange` | `PSCH` | 0x8014B294 | 560 | 144 | 56 | `fn_8014B558` | `lbl_803B5380` | **nothing** |
| `LoadCameraPitch` | `CAMP` | 0x801FB1E4 | 576 | 464 | 552 | `fn_801FBAAC` | `lbl_803B7B10` | **nothing** |
| `LoadSilhouette` | `SILH` | 0x80101B48 | 580 | 256 | 576 | `fn_80109A20` | `lbl_803B4A88` | **nothing** |
| `LoadDock` | `DOCK` | 0x800B63DC | 636 | 144 | 744 | `fn_800B76E4` | `lbl_803B3498` | **nothing** |
| `LoadRumbleEffect` | `RUMB` | 0x80101394 | 636 | 256 | 576 | `fn_80109A20` | `lbl_803B4A88` | **nothing** |
| `LoadTriggerOrientated` | `TRGO` | 0x801B7F44 | 636 | 288 | 552 | `fn_801B83DC` | `lbl_803B6910` | **nothing** |
| `LoadBallTrigger` | `BALT` | 0x80118B08 | 648 | 288 | 584 | `fn_801193A8` | `lbl_803B4D78` | **nothing** |
| `LoadHUDHint` | `HHNT` | 0x802328A4 | 656 | 176 | 392 | `fn_802331D8` | `lbl_803B2BEC` | **nothing** |
| `LoadTrigger` | `TRGR` | 0x80070DB4 | 680 | 272 | 456 | `fn_8007311C` | `lbl_803B2018` | **nothing** |
| `LoadEMPulse` | `EMPU` | 0x8012DEA4 | 688 | 192 | 392 | `fn_8012E6AC` | `lbl_803B4F10` | **nothing** |
| `LoadShadowProjector` | `SHDW` | 0x801928DC | 696 | 208 | 392 | `fn_80192FA0` | `lbl_803B5BC0` | **nothing** |
| `LoadDistanceFog` | `DFOG` | 0x800FF648 | 704 | 192 | 76 | `fn_800FFAF0` | `lbl_803B4648` | **nothing** |
| `LoadTriggerEllipsoid` | `TRGE` | 0x801E2394 | 712 | 288 | 512 | `fn_801E2CB4` | `lbl_803B75A8` | **nothing** |
| `LoadGenerator` | `GENR` | 0x800A4850 | 720 | 160 | 64 | `fn_800A5398` | `lbl_803B32C0` | **nothing** |
| `LoadAreaAttributes` | `REAA` | 0x8013C2E4 | 744 | 144 | 92 | `__ct__21CScriptAreaPropertiesF9TUniqueIdRC11CEntityInfoffUibUiUiiiffRC` | `__vt__21CScriptAreaProperties` | MetroidPrime/ScriptObjects/CScriptAreaProperties.cpp |
| `LoadDamageableTrigger` | `DTRG` | 0x800D06CC | 744 | 592 | 456 | `fn_800D0DE4` | `lbl_803B2D60` | **nothing** |
| `LoadSoundModifier` | `SNDM` | 0x8022EC6C | 760 | 688 | 336 | `fn_8022F674` | `lbl_803B88B0` | **nothing** |
| `LoadSteam` | `STEM` | 0x80116C74 | 764 | 288 | 488 | `fn_8011723C` | `lbl_803B4C80` | **nothing** |
| `LoadSubtitle` | `SUBT` | 0x8020DB08 | 772 | 224 | 3728 | `fn_8020E2F0` | `lbl_803B7D70` | **nothing** |
| `LoadRadialDamage` | `RADD` | 0x80101838 | 784 | 256 | 576 | `fn_80109A20` | `lbl_803B4A88` | **nothing** |
| `LoadTeamAI` | `TMAI` | 0x801724CC | 812 | 176 | 160 | `fn_801756EC` | `lbl_803B5600` | **nothing** |
| `LoadCameraFilterKeyframe` | `FILT` | 0x800BCFC4 | 828 | 224 | 68 | `fn_800BD574` | `lbl_803B3660` | **nothing** |
| `LoadPortalTransition` | `PRTT` | 0x802317B0 | 868 | 240 | 84 | `fn_80232234` | `lbl_803B2BCC` | **nothing** |
| `LoadSpecialFunction` | `SPFN` | 0x801020A4 | 948 | 304 | 576 | `fn_80109A20` | `lbl_803B4A88` | **nothing** |
| `LoadVisorGoo` | `VGOO` | 0x80148D4C | 956 | 224 | 400 | `fn_8014983C` | `lbl_803B51E8` | **nothing** |
| `LoadCamera` | `CAMR` | 0x801DE74C | 956 | 912 | 872 | `fn_801DF138` | `lbl_803B7458` | **nothing** |
| `LoadTextPane` | `TXPN` | 0x801FFF5C | 1072 | 416 | 3768 | `fn_80200914` | `lbl_803B7C18` | **nothing** |
| `LoadSpinner` | `SPIN` | 0x80100F4C | 1096 | 272 | 576 | `fn_80109A20` | `lbl_803B4A88` | **nothing** |
| `LoadConditionalRelay` | `CRLY` | 0x80197A30 | 1128 | 368 | 128 | `fn_801981EC` | `lbl_803B5EB0` | **nothing** |
| `LoadDamageableTriggerOriented` | `DTRO` | 0x80226D30 | 1164 | 736 | 552 | `fn_802273A8` | `lbl_803B2CE4` | **nothing** |
| `LoadColorModulate` | `CLRM` | 0x801529C0 | 1208 | 336 | 140 | `fn_8015444C` | `lbl_803B53E0` | **nothing** |
| `LoadAdvancedCounter` | `ACNT` | 0x8022E13C | 1208 | 208 | - | `fn_8022E8DC` | `lbl_803B8890` | **nothing** |
| `LoadDebris` | `DBR1` | 0x800D1E6C | 1248 | 672 | 944 | `fn_800D48D8` | `lbl_803B3708` | **nothing** |
| `LoadDynamicLight` | `DLHT` | 0x8021FBCC | 1256 | 1392 | 1096 | `fn_802216C8` | `lbl_803B83F8` | **nothing** |
| `LoadAmbientAI` | `AMIA` | 0x801790BC | 1308 | 1152 | 864 | `fn_80179E1C` | `lbl_803B5650` | **nothing** |
| `LoadControlHint` | `CTLH` | 0x8022C524 | 1324 | 464 | 512 | `fn_8022CEFC` | `lbl_803B8778` | **nothing** |
| `LoadPathCamera` | `PCAM` | 0x801E0004 | 1416 | 1184 | 776 | `fn_801E09D8` | `lbl_803B7558` | **nothing** |
| `LoadSound` | `SOND` | 0x8009D644 | 1528 | 304 | 432 | `fn_8009F240` | `lbl_803B3190` | **nothing** |
| `LoadCameraHint` | `CAMH` | 0x800B7928 | 1564 | 400 | 576 | `fn_800B8524` | `lbl_803B3530` | **nothing** |
| `LoadEffect` | `EFCT` | 0x80080394 | 1772 | 640 | 720 | `__ct__13CScriptEffectF9TUniqueIdRCQ24rstl66basic_string<c,Q24rstl14cha` | `lbl_803B2768` | **nothing** |
| `LoadSpindleCamera` | `SPND` | 0x801DF2A0 | 1832 | 2752 | 1760 | `fn_801DFE28` | `lbl_803B74D8` | **nothing** |
| `LoadWorldTeleporter` | `TEL1` | 0x80147A78 | 1884 | 384 | 164 | `fn_801489E4` | `lbl_803B51C8` | **nothing** |
| `LoadVisorFlare` | `FLAR` | 0x801468C4 | 2036 | 512 | 616 | `fn_801477D8` | `lbl_803B5148` | **nothing** |
| `LoadPlatform` | `PLAT` | 0x8009F550 | 2072 | 1760 | 1168 | `fn_800A40E8` | `lbl_803B3210` | **nothing** |
| `LoadActor` | `ACTR` | 0x8006EC28 | 2284 | 1456 | 920 | `fn_80070744` | `lbl_803B1F80` | **nothing** |
| `LoadDoor` | `DOOR` | 0x8007B1F8 | 2376 | 1792 | 1200 | `fn_8007DAFC` | `lbl_803B2698` | **nothing** |
| `LoadSurfaceCamera` | `SURC` | 0x801E979C | 2468 | 1808 | 648 | `fn_801EA83C` | `lbl_803B7798` | **nothing** |
| `LoadDebrisExtended` | `DBR2` | 0x800D1060 | 2948 | 1008 | 944 | `fn_800D3F20` | `lbl_803B3708` | **nothing** |
| `LoadRoomAcoustics` | `RMAC` | 0x801344A8 | 2992 | 512 | 236 | `fn_8013559C` | `lbl_803B2BAC` | **nothing** |
| `LoadWater` | `WATR` | 0x800D6644 | 3640 | 1200 | 816 | `fn_800DA20C` | `lbl_803B3810` | **nothing** |
