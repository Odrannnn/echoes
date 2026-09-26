# `CGameGlobalObjects::CGameGlobalObjects` - written, Matching, and deliberately not landed

Lane `cgo`, 2026-09-26. Everything here is measured. The work is preserved, fixed, in
`docs/research/cgameglobalobjects_ctor.patch` (`git apply --3way` it; see "Applying the patch").

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

**Floor: 333 - 2 (other lane) - 3 (cheap) = 328, which is +2 over HEAD's 326 and +3 over the
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
