# Handoff sections moved out on 2026-10-01

Moved verbatim from `docs/HANDOFF.md` when it was cut back to the current position. The figures
are those of the day each paragraph was written, not current; `build/report.json` is the truth.

## How the state-block numbers got here (syncs and flips, 2026-09-28 to 2026-09-30)

The seventh sync (`PrimeDecomp/echoes` a6538995, 2026-09-30, #281-#290) took matched 10462 -> 11132,
linked 5051 -> 5507 and DOL 8914 -> 9584, measured against a build of master's pre-merge head. It brings
`CTweakTargeting`, `CTweakGuiColors`, `CTweakGui` and `CGuiCompoundWidget` as `MatchingFor`, and
`CTweakPlayer`, `CTweakPlayerGun`, `CBomb`, `CScannableObjectInfo`, `CAuiImagePane`, `CAuiMeter`,
`CGuiCamera` and `CGuiLight` as NonMatching. Their ranges absorbed eight of our Matching units, which
were deleted: `Carve800836B0.c`, `Carve80083FD8.c` (in `CBomb`), `Tweaks/CTweakPlayerSuit.cpp`,
`Tweaks/CTweakPlayerAnalog.cpp` (in `CTweakPlayer`), and `Carve8027409C.c`, `Carve80274774.cpp`,
`Carve80274C1C.c`, `Carve80276AD8.c` (in the GuiSys units). The only functions that lost 100% are
`fn_80083FD8`/`fn_80083FDC`, now in NonMatching `CBomb`; the other eleven are renames that are still at 100%
(`SLdrSpline` -> `CMayaSpline`, `fn_800368E4` -> `ReturnFirstIfSingleElseSecond`). Upstream also swapped
`CStateManager::AddWeaponId`/`RemoveWeaponId`, and it is right: 0x80037FC0 calls `fn_800B321C`, which
decrements and erases at zero, so it is the remove. The seven non-Tweak units are EXCLUDED from the port
in `tools/check_files_cmake.py`: listed together they took the link from 250 to 281 undefined.

The sixth sync (`PrimeDecomp/echoes` 7898947, 2026-09-30: `CAuiEnergyBarT01`, NonMatching) took matched
10097 -> 10104 and linked 4918 -> 4917: its unit claims 0x8027E3F8..0x8027EF10, which absorbed the
one-function `Carve8027E404.c` (a Matching carve), so that carve was deleted rather than kept beside it.

Measured 2026-09-28 on the upstream merge (`PrimeDecomp/echoes` f2dcbf4 taken as the base, our work
re-applied on top). Before the merge master stood at 3980 matched / 2557 linked / 322 undefined;
what the merge gained and the 50 master functions it still does not reproduce are in "The upstream
merge, landed" below. Linked then rose 3498 -> 3525 by flipping three units that were already all-100%
(`CGuiFrameFactory`, `CAnimTreeSingleChild`, `CInt32POINode`); see "The flip pre-pass" below.
The second upstream sync (`PrimeDecomp/echoes` c3537e0) then took matched 8099 -> 8640 and linked
3526 -> 3496, then 3497 by flipping `CStaticGeometryMap`; see "The second upstream sync" below for what was traded and where it is queued.
The fourth sync (`PrimeDecomp/echoes` 750bdca, 2026-09-29: particle-element names from the Remaster
and `CEmitterElement`/`CIntElement` matches) took matched 9117 -> 9120 with linked unchanged and
no function regressing; the other 13 of its 16 "+100%" rows are renames of already-matched functions.
Then 9120 -> 9121 by taking `rstl::vector::clear` back out of line (`include/rstl/vector.hpp`):
upstream's b0934a1 made it `inline`, which dropped `CMoviePlayer::Rewind` to 78% here - retail calls
`clear` out of line (0x80317F98). Then 9188 -> 9189 matched and 4014 -> 4047 linked by flipping
`MetroidPrime/BodyState/CBodyStateCmdMgr` (2026-09-29): an `inline_max_size(127)` pragma, three retail
vtables named in `symbols.txt` so MWLD drops our weak copies, and one stray `.sdata` byte claimed; see
`docs/RUNNING_THE_DECOMP.md`, "An unnamed retail vtable keeps our weak copy alive". Upstream's `CColor(const float, ...)` stays although it scores
`CScriptForgottenObject::RenderInternal` 95.18 -> 88.19: its instructions and relocations are
identical to retail and the REL still hashes, while reverting it costs five Tweaks ctors at 100%.
Then 9188 -> 9189 and linked 4014 -> 4059 by taking `Kyoto/Particles/CColorElement` to
`Matching` 45 / 45; its last short function was the `CCEKEYF::GetValue` the flip pre-pass below
predicted would need different work from its three siblings, and it did - see the row in
`RUNNING_THE_DECOMP.md`. That leaves `CIntElement` as the only one of the four particle elements
still queued.
Then linked 4112 -> 4276, matched unchanged, by flipping seven units that already had every function
at 100% (2026-09-29): `CIOWinManager`, `CWorldLayerState`, `CGameHintInfo`, `CAnimTreeSequence`,
`DolphinCColor`, `DolphinCDvdFile`, `CSfxHandle`. Four of them were template-pool emission order, which
is per-TU fixable after all - see `RUNNING_THE_DECOMP.md`, "An emission-order wall", the superseding note.
Then 4276 -> 4293 by flipping `CPASDatabase` the same way, plus a non-inline forward declaration of
`rstl::destroy_impl(T*)` so its out-of-line copies are weak and deduplicated as retail's were.
Then 4293 -> 4318 by flipping `CFontRenderState` with both techniques.
Then 4318 -> 4342 by flipping `CTextRenderBuffer`, whose only blocker was `align:16` missing on the
next unit's (`CCubeMoviePlayer`) `.rodata` split.
Then 4342 -> 4346 by flipping `CAnimTreeNode`, whose placement-new string had been left unclaimed.
Then 4346 -> 4370: `CStaticAudioPlayer` flipped once `DecodeMonoAndMix` matched (a declaration reorder) and its
template instantiations were placed with the two emission-order techniques. `CStateMachineFactory`
cannot flip alone: its string pool is shared with `Enemies/CStateMachine`, so the two were one TU.
Then 4370 -> 4374 by flipping `CSoundPOINode`: its vtable sat in an unclaimed `.data` blob, so our
strong copy was multiply defined until the split claimed `.data 0x803BBB58..0x803BBB68`.
Then 4374 -> 4446 by flipping `CIntElement`: declaring `CIEParticleCreationTime::GetValue` before
its destructor made `GetValue` the key function, which moved the vtable to where retail has it.
`CParticleGen` is parked: see `RUNNING_THE_DECOMP.md`, the vtable notes near the string-pool paragraph.
Then 4446 -> 4450 by flipping `CABSIdle`: 13 retail weak copies it duplicates were named in
`symbols.txt` so MWLD drops ours. `CTweakAutoMapper` cannot flip: its jumptable is 4-aligned
`.data` (`0x803B822C`), the toolchain limit in "A switch jumptable forces the unit to own the vtable".
Then 4450 -> 4454 by flipping `CFluidPlane` the same way: 4 weak copies (`optional_object<TLockedToken<CTexture>>`
assign, `CFluidUVMotion` copy ctor, its `reserved_vector` copy ctor, `uninitialized_copy_n`) named in `symbols.txt`.
Then 4454 -> 4497 by flipping `CCollisionResponseData`: one weak copy named (`0x800DAC9C`), the three
`vector::assign`s defined in the unit before the constructor, and inline `clear` specialisations (the emission-order recipe).
The fifth sync (`PrimeDecomp/echoes` 03bd14b, 2026-09-30: 15 commits, scaffolds for CAuxWeapon, the
GunController states, CCollisionActor, four ScriptObjects, CIceImpact and CConsoleOutputWindow, plus
matched CFont, CCollisionPrimitive and CCollisionInfo) took matched 9497 -> 9649 and linked 4818 -> 4859,
with no function that was at 100% before dropping. Upstream named four `CAuxWeapon` bodies and
`CConsoleOutputWindow`'s globals that our `Matching` carves referenced as `fn_`/`lbl_`, so the carves now
spell the new mangled names; `rstl::vector<float>::reserve` cannot be spelled as a C identifier, so
`CConsoleOutputWindowCtor.cpp` declares the specialisation instead (see `RUNNING_THE_DECOMP.md`). The port
takes 10 of the 21 new units, chosen by the linker (the block at the end of `files.cmake`); port link
254 -> 250 undefined.

## Two independent workstreams, and where each stands

**1. The DOL** - 8028 of 16726 functions (2026-09-29, after `CStateManager`'s `AreaLoaded`,
`AreaUnloaded`, `RayCollideWorld` and `UpdateActorInSortedLists`; the figure
includes the SDK). Verified matches land here steadily, and the two units the whole port was
waiting on are in: `CAi` 11/11 `Matching`; `CPatterned` 29/103 is `NonMatching` since the upstream
merge widened it. Others, measured after the second upstream sync (2026-09-28): `TypesMatch` 503/511,
`CStateManager` 92/239, `CPlayerGun` 68/136, `CPlayerState` 67/72 - see "The second upstream sync"
(the sync's +8 in `CStateManager` from 0x168C up is fixed: `mMapWorldInfo` belongs at 0x167C).
(Those three fell on 2026-09-26 when lane f1 made `rstl::rc_ptr` retail's 8-byte width - all
three are `NonMatching`, so none of them is in the binary and the DOL's sha1 did not move. See
`docs/research/rc_ptr.md`.) `CStateManager`, `CPlayerGun` and `CPlayerState` are back up to
72/239, 61/135 and 69/72 on 2026-09-26: `CStateManager`'s `pad2_2` was `0x34` where retail has
`0x2C`, which pushed every member from `x1684` up by 8 and put `m_isDarkWorld` at 0x2954 instead
of 0x294C. Fixing the one pad took 17 functions to 100% in a single edit and broke none; see
`RUNNING_THE_DECOMP.md`, "A header comment that recorded mwcceppc's own output".
The reachable pools are thinning; what remains is dominated by FPU register allocation,
instruction scheduling, string-pool offsets, and weak rstl instantiations whose callers are
not decompiled. All of those are documented in `RUNNING_THE_DECOMP.md` - check it before
spending a session rediscovering one.

**`TypesMatch` was gated on naming, and that is now measured and paid off** (2026-09-25).
508 of 511 functions are exact: the first 94 (`30` `TypesMatch` overrides plus `64` `TCastToPtr`
casts) were landed under the placeholder class names `CUnknown<id>`, because every naming source
in the tree - the previous versions' configs, `symbols.txt`, the DOL's strings, the RELs - was
exhausted and none of them names those 32 classes. The *shape* is not a guess: each class's parent
is the class whose `::TypesMatch` the retail override calls, and the addresses come from the vtable
holding each id. `docs/research/TypesMatch_unnamed_ids.txt` is that table;
`docs/research/rename_typesmatch_ids.py` regenerates the block from it, so identifying a class later
is one line and a re-run, and it is now known what three of those classes are: id 76 has no members,
id 63 is `optional_object<TCachedToken<T>>` at 0x158, id 50 is an empty `CScriptDamageableTrigger`
subclass. **Three functions remain**, all characterised: `fn_8009D3D8` (93.27%) and `fn_8009D45C`
(79.70%) need a stack home and an outgoing-arg copy retail has and no source shape produced, and
`fn_80097520` is a 32-byte MWCC thunk to an unnamed function that nothing references.

**2. The REL modules** - 662 of 11739 functions (2026-09-28), 86 modules. That count is low partly because
claiming a range for a unit *removes* those bytes from the `auto_*` units that match for free -
see "why the matched total can go down" in `RUNNING_THE_DECOMP.md`. **The recipe works and is written
up**: a module may be partly decompiled, with the `Matching` unit claiming only the ranges its
own object reproduces and everything else unclaimed so `dtk` fills it from retail.

**Measure this, never recall it**: `python3 tools/check_module_wiring.py`. As of the last commit it
reports **99 units of our own code in 77 modules** - `AIMannedTurret`, `AtomicAlpha`, `AtomicBeta`, `BacteriaSwarm`, `Blogg`, `ChozoGhost`, `DarkCommando`, `DarkSamus`, `DarkSamusBattleStage`, `DarkTrooper`, `DestructibleBarrier`, `DigitalGuardian`, `ElitePirate`, `EmperorIngStage1`, `EmperorIngStage2Tentacle`, `EmperorIngStage3`, `EyeBall`, `FishCloud`, `FlyerSwarm`, `FlyingPirate`, `FogOverlay`, `GeomBlobV2`, `Glowbug`, `Grenchler`, `GunTurret`, `Ing`, `IngBlobSwarm`, `IngBoostBallGuardian`, `IngPuddle`, `IngSnatchingSwarm`, `IngSpaceJumpGuardian`, `IngSpiderballGuardian`, `Kralee`, `Krocuss`, `MediumIng`, `Metaree`, `MetareeSwarm`, `Metroid`, `MinorIng`, `MysteryFlyer`, `OctapedeSegment`, `Parasite`, `PillBug`, `PirateRagDoll`, `PlantScarabSwarm`, `PuddleSpore`, `Puffer`, `Rezbit`, `Ripper`, `RubiksPuzzle`, `SandBoss`, `Sandworm`, `ScriptCoin`, `ScriptFrontEndDataNetwork`, `ScriptGui`, `ScriptPlayerActor`, `ScriptPlayerProxy`, `ScriptPlayerTurret`, `ScriptRiftPortal`, `ScriptRsfAudio`, `ScriptSafeZone`, `ScriptStreamedMovie`, `Shredder`, `SnakeWeedSwarm`, `SpacePirate`, `SpankWeed`, `Splinter`, `Splitter`, `Sporb`, `StoneToad`, `SwampBossStage1`, `SwampBossStage2`, `SwarmBasics`, `Tryclops`, `WallCrawler`, `WallWalker`, `WispTentacle`. `Tweaks` left the list in the third upstream sync (2026-09-29): upstream rewrote both its units (`Tweaks/Tweaks.cpp` and the generated `ScriptLoader/Tweaks.cpp`) and marks them `NonMatching`, and we took that as-is; the module still hashes because `dtk` fills it from retail. `Rezbit` joined on 2026-09-29 with its head at `.text 0x0..0x168` (17 functions); this 73-in-57 figure is superseded by that. `Parasite` (`.text 0x0..0x148`, 17 functions) and `ElitePirate` (`.text 0x0..0x178`, 19 functions) joined the same day, rescued from the goal loop's review queue (see `RUNNING_THE_DECOMP.md`'s rows for them); 77 in 61 is superseded by that. `Splitter` joined after them, also rescued, as **two** units - its head `.text 0x0..0xFC` (15 functions) and, because RELExit/RELMain sit at 0x8210/0x8234 rather than next to the accessors, a separate `CSplitterRelMain.cpp` at `.text 0x81F8..0x82B0` (6); 79 in 63 is superseded by that.
`Metroid` joined 2026-09-29 with its module head, `.text 0x0..0x17C`, eighteen functions - the only
head in this family whose loader record is **0x10 bytes rather than four**, because the DOL's
`OnDockTouch__13CMetroidAlpha` reads words 4..15 of it as a CodeWarrior pointer-to-member-function;
see the `Metroid` row of the Attempted modules table in `RUNNING_THE_DECOMP.md`.
`MetareeSwarm` joined on 2026-09-29 with its module head, `.text 0x0..0xD8`, five functions - see
"`CMetareeSwarmRel` is the module head, and `>> 7` is a 25-bit rotate" in `RUNNING_THE_DECOMP.md`.
`IngPuddle` joined the same day, the same way: its module head, `.text 0x0..0xA8`, five functions -
see "`CIngPuddleRel` is a module head, and a vtable call needs a class" in `RUNNING_THE_DECOMP.md`.
`IngSnatchingSwarm` joined 2026-09-29 as the **second module whose head is IngPuddle's
instruction for instruction**: same 0xA8 bytes, same two vtable accessors at vtable offsets 0x38
and 0x3C, same four-byte `.bss` loader slot, and only the two `bl` targets and the `addi r3,r3,0x1F4`
differ. See "`CIngSnatchingSwarmRel` is `CIngPuddleRel` with the loader import spelled out" in
`RUNNING_THE_DECOMP.md`.
`PlantScarabSwarm` joined the same day too, and its head is **the same 0xD8 bytes as
`MetareeSwarm`'s instruction for instruction** - the same 0xB8-byte record, the same three floats
at +0x0C/+0x1C/+0x2C, the same flag byte at +0xB2, and only the two `bl` targets differ because
each module registers its own loader. That is a fact about the two modules, not a copy of a
source file, and it is why `CPlantScarabSwarmRel.cpp` is `CMetareeSwarmRel.cpp` with the module
number changed.
`AtomicAlpha` joined on 2026-09-29 and is the first of these heads to be **larger than the loader
trio**: its `.text 0x0..0x13C` is 18 functions, because the fourteen-accessor block the REL loader
generator emits at the head of a scripted-actor module comes *before* the trio here. Twelve of those
fourteen accessors are the same bodies `AtomicBetaAccessors.cpp` already reproduces at 100% (same
three DOL relocations - `lbl_8041AAB8`, `kInvalidUniqueId`, `lbl_8041B758`); the other two are
AtomicAlpha's *leading* pair, extra, naming +0x8C8 and +0x7D8 where AtomicBeta opens with the float
store. So the block is **not** byte for byte identical to AtomicBeta's - twelve of fourteen is the
measured number - but no spelling had to be discovered. See "`CAtomicAlphaRel` is a module head, and
twelve of its fourteen accessors are shared" in `RUNNING_THE_DECOMP.md`.
`SnakeWeedSwarm` joined on 2026-09-29 as well, with a head that is *not* shaped like the other
three: `.text 0x0..0xDC`, four functions, and its registration fills a **0x1C-byte** record - an
`FScriptLoader` and two CodeWarrior pointer-to-member-functions, which are 12 bytes each because
`__ptmf_scall` reads three words. The two member-function pointers are copied out of `.data`
verbatim rather than assigned, which is what the six loads and the `stwu` in `fn_71_70` are. See
"`CSnakeWeedSwarmRel` is a module head, and a pmf is 12 bytes" in `RUNNING_THE_DECOMP.md`.
`FishCloud` joined on 2026-09-29 too, and it is the **cheapest of the eight heads**: `.text 0x0..0xAC`,
four functions, and **nothing had to be discovered at all** - no new spelling, no new stand-in class,
and **no locally spelled struct**, because `SFishCloud_FuncPtrs` is already in `ScriptLoaderRel.hpp`.
Its `fn_20_0` is not merely the same CActor `GetHealthInfo` vtable entry SnakeWeedSwarm's `fn_71_0`
is: `diff` of the two disassembly listings is **empty** over all eleven instructions, so the two heads
open with byte-for-byte identical code, and FishCloud stores it in **both** of its vtables
(`lbl_20_data_8` and `lbl_20_data_84`, 0x7C bytes each) so the link cannot dead-strip it. Its
registration fills an **8-byte** record, two `FScriptLoader`s, and it is the only one of these heads
with no member-function pointer in it at all. See "`CFishCloudRel` is a module head, and the header
already had the record" in `RUNNING_THE_DECOMP.md`.
`Tryclops` joined on 2026-09-29 with its module head, `.text 0x0..0x178`, nineteen functions. The
lane landed 0x4C..0x178 believing `fn_81_10`'s `optional_object<CAABox>` return blocked the three
functions below it; it is the MysteryFlyer `fn_45_10` shape (the converting ctor is called out of
line, at 0x4FEC), so the claim was taken to 0x0 when the change was rescued. Its thirteen-accessor
block is AtomicAlpha's (both 0x8C bytes, 35 instructions, identical multiset). See
"`CTryclopsRel` is sixteen functions, and a REL unit's 100% is not the module's verdict" in
`RUNNING_THE_DECOMP.md`.
`IngBlobSwarm` joined on 2026-09-29 with the module head only (5 functions in one `Matching`
unit claiming `.text 0x0..0xD8`); its 53 remaining class functions need the
`CIngBlobSwarm`/`CActor`/`CPatterned` hierarchy and are still retail.
`PillBug` joined on 2026-09-29 with its module head, `.text 0x0..0x130`, seventeen functions: the
loader generator's thirteen-accessor block, `fn_48_90` (a call through CAi's vtable slot 0x38, via a
thirteen-virtual stand-in), and the loader trio. It was rescued from the review queue - the lane's
code was right and only the raw-offsets gate failed, for want of a `raw_offsets.md` section. See
"`CPillBugRel` is a module head, and the accessor block is already in the DOL" in
`RUNNING_THE_DECOMP.md`.
`Blogg` joined on 2026-09-29 with its module head, `.text 0x94..0x108`, three functions: `RELExit`,
`RELMain` and the loader registration `fn_7_D8`. It has no accessor block - `fn_7_0` (0x0, 0x94) is
a `CDamageVulnerability` destructor and stays retail - so the claim starts at 0x94. Rescued from the
review queue: the code was right, and the lane's last attempt failed only because the asm guard in
`tools/goal_check.sh` matched a comment citing `build/G2ME01/asm/...` (fixed on master, 6c2d8d7).
`GeomBlobV2` joined on 2026-09-29 and is the **first head that is not at 0x0 and not the
thirteen-accessor family**: its entry-point block is `.text 0x23E8..0x2490` (four functions -
`fn_25_23E8`, `RELExit`, `RELMain`, the registration `fn_25_2460`) because `fn_25_0` (0x0, 0x1FC) is
a real bone-blend loop, and its accessor block is a **different, much simpler set** - two pointer
getters at `+0x15C` and float accessors at `+0x190` / `+0x198`, naming no DOL global at all. It is
also the first head whose accessor block needed **two** units (`CGeomBlobV2Accessors.cpp` at
`0x2544..0x255C` and `CGeomBlobV2AccessorsTail.cpp` at `0x256C..0x2584`, six accessors with the two
float getters at `0x255C..0x256C` left retail), because two of its eight accessors are not in dtk's
FORCEACTIVE list and are dead-stripped if a unit claims them. See
"`CGeomBlobV2`'s accessor block is six functions in two units, and `unit_fit.sh` said it fit" in
`RUNNING_THE_DECOMP.md`.

`DarkTrooper` joined on 2026-09-29 (goal item `progress-rel-head-darktrooper`) with its module head,
`.text 0x0..0x12C`, **sixteen functions**. Its
thirteen-function block is **PillBug's, re-ordered** and measured as such: this one opens with an
8-byte member-address accessor, moves PillBug's 0x10-byte float store to 0x08, runs one
`li r3,0; blr` predicate in the run where PillBug runs four (3 against 6 over the whole block), and
carries one function PillBug has not -
`fn_12_38`, the `+0x34C` flag bit, which transfers verbatim from AtomicBeta's `fn_5_48` over the
same byte - the same three instructions, `88 03 03 4C / 54 03 EF FE / 4E 80 00 20`. See
"`CDarkTrooperRel` is a module head, and the accessor block is a sibling's in another order" in
`RUNNING_THE_DECOMP.md`.
`Puffer` joined by being promoted rather than restored: with
its two units `Matching` the mutation check (change one byte of our source, the module hash must
break) proves our object really is in the link. The list this paragraph used to carry was wrong in both
directions and is exactly the kind of claim that must not be written from memory:

- `Puffer`, `WallCrawler` and `ScriptGui` had **lost their `Rel(...)` blocks** to later commits that
  copied an older `configure.py` (`33b73a3` replaced Puffer's block with WallCrawler's own; `f599488`
  dropped WallCrawler's and ScriptGui's). Their sources had been sitting in `src/` compiled by
  nothing, which is why the report showed those units at 0.00%. Restoring the blocks and the
  `Matching` states they had is worth **30 matched functions**, and `check_module_wiring.py` exists so
  the next one is caught the same day.
- `AIMannedTurret` was called "the working example" - it is not. Its unit is at 3/3 in the report, but
  promoting it to `Matching` **breaks the module's hash** (85/86), so its code is not in the link and
  never was. It stays `NonMatching`.
- `Puffer`'s units were `NonMatching` even in the commit that landed them, so its earlier "sha1
  verified" claim proved nothing - the vacuous-verification trap the rules warn about.

**The creature base classes now exist** (`CAi`, `CPatterned`), so the 75 creature and swarm modules
are no longer blocked on the hierarchy existing - only on their own code, and on `CPatterned`'s
0xB58-byte constructor if they need it.

## Keeping this documentation true

These three files are load-bearing: a session that trusts a stale handoff wastes its whole
budget re-deriving what the last one knew. So updating them is **part of finishing a piece of
work**, not a separate chore. The rule is short:

**If you changed the answer to a question one of these files answers, update that file in the
same commit as the change.**

**A claim that can be derived must be derived, and that now has a checker.**
`python3 tools/check_docs_claims.py` verifies the numbers this file and `RUNNING_THE_DECOMP.md` quote -
the state block, the per-unit counts in the prose, the list of modules that link our code, and the
pinned hashes - against `build/report.json` and `tools/check_module_wiring.py`. Run it before
committing anything that moves a number, and after any config merge. It exists because the rule below
was in place for a whole session while a paragraph still listed sixteen modules linking our code when
three of them had no `Rel(...)` block at all: the state block was current and the prose was not, and
prose is where the reader forms their plan. Two more stale per-unit counts were found the moment the
checker was first run.

Concretely, after any turn that changes the position or the method:

| what changed | where it goes |
| matched counts, which units/modules are done | the state block at the top of this file |
| a unit or module is now `Matching` and verified | the state block, and the module table in `RUNNING_THE_DECOMP.md` |
| a new blocker found, or an old one cleared | the blocker section here, and the relevant one in `RUNNING_THE_DECOMP.md` |
| a technique that worked, or a pattern that cannot work | `RUNNING_THE_DECOMP.md` (recipe, known-hard, gates) |
| a module attempted, whatever the outcome | the "Attempted modules" table at the end of `RUNNING_THE_DECOMP.md` |
| how the port itself works | `PORT_NOTES.md` |
| a lane-collection or lane-spawning lesson | the "Parallel lanes" section of `RUNNING_THE_DECOMP.md` |

Rules for the writing itself, learned by getting it wrong:

- **Measure numbers, do not recall them.** Every wrong figure found in these files was written
  from memory. Run `./tools/decomp_build.sh` and the config.yml hash check and quote what they
  print. If a number in a doc disagrees with `build/report.json`, the report wins.
- **Annotate stale checkpoints, do not silently rewrite history.** If a figure was right at the
  time it was written, leave it and mark it as a checkpoint pointing at the current source of
  truth. `PORT_NOTES.md` line ~340 does this.
- **Say what is not known.** "CPatterned's constructor was not attempted" is worth more than a
  confident-sounding guess, and it is what stops the next session repeating the work.
- **A negative result belongs in the docs.** The CAi link-order cycle, the assembly dead end,
  the vacuous `NonMatching` verification - each cost a session, and each is now one paragraph
  that saves the next one.
- **Do not let a doc outlive its claim.** If a statement is superseded, correct it in place and
  note that it was superseded; an earlier version of `RUNNING_THE_DECOMP.md` concluded that no
  module could be decompiled, which was wrong two sessions later.

There is a repo `AGENTS.md` that repeats the short version of this, so an agent that never
opens this file still sees it.

## The frame-time pair is written; the loop's stop is now `fn_80049244` (2026-09-28, goal item `port-boot-frame0-fn80049244`)

`fn_80006954` (0x80006954, 0x58) and `fn_80008B60` (0x80008B60, 0xC8) both have bodies, in
`src/MetroidPrime/PortFrameTimeHistory.c` - port-only, in `files.cmake`, claiming nothing in the
DOL, because 0x80006954 is inside `MetroidPrime/main.cpp`'s `.text` claim and 0x80008B60 inside
`MetroidPrime/mainTail.cpp`'s, so a carve is two other lanes' cuts. Both `PORT_FRAME_STOP`s in
`CMain::RsMain` are replaced by retail's call on retail's line, and the two 8-byte stack locals
become `SFrameTimeTotal`, stored to `CMain`+0x40 and `CMain`+0x44 as retail does.
`fn_80008B60` is 50 instructions in 200 bytes against retail's 50 in 200, differing only in
relocations; `fn_80006954` is 22 in 88 against retail's 22 in 88, differing in the `bl` displacement
and in the order of the two stores after the call.

**It is a mean, not a sum, and 0x8041A428 is not 200.0.** That constant is
`43300000 80000000` = 2^52 + 2^31, mwcc's integer-to-double bias, and the `fsubs` cancels it and
leaves `count`; the body is `sum * (1.0f / count)`. Written with a `- 200.0f` it is 58 instructions
in 232 bytes, because mwcceppc then emits *two* subtractions of 200 - one against the double
constant and one against the float. So this correction is about **the value being a mean where
earlier passages said sum**, and each of those is annotated in place where it stands:
`docs/RUNNING_THE_DECOMP.md:3237` and `:3241` (the layout table row and the "sums, not a running
minimum" sentence), `docs/RUNNING_THE_DECOMP.md:4117`, `docs/research/boot_path.md:140`, and this
file's own `:2679` and `:2699` from the 2026-09-27 pass - all superseded, none of them about
0x8041A428.

Verified, `./tools/goal_check.sh build/goal/item.json` -> `goal_check: PASS
port-boot-frame0-fn80049244`: `gate.sh` (DOL sha1, 86 RELs, report diff, wiring, docs claims, port
probe), `matched 3980 -> 3980 linked 2557 -> 2557`, `All: 8.52% fuzzy, 7.54% matched, 5.32% linked
(3980 / 28465 functions)`, `2 path(s) changed under src/ or include/`, `port undefined 317 -> 317`,
`probe: (then 659 source) files, 0 failed, 0 errors; link: LINKED (317 undefined, 0 duplicates)`,
`verify boot-progress.sh: BOOT_PROGRESS PASS: all 2 runs got further than all 2 head runs`. The port
probe's file count moved **658 -> 659**, so every "N files" claim in these docs moved with it; the
four historical transcripts keep their own figure, reworded.

**Next wall, and it is the item's own target - a crash, not a declared stop.** `fn_80049244`
(0x80049244, 0x118) SIGSEGVs on frame 1 at `src/MetroidPrime/Carve80049244.cpp:143`
(`win->PreDraw()`), called from `CMain::RsMain` at `PortBoot.cpp:459`. The draw list holds four
IOWins; the two whose constructors are not written - `CConsoleOutputWindow` and `CAudioStateWin` -
have garbage vtable pointers, measured in gdb (node1 vptr `0x555002f239b4`, node3
`0x555002f239e4`, against real vtables `0x5555559bb360` and `0x555556c7b290` for
`CErrorOutputWindow` and `CMainFlow`). Adding the two `configure.py` units to `files.cmake` was
measured and **rejected**: the port's undefined count goes 317 -> 327, and it would not fix the
fault, because both bodies store a **retail PowerPC vtable address** (`lbl_803B37F0`,
`lbl_803B3950`) as the object's vptr. Full evidence in
`build/goal/notes/port-boot-frame0-fn80049244.md`.

## Index of `handoff-to-2026-09-29.md`


Earlier session narratives are in `docs/history/handoff-to-2026-09-29.md`, verbatim, one section per heading below. Their figures are not current.

- The upstream merge, landed (2026-09-28)
- The flip pre-pass: +27 linked with no agent, and the queue rebuilt around near-misses (2026-09-28)
- The second upstream sync (2026-09-28, `upstream/main` c3537e0)
- Where the port is: step 17, and the three functions in front of it
- The carve vein is the cheapest `Matching` in the tree, and it has four traps
- If you are picking this up (2026-09-25, end of session)
- The port links now, as far as it can — measured 2026-09-25
- The pak chain: `CResLoader` is typed, the pump is linked, and `CPakFile` is the wall
- What the six collected lanes added, 2026-09-26 evening
- A latent port link bug that only the shipping configuration could see
- The boot probe now heals its own link, and the stop message tells the truth
- PROVEN: `CErrorOutputWindow` is blocked by the compiler *version*, and retail's own binary proves it
- The named blocking list moved: `InitializeSubsystems` 72.36% -> 97.76%, `FillInAssetIDs` 100%
- PROVEN: `ResetGameState`'s loop is not what blocks it — the object is 4 bytes too big
- SOLVED: the second compiler is already installed, and it is GC/3.0a3 or later
- PROVEN: `CMain::ResetGameState` is blocked, by a chain with a named cause at each link
- `COsContext`'s two words were named for each other's contents
- SUPERSEDED: a second compiler version does **not** unblock `CErrorOutputWindow`
- The `mainTail.cpp` split WORKED - which is the unlock for `CMain::RsMain`
- Step 17 RUNS. The stop was a stale guard, and its own comment said so
- `CMain::RsMain`: the split works, and it is still not worth taking yet
- `AllocateRenderer` is a proven structural wall, and row 21c was wrong about what the pixels are
- The ladder now CALLS step 12, and the boot dies inside it at a named point
- Boot steps 18, 19 and 20 are now in the ladder, and adding them cost zero link symbols
- A first frame IS reachable, and the reason is that Aurora's render-target model is retail's
- The fault moved into real code. The next blocker is GX init, not a missing symbol
- The boot's crash chain, measured - and a lane's inference about it was wrong
- Two adjacent carves, two Matching units, and my brief's claim about renames was wrong
- `sizeof(CCubeRenderer)` is 1376 and the header says 860 - 516 bytes short
- The allocator crash is fixed, and the boot is 38 reach-stubs deeper
- The boot now stops for want of *data*, and the rbtree root was red
- Where the boot stops now, and it is not a code defect
- PROVEN, and it is the answer to "what would unblock a frame": the game's assets are not on this machine
- Session end state, and an honest account of what is reviewed and what is not
- CORRECTION: the port gap in the committed tree is 321, not 313
- CORRECTION: the port gap in the committed tree is 321, not 313
- The next wall, measured: Aurora's `ARInit` faults, and it is not a decompilation problem
- The ARAM wall is cleared, and it was a data symbol stubbed as a function
- `CMain::AsyncIdle`'s last instruction: a proven compiler wall, and my brief was wrong twice
- Two more lanes in, and one carve that is free but could not be wired
- The renderer work measured +2 matched and +2 linked, and I could not land it safely
- Three lanes collected: `matched` 3977, `linked` 2555, port 309 undefined / 0 duplicates
- `sizeof(CMain)` was wrong by 4 bytes, and retail says the right number itself
- The renderer work LANDED: `matched` 3979, `linked` 2556, and the vtable is real
- The constructor now completes with no paks - and the vtable costs ~70 symbols to link
- The port links: 0 undefined, 0 duplicates, and `CMain::RsMain` is on the stack
- `CCubeRenderer::EndScene` is `Matching` 100.00%, and `linked` rose to 2557
- The probe linked nothing and said "0 failed" - and the fix is a REGRESSION gate, not an absolute one
- `dol_read.py` decoded `.data` as little-endian, and the damage is provably zero
- `"DUMB_SnowForces"` was never a pool token, and the next wall needs a real pak
- 7 of 8 paks now load from the real ISO - and my `__fstLoad` diagnosis was wrong in its premise
- `tools/link_closure.sh`: both my premises were false, and the six waves were my own behaviour
- Upstream `PrimeDecomp/echoes` exists, is the same project, and is AHEAD. Reconciliation has begun.
- The upstream merge: what I measured, what I got wrong, and where it stands
- `AddPaksAndFactories` block 7 IS the pump - and the byte-order wall is now the only thing left
- The boot reaches retail's frame loop: two `src/` fixes (2026-09-27, goal item `port-boot-cpakfile-sresinfo-getsize`)
- `LoadTypedefEditorProperties` is defined for real, so one reach stub came out (2026-09-28, goal item `port-loadtypedefeditorprops`)
- Both `LdrToEntityInfo` symbols are defined, so two reach stubs came out (2026-09-28, goal item `port-ldrtoentityinfo`)
- `CModelData::~CModelData()` is defined, so its reach stub came out (2026-09-28, goal item `port-modeldata-dtor`)
- The module manager's update closure has a host body, so the frame loop moved to `fn_8030172C` (2026-09-28)
- The frame loop's DMA cleanup is written, so its stop moved to `fn_80006954` (2026-09-28, goal item `port-boot-cmain-rsmain-0eb92a1`)
