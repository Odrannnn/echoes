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
-2 and +4. **Adding a file to `files.cmake` is not only a win**: a body that calls retail
functions nothing implements turns one closed symbol into four open ones. The table's per-group
counts were stale before this and are now derived from the list:

| group | count | what closes it |
| --- | --- | --- |
| other game methods | 201 | decompilation, one function at a time. This is the honest remainder. **+1 again on 2026-09-26 (lane `k3`), and this time it is `OnMessage` leaving and its two callees arriving:** `CMainFlow::OnMessage` (retail `fn_8001DF54`, 180 bytes) is a `Matching` unit, so the port *has* it - and it calls `AdvanceGameState` and `SetGameState`, which it does not. Those are retail's `fn_8001DE68` (224 bytes, a jumptable at `0x803B178C` on `x14_gameState`) and `fn_8001DB54` (788 bytes, writes the state at `+0x14` and switches on it); `symbols.txt` now names them so the `Matching` unit can call them, and 1,012 bytes of decompilation stand between the port and the next frame. That trade is worth making and the sign is the point: the linker now asks for two *named* functions instead of one. **+1 earlier the same day (lane `j3`), and that +1 was also the point:** writing `~CIOWin` and `~CMainFlow` as `Matching` units takes `vtable for CIOWin` and `vtable for CMainFlow` off the link for good, and `CMainFlow`'s vtable has an `OnMessage` slot, so the port's link now asks for `CMainFlow::OnMessage` **by name** instead of for a vtable that named nothing. `CResFactory`'s and `CSimplePool`'s vtables are still in the c++ runtime bucket because nothing in the tree defines their key functions either. See `docs/research/boot_probe.md`. **-14 on 2026-09-26 (lane `h1`):** the whole audio stack left the list - `CAudioSys`'s constructor, destructor and ten methods, and `CStreamAudioManager::SetSfxVolume`/`SetMusicVolume`. Twelve of the fourteen are also `Matching` units in `src/Kyoto/Audio/`; the port's own bodies for all fourteen are in `src/MetroidPrime/PortAudio.cpp`, and **eleven of them are reached before the game's first frame**, so they could not be stubbed. See `docs/research/audio_stack.md`. **-2 earlier the same day (lane `g1`):** `CResLoader::GetPakCount` and `CResLoader::GetPakFile` left the list, `GetPakCount` because `src/Kyoto/CResLoaderGetPakCount.cpp` is a `Matching` unit at 100.00% and `GetPakFile` because `src/Kyoto/CResLoaderGetPakFile.cpp` now exists and is in the port build at 80.13% - a symbol the port *compiles a body for* is no longer missing, whether or not the body is retail's. Both needed `include/Kyoto/CResLoader.hpp` to model `CResLoader` correctly first; see `docs/research/paks.md`. || REL module loaders | 72 | **all 159 entity loaders are identified and 72 are landed** - see `docs/research/rel_loaders.md`, which has every address, size and dispatch global. What is left is 86 real loaders of 288..3,640 bytes (**77,500 bytes, ~25x the thunk family**), the 68 `LoadTypedefSLdr*` instantiations of one template, and 7 helpers. No unidentified symbols remain in this group |
| REL module loaders | 72 | **all 159 entity loaders are identified and 72 are landed** - see `docs/research/rel_loaders.md`, which has every address, size and dispatch global. What is left is 86 real loaders of 288..3,640 bytes (**77,500 bytes, ~25x the thunk family**), the 68 `LoadTypedefSLdr*` instantiations of one template, and 7 helpers. No unidentified symbols remain in this group |
| unmangled: fn_*, lbl_*, globals | 30 | functions and labels nobody has identified. **+3 on 2026-09-26 (lane `m3`), and the sign is the point again:** `CResFactory::Build` is a `Matching` unit and in the port build, so `_ZN11CResFactory5BuildERK10SObjectTagRK15CVParamTransfer` left the list and its three retail callees - `fn_802FAAE4`, `fn_802FA1BC`, `fn_802FA7D4` - arrived. See "The three that `CResFactory::Build` brought with it" below. **+12 on 2026-09-26 (lane `k2`), and the direction is worth naming: the count went *up* in the same session that made two functions byte-exact.** Writing `CMainFlow::SetGameState` and `CMainFlow::AdvanceGameState` as a `Matching` unit (`src/MetroidPrime/CMainFlowDtor.cpp`, 1,108 bytes of `.text` and 96 of `.data`, 3/3 at 100.00%) put twelve new retail callees on the port's link: the six window constructors `fn_80020478`, `fn_800214A0`, `fn_80022C74`, `fn_80192808`, `fn_80193E08`, `fn_801F47F4`, the message factory `fn_80048EA4`, the four game-state helpers `fn_801423A8`, `fn_80143884`, `fn_80143E88`, `fn_80180598`, and `StreamNewGameState__5CMainFR12CInputStreami` (0x800053B8, which is `CMain::StreamNewGameState` - a mangled name, so it lands here rather than in the group above). All twelve are **named now**, which is the improvement: `docs/research/unidentified.md` exists to be emptied. What closes them is their own decompilation; none can be stubbed, because `SetGameState` is the only thing that creates the front-end and game windows and it runs before the game's first frame. The six `.sdata` words the same unit reads (`lbl_80417DE0`..`lbl_80417DF4`) did **not** reach this list - they are defined with retail's values in `src/MetroidPrime/PortGlobals.cpp`, the same treatment `lbl_803A60A0` gets. **This group grew 23 -> 61 when the 96 omitted units were added to the port build**: compiling code that references retail symbols we do not define surfaces new unnamed ones, so adding a file is not only a win. `docs/research/unidentified.md` has 22 of the original 23 named |
| ~~static data members~~ | 0 | **closed 2026-09-25** - see the section below |
| ~~`rstl` templates~~ | 0 | **closed 2026-09-25** - see the section below |
| ~~`SLdr*` script-loader struct constructors~~ | 0 | **closed 2026-09-25.** "One generator, all trivial in retail" was wrong twice over - see below |

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
| static data members | 0 | **closed 2026-09-25** - see the section below |
| `TypesMatch` overrides | 0 | **closed 2026-09-25** - see the section below |
| `rstl` templates | 0 | **closed 2026-09-25** - see the section below |
| ~~`SLdr*` script-loader struct constructors~~ | 0 | **closed 2026-09-25.** "One generator, all trivial in retail" was wrong twice over - see below |

So the shape of the remaining work is **302 decompilation proper, 302 unidentified, and 0 already done**.** That is a different project from "close 63 symbols", and
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

## What this does not tell you

The 106 symbols attributed to Aurora's sources are attributed because the identifier appears
in a file under `extern/aurora/lib`. That is strong evidence, not proof - a name in a source
file is not the same as a definition in an object. **The authoritative answer is an actual
link**, and the only symbol where the distinction is already known to bite is `AIStartDMA`,
which appears in an Aurora *header* and in none of its sources. Do not treat the 106 as
resolved until a link has succeeded.

