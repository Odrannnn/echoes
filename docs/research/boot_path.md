# What stands between this tree and a rendered frame

Written 2026-09-25 from a clean build of commit `a1d3702`, every address and size read out of
`build/G2ME01/main.elf` with `build/binutils/powerpc-eabi-objdump`. Nothing here is recalled.

**The answer, first, because it is the thing a later session should not have to derive:** the
port does not link, so no part of this path can be executed, and this document is therefore a
map, not a result. `python3 tools/link_gap.py --rebuild` reports **724 unaccounted symbols** over
124 objects, and `MP_SDK_HEADERS_ONLY=ON` means there is no executable target to attempt a link
with even if the count were zero. What *can* be made correct is the order and the identity of
every step, and that is what is measured here.

## The three corrections this map starts from

Two of them change what a lane should be pointed at, and one of them invalidates a claim that
`docs/research/port_link_gap.md` carries.

### 1. Retail Echoes has no `CMain::OpenWindow`, and no `COsContext::OpenWindow`

`docs/research/port_link_gap.md` says the two things that block a first frame are
"`CMain::RsMain` is an empty body and `CMain::OpenWindow` is unimplemented". The first is
right. The second is wrong: **`CMain::OpenWindow` does not exist in retail.** Four
independent measurements:

1. `config/G2ME01/symbols.txt` names **19** `CMain` methods - `__ct__`, `__dt__`, `RsMain`,
   `InitializeSubsystems`, `ShutdownSubsystems`, `AsyncIdle`, `CheckReset`, `CheckTerminate`,
   `DrawDebugMetrics`, `MemoryCardInitializePump`, `AddWorldPaks`, `EnsureWorldPaksReady`,
   `ResetGameState`, `StreamNewGameState`, `FillInAssetIDs`, `SetFrameTimeMinimum`,
   `SetGameFrameDrawn`, `SetMaxSpeed`, `fn_80008A1C`. `OpenWindow` is not one of them, and the
   string does not occur anywhere in `config/`.
2. The string `OpenWindow` occurs **nowhere** in `powerpc-eabi-objdump -d` of the whole DOL
   (985,921 lines of disassembly, every `.text` symbol in `main.elf`).
3. `CMain::RsMain` (0x80005C6C, 0x864 = 2,148 bytes) is fully disassembled and makes **no**
   call on `x0_osContext`. Its only two uses of that pointer are `lwz r4,0(r31)` at
   0x80005CDC and 0x80005E20, feeding `CGameGlobalObjects::CGameGlobalObjects` and
   `CGameArchitectureSupport::CGameArchitectureSupport`.
4. Retail's window/VI bring-up is in `main` - the **caller** of `InvokeCMain` - and the chain
   has exactly one caller at each hop, so it is complete:

   | step | address | size | what it is |
   | --- | --- | --- | --- |
   | `main` | 0x801EFB00 | 0x168 | the DOL's own entry; builds the OS context, the memory system and the graphics object, then calls `InvokeCMain` |
   | `fn_802BE85C` | 0x802BE85C | 0x48 | constructor of the 8-byte object at `main`'s r1+8, which `main` passes as `InvokeCMain`'s **sixth** argument. Guards on a global "already initialised" byte, then calls the next one |
   | `fn_802C329C` | 0x802C329C | 0x138 | `VIInit`, `VISetBlack(TRUE)`, then the next one; also reads `osContext+0x24` and `osContext+0x2C` - the first external framebuffer and its size - through `fn_802C33F8` |
   | `fn_802C2FD4` | 0x802C2FD4 | 0x284 | **this is retail's window bring-up**: `VIGetTvFormat` -> `GXAdjustForOverscan` -> two framebuffers -> `VIConfigure` -> `VIFlush` -> `GXInit` -> `GXSetCopyFilter` |

   `fn_802C2FD4` is the same shape as our `COsContext::OpenWindow`
   (`src/Kyoto/Basics/COsContext.cpp`), but that is because the Metroid Prime port's
   `COsContextDolphin.cpp` is the reference our version was written from - as that file's own
   header says. In *this* DOL the render mode `fn_802C2FD4` adjusts is the **static** at
   0x80417264, which belongs to CGX: nothing in the DOL ever reads `COsContext`+0x30. The only
   `COsContext` fields retail's chain touches are +0x24 and +0x2C.

   So `include/MetroidPrime/CMain.hpp:51`'s `void OpenWindow();` is Metroid Prime carry-over.
   **Do not write a body for it and call it retail's.** `src/MetroidPrime/PortBoot.cpp` gives it
   a host-only definition for the port's own reasons, with this measurement in its header.

### 2. The boot path starts at `main` (0x801EFB00), not at `InvokeCMain`

`InvokeCMain` is a seam, not the start. The five objects retail's `main` builds before calling
it are where the window, the arena and the DVD bootstrap actually live:

| what `main` builds | retail | the port passes |
| --- | --- | --- |
| `COsContext osContext(true, true)` at r1+44, 0x6C bytes | `fn_8028C09C` (0x8028C09C, 0xE0) - `OSGetLanguage`, a `fn_8028BF68` call, `OSGetConsoleType` and a switch, then seven zero stores from +0x14 to +0x2C | a real `COsContext` (`platform/main.cpp:123`) |
| a 12-byte saved-region helper at r1+20 | `fn_801EFC68` (0x801EFC68, 0x84) - `OSGetSavedRegion`, `OSSetSaveRegion(0,0)`, a 128-byte copy into a global | **`nullptr`** |
| `CMemorySys memorySys(osContext, allocator)` at r1+16 | `CMemorySys::GetGameAllocator`, `fn_801EFE6C`, `CMemorySys::CMemorySys` (0x802CE698) | a real `CMemorySys` (`platform/main.cpp:124`) |
| a global byte at 0x804198E8 forced to 1 | `lbz r0,-25752(r13)` / `stb` at 0x801EFB68-0x801EFB78 | not set - `platform/main.cpp` has no equivalent |
| the 8-byte graphics object at r1+8 | `fn_802BE85C` (see table above) | **`nullptr`** |
| a DVD-read spin loop | `fn_801EFEFC` (0x801EFEFC, 0x1C4) + `fn_801EFECEC` (0x801EFECEC, 0x164) | `aurora_dvd_open` in `platform/main.cpp:100` |

`platform/main.cpp` therefore substitutes Aurora for the last row and passes null for the
second and fifth. That is a real, recorded divergence, and it is why the port's
`COsContext` and `CMemorySys` are built in the entry point rather than inherited.

### 3. The frame loop is not 300 functions of decompilation away. It is 12 named
infrastructure symbols and two null pointers away.

`CGameArchitectureSupport::UpdateTicks` is written, and `Update` is byte-exact against retail
0x80007A14. Disassembling retail's `UpdateTicks` (0x80007BC0, 0x228) shows it calls exactly
one thing the port does not have, twice, and one thing it half has:

| callee | retail | status |
| --- | --- | --- |
| `CIOWinManager::PumpMessages(CArchitectureQueue&)` | 0x800496A0, 0xC4 | **missing**, and it is on the link-gap ratchet (`_ZN13CIOWinManager12PumpMessagesER18CArchitectureQueue`) |
| `CInputGenerator::Update(float, CArchitectureQueue&)` | 0x8001D888, 0x1FC, unnamed in retail | **missing**, on the ratchet (`_ZN15CInputGenerator6UpdateEfR18CArchitectureQueue`) |
| `CIOWinManager::AddIOWin` / `RemoveAllIOWins` / ctor / dtor | 0x80049BDC 0x17C, 0x80049A18 0x80, 0x80049DE8 0x28, 0x80049D84 0x64 | **missing**, all four on the ratchet |
| `CStopwatch::CSWData::Initialize` and `::Wait` | 0x8028C17C, 0x7C; 0x8028C1F8, 0x94 | **missing**, both on the ratchet. `CStopwatch::Reset` and `GetElapsedTime` are inline in `include/Kyoto/Basics/CStopwatch.hpp` but reach `Initialize`, so `UpdateTicks` pulls them in without naming them |
| `MakeMsg::CreateFrameBegin` / `CreateTimerTick` / `CArchitectureQueue::Push` / `rc_ptr::ReleaseData` | 0x80048A80, 0x80048DC8, 0x80007A80, 0x80008F40 | written, in `src/MetroidPrime/main.cpp` |
| `CGameArchitectureSupport::UnloadAudio` | 0x8029EF20, 0xAC (unnamed; the destructor calls it at 0x80007E34) | **missing**, on the ratchet (from the destructor) |
| `CMainFlow::CMainFlow` | 0x8001E008, 0x68 | **missing**, on the ratchet (from the constructor) |
| `CMain::ResetGameState` | 0x80003A48, 0x1A0 | **missing**, on the ratchet (from the constructor) |
| `CInputGenerator::CInputGenerator(COsContext*, float, float)` | 0x8001DA84, 0x70 | **missing**, on the ratchet (from the constructor) |
| `AllocateRenderer` | 0x8026EF54, 0x9C | **missing**, on the ratchet (from `PostInitialize`) |

**Twelve symbols, all of them already on the ratchet**, stand between the written frame loop
and a call that links. They total **2,584 bytes** of retail code: `CIOWinManager` 844
(40+100+128+196+380), `CInputGenerator` 620 (508+112), `CStopwatch::CSWData` 272 (124+148),
`CMain::ResetGameState` 416, `UnloadAudio` 172, `AllocateRenderer` 156, `CMainFlow` 104. That
is a session of work, not 300 functions. It is also not reachable on its own, because of the
next section.

**Corrected 2026-09-26: that is thirteen functions, and 47% of them are blocked by one header.**
Lane e2 worked the list and `docs/research/frame_loop.md` has every address, size and state. The
byte breakdown above lists **thirteen** functions, not twelve - `CIOWinManager` is four,
`CInputGenerator` two, `CSWData` two - and of the 2,584 bytes, **1,212 (47%: `RemoveAllIOWins` 128,
`PumpMessages` 196, `AddIOWin` 380, `CInputGenerator::Update` 508) cannot be written at all** until
`include/rstl/rc_ptr.hpp` models retail's eight-byte `{ T* x0_ptr; u32* x4_refCount; }` instead of
this tree's four-byte `CRefData*`: retail's copy constructor is out-of-line (`fn_80049010`, 0x80049010)
and this tree's is `inline`, so no spelling of those four bodies produces retail's instructions.
Modelling it correctly is the highest-value single action available on the frame loop, and it will
move other units, so it has to be done as its own piece of work.

What did land, all `Matching` and byte-exact (480 of the 2,584 bytes, plus 64 more that were not on
this list): `CIOWinManager`'s constructor and destructor, `CStopwatch::CSWData::Initialize`,
`CInputGenerator`'s constructor, `CMainFlow`'s constructor, and `CIOWin::CIOWin` - the last because
`CMainFlow` could not link without it, and it needed three renames in `config/G2ME01/symbols.txt`
(retail's `CIOWin` constructor and both vtables are unnamed there).

## The ordered path

State is one of **written** (a body exists and does what retail's does), **stub** (a body exists
and does less, and the gap is written down at the definition), **empty** (`{}` in the source),
**missing** (declared or called, no definition in the tree), or **host-only** (a definition
exists that is the port's, not retail's). Addresses and sizes are retail's, from
`config/G2ME01/symbols.txt` and `main.elf`.

| # | step | retail | state | what it blocks |
| --- | --- | --- | --- | --- |
| 0 | `aurora_initialize` + `aurora_dvd_open` + the disc check | n/a (port) | **written** (`platform/main.cpp`) | nothing. The window exists before the game is entered |
| 1 | `main` | 0x801EFB00, 0x168 | **missing** | see correction 2. The port replaces it with `platform/main.cpp` and nulls two of its five arguments |
| 2 | `COsContext::COsContext` | 0x8028C09C, 0xE0 | **written**, behaviour-only (`src/Kyoto/Basics/COsContext.cpp`; it is a port of the Metroid Prime file, and says so) | nothing now; it owns `OSInit`, so `CGameAllocator::Initialize` works |
| 3 | the window/VI bring-up (`fn_802C2FD4` in retail) | 0x802C2FD4, 0x284 | **host-only** (`CMain::OpenWindow` -> `COsContext::OpenWindow`, `src/MetroidPrime/PortBoot.cpp`) | a frame's EFB/XFB shape. It is now *called*; before this it was written and nothing called it |
| 4 | `CMain::CMain` | 0x80008898, 0x114 | **written** (`src/MetroidPrime/main.cpp:139`) | nothing; it sets `gpMain`, which steps 17 and 21 need |
| 5 | `InvokeCMain` | 0x80008818, 0x80 | **written** (`main.cpp:168`) | nothing; it is the seam the port enters through |
| 6 | `CMain::RsMain` | 0x80005C6C, 0x864 | **host-only** (`PortBoot.cpp`: `OpenWindow()` then return) | **everything below.** Retail's 2,148 bytes is unwritten and cannot be written |
| 7 | `new CGameGlobalObjects` (via `fn_80008AD4`) | 0x8000848C, 0xE4 | **stub** (`main.cpp:198` - initialises `simplePool` from an uninitialised `resFactory`) | `PostInitialize`, and so the renderer |
| 8 | `fn_80003A18(this)` | 0x80003A18, 0x30 | **missing** | unidentified; one of the two unnamed `CMain` methods `RsMain` calls |
| 9 | `CStringTable::SetLanguage` | 0x80312AFC, 0x18 | **written** | nothing |
| 10 | `fn_800069AC` | 0x800069AC, 0x134 | **missing** | not an IOWin registration, which is what it looks like from the call shape. It is a bounded, insertion-sorted float push - `r3` points at `{int n; float v[4];}` and it appends `*(float*)r4` and re-sorts - and `RsMain` calls it six times: four at 0x80005D0C-0x80005D24 to seed two of them with two `.sdata2` constants, and two more per frame at 0x80006108 and 0x80006228. The two histories are at `CMain`+0x18 and `CMain`+0x2C (20 bytes each, so 0x18..0x2C and 0x2C..0x40), inside the `char x10_pad[0x38]` that `include/MetroidPrime/CMain.hpp` does not model. Two floats at +0x40 and +0x44 take the running minimum, and `CMain::DrawDebugMetrics` reads them - this is the frame-time history the debug metrics display is built on, and it is why the port's metrics will be empty |
| 11 | `CMain::InitializeSubsystems` | 0x80008680, 0x15C | **stub** (`main.cpp:191` - `ARInit` and a TODO) | deliberately not written, 2026-09-25, and here is why. Retail's body is `ARInit((u32*)0x803C5AB8, 3)` -> `ARAlloc` -> a global bump -> `ARQInit` -> `OSGetCurrentThread` -> an icache invalidate -> `DCFlushRange` -> two `printf`s of a build string -> `fn_802DAE30`, `fn_8002ADC8`, `fn_80301CC4(2048, 0x600000, 4096)`, `fn_800E85A8`, `fn_800DC0B0` -> `CFrameDelayedKiller::Initialize`. Of those, **only `CFrameDelayedKiller::Initialize` is written**; the other five are unwritten, and calling them from the host build would add five missing symbols and remove none - the trap `src/MetroidPrime/CMiscTableInit.cpp` documents. Two further parts are actively *harmful* on a host: the icache block `memset`s 0x7338D0D0 over the 8 KB below `OSGetCurrentThread()`'s stack pointer, and **`ARInit` is a live hazard** - Aurora's `ARInit` (`extern/aurora/lib/dolphin/AR.cpp:98`) only stores the pointer it is given, and `ARAlloc` then dereferences it (`AR_StackPointer -= *AR_BlockLength;`, line 71), so passing retail's guest address 0x803C5AB8 faults on the very next call. Aurora does define `ARInit`/`ARAlloc`/`ARQInit`, so nothing about this is a link problem; it is a "this line has to change on PC" problem, and it is one line |
| 12 | `CGameGlobalObjects::PostInitialize` | 0x800083E0, 0xAC | **written but blocked** (`main.cpp:201`) | calls `AllocateRenderer` (missing), `CGameGlobalObjects::AddPaksAndFactories` (empty) and `CEnvFxManager::Initialize` (missing) |
| 13 | `CGameGlobalObjects::AddPaksAndFactories` | 0x80007168, **0x790 = 1,936 bytes** | **empty** (`main.cpp:390`) | the entire resource system: paks, factories, the object pool. Nothing after this can run without it |
| 14 | `CMain::AddWorldPaks` | 0x80005968, 0x180 | **written** (`main.cpp:467`) | needs `CResLoader::AddPakFileAsync` (missing) and `CToken`-level DVD reads |
| 15 | `fn_8016BDE4` (the boot-tweak string test) | 0x8016BDE4, 0x8 | **missing** | decides the `-seed`/tweak string passed on |
| 16 | `CMain::FillInAssetIDs` | 0x80006B38, 0x48 | **written** (`main.cpp:409`) | needs `gpSimplePool` and `gpResourceFactory`, i.e. step 13 |
| 17 | `new CGameArchitectureSupport(osContext)` (via `fn_80008A48`) | 0x80007EC4, 0x378 | **written** (`main.cpp:223`) - and **it faults on the host** | the frame loop. See the wall |
| 18 | `CMainFlow`, `CConsoleOutputWindow`, `CAudioStateWin`, `CErrorOutputWindow` constructors | 0x8001E008 etc. | **missing** (`CMainFlow` on the ratchet) | four IOWins; `CIOWinManager` itself is missing too |
| 19 | `CGameOptions(CBitStreamReader&)`, `CGameOptions::EnsureOptions` | 0x80161828, 0x320; 0x801612C4, 0x10C | **written** (`src/MetroidPrime/Player/CGameOptions.cpp`) | needs `CBitStreamReader::CBitStreamReader(CInputStream&)` (0x80342F58, 0x14), `CBitStreamReader::ReadBits` (0x80342DD4, 0x148) and `CMemoryInStream::CMemoryInStream(const void*, unsigned long)` (0x802FFF04, 0x3C) |
| 20 | `CDvdFile::FileExists` | 0x8030C04C | **written** (`src/Kyoto/DolphinCDvdFile.cpp`) | nothing |
| 21 | the frame loop | 0x80006034-0x80006354 | **written, unreachable** | see below |
| 21a | `CMain::MemoryCardInitializePump` | 0x80007958, 0xBC | **empty** (`main.cpp:388`) | the memory card; the port has no card, so a no-op is legitimate *if* it says so - and this one does not |
| 21b | `CGameArchitectureSupport::UpdateTicks` | 0x80007BC0, 0x228 | **written**, near-matched | the thirteen functions in correction 3. Six are now `Matching`; see `docs/research/frame_loop.md` |
| 21c | the draw: `lwz r12,148(r12); mtctr; bctrl` through `gpRender`'s vtable, and `fn_80049244` | 0x800061BC; 0x80049244, 0x118 | **missing** | pixels. `gpRender` is `nullptr` until step 12 succeeds, and the vtable is the one `AllocateRenderer` returns (`IRenderer` / `CCubeRenderer`) - the slot is not named in either tree. **Re-measured 2026-09-26 by lane e2 and still not identified**: nothing in `config/G2ME01/symbols.txt` or `include/MetaRender/` names vtable slot +0x94, and `fn_80049244` is still unwritten. Not attempted, and worth nothing until step 17's two null dereferences are fixed |
| 21d | `CMain::DrawDebugMetrics` | 0x800070FC, 0x6C | **written** (`main.cpp:392`) | nothing |
| 21e | `CMain::AsyncIdle` | 0x80005B44, 0x120 | **written** (`main.cpp:434`) | needs `CResFactory::AsyncIdle` (missing) |
| 21f | `CGameArchitectureSupport::Update` | 0x80007A14, 0x70 | **written, byte-exact** | needs `CGameState::GetWorldState()` - and retail's `CGameState` constructor, which is unwritten, is what fills `+0x3C`. The comment on `Update` at `main.cpp:333` already says this |
| 21g | `fn_80003858` | 0x80003858, 0x24 | **missing** | unidentified |
| 21h | `CMain::CheckTerminate` | 0x800070F4, 0x8 | **written** (returns false) | nothing - but it means the loop never ends on its own |
| 21i | `CMain::CheckReset` | 0x80006BA4, **0x49C = 1,180 bytes** | **stub with a defect** (`main.cpp:407`: `bool CMain::CheckReset() {}` - a non-void function with no `return`) | **UB.** It is on the loop's exit path, so the host loop's behaviour is undefined until this is written. `-Wno-error=return-type` in `CMakeLists.txt:90` is what lets it compile |
| 22 | `~CGameArchitectureSupport` | 0x80007DE8, 0xDC | **written** (`main.cpp:253`) | `UnloadAudio` (missing) |
| 23 | `CMain::ShutdownSubsystems` | 0x80008570, 0x110 | **empty** (`main.cpp:196`) | clean shutdown only |
| 24 | `~CGameGlobalObjects` (via `single_ptr_assign_800064D0` / `__dt__80006AE0`) | 0x800064D0, 0x48; 0x80006AE0, 0x58 | **missing** | clean shutdown only |

## `CMain` offsets, as this path reads them

Measured from `CMain::RsMain`'s own accesses, so a lane writing retail's `RsMain` does not have
to re-derive them. Only what an instruction proves is listed.

| offset | what | proof |
| --- | --- | --- |
| +0x00 / +0x04 / +0x08 / +0x0C | `osContext`, `x4_unk1`, `memorySys`, `xc_unk2` | `lwz r4,0(r31)`, `lwz r5,8(r31)` at 0x80005CDC/0x80005CE0 |
| +0x18, +0x2C | two `{int n; float v[4];}` frame-time histories, 20 bytes each | `fn_800069AC`'s shape; called with `addi r3,r31,24` and `addi r3,r31,44` |
| +0x40, +0x44 | two floats, running minimums of the histories | `stfs`/`lfs` at 0x80005D34/0x80005D38 and 0x80006120/0x8000623C |
| +0x54 | `gameGlobalObjects` | `stw r0,84(r31)` at 0x80005CF4 |
| +0x58 | `restartMode`, set to 6 = `kRM_StateSetter` on the reset path | `li r0,6; stw r0,88(r31)` at 0x800063E0 |
| +0x5C | `x5c` | `lfs f1,92(r31)` at 0x80006184 |
| +0x90 | one byte of bits: `finished` is bit 0, `x90_31_cardBusy` is bit 7, and the frame loop's back-edge at 0x8000645C (`rlwinm. r0,r0,25,31,31`) tests **bit 7** | `clrlwi r0,r0,31` at 0x80006314 tests bit 0; `rlwimi r0,r3,0,31,31` at 0x80006338 clears bit 0 |

`+0x18`..`+0x48` is the `char x10_pad[0x38]` that `include/MetroidPrime/CMain.hpp` declares and
does not model. Nothing outside the boot path reads it today, so it is padding for now; it is
not padding for retail, and step 10 lives in it.

## The wall, in one paragraph

Step 17 is the wall, and it is a wall of **null pointer dereferences, not of missing code**.
`CGameArchitectureSupport::CGameArchitectureSupport` (0x80007EC4) does
`lwz r29,-28220(r13)` at 0x80007F38 - that is `gpTweakPlayerA`, which
`src/MetroidPrime/PortGlobals.cpp:129` defines as `nullptr` and which retail only ever fills
from the Tweaks REL module (`REL_CreateTweakGlobals`) - and immediately calls
`GetRightAnalogMax`/`GetLeftAnalogMax` on it at 0x80007F40 and 0x80007F4C **with no null
test**. Further down it reaches `gpGameState->GameOptions().EnsureOptions()` -
`lwz r3,-28360(r13)` at 0x800081A4 (`gpGameState`, `.sbss:0x80418EB8`), `addi r3,r3,128`,
then the call at 0x800081AC. So
`UpdateTicks` and `Update`, both written and one of them byte-exact, are **unreachable**, and
so is the whole frame loop behind them. Making them reachable needs, in order: the tweak
singletons (a Tweaks-module bring-up), a `CGameState`, then the twelve symbols in correction 3
- of which `CIOWinManager` and `CInputGenerator` are the real work.

### Corrected 2026-09-26: `gpGameState` does **not** need `StreamNewGameState` or the paks

The paragraph above used to end "and `gpGameState` is null until `CMain::StreamNewGameState`
runs, which needs the paks from step 13". **That is wrong**, and it sends a lane to write the
wrong 1,936 bytes first. `gpGameState` is `.sbss:0x80418EB8` and its one writer in the whole
DOL is **`CGameGlobalObjects::CGameGlobalObjects` at 0x80008548**:

```
80008548:  80 1f 01 30   lwz   r4,304(r31)     ; CGameGlobalObjects+0x130, the single_ptr
8000854c:  90 0d 91 38   stw   r4,-28360(r13)  ; 0x80418EB8 gpGameState
```

and that member is filled eleven instructions earlier, inside the same constructor:

```
800084c4:  38 60 02 f0   li    r3,752          ; 752 = 0x2F0 = sizeof(CGameState)
800084d0:  48 2c 5d a9   bl    802ce278 <__nw__FUlPCcPCc>
800084dc:  48 13 c4 ed   bl    801449c8        ; CGameState::CGameState()
80008534:  90 1d 01 30   stw   r0,304(r31)     ; ... no: the store is 0x80008548, above
```

`CMain::RsMain` calls that constructor at **0x80005CE4**, which is **step 7** — before
`PostInitialize` (step 12) and long before `AddPaksAndFactories` (step 13). So `gpGameState`
needs step 7 and **not** step 13, and `CMain::StreamNewGameState` is not on its path at all.

What is missing is `CGameState::CGameState()` — `fn_801449C8`, past 0x80144B3C, with eight
nested constructors in it (`fn_8015C34C` for a 1200-byte `CWorldState`,
`__ct__12CGameOptionsFv`, `fn_80180738`, `fn_80146154`, two `fn_80144924` + `fn_80004A4C`
pairs, `fn_80193E08`, and more past 0x80144B40) — and **that function has no body anywhere
in this tree**. It cannot be stood in for either: the object is 0x2F0 bytes of nested state,
and `EnsureOptions` would then run against whatever a stand-in left in it.

`docs/research/boot_globals.md` has the whole measurement, including the host stand-in that
removes the *first* of the two dereferences.

### Corrected 2026-09-25: "a Tweaks-module bring-up" is four items, and it is not this one

`docs/research/tweak_globals.md` maps all 1,452 bytes of retail's `REL_CreateTweakGlobals`
(`Tweaks.rel .text:0x508`, 0x5AC) store by store. It changes what this paragraph should say.
`gpTweakPlayerA` **does** end up non-null there - the store is at Tweaks `.text:0x78C` - so
filling the singleton is possible. But it is not enough, and it is not the first job:

1. **What retail puts there is not a `CTweakPlayer`.** The object is four bytes from
   `operator new` whose only word is `&gpTweakContents->TweakPlayer` (retail offset **+0x10E8**).
   `include/MetroidPrime/Tweaks/CTweakPlayer.hpp` has no data members, so
   `GetLeftAnalogMax`/`GetRightAnalogMax` - the two calls this wall is about - have nothing to
   read, and neither is defined anywhere in the tree. Both are already on the ratchet
   (`port_link_gap_list.md:114-115`), and `src/MetroidPrime/main.cpp:225-226` calls them.
2. ~~**`gpTweakContents` is 1,500 bytes too big in this tree.**~~ **Superseded 2026-09-26; see
   `docs/research/tweak_player.md`.** That figure came from a 64-bit host-compiler probe. Measured
   with **mwcceppc (32-bit)**, `sizeof(CTweakContents)` is **0x3244** against retail's 0x31F4,
   **all sixteen members are at retail's offsets** — `TweakPlayer` included, at **+0x10E8** — and
   the only wrong size is `SLdrTweakPlayerRes` (0x548 vs 0x4F8), which moves `TweakSlideShow` and
   `TweakTargeting` by +0x50. So this item is **one struct**, not the `SLdrTweak*` family, and
   `gpTweakPlayerA` is already where retail puts it.
3. **Nothing calls it.** `REL_CreateTweakGlobals` and `REL_LoadTweaks` are reachable only
   through `STweaks_FuncPtrs::CreateGlobals`/`:Loader`, which `TweaksInit` assigns and nothing
   invokes; `mp_relmain_tweaks` only calls `TweaksInit`. `REL_CreateTweakGlobals` also
   dereferences `gpTweakContents` with no null test, as retail does, so the two must be ordered.
4. **Then `gpGameState`**, which this function does not touch - `nm` on the Tweaks object shows
   no reference to it. (It is also not a paks problem; see the correction above.)

**Items 1 and the stand-in are done (2026-09-26, lane `h2`).** `CTweakPlayer` is the 4-byte cell
and all five accessors have bodies, and `src/MetroidPrime/PortTweakGlobals.cpp` now gives
`gpTweakPlayerA`/`gpTweakPlayerB` real cells over a zeroed `SLdrTweakPlayer`, called from
`platform/main.cpp` right after `port::modules::InitAll()`. **Item 2 is one struct**
(`SLdrTweakPlayerRes`), and **item 3 - "give the Tweaks module a caller" - is still open**:
the stand-in is a stand-in, and it is named as one.

Item 1 is also done: **`CTweakPlayer` is modelled as the 4-byte cell** (`SLdrTweakPlayer* mTweak`)
and all five accessors have bodies, each compiling to retail's 12 bytes.

So the order on step 17 is now: **fix `SLdrTweakPlayerRes`, give the Tweaks module a caller** —
and only then write `REL_CreateTweakGlobals`. The lane that mapped it reached 68.29% and claimed no
range; see `docs/RUNNING_THE_DECOMP.md`'s "Attempted modules" table.

## What moved when this was written

- `CMain::OpenWindow` and `CMain::RsMain` now have **host-only** bodies in
  `src/MetroidPrime/PortBoot.cpp`, a new translation unit that `configure.py` never claims -
  the same arrangement as `PortGlobals.cpp`, and for the same reason. `main.cpp` keeps only an
  `#ifndef TARGET_PC` guard around its empty `RsMain`, so **mwcceppc compiles exactly what it
  compiled before**; the gate's per-function diff is `matched 3043 -> 3043, linked
  1653 -> 1653`, i.e. zero movement, which is the point of putting it there.
- One file added to `files.cmake` (the port build's manifest, not the matching build's
  config), so the probe is 124 files.
- `link_gap.py` is **724 before and 724 after**. Nothing was closed: `CMain::OpenWindow` was
  not on the ratchet (nothing referenced it), and `CMain::RsMain` was already defined by
  main.cpp's empty body.

Later on the same day, a separate lane re-measured both of those and they had moved without
anyone touching the boot path: `link_gap.py --rebuild` reports **721 over 126 objects**, not 724
over 124, at commit `9343ca6`. `check_docs_claims.py` derives the numbers the *other* docs
quote from `report.json` and `link_gap.py`, so it does not catch this figure; the count in
this paragraph is a snapshot and is the one to re-measure rather than quote.
- **The port still does not link and therefore does not boot.** No frame has been rendered and
  none can be until the 724 symbols and steps 8/10/13/17/21c above are done.

## Cheapest order from here, by bytes

Not by importance - by how much of the path each unblocks per line of decompilation.

1. `CIOWinManager` (5 methods, 844 bytes total), `CInputGenerator::Update` (508) and
   `CInputGenerator::CInputGenerator` (112). These unblock the *body* of the frame loop and are
   already on the ratchet, so writing them moves the measured number.
2. `CMain::CheckReset`'s missing `return` (one line; a UB fix, not decompilation). Retail's is
   1,180 bytes and it is on the loop's exit path.
3. `CMain::InitializeSubsystems`'s ARAM line (see step 11) - one line, and it is the first thing
   on this list that would be *observable* once the port links.
4. The Tweaks singleton bring-up, so step 17 does not fault. This is a *port* task, not a
   decompilation one, and it is the only item on this list that is not decompilation.
5. `CMain::AddWorldPaks`'s `CResLoader::AddPakFileAsync` (0x802FC268, 0xE8) and
   `CResFactory::AsyncIdle`, then step 13's 1,936 bytes - the real project, and the one the
   234 REL module loaders in `port_link_gap.md` sit behind.

## Which of the 342 remaining undefined symbols is on this path (lane `h4`, 2026-09-26)

The 342 are the symbols `docs/research/port_link_stubs.md` proved must be real, and the question a
lane naturally asks about its own block is *when* each one is reached. The static analysis that
produced the 342 is whole-object and branch-blind, so "reachable" is an upper bound; the partition
below is measured by hand from the referring object and the call site, and it **overturns the
expectation** that the frame loop's own machinery is close.

**Method.** `tools/link_undef_refs.py` over the port's real link log pairs every undefined symbol
with the objects that reference it, and every one of the 40 symbols in this block was found there
exactly once each - no symbol in the block is referenced by an object the analysis could not
reach. The referring object then says what kind of code wants it, and `docs/research/boot_path.md`
above says whether that code runs before the first frame. Cross-referencing the two is the whole
method; there is no new instrument.

**Result: exactly 1 of 40 is reached before the first frame.**

| tier | count | which, and why |
| --- | --- | --- |
| **before the first frame** | **1** | `CGameState::CGameState(CInputStream&, int)` - retail `fn_80144140`, 0x80144140, **0x684 = 1,668 bytes**. One referrer, `main.cpp.o`, at `src/MetroidPrime/main.cpp:704` inside `CMain::StreamNewGameState`, which is the **only** writer of `gpGameState` and of `CGameState`+0x3C - and step 17 dereferences `gpGameState` at 0x800081A4 **with no null test**. It is genuinely on the path. It is also 1,668 bytes with **seventeen unwritten callees** listed in `main.cpp`'s own block map, so it is not a lane-sized job and this block did not attempt it. |
| **first world load** - reached on the first frame of a *loaded* world, i.e. after step 13's 1,936 bytes of paks and factories exist | 19 | all 15 `CModelData` symbols and 4 `CStateManager` ones: `AddObject(CEntity&)`, `fn_800366e4`, `fn_801EDD8C`, `UpdateActorInSortedLists`. Every referrer is `CActor.cpp.o`, and every call site is `CActor`'s constructor, `AdvanceAnimation`, `PreRender`, `Draw`, `GetLocatorTransform`, `IsOpaque` or `InitEffects`. **A `CActor` cannot exist before a world is loaded**, and step 21c's first frame draws through `gpRender`'s vtable with no actors in it. |
| **gameplay only** | 20 | the 5 `CAnimData` methods (`CActor`'s update and pre-render, `CPlayerGun`'s weapon fire), and 15 `CStateManager`/`CGameState` methods reached only from script-message delivery (`fn_8003BE54`, `SendScriptMsg_fn_80037100`), damage application (`ApplyLocalDamage`), the pause/transition test (`fn_80036F10`), `CPlayerState`, and `CPlayerGun`. |

**So the premise this block was given is wrong, and it is worth saying plainly:** `CModelData` is
retail's model/animation container and it is large, but **it is not on the frame path**, and
neither is anything else in this block except one 1,668-byte `CGameState` constructor. A model
that cannot be constructed does mean no *visible* frame - but the first frame in the table above
is the frame loop's first iteration, which renders nothing, and it happens long before any
`CModelData` exists. Prioritising this block as "high priority because it is the core of the
frame" would have been the wrong call; the measurement is what says so.

**What this reorders.** The cheapest-order list above is unchanged and this block does not
compete with it, but it does add one item that is *not* on it and is smaller than everything on
it: `CGameState`'s accessors. `GetGameMode` (0x80142464, 0x8), `GetHardModeDamageMultiplier`
(0x80142498, 0x24), `SetIsDarkWorld` (0x801424BC, 0x10), `SetUnk50` (0x801424EC, 0x8) and
`GetHardModeEnabled` (0x801424CC or 0x801424DC, 0x10) are **8 to 36 bytes each, call-free except
the multiplier**, and they are in the same 0x80142464..0x801424F4 run. Four of the five need
nothing but a named member: retail's are one `lwz r3,412(r3)` / `blr`, one `lbz`/`rlwimi`/`stb` on
the flag byte at +0x2EC, and one `stfs f1,80(r3)` / `blr` - and this tree's `CGameState` already
names that last member `x50_unk` at 0x50. They were not written here because `GetGameMode` needs a
member at **+0x19C** and `SetIsDarkWorld` a bit-field at **+0x2EC**, both inside
`CGameState`'s `char pad2[0x1E0]`, so they are a `CGameState.hpp` layout job rather than a lane's
afternoon, and `check_raw_offsets.py` fails the gate on a new raw-offset access.
