# `CGameGlobalObjects::AddPaksAndFactories`, and what depends on it

Written 2026-09-26 by lane e3 from a build of commit `4d49561`. Every address, size and string
below is read out of `build/G2ME01/main.elf` with `build/binutils/powerpc-eabi-objdump` and
`powerpc-eabi-nm`, or out of `config/G2ME01/symbols.txt`. Nothing here is recalled.

`docs/research/boot_path.md` maps the boot path and calls this function "step 13 - the keystone
of the resource system". This file is the block-by-block map of step 13 and of step 16
(`CMain::StreamNewGameState`), and it exists because the two things a lane needs before it
starts - which callees exist and which do not, and in what order the paks are loaded - are not
derivable from the header.

## The answer first: the dependency order is a cycle, and that is the finding

```
  CResFactory / CSimplePool / gpResourceFactory
        |  constructed in CGameGlobalObjects::CGameGlobalObjects (step 7, a stub)
        v
  CGameGlobalObjects::AddPaksAndFactories        0x80007168, 0x790 = 1,936 B
        |  puts the paks in and the entity factories in
        v
  CMain::FillInAssetIDs                          0x80006B38, 0x48  (written)
  CMain::StreamNewGameState                      0x800053B8, 0x214 = 532 B
        |  reads a persistent-options record out of a pak, via the loader step 13 filled
        v
  gpGameState
        |
        v
  CGameArchitectureSupport::CGameArchitectureSupport  0x80007EC4, 0x378
        |  lwz r3,-28360(r13) at 0x800081A4, then CGameOptions::EnsureOptions at 0x800081AC,
        |  with no null test
        v
  the frame loop
```

Three measurements make that cycle concrete rather than asserted:

1. **`AddPaksAndFactories` is what makes `FillInAssetIDs` work**, and that is already in the
   tree: `FillInAssetIDs` does `gpSimplePool->fn_8029c7e8(*gpResourceFactory->GetResourceIdByName(...))`
   and both globals are filled by step 13's class, whose constructor is still a stub.
2. **`StreamNewGameState` reads its record out of a pak.** The record is 16 bytes, `{u32, void*
   data at +0x04, u32, u32 size at +0x0C}` (`lwz r4,12(r5)` / `lwz r5,4(r5)` at
   0x80005488/0x80005490), it is indexed out of a `CGameState`-owned array
   (`slwi r0,r27,4; addi r5,r1,128; add r5,r5,r0` at 0x80005474-0x8000547C, so stride 16), and
   it is handed to `CMemoryInStream(const void*, unsigned long)` (0x802FFF04) and then to
   `CBitStreamReader(CInputStream&)` (0x80342F58). Nothing else could produce it. So step 13
   is a *hard* prerequisite: without the paks there is no record, and without the record
   `gpGameState` is never assigned.
3. **`gpGameState` is what step 17 dereferences without a test** (`lwz r3,-28360(r13)` at
   0x800081A4), which `boot_path.md` already identifies as the wall. So the wall is not a
   missing-code wall, it is this cycle, and the cycle does not break by writing any one of the
   three functions.

The one thing that breaks it in the other direction is `CResLoader::AddPakFileAsync`
(0x802FC268, 0xE8) - the function every one of the nine pak loads calls. It is already on the
link-gap ratchet, called from the *written* `CMain::AddWorldPaks` (0x80005968, 0x180). **It is
the smallest single unblock on the whole path: 232 bytes, already named, already referenced, and
nothing else in this file can do anything until it exists.**

## The eleven pak names, and where they live

All eleven are in one concatenated `.rodata` pool based at **0x803A56C0**, which begins
`"??(??).."` / `"%d"` / `".pak"` - the `??(??)..` is the placeholder `CMemory.hpp:33` passes to
the allocating `operator new`, so this pool is a shared literal pool, not a table. The
`AddWorldPaks` names live in the same pool.

| offset from 0x803A56C0 | string | retail use |
| --- | --- | --- |
| +112 (0x70) | `Strings.pak` | `CDvdFile::FileExists` probe, 0x800071A8 |
| +176 (0xB0) | `aram:Strings` | pak name, 0x800071C4 |
| +189 (0xBD) | `Standard.NTWK` | `CDvdFile` name, 0x800071F4 |
| +203 (0xCB) | `Tweaks.rel` | unnamed, 0x80007258 |
| +214 (0xD6) | `NoARAM` | pak name, 0x8000728C |
| +221 (0xDD) | `AudioGrp` | pak name, 0x800072BC |
| +230 (0xE6) | `aram:MiscData` | pak name, 0x800072EC |
| +244 (0xF4) | `aram:TestAnim` | pak name, **buildDepList = true**, 0x8000731C/0x8000732C |
| +258 (0x102) | `aram:MidiData` | pak name, 0x8000734C |
| +272 (0x110) | `aram:GGuiSys` | pak name, 0x8000737C |
| +285 (0x11D) | `FrontEnd.pak` | `CDvdFile::FileExists` probe, 0x800073A8 |
| +298 (0x12A) | `FrontEnd` | pak name, **worldPak = true**, 0x800073C4/0x800073D8 |

`Aram` is spelled `aram:` in lowercase on five of them, which is retail's own inconsistency and
is worth copying exactly. `CMain::AddWorldPaks` (already written, 96.00%) builds the *world*
pak names from `gpTweakGame->GetPakFile()`, not from this pool - the two sets are disjoint.

## `AddPaksAndFactories`, block by block

Retail 0x80007168, 0x790 = 1,936 bytes, a 288-byte frame. `this` in r3 (unused after the
prologue). **SUPERSEDED 2026-09-27: there is no `COsContext&`.** `symbols.txt` names it
`AddPaksAndFactories__18CGameGlobalObjectsFv` - **`Fv`, no parameters** - and `PostInitialize` calls
it at 0x80008404 with no argument shuffling. So `mr r30,r4` is retail reading a **dead argument
register**, and `IController::Create` is handed a value retail's own front end cannot name.
Reproducing that first block needs a parameter retail does not have, so it is **not writable**.
`gpResourceFactory` (0x80418EA4) is loaded once into r31 at 0x8000718C and every pak load uses
`r31+4` - the `CResLoader`, which is at `CResFactory`+0x04 because `IFactory`'s vptr is at
+0x00. `gpResourceFactory` is itself `CGameGlobalObjects`+0x04, so the `CResLoader` is at
`CGameGlobalObjects`+0x08 and the first member of `CGameGlobalObjects` is four bytes. (The last
sentence used to say the loader "starts at +0x00 of `CGameGlobalObjects`", which was wrong by
0x08; see the third correction at the end of this file.)

| # | retail range | bytes | what | written? |
| --- | --- | --- | --- | --- |
| 1 | 0x80007170-0x800071A0 | 48 | `CGraphics::SetViewPointMatrix(sIdentity__12CTransform4f)` then `SetModelMatrix` on the same object. `sIdentity__12CTransform4f` is .bss 0x804173D4. Both are **static**, which is why there is no `this` load before either | **yes** |
| 2 | 0x800071A0-0x800071E8 | 72 | `if (CDvdFile::FileExists("Strings.pak")) resLoader.AddPakFileAsync("aram:Strings", false, false);` | **yes** |
| 3 | 0x800071E8-0x80007278 | 152 | the `Standard.NTWK` read: `CDvdFile("Standard.NTWK")`, a `CCallStack`, a 2-byte `CMemory::Alloc`, `CDvdFile::SyncRead` into it, a `CMemoryInStream`, and five unnamed helpers | **no** |
| 4 | 0x80007280-0x800073A4 | 292 | six unconditional `AddPakFileAsync` calls: `NoARAM`, `AudioGrp`, `aram:MiscData`, `aram:TestAnim` (**true**, false), `aram:MidiData`, `aram:GGuiSys` - all with the third argument 0 | **yes** |
| 5 | 0x800073A4-0x800073E8 | 68 | `if (CDvdFile::FileExists("FrontEnd.pak")) resLoader.AddPakFileAsync("FrontEnd", false, **true**);` - the only world pak here | **yes** |
| 6 | 0x800073E8-0x80007418 | 48 | `CErrorOutputWindow(true)` on the stack at r1+200, `fn_802BE8E8(1)`, and `CGraphics::SetViewport(0, 0, mViewport.mWidth, mViewport.mHeight)` - the two widths are `lwz r5,8(r6)` / `lwz r6,12(r6)` off `mViewport__9CGraphics` (.data 0x803B9FE8), and note the first two arguments are literal 0, **not** `mViewport.mLeft`/`mTop` | **no** |
| 7 | 0x80007418-0x800074BC | 164 | `gpController = IController::Create(osContext)`, the store to 0x804192F0, then the **whole load loop** (its back-edge is the `beq 0x80007430` at 0x8000748C and it is entered once by the `b 0x80007480` at 0x8000742C): `fn_802FCCE4(&resLoader)` and, **while it is FALSE**, `fn_802FCCF4(&resLoader)`, `fn_801F05D0(lbl_80418EC8)`, `fn_80180EC0(&err)`, `fn_802C1E60()`, `fn_80180E94(&err)`, `fn_802C1658()`, `CMain::CheckReset()`, a vcall on the `CDvdRequest*` at r1+12 (slot +0x10) and `fn_801F025C(&r1+0x1C)` | **yes** - both pump functions; see the correction below |
| 8 | 0x800074BC-0x80007504 | 72 | `gpController = nullptr`, then the record choice + `CMemoryInStream` + `CBitStreamReader` + `operator new(752, "??(??)..", 0)` | **no** (see `StreamNewGameState` below) |
| 9 | 0x80007504-0x80007864 | **864** | **all 36 factory registrations** - 44.6% of the function | **no**, see the table below |
| 10 | 0x80007864-0x800078F8 | 148 | controller vcall at vtable slot +0x08 with argument 1, `fn_80049E30(&err)` after rewriting the vtable word at r1+200 to 0x803B5910, `fn_801F0308(&mis, -1)`, `~CMemoryInStream`, `CMemory::Free(buf)`, `~CDvdFile` | **no** |

Blocks 1, 2, 4 and 5 are written: **480 of 1,936 bytes** (48 + 72 + 292 + 68), plus the 8-byte
prologue - which is 23.17% of the function, and 480 of the 480 bytes of the blocks whose every
callee already exists in the port or is already on the ratchet. The ten block lengths above sum
to 1,928 and the prologue is the other 8, so the table is the whole function. Everything not
written is a block whose every callee is either unnamed in `config/G2ME01/symbols.txt` and
unwritten, or one of the 36 factories below.

### The `CErrorOutputWindow` and controller blocks are one loop, not two

`CErrorOutputWindow(true)` is constructed at r1+200, and the `fn_80180EC0`/`fn_802C1E60`/
`fn_80180E94`/`fn_802C1658` quadruple is the IOWin pump `CErrorOutputWindow` uses while the
paks load. The vtable word at r1+200 is rewritten to 0x803B5910 before `fn_80049E30`, which
means retail is swapping that window's behaviour at the end of the loop - worth knowing before
anyone writes the four helpers.

### Block 3's locals, for whoever writes it

The frame layout, so a lane does not have to re-derive it: `r1+20` an 8-byte `CCallStack`,
`r1+28` a 0x24-byte object, `r1+52`/`68`/`84`/`100`/`116`/`132`/`148`/`164` eight 16-byte
`rstl::string`s, `r1+180` a `CMemoryInStream`, `r1+200` a `CErrorOutputWindow`, `r1+232` a
20-byte `CDvdFile`, `r1+252` the `CMemory*` that `CMemory::Alloc` is called on, `r1+256` the
`void*` result, `r1+12` the `CDvdRequest*` from `SyncRead`. `lwz r29,252(r1)` at 0x80007208 is
*inside* the `CDvdFile` (r1+232+0x14) and is read straight after the constructor returns, so the
`CMemory*` is a `CDvdFile` member at +0x14 - **not** an incoming stack argument, which is worth
saying because the frame is 288 bytes and an incoming argument would be at +288 or more.

`CCallStack`'s retail constructor (0x8028BFE8, 8 bytes) is two stores and ignores its first
argument: `stw r5,0(r3); stw r6,4(r3)`. So retail passes `(CCallStack*, -1, 0x803A56C0,
0x803FEAB8)` and the object holds two pointers. `0x803FEAB8` is in `.bss`, not `.rodata`, so it
is not a format string - the port's `CCallStack::CCallStack(uint, const char*, const char*)` is
already on the ratchet and this is what it is.

## The 36 factory registrations - the whole table, and they ARE written now

**Updated 2026-09-26 by lane h5.** The 36 registrations are now in
`src/MetroidPrime/main.cpp`, and `CGameGlobalObjects::AddPaksAndFactories` is **57.15%**
(23.17% before). Three claims in this section were wrong and are corrected in place below;
`docs/HANDOFF.md`'s state block and `docs/RUNNING_THE_DECOMP.md` are the other two places the
old figures were repeated.

864 bytes, exactly 24 bytes each: `lis r3` / `lis r4` / `addi r5,r3,N` / `addi r3,r31,116` /
`addi r4,r4,N` / `bl`.

* **r3 is `CResFactory`+0x74, and that is `CFactoryMgr`+0x00 - not +0x10.** `CFactoryMgr` is
  **0x38** bytes at `CResFactory`+0x74 and ends at +0xAC; `CResFactory` is **0xE0** bytes. Both
  are measured: `addi r3, r31, 116` appears 36 times with `r31` = `gpResourceFactory`, and
  `CGameGlobalObjects::CGameGlobalObjects` (0x800084AC-0x800084B8) puts the member it builds
  after the factory at `this+0xE4` with the factory itself at `this+0x04`. The `+0x64` this
  section used to carry, and the `+0x5C` before it, both came from a `CResLoader` modelled too
  small; `CResLoader` is **0x70** bytes (four 0x18 lists at +0x00/+0x18/+0x30/+0x48, then four
  unnamed words), so `CResFactory`+0x74 = `CResLoader`+0x70. **`include/Kyoto/CResFactory.hpp` and
  `include/Kyoto/CResLoader.hpp` now carry the measured numbers** and
  `CHECK_SIZEOF(CResFactory, 0xd0)` is gone - it is `0xe0`. (This paragraph said `0xe4` when it
  was written; the third correction at the end of this file is what fixes it.) This is the fix
  the adjudication section below asks for, and it is the one change here that was *not* confined
  to the factory block: nothing else in the tree read those members, and the gate's per-function
  diff is unmoved (3131 -> 3131 matched), which is the measurement that says so.
* **r4 is a FourCC built big-endian and r5 is the factory's address** - and the paragraph's
  "`addi r5, r3, N`" order above is retail's, but the *roles* are the other way round from what
  the first version of this file said: the FourCC immediate is materialised in **r5** and the
  factory's address in **r4**.
* **The block adds no data.** The FourCC is an immediate, never a string, so a `Matching` unit
  could contain this block - which is what makes the 33 factories worth writing.

**The cost, measured: net 0 on the port's link gap, not +34.** Writing the 36 as bare calls is
a 34-symbol regression (33 unnamed `fn_*` factories plus `FStringTableFactory`; gross closed 0,
gross opened 34). They are written anyway because `include/Kyoto/CFactoryFunctions.hpp` declares
all 36 - with C linkage and typed parameters, so the emitted symbol name is retail's spelling
verbatim and **no rename in `symbols.txt` is needed and no REL module can be disturbed** - and
`src/Kyoto/CFactoryFunctionsPort.cpp` gives the 33 unnamed ones a body, so the symbols resolve.
`link_gap.py`: **309 before, 309 after**. `link_check.sh`: **340 undefined before, 340 after**.

The one that is missing for a reason worth knowing is `FDependencyGroupFactory`:
`src/Kyoto/CDependencyGroup.cpp` defines it and `CDependencyGroup` is a `Matching` DOL unit
(11/11), but that file is not in `files.cmake`, so the port build has no definition of it. That
is a one-line `files.cmake` change, not decompilation. `FRuleSetFactory` is already in the port
via `src/MetroidPrime/CRuleSet.cpp`, and `FStringTableFactory` is **not**: its only definition
is in `src/Kyoto/Text/CStringTable.cpp`, which `files.cmake` deliberately excludes for two
pointer-to-`uint` casts, so `CFactoryFunctionsPort.cpp` has to supply a fourth body.

The two registrars - **and the `+0x14` in the `fn_802F96E0` row below was wrong**:

| symbol | address | how many | how to tell them apart | map it writes |
| --- | --- | --- | --- | --- |
| `fn_802F96E0` | 0x802F96E0 | 33 | `fn_802F9378(out, this+0x00, fourCC)` | `this+0x00` |
| `fn_802F963C` | 0x802F963C | 3 | `fn_802F9428(out, this+0x14, this)` - the manager, not the FourCC | `this+0x14` |

`fn_802F96E0` finds at `this+0x00` (`mr r4, r29` at 0x802F9708) and inserts at `this+0x00`
(`mr r4, r29` again at 0x802F9750, with the map's own header at `this+0x08` -
`addi r3, r29, 8` at 0x802F9720). `fn_802F963C` uses `addi r4, r29, 20` for **both** the find
(0x802F966C) and the insert (0x802F96B0), i.e. `this+0x14`. So the first of those two rows is
`+0x00` on both counts, and `include/Kyoto/CFactoryMgr.hpp` - a `Matching` unit at 100% - was
already right about which map is which.

**The two tables hold different function-pointer types, and only the second one is `CFactoryFn`.**
The only callers of these 36 pointers are the two dispatch sites, and they set a different
number of words:

```
  fn_802F94D8  (FourCC-keyed)  0x802F9518  mr r4,r29 / 0x802F9520 mr r5,r30
                                0x802F9524  mr r6,r31  / 0x802F952C addi r3,r1,12
                                0x802F9530  mtctr r12 / bctrl
      -> r3 (out), r4, r5, r6      : 3 declared arguments
  fn_802F8EB0  (owner-keyed)   0x802F8FDC  mr r4,r31 / 0x802F8FE0 mr r6,r26
                                0x802F8FE4  mr r7,r28 / 0x802F8FEC addi r5,r1,112
                                0x802F8FF0  mtctr r12 / bctrl
      -> r3 (out), r4, r5, r6, r7  : 4 declared arguments
```

`r3` is the hidden return slot in both, because `CFactoryFnReturn` holds an `rstl::auto_ptr` and
cannot come back in registers. **CMDL, AGSC and PATH - the three `fn_802F963C` entries - are the
only three that read the fourth argument**, and CMDL is the proof: 0x80311348 is
`lwz r4, 4(r7)`. `CFactoryFnOwner` is now a separate typedef in
`include/Kyoto/CFactoryMgr.hpp`, and `x14_factoriesByOwner` is a `map<uint, CFactoryFnOwner>`.
The header's `CFactoryFn` was right for 33 of the 36 and wrong for the map's second half.

The three that use `fn_802F963C` are **CMDL, AGSC and PATH** and no others.

| retail call | FourCC | factory function | at |
| --- | --- | --- | --- |
| `fn_802F96E0` | `STRG` | `FStringTableFactory(const SObjectTag&, CInputStream&, const CVParamTransfer&)` | 0x80312320 |
| `fn_802F963C` | `CMDL` | `fn_80311340` | 0x80311340 |
| `fn_802F96E0` | `TXTR` | `fn_802C4878` | 0x802C4878 |
| `fn_802F96E0` | `CSKR` | `fn_8030FEAC` | 0x8030FEAC |
| `fn_802F96E0` | `ANIM` | `fn_802B3200` | 0x802B3200 |
| `fn_802F96E0` | `CINF` | `fn_802AC2D8` | 0x802AC2D8 |
| `fn_802F96E0` | `ANCS` | `fn_8028E7BC` | 0x8028E7BC |
| `fn_802F96E0` | `CRSC` | `fn_8025DD1C` | 0x8025DD1C |
| `fn_802F96E0` | `SWHC` | `fn_802ED864` | 0x802ED864 |
| `fn_802F96E0` | `PART` | `fn_802E7A78` | 0x802E7A78 |
| `fn_802F96E0` | `ELSC` | `fn_8031B4D8` | 0x8031B4D8 |
| `fn_802F96E0` | `SPSC` | `fn_8032B5DC` | 0x8032B5DC |
| `fn_802F96E0` | `SRSC` | `fn_8032F0D4` | 0x8032F0D4 |
| `fn_802F96E0` | `WPSC` | `fn_8025DB38` | 0x8025DB38 |
| `fn_802F96E0` | `FRME` | `fn_80274FD4` | 0x80274FD4 |
| `fn_802F96E0` | `FONT` | `fn_802B514C` | 0x802B514C |
| `fn_802F96E0` | `SCAN` | `fn_80110B18` | 0x80110B18 |
| `fn_802F96E0` | `AFSM` | `fn_8019405C` | 0x8019405C |
| `fn_802F96E0` | `FSM2` | `fn_801FD314` | 0x801FD314 |
| `fn_802F963C` | `AGSC` | `fn_80307544` | 0x80307544 |
| `fn_802F96E0` | `DCLN` | `fn_80254414` | 0x80254414 |
| `fn_802F96E0` | `DPSC` | `fn_802601D0` | 0x802601D0 |
| `fn_802F96E0` | `ATBL` | `fn_8029AB80` | 0x8029AB80 |
| `fn_802F963C` | `PATH` | `fn_8013FDB8` | 0x8013FDB8 |
| `fn_802F96E0` | `MAPW` | `fn_80093638` | 0x80093638 |
| `fn_802F96E0` | `MAPA` | `fn_8007E32C` | 0x8007E32C |
| `fn_802F96E0` | `MAPU` | `fn_801545F0` | 0x801545F0 |
| `fn_802F96E0` | `CSNG` | `fn_80314DC4` | 0x80314DC4 |
| `fn_802F96E0` | `DGRP` | `FDependencyGroupFactory(const SObjectTag&, CInputStream&, const CVParamTransfer&)` | 0x80320B6C |
| `fn_802F96E0` | `SAVW` | `fn_80182830` | 0x80182830 |
| `fn_802F96E0` | `HINT` | `fn_8017F988` | 0x8017F988 |
| `fn_802F96E0` | `CSPP` | `fn_8028B1F4` | 0x8028B1F4 |
| `fn_802F96E0` | `PTLA` | `fn_80255600` | 0x80255600 |
| `fn_802F96E0` | `STLC` | `fn_802FF4BC` | 0x802FF4BC |
| `fn_802F96E0` | `EGMC` | `fn_801EF598` | 0x801EF598 |
| `fn_802F96E0` | `RULE` | `FRuleSetFactory(const SObjectTag&, CInputStream&, const CVParamTransfer&)` | 0x801F6AE4 |

### The recipe for the 33, and two facts that were not in the table above (lane h5)

**The sizes are not all 0x64.** The line below that used to say "each is 0x64 bytes in retail" is
true of the **three retail-named** factories and of nothing else: the 33 unnamed ones are
0x64/0x68/0x6C/0x70/0x74/0x78/0x8C/0x9C/0xB0, because retail aligned the *named* ones and
nothing else. A unit's claimed range is therefore its own size, and the table below is the
authority.

**`CFactoryFnReturn<T>::CFactoryFnReturn(T*)` is always immediately after its factory**, at
`factory + size`, with no gap - checked on all 36: TXTR's is `fn_802C48E4` at 0x802C4878+0x6C,
CSKR's `fn_8030FF10` at 0x8030FEAC+0x64, ANIM's `fn_802B32B0` at 0x802B3200+0xB0, CRSC's
`fn_8025DDB8` at 0x8025DD1C+0x9C, and so on. So one `Matching` unit per factory is **two**
functions, `factory .. factory + size + sizeof(that ctor)`, and mwcceppc puts them in retail's
order by itself because the converting constructor is emitted from the factory's own body. That
was verified end to end on `FStringTableFactory`: a `Matching` unit
`src/Kyoto/Text/CStringTableFactory.cpp` claiming `.text 0x80312320..0x80312434` came out at
**100.00% on both functions with `tools/flip_test.sh` PASS and the DOL sha1 held** - and was then
**reverted**, for the reason in "Why the FStringTableFactory promotion was reverted" at the end
of this file.

**Every `operator new` file-string is a *named* `.rodata` symbol**, so a `Matching` unit can point
at it with `extern "C" const char lbl_803AFxxx[];` and own no data at all. That is the whole of
the "a Matching unit may not own data" problem for these functions, and it is the same trick
`docs/RUNNING_THE_DECOMP.md` describes for `lbl_8041E258` and `lbl_803A60A0`. The two exceptions
are `STRG`'s, which is `lbl_803AFEA8` and is inside the `.rodata` range
`Kyoto/Text/CStringTable.cpp` claims, and **`RULE`'s, which is `@stringBase0` at 0x803AC548 and is
`scope:local`** - a local symbol cannot be named from another translation unit, so `FRuleSetFactory`
would have to *own* those 8 bytes, and the attempt fails at the link with
``undefined: '@stringBase0_803AC548'`` because something else in the `CRuleSet` range still
refers to them. That is a measured negative, not a guess.

**Seven of the 36 do not allocate at all.** SWHC, PART, ELSC, SPSC, SRSC, WPSC and DPSC have no
`operator new`, a 32-byte frame, and end with `fn_80031D98` - the destructor of the
`CVParamTransfer`'s `{obj, count}` pair, which they bump with `stw` on `count`'s target before
handing it on. They are the ones a lane should write **last**: their body is not
`new T(stream)`, and the thing they build is named by a *static* function (`fn_802ED788` and
friends) rather than a constructor.

The whole table, measured from `build/G2ME01/main.elf` at each function's own `size` from
`config/G2ME01/symbols.txt`. `new` is the `li r3, N` operand. "calls" is in address order.

| FourCC | factory | size | frame | `new` | calls | `??(??)` literal |
| --- | --- | --- | --- | --- | --- | --- |
| `STRG` | `0x80312320` | 0x64 | 16 | 0x24 | `__nw__FUlPCcPCc`, `__ct__12CStringTableFR12CInputStream`, `__ct<12CStringTable` | - |
| `CMDL` | `0x80311340` | 0xB0 | 32 | 0x52 | `GXInvalidateVtxCache`, `__nw__FUlPCcPCc`, `fn_80311C34`, `fn_803113F0`, `fn_80031D98` | - |
| `AGSC` | `0x80307544` | 0x74 | 32 | 0x18 | `__nw__FUlPCcPCc`, `fn_80307108`, `fn_803075B8` | `lbl_803AFC48` |
| `PATH` | `0x8013FDB8` | 0x74 | 32 | 0x1EC | `__nw__FUlPCcPCc`, `fn_80140D88`, `fn_8013FE2C` | `lbl_803A91C0` |
| `TXTR` | `0x802C4878` | 0x6C | 16 | 0x104 | `__nw__FUlPCcPCc`, `fn_802C5C94`, `fn_802C48E4` | `lbl_803AF418` |
| `CSKR` | `0x8030FEAC` | 0x64 | 16 | 0x40 | `__nw__FUlPCcPCc`, `fn_803103BC`, `fn_8030FF10` | - |
| `ANIM` | `0x802B3200` | 0xB0 | 32 | 0x156 | `__nw__FUlPCcPCc`, `fn_802B34B8`, `fn_802B32B0`, `fn_80031D98` | - |
| `CINF` | `0x802AC2D8` | 0x64 | 16 | 0x124 | `__nw__FUlPCcPCc`, `fn_802ABF68`, `fn_802AC33C` | `lbl_803AEE10` |
| `ANCS` | `0x8028E7BC` | 0x64 | 16 | 0x124 | `__nw__FUlPCcPCc`, `fn_8028ED7C`, `fn_8028E820` | `lbl_803AED58` |
| `CRSC` | `0x8025DD1C` | 0x9C | 32 | 0x56 | `__nw__FUlPCcPCc`, `fn_8025EA5C`, `fn_8025DDB8`, `fn_80031D98` | `lbl_803ADD60` |
| `SWHC` | `0x802ED864` | 0x6C | 32 | - | `fn_802ED788`, `fn_802ED8D0`, `fn_80031D98` | - |
| `PART` | `0x802E7A78` | 0x74 | 32 | - | `fn_802E79F4`, `fn_802E7AEC`, `fn_80031D98` | - |
| `ELSC` | `0x8031B4D8` | 0x6C | 32 | - | `fn_8031B48C`, `fn_8031B544`, `fn_80031D98` | - |
| `SPSC` | `0x8032B5DC` | 0x6C | 32 | - | `fn_8032B500`, `fn_8032B648`, `fn_80031D98` | - |
| `SRSC` | `0x8032F0D4` | 0x6C | 32 | - | `fn_8032EFF8`, `fn_8032F140`, `fn_80031D98` | - |
| `WPSC` | `0x8025DB38` | 0x6C | 32 | - | `fn_8025DA5C`, `fn_8025DBA4`, `fn_80031D98` | - |
| `FRME` | `0x80274FD4` | 0x9C | 32 | 0x820 | `__nw__FUlPCcPCc`, `fn_80276140`, `fn_80275070`, `fn_80031D98` | `lbl_803AE4F0` |
| `FONT` | `0x802B514C` | 0x9C | 32 | 0x148 | `__nw__FUlPCcPCc`, `fn_802B58F8`, `fn_802B51E8`, `fn_80031D98` | `lbl_803AEE80` |
| `SCAN` | `0x80110B18` | 0x78 | 32 | 0x420 | `__nw__FUlPCcPCc`, `fn_80111264`, `fn_80110B90` | - |
| `AFSM` | `0x8019405C` | 0x64 | 16 | 0x32 | `__nw__FUlPCcPCc`, `fn_8019448C`, `fn_801940C0` | `lbl_803AA230` |
| `FSM2` | `0x801FD314` | 0x68 | 16 | 0x64 | `__nw__FUlPCcPCc`, `fn_801FDEE4`, `fn_801FD37C` | - |
| `DCLN` | `0x80254414` | 0x68 | 16 | 0x56 | `__nw__FUlPCcPCc`, `fn_802541EC`, `fn_8025447C` | - |
| `DPSC` | `0x802601D0` | 0x6C | 32 | - | `fn_8025FE8C`, `fn_8026023C`, `fn_80031D98` | - |
| `ATBL` | `0x8029AB80` | 0x68 | 32 | 0x16 | `__nw__FUlPCcPCc`, `fn_80255DA8`, `fn_8029ABE8` | `lbl_803AEDC0` |
| `MAPW` | `0x80093638` | 0x64 | 16 | 0x68 | `__nw__FUlPCcPCc`, `fn_80095ABC`, `fn_8009369C` | `lbl_803A7BD8` |
| `MAPA` | `0x8007E32C` | 0x9C | 32 | 0x124 | `__nw__FUlPCcPCc`, `fn_802FCB88`, `fn_80080124`, `fn_8007E3C8` | `lbl_803A7268` |
| `MAPU` | `0x801545F0` | 0x8C | 32 | 0x48 | `__nw__FUlPCcPCc`, `fn_8015566C`, `fn_8015467C` | `lbl_803A9568` |
| `CSNG` | `0x80314DC4` | 0x64 | 16 | 0x16 | `__nw__FUlPCcPCc`, `fn_80315084`, `fn_80314E28` | - |
| `DGRP` | `0x80320B6C` | 0x64 | 16 | 0x16 | `__nw__FUlPCcPCc`, `__ct__16CDependencyGroupFR12CInputStream`, `__ct<16CDependencyGroup` | - |
| `SAVW` | `0x80182830` | 0x64 | 16 | 0x132 | `__nw__FUlPCcPCc`, `fn_80182EC8`, `fn_80182894` | `lbl_803A9F68` |
| `HINT` | `0x8017F988` | 0x8C | 32 | 0x16 | `__nw__FUlPCcPCc`, `fn_801807A8`, `fn_8017FA14` | `lbl_803A9F30` |
| `CSPP` | `0x8028B1F4` | 0x64 | 16 | 0x32 | `__nw__FUlPCcPCc`, `fn_8028B624`, `fn_8028B258` | `lbl_803AEAB0` |
| `PTLA` | `0x80255600` | 0x70 | 16 | 0x80 | `__nw__FUlPCcPCc`, `fn_80255D2C`, `fn_80255670` | `lbl_803AD920` |
| `STLC` | `0x802FF4BC` | 0x68 | 32 | 0x16 | `__nw__FUlPCcPCc`, `fn_80051F10`, `fn_802FF524` | `lbl_803AFAB8` |
| `EGMC` | `0x801EF598` | 0x64 | 16 | 0x16 | `__nw__FUlPCcPCc`, `fn_801EFA4C`, `fn_801EF5FC` | `lbl_803AB1B8` |
| `RULE` | `0x801F6AE4` | 0x64 | 16 | 0x32 | `__nw__FUlPCcPCc`, `__ct__8CRuleSetFR12CInputStream`, `__ct<8CRuleSet` | `@stringBase0` |

The class behind each `fn_*` ctor is **not derivable from this tree**. The DOL's own symbol table
has 195 `__ct__` entries out of 25,820 symbols; the 86 REL modules' import tables carry real
retail names but import none of these 30 constructors (checked: 13,219 REL-referenced names are
absent from `config/G2ME01/symbols.txt` and not one of them is a factory or a resource
constructor), and there is no CodeWarrior map on the disc. So the names below are the sizes and
the constructor addresses, and the class names are the lane's reading of the FourCC, not
measurements - **treat the `new` size as the only measured part and the class name as a
hypothesis.**

Three of the 36 are **already in this tree** and are the shape the other 33 need:
`FStringTableFactory` is declared in `include/Kyoto/CFactoryMgr.hpp:39` (no definition),
`FDependencyGroupFactory` is defined in `src/Kyoto/CDependencyGroup.cpp:51` (a `Matching` DOL
unit, but not in the port build), and `FRuleSetFactory` is defined in
`src/MetroidPrime/CRuleSet.cpp:7`. All three take
`(const SObjectTag&, CInputStream&, const CVParamTransfer&)`, and each is 0x64 bytes in retail -
so a lane writing the 33 `fn_...` ones has three matched references to work from.

`CFactoryMgr` itself is a problem for a lane: `include/Kyoto/CFactoryMgr.hpp` is
`class CFactoryMgr { static uint FourCCToTypeIdx(uint); static uint TypeIdxToFourCC(uint);
private: uchar pad[0x38]; };` - **upstream's `CFactoryMgr.cpp` is not in this tree**, and its
comment says so. So the two registrars have nowhere to be *declared* as members, which is a
second reason not to write the 36 calls today: doing it honestly means writing `CFactoryMgr`
first.

## `CMain::StreamNewGameState`, and the record it needs

Retail 0x800053B8, 0x214 = 532 bytes, a 320-byte frame, r26 the `CInputStream&` and r28 `this`.
**`this` is used once** (`lwz r3,84(r28)` for `gameGlobalObjects`, five times) and everything
else is reached through the **old** `gpGameState`, which is the point: this function copies
fields out of the state being replaced.

| retail range | what |
| --- | --- |
| 0x800053D0-0x800053FC | three locals built out of the old state: `fn_80005108(&r1+0xB0, gpGameState+0x54)`, `fn_80004C90(&r1+0x7C, gpGameState+0x110, gpGameState+0x108)`, `fn_80004AA0(&r1+0x24, gpGameState+0x188)` |
| 0x8000540C-0x80005424 | `flag = *(r1+0x28) != 0` - the first word of the `fn_80004AA0` local |
| 0x80005428-0x80005458 | two more locals: `fn_80004C90(&r1+0x48, gpGameState+0x144)`, `fn_80004AA0(&r1+0x14, gpGameState+0x178)`, `fn_80004E84(&r1+0xDC, gpGameState+0x80)` |
| 0x80005458-0x80005474 | release the old `CGameState` (`fn_80004154(&ggo->x130, 0)`, a `single_ptr::operator=(nullptr)`) and set `gpGameState = 0` |
| 0x80005474-0x800054A0 | **the record**: `r5 = flag ? &r1+0x24 : &r1+0x80 + 16 * *(int*)(r1+0x7C+0x5C)`, then `CMemoryInStream(&r1+0x34, *(r5+4), *(r5+0xC))`, then `CBitStreamReader(&r1+8, &mis)` |
| 0x800054A4-0x800054D4 | `::operator new(752, "??(??)..", 0)`, a null test, `fn_80144140(&bitStreamReader)` (0x80144140, **0x684 = 1,676 bytes** - the `CGameState` constructor, and the only writer of the fields below), and `fn_80004154(&ggo->x130, that)` |
| 0x80005500-0x80005510 | `gpGameState = ggo->x130`, then `fn_80003F08(&gpGameState->x54, &r1+0xB0)` |
| 0x80005514-0x80005548 | four more copies: `fn_80142FA4(new, &r1+0x7C)`, `fn_80142920(new, &r1+0x48)`, `fn_801427DC(new, &r1+0x14)`, `fn_80003D00(&new->x80, &r1+0xDC)` |
| 0x80005548-0x80005550 | **`CGameOptions::EnsureOptions()` on `gpGameState+0x80`** - and `CGameState::gameOptions` is at +0x80 in `include/MetroidPrime/Player/CGameState.hpp`, so the offset the header already claims is retail's |
| 0x80005558-0x8000556C | `new->x10C = old->x10C`, `new->x108 = old->x108` (both loaded at 0x800053E8/0x800053EC, *before* the old state is released), and `fn_80142FEC(new)` when `flag` |
| 0x80005570-0x800055B4 | six destructors in reverse: `fn_80004D84(&r1+0xDC)`, `fn_80004A4C(&r1+0x14)`, `fn_80004B9C(&r1+0x48)`, `fn_80004A4C(&r1+0x24)`, `fn_80004B9C(&r1+0x7C)`, `__dt__PersistentOptions_800050A4(&r1+0xB0)` |

So the record's 16 bytes are `{u32, void* data at +0x04, u32, u32 size at +0x0C}` and there is
an **array of them at `r1+0x80`**, indexed by a field at `+0x5C` of the `fn_80004C90` local -
i.e. a save-slot history, the same `{int n; ...}` shape as `fn_800069AC` and `CMain`+0x18/+0x2C
in `boot_path.md`. `fn_80004AA0`'s return word being the flag that picks the *other* source is
the "is there a live save record" test.

**`CGameState` offsets retail reads here: +0x54, +0x80, +0x108, +0x10C, +0x110, +0x144, +0x178,
+0x188.** +0x80 and +0x3C the header already models. +0x54..+0x80 is a 0x2C-byte object and
`__dt__PersistentOptions_800050A4` is the destructor of the local built from it, so
**`CGameState`+0x54 is a `CPersistentOptions`** - which is not where
`include/MetroidPrime/Player/CGameState.hpp` puts `persistentOptions` (after `hintOptions`,
i.e. past +0x80). A lane writing `CGameState` will hit that before anything else here.

Seventeen callees are unnamed in `config/G2ME01/symbols.txt` and unwritten:
`fn_80003D00`, `fn_80003F08`, `fn_80004154`, `fn_80004A4C`, `fn_80004AA0`, `__dt__80004B9C`,
`fn_80004C90`, `fn_80004D84`, `fn_80004E84`, `__dt__PersistentOptions_800050A4`, `fn_80005108`,
`fn_801427DC`, `fn_80142920`, `fn_80142FA4`, `fn_80142FEC`, `fn_80144140`. Writing this function
as calls would add 17 ratchet entries and close none, so the shape is written and the list is
here.

## `CMain::InitializeSubsystems`, and the two parts of it that are harmful on a host

Retail 0x80008680, 0x15C = 348 bytes. `ARInit((u32*)0x803C5AB8, 3)`,
`lbl_80418BA8 += ARAlloc(lbl_80418EA0)`, `ARQInit()`, then the stack guard, then two `printf`s,
then `fn_802DAE30`, `fn_8002ADC8`, `fn_80301CC4(2048, 0x600000, 4096)`, `fn_800E85A8`,
`fn_800DC0B0`, `CFrameDelayedKiller::Initialize()`.

Three measurements that `boot_path.md` step 11 does not have:

1. **`lbl_80418BA8` is .sdata 0x80418BA8 and its initial value is `00004000`** - which is
   Aurora's own `ARAM_STACK_START` (`extern/aurora/lib/dolphin/AR.cpp:17`). It is the ARAM bump
   pointer, not a size.
2. **The stack-guard fill constant is `0x7338D00D`, not `0x7338D0D0`.** `lis r5,29496` /
   `addi r5,r5,-12275` at 0x80008710/0x8000871C is `0x73380000 + 0xD00D` (the raw immediate is
   the bytes `38 a5 d0 0d`, i.e. `0xD00D` = 53261 = -12275 signed). `boot_path.md` transposed
   two digits. The four bytes of that word are `0D D0 38 73`, **not one repeated value**, so the
   source is a `uint` fill loop and not a `memset`; MWCC compiled it as a word loop unrolled
   eight-wide with a remainder.
3. **Aurora's `OSThread` puts `stackBase` at +0x304 and `stackEnd` at +0x308** -
   `extern/aurora/include/dolphin/os/OSThread.h:53-54` - *the same offsets retail reads*. That is
   why the block compiles and looks right on a PC and is still wrong: it fills 8 KB **below**
   `stackBase`, which on a PC is Aurora's heap, not a stack, and then hands the range to
   `OSProtectRange(3, ..., 1024, 0)` and `DCFlushRange`. Aurora even has `OSClearStack(u8 val)`
   for the intended purpose, but retail's value is not a byte, so it cannot be reproduced
   through it.

And the AR hazard, which `boot_path.md` already has and which is now in the source as a guard:
Aurora's `ARInit` (`AR.cpp:98`) only *stores* the pointer - `AR_BlockLength = stack_index_addr;
sAllocationStackBase = stack_index_addr;` - and `ARAlloc` dereferences it two lines later
(`*AR_BlockLength = length; AR_BlockLength += 1;`, lines 71-73). Retail's pointer is the guest
address 0x803C5AB8, three words of DOL `.bss`. `ARAlloc` fails even earlier, on its own
`AURORA_ASSERT(AR_init_flag && !(length & 0x1f), ...)`, because the length retail passes is the
guest word at 0x80418EA0. So the host path is `PortInitializeSubsystems()` in
`src/MetroidPrime/PortBoot.cpp` - a translation unit `configure.py` never claims, the same
arrangement as `CMain::OpenWindow` and `CMain::RsMain` - and mwcceppc, which does not define
`TARGET_PC`, compiles the retail body unchanged.

**The five unwritten callees are still unwritten** (`fn_802DAE30`, `fn_8002ADC8`,
`fn_80301CC4`, `fn_800E85A8`, `fn_800DC0B0`): calling them would add five ratchet entries and
close none. That is why the function sits at 72.36% and not higher, and the missing 27.64% is
48 bytes of `bl` plus the register setup for their arguments.

## What moved when this was written

- `CGameGlobalObjects::AddPaksAndFactories` 0.21% -> **23.17%** (blocks 1, 2, 4, 5).
- `CMain::InitializeSubsystems` 12.44% -> **72.36%** (all of it but the five unwritten calls).
- `CMain::StreamNewGameState` 19.29% -> **25.26%** (the record pick, the release/publish pair and
  `CGameOptions::EnsureOptions`; the seventeen unwritten callees are listed above, not called).
- `main/MetroidPrime/main` 27.24% -> **31.13%** fuzzy, still **20/99** functions at 100%. Nothing
  else in the unit moved: `tools/gate.sh`'s per-function diff lists exactly these three.
- **`tools/link_gap.py` 559 -> 560, and the +1 is
  `_ZN9CGraphics14SetModelMatrixERK12CTransform4f`.** Three new references cost one symbol
  because `CGraphics::SetViewPointMatrix` was *already* on the ratchet
  (`port_link_gap_list.md:247`) - which is the general lesson: naming a callee the port already
  references is free, and the ratchet is per-symbol rather than per-reference.
- `main.o` still carries no definitions for `lbl_80418BA8` or `lbl_80418EA0`. They are
  `extern`-only and only reachable on the non-`TARGET_PC` path, so no port build needs either,
  and a definition here would have cost the unit three points (see the head of
  `src/MetroidPrime/PortGlobals.cpp`).
- **The 13 new string literals cost nothing, and that narrows the warning already in this tree.**
  `.rodata` grew 0x6C -> 0x11A, and of the 81 functions in `main.o` **75 instruction streams are
  byte-identical** to a build of `4d49561`. Three are the ones above. The other three -
  `InfiniteLoopAlarm`, `LoadStringTable`, `PostInitialize` - each differ in **exactly one
  instruction**: the `addi` that is the low half of an `R_PPC_ADDR16_HA`/`R_PPC_ADDR16_LO` pair
  against `@stringBase0` (34 -> 152, 48 -> 166, 58 -> 176; all three moved by the same 118 bytes,
  the amount the new literals pushed the existing ones along inside the pool). That is a
  relocation addend the linker overwrites, so nothing about the linked program moved - the gate's
  per-function diff lists those three as unchanged and the DOL sha1 is unchanged.
  `src/MetroidPrime/main.cpp` and `docs/RUNNING_THE_DECOMP.md` both warn that "a string literal
  added to a `NonMatching` unit can move an unrelated function". **That is still true, but the
  mechanism is narrower than the wording suggests**: a literal that merely joins `.rodata` moves
  an `addi` addend; what cost two 100% functions in the recorded case was a literal changing what
  an *unrelated* function *computes* - `__ct__CGameArchitectureSupport` growing 32 bytes and
  picking up a `__cvt_dbl_usll` call. Always check the per-function diff; it takes two seconds
  and is the only thing that sees this.
- **The port still does not link and still does not boot.** No frame has been rendered.

## The `CResLoader` layout, and the two pump functions (lane g1, 2026-09-26)

Corrected 2026-09-26 by lane g1, from a build of commit `3d2ce92`. **This file's `CResFactory`+0x5C
for `CFactoryMgr` was wrong, and so was the loop polarity in block 7 above.** Both are corrected
in place above; the evidence is here, and the header now carries it too
(`include/Kyoto/CResLoader.hpp`).

### `CResLoader` is 0x60 bytes and is four `rstl::list`s

`rstl::list` puts its count at `+0x14` and is 0x18 bytes. Three retail instructions read a count at
`CResLoader`+0x14, +0x2C, +0x44 and +0x5C, and one function increments and decrements the last of
them:

| retail | instruction | what it reads |
| --- | --- | --- |
| 0x802fc360 | `lwz r4,8(r3)` | list@+0x48's `x8_end` |
| 0x802fc3d4 / 0x802fc3e0 | `lwz r0,4(r28)` / `bne` / `stw r3,4(r28)` | list@+0x48's `x4_start` |
| 0x802fc3f4 / 0x802fc3fc | `lwz r4,20(r28)` / `addi r0,r4,1` / `stw r0,20(r28)` | list@+0x48's `x14_count` **++** |
| 0x802fbc60 / 0x802fbc64 | `lwz r4,44(r3)` / `lwz r0,68(r3)` / `add r3,r4,r0` | `GetPakCount()` = counts of the lists at **+0x18** and **+0x30** |
| 0x802fcce4 | `lwz r0,92(r3)` | `AreAllPaksLoaded()` = the count of the list at **+0x48** |
| 0x802fd1e8 / 0x802fd1f4 | `lwz r4,20(r29)` / `addi r0,r4,-1` / `stw r0,20(r29)` | the **same** count, **--**, by `fn_802FD174(&this->x48, node)` |

The last row is what makes `+0x48` a list and not the four scalars `include/Kyoto/CResLoader.hpp`
used to declare there: the word `AreAllPaksLoaded` reads and the word the erase decrements are the
same word of the same object. So the loader is four 0x18-byte lists at **+0x00, +0x18, +0x30 and
+0x48**, and is **0x60** bytes - not 0x58, which is what every offset downstream of it assumed.
`CFactoryMgr` therefore starts at `CResFactory`+0x64, and the 36 registrations of block 9 target
`CFactoryMgr`+0x10 rather than +0x18.

The item is **8 bytes**, and three instructions fix it: `fn_802FC378` allocates a **16**-byte node
(`li r3,16` at 0x802fc3a0) and copies exactly the words at node+8 and node+0xC; `fn_802FCFF4`
reads a **byte** at `*(item+4)` to choose a list; `GetPakFile` returns `lwz r3,12(node)`. So
`SPakLoadEntry { bool x0_inList; CPakFile* x4_pak; }` - the pair lane f2 had to declare locally in
`src/Kyoto/CResLoaderAddPakFileAsync.cpp`, now in the header next to the lists it belongs to.

**Corrected 2026-09-26 by lane k4: the item is `rstl::auto_ptr< CPakFile >`, and `fn_802FC378`
is `rstl::list`'s own `do_insert_before`.** The paragraph above reads `stb r0,0(r30)` at
0x802fc3d0 as "the insert clears the caller's flag", and that is the *symptom*: it is the
element's **copy constructor**, and the evidence is a function this tree already matches.
`fn_802FC378`'s 0xA8 bytes are identical, instruction for instruction and register for register,
to retail's own named

```
do_insert_before__Q24rstl70list<Q24rstl28auto_ptr<16CFilePreloadData>,Q24rstl17rmemory_allocator>
                    FPQ34rstl70list<...>4nodeRCQ24rstl28auto_ptr<16CFilePreloadData>>
                      = .text:0x803445DC; size:0xA8 scope:weak
```

- the same prologue, the same `li r3,16`, the same `r28`/`r29`/`r30`/`r31` assignment order
(`mr r30,r5` / `mr r29,r4` / `mr r28,r3`), the same `addic. r5,r3,8` / `beq` over the 8-byte
copy, the same head fixup, the same two link stores, the same `++x14_count`, the same 32-byte
frame. That instantiation is a `Matching` unit (`src/Kyoto/Streams/CFilePreload.cpp`, 100.00%),
so the shape was already reproducible for a different element type.

`rstl::auto_ptr<T>` in this tree is `{ mutable bool x0_has; T* x4_item; }` with an
**auto-relinquishing** copy constructor (`include/rstl/auto_ptr.hpp:25`) - copy both words, then
`other.x0_has = false`. That is exactly the three stores and the `stb r0,0(r30)`. And
`fn_802FD174` (`do_erase`) confirms it from the other side: it `lbz`es the flag byte and then
`bl __dt__CPakFileFv` on `*(item+4)`, which is that class's destructor.

`include/Kyoto/CResLoader.hpp` now spells the same class out as `SPakLoadEntry` **with that copy
constructor** (and `mutable` on the flag, because the source is `const SPakLoadEntry&`). With it,
`fn_802FC378` is `do_insert_before` called through the list's public `node*`, and it is a
`Matching` unit - see "The pak insert, landed" below. Nothing about `rstl::construct` had to
change: the `addic. r5,r3,8` / `beq` guard is the placement `new` and it is correct.

`fn_802FCFF4` chooses on the entry's pak's **ARAM-file** bit, not its world-pak bit:
`rlwinm. r0,r0,26,31,31` at 0x802fd008 is flag **field 25**, and in `CPakFile`'s constructor field
25 is the one filled from `CDvdFile::IsARAMFile()` (`rlwimi r0,r4,6,25,25` at 0x803245d0 over
`lbz r4,8(this)`), while field 26 is `worldPak` and field 27 is `stashedInARAM`. `+0x18` is
therefore the **ARAM-file** paks and `+0x30` the ordinary ones. Writing `IsWorldPak()` there
compiles to `rlwinm ...,27,31,31` and is a different bit.

### The loop polarity, which block 7 above had backwards

```
80007480: addi r3,r31,4
80007484: bl   fn_802FCCE4
80007488: clrlwi. r0,r3,24
8000748c: beq  80007430        <- the BODY
```

`fn_802FCCE4` returns 1 iff the count at `+0x5C` is **zero** (`cntlzw` of 0 is 32, `srwi 5` of 32
is 1), so the body runs while the count is non-zero. The caller's loop is
`while (!resLoader.AreAllPaksLoaded()) { resLoader.AsyncIdlePakLoading(); ... }` - not "while it is
true". The name is right; the polarity in the prose was not.

### `AsyncIdlePakLoading`'s latch, and why it is not the world-pak condition

`fn_802FCCF4` keeps one bool in r28 and reuses one flag read in r31:

```
r31 = pak->x28_aramFile                     ; read once, at 0x802fcd18
if (r31 || r28 == 0) pak->AsyncIdle();      ; 0x802fcd1c
if (pak->IsCompletelyLoaded()) {            ; 0x802fcd34
  fn_802FCFF4(this, &node->x8_item);        ; move to the finished list
  r29 = fn_802FD174(&this->x48, r29);       ; unlink; returns the next node
  goto the test;                            ; the latch is NOT set on this path
} else if (!r31) { r28 = 1; }               ; 0x802fcd60
r29 = r29->x4_next;
while (r29 != this->x48.x8_end);
```

So r28 means "a plain pak has been idled above and is still loading", and once it is set the rest
of the call only idles the **ARAM-file** paks. It is not a loop-carried error flag and it is not
the world-pak condition.

### What landed, and what it cost to land

Four `Matching` units, all `flip_test.sh` PASS with the DOL sha1 held at
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` and all 86 RELs `cmp`-equal. The first two are lane
g1's; the last two are lane k4's, 2026-09-26.

| unit | range | functions | score |
| --- | --- | --- | --- |
| `src/Kyoto/CResLoaderPakPump.cpp` | `.text 0x802FCCE4..0x802FCD90`, 172 B | `AreAllPaksLoaded`, `AsyncIdlePakLoading` | 100.00% / 2 of 2 |
| `src/Kyoto/CResLoaderGetPakCount.cpp` | `.text 0x802FBC60..0x802FBC70`, 16 B | `GetPakCount` | 100.00% / 1 of 1 |
| `src/Kyoto/CResLoaderInsert.cpp` | `.text 0x802FC350..0x802FC420`, 208 B | `fn_802FC350`, `fn_802FC378` | 100.00% / 2 of 2 |
| `src/Kyoto/CResLoaderResAccessors.cpp` | `.text 0x802FCAE8..0x802FCC44`, 348 B | `fn_802FCAE8`, `fn_802FCB40`, `fn_802FCB88`, `fn_802FCBD0`, `fn_802FCC00` | 100.00% / 5 of 5 |

`GetPakCount` needs its own unit because 0x802FBC60 and 0x802FCCE4 are 0x1B4 apart and a unit may
claim several contiguous ranges but not two discontiguous ones. `fn_802FCCE4` and
`fn_802FCCF4` are unnamed in retail, so they were renamed in `config/G2ME01/symbols.txt` to
`AreAllPaksLoaded__10CResLoaderCFv` and `AsyncIdlePakLoading__10CResLoaderFv` - which is legal
here because no REL module imports either name (`grep -r 'fn_802FCCE4\|fn_802FCCF4'
config/G2ME01/rels/` is empty), and it is the only way the DOL link can resolve the three
references `main.o` and two `auto_*` objects make to them.

The two things that were *not* obvious and cost the lane most of its time, both now written at the
definition and in `docs/RUNNING_THE_DECOMP.md`:

1. **mwcceppc hands out r30, r29, r28 to the first, second and third local of a function** - so the
   declaration order that reproduces retail's registers is the *reverse* of the order the code reads
   in, and `pak` has to be declared uninitialised, before the cursor it is derived from.
2. **The two functions must be declared in the order `AsyncIdlePakLoading` then
   `AreAllPaksLoaded`**, which is *ascending* by retail offset, because mwcceppc emits in reverse
   source order. With them the readable way round the object is still exactly 0xAC bytes,
   `unit_fit.sh` says "fits", objdiff still reads 100%, and the whole object lands 0x200 bytes
   early in the DOL - the shasum is the only thing that sees it.

### The pak insert, landed (lane k4, 2026-09-26)

**`fn_802FC350` / `fn_802FC378` are a `Matching` unit**, `src/Kyoto/CResLoaderInsert.cpp`,
`.text 0x802FC350..0x802FC420`, 208 bytes, 2 of 2 at 100.00%, `flip_test.sh` PASS. The pair is
**the last thing between this file and a constructed `gpResourceFactory`** - it is the insert
every one of the nine pak loads goes through, and `CResLoader::AddPakFileAsync` is a `Matching`
unit that calls it by name.

The two bodies are not transcriptions:

```cpp
extern "C" void* fn_802FC378(void* pakList, void* pos, void* item) {
  typedef rstl::list< SPakLoadEntry > list_t;
  return static_cast< list_t* >(pakList)->do_insert_before(
      static_cast< list_t::node* >(pos), *static_cast< SPakLoadEntry* >(item));
}
extern "C" void* fn_802FC350(void* pakList, void* item) {
  typedef rstl::list< SPakLoadEntry > list_t;
  list_t* const self = static_cast< list_t* >(pakList);
  return fn_802FC378(pakList, self->end().get_node(), item);
}
```

Four things a next lane should not have to rediscover:

1. **`#pragma inline_max_size` is what keeps the two functions apart, and it must be *large*.
   It is 200, and the threshold is fragile.** With it unset, mwcceppc emits `do_insert_before`
   as a separate COMDAT and `fn_802FC378` becomes a 0x20-byte forwarder to it - 0x58 of the
   0xA8, and a symbol retail does not have. `#pragma inline_max_size(0)` is the opposite
   mistake and much worse: it suppresses every inline, so `rstl::construct` stops being a
   placement `new` and becomes a call to `__nw__FUlPv` plus a null test plus a call to the
   copy constructor. Measured: **0, 125, 150, 160, 170 and 180 all fail; 190, 200, 250, 400,
   1000 and 100000 give 0xD0 and two `T` symbols.** **And 125 was measured working earlier in
   the same session, before `Kyoto/CResLoader.hpp` began including `Kyoto/CPakFile.hpp` for
   `x68_curRes`'s type** - same source, same compiler, 55 bytes of threshold apart, and the
   only symptom is `unit_fit.sh` reporting a third function and "over by 32". If this unit
   ever stops flipping, raise the pragma before touching the body.
2. **`fn_802FC378` is declared *before* `fn_802FC350`** even though its retail offset is higher,
   because mwcceppc emits in reverse source order. `tools/check_decl_order.py` is the gate; with
   them the other way round the object is 0xD0 bytes and every function still 100%.
3. **The out-of-line `allocate` needs no macro.** `bl allocate__Q24rstl17rmemory_allocatorFi` is
   this tree's *default* `rstl/rmemory_allocator.hpp` revision, so `RSTL_INLINE_RESERVE_HELPERS`
   is off here. The two revisions coexist per object exactly as that header says, and this object
   is on the default one.
4. **No rename, and no `new` file-string to name.** Neither function allocates a `.rodata` byte
   of its own, and both keep their dtk names, so no `symbols.txt` edit and no `splits.txt` entry
   for data. `fn_802FC378` is referenced only from inside the object it is in; `fn_802FC350` is
   referenced by `auto_03_802FCD90_text.o` twice (`fn_802FCFF4`) and by the `Matching`
   `AddPakFileAsync`, which is why **neither name may be renamed** - unlike g1's two.

**The `rstl::construct` guard is retail's and stays.** The `addic. r5,r3,8` / `beq` is the
placement `new` in `rstl/construct.hpp` and the branch is dead - the carry out of a 16-bit add of
8 to a 16-byte-aligned pointer cannot be set. The recorded negative result stands and is now
*also* the reason this unit is possible: replacing that `new (dest) T(src)` with an assignment
breaks five `Matching` units and the DOL sha1, so the fix is the element's **copy constructor**,
not the guard. `include/Kyoto/CResLoader.hpp` carries it, and the identical bytes are already
produced for `list<auto_ptr<CFilePreloadData>>` in `src/Kyoto/Streams/CFilePreload.cpp`. **That
unblocks `CPakFile::RebuildResourceLists` too** - it is at 41.02% and its missing piece is
`rstl::construct`'s spelling for an 11-byte element, the same decision.

**The port got better, not just bigger.** `src/MetroidPrime/PortGlobals.cpp` carried hand
transcriptions of both functions written against retail's 32-bit list offsets (`SPakLoadList`'s
head at +4, tail at +8, count at +0x14, and a 32-byte `SPakLoadNode`), which is why
`AddPakFileAsync`'s `TARGET_PC` half had to spell the insert out itself rather than call them.
Both are gone: the new unit is in `files.cmake`, so the port links the *real*
`rstl::list< SPakLoadEntry >` at 64-bit width, and `AddPakFileAsync`'s two halves now differ only
in how the tag/pak pair is built. `link_gap.py` 289 -> 289; `link_check.sh` 318 undefined before
and after.

### The five current-resource accessors, and what `+0x64`/`+0x68` are (lane k4)

`src/Kyoto/CResLoaderResAccessors.cpp`, `.text 0x802FCAE8..0x802FCC44`, 348 bytes, 5 of 5 at
100.00%, `flip_test.sh` PASS. The same dtk object also holds `fn_802FC420`, `fn_802FC4D8`,
`fn_802FC63C`, `fn_802FC81C`, `fn_802FC898`, `fn_802FCA68` and `fn_802FCC44`; see the characterisa-
tion below.

All five have one shape - `fn_802FCDE8(this, id)`, and only if that returned non-null a call on
`this->x68_curRes` - and the load-bearing discovery is what `+0x64` and `+0x68` are.
`fn_802FCF98` (0x802FCF98, 0x54) is the per-pak probe `fn_802FCDE8` calls, and after
`CPakFile::GetResInfo(id)` returns non-null it writes **both**:

```
802fcfd0:  stw r31,100(r30)   ; this->x64_ = the id it looked up   (r31 = its second argument)
802fcfd4:  stw r3,104(r30)    ; this->x68_ = the CPakFile::SResInfo* it found
802fcfd8:  li  r3,1
```

and each of the five reads **only** `+0x68` - `lwz r3,104(r31)` and then `GetSize` / `GetOffset` /
`GetType` / `IsCompressed` on it, all four of which are `CPakFile::SResInfo` members taking no
argument. `SResInfo` is 11 bytes, so `+0x68` is a **pointer to one** and not the struct. Two of
the loader's four "unnamed words" are therefore named now:
`CAssetId x64_curId` and `CPakFile::SResInfo* x68_curRes`, in `include/Kyoto/CResLoader.hpp`,
with the four `lwz`/`stw` above quoted at them. `+0x60` and `+0x6C` are still unnamed.

Three more measured facts about the five:

* **`fn_802FCC00` is the only one that does not read `4(r4)`**, and that is what fixes its
  parameter as a bare `CAssetId` rather than a `const SObjectTag&`: it is
  `CResLoader::GetResourceTypeById(CAssetId)`, which the header already declared. The other four
  read `tag.id` - `SObjectTag` is `{ FourCC type; CAssetId id; }`, so +4 is the id.
* **The `li r3,0` sits *after* the body** and the body ends in an unconditional `b` over it, so
  MWCC laid "not found" out as the `beq`'s taken arm. The source has to be
  `if (found) { return ...; } return 0;`; the other order costs a branch.
* **`fn_802FCAE8` needs `IsCompressed() ? 1 : 0` and not `IsCompressed()`, and that is 12 bytes.**
  Retail's tail is `clrlwi r3,r3,24` - the callee's `bool` return being normalised - *followed by*
  `neg r0,r3 ; or r0,r0,r3 ; srwi r3,r0,31`, the same test again. `return x->IsCompressed();`
  against an `int` return emits only the `clrlwi`, and the function comes out 0x4C against
  retail's 0x58. Measured, both ways, same object. `fn_802FCBD0` is 0x30 bytes and has no
  `x68_` read and no `li r3,0` at all, because its answer *is* the lookup's.

They are `extern "C"` free functions, not `CResLoader` members, because **21 dtk objects call
them by their dtk names** (`auto_03_8004E448` through `auto_03_802F8EB0`) and a rename in
`symbols.txt` would break every one of those references. The two members the header declares for
them (`GetResourceTypeById`, `ResourceSize`) are left declared and undefined, which is the state
this tree was already in - nothing in the port build calls them, and the link gap does not move.

**`fn_802FCDE8` itself is 0x104 bytes and walks three of the four lists**, measured: the first
loop tests `this+0x20` against `this+0x38` (0x802fce3c/0x802fce40), the second reads `this+0x60`
(0x802fce48) and the third `this+0x38` again (0x802fcebc/0x802fcec0), so `+0x00`, `+0x18` and
`+0x30` are visited and **`+0x48`, the loading list, never is**. Each step is `node->x4_pak`
(`lwz r31,12(r30)`), and the probe differs per loop - `fn_802FCF98` for `+0x00` and
`fn_802FCF10` for the other two, 0x54 and 0x88 bytes. Neither is written and their difference is
not derivable from this tree, so the port's `TARGET_PC` `fn_802FCDE8` is the honest simplification
(one `GetResInfo` per pak, same two stores). Without it the unit would add `fn_802FCDE8` to the
port's link gap rather than close anything, which `tools/gate.sh` fails on.

**`fn_802FCEEC` (0x802FCEEC, 0x24) is landed too**, as `src/Kyoto/CResLoaderFindPak.cpp` - a
third `Matching` unit, 36 bytes, 1 of 1 at 100.00%. It is `fn_802FCDE8` with `tag.id` hoisted
into r4 and the result passed straight through, six instructions and no `stw r31` / `mr r31,r3`
- which is the proof that `this` is never live across the call there. It needs its own unit only
because a unit may claim several *contiguous* ranges and 0x802FCC44..0x802FCEEC is
`fn_802FCDE8`'s 0x104 bytes.

### The seven, resolved (lane m2, 2026-09-26): four landed, two at 98-99%, one blocked

**[Superseded in part 2026-09-26 by lane m2.]** The table below is the original characterisation and
it stands as the record of *what each function is*; the corrections are in the table that follows it.

**Four are `Matching` units at 100.00%, `flip_test.sh` PASS, DOL sha1 held at
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` and all 86 RELs unchanged.** Between them 412 bytes of
retail's `.text`. **`fn_802FC420` was the fourth and it came out right on the first build**, so the
two things the characterisation above called its blocker were not blockers at all.

| unit | range | size | functions | score |
| --- | --- | --- | --- | --- |
| `src/Kyoto/CResLoaderLoadPartAsync.cpp` | `.text 0x802FC81C..0x802FC898` | 124 B | `fn_802FC81C` | 100.00% / 1 of 1 |
| `src/Kyoto/CResLoaderLoadAsync.cpp` | `.text 0x802FCA68..0x802FCAE8` | 128 B | `fn_802FCA68` | 100.00% / 1 of 1 |
| `src/Kyoto/CResLoaderGetResIdByName.cpp` | `.text 0x802FCC44..0x802FCCE4` | 160 B | `fn_802FCC44` | 100.00% / 1 of 1 |
| `src/Kyoto/CResLoaderLoadResourceSync.cpp` | `.text 0x802FC420..0x802FC4D8` | 184 B | `fn_802FC420` | 100.00% / 1 of 1 |

Six corrections to the characterisation above, all of which cost time:

1. **`kUnknownType__10CCallStack` is at 0x803AEAB8, not 0x803AE558.** The row above says
   "0x803AE558"; that address is `lbl_803AE558` (`size:0x7`, a different string). The symbol retail's
   own relocation names is `kUnknownType__10CCallStack` and `config/G2ME01/symbols.txt:17706` puts it
   at `0x803AEAB8`, `size:0xC`, `"UnknownType\0"` - which is exactly `CCallStack`'s declared default
   `type`. **So the third `CCallStack` argument does not have to be written at all**: it is private,
   and the default *is* the retail symbol. `CCallStack(-1, lbl_803AFAA0)` is the whole call.
2. **Both of the `new`s in these functions need retail's own `lbl_803AFAA0` spelled out**, exactly as
   `CResLoaderAddPakFileAsync.cpp` does and for the same reason. `Kyoto/Alloc/CMemory.hpp`'s inline
   `operator new(size_t)` forwards to `operator new(sz, "??(??)", nullptr)`, and *that* is retail's
   symbol - but only because the literal and `lbl_803AFAA0` are the same six bytes. mwcceppc emits
   the literal as a local `@stringBase0` at a different address, so the `addi` differs and only the
   hash sees it. Measured: with the literal the object references `@stringBase0` and the unit reads
   40.24%; with `new (lbl_803AFAA0, nullptr)` it reads 100%.
3. **The `CMemory::Alloc` argument list reproduces only when the `CCallStack` is a *temporary passed
   straight into the call*.** Naming it (`const CCallStack callstack = ...;` then passing that)
   compiles to `addi r7,r1,8`; retail has `addi r3,r1,8` / `bl __ct__10CCallStackFUiPCcPCc` /
   `mr r7,r3`, which is only reachable if the reference *is* the value r3 already holds, because
   retail's 12-byte ctor (`stw r5,0(r3)` / `stw r6,4(r3)` / `blr`, 0x8028BFE8) leaves r3 alone. Two
   differing instructions and the unit drops to 91.70%.
4. **`fn_802FC420` returns `void` and its return type has to say so.** The frame's last `bl` is
   `SyncSeekRead` and the epilogue follows with no `mr r3`; a `void*` return emits a trailing
   `li r3,0` and the function comes out eight bytes longer. This is the same polarity rule as
   `fn_802FCAE8`'s `IsCompressed() ? 1 : 0`, one function over: **MWCC normalises a return value it
   can see is dead, so the declared type has to be the dead one.**
5. **`fn_802FCA68` needs the length in a *named* local.** `AsyncSeekRead(buf, (res->GetSize() + 31)
   & ~31, kSO_Set, res->GetOffset())` inlines both accessors but evaluates them in the *wrong* order
   and puts the `+31`/`clrrwi` in the wrong place - 87.03%. Writing `const uint size = res->GetSize();`
   first and then `AsyncSeekRead(buf, (size + 31) & ~31, kSO_Set, res->GetOffset())` gives retail's
   exact order (`GetSize`, then `GetOffset`, then `addi r0,r31,31`) and 100.00%. **The argument
   order of an expression is not the evaluation order mwcceppc picks; a named local is.**
6. **`fn_802FCC44` returns `const SObjectTag*`, not a `CAssetId`.** The found path branches to the
   epilogue with the callee's r3 untouched (`b 0x84` over the `li r3,0`), so no field is extracted.
   The header's `GetResIdByName` was never declared, so this is a free function; it is referenced by
   **no** other object, so its name was free to change and was not.

#### The two `CMemoryInStream`/`CLZOInputStream` bodies: identified, 97-98%, blocked on allocation

**`fn_802FC4D8` (0x164) is 99.10% and `fn_802FC63C` (0x1E0) is 98.04%, both `NonMatching` with their
ranges claimed so retail's bytes stay in the link.** The *body* of each is right - 89 and 120
instructions against retail's 89 and 120 - and the whole of what remains is **which register
mwcceppc hands the compressed arm's four temporaries**.

Retail, `fn_802FC4D8` at 0x802fc59c-0x802fc5c4: `r7` for the stream, `r6` for `x8_ptr`, `r30` for
the decompressed size, `r29` for the `new`'s result. This build: `r6`/`r7` and `r29`/`r30` - the two
pairs are each **swapped**, and 20 instructions differ. `fn_802FC63C` is the mirror image: retail
uses `r28`/`r29`/`r30` and this build uses `r27`/`r28`/`r29`, so all three are one lower, 16
instructions. **About forty body shapes were tried** - `prefix` vs `stream->x8_ptr`, `+= 4` vs
`= prefix + 4`, `const`/`uchar*`/`void*`/`uint*` cursor types, `uint`/`s32`/`long`/`unsigned long`
for the size, naming the `CLZOInputStream` result or not, naming the cursor, `Get(4)`,
`stream.get()`, `*stream`, a `CMemoryInStream*` cast, `operator->`, a `CDvdFile&` local, the
declarations hoisted or sunk, `owns` as a named bool, `if` instead of `?:`, a named `resSize` - and
**none of them moves the allocation**. The two best are 20 and 16. This is a register-allocator
preference, not a source-shape problem, and the honest report is that it is unsolved.

What *was* settled about them, and it is the reusable part:

* **The four-byte decompressed-size prefix is read off the front of the memory stream, through
  `CInputStream`'s private `x8_ptr`, and the cursor is advanced past it** -
  `lwz r6,8(r7)` / `addi r0,r6,4` / `stw r0,8(r7)` / `lwz r28,0(r6)` at 0x802fc74c-0x802fc75c, and
  **the identical four instructions at 0x802fc334-0x802fc344 in `fn_802FC63C`**. That is
  `CInputStream::Get(4)` inlined. It cannot be spelled that way here: `Get` is defined in
  `Kyoto/Streams/CInputStream.cpp`, a different translation unit, and spelling it costs 37
  instructions (57 vs 20) because the call is not inlined. **`fn_802FC4D8` and `fn_802FC63C` are
  friends of `CInputStream`** and read the member. Note the ordering: the *store* to `x8_ptr` comes
  **before** the load of the value (`addi`/`stw` at 0x802fc33c/0x802fc340, then
  `lwz r28,0(r6)` at 0x802fc344), so the source is `prefix` first, then the store, then the read -
  and `const uint d = *(const uint*)stream->x8_ptr; stream->x8_ptr += 4;` puts the load first and
  costs 8 of the 20.
* **The compressed length is `GetSize() - GetReadPosition()`** - and that is *not* a reading of
  `SResInfo`. `lwz r5,20(r1)` re-reads the *stream* out of the `auto_ptr`, and `lwz r4,4(r5)` /
  `lwz r0,8(r5)` / `subf r31,r4,r0` are `CInputStream::x4_buffer` and `x8_ptr`, i.e.
  `GetReadPosition()`. This corrects the characterisation above, which read the `+4`/`+8` loads as
  `SResInfo`'s packed words. The consequence is that the four-byte prefix *is* included in
  `GetReadPosition()`, which is what makes the subtraction come out right, and it is why the prefix
  read has to happen first.
* **`rstl::auto_ptr< CInputStream >` is the frame's `r1+8`/`r1+12` pair** in `fn_802FC4D8` and
  `r1+16`/`r1+20` in `fn_802FC63C` (where the `CCallStack` occupies `r1+8`..`r1+0x10`), and the
  three-instruction `neg`/`or`/`srwi`/`stb` is its `auto_ptr(T*)` constructor, not a store pair.
  The `CLZOInputStream` constructor then takes it by reference (`addi r4,r1,8`) and *consumes* it -
  `rstl::auto_ptr< CInputStream > stream(in);` inside a `Matching` unit - so the loader's own
  `auto_ptr` comes back empty and the teardown at 0x802fc59c is the null path. It is still emitted,
  and that is what identifies the local as an `auto_ptr` rather than a raw pointer.
* **The teardown is `~auto_ptr` inlined and it calls `~CInputStream(1)` - the *deleting* destructor -
  as vtable slot +8.** The `li r4,1` is the whole identification. It is confirmed from retail's own
  `__dt__15CMemoryInStreamFv` (0x800055CC): it saves `r4` into `r31`, stores the base vptr, `bl`s
  `__dt__12CInputStreamFv`, then `extsh. r0,r31 ; ble` - i.e. it calls `CMemory::Free` **only when
  the flag is non-zero**. `__vt__15CMemoryInStream` is 0xC bytes at 0x803B0D5C and reads
  `[0, 0, __dt__15CMemoryInStreamFv]`, and the vptr `__ct__15CMemoryInStreamFPCvUl` stores is
  **0x803B0D5C itself** (`lis r4,-32709` / `addi r0,r4,3420` at 0x802fff1c/0x802fff24), so slot +8
  is the third word. `CInputStream`'s only virtual is its destructor, so the `auto_ptr`'s
  `delete x4_item` is the only thing in these functions that can call it.
* **`fn_802FC63C`'s `CMemoryInStream` takes its `EOwnerShip` argument and it is
  `callerBuf != nullptr`** - `__ct__15CMemoryInStreamFPCvUlQ215CMemoryInStream10EOwnerShip` against
  the two-argument form in the other two, fed by
  `neg r0,r30 ; or r0,r0,r30 ; srwi r30,r0,31` at 0x802fc6d4-0x802fc6e0. That is `kOS_NotOwned` (= 1),
  and it is the only correct answer: a buffer the caller owns must not be freed, one this function
  allocated must be. So the ownership is *derived from which arm of the `cmplwi r30,0` produced the
  pointer*, and the three-argument constructor is what expresses it. It is also why `fn_802FC63C`'s
  frame is 48 and not 32 - the `CCallStack` and the `auto_ptr` coexist in it.
* **`fn_802FC63C` reads only r3/r4/r5**, so it is the *three*-argument
  `LoadNewResourceSync(const SObjectTag&, void*)`; the four-argument
  `LoadNewResourceSync(const SObjectTag&, int, int, char*)` that
  `include/Kyoto/CResLoader.hpp:101` declares is a different, also-unnamed function.
* **A `new` in a `Matching` unit has to compile on the host too**, and the host has no
  three-argument `operator new`. `CResLoaderAddPakFileAsync.cpp` solves that by *defining* one under
  `__MWERKS__`; with two `new`s in one function a macro is enough
  (`#if defined(__MWERKS__) ... #define RESLOADER_NEW new (lbl_803AFAA0, nullptr) #else ... #endif`).
  Without it `tools/probe_sources.sh` fails and with it the DOL bytes are unchanged.
* **A `friend` declaration inside a class is the *first* declaration of the function if nothing
  precedes it**, so it gets C++ linkage and then conflicts with the `extern "C"` declaration
  elsewhere. `Kyoto/CResLoader.hpp` gets away with it because the namespace-scope `extern "C"`
  declarations sit above the class in the same header; `Kyoto/Streams/CInputStream.hpp` had to be
  given the same pair above `class CInputStream` for the same reason. mwcceppc reads the `extern` in
  `friend extern "C" f(...)` as a storage class and rejects it, and GCC rejects the mismatch. This is
  the *fourth* time this tree has hit it; it belongs in `RUNNING_THE_DECOMP.md` as a rule.

#### `fn_802FC898`: blocked, and the blocker is bigger than "fn_802FB994 is unwritten"

**`fn_802FC898` (0x1D0 = 464 bytes) is not written, and it needs four unnamed functions, one of
which is in a different part of the loader entirely.** Measured, not inferred:

* `fn_802FB994` (0x802FB994, **0x88 bytes**, unclaimed, referenced only by `auto_03_802FC898`).
  Signature from the call site (`mr r4,r27` / `mr r5,r31` / `mr r6,r3` / `mr r7,r28` /
  `addi r3,r1,12`): `void f(uint* out, CResLoader* self, CPakFile* pak, int offset, uint size)`.
  Its body is a **backwards walk of the fourth list** - `lwz r31,8(r4)` is `x0_aramList.x8_end`,
  `lwz r31,0(r31)` is `node->x0_prev`, and the loop ends at `cmplw r31,*(r27+4)` =
  `x0_aramList.x4_start`, falling out to `*(out) = x8_end` - calling `fn_803434E8` on `node + 8` for
  each entry whose flag byte is clear.
* `fn_803434E8` (0x803434E8, **0x54 bytes**, unclaimed) is a **dependency-group range test**:
  `lwz r0,8(r3)` / `cmplw r4,r0` / `lwz r4,12(r3)` / `lwz r0,16(r3)` / `add r4,r4,r0` /
  `cmplw r5,r4` - is `[offset, offset+size)` inside the node's `[+8, +8++16)` window? That is a
  `CDependencyGroup` method, and `src/Kyoto/CDependencyGroup.cpp` claims 0x803208F0..0x80320FD8,
  so **this is a *different* dependency-group function in a different, unclaimed range**.
* `fn_8034353C` (0x8034353C, **0x9C bytes**, unclaimed) is the "no group covers this" path and it
  materialises a `CCallStack`-style string pair (`lis r4,0x803b` / `addi r4,r4,832` = **0x803B0340**,
  unclaimed `.rodata`).
* `fn_803433D4` (0x803433D4, **0xA0 bytes**, unclaimed) is the fourth, called at 0x802fc5b0.

**And one of them contradicts the header.** `fn_802FB994` reads a **byte at `node + 0x1E`**
(`lbz r0,30(r31)` at 0x802fb9c4) and passes `node + 8` to the range test. So the fourth list's
**element is at least 0x18 bytes, not 8**: with the 16-byte `rstl::list` node header, `node + 0x1E`
is `item + 0x16`, and `fn_803434E8` reads `item + 0x8`, `item + 0xC` and `item + 0x10`. The item is
therefore 0x18 bytes and the node 0x28. **`include/Kyoto/CResLoader.hpp:141`'s
`rstl::list< SPakLoadEntry > x0_aramList` is therefore the wrong element type** - the other three
lists are `SPakLoadEntry` and are reached by the pak chain, but `x0_aramList` is a list of something
else and `fn_802FB994` is the only instruction in retail that says so. The four lists are still
0x18 apart and the 0x70 size is unaffected, so nothing else moves; but this is a real correction to
the header and the next lane that needs `x0_aramList` should start there.

### The seven as originally characterised

The object `auto_03_802FC350_text.o` held **14** functions over 0x802FC350..0x802FCD90, not the
13 the lane brief said - `fn_802FCCE4` and `fn_802FCCF4` are the two g1 already renamed and
claimed. Eight are now `Matching` (the pair, the five accessors, and `fn_802FCEEC`); these seven
are not, and all seven are the *load* half of the loader. Sizes are
`config/G2ME01/symbols.txt`'s.

| function | addr | size | frame | what it is | what blocks it |
| --- | --- | --- | --- | --- | --- |
| `fn_802FC420` | 0x802FC420 | 0xB8 = 184 | 48 | `LoadResourceSync`, the **uncompressed** path: `fn_802FCEEC`, then `CMemory::Alloc` of `GetSize` rounded up to 32 (`addi r0,r3,31` / `clrrwi r29,r0,5`), `CDvdFile::SyncSeekRead`, then `*(void**)r5 = buf` and `*(uint*)r6 = GetSize()` | the `CMemory::Alloc` needs a `CCallStack` built from **two** named objects - `lbl_803AFAA0` (0x803AFAA0) and `kUnknownType__10CCallStack` (0x803AE558) - each a `lis`+`addi` pair, and only the first is already named by `CResLoaderAddPakFileAsync.cpp` |
| `fn_802FC4D8` | 0x802FC4D8 | 0x164 = 356 | 32 | the **compressed** twin: `new (20)` for a `CMemoryInStream` (with the dead `neg r0,r30 ; or ; srwi 31` null test at 0x802fc538), `IsCompressed` on `x68_curRes`, then `new (20)` for a `CLZOInputStream` over `size - (end - begin)`, and a `delete` through **vtable slot 2 with argument 1** | `CMemoryInStream`'s three-argument ctor and `CLZOInputStream`'s ctor have to be byte-exact first, and the `rstl::auto_ptr< CInputStream >` teardown is reached through the vptr, so the stream classes' vtables are in scope  **[Landed 99.10%, `NonMatching`: the vtable slot is +8 and it is the *deleting* destructor; see above.]** |
| `fn_802FC63C` | 0x802FC63C | 0x1E0 = 480 | 48 | `LoadNewResourceSync(tag, int, int, extBuf)`: a seek, an **optional caller-supplied buffer** (`cmplwi r30,0` / `beq` picks between the caller's pointer and a fresh `CMemory::Alloc`), then the same compressed/uncompressed tail as `fn_802FC4D8` | everything `fn_802FC4D8` needs; the frame is 48 rather than 32 because of the two extra arguments |
| `fn_802FC81C` | 0x802FC81C | 0x7C = 124 | 32 | `LoadResourcePartAsync`: `fn_802FCEEC`, `GetOffset`, then `AsyncSeekRead(dvd, arg5, arg4, arg3 + offset, 0)` - the offset is **added to the caller's base**, so r7 is a pointer and r5/r6 are a length and an origin | only `CDvdFile::AsyncSeekRead` and the `SResInfo` accessors, so **this is the cheapest of the seven** |
| `fn_802FC898` | 0x802FC898 | 0x1D0 = 464 | 64 | the **grouped-resource** path: `fn_802FB994` (unnamed, elsewhere in the loader), then `GetGroupedSize` and a null test on it at 0x802fc900, then an `AsyncSeekRead` and a `new (28)` | `fn_802FB994` is unwritten and unnamed, and it is the biggest single unknown in the block **[Measured: it needs three more unnamed functions and it proves `x0_aramList` is not a list of `SPakLoadEntry`; see above.]** |
| `fn_802FCA68` | 0x802FCA68 | 0x80 = 128 | 32 | `LoadResourceAsync`: `fn_802FCEEC`, `GetSize`, `GetOffset`, then `AsyncSeekRead(dvd, buf, (size + 31) & ~31, offset, 0)` | as `fn_802FC81C`, plus the 32-byte rounding |
| `fn_802FCC44` | 0x802FCC44 | 0xA0 = 160 | 32 | `GetResIdByName(const char*)`: the **same two-list walk as `fn_802FCDE8`** but comparing with `CPakFile::GetResIdByName` instead of `GetResInfo`, over `+0x1C`/`+0x20` and then `+0x34`/`+0x38` | its callee `CPakFile::GetResIdByName` is already written (retail 0x803236CC, `src/Kyoto/CPakFile.cpp`), so this is 160 bytes of loop and nothing else |

**The order to write them in is `fn_802FC81C` and `fn_802FCA68` first** - they are the only two
that call nothing but `CDvdFile` and the `SResInfo` accessors, 252 bytes between them, and they
are not adjacent (0x802FC898..0x802FCA68 is `fn_802FC898` between them), so they are two units.
`fn_802FCC44` is next. The three `CMemoryInStream`/`CLZOInputStream` bodies are **one** shared
blocker and should be one lane, not three.

**[Superseded 2026-09-26 by lane m2: that order was right, and the recommendation held. All four
of the first four landed at 100% with no iteration; `fn_802FC420` - fourth on the list, described
above as needing two named constants that do not in fact need naming - was 100% on the first build.
The two `CMemoryInStream` bodies are 99.10% and 98.04% and are blocked on register allocation, not
on anything in this table. See the section above.]**

### Still missing between this and a constructed `gpResourceFactory`

- **`CResFactory::AsyncIdle`** (0x802FA384, `size:0x10C` = 268 bytes), on the link-gap ratchet and
  called by the written `CMain::AsyncIdle` (boot-path step 21e). **The class's interior is now
  measured - from this function, its constructor and its destructor - and every member is named.**
  See "The `CResFactory` interior, measured" at the end of this file. `AsyncIdle` itself is still
  unwritten, but nothing about the layout blocks it any more: the four words it reads are
  `+0xA0`, `+0xB0`, `+0xCC` and `+0xD0`, and all four are named members in
  `include/Kyoto/CResFactory.hpp` as of lane `m3`.
- **`CResLoader::GetPakFile`** (0x802FBA68, `size:0xFC` = 252 bytes, on the ratchet) - written
  to **80.13%** and left `NonMatching` on purpose, with the range claimed so retail's bytes stay in
  the link. It is one shape away, not twenty: MWCC unrolls the node walk by eight **and peels the
  first eight iterations**, so retail's chunk count is `((idx - 8) + 7) >> 3` behind a
  `cmpwi r4,8`, and this build emits `(idx - count18) >> 3` with no peel and an object 0xE0 = 224
  bytes against 0xFC. `src/Kyoto/CResLoaderGetPakFile.cpp` carries the instruction map.
- **`CResLoader::GetPakCount`** (0x802FBC60, `size:0x10`) is **landed** - see below - and it is
  the model for the pair: the count of the two *finished* lists, `+0x18` and `+0x30`, deliberately
  not the loading list at `+0x48`.
- **`fn_802FC350` / `fn_802FC378`** - the insert, and the handshake's `stb r0,0(r30)`. The port has
  transcriptions in `src/MetroidPrime/PortGlobals.cpp`; the matching side does not, and
  `AddPakFileAsync` is a `Matching` unit that calls the former by name.
  **[Superseded 2026-09-26 by lane k4: both are landed** - `src/Kyoto/CResLoaderInsert.cpp`, a
  `Matching` unit at 100.00% / 2 of 2, and the transcriptions are gone. See "The pak insert,
  landed" above for the four things it took, including that the `stb` is the element's copy
  constructor and not a statement in the insert.
- **The 33 functions of `CPakFile`**, still `NonMatching` at 88.30% with nine below 100%. The
  constructor and the destructor - the two the pak chain actually needs - are **already 100%**.
  What blocks the unit is `reserve<rstl::vector<CPakFile::SResInfo>>` at 33.84%, which needs
  `include/rstl/rmemory_allocator.hpp` (out of line `allocate`, and `rs_new` where retail inlines
  `CMemory::Alloc` with a `CCallStack`), and `RebuildResourceLists` at 39.63%, which calls an
  unnamed `fn_80052220` where the port calls `reserve<rstl::vector<uint>>`. **A lane that owns
  `include/rstl/` unblocks all 33**, and with them the resource system.
- **`~CPakFile`'s `AsyncIdle` spin, which is a host hazard and is now bounded.** Measured, and the
  obvious reading is wrong: `AddPakFileAsync`'s same-call `delete` is **not** taken, because
  `fn_802FC378` clears the caller's flag byte (0x802fc3d0), and `fn_802FD174` only destroys an
  entry whose pak `IsCompletelyLoaded()`. All three retail paths are safe. The real hazard is
  `InitialHeaderLoad` returning **without advancing the phase** when the pak's first word is not
  0x30005 (0x80323F58) while `CInputStream::ReadInt32` has no bounds check: a foreign or truncated
  pak never reaches `kAP_Loaded`, so any host path that destroys a `CPakFile` without testing the
  phase loops forever and silently. `src/Kyoto/CPakFile.cpp` now bounds the pump under `TARGET_PC`
  and names the pak and the phase when it gives up; mwcceppc compiles retail's loop unchanged and
  the unit's scores are identical either way.

## Adjudication: `CGameGlobalObjects`' layout, and which lane was right

Lanes g1 and g3 both measured this area and **contradicted each other**, and both edited
`include/Kyoto/CResLoader.hpp`, `include/Kyoto/CResFactory.hpp` and this file. The disagreement
is resolved here, from the disassembly, because the merged tree cannot carry both.

`CGameGlobalObjects::CGameGlobalObjects` is `fn_800084A0`, and it builds its members in order:

```
800084a0:  bl     803096c4 <fn_803096C4>        ; r3 = r31 + 0
800084a4:  addi   r3,r31,4
800084a8:  bl     802fb154 <fn_802FB154>        ; r3 = r31 + 4
800084ac:  addi   r3,r31,228                    ;   0xE4
800084b0:  addi   r4,r31,4
800084b4:  bl     80301008 <fn_80301008>        ;   (this+0xE4, this+4)
800084b8:  addi   r3,r31,264                    ;   0x108
800084bc:  bl     80032008 <fn_80032008>        ;   r3 = r31 + 0x108
```

| claim | verdict |
| --- | --- |
| **`CResFactory` at +0, `CResLoader` at +4** | **both wrong, and superseded twice over.** `CResFactory` is at **`CGameGlobalObjects`+0x04** and `CResLoader` at **+0x08**. The ctor's *order* is all this row ever had to go on, and order does not say where the object starts - see the third correction at the end of this file. |
| **`CGameGlobalObjects.hpp`'s `char pad0[4]` is spurious** | **refuted.** It is a real four-byte member with a constructor (`fn_803096C4`) and a destructor (`fn_80309660`). `gpResourceFactory` is stored as `this+0x04`, and `gpResourceFactory` is the `CResFactory*`. What *is* defective is `CResFactory`'s size, four bytes too large - that is the "one defect" below, re-diagnosed. |
| **`CResLoader`'s first four members are four 0x18-byte `rstl::list` at +0x00/+0x18/+0x30/+0x48** | **g1, confirmed.** `AreAllPaksLoaded` is `lwz r0,92(r3)` = +0x5C = `x48.x14_count`, and `fn_802FD174` decrements that same word. |
| **`CResLoader` is 0x60 bytes** | **g1, wrong, and so was my own adjudication - see the correction below.** 0x60 is where g1's evidence *stops* - four lists - and it read that as the whole. **`CResLoader` is 0x70**: four 0x18 lists plus four words, confirmed by `CHECK_SIZEOF` and by the registrars using `CResFactory`+0x74, which is exactly where 0x04 + 0x70 lands. |
| **`CFactoryMgr` at `CResFactory`+0x74** | **neither, as stated.** `gpResourceFactory+0x74` is `CResLoader`+0x70, *inside* `CResLoader` — g3 divided by a `CResFactory` base that carries a 4-byte phantom pad. The registrars' map is a member of `CResLoader`, not a separate class after it. `CFactoryMgr`'s two `rstl::map`s and its four `Matching` methods are unaffected — that unit is 100% on its own bytes — but the *ownership* is `CResLoader`'s. **[Superseded: the ownership is `CResFactory`'s, not `CResLoader`'s — see the correction below. The +0x74 offset itself stands.]** |
| **`CResFactory` is 0xC8 / 0xD0 / 0xE4** | **none of the three. It is 0xE0.** See the correction below and, for the fix that is now landed, the third correction at the end of this file. |

### The one defect under all of it — diagnosed wrongly, then re-diagnosed

**As first written:** `include/MetroidPrime/CGameGlobalObjects.hpp:38` had `char pad0[4];`, so
in this tree `CResFactory` sat at +4 and **every offset measured from `CGameGlobalObjects` read 4
too high**. That symptom is real and measured. The cause was not the pad.

**What it actually was, and is now fixed (lane j4, 2026-09-26):** `CResFactory` is **0xE0** bytes,
not 0xE4. The four wrong bytes were the last four of the factory, not four in front of it, so
`CSimplePool` compiled to `this+0xE8` against retail's `this+0xE4` and everything below it was
4 too high for exactly the same reason. `CHECK_SIZEOF(CResFactory, 0xe0)` and
`uchar xac_[0x34]` are in `include/Kyoto/CResFactory.hpp`; the `pad0` line **stays**. The
unit-movement report for both the real fix and the pad deletion that was proposed instead is the
third correction at the end of this file.

### The other two corrections, which stand

- **Block 7's loop polarity was backwards.** Its body runs while `AreAllPaksLoaded()` is
  **false** — `clrlwi. r0,r3,24; beq body`.
- **`fn_802FCFF4`/`fn_802FCCF4` test `x28_aramFile` (field 25), not `worldPak` (field 26).**
  Writing `IsWorldPak()` compiles to `rlwinm ...,27,...` — a different bit, and a
  plausible-looking wrong answer.

## Why the `FStringTableFactory` promotion was reverted (lane h5, 2026-09-26)

This is the most valuable negative result of the lane, because the promotion *works* and then
costs two functions at 100%.

`FStringTableFactory` is retail 0x80312320, `size:0x64`, and it was **already at 100.00%** in
`src/Kyoto/Text/CStringTable.cpp` - a `NonMatching` unit, so its bytes were dtk's and it was not
in the linked DOL. Promoting it is what puts them there. The unit is
`src/Kyoto/Text/CStringTableFactory.cpp`, `Matching`, claiming `.text 0x80312320..0x80312434`,
and it is two functions rather than one because `CFactoryFnReturn`'s converting constructor is
defined in the header and mwcceppc emits it immediately after the factory (0x80312384, 0xB0) -
which is exactly where retail has it. Measured:

```
$ ./tools/flip_test.sh Kyoto/Text/CStringTableFactory.cpp
TEST Kyoto/Text/CStringTableFactory.cpp
  PASS  -> kept as Matching
$ sha1sum build/G2ME01/main.dol
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
$ ./build/tools/objdiff-cli report generate -o build/report.json
main/Kyoto/Text/CStringTableFactory  100.00% on all six measures, 2/2 functions
$ ./tools/gate.sh
per-function diff  matched 3131 -> 3131  linked 1754 -> 1756  (+2 functions at 100%)
  WORSE  main/Kyoto/Text/CStringTable :: GetIObjObjectFor__22TToken<12CStringTable>FRCQ24rstl24auto_ptr<12CStringTable>  100.00% -> 0.00%
  WORSE  main/Kyoto/Text/CStringTable :: GetNewDerivedObject__40TObjOwnerDerivedFromIObj<12CStringTable>FRCQ24rstl24auto_ptr<12CStringTable>  100.00% -> 0.00%
```

**The mechanism, and it is a new one for this tree.** The converting constructor calls
`TToken<CStringTable>::GetIObjObjectFor` and `TObjOwnerDerivedFromIObj<CStringTable>::GetNewDerivedObject`,
both weak inline template members, so the new object emits them - **and so did
`src/Kyoto/Text/CStringTable.o`, which is where they had been coming from all along.** Retail
has them at 0x80312434 and 0x80312460, immediately after the constructor; the new object puts
them at +0x2F0 and +0x31C, because three weak `__dt__` instantiations land between. Two symbols,
two owners, and the linked ELF keeps retail's addresses (verified: both still at 0x80312434 and
0x80312460 in `build/G2ME01/main.elf`, and the DOL sha1 never moved) - but objdiff now pairs the
vanilla functions with the *dropped* copies and reports 0.00% for both.

So the generalisation is the one to record:

> **A `Matching` DOL unit's object emits its weak template instantiations wherever it likes, and
> when one of them is a function retail also has somewhere else, the symbol has two owners.** The
> DOL can still be byte-perfect - `flip_test.sh` passes, the sha1 holds - while the *report* loses
> the function entirely. This is the report-side twin of the 0-byte-stub trap in
> `docs/research/port_link_stubs.md`, and the acceptance test does not catch it: the gate's
> per-function diff does, and only because the baseline was recorded on a clean tree.

The fix is not a source change - MWCC 2.7 has no `extern template` and no way to suppress a weak
instantiation's placement. It is one of: claim through 0x803124FC and make the order match (it
does not, and cannot), or move `GetIObjObjectFor`/`GetNewDerivedObject` into a unit of their own
once something can emit them at the right offset, or accept the two functions' loss. The lane
accepted the loss; the 276 bytes of the promotion are still correct and the recipe above is
what a future lane needs.

`FRuleSetFactory` is the same shape and fails one step earlier, for a different reason: its
`operator new` names a **`scope:local`** symbol (`@stringBase0` at 0x803AC548), a local cannot be
named from another translation unit, so the unit has to own those 8 bytes - and claiming them
breaks the link with ``undefined: '@stringBase0_803AC548'`` because the rest of the `CRuleSet`
range still refers to them. Both attempts are in this lane's diff history, unreverted-in-intent
but not landed.

## Correction to the adjudication above: `CResLoader` is 0x70, and my 0xE0 was a bad inference

The adjudication in the previous section says `CResLoader` is **0xE0 bytes**, derived from
`CGameGlobalObjects`'s constructor placing its next member at +0xE4 with `CResLoader` starting at
+4. **That derivation is wrong and the number is wrong.** The value is **0x70**.

The error is a specific one, and it is the same shape as a trap this project has hit repeatedly:

> **A constructor's next call is not a bound on the preceding member's size.** Members that need
> no construction - plain integers, pointers, POD aggregates - are never passed to a constructor,
> so any of them can sit between two constructed members and the gap is invisible to the ctor.
> `fn_800084A0` proves where the next *constructed* member begins; it proves nothing about how much
> of the object that member's predecessor occupies.

Lane h5 measured the real size two ways that do not depend on the ctor: `CHECK_SIZEOF` with
mwcceppc's own flags, and the fact that the 36 registrations pass `r3 = gpResourceFactory + 0x74`,
which is exactly `CResLoader + 0x70` for a loader at `CResFactory + 0x04`. Both agree, and both
disagree with me.

**The rest of the adjudication survives, and one row strengthens:**

| claim | verdict |
| --- | --- |
| `CResFactory` at `CGameGlobalObjects`+0, `CResLoader` at +4 | **both refuted** — the factory is at +0x04 and the loader at +0x08 |
| `CGameGlobalObjects.hpp`'s `char pad0[4]` is spurious | **refuted** — it is a real four-byte member, constructed at 0x800084A0 and destroyed at 0x800065F0 |
| `CFactoryMgr` is at `CResFactory`+0x74 | **confirmed by h5** - `addi r3,r31,116` appears 36 times in `AddPaksAndFactories` with `r31 = gpResourceFactory` |
| `CResLoader`'s first four members are four 0x18 lists at +0x00/+0x18/+0x30/+0x48 | **confirmed** |
| **`CResLoader` is 0xE0** | **refuted. It is 0x70.** |
| `CResFactory` is 0xC8 / 0xD0 / 0xE4 | **none of the three. It is 0xE0** — see the correction below |

**And the nesting is the part I had wrong conceptually.** `CResLoader` (at `CResFactory`+0x04,
0x70 bytes) and `CFactoryMgr` (at `CResFactory`+0x74) are **members *inside* `CResFactory`**, not
siblings of it in `CGameGlobalObjects`. I recorded them as siblings because the constructor calls
into +0 and then +4, which reads like two adjacent members of the outer object. It is one member
of the outer object with two members of its own. The last sentence of the paragraph above is
**also wrong and is superseded**: `CFactoryMgr` living inside `CResFactory` is not why
`CResFactory` is 0xE4, and 0xE4 was never its size. **It is 0xE0.**

## Third correction: the `pad0` line is **not** spurious, and `CResFactory` is **0xE0** (lane j4, 2026-09-26)

This is the third pass over the same four bytes, and it reverses the previous two. Both earlier
passes read only the **order** of `CGameGlobalObjects::CGameGlobalObjects`'s constructor calls. The
order says what is built first; it says nothing about where the object begins. The instruction
that settles it is eleven instructions later, in the same function, and it is a **store to a
global**:

```
80008528:  addi   r0,r31,4
8000852c:  addi   r5,r31,228                   ; 0xE4
80008530:  addi   r4,r31,264                   ; 0x108
80008534:  stw    r0,-28380(r13)               ; 0x80418EA4  gpResourceFactory = this + 0x04
80008540:  stw    r5,-28376(r13)               ; 0x80418EA8  gpSimplePool         = this + 0xE4
80008544:  stw    r4,-28372(r13)               ; 0x80418EAC  gpCharacterFactory... = this + 0x108
```

(`tools/sda.py` is the only supported way to read those displacements: `_SDA_BASE_` for G2ME01 is
0x8041FD80. Read against anything else you get a plausible wrong address.)

**`gpResourceFactory` is `this+0x04`, and `gpResourceFactory` is the `CResFactory*`.** Three
things make that identification independent of its name:

* `AddPaksAndFactories` addresses the factory manager as `gpResourceFactory`+0x74, 36 times;
* `fn_802FB154` - the constructor called on `this+0x04` - builds `+0x04` (`fn_802FD0F4`) and
  `+0x74` (`fn_802F98D0`) in itself, and writes a vtable at its own `+0x00`;
* `fn_802FB038` is its destructor, and `CGameGlobalObjects`'s destructor calls it on `this+0x04`
  (`addi r3,r30,4` / `bl 802fb038` at 0x800065DC/0x800065E4).

So `CResFactory` is at `CGameGlobalObjects`+**0x04**, `CResLoader` at +**0x08**, and there are four
real bytes in front of the factory. `include/MetroidPrime/CGameGlobalObjects.hpp`'s `char
pad0[4];` is **correct** and stays.

**The four bytes are not an anonymous `char[4]` either - they are a member with a constructor and
a destructor.** `CGameGlobalObjects`'s destructor destroys `this+0x00` **last of all the
sub-objects it tears down**, with `bl 80309660` at 0x800065F0, and the constructor calls
`bl 803096c4` on `this+0x00` first (0x800084A0). `fn_803096C4` is 0x4C bytes: it reads and writes
two `.sbss` bytes, calls `CARDInit` once behind a flag, and **never writes `*this`** - it is a
one-shot memory-card initialiser that takes `this+0x00` as an argument it does not use.
`fn_80309660`, 0x64 bytes, clears one of those flags and calls `fn_80309108(0)` and
`fn_80309108(1)`. The class is 4 bytes - a vptr and nothing else - and this tree has no name for
it; the header comment now says so instead of calling the slot a pad.

### What was actually wrong: `CResFactory` is 0xE0

The symptom both earlier passes described is real and is now explained: **every offset measured
from `CGameGlobalObjects` read 4 too high**. The cause was the factory's own size. `CResFactory`
is **0xE0**, not 0xE4 - four bytes too long, and the four bytes belong to the member *after* it.
Three independent measurements, all from `gpResourceFactory` as the base:

| measurement | value |
| --- | --- |
| `fn_802FB154`'s **last store**: `stw r6,220(r31)` at 0x802FB1E4 | `+0xDC`, four bytes, so the extent is **0xE0** |
| `CGameGlobalObjects`'s ctor builds `CSimplePool` on `this+0xE4` with `this+0x04` as its `IFactory&` (0x800084AC-0x800084B4) | 0xE4 - 0x04 = **0xE0** |
| the next member is built on `this+0x108` (0x800084B8) and `CSimplePool` is `CHECK_SIZEOF(..., 0x24)` | 0xE4 + 0x24 = **0x108** |

`~CResFactory` corroborates: `fn_802FB038` destroys `+0xC8`, `+0xB4` and `+0x9C` and nothing
higher, so there is nothing hidden in the last four bytes of a 0xE0 object.

Landed: `CHECK_SIZEOF(CResFactory, 0xe0)` and `uchar xac_[0x34]` in
`include/Kyoto/CResFactory.hpp`. **`CHECK_SIZEOF` never measured any of this** - it only checks
that a model agrees with itself. `CHECK_SIZEOF(CResFactory, 0xe4)` passed for as long as the model
said 0xE4, and so would `0xd0` or `0x30`. That is worth stating plainly, because two of the three
sessions that touched these four bytes reported a `CHECK_SIZEOF` "confirmation" as evidence for a
size that was wrong.

### The full `CGameGlobalObjects` layout, now that it is measured end to end

From the constructor (0x8000848C, 0xE4 bytes), `PostInitialize` (0x800083E0), the destructor
(0x80006518, 0x108 bytes) and the ctor's global stores. Offsets are retail's.

| offset | member | evidence |
| --- | --- | --- |
| +0x000 | unnamed 4-byte class, vptr only | ctor `fn_803096C4` at 0x800084A0, dtor `fn_80309660` at 0x800065F0 |
| +0x004 | `CResFactory`, **0xE0** | ctor `fn_802FB154` at 0x800084A8, dtor `fn_802FB038` at 0x800065E4, `gpResourceFactory = this+0x04` at 0x80008534 |
| +0x0E4 | `CSimplePool`, 0x24 | ctor `fn_80301008` with `(this+0xE4, this+0x04)`, `gpSimplePool = this+0xE4` |
| +0x108 | `CCharacterFactoryBuilder` | ctor `fn_80032008` at 0x800084B8, dtor at 0x800065C4, `gpCharacterFactoryBuilder = this+0x108` - **not modelled in this tree** |
| +0x130 | `gameState` (`rstl::single_ptr`) | `stw r0,304(r31)` at 0x800084E4, dtor at 0x800065B8, `gpGameState` |
| +0x134 | `memoryCard` | `stw r0,308(r31)` at 0x800084F4, dtor at 0x800065A4 |
| +0x138 | `stringTable` (`rstl::optional_object<TLockedToken<CStringTable>>`), **0x10** | dtor tests the engaged flag at `+0x144` (`lbz r0,324(r30)` at 0x80006580) then destroys `+0x138`; ctor clears `+0x144` at 0x80008500 |
| +0x148 | `renderer` | `stw r31,328(r29)` at 0x80008460, `lwz r0,328(r29)` at 0x80008438 and 0x80008464, `gpRender` |
| +0x14C | `inGameTweakManager` | `stw r0,332(r31)` at 0x8000851C, `gpTweakManager = *(this+0x14C)` at 0x80008550/0x80008554 |
| +0x150 | at least one more member | destroyed first (0x80006538), published as `lbl_80418EC8 = this+0x150` at 0x80008558 |

The unmodelled `CCharacterFactoryBuilder` is why `CGameGlobalObjects::PostInitialize` still reads
99.91% and not 100%: its `renderer` accesses are `lwz/stw 288(r29)` against retail's 328, exactly
0x28 low. It is 0x28 bytes by subtraction, and 0x108 + 0x28 = 0x130 which is where `gameState`
starts.

### The unit-movement report, and the two changes it covers

Both variants were built at this commit and measured against a baseline recorded on the clean tree
(`build/report.base.json`: 1,376 units, 3,155 matched functions, 1,771 linked, 383 complete
units). **Deleting `pad0[4]`** moves **no unit at all** and **two functions, both worse**, both in
`main/MetroidPrime/main`, a **`NonMatching`** unit:

```
=== UNITS: 0 moved, 0 added, 1376 total in baseline
  WORSE  main/MetroidPrime/main :: PostInitialize__18CGameGlobalObjectsFR10COsContextR10CMemorySys  99.88 -> 98.37
  WORSE  main/MetroidPrime/main :: __ct__18CGameGlobalObjectsFR10COsContextR10CMemorySys             28.35 -> 22.11
  BETTER main/MetroidPrime/main :: StreamNewGameState__5CMainFR12CInputStreami                     25.26 -> 25.26   (size change, same %)
```

`tools/lanediff.sh` says why, and it is the same instruction the `gpResourceFactory` store proves:
with the pad deleted the fourth argument to `AllocateRenderer` becomes `mr r6,r29` where retail has
`addi r6,r29,4`. The pad is the thing that makes that one instruction correct.

**`CResFactory` 0xE4 -> 0xE0** - the change that is landed - moves **no unit at all** and **two
functions, neither worse**:

```
=== UNITS: 0 moved, 0 added, 1376 total in baseline
  BETTER main/MetroidPrime/main :: PostInitialize__18CGameGlobalObjectsFR10COsContextR10CMemorySys  99.88 -> 99.91
  BETTER main/MetroidPrime/main :: StreamNewGameState__5CMainFR12CInputStreami                     25.26 -> 25.26   (size change, same %)
```

`addi r3, r29, 228` now matches retail's `addi r3, r29, 228`, which is the whole of the gain. Both
variants leave the DOL at `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` with all 86 REL hashes, so
neither is a link change; the only thing that moves is how `CGameGlobalObjects`'s members are
addressed, and only `main.cpp` addresses them.

**Why the blast radius is one unit, as a general rule:** deleting a member at the front of
`CGameGlobalObjects` shifts the *absolute* offsets of `CGameGlobalObjects`'s own members and
nothing else. `CResLoader` at `CResFactory`+0x04 and `CFactoryMgr` at `CResFactory`+0x74 are
offsets from a `CResFactory*`, and every caller of those two reaches them through
`gpResourceFactory`, so a change to `CGameGlobalObjects` cannot touch them. `CResFactory`'s
*extent* is a different matter, and it is the one that reaches `CSimplePool` and everything after
it. **A class's size matters to its owner's layout; its members' offsets do not.** That asymmetry
is why three sessions expected a tree-wide event here and went looking for one.

## The `CResFactory` interior, measured (lane `m3`, 2026-09-26)

**This is what `AsyncIdle` was for.** The lane briefing called it "the largest single item here
and the least well-anchored" and the previous characterisation said it "needs the `CResFactory`
member model past +0x9C, which is not in `include/Kyoto/CResFactory.hpp` and is not derivable
from anything this lane measured". Both are now wrong: `AsyncIdle` reads four words, and retail's
own constructor writes **every** member of the class except one, in twenty instructions that are
free to read. The constructor is the map; `AsyncIdle` is the corroboration.

### `fn_802FB154`, the constructor, is the whole class in twenty instructions

`CGameGlobalObjects` calls it on `this+0x04` and retail calls nothing else on a `CResFactory`, so
it is a leaf that does nothing but initialise. Its stores, with `r31` = `this`:

```
802fb15c  lis  r4,0x803B      / 802fb164  addi r0,r4,6584   ; 0x803B19B8 = vtable for IFactory
802fb174  stw  r0,0(r31)                                        ; ... stored, then overwritten:
802fb170  lis  r3,0x803C      / 802fb178  addi r0,r3,-20728 ; 0x803BAF08 = vtable for CResFactory
802fb180  stw  r0,0(r31)                                        ;     the real vptr
802fb17c  addi r3,r31,4       / 802fb184  bl 802fd0f4        ; CResLoader   (+0x04, 0x70)
802fb188  addi r3,r31,116     / 802fb18c  bl 802f98d0        ; CFactoryMgr  (+0x74)
802fb190  addi r7,r31,168                                     ; &x9c.xc_empty_prev = 0xA8
802fb198  stw  r7,160(r31)                                    ; +0xA0
802fb1a8  stw  r7,164(r31)                                    ; +0xA4
802fb1b0  stw  r7,168(r31)                                    ; +0xA8
802fb1b4  stw  r7,172(r31)                                    ; +0xAC
802fb1b8  stw  r6,176(r31)                                    ; +0xB0 = 0
802fb19c  addi r0,r31,212                                     ; &xc8.xc_empty_prev = 0xD4
802fb1d4  stw  r0,204(r31)                                    ; +0xCC
802fb1d8  stw  r0,208(r31)                                    ; +0xD0
802fb1dc  stw  r0,212(r31)                                    ; +0xD4
802fb1e0  stw  r0,216(r31)                                    ; +0xD8
802fb1e4  stw  r6,220(r31)                                    ; +0xDC = 0   <- the object's last store
802fb1a0  lbz  r5,8(r1)  /  802fb1ac  lbz r4,12(r1)           ; +0xB4, +0xB5, from ITS OWN FRAME
802fb1bc  stb  r5,180(r31) / 802fb1c0  stb r4,181(r31)
802fb1c4  stw  r6,184(r31) / 802fb1c8  stw r6,188(r31)        ; +0xB8, +0xBC = 0
802fb1cc  stw  r6,192(r31) / 802fb1d0  stw r6,196(r31)        ; +0xC0, +0xC4 = 0
```

**Two `rstl::list`s, four pointers at one address and a zero each.** That is this tree's
`rstl::list`'s own empty state - `x4_start`, `x8_end`, `xc_empty_prev`, `x10_empty_next` all
`&xc_empty_prev`, `x14_count` 0 - so the members are at **+0x9C and +0xC8**, 0x18 bytes each, and
**`x0_allocator` at +0x9C and +0xC8 gets no store at all**, which is what an empty allocator
compiles to and is why those two words read as uninitialised in any dump.

| offset | member | evidence |
| --- | --- | --- |
| +0x000 | vptr | two stores, `IFactory`'s then `CResFactory`'s |
| +0x004 | `CResLoader`, 0x70 | `bl 802fd0f4` here, `bl 802fd034` in the destructor |
| +0x074 | **`CFactoryMgr`, 0x28 - not 0x38** | `bl 802f98d0` here, `bl 802f9784` in the destructor |
| +0x09C | `rstl::list<T>` #1, 0x18 | the four self-pointers at +0xA0..+0xAC and +0xB0 = 0 |
| +0x0B4 | unidentified, 0x14 | two ctor bytes, then four zeroed words; see below |
| +0x0C8 | `rstl::list<T>` #2, 0x18 | the four self-pointers at +0xCC..+0xD8 and +0xDC = 0 |

**`CFactoryMgr` is 0x28 bytes, and that is a correction to this file's own header.** Two
`rstl::map`s of 0x14 each is 0x28; `fn_802F9784`, the deleting destructor `~CResFactory` calls on
`+0x74`, destroys `this+0x14` and `this+0x00` and nothing else; the 36 registrations and both
dispatch sites address no other field. The four unnamed `uint`s the header carried at +0x28..+0x38
are **not this class's** - they are `CResFactory`+0x9C..+0xAC, the first half of the list above.
`CHECK_SIZEOF(CFactoryMgr, 0x28)` replaces `0x38`, and `CResFactory` is still 0xE0 because the
list the manager was swallowing starts exactly where the manager now ends.

### `AsyncIdle`'s four words, and what each one is

| retail | member | what it is |
| --- | --- | --- |
| `lwz r0,160(r26)` / `stw r0,8(r1)` (0x802FA434) | `+0xA0` = `x9c_loading.x4_start` | copied to the stack and handed to `fn_802FA1BC` as the second argument. So the pump's per-request state is the *head* of the first list. |
| `lwz r0,176(r26)` / `cmpwi r0,0` (0x802FA428) | `+0xB0` = `x9c_loading.x14_count` | tested twice, once before the timed section and once as the loop condition at 0x802FA470. `if (x9c_loading.size())` - the first list is the outstanding-build queue and the whole second half of `AsyncIdle` is a no-op when it is empty. |
| `lwz r31,204(r26)`, `lwz r31,4(r31)`, compared against `208(r26)` | `+0xCC` = `x4_start` and `+0xD0` = `x8_end` | the walk. `r31` is the node, `+4` is `node::x4_next`, and the loop ends at the sentinel - exactly `for (node* it = begin(); it != end(); it = it->x4_next)`. |
| `addi r3,r26,200` / `bl 802fb2e4` (0x802FA3DC) | `+0xC8` = the second list | the erase. So the vcall on `*(node+0x14)` is a "is this finished" test and anything true is unlinked from the second list. |

**`AsyncIdle`'s timed half has nothing to do with the class layout at all**, and that is worth
recording because it looks like it does. `0x80411050` (`Kyoto/Basics/CStopwatch.cpp` claims
0x80411050-0x80411068) is a 64-bit divisor; `fn_802FA1BC(this, &xA0, now - start) / divisor` is
elapsed-time-in-units, and the loop runs while the result is below `time`. The word at
`Kyoto/Basics/CStopwatch.cpp`'s claim is retail's tick frequency, not a member.

### The one member still unidentified: +0xB4, 0x14 bytes

Everything below is measured; the *name* is not, and no lane should invent one.

* The constructor sets `+0xB4` and `+0xB5` from **its own incoming stack frame** - `lbz r5,8(r1)`
  and `lbz r4,12(r1)`, with no argument ever passed. `CGameGlobalObjects` calls it as
  `addi r3,r31,4 / bl 0x802FB154` and sets nothing, so on the only call in the DOL retail reads
  two uninitialised bytes. **It is the one place in this class where retail's own code reads
  garbage**, and it is why `CResFactory::CResFactory` is still a port-only stub.
* The constructor then zeroes +0xB8, +0xBC, +0xC0 and +0xC4 and nothing else, and `~CResFactory`
  destroys it with `fn_802FB0E0` (0x802FB0E0, 0x14 bytes), which tests `*(this+0x10)`, then
  `*(this+0x04)`, and recurses through `fn_802FB1FC` (0x802FB1FC) - a two-way tree free ending in
  `CMemory::Free(this)`. Four words freed as two trees is **two `rstl::map`s at +0x04 and +0x10**,
  i.e. at +0xB8 and +0xC4.
* `fn_802FAAE4` - the helper both `Build` and `CancelBuild` call first - walks
  `fn_802FAA20(&xB4_x, tag)`, whose first instruction is `lwz r7,16(r3)`, so **the lookup is
  rooted at +0xC4**, and the sentinel it compares against is `&xB4_x[+0x08]` = `CResFactory`+0xBC.
  It returns `this+0xA4` when the tree is empty and the found node's `+0x18` otherwise, so it
  answers "is this tag already being built", and `CResFactory::Build`'s **entire** first half is
  that question.

### What landed from it, and what it cost

* **`CResFactory::Build`** - retail `fn_802FA960`, 0x802FA960, 0xC0 = 192 bytes - is a `Matching`
  unit, `src/Kyoto/CResFactoryBuild.cpp`, **100.00%, 1 of 1, `flip_test` PASS**, and the key
  function of the third and last frame-0 vtable. `config/G2ME01/symbols.txt` renames it and the
  other four members of the class, so the vtable's six slots are all named:
  `__dt__11CResFactoryFv`, `Build__11CResFactoryFRC10SObjectTagRC15CVParamTransfer`,
  `BuildAsync__11CResFactoryFRC10SObjectTagRC15CVParamTransferPP4IObj`,
  `CancelBuild__11CResFactoryFRC10SObjectTag`, `CanBuild__11CResFactoryFRC10SObjectTag` and
  `GetResourceIdByName__11CResFactoryCFPCc`. **Those six names are not guessed** - compiling the
  class declaration with `tools/probe_cc.sh` and reading the object's `.data` relocations gives
  them, in that order, at +0x08..+0x1C, and MWCC spells a const member `C` immediately before the
  parameter list, which is why the last one is `CFPCc` and not `FPCc`.
* **Two of the five slots are 36-byte forwarders retail placed in unrelated CGame code**:
  `CanBuild` at **0x8008F3C8** and `GetResourceIdByName` at **0x80006B80**, each a prologue,
  `addi r3,r3,4` - the `CResLoader` at `CResFactory`+0x04 - a tail call (`fn_802FCBD0` and
  `fn_802FCC44` respectively) and an epilogue. **0x80006B80 is inside `MetroidPrime/main.cpp`'s
  claimed 0x800053B8-0x80009880**, so promoting it means re-splitting that unit. They are the
  cheapest thing in the class and a lane should take them first.
* **`~CResFactory` is byte-exact and still not landed**, and the reason is a real contradiction
  rather than a missing effort: see "The vtable and the destructor that cannot own it" below.

### The vtable and the destructor that cannot own it

`vtable for CResFactory` is at **0x803BAF08**, 0x20 bytes, and `config/G2ME01/symbols.txt` calls it
`lbl_803BAF08`. MWCC's vtable is two zero words (offset-to-top, and the typeinfo a `-RTTI off`
build zeroes), then one slot per virtual in declaration order, and the vptr points at the **first
of the two zero words** - so `vtable for CIOWin` at 0x803B1BA0 and `vtable for CResFactory` at
0x803BAF08 are the same shape, and `CResFactory`'s has **six slots and no NULL** because the class
overrides all five of `IFactory`'s pure virtuals.

A `Matching` unit can own it, and getting there took four measurements, three of which are
counter-intuitive:

1. **The destructor has to be out of line, and then `Build` stops being the key function.** With
   `~CResFactory() {}` inline - which is what this tree had - the first non-inline virtual is
   `Build`, and mwcceppc does emit the vtable into `Build`'s unit; but it also emits a *weak*
   `__dt__11CResFactoryFv` into every unit that touches the class, plus the whole
   `CFactoryMgr`/`rstl::red_black_tree` teardown as weak instantiations. Declaring
   `~CResFactory()` and defining it in a unit of its own gives an object with **exactly** the
   destructor and the vtable in it - **and the vtable is emitted there, not in `Build`'s**:
   MWCC 2.7 puts a vtable in the unit defining the class's *first* virtual, and with the
   destructor merely *declared* it emits no vtable at all from the unit that defines `Build`.
   The unit was `Kyoto/CResFactoryDtor.cpp` during the lane and is not in the tree; what is
   landed is the port-only `Kyoto/CResFactoryPortVirtuals.cpp`, which is where the vtable has to
   come from for the port.
2. **Five calls, three of them written and two of them the compiler's, in descending offset
   order.** `CResFactory::~CResFactory() { fn_802FB370(&xc8_active, -1);
   fn_802FB0E0(&xb4_pending, -1); fn_802FB370(&x9c_loading, -1); }` - with `~CFactoryMgr` and
   `~CResLoader` **declared** out of line in their own headers - compiles to `fn_802FB038`
   **instruction for instruction, 0xA8 = 168 bytes**: the two vtable stores, the
   `this == nullptr` early return and the `delete this` tail included, and the two
   `addi r3,r30,N / li r4,-1 / bl` pairs for the class members that mwcceppc appends itself.
   Those two declarations are what keeps the map and list teardowns out of the object - MWCC calls
   *any* out-of-line destructor with the flag in r4, so neither class needs to be polymorphic and
   `CFactoryMgr`'s two maps stay at +0x00 and +0x14 where the 36 registrations address them.
   **They are not landed**: they were reverted with the rest of the destructor, because the
   destructor's only consumer is a unit that cannot be `Matching` (point 3), and declaring them
   would add two link requirements to the port for no gain. A lane that wants the destructor needs
   them back.
3. **And then the object still carries two things retail does not have there, so it is not
   `Matching`.** `__vt__8IFactory` - 0x20 bytes of `.data` that retail has at 0x803B19B8 **with a
   zero in the destructor slot; the whole 0x20 is zeros** - and a weak `__dt__8IFactoryFv`,
   0x48 bytes of `.text` at 0x802FB0E0, which is retail's `fn_802FB0E0`. Both come from
   `virtual ~IFactory() {}` being inline in `Kyoto/CResFactory.hpp`.
4. **Making the base destructor pure fixes the `.data` and breaks the body**, measured: the
   destructor becomes 0x9C and MWCC replaces the base-vptr store with
   `mr r3,r30 / li r4,0 / bl __dt__8IFactoryFv`. That is the same trap
   `docs/research/boot_probe.md` records for `CIOWin` and `CMainFlow`, and it is why
   `docs/research/port_link_gap.md` warns against it.

**So retail's all-zero `__vt__8IFactory` and retail's base-vptr store are two measurements this
header cannot satisfy at the same time**, and until one of them is explained the vtable's `.data`
stays with dtk's fill. The port does not care: `src/Kyoto/CResFactoryPortVirtuals.cpp` is
port-only, defines `~CResFactory` - which is the key function in GCC too, so the vtable is
emitted there - and the reachstub is gone. `link_check.sh`: **330 -> 332 undefined, gross +3
(`fn_802FAAE4`, `fn_802FA1BC`, `fn_802FA7D4`) against gross -1 (`vtable for CResFactory`), net
+2**, 0 duplicates.
