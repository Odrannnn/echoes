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
+0x00. The CResLoader itself starts at +0x00 of `CGameGlobalObjects`.

| # | retail range | bytes | what | written? |
| --- | --- | --- | --- | --- |
| 1 | 0x80007170-0x800071A0 | 48 | `CGraphics::SetViewPointMatrix(sIdentity__12CTransform4f)` then `SetModelMatrix` on the same object. `sIdentity__12CTransform4f` is .bss 0x804173D4. Both are **static**, which is why there is no `this` load before either | **yes** |
| 2 | 0x800071A0-0x800071E8 | 72 | `if (CDvdFile::FileExists("Strings.pak")) resLoader.AddPakFileAsync("aram:Strings", false, false);` | **yes** |
| 3 | 0x800071E8-0x80007278 | 152 | the `Standard.NTWK` read: `CDvdFile("Standard.NTWK")`, a `CCallStack`, a 2-byte `CMemory::Alloc`, `CDvdFile::SyncRead` into it, a `CMemoryInStream`, and five unnamed helpers | **no** |
| 4 | 0x80007280-0x800073A4 | 292 | six unconditional `AddPakFileAsync` calls: `NoARAM`, `AudioGrp`, `aram:MiscData`, `aram:TestAnim` (**true**, false), `aram:MidiData`, `aram:GGuiSys` - all with the third argument 0 | **yes** |
| 5 | 0x800073A4-0x800073E8 | 68 | `if (CDvdFile::FileExists("FrontEnd.pak")) resLoader.AddPakFileAsync("FrontEnd", false, **true**);` - the only world pak here | **yes** |
| 6 | 0x800073E8-0x80007418 | 48 | `CErrorOutputWindow(true)` on the stack at r1+200, `fn_802BE8E8(1)`, and `CGraphics::SetViewport(0, 0, mViewport.mWidth, mViewport.mHeight)` - the two widths are `lwz r5,8(r6)` / `lwz r6,12(r6)` off `mViewport__9CGraphics` (.data 0x803B9FE8), and note the first two arguments are literal 0, **not** `mViewport.mLeft`/`mTop` | **no** |
| 7 | 0x80007418-0x800074BC | 164 | `gpController = IController::Create(osContext)`, the store to 0x804192E0, then the **whole load loop** (its back-edge is the `beq 0x80007430` at 0x8000748C and it is entered once by the `b 0x80007480` at 0x8000742C): `fn_802FCCE4(&resLoader)` and, while it is true, `fn_802FCCF4(&resLoader)`, `fn_801F05D0(lbl_80418EC8)`, `fn_80180EC0(&err)`, `fn_802C1E60()`, `fn_80180E94(&err)`, `fn_802C1658()`, `CMain::CheckReset()`, a vcall on the `CDvdRequest*` at r1+12 (slot +0x10) and `fn_801F025C(&r1+0x1C)` | **no** |
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

## The 36 factory registrations - the whole table, and why none of it is written

864 bytes, exactly 24 bytes each: `lis r3` / `lis r4` / `addi r5,r3,N` / `addi r3,r31,116` /
`addi r4,r4,N` / `bl`. r3 is always `CResFactory`+0x74, which is `CFactoryMgr`+0x18
(`CFactoryMgr` is at +0x5C and is 0x38 bytes, so it ends at +0x94 where `CResFactory::x94_` is).
r4 is a FourCC built big-endian, r5 is the address of a **named retail factory function**.

**35 of the 36 factory functions are neither defined in the port nor on the link-gap ratchet**,
so writing these 36 registrations as calls would add 35 symbols to close none, plus the two
registrars. That is 37 ratchet entries for 864 bytes of a `NonMatching` unit whose bytes are
not in the link. The table is recorded here instead, which costs nothing and is what a lane
writing those 36 functions needs.

The one that *is* already there is `FRuleSetFactory`, because `src/MetroidPrime/CRuleSet.cpp`
is in `files.cmake`. **The one that is missing for a reason worth knowing is
`FDependencyGroupFactory`: `src/Kyoto/CDependencyGroup.cpp` defines it and `CDependencyGroup`
is a `Matching` DOL unit (11/11), but that file is not in `files.cmake`, so the port build has
no definition of it.** That is a one-line `files.cmake` change, not decompilation - and it is
the only one of the 36 that is not a decompilation job.

The two registrars:

| symbol | address | how many | how to tell them apart |
| --- | --- | --- | --- |
| `fn_802F96E0` | 0x802F96E0 | 33 | calls `fn_802F9378(out, this+0x14, fourCC)` - a sub-list at +0x14 |
| `fn_802F963C` | 0x802F963C | 3 | calls `fn_802F9428(out, this+0x14, this)` - the manager itself, not the fourCC |

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
