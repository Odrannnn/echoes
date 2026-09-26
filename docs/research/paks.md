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
prologue), the `COsContext&` in r4 (`mr r30,r4` at 0x80007184, used once at 0x8000741C).
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
| 7 | 0x80007418-0x800074BC | 164 | `gpController = IController::Create(osContext)`, the store to 0x804192E0, then the **whole load loop** (its back-edge is the `beq 0x80007430` at 0x8000748C and it is entered once by the `b 0x80007480` at 0x8000742C): `fn_802FCCE4(&resLoader)` and, **while it is FALSE**, `fn_802FCCF4(&resLoader)`, `fn_801F05D0(lbl_80418EC8)`, `fn_80180EC0(&err)`, `fn_802C1E60()`, `fn_80180E94(&err)`, `fn_802C1658()`, `CMain::CheckReset()`, a vcall on the `CDvdRequest*` at r1+12 (slot +0x10) and `fn_801F025C(&r1+0x1C)` | **yes** - both pump functions; see the correction below |
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

Two `Matching` units, both `flip_test.sh` PASS with the DOL sha1 held at
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` and all 86 RELs `cmp`-equal:

| unit | range | functions | score |
| --- | --- | --- | --- |
| `src/Kyoto/CResLoaderPakPump.cpp` | `.text 0x802FCCE4..0x802FCD90`, 172 B | `AreAllPaksLoaded`, `AsyncIdlePakLoading` | 100.00% / 2 of 2 |
| `src/Kyoto/CResLoaderGetPakCount.cpp` | `.text 0x802FBC60..0x802FBC70`, 16 B | `GetPakCount` | 100.00% / 1 of 1 |

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

### Still missing between this and a constructed `gpResourceFactory`

- **`CResFactory::AsyncIdle`** (0x802FA384, `size:0x10C` = 268 bytes), on the link-gap ratchet and
  called by the written `CMain::AsyncIdle` (boot-path step 21e). Characterised but not written;
  see the report for the block map. It needs the `CResFactory` member model past +0x9C, which is
  not in `include/Kyoto/CResFactory.hpp` and is not derivable from anything this lane measured.
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
