# What the port still needs in order to link

Measured 2026-09-25 with `python3 tools/link_gap.py`. Regenerate the numbers with
`--list`; the list at the bottom is the ratchet the tool checks, so a symbol appearing that
is not listed here fails, and a listed symbol that is no longer missing also fails.

## Why this had to be measured

The port builds its game sources as an **OBJECT library**, so there is no link step and
therefore nothing that ever reports what is missing. `MP_SDK_HEADERS_ONLY` is on by default
and `PORT_NOTES.md` records it as the only verified configuration. "The game does not link"
was true and unquantified.

`tools/link_gap.py` compiles `mp_game` for the host, subtracts what the objects define from
what they reference, and classifies the remainder. As of 2026-09-26, after the 72 entity-loader
thunks, the 136 `SLdr*` struct members, the 26 small symbols and the five `CTweakPlayer`
accessors landed, the categories are 33 C++ runtime, 44 libc/libm, 115 in Aurora's own sources,
0 only in an Aurora header, and **554 genuinely unaccounted for**. Those 554 are the work. The
first measurement of that figure said 44; the 170 that have closed since are the retail globals
below, 31 `fn_*`/`lbl_*` sentinels, 72 loader thunks, 62 struct members and small symbols, and
`CTweakPlayer`'s five accessors. `link_gap.py` does not print an undefined total, so the category
split is derived by the same `nm` calls the tool makes.

> **A superseded paragraph, kept because it is the kind of error worth naming.** An earlier
> version of this section read "of 1440 undefined symbols, 722 are the C++ runtime, 23 are libc,
> 106 appear in Aurora's own sources, 1 only in an Aurora header, and 44 are genuinely
> unaccounted for". **Those numbers do not add up** - they sum to 896 of 1440 - and the 44
> predates every closure since. A formatted table is not a measurement: check that the parts sum
> to the whole before believing any of them.

> The first measurement said 63. The 19 that closed are the retail globals below, and closing them
> turned up two miscounts worth recording: the split of 63 was **29** unwritten functions, **19**
> retail globals, 8 game globals, 6 REL symbols and `BuildTime` - the text said 30 and 20, which
> sums to 64.

## What this does not measure, and it hid a stale claim for a whole release

`link_gap.py` only globs objects under a path containing `mp_game`. The other two port targets
are invisible to it: `mp_platform` (the SDK shims and stubs) and `mp_port_entry` (`platform/main.cpp`,
the process entry point). The entry point is the one that *references* the game's classes, so
**nothing the port layer needs from the game can ever appear in this list.**

That is how `PORT_NOTES.md` came to say for a whole release that `COsContext`/`CMemorySys` were
"still missing upstream", and it was wrong about one of the two: `CMemorySys`'s three methods
have been in `src/Kyoto/Alloc/CMemory.cpp` since the port's first build. A header with no `.cpp`
of its own is not a class with no definition, and the tool could not have caught the difference
because no `mp_game` object references either class.

The entry point's own gap is measurable without an executable target - `nm -u` on
`mp_port_entry`'s object, minus everything `mp_game` and `mp_platform` define - and it is now
**zero**: `COsContext` and `CMemorySys` are both fully defined (`src/Kyoto/Basics/COsContext.cpp`
is new; `CMemory.cpp` already had `CMemorySys`). What remains unresolved there is Aurora's own
entry points, libc and the C++ runtime, which `MP_SDK_HEADERS_ONLY` deliberately does not link.
Extending `link_gap.py` to cover all three targets would fold that into the ratchet; it is not
done, because it changes what MISSING means and every entry in the list below would have to be
re-derived.

## The correction that produced this number

**The first version of this measurement said 63, and it was wrong by more than 20x.** It
classified any symbol starting `_Z` as "c++ runtime", which is true of `std::` instantiations
and false of every game function MWCC mangles - they all start `_Z` too. That hid **700 real
game symbols** in a bucket nobody reads. The tool now demangles with `c++filt` and classifies on
the result.

Two smaller versions of the same mistake are recorded because they recur. A bare word match is
not evidence that Aurora provides a symbol - "Allocate" and "Renderer" occur all over its trees,
and whole-word matching filed the game's own `AllocateRenderer` and
`CInputGenerator::CInputGenerator` as provided - so a symbol must be *used*, followed by `(` or
preceded by `::`, `.`, `->`, `&`, `*`, before Aurora's tree may claim it. And a measurement of
stale objects is worse than none, so a source newer than the newest object exits 3 rather than
being believed.

**303 MISSING over 269 objects is the honest number** (was 288/254 before lane `j3` added the four `CIOWin`/`CMainFlow` units - one new MISSING symbol, `CMainFlow::OnMessage`, against the two vtables that left the c++ runtime bucket, which fell 18 -> 16). (was 554, 499 after lane `f1`, and 724 before
the correction), and itsshape matters more than its size. **Re-measured 2026-09-26 by lane f1**, which closed`CIOWinManager::AddIOWin` and removed 30 entries the generated list still carried after earlier
work had closed them; see "The three that only look free" below. **495 again by lane g4**, which
closed `_ZN13CIOWinManager12PumpMessagesER18CArchitectureQueue` and `_ZNK6CModel5TouchEi` and
added four `fn_*` callees those two bodies call, so the *net* moved by +2 while the gross was
-2 and +4. **Re-measured 2026-09-28 (goal item `port-loadtypedefeditorprops`):** `python3
tools/link_gap.py --list` reports **317 MISSING over 649 objects** (`16` c++ runtime, `34`
libc/libm, `134` aurora source, `0` only in an Aurora header) and the generated list holds **317**
entries - down 318 -> 317, `_Z27LoadTypedefEditorPropertiesR20SLdrEditorPropertiesR12CInputStream`
closed by `src/MetroidPrime/ScriptLoader/Structs/SLdrEditorProperties_Load.cpp`. **Re-measured
2026-09-28 (goal item `port-ldrtoentityinfo`):** `python3 tools/link_gap.py` reports **315
MISSING over 650 objects** (`16` c++ runtime, `34` libc/libm, `134` aurora source, `0` only in
an Aurora header) and the generated list holds **315** entries - down 317 -> 315, both
`_Z15LdrToEntityInfoR11CEntityInfoRK20SLdrEditorProperties` and
`_Z15LdrToEntityInfoRK11CEntityInfoRK20SLdrEditorProperties` closed by
`src/MetroidPrime/LdrToEntityInfo.cpp`. **The `303
MISSING over 269 objects` at the head of this paragraph is superseded**, both figures being from
before `link_gap.py` measured `mp_platform` and `mp_port_entry` alongside `mp_game`.
**Re-measured 2026-09-28 after the upstream merge (PrimeDecomp/echoes f2dcbf4 as the base):**
`python3 tools/link_gap.py --rebuild` reports **309 MISSING over 735 objects** (`18` c++
runtime, `37` libc/libm, `145` aurora source, `0` only in an Aurora header). The net figure
(315 -> 309) hides a large gross change: **134 entries closed and 128 opened**. The 74 upstream
units added to `files.cmake` define most of what closed (`CPlayerGun` 22, `CModelData` 13,
`CStreamAudioManager` 9, `CSfxManager`, `CGunWeapon`, `CAnimData`, `CGunStateMachine`,
`CActorLights`, ...), and upstream's real class names replaced our placeholders
(`CPlayerGunUnk578` -> `CAuxWeapon`, `CPlayerGunUnk570` -> `CGunMotion`, `CGunEffectUnk` ->
`CGSFidget`). The newly opened symbols are what those bodies call and nothing implements yet:
`CAudioSys` 16, `CGraphics` 10, `CDSPStreamManager` 6, the renamed `CAuxWeapon`/`CGunMotion`
methods, `CJointData_LinearStorage` 5, `CGunController` 5, `CTweakPlayerGun` 4, and so on. The
list was regenerated with `--write-list`; the `static data members` group is new.
**Adding a file to `files.cmake` is not only a win**: a body that calls retail
functions nothing implements turns one closed symbol into four open ones. The table's per-group
counts were stale before this and are now derived from the list:

| group | count | what closes it |
| other game methods | 164 | generated by `tools/link_gap.py` |
| REL module loaders | 12 | generated by `tools/link_gap.py` |
| unmangled: fn_/lbl_/globals | 57 | generated by `tools/link_gap.py` |
| static data members | 4 | generated by `tools/link_gap.py` |

2026-09-30, 160 -> 162: decompiling `MetroidPrime/CTargetReticles` (goal item
`progress-prime1-ctargetreticles`) made two more of its bodies call retail methods the port
still has no definition for, and both are listed rather than closed here:

- `_Z10TCastToPtrI19CScriptGrapplePointEPT_P7CEntity` - `CCompoundTargetReticle::IsGrappleTarget`
  (now byte-exact) needs the `CScriptGrapplePoint` cast. The explicit instantiation lives in
  `src/MetroidPrime/TypesMatch.cpp`, which `files.cmake` deliberately leaves out: its
  `x_pad0[0x2f0 - sizeof(CPhysicsActor)]` members underflow on a 64-bit host. So the definition
  exists on disk and is still missing from the object library - the `LoadForgottenObject` trap
  above, one file over.
- `_ZNK11CGameCamera6GetFovEv` - `CalculateOrbitZoneReticlePosition` needs the camera's fov.
  `src/MetroidPrime/Cameras/CGameCamera.cpp:160` defines it, and `CGameCamera.cpp` is not in
  `files.cmake` either, so again it is on disk and absent from the build.

Both are listed because they are what the measured gap is, not because anything here provides
them. `link_gap.py --write-list` changed nothing else in the list.

2026-09-29, 253 -> 250: retail's CGraphics bring-up is ported, in port-only
`src/Kyoto/Graphics/CGraphicsHostStartup.cpp` (`CGraphicsSys`'s ctor/dtor and
`Startup` -> `ConfigureVideo` -> `InitGraphicsVariables` -> `ConfigureFrameBuffer` ->
`InitGraphicsDefaults` -> `SetDefaultVtxAttrFmt`, plus the eleven `CGraphics` static data members
the chain needs), and `CGraphicsSys` is constructed in `platform/main.cpp` where retail's `main`
builds it. **Six close and three open, and the three are the interesting part:**

- closed: `CGraphics::SetViewPointMatrix` (0x802C2534, now listed in `files.cmake` because
  `SetIdentityViewPointMatrix`'s retail body is exactly that one call), `DisableAllLights`,
  `SetDepthWriteMode`, `SetCullMode`, and `lbl_80418B08` - which is `CTexture::sLoadedTextures`
  and is a **pointer**, not an array, so `CGX::ResetGXStates` (a listed unit, and called by
  `Startup` for the first time) dereferenced null until it had storage. Removing the host-only
  `CMain::OpenWindow` stand-in closes its two callees outside this list.
- opened, and **none of the three has a body to write**:
  `fn_802BE51C` is `CTevCombiners::Init` (0x6C bytes) and `fn_8032F6EC` is the skinned-model
  workspace allocator (0x88 bytes) - both unwritten in this tree, both reached now because
  `InitGraphicsDefaults` and `ConfigureVideo` are. The third, `GXNtsc480Prog`, is not in this
  list at all and that is worth stating: Aurora **declares** it in
  `dolphin/gx/GXFrameBuffer.h` and never defines it (`GXFrameBuffer.cpp` defines `GXNtsc480IntDf`,
  `GXNtsc480Int`, `GXPal528IntDf`, `GXMpal480IntDf` and no progressive template), so
  `link_gap.py` files it under `aurora header only` - the "a bare word is not evidence that Aurora
  provides a symbol" mistake, in its subtler form. The `progressive` branch is dead on the host
  only because `platform/main.cpp` passes `false`.

2026-09-29, 255 -> 253: `CGraphics::EndScene` and `BeginScene` are written, in port-only
`src/Kyoto/Graphics/CGraphicsHostScene.cpp`, which also defines the `extern "C"` names retail's
callers use for them (`fn_802C1658`, `fn_802C1E60`). Those are the two closed; `SwapBuffers`,
`ClearBackAndDepthBuffers` and the two VI retrace callbacks come with them but were not in the list.

2026-09-29, 261 -> 255: the message pump distributes for real now. Upstream `CInputGenerator.cpp`
and `CIOWinManager.cpp` replace their split files (closing `CInputGenerator::Update` and
`fn_8004935C`); port-only `PortMakeMsg.cpp` carries upstream's `MakeMsg` bodies (closing
`GetParmTimerTick` and `fn_80048EA4`, the `extern "C"` name `CMainFlowDtor.cpp` calls);
`CPreFrontEnd.cpp` closes its constructor `fn_80192808`; `CGraphicsHostGlobals.cpp` defines
`lbl_80418AE4` (retail `.sdata` value **1**, where the reach-data stub it replaces was 0) and
`CGraphics::SetIsBeginSceneClearFb`, the one symbol `CPreFrontEnd::Draw` opened. Outside this
list, `PortMwccNew.cpp` defines `__nw__FUlPCcPCc` (libc/libm group 37 -> 36).

2026-09-29, 263 -> 261: `CSfxManager::Update` is written, and with it `AddPitchBend` and a
port-only `CSfxPitchBend.cpp`, which closes those two. Its three new `CAudioSys` callees
(`SfxPitchBend`, `S3dUpdateListener`, `S3dFlushUnusedEmitters`) are given port bodies in
`PortAudio.cpp` in the same change, so none opens.

2026-09-29, 254 -> 263: the port now links the three boot `CIOWin` windows from their headers
(`CErrorOutputWindow.cpp`, `src/MetroidPrime/PortIOWins.cpp`) instead of reach-stubbed carves, so
frame 1 draws. That closed 4 (the three constructors and `lbl_803B5910`) and opened 13 callees
of `CErrorOutputWindow`'s draw path: `CTextExecuteBuffer` 6, `CTextRenderBuffer` 2,
`CGraphics::SetOrtho`/`SetCullMode`, `MakeMsg::GetParmTimerTick`, `gpDefaultFont`,
`CMemoryCardSys::mIsCardBusy`.

Two of those 67 are not really "free": they are the callees of two `CStateManager` functions
written for the DOL on 2026-09-29 (`AreaLoaded` and `UpdateActorInSortedLists`), and naming them
is what the two new definitions cost in undefined symbols.

- **`fn_800B89FC`** (0x800B89FC, 0x15C bytes) walks `CMapWorldInfo`'s four parallel words, matching
  the high half of each against an area id and dispatching `SendScriptMsgs` on the hits. Retail
  reaches it only from `CStateManager::AreaLoaded`.
- **`fn_80041CCC`** (0x80041CCC, 0x194 bytes) fills a `CAABox` plus a validity byte from an actor,
  dispatching through vtable slot 9 of `CPhysicsActor` after a `TCastToPtr<CPhysicsActor>`.
  `CStateManager::UpdateActorInSortedLists` is its only caller.

The net on the link was zero, and for a reason worth keeping: defining
`_ZN13CStateManager24UpdateActorInSortedListsEP6CActor` took that symbol **out** of the list while
`fn_80041CCC` went in, and `AreaLoaded` was the last function the tree could afford at all. **A
DOL function and a port undefined symbol can cancel**, so "this costs N undefined symbols" is a
statement about a specific pair, not about a function.

One of the 170 was closed on 2026-09-28 without touching anything the port asks for at boot:
`CLight::CLight(const CLight&)` (`_ZN6CLightC1ERKS_`, retail `__ct__6CLightFRC6CLight`,
0x80038C9C, 0xA4 bytes) is retail's out-of-line copy constructor and it lives in
`CStateManager.o`, not in `CLight.o` - `fn_80038C5C` there returns a `{u16, CLight}` aggregate and
calls it. It is written in `src/MetroidPrime/CStateManager.cpp`, so the port gets the symbol from a
file it already compiles. **A port symbol can be closed by a function written for the DOL's sake
rather than for the port's**; the object it belongs to is decided by retail's `.text`, not by which
file reads best.

2026-09-30, 246 -> 246: `CGunController::LoadFidgetAnimAsync` is written for the DOL
(`src/MetroidPrime/Weapons/GunController/CGunController.cpp`, 48 bytes, now 100% in
`report.json` against retail's 0x801DC7F0), and one goes in as one comes out. The out one is
`_ZN14CGunController19LoadFidgetAnimAsyncER13CStateManageriii`; the in one is its only callee,
`_ZN9CGSFidget13LoadAnimAsyncER9CAnimDataiiiR13CStateManager` (retail 0x801DD05C, 0xF0 bytes),
which nothing had needed before because the only caller was itself a reach stub. It is not
closable from what is in this tree: retail's body ends in
`NWeaponTypes::get_token_vector(CAnimData&, int, rstl::vector<CToken>&, bool)`, and
`include/MetroidPrime/Weapons/WeaponCommon.hpp` declares no such function - Prime 1's is two
overloads in `WeaponTypes.cpp` (0xFC and 0xE0 bytes) over token-loading machinery this port does
not have. Until that exists the symbol stays a logged reach stub, so the boot is unchanged: the
call now stops one level deeper, inside a body that was already a stub.

2026-09-30, 246 -> 246 again, and the paragraph above is **superseded**: the callee it called
unclosable is now written. `CGSFidget::LoadAnimAsync`
(`_ZN9CGSFidget13LoadAnimAsyncER9CAnimDataiiiR13CStateManager`, retail 0x801DD05C, 0xF0 bytes) is
in `src/MetroidPrime/Weapons/GunController/CGSFidget.cpp` at 100% in `report.json`: Prime 1's body,
calling retail's single-id `get_token_vector`, which is `fn_8018A8D0` (0x8018A8D0, 0xB4 bytes) - the
form the two `fn_` names already declared in `WeaponCommon.hpp` were reaching towards. It is declared
in that header and left undefined, so the count is again 246 -> 246 with one name changed. The
obstacle was never the token machinery Prime 1's two overloads sit on: it was that **nobody had
named the single-id form**, and the gap list only ever carries names. The first half of the paragraph
above - retail's body ends in exactly that four-argument call - is what made it look like a dead end.
`CGSFidget` is still `NonMatching`, so nothing links the new call yet; `fn_8018A8D0` is on the list in
its place.

## The three that only look free, and the one that is

**`docs/research/rel_loaders.md` says "The port already defines `LoadForgottenObject`, which is why
it is not on the gap list." That is a stale claim and it is worth 30 lines to correct.**
`LoadForgottenObject(CStateManager&, CInputStream&, const CEntityInfo&)` is defined at
`src/MetroidPrime/ScriptObjects/CScriptForgottenObject.cpp:96`, that file **is** a unit in
`configure.py:832`, and it is **absent from `files.cmake`** - so the port never compiles it and the
symbol really is missing. `rel_loaders.md`'s sentence is true of the *source* and not of the
*build*, which is the exact distinction that makes a decompiled file read as landed.

**Adding the one line is a net loss of two, and that is the useful part.** Measured: 499 -> **501**
over 235 objects. The file defines the one symbol and *calls* three the port does not define:

| closed | opened |
| --- | --- |
| `_Z19LoadForgottenObjectR13CStateManagerR12CInputStreamRK11CEntityInfo` | `_Z10TCastToPtrI12CScriptActorEPT_P7CEntity` |
| | `_ZNK10CModelData6RenderERK13CStateManagerRK12CTransform4fPK12CActorLightsRK11CModelFlags` |
| | `_ZNK12CScriptActor20CheckActorRenderOnlyEv` |

**Do not add it until those three are written.** And note what the stale list said about them: all
three were *already* listed in `port_link_gap_list.md` as missing, while the tool reported them as
not missing - because with the file uncompiled nothing referenced them. **A source that nothing
compiles hides its own callees from the measurement**, which is why this file and
`rel_loaders.md` disagreed and neither was obviously wrong.
| `TypesMatch` overrides | 0 | **closed 2026-09-25** - see the section below |
| `rstl` templates | 0 | **closed 2026-09-25** - see the section below |
| ~~`SLdr*` script-loader struct constructors~~ | 0 | **closed 2026-09-25.** "One generator, all trivial in retail" was wrong twice over - see below |

So the shape of the remaining work is **318 decompilation proper, 318 unidentified, and 0 already done**.** (311 until 2026-09-26, lane `chain`; see the integration section below.) That is a different project from "close 63 symbols", and
worth knowing before a lane is pointed at the wrong thing.

> **Three "bulk work, one generator" claims in earlier versions of this table were all wrong**, and
> each would have sent a lane after a generator that does not exist. They are corrected here because
> the corrections are the reusable part:
>
> - **The loaders were neither "all one shape" nor "133 unnamed".** All 159 are named in
>   `symbols.txt`, as `fn_802189A4` and friends - dtk could not *pair* them because their only
>   caller is a static initialiser, but the names were in the symbol table the whole time. And
>   **93 thunks of that 44-byte shape are in the DOL**, of which 73 were unclaimed, so the real
>   split was 73 free and 86 real, not 20 free and 133 unknown. The key that unlocked it was
>   `__sinit_ScriptLoader_cpp` (0x80242894, 5,696 bytes, already `Matching`): retail builds the
>   same 184-entry `{FourCC, FScriptLoader}` table the port's `ScriptLoader.cpp` has, so decoding
>   its `lis`/`addi`/`stw` dataflow yields every loader's address, and the port's 184 tags then
>   match **position for position, zero mismatches**. That is what made it evidence rather than a
>   guess. `docs/research/rel_loaders.md` has the table and the method.
> - **The 136 `SLdr*` struct constructors and destructors were "all trivial in retail" and were not.**
>   **0 of the 68 constructors are no-ops** - 30 construct members and then store defaults, the
>   largest is 9,716 bytes - and decisively, **retail never defines those symbols at all**: it
>   spells its implicit constructor/destructor `__ct__<len><Class>Fv`/`__dt__<len><Class>Fv` where
>   GCC wants `C1Ev`/`D1Ev`, so there was never a retail range to claim and **no `Matching` unit
>   could be written**. `docs/research/sldr_ctors.md`.
> - **A gap list is not a closed set of work.** Defining a default constructor constructs its
>   members, so closing the `SLdr*` group *opened* 14 new gaps on the way, and a whole-tree sweep
>   finds **488** classes under `include/` declaring a constructor or destructor nothing defines.
>   The list is what is *reachable*, not what is left.


## The ten that a carve-out closed, and the rule it needed (2026-09-26, lane `v2`)

321 -> 311. Ten symbols off the generated list, and the mechanism is worth more than the ten:
**a single retail function can be claimed out of a dtk `auto_03_*` range and linked as a
`Matching` unit of its own**, which is a supply of units the project had not been using.
Everything here is 4 to 48 bytes, and it is nine files plus one that stayed `NonMatching`.

| closed symbol | retail | bytes | what provides it |
| --- | --- | --- | --- |
| `_ZNK13CSimpleShadow12GetTransformEv` | `GetTransform__13CSimpleShadowCFv`, 0x800DF478 | 4 | `src/MetroidPrime/CSimpleShadowAccessors.cpp` - `return x0_xf;`, a member at +0, so the body is one `blr` |
| `_ZNK13CSimpleShadow5ValidEv` | `Valid__13CSimpleShadowCFv`, 0x800DF268 | 12 | `src/MetroidPrime/CSimpleShadowValid.cpp` - `return x48_24_collision;` |
| `_ZN10CGameState11GetGameModeEv` | `GetGameMode__10CGameStateFv`, 0x80142464 | 8 | `src/MetroidPrime/Player/CGameStateGetGameMode.cpp` - one `lwz r3,412(r3)` of `x19c_ptr` |
| `_ZN10CGameState14SetIsDarkWorldEb` | `SetIsDarkWorld__10CGameStateFb`, 0x801424BC | 16 | `src/MetroidPrime/Player/CGameStateSetIsDarkWorld.cpp` - one `bool : 1` of the flag byte at 0x2EC |
| `_ZNK11CQuaternion13BuildInvertedEv` | `BuildInverted__11CQuaternionCFv`, 0x80028E08 | 48 | `src/Kyoto/Math/CQuaternionBuildInverted.cpp` - `CQuaternion(w, -x, -y, -z)`, **not** the header's inline `BuildEquivalent()`, which negates the scalar too |
| `_ZNK13CSimpleShadow9GetBoundsEv` | `GetBounds__13CSimpleShadowCFv`, 0x800DF3DC | 124 | `src/MetroidPrime/CSimpleShadowGetBounds.cpp`, **`NonMatching` at 76.65%** - the port gets the symbol, the DOL does not get our bytes |
| `fn_8021FA80` | 0x8021FA80 | 8 | `src/MetroidPrime/ScriptLoader/CoinLoaderSet.cpp` - `gLoader_Coin.value = loader` |
| `fn_80227B2C` | 0x80227B2C | 8 | `src/MetroidPrime/ScriptLoader/RsfAudioLoaderSet.cpp` - the same for `gLoader_RsfAudio` |
| `fn_80229FBC` | 0x80229FBC | 8 | `src/MetroidPrime/ScriptLoader/FlyerSwarmLoaderSet.cpp` - `gLoader_FlyerSwarm` |
| `fn_80232334` | 0x80232334 | 8 | `src/MetroidPrime/ScriptLoader/SkyRippleLoaderSet.cpp` - `gLoader_SkyRipple` |

Two more units landed `Matching` without closing a port symbol, because the port does not ask
for them: `CSimpleShadow::SetAlwaysCalculateRadius` (0x800DF458, 16 bytes) and
`CGameState::GetHardModeDamageMultiplier` (0x80142498, 36 bytes). Each is one more `linked`
function, which is the count the project's one rule accepts.

**The four loader setters are a `fn_` family whose four source files said they could not be
written, and they can.** `Coin.cpp`, `RsfAudio.cpp`, `FlyerSwarm.cpp` and `SkyRipple.cpp` each
carried the note "The 8-byte setter at 0x8021FA80 is deliberately NOT claimed: REL modules import
it by its retail name, so it cannot be renamed and must stay in dtk's auto unit". The name really
is fixed - all four modules import theirs - but the constraint is on the **name**, not on the
file. `Coin.cpp` cannot claim 0x8021FA80..0x8021FA88 because it already claims
0x8021FA54..0x8021FA80 immediately below it. A unit that emits nothing but `fn_8021FA80` can, and
it keeps the retail name, so every REL import still resolves. Each setter is a single
`stw r3,<disp>(r13) ; blr` into the `.sbss` slot the thunk unit above it owns
(`tools/sda.py` names all four: `gLoader_Coin` 0x80419530, `gLoader_RsfAudio` 0x80419588,
`gLoader_FlyerSwarm` 0x804195A0, `gLoader_SkyRipple` 0x80419630), and the port already declared
all four as `void fn_XXXXXXXX(FScriptLoader*)` - which is where the signature came from.

**The rule the mechanism needs, which nothing in the tree said: one discontiguous range per unit
per section.** Adding `GetBounds` (0x800DF3DC..0x800DF458) as a *second* `.text` range of
`CSimpleShadowAccessors.cpp`, four bytes away from the `GetTransform` it already claimed, fails
in `dtk dol split` before anything is compiled:

```
Cyclic dependency encountered while resolving link order:
MetroidPrime/CSimpleShadowAccessors.cpp -> auto_03_800DF458_text
```

This is the same failure the `ScriptCoin` write-up records for a REL split ("one unit cannot
claim two discontiguous ranges"), and it is why `CAi`'s four claimed ranges are legal: they are
four *sections*, not two ranges inside one. `Runtime/__init_cpp_exceptions.cpp`'s two `.dtors`
ranges survive because `.dtors` carries no relocations. **Each carved function therefore needs
its own source file and its own unit** - which is also what `linked` wants, since one unit is one
`linked` function.

**The `bool : 1` encoding, measured rather than decoded, and it is worth having.** mwcceppc
allocates a one-bit field in **declaration order**, and the write encoding counts *down* from the
top of the byte:

| field index | write | read |
| --- | --- | --- |
| 0 | `rlwimi r0,rX,7,24,24` | `rlwinm r3,r0,25,31,31` |
| 1 | `rlwimi r0,rX,6,25,25` | `rlwinm r3,r0,26,31,31` |
| 2 | `rlwimi r0,rX,5,26,26` | `rlwinm r3,r0,27,31,31` |
| n | `rlwimi r0,rX,7-n,24+n,24+n` | `rlwinm r3,r0,25+n,31,31` |

`CGameState::SetIsDarkWorld` needed retail's `,5,26,26`, and a struct with the field declared
*first* compiles to `,7,24,24` and scores 96.25% - so the field is the **third**, and
`include/MetroidPrime/Player/CGameState.hpp`'s `u8 x2ec_flags` had to become
`{ bool b7:1; bool b6:1; bool b5:1; u8 rest:5; }`. That header's own comment had said the split
was "a change with no measured effect until `CGameStateStreamCtor.cpp` reaches 0x80144300"; that
unit has not (it is `NonMatching` at 24.33%), but `SetIsDarkWorld` does, so the change is made
and `b5` is the dark-world flag - the same byte and bit the save-game reader's **third**
`ReadBits(1)` fills (0x80144390). `CGameStateCtor.cpp` already carried its own
layout-identical local copy of that struct, which is the independent check on the layout. The
same table then made `CSimpleShadow::Valid` a one-liner: `rlwinm r3,r0,25,31,31` is field 0, and
the header already called field 0 `x48_24_collision`; `SetAlwaysCalculateRadius`'s `,6,25,25`
is field 1, which the header called `x48_25_alwaysCalculateRadius`. Header and retail agree, which
is the check that these are the same fields and not a coincidence of the numbering.

**What did not work, so it is not retried.**

- `CAABox`-taking shapes for `CSimpleShadow::GetBounds`: the corner arithmetic, the register
  assignment, the dead `addi r5,r1,8` and both corner addresses are byte-identical to retail in
  nine tried bodies, but retail's frame is 48 bytes and holds a *third* `CVector3f` (the
  translation, spilled to sp+32/36/40) where every shape tried here gives 32 bytes and two.
  Bodies that materialise the third `CVector3f` lose the store addresses instead: 25.97%
  (default-construct then assign), 49.16% (three named `float`s), 60.65% (named `CVector3f`
  temporaries), 6.0% (a static helper taking `const CVector3f&`, or `CVector3f::operator+` with
  a scalar). The best is 76.65% and the gap is one frame decision, not logic. The `splits.txt`
  range and dtk's retail object are in place, so `tools/try_batch.py` iterates on it directly.
- ~~**`CActor::SetDirtyFlags` (0x8004A0A0, 56 bytes) is blocked on a header change.**~~
  **SUPERSEDED - it was never blocked, and the diagnosis above was wrong in the one direction
  that matters.** `MetroidPrime/CActorSetDirtyFlags.cpp` is now `Matching` at 100.00%, 1/1, and
  it is in `files.cmake`, so the port's real link no longer asks for the symbol. The reasoning
  that said the `uint` group at 0x150 had to be split into `u8`s was a misreading of `lbz`/`stb`
  as evidence about the group's declared type: mwcceppc packs a 32-bit bitfield unit MSB-first
  and then picks the *narrowest access covering the field*, so a 1-bit field in the group's
  **first byte** already compiles to `lbz`/`stb` with `mb` = 24 + that field's index within the
  byte. Retail's bytes are `mb` 27/28/29/30, i.e. the 4th, 5th, 6th and 7th fields of a group
  whose byte is 0x150 - reproduced byte for byte by the existing `uint` declaration, with no
  header change and no `Matching` unit moved. The measurement is in that file's header and it
  came from `tools/probe_cactor_offsets.py`, which compiles one setter per bitfield with
  mwcceppc's own flags. See `docs/PROCESS_LESSONS.md` #19.
- ~~`CDamageVulnerability::NormalVulnerabilty` (0x800DBB70, 16 bytes) ... **the four objects do
  not fit.**~~ **SUPERSEDED - the overlap was arithmetic on two addresses from different
  sections.** `MetroidPrime/CDamageVulnerabilityStatics.cpp` is now `Matching` at 100.00%, 1/1.
  The three instructions are `lis r3,-32706` / `addi r3,r3,-22124` / `addi r3,r3,4` against
  `lbl_803DA994`, which evaluates to **0x803DA998**, not 0x802DA698 - `802DA` and `803DA` are one
  digit apart in two places, which is exactly the shape of a miskeyed address. And
  `SetGlobalOrientAndTrans` is in **`.text`** while these objects are in **`.bss`**
  (0x803C5A20..0x80417D64), 0x11000 apart, so a `.bss` object could not overlap a `.text`
  function even in principle. There are **five** objects, not four, every stride exactly 0x30,
  spanning 0x803DA994..0x803DAA88 (0xF4, recorded as one `lbl_803DA994` in `symbols.txt`), and
  `NormalVulnerabilty` returns the lowest. The `+4` is a subobject offset and it only survives if
  the source names a subobject: a literal address folds it into the `addi` and the unit scores
  three instructions instead of four. The other four accessors are still unnamed and still
  unwritten - the enum order in `CDamageVulnerability.hpp` cannot be turned into addresses by
  counting callers.
- `include/Kyoto/Animation/CSkinnedModel.hpp` **cannot be included by any decompilation unit**:
  mwcceppc rejects it with "implicit 'int' is no longer supported in C++" on the implicit-int
  `virtual ~CSkinnedModel();`, and after spelling that out it still reports "illegal
  struct/union/enum/class definition" on the class body. So
  `CSkinnedModel::ClearPointGeneratorFunc` (0x8030F048, 12 bytes, and only
  `li r0,0 ; stw r0,-25032(r13) ; blr`) could not be written without first repairing that
  header, and the repair is not understood. A `Matching` unit needs a source that compiles, so
  this one is left.
- **The nine units at "100% fuzzy, 0 bad functions, still `NonMatching`" are not free.** All nine
  were flip-tested and all nine FAIL: `CScanTreeInventory` and `CIOWinManagerPumpMessages` and
  `CObjectReference` and `CEntity` and `DolphinCDvdFile` and `CPowerBeam` carry COMDAT weak copies
  *and* have `.data`/`.bss`/`.rodata` their object does not fill, `CQuaternion` is 7 bytes short
  in `.sbss` and 4 in `.sdata2`, and `DolphinCColor` is 4 bytes short in `.sdata2` with 4
  unclaimed in `.sbss2`. `CObjectReference` additionally fails to link on an undefined
  `CVParamTransfer::Null()` and `CPowerBeam` on undefined `CGunWeapon::Reset/PlayAnim/Draw`.
  Reading "0 bad functions" as "ready to promote" is the trap; `tools/unit_fit.sh` per unit is
  what separates them.

Two more things measured along the way that the next lane will otherwise re-derive:

- **mwcceppc evaluates `CAABox`'s two constructor arguments right to left.** Retail's sp+8 holds
  the *second* argument's `CVector3f`. `CAABox(pos - r, pos + r)` is therefore the correct
  source and `CAABox(pos + r, pos - r)` scores 60.84% against 76.65% for the same stores.
- **`link_gap.py --rebuild` is part of `tools/gate.sh`, and closing a listed symbol fails the gate
  until `python3 tools/link_gap.py --write-list` is run.** It also checks the counts quoted in the
  table above and in `docs/HANDOFF.md`'s state block, so a lane that closes symbols has to
  regenerate the list *and* re-derive the state block in the same change, not after it. And
  `build-port/build.ninja` does **not** depend on `files.cmake`: after adding a source to
  `files.cmake`, `tools/link_gap.py --rebuild` measures a build that never compiled it and the
  symbol still reads as missing. `touch CMakeLists.txt` first, or the number is about the wrong
  tree.


## The 26 small symbols, and what closing them taught (2026-09-25)

724 -> 698, in one turn, all in `src/MetroidPrime/PortGlobals.cpp`. The three groups were the
easiest thing in the document and they were still two days of reading, so the recipes are worth
writing down.

**A static data member is an ordinary C++ static, and its value is in the binary.** Ten of the
twelve are named in `config/G2ME01/symbols.txt` and one `objdump -s` away. Two are not named at
all and were the only real work:

- `CSfxManager::kMedPriority` and `CAudioSys::kMaxVolume` have **no Echoes symbol** (Metroid
  Prime's map has both), so they cannot be found by name. `kMedPriority` turned out to be a
  2-byte word at `.sdata2:0x8041E2E4`, found by reading the priority argument of the emitter
  call `fn_8029EAF4`, whose 33 call sites pass `lha r9,-16604(r2)`; the map types
  `0x8041E2E0` as four consecutive 2-byte objects, and the three that matter are 255
  (`kMaxPriority`), 127 (`kMedPriority`) and 0xFFFF (`kInternalInvalidSfxId`) - the three the
  neighbouring header comments already quote. `kMaxVolume` is a **1-byte** object at
  `.sdata2:0x8041F018`, and the way to know it is one byte is that all fourteen of its readers
  are `lbz` and none is an `lfs`. Its value, 192, is a `clamp(v, kMaxVolume)` ceiling in
  `fn_8016864C`, which is what identified it.
- **`li rX,127` before a `CSfxManager` call is not evidence of the priority.** It appears at 147
  sites and it is the *volume* argument of `SfxStart(id, vol, pan, ...)`; 127 is the max 7-bit
  volume. A constant that is a plausible argument in the wrong position is worth nothing.

`CAudioSys::kVolumeTable` is the one that is not a scalar. Its 128 halfwords are **exactly**
`(i*i*32768) / (127*127)` for i = 0..127, with zero mismatches - a square-law amplitude ramp -
which is also the proof that the object is 128 entries and not 256, since dtk's `size:0x100`
happens to be both the width and the distance to the next symbol here. Nothing in the DOL
references the table, so the index range is unproven and the header's `int` and `uchar` indices
can both read past the end.

**The eight `TypesMatch` overrides were not eight unwritten bodies.** Six of them are already
written in `src/MetroidPrime/TypesMatch.cpp`; they are missing from the port because that file is
not in `files.cmake`, and it **cannot** be added: it sizes its throwaway classes with
`uchar x_pad0[0x2f0 - sizeof(CPhysicsActor)]` and the host's `CPhysicsActor` is larger than
retail's 0x2f0, so the subtraction underflows and gcc rejects the array. Only
`CScriptPickup::TypesMatch` and `CScriptSequenceTimer::TypesMatch` are genuinely undefined
anywhere. The earlier text here said "eight classes declare `TypesMatch` and never define it";
that was wrong for six of the eight and is superseded. All eight are now defined in
`PortGlobals.cpp`, which means **adding `TypesMatch.cpp` to `files.cmake` later would duplicate
six of them** - either that block moves into the file or its two `TYPES_MATCH_IMPL` lines come
out. The bodies are retail's: 0x38 bytes each and `cmpwi` against the id the header's
`EEntityType` already names, with the parent read off the `bl`.

**Two of the six `rstl` symbols were not what the list said.** `_ZN4rstlplERK...` is
`rstl::operator+(const string&, const string&)` - the Itanium mangling of `operator+` is `pl`, and
the function is already declared at `include/rstl/string.hpp:344` and never defined, so it is not
a new `pl` at all. And the two `basic_string::mNull` sentinels are *already written* in
`src/rstl/rstl_strings.cpp` as `template <> char basic_string<char>::mNull;`, which is a
**declaration and not a definition**: a static data member without an initialiser emits nothing.
That is the same trap LANE.md records for `extern "C"`, in a `Matching` unit, so the fix belongs
there and was not made from a port TU.

**`rstl::CRefData::sNull` is the one symbol in the group with no retail value at all.**
`CRefData` is absent from the map, from every dtk object and from `main.elf`. The
`R_PPC_EMB_SDA21 sNull__Q24rstl8CRefData` that `RUNNING_THE_DECOMP.md` cites is a relocation in
**our** `build/G2ME01/src/MetroidPrime/CStateManager.o`, not a retail one, so that sentence is
superseded. The count still has to be large, because `rc_ptr`'s default constructor AddRefs the
sentinel and `ReleaseData` deletes it as soon as `DelRef() <= 0`; 0xFFFFFF is the port layer's
own value for the same class in the sibling tree.

## The upstream Tweaks rewrite (2026-09-29, upstream sync a14f961)

Upstream replaced the sixteen `SLdrTweak*.cpp` loaders with one generated
`src/MetroidPrime/ScriptLoader/Tweaks.cpp` and gave `Tweaks/Tweaks.cpp` a real
`REL_CreateTweakGlobals`, which the host now compiles instead of the old empty `TARGET_PC` stub.
Net effect on the host link: `link_check.sh` 314 -> 259 undefined; the "REL module loaders"
group fell 71 -> 12 (the loaders upstream's sync now defines) and the list gained five game functions
those bodies call and nothing defines yet, upstream included:
`LoadTypedefSLdrDamageInfo`, `CMappableObject::ReadAutomapperTweaks`,
`CTweakPlayerGun::InitBeamInfo`, `CTweakPlayerRes::ResolveResources` and
`CPlayerCameraBob::ReadTweaks`. The `gpTweak*` single_ptr slots it also references are
defined in `src/MetroidPrime/PortGlobals.cpp` beside the others, so they are not on the list.

## The fifth upstream sync (2026-09-30, upstream 03bd14b)

The port took `CAuxWeapon.cpp` and the five GunController units (`files.cmake`'s last block),
which define eleven listed symbols: `CAuxWeapon`'s ctor, `GetTargetId` and `SetTargetId`,
`CGunController::Reset`/`EnterFidget`/`EnterComboFire`/`ReturnToDefault`, `CGunMotion`'s dtor and
`BasePosition`, and `CGSFidget::IsAnimLoaded`/`UnLoadAnim`. Their bodies ask for five symbols nothing
defined at that point, upstream included: `CGameProjectile::GetBeamAttribType` (from `CAuxWeapon`),
`CPASDatabase::FindBestAnimation`/`GetAnimState`, `NWeaponTypes::are_tokens_ready` and
`CGunMotion::LoadAnimations` (from the GunController set). **Superseded 2026-09-30:**
`CGunMotion::LoadAnimations` is now written for real in `src/MetroidPrime/Weapons/GunController/`
`CGunMotion.cpp` (goal item `progress-prime1-cgunmotion`), so it has left the list; in its place
the same body asks for `fn_8018A7E8` (retail 0x8018A7E8, 0xE8 bytes), which is still unwritten.
The other four are unchanged. The two other new entries,
`~CCollidableAABox` and `~CCollidableSphere`, come from the sync's headers: they now declare both
destructors out of line, in `CCollidableAABox.cpp`/`CCollidableSphere.cpp`, which the port leaves out
because they open more than they close. `CGameCollision.cpp` and `CRagDoll.cpp` build the temporaries
that need the destructors. Net: `link_check.sh` 254 -> 250.

## The seventh upstream sync (2026-09-30, upstream a6538995)

The port took upstream's five Tweak units (`CTweakPlayer`, `CTweakPlayerGun`, `CTweakTargeting`,
`CTweakGuiColors`, `CTweakGui`), which define eight listed symbols: `CTweakPlayer::GetBallRadius` and
`GetEyeOffset`, and `CTweakPlayerGun`'s `InitBeamInfo`, `GetMaxAbsorbedPhazonShots`, `GetBeamInfo`,
`GetHoloHoldTime`, `GetGunTransformTime` and `GetGunExtendDistance`. Two entries are renames, not new
gaps: `CStateManager::fn_8003C4B8` is now `InformListeners(const CVector3f&, EListenNoiseType)`, and
`CGameSplineDesc`'s ctor takes `CMayaSpline` where it took `SLdrSpline`. The two new names,
`LdrToDamageInfo(const SLdrDamageInfo&)` and `CDamageInfo(const SLdrTDamageInfo&, bool, bool, bool, bool)`,
are asked for by the new `CTweakPlayer.cpp` and `CTweakPlayerGun.cpp` respectively (measured with `nm` over
`build-port-link`); nothing in the tree defines either yet. The sync's other seven units stay EXCLUDED in
`tools/check_files_cmake.py`. Net: `link_check.sh` 250 -> 244, list 246 -> 240.

## What is left, and what the port still needs

`COsContext` is written (`src/Kyoto/Basics/COsContext.cpp`), and the `CMemorySys` claim in
`PORT_NOTES.md` was simply wrong - its three methods and the allocator it returns have been in
`src/Kyoto/Alloc/CMemory.cpp` since the port's first build. **A header with no `.cpp` of its own
is not a class with no definition.**

> **Superseded 2026-09-25.** This section used to say: "The two things that block a first frame
> are not in this list at all, because they are not symbol problems: `CMain::RsMain` is an
> empty body and `CMain::OpenWindow` is unimplemented, so nothing calls
> `COsContext::OpenWindow` yet." The first half is right and the second half is wrong on both
> counts. **`CMain::OpenWindow` does not exist in retail Echoes** - 19 `CMain` methods are
> named in `symbols.txt` and it is not one of them, the string occurs nowhere in the DOL's
> disassembly, and `RsMain` (0x80005C6C, 0x864) makes no call on `x0_osContext` at all.
> Retail's window/VI bring-up is in `main` (0x801EFB00), the caller of `InvokeCMain`, through
> `fn_802BE85C` -> `fn_802C329C` -> `fn_802C2FD4`. The *second* half of the correction is that
> the frame loop **is** a symbol problem, and it is a small one: the twelve symbols
> `CGameArchitectureSupport`'s constructor, `UpdateTicks` and destructor reference are all
> already on the list in `port_link_gap_list.md` - `CIOWinManager`'s five methods,
> `CInputGenerator::Update` and its constructor, `CStopwatch::CSWData::Initialize` and `::Wait`,
> `CMainFlow::CMainFlow`, `CMain::ResetGameState`, `CGameArchitectureSupport::UnloadAudio` and
> `AllocateRenderer`. 2,584 bytes of decompilation, not 300 functions.
>
> The full ordered map, measured step by step, is **`docs/research/boot_path.md`**. Read that
> before planning port work; this section is the summary.

## The three that `CResFactory::Build` brought with it, and what they are (2026-09-26)

`CResFactory::Build` is written, byte-exact and `Matching` (retail `fn_802FA960`, 0x802FA960,
0xC0 = 192 bytes, 100.00%, `flip_test` PASS), and `src/Kyoto/CResFactoryBuild.cpp` is in
`files.cmake` so the port compiles it. That closed the third and last frame-0 vtable -
`PortReachStubs.cpp`'s `extern "C" char reachstub_data_0[64] asm("_ZTV11CResFactory")` is deleted,
and the *c++ runtime / linker* group is **16 -> 15** - and it cost **+3 MISSING**:

| symbol | retail | what it is |
| --- | --- | --- |
| `fn_802FAAE4` | 0x802FAAE4, 0x88 = 136 | the lookup `Build` calls first. Returns `this+0xA4` when the map at `CResFactory`+0xB4 is empty and the found node's `+0x18` otherwise, so it answers "is this resource already being built". |
| `fn_802FA1BC` | 0x802FA1BC, 0x12C = 300 | the pump. `Build` calls it with `(this, &entry, 0)` and `CMain::AsyncIdle` - already written and on the ratchet - calls it with `(this, &xA0, elapsed)`. |
| `fn_802FA7D4` | 0x802FA7D4, 0x18C = 396 | the synchronous build: `CFactoryMgr`'s find, `CResLoader::LoadResourceSync`, `CResLoader::LoadNewResourceSync`, both dispatch sites, and two `delete`-through-the-vtable teardowns of a `CInputStream`. |

**This is the right way round and it is worth saying why.** The alternative was to keep the zero
vtable, which also "linked". A vtable is emitted only by the translation unit defining the class's
key function - the first non-pure, non-inline virtual *declared*, which is `~CResFactory` in both
MWCC and GCC because `Kyoto/CResFactory.hpp` declares it out of line - so the alternatives were a
real vtable or 64 zero bytes. The three holes are **named**, they are in the resource chain that
`docs/research/boot_path.md` already says is the wall, and they are the *next* thing a lane
writes. A zero vtable names nothing and crashes on first use.

`~CResFactory` itself is **not** written, and that is a measured negative worth its own paragraph.
Its body is 0x802FB038, 0xA8 = 168 bytes, and a user-provided destructor with five
`extern "C"` calls reproduces it **instruction for instruction** - the two vtable stores, the
`this == nullptr` early return and the `delete this` tail included. It cannot be a `Matching`
unit because the same object then carries two things retail does not have there:

* **`__vt__8IFactory`**, 0x20 bytes of `.data` that retail has at 0x803B19B8 **with a zero in the
  destructor slot** - the whole 0x20 is zeros - while MWCC writes a relocation against a weak
  `__dt__8IFactoryFv`, because `virtual ~IFactory() {}` is inline in `Kyoto/CResFactory.hpp`;
* that weak **`__dt__8IFactoryFv`**, 0x48 bytes of `.text` at 0x802FB0E0, which is retail's
  `fn_802FB0E0` and therefore not free to reuse.

Making the base destructor pure fixes the first and breaks the second: measured, the destructor
becomes 0x9C and the base-vptr store is replaced by `mr r3,r30 / li r4,0 / bl __dt__8IFactoryFv`,
which is the trap `docs/research/boot_probe.md` records for the other two vtables. **So retail's
all-zero `__vt__8IFactory` and retail's base-vptr store are two facts that this header cannot
satisfy at once**, and that is a real blocker rather than a missing optimisation flag.
`docs/research/paks.md` is where the next lane should write it up.

## The integration of `CGameGlobalObjects`' constructor (2026-09-26, lane `chain`)

**`CGameGlobalObjectsCtor.cpp` and the `CGameState` default-construction chain are in the port
build, and `tools/boot_probe.sh` gets past `gpGameState is null` to `CGameArchitectureSupport`'s
constructor (step 17).** `tools/link_check.sh` goes **315 -> 322**, and `link_gap.py`'s MISSING
311 -> 318. That is +7 against the lane's target of neutral, and the rest of this section says why
it stops there.

The integration is `docs/research/cgameglobalobjects_ctor.md`'s patch (nine units listed and
`gameGlobalObjects = new CGameGlobalObjects(*osContext, *memorySys)` in `PortBoot.cpp`). On this
tree it opened twelve and closed none (**315 -> 327**). Five of the twelve are closed by real bodies
in the same change:

| symbol | closed by |
| --- | --- |
| `CSimplePool::CSimplePool(IFactory&)`, `CSimplePool::~CSimplePool()` | `src/Kyoto/CSimplePoolPort.cpp`, port-only: the constructor, the destructor and all nine virtuals in one file, so GCC emits `vtable for CSimplePool` with it. A tag-keyed table of `CObjectReference`s over the header's `rstl::hash_map` buckets. **The prior lane's reason for declining `~CSimplePool` is resolved**: the destructor opens the vtable only while nothing defines the key function, and this file defines it. |
| `CVParamTransfer::Null()` | the same file. Already missing before the integration, so it is the -1 |
| `fn_80009AC0` | `SGameStateMemcardBufFill.cpp` listed (`NonMatching` 85.20%), with `lbl_80417D92` = 1 read from the DOL into `PortGlobals.cpp` |
| `fn_8000934C` | `CPlayerStateRefRelease.cpp` listed, with a host `__dt__12CPlayerStateFv` (MWCC deleting-destructor convention over `CPlayerState::~CPlayerState()`) in `PortGlobals.cpp` |

**The eight that are left**, all of them real work rather than stubs:

| symbol | owner | reached at boot? | what closes it |
| --- | --- | --- | --- |
| `fn_80145C98` | the `CGameState` chain lane (`CPersistentOptionsInit.cpp`, `Matching`, in flight) | yes, once | listing it. Measured: **+2 net** as it stands (opens `fn_80145ACC`, `SPersistentOptionsValue(int,int,int)` and `lbl_803A9208`, the guest `.rodata` pool its eleven names are offsets into, which the host has to have as real strings) |
| `fn_80145628`, `fn_80145A2C` | the chain lane | no (`gpMemoryCard` is null) | 0x20 and 0xA0; their only callees are `fn_80145ACC` and `fn_801462DC`, which `fn_80145C98` needs too |
| `fn_8014306C`, `fn_801437DC`, `fn_80180430` | the chain lane | no (`gpMemoryCard` is null) | `fn_801440C0`'s other three callees. Unlisting `CGameStatePlayerLoop.cpp` would read 2 lower and hide them behind one symbol the boot *does* reach, with the same work still owed; not done |
| `CCharacterFactory::CCharacterFactory(CSimplePool&, ...)`, `~CCharacterFactory()` | unassigned | no | `CCharacterFactoryBuilder::CDummyFactory::Build` is a vtable slot, so listing the builder names them. `fn_80030410` is 0x560 with ~20 unwritten direct callees and the class is still an opaque `x4_unk[0x88]`. Leaving the builder unlisted instead is 1 lower, and the boot then runs a null `CCharacterFactoryBuilder` |

**`fn_8029c7e8` is not a `CSimplePool` method** even though `symbols.txt` gives it that mangling:
at 0x8029C7E8 it takes the store in `r3` and calls `GetObj(tag)` through vtable slot 0xC, among
the audio code. It is still missing and was before this change.

> **Superseded 2026-09-26 (later), annotated rather than rewritten:** the `REL module
> loaders` count in the table above is now **72**, because ten sibling carve units joined
> `files.cmake` after that paragraph was written. The reasoning is kept because it is the
> only record that a tentative `extern` definition with no initialiser is bound as a
> `FUNC` and placed in **`.text`** - the check for that class of bug is `readelf -sW`, and
> a data symbol showing as `FUNC` in `.text` is it. The count was the part that went
> stale; the finding did not. (This note is below the table on purpose:
> `tools/check_docs_claims.py` slices the table from its header to the first blank line,
> so a blank line *inside* the table silently reduces the check to the rows above it.)

### The carve batch's one new symbol, and why it is not excluded

`CGunWeapon::IsLoaded` appears because `Weapons/CGunWeaponIsLoaded.cpp` is a `Matching` unit in
`files.cmake`. It is not on the boot path - a weapon accessor is reached when a weapon is equipped,
and the boot stops at step 17 before that. So the honest entry is a cost, not an exclusion:

| symbol | owner | reached at boot? | what closes it |
| --- | --- | --- | --- |
| ~~`CGunWeapon::IsLoaded`~~ | **REMOVED from the list — it was never missing.** An independent review found this entry spurious: `PortLinkStubs.cpp` already defined the symbol, which is exactly why the carve needed a hand deletion from the stub file in the same commit. **The batch opens no link symbol at all**, and the absolute figures are **317 -> 315**, not 315 -> 313. The unit still stays listed — it is a real `Matching` function — it just was never missing, so listing it cost nothing. |

**This is the shape to prefer: a carve that is net-negative on the gap goes in, and its one
positive line is written down rather than hidden.** The alternative - excluding units that open a
symbol - optimises the count against the frontier, and the frontier is what the count is a proxy
for.

### The four `CPatternedAiFunctions` trigger functions opened (2026-09-30, lane L1)

Decompiling four more of `CPatternedAiFunctions`' trigger/AI functions (`CPatterned::Dead`,
`NoPathNodes`, `fn_801524fc`, `IsOnScreen`; all four now measure 100% against retail) calls four
functions whose units are `NonMatching` and therefore not in the port build. They are **not** boot
path, so this is a cost, not an exclusion, and it is written down:

| symbol | owner | reached at boot? | what closes it |
| --- | --- | --- | --- |
| `_ZNK14CBodyStateInfo15GetCurrentStateEv` | `MetroidPrime/BodyState/CBodyStateInfo.cpp` | no - only reached from `CPatterned::Dead` | matching that unit |
| `_ZN15CPathFindSearch6SearchERK9CVector3fS2_` | `MetroidPrime/PathFinding/CPathFindSearch.cpp` | no - only from `CPatterned::fn_801524fc` | matching that unit |
| `_ZNK15CPathFindSearch6OnPathERK9CVector3f` | `MetroidPrime/PathFinding/CPathFindSearch.cpp` | no - only from `CPatterned::NoPathNodes` | **closed 2026-10-01** - `src/MetroidPrime/PathFinding/CPathFindOnPath.cpp` defines it and the four functions it calls; see below |
| `_ZNK11CGameCamera20ConvertToScreenSpaceERK9CVector3f` | `MetroidPrime/Cameras/CGameCamera.cpp` | no - only from `CPatterned::IsOnScreen` | matching that unit |

**246 MISSING**, against a recorded `link_check` baseline of 250 undefined, so the count gate
stays green - but the *set* gate is what caught this, not the count: `link_gap.py` fails on any
missing symbol absent from `port_link_gap_list.md` regardless of the total. Recorded here because
that is the mechanism to expect next time: writing a matching function is nearly always a net
positive on the gap, and the list is where the cost is recorded.

### Closing one of the four without matching its unit (2026-10-01)

`_ZNK15CPathFindSearch6OnPathERK9CVector3f` is now defined for the port, by
`src/MetroidPrime/PathFinding/CPathFindOnPath.cpp`: `OnPath` plus the four functions it calls
(`CPFArea::FindRegions`, `CPFArea::GetOctreeRegionList`, `CPFAreaOctree::GetRegionList`,
`CPFAreaOctree::GetChildIndex`), bodies copied verbatim from the two `NonMatching` units that
already hold them. Its remaining callee `close_enough` and the three `CPFRegion` predicates it
reaches are in `Kyoto/Math/CloseEnough.cpp` and `PathFinding/CPathFindRegion.cpp`, both already
listed, so **nothing new is opened**: the list is 245 MISSING and `link_check` 250 -> **249**.

**The row above therefore did not need "matching that unit", which is worth recording because it
is the cheaper of the two fixes and the one the `NonMatching` row would have hidden.**  The
obstacle was never that the body was unwritten - it was written and measures 96.29% - but that
`CPathFindSearch.cpp` is a whole 26728-byte `NonMatching` unit whose other members open 12
symbols and close none, so listing it is a net rise. The port's pattern for that is a
closed-group file per function, the same one `CGameAreaSetAreaAttributes.cpp` and
`CGrappleArmReturnToDefault.cpp` use, and the *closure* of the function is what has to fit in
it, not just the function. `OnPath`'s closure is five functions; `CGameCamera::ConvertToScreenSpace`
and `CPathFindSearch::Search` are the other two of the four, and neither has been measured this
way yet.

## What this does not tell you

The 106 symbols attributed to Aurora's sources are attributed because the identifier appears
in a file under `extern/aurora/lib`. That is strong evidence, not proof - a name in a source
file is not the same as a definition in an object. **The authoritative answer is an actual
link**, and the only symbol where the distinction is already known to bite is `AIStartDMA`,
which appears in an Aurora *header* and in none of its sources. Do not treat the 106 as
resolved until a link has succeeded.

