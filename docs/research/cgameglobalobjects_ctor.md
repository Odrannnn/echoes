# `CGameGlobalObjects::CGameGlobalObjects` - written, Matching, and deliberately not landed

Lane `cgo`, 2026-09-26. Everything here is measured. The work is preserved, fixed, in
`docs/research/cgameglobalobjects_ctor.patch` (`git apply --3way` it; see "Applying the patch").

## Update, lane `chain` (2026-09-26): landed - the constructor is in the port build

**The integration below is applied, and `tools/boot_probe.sh` now stops at step 17** (`CGameArchitectureSupport`'s
constructor: `CConsoleOutputWindow`, `CErrorOutputWindow`, `CMain::ResetGameState`) instead of
`gpGameState is null`. `tools/link_check.sh` is **315 -> 322** with it (the tree had moved from
325 to 315 since lane `frame` measured 338, so the patch itself now costs +12, not +13).
`docs/research/patches/cgameglobalobjects_integration.patch` was never committed: it survived
only in lane `frame`'s keeper directory, and is now simply the tree.

Five of the twelve are closed by real bodies, not stubs: `CSimplePool`'s constructor and
destructor by **`src/Kyoto/CSimplePoolPort.cpp`** (port-only: all nine virtuals too, so
`vtable for CSimplePool` is emitted with them - the premise under "`~CSimplePool` ... not written,
deliberately" below no longer holds), `fn_80009AC0` by listing `SGameStateMemcardBufFill.cpp`
with `lbl_80417D92` = 1, `fn_8000934C` by listing `CPlayerStateRefRelease.cpp` with a host
`__dt__12CPlayerStateFv`, and `CVParamTransfer::Null()` (already missing before) by the pool file.
**The eight left are itemised in `docs/research/port_link_gap.md`**, "The integration of
`CGameGlobalObjects`' constructor": six are the `CGameState` chain's unwritten callees (only
`fn_80145C98` runs at boot), and two are `CCharacterFactory`'s constructor and destructor, named
by `CCharacterFactoryBuilder::CDummyFactory::Build` and not reached.

## Update, lane `frame` (2026-09-26): the integration, measured end to end

**Listed together, the constructor and everything it constructs take the port's link from 325
to 338 undefined, and the boot gets past `gpGameState`.** Three of the thirteen are other lanes'
work in flight. So the brief's "326 or below with the constructor listed" is **not reached**, and
the integration is kept out of `files.cmake`. It is one `git apply` away:
`docs/research/patches/cgameglobalobjects_integration.patch` lists the nine units, adds
`gameGlobalObjects = new CGameGlobalObjects(*osContext, *memorySys)` to `CMain::RsMain`
(retail 0x80005CE4, stored at `CMain`+0x54), and drops the nine `check_files_cmake.py`
exclusions. Measured after applying it: 338, the same symbol set, and `check_files_cmake.py` ok.

`tools/link_check.sh`, unique undefined symbols, all on this worktree:

| tree | undefined |
|---|---|
| HEAD + the `cgo` patch, nothing listed | 325 |
| + `CGameGlobalObjectsCtor.cpp` listed (reproduces the table below) | 333 |
| + the three cheap closes and `CCharacterFactoryBuilder.cpp` | 331 |
| + `CGameState`'s whole default-construction chain, the block operations and the boot call (the patch) | **338** |
| the tree as left: every unit that is net zero listed, the rest excluded | **325**, the identical set |
| `CCharacterFactoryBuilder.cpp` alone | 329 (+4, see below) |

### What was closed, and what the eight became

| symbol | now |
|---|---|
| `lbl_80418EC8` | **closed**: host definition in `PortGlobals.cpp` |
| `fn_8016C230` | **closed**: `CInGameTweakManagerCtor.cpp`, `Matching` 100% |
| `fn_801F0A44` | **closed**: `CGameGlobalObjectsTailCtor.cpp`, `Matching` 100% (since 2026-10-01 it is `CRELFileManager::CRELFileManager()` in upstream's `CRelFile.cpp`; the carve is deleted) |
| `fn_80032008` | **closed**: `Factories/CCharacterFactoryBuilder.cpp`, `NonMatching` 80.33% (8/10 at 100%). It opens `CCharacterFactory`'s constructor (`fn_80030410`, 0x560, the root of 114 unwritten functions) **and its destructor**, because g++ `-O2` speculatively devirtualises the `delete` in `TObjOwnerDerivedFromIObj<CCharacterFactory>::~` into a guarded direct call. Net +1 over leaving `fn_80032008` open. |
| `fn_801449C8` | **closed** by listing `CGameStateCtor.cpp`, which is the chain below |
| `CSimplePool::CSimplePool(IFactory&)`, `fn_803096C4` | lane `v1`, in flight |
| `CSimplePool::~CSimplePool()` | **not written, deliberately.** Retail's is `fn_80300EF4` (0xA0: vptr store, virtual `Flush`, `fn_8030093C`, the `rc_ptr` and tree destructors - five functions, 0x234 bytes). On the host a body stores the vptr and g++ devirtualises `Flush`, which opens `vtable for CSimplePool` and `CSimplePool::Flush`; neither exists in the port. It becomes net -1 once whoever writes `CSimplePool`'s virtuals defines the vtable. |

The eleven small `CGameState`-chain dependencies found on the way are closed too: the seven guest
constants (`lbl_80417D90`/`D91`/`D93`, `lbl_804183DD`/`DF`, `lbl_8041C1A8`/`B8`, values read from
the DOL, in `PortGlobals.cpp`) and `SGameStateBlock`'s `rstl::vector<unsigned char>` operations
(`fn_80004AA0`, `fn_80004D5C`, `fn_80142914`, `fn_80142BA4`, `fn_801465EC`, five units, three
`Matching`). With them, `CGameStateSlotsCtor.cpp`, `CGameStateBlockCopy.cpp`,
`CGameStateBlockDtor.cpp`, `CGameStateSysOptsPutTo.cpp` and `CGameStateSlotDefaults.cpp` became net
zero and are listed.

### Where the boot stops now

`tools/boot_probe.sh` with the patch applied (the probe's reach stubs log every unwritten function
the boot calls, in order). **Before**: `boot stopped: gpGameState (DOL 0x80418EB8) is null.`
**After**:

```
[reach-stub 0019] fn_803096C4                      <- CGameGlobalObjects+0x00 (v1)
[reach-stub 0020] CSimplePool::CSimplePool(IFactory&)   <- +0xE4 (v1)
[reach-stub 0021] CSimplePool::CSimplePool(IFactory&)   <- CCharacterFactoryBuilder+0x04 (v1)
[reach-stub 0022] fn_80145C98    x2                <- CGameState+0x54 / +0xDC options
[reach-stub 0024] fn_80193E08
[reach-stub 0025] fn_80009AC0                      <- SGameStateMemcard (v4)
[reach-stub 0026] CStaticInterference::CStaticInterference(int), fn_8000934C   x4
...
boot stopped: CGameArchitectureSupport's constructor is reachable - both globals are
  set - but three of the functions it calls still have no body (CConsoleOutputWindow,
  CErrorOutputWindow, CMain::ResetGameState), so step 17 cannot complete and no frame
  has been rendered.
```

So the frontier is `CGameArchitectureSupport`'s constructor (step 17), and the next three are
`cwin`'s two windows and `CMain::ResetGameState` (`CMainResetGameState.cpp`, 98.61%, excluded at +15).
Two probe runs before that one crashed, and both were real bugs rather than stub artefacts: a
segfault inside `fn_800098CC` (a `reinterpret_cast` a header change had silently rewritten - fixed,
`SGameStateMemcardFill.cpp` is back at 99.55%), then one in `fn_80142DD4` flushing a stream into
the buffer `fn_80142BA4` had not allocated (that was the unwritten block fill, now written).

**The probe needs work of its own first.** At HEAD it does not link: `PortReachStubs.cpp` is
generated from `docs/research/boot_path_reachable.tsv` and is stale by 19 symbols that have nothing
to do with this work (`REL_LoadPuffer`, `fn_60_B8`, `lbl_70_rodata_C`, ...). The runs above added
temporary stubs for those nineteen and for the integration's own (`fn_8000934C` and the thirteen), and put the generated file
back afterwards. Regenerating the TSV with `tools/link_reach.py` is the fix.

### What still stands between this and a frame, exactly

Of the 338 with the patch applied, the thirteen the integration adds, with the size of what each
one drags in (retail call graph walked against the port's defined symbols) and whether the boot
reaches it:

| symbol | owner | subtree | reached at boot |
|---|---|---|---|
| `CSimplePool::CSimplePool(IFactory&)` | `v1` | - | yes, twice |
| `fn_803096C4` | `v1` | - | yes |
| `fn_80009AC0` | `v4` | 2 functions, 0x131 | yes |
| `fn_80145C98` | unassigned | 15 functions, 0x963 | yes, twice |
| `fn_8000934C` | written (`CPlayerStateRefRelease.cpp`), unlinkable in the DOL and C-named `__dt__12CPlayerStateFv` on the host - listing it swaps one symbol for the other | - | yes, 4x |
| `CCharacterFactory` ctor and dtor | unassigned | 114 functions, 0x467A | no |
| `CSimplePool::~CSimplePool()` | needs `CSimplePool`'s vtable first | 5 functions, 0x234 | no (exception cleanup only) |
| `fn_80145628`, `fn_80145A2C` | unassigned | 16 functions, 0x72F | no (`gpMemoryCard` is null) |
| `fn_8014306C`, `fn_801437DC`, `fn_80180430` | unassigned | 65 + 44 + 7 functions | no (`gpMemoryCard` is null) |

The link count falls to the 326 baseline only when the ten unassigned lines are written, and three
of those (`fn_80145C98`, `fn_8000934C` and `CSimplePool`'s vtable) are the only ones the boot
actually executes. **For the frame the order is: `v1` and `v4` land, then `fn_80145C98`, then the
three step-17 functions.** The other seven are link-count work, not boot work.

**Superseded, further down:** "Floor: 333 - 2 - 3 = 328" assumed `fn_80032008` could close net-negative.
It cannot, because `Build` needs `CCharacterFactory`, whose destructor g++ names as well.

## The decision, and the number that decided it

The constructor (`__ct__18CGameGlobalObjectsFR10COsContextR10CMemorySys`, retail 0x8000848C, 0xE4
bytes, the only writer of `gpGameState` in the DOL, boot step 7) is a `Matching` unit at 100.00%,
and on the DOL side it now costs nothing. **It is still not landed, because listing it would make
the port's link worse, and no amount of work in this lane can get that to neutral.**

`tools/link_check.sh`, unique undefined symbols:

| tree | undefined |
|---|---|
| HEAD (453cdfa), no re-split | **326** |
| patch applied, `CGameGlobalObjectsCtor.cpp` **not** in `files.cmake` | 325 |
| patch applied, `CGameGlobalObjectsCtor.cpp` listed | **333** (+8, 0 closed) |

The 326 -> 325 step is not progress. HEAD's stub constructor in `main.cpp` instantiated the inline
`CSimplePool(IFactory&)` and so asked for `vtable for CSimplePool`, and the patch removes the stub.
Separately, `mainTail.cpp` happens to emit `TCastToPtr<CPlayer>(CEntity*)`, which
`CScriptCannonBall.cpp` asks for.

### The eight symbols listing adds

| symbol | what it is | closable how | cost |
|---|---|---|---|
| `CSimplePool::CSimplePool(IFactory&)` | retail 0x80301008 | the concurrent `CSimplePoolCtor.cpp` lane | theirs |
| `fn_803096C4` | the +0x00 member's ctor (one-shot `CARDInit`) | the concurrent `CResFactoryCardInit.cpp` lane, **if it keeps the `extern "C" fn_803096C4` name** this header calls | theirs |
| `lbl_80418EC8` | `.sbss` word, = `this+0x150` | port-side definition (it is in an `auto_` range, so not in a mwcceppc unit) | cheap |
| `fn_8016C230` | `CInGameTweakManager` ctor, 5 instructions (`li r0,0` and 3 `stw`) | a Matching unit, host-compilable | cheap |
| `fn_801F0A44` | the +0x150 member's ctor, 12 instructions; stores two **uninitialised frame bytes** at +0/+1 | a Matching unit using the `volatile` 8-byte-local trick in `CGameStateCtor.cpp`'s family | cheap |
| `fn_801449C8` | `CGameState::CGameState()` | list `CGameStateCtor.cpp`, which is **itself +10** on the port (its entry in `tools/check_files_cmake.py`) | subtree |
| `fn_80032008` | `CCharacterFactoryBuilder` ctor: `CDummyFactory` vptrs + `CSimplePool(x0_dummyFactory)` at +4 | a Matching unit is easy, but on the host it asks for `vtable for CCharacterFactoryBuilder::CDummyFactory`, which means all six virtuals: `fn_80031E70` (dtor, 0x5C), `fn_800320EC` (0x144), `fn_80032060` (0x8C), `fn_8003205C`, `fn_80031E60`, `fn_80031E68` | subtree |
| `CSimplePool::~CSimplePool()` | retail `fn_80300EF4`, 0xA0. The host asks for it **only from g++'s exception cleanup** (`.text.unlikely`) for `simplePool` | a body that calls `Flush` through the vtable and `fn_8030093C`, then destroys the `rc_ptr` and the `hash_map` (`fn_80300F94`) | subtree |

**Floor: 333 - 2 (other lane) - 3 (cheap) = 328** (superseded - see the update above; the cheap three are written and the measured result is 338 for the whole chain), which is +2 over HEAD's 326 and +3 over the
re-split tree's 325.** Each of the last three opens more symbols than it closes. That matches how
this project has behaved all along: a written body names every retail callee it reaches.

A host caller (`new CGameGlobalObjects(os, mem)` in `PortBoot.cpp`'s `RsMain`) adds nothing to the
count, because the constructor is already asked for once the file is listed. It buys nothing either
until the eight resolve: with `--warn-unresolved-symbols` the first call into `fn_801449C8` jumps to
0.

**Count `fn_801449C8` as an edge, not as this unit's cost.** Whichever of this unit and
`CGameStateCtor.cpp` is listed second closes it. The port is ready for this constructor when the
`CGameState` default-construction subtree, `CCharacterFactoryBuilder` and `CSimplePool` are ready.
The constructor itself is written, so listing it last costs nothing.

## What the patch contains, and what was fixed relative to the reverted version

The DOL side of the patch, measured with `tools/gate.sh`: **GATE PASS**, matched 3187 -> 3188,
linked 1803 -> 1804, DOL sha1 `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, 86/86 RELs,
`flip_test.sh MetroidPrime/CGameGlobalObjectsCtor.cpp` **PASS**, and **no function falls**. The
only `WORSE` line is `StreamNewGameState` at 25.26% -> 25.26%, a rounding artefact.

These fixes are in the patch:

1. **`~CGameArchitectureSupport` stays in `main.cpp`, at 95.27%.** The first version moved it to
   `mainTail.cpp` to get `__dl__38TOneStatic<24CGameArchitectureSupport>FPv` emitted there. The
   destructor is at 0x80007DE8, inside `main.cpp`'s range, so its copy in `mainTail` paired with
   nothing and scored **0.00%**. The deleting operator is now reached from `mainTail.cpp`'s
   `SForceTailWeakCopies` destructor
   (`TOneStatic<CGameArchitectureSupport>::operator delete(mgr)`). Both functions score: the
   destructor at 95.27% and `__dl__` at 100%, with `mainTail` at 15/48.
2. **The `Touch()`/`TouchWin()` helpers were dead code.** `nm` shows neither in the object, and
   deleting both leaves `mainTail` at 15/48. The weak `ReleaseData` and list-destructor copies
   come from the struct's implicit constructor and destructor, which the namespace-scope object
   forces mwcceppc to generate. The note "`AddIOWin(new CMainFlow())` makes it 100%, a bare copy
   73.6%" described code that was never emitted.
3. **The +0x150 member is 0x14 bytes, not 0x18.** `CMain::RsMain` allocates the object with
   `li r3,0x164` at 0x80005CC4, and 0x150 + 0x14 = 0x164.
4. **`~CGameGlobalObjects()` being declared does not suppress the five weak member-destructor
   copies** (476 bytes: `__dt__Q24rstl24single_ptr<10CGameState>Fv` and four siblings, all `W`).
   The header used to claim it did. They are harmless: `flip_test.sh` PASSes with them in the
   object, so the CAi precedent holds for a second unit. The previous lane asked whether these
   bytes cost the destructor's pairing. They did not. Moving the source did (point 1).
5. Stale comments corrected. `configure.py` and `main.cpp` said "one unit claims two ranges",
   which contradicts the three-unit split that was actually done. `PortBoot.cpp`'s stop message,
   the constructor's header and the `check_files_cmake.py` entry now carry the numbers above.

The patch does **not** include the previous lane's comment edit to `src/Kyoto/CResFactoryCtor.cpp`,
because the concurrent lane is rewriting that file.

## The mechanics, so the next session does not re-derive them

- **A DOL unit may not claim two ranges in one section.** dtk reports "Cyclic dependency
  encountered while resolving link order: MetroidPrime/main.cpp ->
  MetroidPrime/CGameGlobalObjectsCtor.cpp". So `main.cpp` is cut three ways: `main.cpp`
  0x800053B8-0x8000848C, `CGameGlobalObjectsCtor.cpp` 0x8000848C-0x80008570 (`Matching`), and
  `mainTail.cpp` 0x80008570-0x80009880. **`.ctors` and `.sbss` go on the last of the three**,
  otherwise dtk reports "Mismatched splits for .ctors". The source above 0x80008570 moves into
  `mainTail.cpp`, because objdiff pairs by name within a unit.
- **A new unit must not *define* a global inside another unit's claimed `.sbss`.** Defining
  `gpCharacterFactoryBuilder` in the constructor's unit produces the same cycle message. Declare
  it `extern` instead. `lbl_80418EC8` is `extern` for the same reason.
- **Two `symbols.txt` renames are forced:** `fn_802FB154` -> `__ct__11CResFactoryFv` and
  `fn_80301008` -> `__ct__11CSimplePoolFR8IFactory`. Both classes have a polymorphic base, so an
  inline constructor body puts the base vptr store (and, for `CSimplePool`, the whole `hash_map`
  construction) in the caller. The constructors must be declared-only, and the mangled name must
  exist in the DOL.
- **Unnamed member classes** (+0x00, +0x108, +0x150) are modelled as classes of the right size
  whose *inline* constructor is one `extern "C"` call (`fn_803096C4`, `fn_80032008`,
  `fn_801F0A44`). That puts the `bl` at retail's address with no rename.
- The `operator new` file operand here is **`lbl_803A56C0`**, not `CGameStateCtor.cpp`'s
  `lbl_803A9208`. The `operator new` declarations must come **before** every include in this
  unit, because `rstl::construct<>` is instantiated by the header graph.
- `CGameState* made = self; if (made != 0) made = f(made); return made;` is the only spelling
  that gives retail's `mr r0,r3 ; stw r0,304(r31)`. The ternary, a `static_cast` and a
  `reinterpret_cast` round trip all give `stw r3`.

## Applying the patch

Against 453cdfa it applies cleanly (`git apply --check`). Against `master` (b01d5fb), everything
applies except two hunks, whose context moved when other units were added:
`config/G2ME01/splits.txt` (two new units sit next to `main.cpp`'s block) and
`tools/check_files_cmake.py`. `git apply --3way` or a hand merge of those two is enough. Neither
hunk conflicts in substance.

After applying: coordinate with whichever lane owns `CResFactory`/`CSimplePool`. The patch
changes both headers to declared-only constructors and renames both symbols, which is what that
lane needs as well. Then run the gate and `link_check.sh`, and put the unit in `files.cmake` only
once the three subtrees above are written.
