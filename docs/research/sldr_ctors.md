# The 136 `SLdr*` script-loader struct constructors and destructors

Measured 2026-09-25 in a worktree at `a1d3702`. **The premise this document was written to test
was wrong, and the correction is the point of it.**

`docs/research/port_link_gap.md` claimed of this group: *"**one generator.** The
`SLdrTweak*`/`SLdr*` structs' default constructors and destructors; retail's are all trivial."* The
brief that commissioned the work said the same thing in a different words: *"In retail these are
almost certainly trivial - a POD struct's default constructor and its destructor. That is a
hypothesis, and your first job is to test it."* Tested: **not one of the 136 is a no-op.** The
smallest constructor is 12 bytes and it stores `-1`; the largest is 9,716
(`__ct__23SLdrTweakGuiColors_MiscFv`). Nothing here is one generator's output.

There is a second, larger correction, and it is the one that decides where the definitions go.

## Retail does not define these symbols. It defines different ones

Not one of the 136 appears in `config/G2ME01/symbols.txt` or in any
`config/G2ME01/rels/*/symbols.txt`. The port's names come from GCC's Itanium ABI; retail's come
from MWCC, which spells the *implicit* constructor and destructor of a class with a private
convention of its own:

| | constructor | destructor |
| --- | --- | --- |
| port (GCC, Itanium ABI) | `_ZN17SLdrTweakGui_MiscC1Ev` | `_ZN17SLdrTweakGui_MiscD1Ev` |
| retail (MWCC) | `__ct__17SLdrTweakGui_MiscFv` | `__dt__17SLdrTweakGui_MiscFv` |

The two toolchains generate the same *requirement* and neither can satisfy the other's *name*. So
there is **no retail range for a `Matching` unit to claim**: no `config/G2ME01/splits.txt` block,
no `Object(Matching, ...)` line, nothing for `tools/flip_test.sh` to test. Every instruction in the
lane brief about claiming a range and grouping by header applies to a mechanism that cannot exist
for this group.

This is verifiable in one command, and it is the first thing to check for any other group in
`port_link_gap_list.md`:

```sh
grep -rn 'SLdr.*C1Ev\|SLdr.*D1Ev' config/G2ME01/     # no output
grep -rn '__ct__17SLdrTweakGui_MiscFv' config/G2ME01/rels/Tweaks/symbols.txt
```

Where the retail bodies are, for reading them: of the 68 classes, 56 are in the `Tweaks` REL module
(112 of the 121 functions that have a counterpart at all), 5 are in the DOL (8 functions), and 7
have no counterpart under any name. The `Tweaks` ones are in
`build/G2ME01/Tweaks/obj/auto_00_00006EC4_text.o`, and that object's addresses are the module's
minus `0x6EC4` (`__ct__17SLdrTweakGui_MiscFv` is `0xA774` in `symbols.txt` and `0x38B0` in the
object). Getting that wrong is easy and produces an empty disassembly rather than an error.

`build/G2ME01/src/MetroidPrime/ScriptLoader/SLdrTweakGui.o` shows the decompilation build's side of
the same fact: it carries `U __ct__17SLdrTweakGui_MiscFv` and `U __dt__17SLdrTweakGui_MiscFv`,
because the generated header *declares* `SLdrTweakGui_Misc()` and `~SLdrTweakGui_Misc()` and MWCC
honours the declaration. The definitions themselves live in the `Tweaks` module, so the DOL-side
reference resolves and retail links. Ours does not, because nothing in the port defines them.

## Shape distribution

Per function, over the 136 (`C1Ev` and `D1Ev` pairs for 68 classes, plus
`SLdrSpline::operator=`):

| shape | ctors | dtors |
| --- | --: | --: |
| calls out (constructs/destroys members, then also sets defaults) | 30 | 60 |
| member init from immediates (`li` + `stw`, no call) | 14 | - |
| member init from `.rodata` (`lis`/`lfs`/`lwz` off an SDA pair, no call) | 16 | - |
| **no-op (prologue/epilogue only)** | **0** | **0** |
| no retail counterpart under that name | 7 | 8 |
| not in the gap list | 1 | 67 |

`SLdrSpline` is the one class in the list that is not a ctor/dtor pair: the port needs only
`~SLdrSpline()` and `operator=`, because `SLdrSpline::SLdrSpline()` is already defined in
`src/Kyoto/Math/CMayaSpline.cpp`, a `Matching` unit.

### What the shapes actually are

**`member init from immediates`** is the cheapest real thing in the group and it is completely
readable. `__ct__25SLdrTweakGui_MovieVolumesFv` (Tweaks `0x452C`, 40 bytes) is nine instructions:

```
452c:  li    r0,127          4540:  stw   r0,0x10(r3)
4530:  stw   r0,0x0(r3)      4544:  stw   r0,0x14(r3)
4534:  stw   r0,0x4(r3)      4548:  stw   r0,0x18(r3)
4538:  stw   r0,0x8(r3)      454c:  stw   r0,0x1c(r3)
453c:  stw   r0,0xc(r3)      4550:  blr
```

Eight ints, all 127 - the header declares exactly eight ints, so the member order is unambiguous.
The two `SLdrTweakGame_*LimitChoices` constructors are the same shape (`200/400/600/800/1000` and
`0/5/10/15/20`) and their members are even named `coinLimit0..4` and `fragLimit0..4`. Those three,
plus `SLdrScannableParameters` (`li r0,-1` into its single `CAssetId`, i.e. `kInvalidAssetId`), are
reproduced verbatim in `src/MetroidPrime/ScriptLoader/SLdrStructMembers.cpp`.

**`member init from .rodata`** is the same thing with the constants in a table instead of in
`li` immediates, so it needs the table extracted and the member order recovered. The largest are
`SLdrTweakGui_ScannableObjectDownloadTimes` at 28 bytes and `SLdrTweakGui_MovieVolumes` at 40; the
bulk are 44-208 bytes.

**`calls out`** is the majority and is where the group's real cost is. `__ct__17SLdrTweakGui_MiscFv`
is 2,660 bytes, and the interesting part is that it is *not* mostly stores: it sets each
`rstl::string` member from `rstl::basic_string<char>::mNull` (`lis r3,0` /
`R_PPC_ADDR16_HA mNull__Q24rstl66basic_string<...>`) and each `CColor` from
`__ct__6CColorFffff` / `Green__6CColorFv`, and only then the defaults. The 30 constructors in this
shape call one or more of `Green__6CColorFv`, `__ct__6CColorFffff`, `__ct__10SLdrSplineFv`,
`__ct__15SLdrTDamageInfoFv`, `__ct__17SLdrTGunResourcesFv`, `__ct__20SLdrEditorPropertiesFv`,
`__ct__27SLdrTweakPlayer_GrappleBeamFv`, `fn_8023F9A4`, `fn_80241AA4`, `fn_82_1DBBC`,
`fn_8023B55C`.

**The destructors are the most uniform thing in the group, and it is not useful.** 40 of the 60 are
**byte-identical**, sha1 `ecaba247`, 60 bytes each:

```
stwu r1,-16(r1); mflr r0; stw r0,20(r1); stw r31,12(r1)
mr.  r31,r3 ;  beq epilogue          # if (this == 0) skip
extsh. r0,r4 ; ble epilogue          # if ((s16)flags <= 0) skip
bl   CMemory::Free(void*)
epilogue: lwz r0,20(r1); mr r3,r31; lwz r31,12(r1); mtlr r0; addi r1,r1,16; blr
```

That is MWCC's delete-flag entry boilerplate, and it does nothing at all unless the `flags` argument
is positive. The other 20 destructors are 84-588 bytes and call their members' destructors for
real (`__dt__10SLdrSplineFv`, `__dt__15SLdrTDamageInfoFv`,
`__dt__Q24rstl51vector<14SLdrConnection,...>Fv`,
`internal_dereference__Q24rstl66basic_string<...>Fv`).

## The 15 functions, in 9 classes, with no retail counterpart

Reproduce by grepping the class name out of `build/G2ME01/*/obj/*.o` with
`build/binutils/powerpc-eabi-nm -S`; the per-function table below is the same data one row at a
time. Three of the nine are pure renames, three have a counterpart for only one of the two halves,
two have none at all, and one is a type the generator invented:

| class | what retail calls it |
| --- | --- |
| `SLdrTweakGui_DarkWorld` | `SLdrTweakGui_DarkVisor` (`__ct__22SLdrTweakGui_DarkVisorFv`, Tweaks `0x7070`) |
| `SLdrTweakPlayerControls_Booleans` | `SLdrTweakPlayerControls_UnknownStruct1` |
| `SLdrTweakPlayerControls_Controls` | `SLdrTweakPlayerControls_UnknownStruct2` |
| `SLdrTweakPlayerGun_Beam_Combo` | nothing; no `Beam_Combo` symbol anywhere in `Tweaks` |
| `SLdrConnection` | nothing; it only ever appears inside `rstl::vector<SLdrConnection>`, so retail never emits a helper for it |
| `SLdrSequenceTimer` | `__dt__17SLdrSequenceTimerFv` exists (DOL `0x801E19BC`, 100 bytes) but there is no `__ct__` - the constructor is always inlined |
| `SLdrPickup` | `__ct__10SLdrPickupFv` exists (DOL `0x800B40B4`, 328 bytes) but no `__dt__` |
| `SLdrSpline` | `__ct__10SLdrSplineFv` exists (DOL `0x80329B60`, 64 bytes); `__dt__10SLdrSplineFv` is *referenced* by the `Tweaks` text object and defined nowhere, and there is no `__as__` for the type |
| `SLdrAreaAttributes` | **no such type in retail at all.** Retail's area attributes are loaded by `LoadAreaAttributes(CStateManager&, CInputStream&, const CEntityInfo&)`, a function with no `SLdr*` parameter. `scripts/generate_script_loaders.py` invented the struct; its nine members and their order are unverified |

`SLdrAreaAttributes` is the one to be careful with: defining its constructor is harmless for the
link, but nothing in retail corroborates the layout, and per the project's own rule a wrong member
layout is worse than no definition. It is defined empty here and flagged in the source.

## Per-function table

`bytes` is the retail function's length. `size lower bound` is a **lower bound on the struct's
retail size**, taken as the highest member offset the constructor writes plus the width of that
store; it is not the struct size, because a constructor need not touch the last member. `DOL`
addresses are in `build/G2ME01/main.elf`; `Tweaks` addresses are in
`build/G2ME01/Tweaks/obj/auto_00_00006EC4_text.o`, which is the module minus `0x6EC4`.

| # | class | port symbol | retail symbol | where | addr | bytes | shape | size lower bound |
| --: | --- | --- | --- | --- | --- | --: | --- | --: |
| 1 | `SLdrAreaAttributes` | `_ZN18SLdrAreaAttributesC1Ev` | - | - | - | - | **no retail counterpart** | - |
| 2 | `SLdrAreaAttributes` | `_ZN18SLdrAreaAttributesD1Ev` | - | - | - | - | **no retail counterpart** | - |
| 3 | `SLdrCameraShakerData` | `_ZN20SLdrCameraShakerDataC1Ev` | `__ct__20SLdrCameraShakerDataFv` | DOL | `0x802411ac` | 100 | calls out (`__ct__10SLdrSplineFv`) | 0xdc |
| 4 | `SLdrCameraShakerData` | `_ZN20SLdrCameraShakerDataD1Ev` | `__dt__20SLdrCameraShakerDataFv` | DOL | `0x8024113c` | 112 | calls out (`Free__7CMemoryFPCv`, `fn_800327FC`) | 0x18 |
| 5 | `SLdrConnection` | `_ZN14SLdrConnectionC1Ev` | - | - | - | - | **no retail counterpart** | - |
| 6 | `SLdrConnection` | `_ZN14SLdrConnectionD1Ev` | - | - | - | - | **no retail counterpart** | - |
| 7 | `SLdrEditorProperties` | `_ZN20SLdrEditorPropertiesC1Ev` | `__ct__20SLdrEditorPropertiesFv` | DOL | `0x8023f0e4` | 88 | calls out (`fn_8023F9A4`) | 0x3c |
| 8 | `SLdrEditorProperties` | `_ZN20SLdrEditorPropertiesD1Ev` | `__dt__20SLdrEditorPropertiesFv` | DOL | `0x8023f07c` | 104 | calls out (`Free__7CMemoryFPCv`, `fn_8023F968`, `internal_dereference__Q24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>Fv`) | 0x18 |
| 9 | `SLdrPickup` | `_ZN10SLdrPickupC1Ev` | `__ct__10SLdrPickupFv` | DOL | `0x800b40b4` | 328 | calls out (`__ct__20SLdrEditorPropertiesFv`, `__ct__6CColorFffff`, `fn_8023B55C`, `fn_8023E118`, `fn_8023F534`, `fn_802422E4`) | 0x118 |
| 10 | `SLdrPickup` | `_ZN10SLdrPickupD1Ev` | - | - | - | - | **no retail counterpart** | - |
| 11 | `SLdrScannableParameters` | `_ZN23SLdrScannableParametersC1Ev` | `__ct__23SLdrScannableParametersFv` | DOL | `0x8024156c` | 12 | member init from immediates (1 stores, 1 `li`) | 0x4 |
| 12 | `SLdrScannableParameters` | `_ZN23SLdrScannableParametersD1Ev` | `__dt__23SLdrScannableParametersFv` | DOL | `0x80241530` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 13 | `SLdrSequenceTimer` | `_ZN17SLdrSequenceTimerC1Ev` | - | - | - | - | **no retail counterpart** | - |
| 14 | `SLdrSequenceTimer` | `_ZN17SLdrSequenceTimerD1Ev` | `__dt__17SLdrSequenceTimerFv` | DOL | `0x801e19bc` | 100 | calls out (`Free__7CMemoryFPCv`, `__dt__20SLdrEditorPropertiesFv`, `__dt__Q24rstl51vector<14SLdrConnection,Q24rstl17rmemory_allocator>Fv`) | 0x18 |
| 15 | `SLdrSpline` | `_ZN10SLdrSplineD1Ev` | - | - | - | - | **no retail counterpart** | - |
| 16 | `SLdrSpline` | `_ZN10SLdrSplineaSERKS_` | - | - | - | - | **no retail counterpart** | - |
| 17 | `SLdrTBallTransitionResources` | `_ZN28SLdrTBallTransitionResourcesC1Ev` | `__ct__28SLdrTBallTransitionResourcesFv` | Tweaks | `0x1653c` | 124 | calls out (`__ct__10SLdrSplineFv`, `__ct__17SLdrTGunResourcesFv`) | 0xc |
| 18 | `SLdrTBallTransitionResources` | `_ZN28SLdrTBallTransitionResourcesD1Ev` | `__dt__28SLdrTBallTransitionResourcesFv` | Tweaks | `0x1648c` | 176 | calls out (`Free__7CMemoryFPCv`, `__dt__10SLdrSplineFv`, `__dt__17SLdrTGunResourcesFv`, `internal_dereference__Q24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>Fv`) | 0x18 |
| 19 | `SLdrTBeamInfo` | `_ZN13SLdrTBeamInfoC1Ev` | `__ct__13SLdrTBeamInfoFv` | Tweaks | `0x16b90` | 64 | calls out (`fn_82_1DBBC`) | 0x18 |
| 20 | `SLdrTBeamInfo` | `_ZN13SLdrTBeamInfoD1Ev` | `__dt__13SLdrTBeamInfoFv` | Tweaks | `0x16b38` | 88 | calls out (`Free__7CMemoryFPCv`, `fn_82_1DB58`) | 0x18 |
| 21 | `SLdrTDamageInfo` | `_ZN15SLdrTDamageInfoC1Ev` | `__ct__15SLdrTDamageInfoFv` | Tweaks | `0x16ed8` | 60 | member init from immediates (5 stores, 1 `li`) | 0x4 |
| 22 | `SLdrTDamageInfo` | `_ZN15SLdrTDamageInfoD1Ev` | `__dt__15SLdrTDamageInfoFv` | Tweaks | `0x16e9c` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 23 | `SLdrTGunResources` | `_ZN17SLdrTGunResourcesC1Ev` | `__ct__17SLdrTGunResourcesFv` | Tweaks | `0x167fc` | 76 | member init from immediates (15 stores, 1 `li`) | 0x4c |
| 24 | `SLdrTGunResources` | `_ZN17SLdrTGunResourcesD1Ev` | `__dt__17SLdrTGunResourcesFv` | Tweaks | `0x16760` | 156 | calls out (`Free__7CMemoryFPCv`, `internal_dereference__Q24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>Fv`) | 0x18 |
| 25 | `SLdrTIcon_Configurations` | `_ZN24SLdrTIcon_ConfigurationsC1Ev` | `__ct__24SLdrTIcon_ConfigurationsFv` | Tweaks | `0x16a44` | 48 | member init from .rodata (9 stores, 1 `lis`) | - |
| 26 | `SLdrTIcon_Configurations` | `_ZN24SLdrTIcon_ConfigurationsD1Ev` | `__dt__24SLdrTIcon_ConfigurationsFv` | Tweaks | `0x16a08` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 27 | `SLdrTweakAutoMapper_Base` | `_ZN24SLdrTweakAutoMapper_BaseC1Ev` | `__ct__24SLdrTweakAutoMapper_BaseFv` | Tweaks | `0xf4f8` | 2524 | calls out (`Green__6CColorFv`, `__ct__6CColorFffff`) | 0x134 |
| 28 | `SLdrTweakAutoMapper_Base` | `_ZN24SLdrTweakAutoMapper_BaseD1Ev` | `__dt__24SLdrTweakAutoMapper_BaseFv` | Tweaks | `0xf4bc` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 29 | `SLdrTweakAutoMapper_DoorColors` | `_ZN30SLdrTweakAutoMapper_DoorColorsC1Ev` | `__ct__30SLdrTweakAutoMapper_DoorColorsFv` | Tweaks | `0xe50c` | 736 | calls out (`Green__6CColorFv`, `__ct__6CColorFffff`) | 0x30 |
| 30 | `SLdrTweakAutoMapper_DoorColors` | `_ZN30SLdrTweakAutoMapper_DoorColorsD1Ev` | `__dt__30SLdrTweakAutoMapper_DoorColorsFv` | Tweaks | `0xe4d0` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 31 | `SLdrTweakBall_BoostBall` | `_ZN23SLdrTweakBall_BoostBallC1Ev` | `__ct__23SLdrTweakBall_BoostBallFv` | Tweaks | `0xe138` | 268 | calls out (`__ct__15SLdrTDamageInfoFv`) | 0x2c |
| 32 | `SLdrTweakBall_BoostBall` | `_ZN23SLdrTweakBall_BoostBallD1Ev` | `__dt__23SLdrTweakBall_BoostBallFv` | Tweaks | `0xe0e0` | 88 | calls out (`Free__7CMemoryFPCv`, `__dt__15SLdrTDamageInfoFv`) | 0x18 |
| 33 | `SLdrTweakBall_Camera` | `_ZN20SLdrTweakBall_CameraC1Ev` | `__ct__20SLdrTweakBall_CameraFv` | Tweaks | `0xdc2c` | 576 | member init from .rodata (63 stores, 24 `lis`) | - |
| 34 | `SLdrTweakBall_Camera` | `_ZN20SLdrTweakBall_CameraD1Ev` | `__dt__20SLdrTweakBall_CameraFv` | Tweaks | `0xdbf0` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 35 | `SLdrTweakBall_CannonBall` | `_ZN24SLdrTweakBall_CannonBallC1Ev` | `__ct__24SLdrTweakBall_CannonBallFv` | Tweaks | `0xd2d4` | 48 | calls out (`__ct__15SLdrTDamageInfoFv`) | 0x18 |
| 36 | `SLdrTweakBall_CannonBall` | `_ZN24SLdrTweakBall_CannonBallD1Ev` | `__dt__24SLdrTweakBall_CannonBallFv` | Tweaks | `0xd280` | 84 | calls out (`Free__7CMemoryFPCv`, `__dt__15SLdrTDamageInfoFv`) | 0x18 |
| 37 | `SLdrTweakBall_DeathBall` | `_ZN23SLdrTweakBall_DeathBallC1Ev` | `__ct__23SLdrTweakBall_DeathBallFv` | Tweaks | `0xd420` | 112 | calls out (`__ct__15SLdrTDamageInfoFv`) | 0x8 |
| 38 | `SLdrTweakBall_DeathBall` | `_ZN23SLdrTweakBall_DeathBallD1Ev` | `__dt__23SLdrTweakBall_DeathBallFv` | Tweaks | `0xd3c8` | 88 | calls out (`Free__7CMemoryFPCv`, `__dt__15SLdrTDamageInfoFv`) | 0x18 |
| 39 | `SLdrTweakBall_Misc` | `_ZN18SLdrTweakBall_MiscC1Ev` | `__ct__18SLdrTweakBall_MiscFv` | Tweaks | `0x15fe0` | 76 | member init from .rodata (7 stores, 4 `lis`) | - |
| 40 | `SLdrTweakBall_Misc` | `_ZN18SLdrTweakBall_MiscD1Ev` | `__dt__18SLdrTweakBall_MiscFv` | Tweaks | `0x15fa4` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 41 | `SLdrTweakBall_Movement` | `_ZN22SLdrTweakBall_MovementC1Ev` | `__ct__22SLdrTweakBall_MovementFv` | Tweaks | `0x15c98` | 404 | member init from .rodata (48 stores, 19 `lis`) | - |
| 42 | `SLdrTweakBall_Movement` | `_ZN22SLdrTweakBall_MovementD1Ev` | `__dt__22SLdrTweakBall_MovementFv` | Tweaks | `0x15c5c` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 43 | `SLdrTweakBall_ScrewAttack` | `_ZN25SLdrTweakBall_ScrewAttackC1Ev` | `__ct__25SLdrTweakBall_ScrewAttackFv` | Tweaks | `0xd104` | 220 | calls out (`__ct__15SLdrTDamageInfoFv`) | 0x38 |
| 44 | `SLdrTweakBall_ScrewAttack` | `_ZN25SLdrTweakBall_ScrewAttackD1Ev` | `__dt__25SLdrTweakBall_ScrewAttackFv` | Tweaks | `0xd0ac` | 88 | calls out (`Free__7CMemoryFPCv`, `__dt__15SLdrTDamageInfoFv`) | 0x18 |
| 45 | `SLdrTweakGame_CoinLimitChoices` | `_ZN30SLdrTweakGame_CoinLimitChoicesC1Ev` | `__ct__30SLdrTweakGame_CoinLimitChoicesFv` | Tweaks | `0x6064` | 44 | member init from immediates (5 stores, 5 `li`) | 0x14 |
| 46 | `SLdrTweakGame_CoinLimitChoices` | `_ZN30SLdrTweakGame_CoinLimitChoicesD1Ev` | `__dt__30SLdrTweakGame_CoinLimitChoicesFv` | Tweaks | `0x6028` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 47 | `SLdrTweakGame_FragLimitChoices` | `_ZN30SLdrTweakGame_FragLimitChoicesC1Ev` | `__ct__30SLdrTweakGame_FragLimitChoicesFv` | Tweaks | `0x6224` | 44 | member init from immediates (5 stores, 5 `li`) | 0x14 |
| 48 | `SLdrTweakGame_FragLimitChoices` | `_ZN30SLdrTweakGame_FragLimitChoicesD1Ev` | `__dt__30SLdrTweakGame_FragLimitChoicesFv` | Tweaks | `0x61e8` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 49 | `SLdrTweakGame_TimeLimitChoices` | `_ZN30SLdrTweakGame_TimeLimitChoicesC1Ev` | `__ct__30SLdrTweakGame_TimeLimitChoicesFv` | Tweaks | `0x5e80` | 80 | member init from .rodata (5 stores, 5 `lis`) | - |
| 50 | `SLdrTweakGame_TimeLimitChoices` | `_ZN30SLdrTweakGame_TimeLimitChoicesD1Ev` | `__dt__30SLdrTweakGame_TimeLimitChoicesFv` | Tweaks | `0x5e44` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 51 | `SLdrTweakGuiColors_HUDColorsTypedef` | `_ZN35SLdrTweakGuiColors_HUDColorsTypedefC1Ev` | `__ct__35SLdrTweakGuiColors_HUDColorsTypedefFv` | Tweaks | `0xc9e4` | 1100 | calls out (`Green__6CColorFv`, `__ct__6CColorFffff`) | 0x40 |
| 52 | `SLdrTweakGuiColors_HUDColorsTypedef` | `_ZN35SLdrTweakGuiColors_HUDColorsTypedefD1Ev` | `__dt__35SLdrTweakGuiColors_HUDColorsTypedefFv` | Tweaks | `0xc9a8` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 53 | `SLdrTweakGuiColors_Misc` | `_ZN23SLdrTweakGuiColors_MiscC1Ev` | `__ct__23SLdrTweakGuiColors_MiscFv` | Tweaks | `0xa078` | 9716 | calls out (`Green__6CColorFv`, `__ct__6CColorFffff`) | 0x2cc |
| 54 | `SLdrTweakGuiColors_Misc` | `_ZN23SLdrTweakGuiColors_MiscD1Ev` | `__dt__23SLdrTweakGuiColors_MiscFv` | Tweaks | `0xa03c` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 55 | `SLdrTweakGuiColors_Multiplayer` | `_ZN30SLdrTweakGuiColors_MultiplayerC1Ev` | `__ct__30SLdrTweakGuiColors_MultiplayerFv` | Tweaks | `0x7e84` | 632 | calls out (`Green__6CColorFv`, `__ct__6CColorFffff`) | 0x28 |
| 56 | `SLdrTweakGuiColors_Multiplayer` | `_ZN30SLdrTweakGuiColors_MultiplayerD1Ev` | `__dt__30SLdrTweakGuiColors_MultiplayerFv` | Tweaks | `0x7e48` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 57 | `SLdrTweakGuiColors_TurretHudTypedef` | `_ZN35SLdrTweakGuiColors_TurretHudTypedefC1Ev` | `__ct__35SLdrTweakGuiColors_TurretHudTypedefFv` | Tweaks | `0x76a8` | 116 | calls out (`Green__6CColorFv`) | 0x18 |
| 58 | `SLdrTweakGuiColors_TurretHudTypedef` | `_ZN35SLdrTweakGuiColors_TurretHudTypedefD1Ev` | `__dt__35SLdrTweakGuiColors_TurretHudTypedefFv` | Tweaks | `0x766c` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 59 | `SLdrTweakGui_Completion` | `_ZN23SLdrTweakGui_CompletionC1Ev` | `__ct__23SLdrTweakGui_CompletionFv` | Tweaks | `0x7a3c` | 472 | calls out (`Green__6CColorFv`, `__ct__6CColorFffff`) | 0x48 |
| 60 | `SLdrTweakGui_Completion` | `_ZN23SLdrTweakGui_CompletionD1Ev` | `__dt__23SLdrTweakGui_CompletionFv` | Tweaks | `0x79c0` | 124 | calls out (`Free__7CMemoryFPCv`, `internal_dereference__Q24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>Fv`) | 0x18 |
| 61 | `SLdrTweakGui_Credits` | `_ZN20SLdrTweakGui_CreditsC1Ev` | `__ct__20SLdrTweakGui_CreditsFv` | Tweaks | `0x73e8` | 256 | calls out (`Green__6CColorFv`, `__ct__6CColorFffff`) | 0x38 |
| 62 | `SLdrTweakGui_Credits` | `_ZN20SLdrTweakGui_CreditsD1Ev` | `__dt__20SLdrTweakGui_CreditsFv` | Tweaks | `0x736c` | 124 | calls out (`Free__7CMemoryFPCv`, `internal_dereference__Q24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>Fv`) | 0x18 |
| 63 | `SLdrTweakGui_DarkWorld` | `_ZN22SLdrTweakGui_DarkWorldC1Ev` | - | - | - | - | **no retail counterpart** | - |
| 64 | `SLdrTweakGui_DarkWorld` | `_ZN22SLdrTweakGui_DarkWorldD1Ev` | - | - | - | - | **no retail counterpart** | - |
| 65 | `SLdrTweakGui_EchoVisor` | `_ZN22SLdrTweakGui_EchoVisorC1Ev` | `__ct__22SLdrTweakGui_EchoVisorFv` | Tweaks | `0x6c6c` | 500 | calls out (`Green__6CColorFv`, `__ct__6CColorFffff`) | 0x30 |
| 66 | `SLdrTweakGui_EchoVisor` | `_ZN22SLdrTweakGui_EchoVisorD1Ev` | `__dt__22SLdrTweakGui_EchoVisorFv` | Tweaks | `0x6c30` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 67 | `SLdrTweakGui_HudColorTypedef` | `_ZN28SLdrTweakGui_HudColorTypedefC1Ev` | `__ct__28SLdrTweakGui_HudColorTypedefFv` | Tweaks | `0x64c0` | 788 | calls out (`Green__6CColorFv`, `__ct__6CColorFffff`) | 0x28 |
| 68 | `SLdrTweakGui_HudColorTypedef` | `_ZN28SLdrTweakGui_HudColorTypedefD1Ev` | `__dt__28SLdrTweakGui_HudColorTypedefFv` | Tweaks | `0x6484` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 69 | `SLdrTweakGui_LogBook` | `_ZN20SLdrTweakGui_LogBookC1Ev` | `__ct__20SLdrTweakGui_LogBookFv` | Tweaks | `0x527c` | 2712 | calls out (`Green__6CColorFv`, `__ct__10SLdrSplineFv`, `__ct__6CColorFffff`) | 0x238 |
| 70 | `SLdrTweakGui_LogBook` | `_ZN20SLdrTweakGui_LogBookD1Ev` | `__dt__20SLdrTweakGui_LogBookFv` | Tweaks | `0x5200` | 124 | calls out (`Free__7CMemoryFPCv`, `__dt__10SLdrSplineFv`) | 0x18 |
| 71 | `SLdrTweakGui_Misc` | `_ZN17SLdrTweakGui_MiscC1Ev` | `__ct__17SLdrTweakGui_MiscFv` | Tweaks | `0x38b0` | 2660 | calls out (`Green__6CColorFv`, `__ct__10SLdrSplineFv`, `__ct__6CColorFffff`) | 0x388 |
| 72 | `SLdrTweakGui_Misc` | `_ZN17SLdrTweakGui_MiscD1Ev` | `__dt__17SLdrTweakGui_MiscFv` | Tweaks | `0x37f0` | 192 | calls out (`Free__7CMemoryFPCv`, `__dt__10SLdrSplineFv`, `internal_dereference__Q24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>Fv`) | 0x18 |
| 73 | `SLdrTweakGui_MovieVolumes` | `_ZN25SLdrTweakGui_MovieVolumesC1Ev` | `__ct__25SLdrTweakGui_MovieVolumesFv` | Tweaks | `0x452c` | 40 | member init from immediates (8 stores, 1 `li`) | 0x20 |
| 74 | `SLdrTweakGui_MovieVolumes` | `_ZN25SLdrTweakGui_MovieVolumesD1Ev` | `__dt__25SLdrTweakGui_MovieVolumesFv` | Tweaks | `0x44f0` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 75 | `SLdrTweakGui_ScanVisor` | `_ZN22SLdrTweakGui_ScanVisorC1Ev` | `__ct__22SLdrTweakGui_ScanVisorFv` | Tweaks | `0x1af4` | 880 | calls out (`Green__6CColorFv`, `__ct__6CColorFffff`) | 0x38 |
| 76 | `SLdrTweakGui_ScanVisor` | `_ZN22SLdrTweakGui_ScanVisorD1Ev` | `__dt__22SLdrTweakGui_ScanVisorFv` | Tweaks | `0x1ab8` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 77 | `SLdrTweakGui_ScannableObjectDownloadTimes` | `_ZN41SLdrTweakGui_ScannableObjectDownloadTimesC1Ev` | `__ct__41SLdrTweakGui_ScannableObjectDownloadTimesFv` | Tweaks | `0x15504` | 28 | member init from .rodata (2 stores, 2 `lis`) | - |
| 78 | `SLdrTweakGui_ScannableObjectDownloadTimes` | `_ZN41SLdrTweakGui_ScannableObjectDownloadTimesD1Ev` | `__dt__41SLdrTweakGui_ScannableObjectDownloadTimesFv` | Tweaks | `0x154c8` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 79 | `SLdrTweakGui_VisorColorSchemeTypedef` | `_ZN36SLdrTweakGui_VisorColorSchemeTypedefC1Ev` | `__ct__36SLdrTweakGui_VisorColorSchemeTypedefFv` | Tweaks | `0x68e4` | 168 | calls out (`Green__6CColorFv`, `__ct__6CColorFffff`) | 0x8 |
| 80 | `SLdrTweakGui_VisorColorSchemeTypedef` | `_ZN36SLdrTweakGui_VisorColorSchemeTypedefD1Ev` | `__dt__36SLdrTweakGui_VisorColorSchemeTypedefFv` | Tweaks | `0x68a8` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 81 | `SLdrTweakPlayerControls_Booleans` | `_ZN32SLdrTweakPlayerControls_BooleansC1Ev` | - | - | - | - | **no retail counterpart** | - |
| 82 | `SLdrTweakPlayerControls_Booleans` | `_ZN32SLdrTweakPlayerControls_BooleansD1Ev` | - | - | - | - | **no retail counterpart** | - |
| 83 | `SLdrTweakPlayerControls_Controls` | `_ZN32SLdrTweakPlayerControls_ControlsC1Ev` | - | - | - | - | **no retail counterpart** | - |
| 84 | `SLdrTweakPlayerControls_Controls` | `_ZN32SLdrTweakPlayerControls_ControlsD1Ev` | - | - | - | - | **no retail counterpart** | - |
| 85 | `SLdrTweakPlayerGun_Arm_Position` | `_ZN31SLdrTweakPlayerGun_Arm_PositionC1Ev` | `__ct__31SLdrTweakPlayerGun_Arm_PositionFv` | Tweaks | `0x1704` | 136 | member init from .rodata (12 stores, 6 `lis`) | - |
| 86 | `SLdrTweakPlayerGun_Arm_Position` | `_ZN31SLdrTweakPlayerGun_Arm_PositionD1Ev` | `__dt__31SLdrTweakPlayerGun_Arm_PositionFv` | Tweaks | `0x16c8` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 87 | `SLdrTweakPlayerGun_Beam_Combo` | `_ZN29SLdrTweakPlayerGun_Beam_ComboC1Ev` | - | - | - | - | **no retail counterpart** | - |
| 88 | `SLdrTweakPlayerGun_Beam_Combo` | `_ZN29SLdrTweakPlayerGun_Beam_ComboD1Ev` | - | - | - | - | **no retail counterpart** | - |
| 89 | `SLdrTweakPlayerGun_Beam_Misc` | `_ZN28SLdrTweakPlayerGun_Beam_MiscC1Ev` | `__ct__28SLdrTweakPlayerGun_Beam_MiscFv` | Tweaks | `0x122c` | 240 | calls out (`__ct__15SLdrTDamageInfoFv`) | 0x48 |
| 90 | `SLdrTweakPlayerGun_Beam_Misc` | `_ZN28SLdrTweakPlayerGun_Beam_MiscD1Ev` | `__dt__28SLdrTweakPlayerGun_Beam_MiscFv` | Tweaks | `0x11bc` | 112 | calls out (`Free__7CMemoryFPCv`, `__dt__15SLdrTDamageInfoFv`) | 0x18 |
| 91 | `SLdrTweakPlayerGun_Holstering` | `_ZN29SLdrTweakPlayerGun_HolsteringC1Ev` | `__ct__29SLdrTweakPlayerGun_HolsteringFv` | Tweaks | `0x13ef4` | 44 | member init from .rodata (3 stores, 3 `lis`) | - |
| 92 | `SLdrTweakPlayerGun_Holstering` | `_ZN29SLdrTweakPlayerGun_HolsteringD1Ev` | `__dt__29SLdrTweakPlayerGun_HolsteringFv` | Tweaks | `0x13eb8` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 93 | `SLdrTweakPlayerGun_Misc` | `_ZN23SLdrTweakPlayerGun_MiscC1Ev` | `__ct__23SLdrTweakPlayerGun_MiscFv` | Tweaks | `0x13d4c` | 132 | member init from .rodata (12 stores, 7 `lis`) | - |
| 94 | `SLdrTweakPlayerGun_Misc` | `_ZN23SLdrTweakPlayerGun_MiscD1Ev` | `__dt__23SLdrTweakPlayerGun_MiscFv` | Tweaks | `0x13d10` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 95 | `SLdrTweakPlayerGun_Position` | `_ZN27SLdrTweakPlayerGun_PositionC1Ev` | `__ct__27SLdrTweakPlayerGun_PositionFv` | Tweaks | `0x13ab4` | 48 | member init from .rodata (4 stores, 3 `lis`) | - |
| 96 | `SLdrTweakPlayerGun_Position` | `_ZN27SLdrTweakPlayerGun_PositionD1Ev` | `__dt__27SLdrTweakPlayerGun_PositionFv` | Tweaks | `0x13a78` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 97 | `SLdrTweakPlayerGun_RicochetDamage_Factor` | `_ZN40SLdrTweakPlayerGun_RicochetDamage_FactorC1Ev` | `__ct__40SLdrTweakPlayerGun_RicochetDamage_FactorFv` | Tweaks | `0x13948` | 36 | member init from .rodata (6 stores, 1 `lis`) | - |
| 98 | `SLdrTweakPlayerGun_RicochetDamage_Factor` | `_ZN40SLdrTweakPlayerGun_RicochetDamage_FactorD1Ev` | `__dt__40SLdrTweakPlayerGun_RicochetDamage_FactorFv` | Tweaks | `0x1390c` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 99 | `SLdrTweakPlayerRes_AutoMapperIcons` | `_ZN34SLdrTweakPlayerRes_AutoMapperIconsC1Ev` | `__ct__34SLdrTweakPlayerRes_AutoMapperIconsFv` | Tweaks | `0x1373c` | 124 | member init from immediates (27 stores, 1 `li`) | 0x8c |
| 100 | `SLdrTweakPlayerRes_AutoMapperIcons` | `_ZN34SLdrTweakPlayerRes_AutoMapperIconsD1Ev` | `__dt__34SLdrTweakPlayerRes_AutoMapperIconsFv` | Tweaks | `0x13660` | 220 | calls out (`Free__7CMemoryFPCv`, `internal_dereference__Q24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>Fv`) | 0x18 |
| 101 | `SLdrTweakPlayerRes_MapScreenIcons` | `_ZN33SLdrTweakPlayerRes_MapScreenIconsC1Ev` | `__ct__33SLdrTweakPlayerRes_MapScreenIconsFv` | Tweaks | `0x13238` | 400 | member init from immediates (96 stores, 1 `li`) | 0x1fc |
| 102 | `SLdrTweakPlayerRes_MapScreenIcons` | `_ZN33SLdrTweakPlayerRes_MapScreenIconsD1Ev` | `__dt__33SLdrTweakPlayerRes_MapScreenIconsFv` | Tweaks | `0x12fec` | 588 | calls out (`Free__7CMemoryFPCv`, `internal_dereference__Q24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>Fv`) | 0x18 |
| 103 | `SLdrTweakPlayer_AimStuff` | `_ZN24SLdrTweakPlayer_AimStuffC1Ev` | `__ct__24SLdrTweakPlayer_AimStuffFv` | Tweaks | `0x12720` | 208 | member init from .rodata (16 stores, 12 `lis`) | - |
| 104 | `SLdrTweakPlayer_AimStuff` | `_ZN24SLdrTweakPlayer_AimStuffD1Ev` | `__dt__24SLdrTweakPlayer_AimStuffFv` | Tweaks | `0x126e4` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 105 | `SLdrTweakPlayer_Collision` | `_ZN25SLdrTweakPlayer_CollisionC1Ev` | `__ct__25SLdrTweakPlayer_CollisionFv` | Tweaks | `0x123d8` | 80 | member init from .rodata (5 stores, 5 `lis`) | - |
| 106 | `SLdrTweakPlayer_Collision` | `_ZN25SLdrTweakPlayer_CollisionD1Ev` | `__dt__25SLdrTweakPlayer_CollisionFv` | Tweaks | `0x1239c` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 107 | `SLdrTweakPlayer_DarkWorld` | `_ZN25SLdrTweakPlayer_DarkWorldC1Ev` | `__ct__25SLdrTweakPlayer_DarkWorldFv` | Tweaks | `0x161d8` | 136 | calls out (`fn_80241AA4`) | 0xc |
| 108 | `SLdrTweakPlayer_DarkWorld` | `_ZN25SLdrTweakPlayer_DarkWorldD1Ev` | `__dt__25SLdrTweakPlayer_DarkWorldFv` | Tweaks | `0x16180` | 88 | calls out (`Free__7CMemoryFPCv`, `fn_80241A68`) | 0x18 |
| 109 | `SLdrTweakPlayer_FirstPersonCamera` | `_ZN33SLdrTweakPlayer_FirstPersonCameraC1Ev` | `__ct__33SLdrTweakPlayer_FirstPersonCameraFv` | Tweaks | `0xf80` | 188 | member init from .rodata (15 stores, 10 `lis`) | - |
| 110 | `SLdrTweakPlayer_FirstPersonCamera` | `_ZN33SLdrTweakPlayer_FirstPersonCameraD1Ev` | `__dt__33SLdrTweakPlayer_FirstPersonCameraFv` | Tweaks | `0xf44` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 111 | `SLdrTweakPlayer_Frozen` | `_ZN22SLdrTweakPlayer_FrozenC1Ev` | `__ct__22SLdrTweakPlayer_FrozenFv` | Tweaks | `0x12248` | 36 | member init from immediates (3 stores, 1 `li`) | 0x8 |
| 112 | `SLdrTweakPlayer_Frozen` | `_ZN22SLdrTweakPlayer_FrozenD1Ev` | `__dt__22SLdrTweakPlayer_FrozenFv` | Tweaks | `0x1220c` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 113 | `SLdrTweakPlayer_Grapple` | `_ZN23SLdrTweakPlayer_GrappleC1Ev` | `__ct__23SLdrTweakPlayer_GrappleFv` | Tweaks | `0xc4c` | 252 | calls out (`__ct__27SLdrTweakPlayer_GrappleBeamFv`) | 0x34 |
| 114 | `SLdrTweakPlayer_Grapple` | `_ZN23SLdrTweakPlayer_GrappleD1Ev` | `__dt__23SLdrTweakPlayer_GrappleFv` | Tweaks | `0xbf4` | 88 | calls out (`Free__7CMemoryFPCv`, `__dt__27SLdrTweakPlayer_GrappleBeamFv`) | 0x18 |
| 115 | `SLdrTweakPlayer_GrappleBeam` | `_ZN27SLdrTweakPlayer_GrappleBeamC1Ev` | `__ct__27SLdrTweakPlayer_GrappleBeamFv` | Tweaks | `0x120dc` | 64 | member init from .rodata (4 stores, 4 `lis`) | - |
| 116 | `SLdrTweakPlayer_GrappleBeam` | `_ZN27SLdrTweakPlayer_GrappleBeamD1Ev` | `__dt__27SLdrTweakPlayer_GrappleBeamFv` | Tweaks | `0x120a0` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 117 | `SLdrTweakPlayer_Misc` | `_ZN20SLdrTweakPlayer_MiscC1Ev` | `__ct__20SLdrTweakPlayer_MiscFv` | Tweaks | `0x11ee8` | 172 | member init from immediates (15 stores, 1 `li`) | - |
| 118 | `SLdrTweakPlayer_Misc` | `_ZN20SLdrTweakPlayer_MiscD1Ev` | `__dt__20SLdrTweakPlayer_MiscFv` | Tweaks | `0x11eac` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 119 | `SLdrTweakPlayer_Motion` | `_ZN22SLdrTweakPlayer_MotionC1Ev` | `__ct__22SLdrTweakPlayer_MotionFv` | Tweaks | `0x11984` | 636 | member init from immediates (79 stores, 1 `li`) | 0x10 |
| 120 | `SLdrTweakPlayer_Motion` | `_ZN22SLdrTweakPlayer_MotionD1Ev` | `__dt__22SLdrTweakPlayer_MotionFv` | Tweaks | `0x11948` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 121 | `SLdrTweakPlayer_Orbit` | `_ZN21SLdrTweakPlayer_OrbitC1Ev` | `__ct__21SLdrTweakPlayer_OrbitFv` | Tweaks | `0x10c2c` | 476 | member init from immediates (50 stores, 6 `li`) | 0x84 |
| 122 | `SLdrTweakPlayer_Orbit` | `_ZN21SLdrTweakPlayer_OrbitD1Ev` | `__dt__21SLdrTweakPlayer_OrbitFv` | Tweaks | `0x10bf0` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 123 | `SLdrTweakPlayer_ScanVisor` | `_ZN25SLdrTweakPlayer_ScanVisorC1Ev` | `__ct__25SLdrTweakPlayer_ScanVisorFv` | Tweaks | `0x103a4` | 64 | member init from immediates (7 stores, 1 `li`) | - |
| 124 | `SLdrTweakPlayer_ScanVisor` | `_ZN25SLdrTweakPlayer_ScanVisorD1Ev` | `__dt__25SLdrTweakPlayer_ScanVisorFv` | Tweaks | `0x10368` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 125 | `SLdrTweakPlayer_Shield` | `_ZN22SLdrTweakPlayer_ShieldC1Ev` | `__ct__22SLdrTweakPlayer_ShieldFv` | Tweaks | `0x10180` | 52 | member init from immediates (4 stores, 1 `li`) | - |
| 126 | `SLdrTweakPlayer_Shield` | `_ZN22SLdrTweakPlayer_ShieldD1Ev` | `__dt__22SLdrTweakPlayer_ShieldFv` | Tweaks | `0x10144` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 127 | `SLdrTweakPlayer_SuitDamageReduction` | `_ZN35SLdrTweakPlayer_SuitDamageReductionC1Ev` | `__ct__35SLdrTweakPlayer_SuitDamageReductionFv` | Tweaks | `0xfff8` | 44 | member init from .rodata (3 stores, 3 `lis`) | - |
| 128 | `SLdrTweakPlayer_SuitDamageReduction` | `_ZN35SLdrTweakPlayer_SuitDamageReductionD1Ev` | `__dt__35SLdrTweakPlayer_SuitDamageReductionFv` | Tweaks | `0xffbc` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 129 | `SLdrTweakTargeting_Charge_Gauge` | `_ZN31SLdrTweakTargeting_Charge_GaugeC1Ev` | `__ct__31SLdrTweakTargeting_Charge_GaugeFv` | Tweaks | `0x828` | 224 | calls out (`Green__6CColorFv`, `__ct__6CColorFffff`) | 0x1c |
| 130 | `SLdrTweakTargeting_Charge_Gauge` | `_ZN31SLdrTweakTargeting_Charge_GaugeD1Ev` | `__dt__31SLdrTweakTargeting_Charge_GaugeFv` | Tweaks | `0x7ec` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 131 | `SLdrTweakTargeting_LockDagger` | `_ZN29SLdrTweakTargeting_LockDaggerC1Ev` | `__ct__29SLdrTweakTargeting_LockDaggerFv` | Tweaks | `0x578` | 200 | calls out (`Green__6CColorFv`, `__ct__6CColorFffff`) | 0xc |
| 132 | `SLdrTweakTargeting_LockDagger` | `_ZN29SLdrTweakTargeting_LockDaggerD1Ev` | `__dt__29SLdrTweakTargeting_LockDaggerFv` | Tweaks | `0x53c` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 133 | `SLdrTweakTargeting_LockFire` | `_ZN27SLdrTweakTargeting_LockFireC1Ev` | `__ct__27SLdrTweakTargeting_LockFireFv` | Tweaks | `0x358` | 136 | calls out (`Green__6CColorFv`, `__ct__6CColorFffff`) | 0xc |
| 134 | `SLdrTweakTargeting_LockFire` | `_ZN27SLdrTweakTargeting_LockFireD1Ev` | `__dt__27SLdrTweakTargeting_LockFireFv` | Tweaks | `0x31c` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |
| 135 | `SLdrTweakTargeting_OuterBeamIcon` | `_ZN32SLdrTweakTargeting_OuterBeamIconC1Ev` | `__ct__32SLdrTweakTargeting_OuterBeamIconFv` | Tweaks | `0x17c` | 176 | calls out (`Green__6CColorFv`, `__ct__6CColorFffff`) | 0x14 |
| 136 | `SLdrTweakTargeting_OuterBeamIcon` | `_ZN32SLdrTweakTargeting_OuterBeamIconD1Ev` | `__dt__32SLdrTweakTargeting_OuterBeamIconFv` | Tweaks | `0x140` | 60 | calls out (`Free__7CMemoryFPCv`) | 0x18 |

## What was done about it, and what was not

`src/MetroidPrime/ScriptLoader/SLdrStructMembers.cpp`, listed in `files.cmake` and **absent from
`configure.py`**, defines all 136 plus 20 more that defining them brought into view - 78 classes,
156 functions, `link_gap.py` MISSING **724 -> 588**.

The bodies are C++'s own default construction and destruction, which is the *member* half of what
retail's `__ct__`/`__dt__` do, and it is exactly right for the three classes retail constructs
trivially or not at all. The four constructors listed above reproduce retail's defaults as well.
The other 64 do not: they leave POD members uninitialised where retail stores a default tweak
value. That is not a shortcut with a workaround behind it - it is 64 decompilations, each needing
its `.rodata` table and its member order recovered, and the member names in these headers are
hashes (`unknown_0xae149646`), so the order has to come from retail rather than from the header.
In the shipped game the half is dead, because `LoadTypedefSLdrTweak*` and `Tweaks.cpp` overwrite
every field from the config stream; a field the loader does not read is where it would show.

### A cascade worth knowing about

Defining these 68 classes closed 122 symbols, not 136, and *created 14 new gaps*
(`_ZN13SLdrTransformC1Ev` and 13 more) - `link_gap.py` reported them as `gap grew`. The reason is
that a default constructor default-constructs its members, so defining `SLdrEditorProperties()`
made the port reference `SLdrTransform::SLdrTransform()`, which nothing defined. Three more classes
are members of members of those (`SLdrLightParameters` and `SLdrVisorParameters` under
`SLdrActorParameters`, `SLdrWeaponType` under `SLdrDamageInfo`), so ten classes and twenty symbols
were added, and the net is exactly the 136 the list claimed.

The lesson generalises past this group: **an unreferenced undefined member is invisible to
`link_gap.py`, so closing one gap can open another.** 488 classes under `include/` declare a
default constructor or destructor and have no definition in `src/` (624 declare one, 136 are
defined), and only the ones something actually constructs are counted. That count is a whole-tree
sweep over `include/`, not a measurement of this group. Chasing the closure is mechanical - the
member graph is finite and shallow - but it is not bounded by the gap list, and a lane that works
only from the list will keep finding new entries.

## Method

- symbols: the `SLdr*` group of `port_link_gap_list.md` as it stood at `a1d3702` (lines 554-692;
  the group no longer exists), then `_ZN<len><Class><C1|D1>Ev` ->
  `__ct__<len><Class>Fv` / `__dt__<len><Class>Fv`
- addresses and sizes: `build/binutils/powerpc-eabi-nm -S` over `build/G2ME01/*/obj/*.o` and
  `build/G2ME01/main.elf`
- shapes: `powerpc-eabi-objdump -d -r` over each function, classified on store count, `li`/`lis`
  count, and the set of `R_PPC_REL24` targets
- dtor uniformity: sha1 of the raw opcode bytes per function; 40 of 60 share `ecaba247`

Relocations matter and `-r` is not optional: without it every `bl` reads as a branch to the next
instruction and every `lis` looks like an immediate, which classifies 68 real constructors as
empty. That mistake is the same shape as the two `link_gap.py` misclassifications recorded in
`port_link_gap.md`: a measurement that cannot fail is not a measurement.
