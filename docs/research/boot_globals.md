# The two globals `CGameArchitectureSupport`'s constructor dereferences, and who has to set them

Written 2026-09-26 in lane `h2`. Every address and size below is read out of
`build/G2ME01/main.elf` and `config/G2ME01/symbols.txt`; nothing is recalled. The one
correction in here supersedes a claim that was in `docs/research/boot_path.md`,
`src/MetroidPrime/PortBoot.cpp` and `docs/research/tweak_globals.md` and was wrong.

## The short answer

| | `gpTweakPlayerA` | `gpGameState` |
| --- | --- | --- |
| address | `.sbss:0x80418F44` | `.sbss:0x80418EB8` |
| dereferenced at | **0x80007F38** | **0x800081A4** |
| what the ctor does with it | `mr r3,r29; bl GetRightAnalogMax` (0x80007F40), then `GetLeftAnalogMax` (0x80007F4C) - the two floats become `CInputGenerator`'s ctor arguments two instructions later | `addi r3,r3,128; bl CGameOptions::EnsureOptions` (0x800081AC) |
| null test | **none** | **none** |
| only writer in the DOL | Tweaks.rel `REL_CreateTweakGlobals`, module `.text:0x78C` | `CGameGlobalObjects::CGameGlobalObjects` at **0x80008548** |
| boot-path step that writes it | none - nothing calls `CreateGlobals` | **step 7** (`CMain::RsMain` 0x80005CE4) |
| does it need the paks? | yes, for the *data* (`Standard.NTWK`) | **no. This was the wrong claim.** |
| state on the port | **fixed 2026-09-26** - `port::tweaks::CreateStandInTweakPlayers()` | **not fixable today** |

`CGameArchitectureSupport::CGameArchitectureSupport` is DOL `.text:0x80007EC4` and is
**0x3F8 = 1016 bytes**, ending at 0x800082BC. It makes exactly two unguarded global
dereferences, at 0x80007F38 and 0x800081A4. (`include/MetroidPrime/CGameArchitectureSupport.hpp:52`
says `CHECK_SIZEOF(CGameArchitectureSupport, 0xd0)`; retail's `operator new` in `RsMain` at
0x80005E08 is `li r3,168`, so the object is **0xA8** and the header's figure is wrong by 0x28.
The last store in the constructor is `stb r0,160(r31)` = +0xA0, which agrees with 0xA8. Not
changed here: it is a header-wide move like `CGameGlobalObjects`'s `pad0[4]`, and no unit in
this lane depends on it.)

## The correction: `gpGameState` does not need `StreamNewGameState` or the paks

`docs/research/boot_path.md` said, and so did `PortBoot.cpp` and `tweak_globals.md`:

> `gpGameState` is null too, until `CMain::StreamNewGameState` runs, which needs the paks
> from step 13.

**That is wrong, and it is wrong in the expensive direction** - it puts step 13
(`CGameGlobalObjects::AddPaksAndFactories`, 1,936 bytes of retail with no body) in front of a
problem that step 13 does not solve. The disassembly:

```
800084c4:  38 60 02 f0   li    r3,752                 ; 752 = 0x2F0 = sizeof(CGameState)
800084c8:  38 84 56 c0   addi   r4,r4,22208
800084cc:  38 a0 00 00   li    r5,0
800084d0:  48 2c 5d a9   bl    802ce278 <__nw__FUlPCcPCc>
800084d4:  7c 60 1b 79   mr.    r0,r3
800084d8:  41 82 00 0c   beq   800084e4
800084dc:  48 13 c4 ed   bl    801449c8              ; CGameState::CGameState()
...
80008548:  80 1f 01 30   lwz   r4,304(r31)           ; CGameGlobalObjects+0x130
8000854c:  90 0d 91 38   stw   r4,-28360(r13)        ; 0x80418EB8 = gpGameState
```

`_SDA_BASE_` is 0x8041FD80, so `-28360` is `0x8041FD80 - 0x6EC8` = 0x80418EB8. All of that is
inside `CGameGlobalObjects`'s **constructor**, and `CMain::RsMain` calls it at **0x80005CE4**,
which is boot-path step 7. `PostInitialize` is step 12 (0x80005D4C) and `AddWorldPaks` step 14
(0x80005D54). So the ordering is 7 < 12 < 13: `gpGameState` is set before anything needs a pak.

What is actually missing is **`CGameState::CGameState()`**, `fn_801449C8`, which runs past
0x80144B3C and contains eight nested constructors:

| callee | at | what it builds |
| --- | --- | --- |
| `__nw__FUlPCcPCc(1200)` + `fn_8015C34C` | 0x80144A00, 0x80144A24 | a 1200-byte `CWorldState`, stored at `this+0x3C`, plus a 4-byte refcount word set to 1 at `this+0x40` - which is exactly the `rstl::rc_ptr<CWorldState>` pair `CGameState::GetWorldState` hands out |
| `fn_80145950` | 0x80144A6C | `this+0x54` |
| `__ct__12CGameOptionsFv` | 0x80144A74 | `this+0x80`, the member `EnsureOptions` is called on |
| `fn_80180738` | 0x80144A7C | `this+0xC4` |
| `fn_80146154(1)` | 0x80144A88 | `this+0xDC` |
| `fn_80144924` + `fn_80004A4C` | 0x80144ABC, 0x80144AE8 | two 3-element containers at `this+0x110` and `this+0x144` |
| `__nw__FUlPCcPCc(12)` + `fn_80193E08` | 0x80144B08, 0x80144B30 | `this+0x1A0` |

**None of these has a body in this tree**, and `CGameState()` is declared in
`include/MetroidPrime/Player/CGameState.hpp:14` with no definition anywhere. That is the whole
of the second wall. It is not a port-side fix: the object is 0x2F0 bytes of nested state, and
`CGameOptions::EnsureOptions` would then run against whatever a stand-in left in it.

## `gpTweakPlayerA`, and the host stand-in that removes it

`0x80418F44` has two touches in the DOL and one in the Tweaks module:

* `fn_800324A4` (0x800324A4..0x80032670, first word of `.ctors` entry 4) stores **0** into it
  at 0x80032588 and registers a destructor with `__register_global_object` (0x80344E20). That is
  retail *announcing* the slot, not filling it.
* Tweaks.rel `REL_CreateTweakGlobals` (module `.text:0x508`, 0x5AC = 1,452 bytes) stores into it
  at module `.text:0x78C`, and that is the only store anywhere. The store-by-store map is
  `docs/research/tweak_globals.md`.

What it stores is **not a `CTweakPlayer` with members**: it is a 4-byte object whose only word is
`&gpTweakContents->TweakPlayer` (retail offset **+0x10E8**), and each accessor is three
instructions that dereference word 0 and read a float at a retail offset in `SLdrTweakPlayer`:

```
802184cc <GetRightAnalogMax__12CTweakPlayerFv>:  lwz r3,0(r3); lfs f1,428(r3); blr   ; +0x1AC
802184d8 <GetLeftAnalogMax__12CTweakPlayerFv>:   lwz r3,0(r3); lfs f1,424(r3); blr   ; +0x1A8
```

`include/MetroidPrime/Tweaks/CTweakPlayer.hpp` now models exactly that, and all five accessors
have bodies that compile to retail's 12 bytes each. **So the accessors are not the blocker and
have not been for a while.**

The blocker is that the pointer is never set on the host:

* `src/MetroidPrime/Tweaks/Tweaks.cpp` has `REL_CreateTweakGlobals`'s body at 68.29%, but under
  `#ifdef TARGET_PC` it is an **empty body** - the real one uses mwcceppc's three-argument
  `new T(file, line)` and names thirteen DOL `.sbss` slots that do not exist on a host.
* Nothing calls it in any case. It is reachable only through
  `STweaks_FuncPtrs::CreateGlobals` (`include/MetroidPrime/ScriptLoaderRel.hpp`), which
  `TweaksInit` assigns and **nothing invokes**; `port::modules::InitAll()` calls
  `mp_relmain_tweaks`, which is `TweaksInit` and nothing more.
* Even with a caller, the *pointer* is not reproducible: it is
  `&gpTweakContents->TweakPlayer`, and `gpTweakContents` is allocated only by
  `REL_LoadTweaks` (.text 0xCD4, 0x218 bytes) from `Standard.NTWK` read out of a pak. That is
  step 13.

### What this lane did about it

`src/MetroidPrime/PortTweakGlobals.cpp` (a port-side TU, **absent from `configure.py`**, so it
cannot touch `main.dol` or any of the 86 REL modules) defines
`port::tweaks::CreateStandInTweakPlayers()`, which gives both player slots real 4-byte cells
over a **zeroed** `SLdrTweakPlayer`. `platform/main.cpp` calls it immediately after
`port::modules::InitAll()`. It has no failure return: the allocation is `rs_new`, which
throws `std::bad_alloc` exactly as retail's `operator new` does. All five accessors then answer `0.0f`, which is a legal answer for
both `CInputGenerator` arguments and is what a tweaks file that set nothing would give.

It is a stand-in and is named as one. It does **not** replace `REL_CreateTweakGlobals`: the
honest fix for the real values is a caller for it plus `REL_LoadTweaks`, i.e. item 3 of
`boot_path.md`'s four-item list, which is still open.

## Why the port now stops with a reason instead of faulting

`src/MetroidPrime/PortBoot.cpp`'s `CMain::RsMain` (host-only) checks both globals by name, in
the order the constructor touches them, and returns 1 with a message naming the address, the
writer and what is missing. The third message - both globals set, still no frame - names the
eight unwritten callees of the constructor: `CAudioSys`'s constructor, `CInputGenerator`'s,
`CIOWinManager`'s, `CMainFlow`'s, `CConsoleOutputWindow`'s, `CErrorOutputWindow`'s,
`CGameOptions::EnsureOptions` and `CMain::ResetGameState`. Only the two `CTweakPlayer` accessors
of that set are written. **A boot that null-derefs on frame 0 tells nobody anything**; this one
stops and says which two globals and which functions it is waiting for.

## The two objects the objective names, measured, and why neither is in the block

`COsContext` and `CMemorySys` are named as link targets, and **`/tmp/opencode/boot_osctx.txt`
does not contain one symbol of either class.** All 8 entries in it are one
`CGameArchitectureSupport` method and seven `CGraphics` methods. The premise is also already
corrected in `docs/research/port_link_gap.md:43-52`, which records that `CMemorySys`'s three
methods have been in `src/Kyoto/Alloc/CMemory.cpp` since the port's first build. Measured
here, with `nm` on the two port objects:

```
build-port/.../src/Kyoto/Basics/COsContext.cpp.o
  T COsContext::COsContext(bool, bool)          T COsContext::~COsContext()
  T COsContext::OpenWindow(char const*, int, int, int, int, bool)
  T COsContext::Update()                         T COsContext::GetOsKeyState(int) const
  T COsContext::AllocFromArena(unsigned long)    B COsContext::mProgressiveMode
build-port/.../src/Kyoto/Alloc/CMemory.cpp.o
  T CMemorySys::CMemorySys(COsContext&, IAllocator&)   T CMemorySys::~CMemorySys()
  T CMemorySys::GetGameAllocator()
  T CMemory::Startup(COsContext&)   T CMemory::SetAllocator(COsContext&, IAllocator&)
  T CMemory::Alloc(...)  T CMemory::Free(void const*)  T CMemory::Shutdown()  ...
```

Their sizes, measured rather than taken from the headers:

* **`COsContext` is 0x6C bytes.** `main` (0x801EFB00) does `stwu r1,-176(r1)` and
  `addi r3,r1,44` before `bl fn_8028C09C` (the constructor, 0xE0 bytes), so the object runs
  r1+44..r1+176 - exactly to the top of the frame. `include/Kyoto/Basics/COsContext.hpp:75`'s
  `CHECK_SIZEOF(COsContext, 0x6c)` agrees.
* **`CMemorySys` is one word, and the header's `uchar x0_unk` should be a signed short.**
  `__ct__10CMemorySysFR10COsContextR10IAllocator` (0x802CE698, 0x54 bytes) stores **nothing**
  to `this`: it only calls `CMemory::Startup` (0x802CE604) and
  `CMemory::SetAllocator` (0x802CE58C). `__dt__10CMemorySysFv` (0x802CE648, 0x4C bytes) calls
  `CMemory::Shutdown` and then, guarded on `extsh. r0,r31; ble`, `CMemory::Free` (0x802CE388) -
  a *sign*-extended halfword test on the flag argument, so the member is a `short`. Not
  changed here: it is a one-word header edit that nothing in this lane reads, and the whole
  `CMemorySys` class is already linked, so the width has no consumer.

**`fn_800084A0` says nothing about either object.** The brief points at retail's
`CGameGlobalObjects::CGameGlobalObjects` as the ground truth for where `COsContext` and
`CMemorySys` sit and how big they are. It is the ground truth for the *member* layout -
`CResFactory` at +0, `CResLoader` at +4 (`fn_802FB154`), next member at +0xE4
(`fn_80301008(this+0xE4, this+4)`, so `CResLoader` is **0xE0** bytes), +0x108
(`fn_80032008`), `single_ptr`s at +0x130/+0x134, +0x150 - and it is worth reading for
`docs/research/paks.md`'s `pad0[4]` adjudication. But its 0x14-byte prologue is
`stwu r1,-16(r1); mflr r0; stw r0,20(r1); stw r31,12(r1); mr r31,r3` and then four member
constructions: **it never reads `r4` or `r5`**, so retail's `COsContext&` and `CMemorySys&`
parameters are named and ignored. Their sizes come from `main`'s frame, as above.

## Reproducing the measurement

```sh
export MP_TOOLCHAIN_DIR=/run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrimePort
python3 configure.py --version G2ME01 --compilers $MP_TOOLCHAIN_DIR/build/compilers \
  --dtk $MP_TOOLCHAIN_DIR/build/tools/dtk --wrapper $MP_TOOLCHAIN_DIR/build/tools/wibo --build-dir build
$MP_TOOLCHAIN_DIR/build/review-tools/bin/ninja

# the two dereferences and everything the constructor calls
build/binutils/powerpc-eabi-objdump -d --start-address=0x80007ec4 --stop-address=0x800082c0 \
  build/G2ME01/main.elf
# gpTweakPlayerA is the load at 0x80007f38; gpGameState is the load at 0x800081a4
# CGameGlobalObjects' constructor, and the single store into gpGameState
build/binutils/powerpc-eabi-objdump -d --start-address=0x8000848c --stop-address=0x80008570 \
  build/G2ME01/main.elf
# retail's own ordering: step 7 at 0x80005ce4, step 12 at 0x80005d4c, step 14 at 0x80005d54
build/binutils/powerpc-eabi-objdump -d --start-address=0x80005cd0 --stop-address=0x80005e34 \
  build/G2ME01/main.elf
# CGameState::CGameState(), the eight nested ctors
build/binutils/powerpc-eabi-objdump -d --start-address=0x801449c8 --stop-address=0x80144b60 \
  build/G2ME01/main.elf
# the sizes, from the map
grep -nE 'gpTweakPlayerA|gpGameState|__ct__10CMemorySys|__dt__10CMemorySys' \
  config/G2ME01/symbols.txt
```

`_SDA_BASE_` is 0x8041FD80 and `_SDA2_BASE_` is 0x804223C0. **The exact rule for turning an
`lwz`/`stw` field into an address is `field = (address - 0x8041FD80) & 0xFFFF`** - the *full*
signed displacement, not half of it - and doing that subtraction by hand is a trap this lane
fell into twice (see `src/Kyoto/Graphics/CGraphicsScreenPosition.cpp`).
