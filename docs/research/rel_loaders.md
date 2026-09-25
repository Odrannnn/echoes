# The REL entity loaders: every one, identified

Measured 2026-09-25 by lane `d1` at commit `a1d3702`, from `build/G2ME01/main.elf` and
`config/G2ME01/symbols.txt`. The method is in the next section and is three commands: read the
link-gap list, decode one static initialiser, and match the two tables positionally.

## How the 234 became 159 addresses, and why that matters

The port's `src/MetroidPrime/ScriptLoader.cpp` has a 184-entry `{FourCC, FScriptLoader}`
table. Retail builds **the same table**, in a static initialiser, at
`__sinit_ScriptLoader_cpp` (0x80242894, 5696 bytes, a `Matching` unit). Decoding its
`lis`/`addi`/`stw` dataflow gives the retail address of **every** loader, keyed by the same
FourCC. The port's 184 tags and retail's 184 tags then match **position for position, with
zero mismatches** - which is the check that makes the mapping evidence rather than a guess.

The decode is short, and the two things that make it work are worth writing down because
neither is obvious. First, the table is built into `r3`, which the initialiser sets with
`lis r3,-32706` and never changes, so the base is `0x80460000 - 7880 = 0x8045E138` and the
first store is a `stwu`, not a `stw` - reading the `stwu`'s own displacement as a table offset
puts every entry 7880 bytes early. Second, **two entries (`BLUR`, `DBAR`) are stored from
registers that were spilled to the stack and reloaded**, so a decoder that tracks only
`lis`/`addi` into registers silently drops them; handling `stw ...,(r1)` / `lwz ...,(r1)`
against a simulated `r1` gets all 184.


That resolves 159 of the 160 signature-matching link-gap symbols. The 160th is
`SetLoader_CannonBall`, a port-side helper with no retail counterpart at that name; retail's
is `SetLoader_CannonBall__FPPF...` at 0x8021FAB4, which `config/G2ME01/symbols.txt`
already names.

**Correction to `port_link_gap.md`, which said 133 of them "are not named in `symbols.txt`
at all".** All 159 are named there - as `fn_802189A4` and friends. dtk could not pair them
because the only caller is a static initialiser, not code objdiff pairs on. They were
identifiable the whole time; the port's names are the human-readable half and retail's
`fn_` names were never missing.

## The shape, and the one number that changed the plan

Of the 159, **73 are the 44-byte (0x2C) vtable thunk** and 86 are real functions of
288..3640 bytes. The briefing expected 20 thunks and 133 unknowns; the truth is 73 free
and 86 real, and **all 159 identified**. The 20 in the briefing are the ones retail
already names - they landed as `main/MetroidPrime/ScriptLoaderRel` (42/42 functions, 100%,
`Matching`) in commit `77f38a9`, before this lane started, so there was nothing to write
there. One of the 73 (`LoadWallWalker`) is inside that unit's claimed range and so was
already `Matching` too; **72 were genuinely free**, and those are what landed.

The thunk is twelve instructions and one displacement:

```
stwu r1,-16(r1) ; mflr r0 ; stw r0,20(r1)
lwz  r6,D(r13)            ; D is off _SDA_BASE_ = 0x8041FD80, naming a .sbss slot
lwz  r12,K(r6)            ; K = 0, 4, 8, 12 or 16: the index into that struct
mtctr r12 ; bctrl
lwz  r0,20(r1) ; mtlr r0 ; addi r1,r1,16 ; blr
```

`r13` is `_SDA_BASE_` and the slot it names holds a *pointer to* a per-module struct of
`FScriptLoader`s, which is why `K` is not always 0: five modules register more than one
loader through one slot (Parasite/Brizgee/Crystallite, the four Sporb loaders, the two
GunTurret loaders, the two Splitter loaders, DigitalGuardian/Head). **93 thunks of this shape
exist in the DOL, and the accounting adds up exactly**: 20 in `ScriptLoaderRel` (already
`Matching`), 72 landed by this lane, and one more at 0x8022D548 that no tag table reaches -
it dispatches through 0x804195D8 and its setter is
`SetSScriptForgottenObject_FuncPtrs` (0x8022D574), so it is `LoadForgottenObject`, reached
through the forgotten-object func-ptr struct rather than the FourCC table. The port already
defines `LoadForgottenObject`, which is why it is not on the gap list.

## What landed

**64 new `Matching` DOL units** under `src/MetroidPrime/ScriptLoader/`, one per `.sbss` slot,
covering all **72** remaining thunks: 3168 bytes of `.text` and 512 bytes of `.sbss`.
+72 matched, +72 linked, port link gap **724 -> 652**, all 64 units at 100% and
`complete: true`. Four edits per unit, exactly the recipe in `RUNNING_THE_DECOMP.md`; each
unit's source comment records the ranges it claims and why it does not claim the setter.

### The three facts that were not obvious, each of which cost a build

1. **The `.sbss` slot must be renamed, and the setter must not be.** The slot arrives in
   `symbols.txt` as `lbl_804193E0`; the setter that writes it, `fn_802189D0`, is imported *by
   name* by 20-odd REL modules (`Blogg` imports `fn_80218B08`, `ChozoGhost` imports
   `fn_80218D24`, ...). A DOL symbol a REL module imports **cannot be renamed**: dtk regenerates
   the DOL's own objects from `symbols.txt`, so intra-DOL references follow the rename, but the
   modules' objects are the retail bytes and do not. So: rename the thunk and the slot, leave
   the setter as `fn_`, and **do not claim the setter's 8 bytes** - a range that swallows it
   deletes the symbol and `dtk rel make` fails with `Failed to find symbol fn_80227530 in any
   module`. That is the same asymmetry `RUNNING_THE_DECOMP.md` records from the other
   direction under "A DOL symbol cannot be renamed ... if a Matching REL unit already defines
   that name".
2. **A multi-slot module needs a named struct, and slot 0 is not `(*p)(...)`.** Five modules
   register 2-4 loaders through one slot. For those the body is `p->slot0(mgr, input, info)`;
   writing `(*p)(mgr, input, info)` on a struct pointer is `call of non-function` and MWCC
   aborts. The single-slot form is `(*p)(...)` and it is what the 20 landed ones use.
3. **`uint` is not visible through `ScriptLoader.hpp` alone.** `ScriptLoaderRel.cpp` gets it via
   `ScriptLoaderRel.hpp` -> `TGameTypes.hpp`; a new file that includes only `ScriptLoader.hpp`
   fails on `declaration syntax error` at the first `uint`. `unsigned int` is the fix, and it
   makes the object byte-identical either way.

## The 72 thunks (all landed)

`unit` is the `.cpp` under `src/MetroidPrime/ScriptLoader/`; `global` is the `.sbss` slot;
`slot` is the `K` above. One unit per global, so the five multi-slot modules get a unit with
more than one thunk. (`LoadWallWalker` is the 73rd thunk of the shape and is not here: it is
inside `main/MetroidPrime/ScriptLoaderRel`'s claimed range and was already `Matching`.)

Five units hold more than one thunk, named after their slot-0 loader: `Parasite.cpp`
(Crystallite, Brizgee, Parasite), `SporbBase.cpp` (Projectile, Top, Base, Needle),
`GunTurretBase.cpp`, `SplitterMainChassis.cpp`, `DigitalGuardian.cpp`.

| unit | thunk | addr | global | slot | retail symbol |
| --- | --- | --- | --- | --- | --- |
| `SpacePirate.cpp` | `LoadSpacePirate` | 0x80200E10 | 0x80419358 | 0 | `fn_80200E10` |
| `Kralee.cpp` | `LoadKralee` | 0x80200E44 | 0x80419360 | 0 | `fn_80200E44` |
| `Parasite.cpp` | `LoadCrystallite` | 0x80200E78 | 0x80419368 | 8 | `fn_80200E78` |
| `Parasite.cpp` | `LoadBrizgee` | 0x80200EA4 | 0x80419368 | 4 | `fn_80200EA4` |
| `Parasite.cpp` | `LoadParasite` | 0x80200ED0 | 0x80419368 | 0 | `fn_80200ED0` |
| `PillBug.cpp` | `LoadPillBug` | 0x80200F04 | 0x80419370 | 0 | `fn_80200F04` |
| `SporbBase.cpp` | `LoadSporbProjectile` | 0x80213C08 | 0x804193A8 | 4 | `fn_80213C08` |
| `SporbBase.cpp` | `LoadSporbTop` | 0x80213C34 | 0x804193A8 | 12 | `fn_80213C34` |
| `SporbBase.cpp` | `LoadSporbBase` | 0x80213C60 | 0x804193A8 | 0 | `fn_80213C60` |
| `SporbBase.cpp` | `LoadSporbNeedle` | 0x80213C8C | 0x804193A8 | 8 | `fn_80213C8C` |
| `Sandworm.cpp` | `LoadSandworm` | 0x80218850 | 0x804193C0 | 0 | `fn_80218850` |
| `CommandPirate.cpp` | `LoadCommandPirate` | 0x80218884 | 0x804193C8 | 0 | `fn_80218884` |
| `DarkSamus.cpp` | `LoadDarkSamus` | 0x802188B8 | 0x804193D0 | 0 | `fn_802188B8` |
| `Ings.cpp` | `LoadIngs` | 0x802188EC | 0x804193D8 | 0 | `fn_802188EC` |
| `SandBoss.cpp` | `LoadSandBoss` | 0x802189A4 | 0x804193E0 | 0 | `fn_802189A4` |
| `FlyingPirate.cpp` | `LoadFlyingPirate` | 0x802189D8 | 0x804193E8 | 0 | `fn_802189D8` |
| `Grenchler.cpp` | `LoadGrenchler` | 0x80218A0C | 0x804193F0 | 0 | `fn_80218A0C` |
| `MediumIng.cpp` | `LoadMediumIng` | 0x80218A40 | 0x804193F8 | 0 | `fn_80218A40` |
| `MinorIng.cpp` | `LoadMinorIng` | 0x80218A74 | 0x80419400 | 0 | `fn_80218A74` |
| `ElitePirate.cpp` | `LoadElitePirate` | 0x80218AA8 | 0x80419408 | 0 | `fn_80218AA8` |
| `Blogg.cpp` | `LoadBlogg` | 0x80218ADC | 0x80419410 | 0 | `fn_80218ADC` |
| `MetroidAlpha.cpp` | `LoadMetroidAlpha` | 0x80218B3C | 0x80419418 | 0 | `fn_80218B3C` |
| `GunTurretBase.cpp` | `LoadGunTurretTop` | 0x80218B70 | 0x80419420 | 4 | `fn_80218B70` |
| `GunTurretBase.cpp` | `LoadGunTurretBase` | 0x80218B9C | 0x80419420 | 0 | `fn_80218B9C` |
| `Lumite.cpp` | `LoadLumite` | 0x80218BD0 | 0x80419428 | 0 | `fn_80218BD0` |
| `Shrieker.cpp` | `LoadShrieker` | 0x80218C04 | 0x80419430 | 0 | `fn_80218C04` |
| `Splinter.cpp` | `LoadSplinter` | 0x80218C38 | 0x80419438 | 0 | `fn_80218C38` |
| `SplitterMainChassis.cpp` | `LoadSplitterCommandModule` | 0x80218C98 | 0x80419440 | 4 | `fn_80218C98` |
| `SplitterMainChassis.cpp` | `LoadSplitterMainChassis` | 0x80218CC4 | 0x80419440 | 0 | `fn_80218CC4` |
| `ChozoGhost.cpp` | `LoadChozoGhost` | 0x80218CF8 | 0x80419448 | 0 | `fn_80218CF8` |
| `Tryclops.cpp` | `LoadTryclops` | 0x80218D2C | 0x80419450 | 0 | `fn_80218D2C` |
| `WispTentacle.cpp` | `LoadWispTentacle` | 0x80218D60 | 0x80419458 | 0 | `fn_80218D60` |
| `SpankWeed.cpp` | `LoadSpankWeed` | 0x80218D94 | 0x80419460 | 0 | `fn_80218D94` |
| `DarkTrooper.cpp` | `LoadDarkTrooper` | 0x80218DC8 | 0x80419468 | 0 | `fn_80218DC8` |
| `GlowBug.cpp` | `LoadGlowBug` | 0x80218DFC | 0x80419470 | 0 | `fn_80218DFC` |
| `IngSpaceJumpGuardian.cpp` | `LoadIngSpaceJumpGuardian` | 0x8021DC00 | 0x804194F8 | 0 | `fn_8021DC00` |
| `DigitalGuardian.cpp` | `LoadDigitalGuardianHead` | 0x8021F958 | 0x80419510 | 4 | `fn_8021F958` |
| `DigitalGuardian.cpp` | `LoadDigitalGuardian` | 0x8021F984 | 0x80419510 | 0 | `fn_8021F984` |
| `Shredder.cpp` | `LoadShredder` | 0x8021F9B8 | 0x80419518 | 0 | `fn_8021F9B8` |
| `FrontEndDataNetwork.cpp` | `LoadFrontEndDataNetwork` | 0x8021F9EC | 0x80419520 | 0 | `fn_8021F9EC` |
| `StoneToad.cpp` | `LoadStoneToad` | 0x8021FA20 | 0x80419528 | 0 | `fn_8021FA20` |
| `Coin.cpp` | `LoadCoin` | 0x8021FA54 | 0x80419530 | 0 | `fn_8021FA54` |
| `CannonBall.cpp` | `LoadCannonBall` | 0x8021FA88 | 0x80419538 | 0 | `fn_8021FA88` |
| `Krocus.cpp` | `LoadKrocus` | 0x802274C8 | 0x80419550 | 0 | `fn_802274C8` |
| `AIMannedTurret.cpp` | `LoadAIMannedTurret` | 0x80227504 | 0x80419560 | 0 | `fn_80227504` |
| `EmperorIngStage1.cpp` | `LoadEmperorIngStage1` | 0x80227540 | 0x80419570 | 0 | `fn_80227540` |
| `OctopedeSegment.cpp` | `LoadOctopedeSegment` | 0x80227574 | 0x80419578 | 0 | `fn_80227574` |
| `Rezbit.cpp` | `LoadRezbit` | 0x80227ACC | 0x80419580 | 0 | `fn_80227ACC` |
| `RsfAudio.cpp` | `LoadRsfAudio` | 0x80227B00 | 0x80419588 | 0 | `fn_80227B00` |
| `IngPuddle.cpp` | `LoadIngPuddle` | 0x80229EB4 | 0x80419598 | 0 | `fn_80229EB4` |
| `FlyerSwarm.cpp` | `LoadFlyerSwarm` | 0x80229F90 | 0x804195A0 | 0 | `fn_80229F90` |
| `StreamedMovie.cpp` | `LoadStreamedMovie` | 0x80229FC4 | 0x804195A8 | 0 | `fn_80229FC4` |
| `IngSpiderBallGuardian.cpp` | `LoadIngSpiderBallGuardian` | 0x80229FF8 | 0x804195B0 | 0 | `fn_80229FF8` |
| `PuddleSpore.cpp` | `LoadPuddleSpore` | 0x8022A02C | 0x804195B8 | 0 | `fn_8022A02C` |
| `EmperorIngStage2Tentacle.cpp` | `LoadEmperorIngStage2Tentacle` | 0x8022A544 | 0x804195C0 | 0 | `fn_8022A544` |
| `BacteriaSwarm.cpp` | `LoadBacteriaSwarm` | 0x8022A580 | 0x804195D0 | 0 | `fn_8022A580` |
| `MetareeSwarm.cpp` | `LoadMetareeSwarm` | 0x8022D57C | 0x804195E0 | 0 | `fn_8022D57C` |
| `IngBlobSwarm.cpp` | `LoadIngBlobSwarm` | 0x8022E108 | 0x804195E8 | 0 | `fn_8022E108` |
| `EmperorIngStage3.cpp` | `LoadEmperorIngStage3` | 0x8022EB9C | 0x804195F0 | 0 | `fn_8022EB9C` |
| `DestructableBarrier.cpp` | `LoadDestructableBarrier` | 0x8022EBD0 | 0x804195F8 | 0 | `fn_8022EBD0` |
| `SwampBossStage2.cpp` | `LoadSwampBossStage2` | 0x8022EC04 | 0x80419600 | 0 | `fn_8022EC04` |
| `SwampBossStage1.cpp` | `LoadSwampBossStage1` | 0x8022EC38 | 0x80419608 | 0 | `fn_8022EC38` |
| `IngBoostBallGuardian.cpp` | `LoadIngBoostBallGuardian` | 0x8022FF98 | 0x80419610 | 0 | `fn_8022FF98` |
| `PlantScarabSwarm.cpp` | `LoadPlantScarabSwarm` | 0x8022FFCC | 0x80419618 | 0 | `fn_8022FFCC` |
| `SkyRipple.cpp` | `LoadSkyRipple` | 0x80232308 | 0x80419630 | 0 | `fn_80232308` |
| `FogOverlay.cpp` | `LoadFogOverlay` | 0x80232808 | 0x80419638 | 0 | `fn_80232808` |
| `MysteryFlyer.cpp` | `LoadMysteryFlyer` | 0x8023283C | 0x80419640 | 0 | `fn_8023283C` |
| `AtomicBeta.cpp` | `LoadAtomicBeta` | 0x80232870 | 0x80419648 | 0 | `fn_80232870` |
| `EyeBall.cpp` | `LoadEyeBall` | 0x80234900 | 0x80419650 | 0 | `fn_80234900` |
| `DarkSamusBattleStage.cpp` | `LoadDarkSamusBattleStage` | 0x80235DA0 | 0x80419658 | 0 | `fn_80235DA0` |
| `DarkCommando.cpp` | `LoadDarkCommando` | 0x80235DD4 | 0x80419660 | 0 | `fn_80235DD4` |
| `RubiksPuzzle.cpp` | `LoadRubiksPuzzle` | 0x802399C8 | 0x804196C0 | 0 | `fn_802399C8` |

## The 86 real loaders (not landed)

`stwu r1,-N(r1)` is the frame size; these are ordinary decompilation, 288 to 3640 bytes,
77500 bytes in total - roughly **25x** the whole thunk family. The biggest first.

| loader | FourCC | addr | size | frame | retail symbol |
| --- | --- | --- | --- | --- | --- |
| `LoadWater` | `WATR` | 0x800D6644 | 3640 | 1200 | `fn_800D6644` |
| `LoadRoomAcoustics` | `RMAC` | 0x801344A8 | 2992 | 512 | `fn_801344A8` |
| `LoadDebrisExtended` | `DBR2` | 0x800D1060 | 2948 | 1008 | `fn_800D1060` |
| `LoadSurfaceCamera` | `SURC` | 0x801E979C | 2468 | 1808 | `fn_801E979C` |
| `LoadDoor` | `DOOR` | 0x8007B1F8 | 2376 | 1792 | `fn_8007B1F8` |
| `LoadActor` | `ACTR` | 0x8006EC28 | 2284 | 1456 | `fn_8006EC28` |
| `LoadPlatform` | `PLAT` | 0x8009F550 | 2072 | 1760 | `fn_8009F550` |
| `LoadVisorFlare` | `FLAR` | 0x801468C4 | 2036 | 512 | `fn_801468C4` |
| `LoadWorldTeleporter` | `TEL1` | 0x80147A78 | 1884 | 384 | `fn_80147A78` |
| `LoadSpindleCamera` | `SPND` | 0x801DF2A0 | 1832 | 2752 | `fn_801DF2A0` |
| `LoadEffect` | `EFCT` | 0x80080394 | 1772 | 640 | `fn_80080394` |
| `LoadCameraHint` | `CAMH` | 0x800B7928 | 1564 | 400 | `fn_800B7928` |
| `LoadSound` | `SOND` | 0x8009D644 | 1528 | 304 | `fn_8009D644` |
| `LoadPathCamera` | `PCAM` | 0x801E0004 | 1416 | 1184 | `fn_801E0004` |
| `LoadControlHint` | `CTLH` | 0x8022C524 | 1324 | 464 | `fn_8022C524` |
| `LoadAmbientAI` | `AMIA` | 0x801790BC | 1308 | 1152 | `fn_801790BC` |
| `LoadDynamicLight` | `DLHT` | 0x8021FBCC | 1256 | 1392 | `fn_8021FBCC` |
| `LoadDebris` | `DBR1` | 0x800D1E6C | 1248 | 672 | `fn_800D1E6C` |
| `LoadColorModulate` | `CLRM` | 0x801529C0 | 1208 | 336 | `fn_801529C0` |
| `LoadAdvancedCounter` | `ACNT` | 0x8022E13C | 1208 | 208 | `fn_8022E13C` |
| `LoadDamageableTriggerOriented` | `DTRO` | 0x80226D30 | 1164 | 736 | `fn_80226D30` |
| `LoadConditionalRelay` | `CRLY` | 0x80197A30 | 1128 | 368 | `fn_80197A30` |
| `LoadSpinner` | `SPIN` | 0x80100F4C | 1096 | 272 | `fn_80100F4C` |
| `LoadTextPane` | `TXPN` | 0x801FFF5C | 1072 | 416 | `fn_801FFF5C` |
| `LoadVisorGoo` | `VGOO` | 0x80148D4C | 956 | 224 | `fn_80148D4C` |
| `LoadCamera` | `CAMR` | 0x801DE74C | 956 | 912 | `fn_801DE74C` |
| `LoadSpecialFunction` | `SPFN` | 0x801020A4 | 948 | 304 | `fn_801020A4` |
| `LoadPortalTransition` | `PRTT` | 0x802317B0 | 868 | 240 | `fn_802317B0` |
| `LoadCameraFilterKeyframe` | `FILT` | 0x800BCFC4 | 828 | 224 | `fn_800BCFC4` |
| `LoadTeamAI` | `TMAI` | 0x801724CC | 812 | 176 | `fn_801724CC` |
| `LoadRadialDamage` | `RADD` | 0x80101838 | 784 | 256 | `fn_80101838` |
| `LoadSubtitle` | `SUBT` | 0x8020DB08 | 772 | 224 | `fn_8020DB08` |
| `LoadSteam` | `STEM` | 0x80116C74 | 764 | 288 | `fn_80116C74` |
| `LoadSoundModifier` | `SNDM` | 0x8022EC6C | 760 | 688 | `fn_8022EC6C` |
| `LoadDamageableTrigger` | `DTRG` | 0x800D06CC | 744 | 592 | `fn_800D06CC` |
| `LoadAreaAttributes` <sup>†</sup> | `REAA` | 0x8013C2E4 | 744 | 144 | `LoadAreaProperties__FR13CStateManagerR12CInputStreamRC11CEntityInfo` |
| `LoadGenerator` | `GENR` | 0x800A4850 | 720 | 160 | `fn_800A4850` |
| `LoadTriggerEllipsoid` | `TRGE` | 0x801E2394 | 712 | 288 | `fn_801E2394` |
| `LoadDistanceFog` | `DFOG` | 0x800FF648 | 704 | 192 | `fn_800FF648` |
| `LoadShadowProjector` | `SHDW` | 0x801928DC | 696 | 208 | `fn_801928DC` |
| `LoadEMPulse` | `EMPU` | 0x8012DEA4 | 688 | 192 | `fn_8012DEA4` |
| `LoadTrigger` | `TRGR` | 0x80070DB4 | 680 | 272 | `fn_80070DB4` |
| `LoadHUDHint` | `HHNT` | 0x802328A4 | 656 | 176 | `fn_802328A4` |
| `LoadBallTrigger` | `BALT` | 0x80118B08 | 648 | 288 | `fn_80118B08` |
| `LoadDock` | `DOCK` | 0x800B63DC | 636 | 144 | `fn_800B63DC` |
| `LoadRumbleEffect` | `RUMB` | 0x80101394 | 636 | 256 | `fn_80101394` |
| `LoadTriggerOrientated` | `TRGO` | 0x801B7F44 | 636 | 288 | `fn_801B7F44` |
| `LoadSilhouette` | `SILH` | 0x80101B48 | 580 | 256 | `fn_80101B48` |
| `LoadCameraPitch` | `CAMP` | 0x801FB1E4 | 576 | 464 | `fn_801FB1E4` |
| `LoadPlayerStateChange` | `PSCH` | 0x8014B294 | 560 | 144 | `fn_8014B294` |
| `LoadCoverPoint` | `COVR` | 0x800EC298 | 552 | 176 | `fn_800EC298` |
| `LoadEnvFxDensityController` | `FXDC` | 0x80101610 | 552 | 192 | `fn_80101610` |
| `LoadActorKeyframe` | `ACKF` | 0x800D5740 | 548 | 128 | `fn_800D5740` |
| `LoadAIWaypoint` | `AIWP` | 0x801E8558 | 544 | 176 | `fn_801E8558` |
| `LoadRepulsor` | `REPL` | 0x802276A8 | 544 | 176 | `fn_802276A8` |
| `LoadFogVolume` | `FOGV` | 0x80101E10 | 540 | 256 | `fn_80101E10` |
| `LoadActorRotate` | `AROT` | 0x80109F88 | 540 | 544 | `fn_80109F88` |
| `LoadPickupGenerator` | `PKGN` | 0x8010C590 | 532 | 144 | `fn_8010C590` |
| `LoadPlayerHint` | `HINT` | 0x8010C018 | 528 | 176 | `fn_8010C018` |
| `LoadAIHint` | `AIHT` | 0x8022A0C0 | 524 | 176 | `fn_8022A0C0` |
| `LoadCameraBlurKeyframe` | `BLUR` | 0x800BD63C | 516 | 128 | `fn_800BD63C` |
| `LoadCounter` | `CNTR` | 0x80092310 | 512 | 112 | `fn_80092310` |
| `LoadRandomRelay` | `RRLY` | 0x800BA234 | 512 | 112 | `fn_800BA234` |
| `LoadDamageActor` | `DMGA` | 0x80100D54 | 504 | 256 | `fn_80100D54` |
| `LoadTimer` | `TIMR` | 0x800866C8 | 500 | 112 | `fn_800866C8` |
| `LoadPointOfInterest` | `POIN` | 0x8010EAE0 | 484 | 176 | `fn_8010EAE0` |
| `LoadControllerAction` | `CNTA` | 0x80149E38 | 476 | 128 | `fn_80149E38` |
| `LoadMidi` | `MIDI` | 0x8015C67C | 476 | 128 | `fn_8015C67C` |
| `LoadWorldLightFader` | `WLIT` | 0x800FF484 | 452 | 144 | `fn_800FF484` |
| `LoadAreaDamage` | `ADMG` | 0x801E2EAC | 444 | 160 | `fn_801E2EAC` |
| `LoadMemoryRelay` | `MRLY` | 0x8017645C | 420 | 112 | `fn_8017645C` |
| `LoadPathMeshCtrl` | `PMCT` | 0x801EB64C | 412 | 160 | `fn_801EB64C` |
| `LoadSwitch` | `SWTC` | 0x8014A2B8 | 404 | 112 | `fn_8014A2B8` |
| `LoadScriptLayerController` | `SLCT` | 0x8022F7C8 | 396 | 128 | `fn_8022F7C8` |
| `LoadCameraShaker` | `CAMS` | 0x800D5128 | 392 | 576 | `fn_800D5128` |
| `LoadAIJumpPoint` | `AJMP` | 0x8014FDFC | 392 | 160 | `fn_8014FDFC` |
| `LoadGrapplePoint` | `GRAP` | 0x800ED18C | 368 | 256 | `fn_800ED18C` |
| `LoadSpiderBallWaypoint` | `BALW` | 0x800E8C8C | 344 | 160 | `fn_800E8C8C` |
| `LoadRelay` | `SRLY` | 0x800B8EFC | 340 | 112 | `fn_800B8EFC` |
| `LoadRipple` | `RIPL` | 0x801173A4 | 324 | 112 | `fn_801173A4` |
| `LoadTimeKeyframe` | `TKEY` | 0x801F9050 | 320 | 112 | `fn_801F9050` |
| `LoadTargetingPoint` | `TGPT` | 0x8012CA24 | 296 | 160 | `fn_8012CA24` |
| `LoadSpiderBallAttractionSurface` | `BALS` | 0x800FED74 | 292 | 160 | `fn_800FED74` |
| `LoadWaypoint` | `WAYP` | 0x80073474 | 288 | 160 | `fn_80073474` |
| `LoadCameraWaypoint` | `CAMW` | 0x800A5474 | 288 | 160 | `fn_800A5474` |
| `LoadAIKeyframe` | `AIKF` | 0x800D5708 | 56 | 16 | `fn_800D5708` |

The six the briefing named - `LoadSpawnPoint` 2636, `LoadPickup` 2072, `LoadHUDMemo` 964,
`LoadAreaProperties` 744, `LoadStreamedAudio` 728, `LoadSequenceTimer` 692 - were already
written before this lane started, in the `CScript*.cpp` units, and are **not** on the link-
gap list. Their current percentages are in `build/report.json`; `LoadSpawnPoint` is 0.15%,
so the "largest of the six" was not the cheapest thing left.

## The rest of the 234-symbol group

`port_link_gap.md` calls the whole group "REL module loaders". It is not: 159 of the 234 had
the `(CStateManager&, CInputStream&, const CEntityInfo&)` signature and are all in this file.
The other 75 are a different family and none of them is a thunk:

| family | count | what it is |
| --- | --- | --- |
| `LoadTypedefSLdr*` | 68 | `void LoadTypedef<T>(T&, CInputStream&)` - one template, 68 instantiations. Pairs with the 136 `SLdr*` constructors in the same group. |
| `LoadEchoParameters` / `LoadActorParameters` / `LoadEditorTransform` | 3 | `void Load*(SLdr*&)` - the non-template half of the same idea |
| `LoadModelData`, `LoadCAABox` | 2 | helpers, different signatures |
| `REL_loader_CannonBall` | 1 | the module's own entry, in `ScriptCannonBall` |
| `SetLoader_CannonBall` | 1 | port-side; retail's is at 0x8021FAB4, already named |

