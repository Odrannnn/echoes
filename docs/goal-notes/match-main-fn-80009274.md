# match-main-fn-80009274

`kind: match`, `target: MetroidPrime/main.cpp`. `fn_80009274` is now written as
`CWorldLayerState::~CWorldLayerState()`, and the **6 functions / 624 bytes** the item predicted
all reach 100%. The unit does not flip, and it is not close: the flip is blocked by four
pre-existing link-level holes and the unit is still SHORT by 8860 bytes of `.text`.

## What `fn_80009274` is

Retail 0x80009274, 0x84 = 132 bytes, and it is **not** a retail-named function: `symbols.txt:192`
had it as an unnamed `fn_`. It is the deleting destructor of the class whose header already
existed in the tree with the right members in the right order and no destructor at all,
`CWorldLayerState` (`include/MetroidPrime/CWorldLayerState.hpp`, `CHECK_SIZEOF(..., 0x34)`).
The identification is not a guess - it is pinned three ways:

- its only caller is `rstl::rc_ptr<CWorldLayerState>::ReleaseData` (0x80009224), which does
  `li r4,1 ; bl fn_80009274` - the same shape as `ReleaseData__Q24rstl22rc_ptr<12CPlayerState>Fv`
  (0x8000934C) that `CGameState.o` already emits at `0x5e68`. `tools/who_calls.py 0x80009224`
  returns `__dt__13CStateManagerFv`, `InitializeMemoryWorlds__10CGameStateFv`,
  `ConfigureGameModeLayers`, `fn_80143E88__Fv` and
  `__ct__11CWorldStateFR16CBitStreamReaderUiRC18CWorldSaveGameInfo` - i.e. retail's
  `CWorldState::mLayerState` holder at `include/MetroidPrime/Player/CWorldState.hpp:46`.
- the four member offsets the body uses are +0x2C, +0x24, +0x10, +0x00, which is the header's
  declaration order read back: `mLayerNameOffsets` (rc_ptr, 8 bytes), `mLayerNames` (rc_ptr, 8),
  `mSaveLayers` (bit_vector, 0x14), `mAreaLayers` (vector, 0x10) - and 0x34 is what that adds up
  to, which is the size the header already asserted.
- the two `rstl::rc_ptr` targets name themselves: `ReleaseData__Q24rstl53rc_ptr<Q24rstl36vector<i,
  Q24rstl17rmemory_allocator>>Fv` (vector<int> = `mLayerNameOffsets`) and
  `ReleaseData__Q24rstl128rc_ptr<Q24rstl110vector<rstl::basic_string<char>,...>>>Fv`
  (vector<string> = `mLayerNames`).

**No source-level modelling was needed and none was invented.** Only the destructor's linkage
was wrong, exactly as in `match-main-cmapworldinfo-dtor`. The body is `{}`; the compiler
generates all 132 bytes, the four member teardowns in reverse declaration order each with
`li r4,-1` so the member destructors run their bodies and skip their own `operator delete`, then
the `extsh. r0,r31 / ble / mr r3,r30 / bl CMemory::Free` deleting tail.

## What changed

| file | change |
|---|---|
| `include/MetroidPrime/CWorldLayerState.hpp:22-27` | **added** `~CWorldLayerState();` - the class had no destructor at all |
| `src/MetroidPrime/main.cpp:37` | `#include "MetroidPrime/CWorldLayerState.hpp"` |
| `src/MetroidPrime/main.cpp:713-733` | `CWorldLayerState::~CWorldLayerState() {}` + the comment |
| `src/MetroidPrime/PortGlobals.cpp:66,1126-1140` | PC-side body, plus the include |
| `config/G2ME01/symbols.txt:192-193` | **renamed** the two unnamed retail symbols (see below) |

The `symbols.txt` rename is the one change that is not source, and it is the only way two of the
six can count: objdiff pairs functions **by name**, so while retail's bytes at 0x80009274 and
0x800092F8 sat in the report as unmatched `fn_80009274` / `fn_800092F8`, our
`__dt__16CWorldLayerStateFv` and
`__dt__Q24rstl56vector<Q212CWorldLayers4Area,Q24rstl17rmemory_allocator>Fv` were not compared
against anything and stayed at 0%. Renamed:

```
fn_80009274 -> __dt__16CWorldLayerStateFv                                          (0x84, strong)
fn_800092F8 -> __dt__Q24rstl56vector<Q212CWorldLayers4Area,
                     Q24rstl17rmemory_allocator>Fv                                (0x54, weak)
```

Both names are read back off our own object, so the rename asserts nothing new: they are the
mangled names mwcceppc gives the two bodies at those addresses, and the sizes still match
(`0x84`, `0x54`). `scope:weak` on the second is what our object says too (`nm` shows `W` in both
`main.o` and `CWorldLayerState.o`). The 0x800092F8 body is byte-for-byte the same 84-byte shape as
`fn_800091D0` that `CMapWorldInfo`'s destructor calls twice - they are the two instantiations of
one `rstl::vector` destructor template, on `CWorldLayers::Area` and on
`rstl::pair<TEditorId, bool>`. **Note for the next run:** this is a `config/` edit, so report it
as a list of intended changes, and note that a *sibling* rename is still available -
`fn_800091D0` (0x800091D0, 0x54) is our
`__dt__Q24rstl62vector<Q24rstl18pair<9TEditorId,b>,Q24rstl17rmemory_allocator>Fv` and would gain a
seventh function the same way, but it belongs to `CMapWorldInfo`'s item and I did not touch it.

## Measured, from `build/report.json`

`main/MetroidPrime/main` **40 -> 46 of 99** functions, `.text` fuzzy **35.91% -> 39.46%**,
`matched_code` 5300 -> 5924 bytes (+624). All six at 100.0%:

```
__dt__16CWorldLayerStateFv                                                132 B  -> 100.0%   (the item's fn_80009274)
__dt__Q24rstl56vector<Q212CWorldLayers4Area,Q24rstl17rmemory_allocator>Fv  84 B  -> 100.0%   (fn_800092F8, the rename)
ReleaseData__Q24rstl53rc_ptr<Q24rstl36vector<i,...>>Fv                     80 B  -> 100.0%
ReleaseData__Q24rstl128rc_ptr<Q24rstl110vector<rstl::basic_string<char>,...>>>Fv  80 B  -> 100.0%
__dt__Q24rstl36vector<i,Q24rstl17rmemory_allocator>Fv                      84 B  -> 100.0%
__dt__Q24rstl110vector<rstl::basic_string<c,...>,Q24rstl17rmemory_allocator>Fv  164 B  -> 100.0%
```

Tree-wide, comparing the full per-function report before and after (baseline copied from
`build/report.json` before the first edit, full rebuild, diff of every `(unit, function)` pair):

```
fns 10059 -> 10065   matched_code 1521584 -> 1522208 (+624)   units 728 -> 728
dol fns 8612 -> 8618   modules 6.2351 (unchanged)   game fns 8728 -> 8734   sdk 98.6673 (unchanged)
functions worse: 0    functions better: 4    functions new (the two renames): 2
```

Gates, all on this tree: `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`;
`tools/probe_sources.sh` = `750 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0
duplicates)` against a baseline of 250 - no growth, no NEW, no GONE; `check_symbol_names.py` =
`0 declared names are missing`; 86 RELs `cmp`-equal and every sha1 in `config.yml` matches.
`main/MetroidPrime/CWorldLayerState` is `Matching` and stays **15 / 15 at 100.0%** - the header
edit did not move a byte of it, which is the check that matters for a shared header.
`flip_test.sh MetroidPrime/main.cpp` **FAILs**; the reasons are below.

## The flip, and why it is out of reach

`./tools/flip_test.sh MetroidPrime/main.cpp` -> `FAIL -> reverted`, and the linker names the same
four blockers the previous item on this unit saw:

```
multiply-defined: 'CErrorOutputWindow::__vt' in CErrorOutputWindow.o
undefined: 'fn_80008C28'      undefined: 'fn_80009224'
undefined: 'rstl::rc_ptr<CMapWorldInfo>::ReleaseData()'
```

`fn_80009224` is retail's `rstl::rc_ptr<CWorldLayerState>::ReleaseData` sitting in main's claimed
range, and our tree emits it as a weak COMDAT in `CGameState.o` (`0x5e68`) instead. A
`symbols.txt` rename cannot fix that one, because the report pairs by `(unit, name)` and the
body is not in `main.o`; it would take a *use* of the `rc_ptr` in `main.cpp`, which is a
contrived use invented only to host a symbol. All four are pre-existing and none was made worse
or better by this change.

`tools/unit_fit.sh MetroidPrime/main.cpp` from the other side: **`.text` claimed 17608, ours 8748,
SHORT by 8860** (it was 8124 / SHORT by 9484 before `match-main-cmapworldinfo-dtor`, and 9484
again before this item), plus 20 functions (1708 bytes) ours has and retail's does not, and
`.ctors` SHORT by 4. 53 functions are still unmatched, the cheapest-looking still being
`AddPaksAndFactories` (0.21%, 1936 bytes), `RsMain` (0.19%, 2148 bytes) and
`SetMaxSpeed__5CMainFb` (0.00%, 96 bytes) - all three essentially unwritten.

`tools/check_decl_order.py --unit "MetroidPrime/main" --list` reports **38 fns, would break on a
flip** (37 before). That is inherited and already listed in `docs/research/decl_order.md:48` as
"not a flip candidate"; the +1 is the new destructor, which is placed in the descending-by-retail-
offset run its address calls for - unit offsets 0x1FBC (`EnsureWorldPaksReady`, 0x80007364), then
0x3ECC (`CWorldLayerState::~CWorldLayerState`, 0x80009274), then 0x3D00
(`CMapWorldInfo::~CMapWorldInfo`, 0x800090A8), so
in the emitted object `__dt__13CMapWorldInfoFv` (`.text+0x2e0`) comes before
`__dt__16CWorldLayerStateFv` (`.text+0x45c`) where retail has 0x800090A8 before 0x80009274. The
unit's permutation is not made worse in the run the new function belongs to.

WALL: MetroidPrime/main.cpp flip - four pre-existing link-level blockers (`CErrorOutputWindow::__vt`
multiply-defined, `fn_80008C28` / `fn_80009224` undefined, `rc_ptr<CMapWorldInfo>::ReleaseData`
undefined) and .text still SHORT by 8860 bytes over 53 unwritten functions, so no amount of
work on a single 132-byte function in this unit can flip it; treat it as `progress`-shaped.

## The port side, and why `PortGlobals.cpp`

The header change is unconditional (the matching build does not define `TARGET_PC`, so an
`#ifdef` would hide it from exactly the build that needs it). The cost is that the PC link wants
`CWorldLayerState::~CWorldLayerState()`, and `PortGlobals.cpp` is where a PC-only definition
belongs: it is not a `configure.py` unit, so a definition there cannot collide with a retail
object at DOL link time nor perturb any unit's `.text`. After the body, `probe_sources.sh` reports
the same 250 undefined as the baseline - no NEW, no GONE, so this one cost the port nothing.
Unlike `~CErrorOutputWindow` there is no second-order vtable effect: `CWorldLayerState` is not
polymorphic, so the declared-but-undefined destructor is only ever a call, never a vtable. The
body is empty for the same reason `CMainFlow::~CMainFlow()` is: the compiler destroys the four
members and the storage. It is not on the boot path.

## What a next run on this unit should know

- 53 functions are still unmatched and the unit is **SHORT by 8860 bytes** before any of them is
  written. `match-main-cmapworldinfo-dtor` and this item each took a generated destructor out of
  line; that is the cheap class of work here and the well is not dry - the remaining unnamed
  `fn_` bodies in main's range are `fn_80009008` (0x50), `fn_800095E4` (0x50), `fn_80008E94`
  (0xB0), `fn_80008D68` (0x80), `fn_80008CE0` (0x88) and friends, and the same
  "the class has no destructor / the use is inlined" arrangement is what most of them will want.
- **`fn_800091D0` is a free seventh function**: 0x800091D0, 0x54, is our
  `__dt__Q24rstl62vector<Q24rstl18pair<9TEditorId,b>,Q24rstl17rmemory_allocator>Fv` (already in
  `main.o` at `.text+0x408`, 84 bytes, because `CMapWorldInfo`'s destructor calls it twice), and
  a one-line `symbols.txt` rename would put it at 100%. Left alone here because it is not this
  item's target.
- The two `rc_ptr` release bodies and both `rstl::vector` destructors this item collected are
  **COMDAT weak template instantiations**: they are emitted into `main.o` because that is the only
  TU that references them, and `unit_fit.sh` will keep listing the ones retail put elsewhere as
  "functions present in ours but not in the retail unit object". That is the harmless-cause class
  its own output describes, and `MetroidPrime/CWorldLayerState.o` carries the same weak
  `__dt__vector<CWorldLayers::Area>` copy byte-for-byte.
- `main.cpp` is a *shared* unit: it carries `__dt__24CGameArchitectureSupportFv` (100%),
  `UpdateTicks` (98.51%), `__ct__5CMain` (88.80%), `AddWorldPaks` (96.00%) and
  `EnsureWorldPaksReady` (100%). Any edit here must leave those where they are, which is why the
  before/after per-function diff in this file is the check to run - **not** the DOL sha1, which is
  green whether or not `main.cpp` is in the link at all.
- The lesson from `match-main-cmapworldinfo-dtor` still holds and this item did **not** trip it:
  an inline `virtual ~X() {}` in a header is load-bearing for every other unit that instantiates
  the teardown. `CWorldLayerState` is safe to take out of line because every other holder
  (`CGameState.o`, `CStateManager`) goes through `rc_ptr`/`ncrc_ptr` and already calls retail's
  out-of-line `ReleaseData`; had retail inlined the teardown anywhere, this would have been a
  DOL-wide byte move the way `~CMemoryInStream` was.

---

# run 2 (lane 4, 2026-09-30) - re-measured, 44 -> 55, and the notes above were stale on the
# *baseline*, not on the method

The tree I started from was clean at `b32bb9f` and did **not** contain any of the run-1 work, so
everything above had to be redone. What survived from the notes and was worth the time: the
identification of `fn_80009274` (`CWorldLayerState::~CWorldLayerState`) and the mechanism
(a declared-not-defined destructor makes the compiler generate the whole 132-byte body). What was
stale: the counts. The baseline moved while this item sat in the queue, so **re-measure**:

```
baseline  b32bb9f  main/MetroidPrime/main  44 / 99 functions, matched_code 6120, fuzzy 39.72%
                       tree                 10065 matched functions
```

So the `+6` the item predicted was still all available, and the 34 functions run 1 called
"still unmatched" are 34 after this run, not 53.

## What this run changed (four files, 37 insertions, 9 deletions)

| file | change |
|---|---|
| `include/MetroidPrime/CWorldLayerState.hpp:22` | **added** `~CWorldLayerState();` (the class had no destructor) |
| `src/MetroidPrime/main.cpp:38` | `#include "MetroidPrime/CWorldLayerState.hpp"` |
| `src/MetroidPrime/main.cpp:830-843` | `CWorldLayerState::~CWorldLayerState() {}` + the comment, in the descending-by-retail-offset run |
| `src/MetroidPrime/PortGlobals.cpp:67,1123-1134` | PC-side body plus the include |
| `config/G2ME01/symbols.txt` | **seven** renames (below) |

The destructor is exactly run 1's arrangement and run 1's disassembly of 0x80009274 re-measures
identically on this tree (`addic. r0,r30,44` / `beq` / `bl ReleaseData` at +0x2C, the same at
+0x24, `addi r3,r30,16 / li r4,-1 / bl __dt__bit_vector` at +0x10, `bl` to 0x800092F8 at +0x00, and
the `extsh. r0,r31 / ble / mr r3,r30 / bl CMemory::Free` tail). The body is `{}`; the header's
`CHECK_SIZEOF(CWorldLayerState, 0x34)` and member order are untouched.

## The other six functions: `symbols.txt` renames, all measured at 100.0%

objdiff pairs by **name**, so while retail's bytes sat under `fn_` / `__dt__8000xxxx` names our own
bodies were never compared to anything. Each name below is read back off **our own `main.o`** with
`powerpc-eabi-nm` (the mangled name mwcceppc gives that body) and each size equals retail's. The
identification is *measured*, not asserted: each rename was applied and the resulting objdiff score
read off `build/report.json`.

```
0x800057EC  0x60 -> destroy<Q24rstl297pointer_iterator<Q24rstl94pair<Q24rstl66basic_string<c,...>,10SObjectTag>,...>>__4rstlF...   96 B -> 100.0%
0x80007AC8  0x70 -> do_insert_before__Q24rstl55list<20CArchitectureMessage,Q24rstl17rmemory_allocator>FPQ34rstl55list<...>4nodeRC20CArchitectureMessage   112 B -> 100.0%
0x80007B38  0x88 -> create_node__Q24rstl55list<20CArchitectureMessage,Q24rstl17rmemory_allocator>FPQ34rstl55list<...>4nodePQ34rstl55list<...>4nodeRC20CArchitectureMessage   136 B -> 100.0%
0x80008AB0  0x24 -> ReferenceCount__38TOneStatic<24CGameArchitectureSupport>Fv    (scope:weak)   36 B -> 100.0%
0x800091D0  0x54 -> __dt__Q24rstl62vector<Q24rstl18pair<9TEditorId,b>,Q24rstl17rmemory_allocator>Fv  (scope:weak)  84 B -> 100.0%
0x80009274  0x84 -> __dt__16CWorldLayerStateFv                                                                        132 B -> 100.0%
0x800092F8  0x54 -> __dt__Q24rstl56vector<Q212CWorldLayers4Area,Q24rstl17rmemory_allocator>Fv  (scope:weak)  84 B -> 100.0%
```

`0x800091D0` was run 1's "free seventh function", left alone there because it looked like it belonged
to another item's class. It belongs to this unit and it is taken here: the item's target is
`MetroidPrime/main` and the count that decides the item is main's.

### How the candidates were found, so the next run does not have to

Run 1's notes warn that the remaining unnamed bodies "will want" the same arrangement. That is true
but it is not the fastest route. **A byte-exact search finds nothing** (the `bl` displacements
differ), so the working filter is a *relocation-aware* instruction comparison: disassemble
`build/G2ME01/main.elf` and `build/G2ME01/src/MetroidPrime/main.o` with
`powerpc-eabi-objdump -d --no-show-raw-insn`, keep the mnemonic and the **operands of the
load/store/arith mnemonics only**, replace every branch/call target with a token (so unresolved
`R_PPC_REL24` and resolved `bl` compare equal), and require equal length. Every rename above is a
1:1 hit that way. Script shape: read the section table with
`powerpc-eabi-readelf -S -W` to get each `.text`'s file offset, then the comparison above. This
finds the 7 above plus the ambiguous cases listed under "what is left".

## Measured, from `build/report.json`

`main/MetroidPrime/main` **44 -> 55 of 99** functions, `.text` fuzzy **39.72% -> 45.90%**,
`matched_code` 6120 -> 7208 (**+1088**). Tree-wide, full per-function diff against the
`b32bb9f` baseline:

```
fns 10065 -> 10076 (+11)   matched_code 1522548 -> 1523636 (+1088)
better 4   (the four rc_ptr/vector bodies the destructor paired)
new    7   (the seven renames)
worse  0   gone 0 (the 7 "gone" are the renamed `fn_`/`__dt__8000xxxx` keys, same addresses)
```

**No function anywhere got worse, and no unit lost a match.**

`./tools/goal_check.sh build/goal/item.json` on this tree:

```
ok  no judge-owned path touched
ok  gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
ok  counts: matched 10065 -> 10076   linked 4918 -> 4918
ok  check_symbol_names.py
ok  All:  31.02% fuzzy, 23.31% matched, 11.78% linked (10076 / 28465 functions)
flip  flip_test MetroidPrime/main.cpp: FAIL - judged below as partial progress
ok  target rose: main/MetroidPrime/main: 44 -> 55 / 99 functions
ok  no asm added
goal_check: PARTIAL match-main-fn-80009274 - flip_test MetroidPrime/main.cpp: FAIL, but the target rose
```

Gates on their own: `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`;
`tools/probe_sources.sh` = `750 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0
duplicates)` against a baseline of exactly 250, so no NEW and no GONE; `check_symbol_names.py` =
`checked 504 units; 0 declared names are missing`; `gate.sh`'s `hashes vs config.yml` = ok (all 86
RELs). `main/MetroidPrime/CWorldLayerState` is `Matching` and stays **15 / 15 at 100.0%** - the
header edit moved no byte of it, which is the check that matters for a shared header.

## The flip: still four pre-existing link-level blockers, and I did not touch them

`./tools/flip_test.sh MetroidPrime/main.cpp` -> `FAIL -> reverted`, and the DOL rebuilds to
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` afterwards. The linker names the same four, unchanged
from run 1:

```
multiply-defined: 'CErrorOutputWindow::__vt' in CErrorOutputWindow.o
undefined: 'fn_80008C28'      undefined: 'fn_80009224'
undefined: 'rstl::rc_ptr<CMapWorldInfo>::ReleaseData()'
```

`tools/unit_fit.sh MetroidPrime/main.cpp` from the other side, and this part **moved in our favour**:
`.text` claimed 17608, ours 9512, **SHORT by 8096**; `.ctors` still SHORT by 4; and the extras list
fell from **20 functions / 1720 bytes to 16 / 1340**, because four of the twenty are no longer
"present in ours but not in the retail unit object" - they are paired now. `check_decl_order.py
--unit "MetroidPrime/main" --list` = 41 fns permuted (39 before this run; the two new ones are the
destructor and nothing else), inherited and already listed in `docs/research/decl_order.md:48` as
"not a flip candidate"; the destructor sits in the descending-by-retail-offset run its address calls
for, immediately after `~CMapWorldInfo` (0x800090A8) and before `~CPlayerState`.

WALL: MetroidPrime/main.cpp flip - the same four pre-existing link-level blockers
(`CErrorOutputWindow::__vt` multiply-defined, `fn_80008C28` / `fn_80009224` undefined,
`rc_ptr<CMapWorldInfo>::ReleaseData()` undefined) and .text still SHORT by 8096 bytes over 34
unwritten functions, so no amount of work on any single function in this unit can flip it; treat
this unit as `progress`-shaped. (Re-measured this run, not copied.)

## What is left, and the three leads a next run should take

34 functions are still unmatched. Sorted by how close they are, not by size:

1. **Two `TOneStatic` renames away - 80 bytes, needs a use, not a rename.** Retail's
   `TOneStatic` family appears **twice** in this range. This run took the
   `CGameArchitectureSupport` one: `fn_80008AB0` -> `ReferenceCount__38TOneStatic<24CGameArchitectureSupport>Fv`,
   pinned by `powerpc-eabi-objdump -r` on our own `__dl__...` at `.text+0x2198` showing
   `R_PPC_REL24 ReferenceCount__38TOneStatic<24CGameArchitectureSupport>Fv`, and by retail's
   `__dl__38TOneStatic<24CGameArchitectureSupport>FPv` (0x80008A78, already 100%) calling
   0x80008AB0. The second family is `0x80008AD4` (48) / `0x80008B04` (44) /
   `0x80008B30` (12) / `0x80008B3C` (36), and it is `TOneStatic<CGameGlobalObjects>`, on retail's
   own evidence rather than on a guess: `tools/dis.sh 0x800065E4 0x44` shows the tail of
   `__dt__CGameGlobalObjects_80006518` is `mr r3,r30 / li r4,-1 / bl __dt__CMemoryCardSys /
   extsh. r0,r31 / ble / mr r3,r30 / bl 0x80008B04` - i.e. **the deleting-destructor tail with
   `li r4,-1`**, exactly the shape of the `__dl__38TOneStatic<24CGameArchitectureSupport>FPv` just
   paired at 0x80008A78, so 0x80008B04 is
   `__dl__38TOneStatic<18CGameGlobalObjects>FPv`. 0x80008B3C (36 B) is the same lazy-init accessor
   body as the `ReferenceCount` paired above, and retail's `0x80008AD4` is its `operator new` (its
   only caller is `RsMain` at 0x80005CD0).
   Our `main.o` instantiates **no** `TOneStatic<CGameGlobalObjects>`, so **a rename alone buys
   nothing**; it needs `CGameGlobalObjects` to actually go through `TOneStatic<CGameGlobalObjects>`
   (`include/Kyoto/TOneStatic.hpp`). Worth 2 functions / 80 bytes (0x80008B04 + 0x80008B3C) and it
   is the same header `__dt__CGameGlobalObjects_80006518` wants anyway.
2. **`__dt__80006AE0` (88 B) is pinned and unreachable today.** `tools/dis.sh 0x80006AE0 0x58`
   shows its single call target is **retail's own `__dt__CGameGlobalObjects_80006518`**, and
   `who_calls.py 0x80006AE0` returns exactly one caller, `RsMain__5CMainFiPCPCc`, at 0x80006498 -
   where the preceding instructions are `addi r3,r1,20 / li r4,-1` against the same `r1+20` local
   that `single_ptr_assign_800064D0` (0x800064D0, 72 B) is handed at 0x80006488 with `li r4,0`. So
   it is `__dt__Q24rstl24single_ptr<18CGameGlobalObjects>Fv`. Our tree has no
   `rstl::single_ptr<CGameGlobalObjects>` instantiation either (`CMain` holds `gameGlobalObjects` as
   a raw `CGameGlobalObjects*`, `include/MetroidPrime/CMain.hpp:170`), so this one also needs a use,
   not a name. Same blocker as (1), and the same fix.
3. **`__dt__80006678` (88 B) is a genuine two-way tie, and objdiff cannot break it - do not flip a
   coin here.** It is a `single_ptr`-shaped deleting destructor whose one call target is
   `__dt__800066D0` (84 B). Our object has **three** 88-byte bodies of that shape -
   `__dt__Q24rstl25single_ptr<11CMemoryCard>Fv` (.text+0x1d04), `__dt__24TLockedToken` (+0x1954)
   and the already-matched `__dt__Q24rstl24single_ptr<10CGameState>Fv` (+0x1d5c) - and retail has
   three 88-byte bodies in the range. **Both remaining renames would score 100.0%**, because the
   bodies are byte-identical, so the score is a verification that cannot fail and the choice would
   be an unmeasurable guess. The one piece of context that looked like it settled it does not:
   `__dt__CGameGlobalObjects_80006518` calls it at member **+0x14C**, which
   `include/MetroidPrime/CGameGlobalObjects.hpp:107` pins as
   `rstl::single_ptr<CInGameTweakManager>`, but **our `__dt__Q24rstl33single_ptr<19CInGameTweakManager>Fv`
   is 84 bytes (0x54), not 88** - so the header offset and the body size disagree and neither body
   can be claimed on that evidence. Break the tie from `CGameGlobalObjects`' real member layout, not
   from the score. (This is `docs/PROCESS_LESSONS.md`'s "verification that cannot fail", met
   head-on.)

The other ambiguous 1:1s the relocation-aware search turned up, all left alone on purpose:
`fn_80006724` (132) is instruction-identical to our `__dt__vector<pair<string,SObjectTag>>`, but
that body is **already matched at 100%** against the *named* retail symbol at 0x80005768, so
0x80006724 is a second instantiation from another TU; `fn_80006830` (32) is identical to both
`Push__18CArchitectureQueueFRC20CArchitectureMessage` and `__sys_free`; `fn_80007AA0` (40) is
identical to `push_back__Q24rstl55list<20CArchitectureMessage,...>`, which is already paired;
`fn_80008B04` (44) is identical to `__dl__38TOneStatic<24CGameArchitectureSupport>FPv`, a name
already used at 0x80008A78. In every case the tie is between two *different* retail bodies, so
taking the rename would move a name onto bytes we cannot say are its own.

Genuinely unwritten, and the big ones: `AddPaksAndFactories` (0.21%, 1936 B), `RsMain` (0.19%,
2148 B), `CheckReset` (0.34%, 1180 B), `StreamNewGameState` (18.68%, 532 B),
`InitializeSubsystems` (12.44%, 348 B), `GetAverageValue<f>` (0%, 200 B),
`reserve__Q24rstl55vector<Q24rstl11pair<Ui,Ui>,...>Fi` (0%, 172 B), `fn_80008E94` (172),
`fn_800069AC` (308), and `__dt__CGameGlobalObjects_80006518` (264) - the last is **not** the
`{}`-body trick run 1 and this run used: it is a 15-member teardown at +0x150/+0x14C/+0x148/+0x138/
+0x134/+0x130/+0x108/+0xE4/+0x04 with an **inline** guard at +0x148, +0x138 and an inline + 0x134
mixed with out-of-line calls, so every member type has to be declared-but-not-defined in the right
order or the inline/out-of-line mix will not match. That is its own item.

## The port side, unchanged from run 1 and re-measured

The header change is unconditional (the matching build does not define `TARGET_PC`, so an
`#ifdef` would hide it from exactly the build that needs it), and `PortGlobals.cpp` is where the
PC-only definition belongs: it is not a `configure.py` unit, so a definition there cannot collide
with a retail object at DOL link time nor perturb any unit's `.text`. `CWorldLayerState` is not
polymorphic, so a declared-but-undefined destructor is only ever a call, never a vtable slot, which
is why there is no second-order effect the way `~CErrorOutputWindow` had. After the body,
`probe_sources.sh` reports **the same 250 undefined as the baseline** - no NEW, no GONE. Not on the
boot path.

---

# run 3 (lane 4, 2026-09-30) - the TOneStatic lead taken: 55 -> 60, 5 functions / 156 bytes

Run 2's lead 1 and lead 2 turned out to be **one fix, not two**, and it is bigger than either of
them said. Re-measured first, as run 2 had to: run 2's own commit (`b30d694 progress:
match-main-fn-80009274`) is on this branch, so run 2's numbers are the baseline and none of its
work needed redoing.

```
baseline  b30d694  main/MetroidPrime/main  55 / 99 functions, matched_code 7208, fuzzy 45.90%
                       tree                 10082 matched functions, 4918 linked
```

## The correction that unlocked it

Run 2 wrote, of the `TOneStatic<CGameGlobalObjects>` family, that "a rename alone buys nothing; it
needs `CGameGlobalObjects` to actually go through `TOneStatic<CGameGlobalObjects>`", and of
`__dt__80006AE0`, that "our tree has no `rstl::single_ptr<CGameGlobalObjects>` instantiation
either ... Same blocker as (1), and the same fix." Both are right, and both name the fix as
something main.cpp has to *use*. What neither of them had is that **the tree already contains the
evidence for the base class and simply never declared it**: `include/MetroidPrime/CGameGlobalObjects.hpp:16`
has included `Kyoto/TOneStatic.hpp` since the class was written, and the class used nothing from
it. It is there because retail has it:

- `CMain::RsMain` calls `0x80008AD4` (0x30 = 48 bytes) with `li r3,356` - `sizeof` of *this* class -
  and then `0x8000848C`, its constructor, with nothing allocating in between
  (`0x80005CC0`-`0x80005CE8`). `0x80008AD4` is `TOneStatic<CGameGlobalObjects>::operator new`, and
  the way that is known is that the *same 48 bytes* at `0x80008A48` are
  `TOneStatic<CGameArchitectureSupport>::operator new` (called at `0x80005E14` with `li r3,168`),
  and `include/MetroidPrime/CGameArchitectureSupport.hpp:20` already declares
  `class CGameArchitectureSupport : public TOneStatic< CGameArchitectureSupport >`.
- `~CGameGlobalObjects` (`0x80006518`) ends in a call to `0x80008B04` (0x2C = 44 bytes) at
  `0x80006600`, behind the deleting-destructor flag test at `0x800065F4` -
  `tools/who_calls.py 0x80008B04` returns that one caller and no other. 44 bytes is
  `TOneStatic<T>::operator delete`, the same 44 as `0x80008A78`, which is
  `__dl__38TOneStatic<24CGameArchitectureSupport>FPv` in `symbols.txt` and is already at 100%.

So `class CGameGlobalObjects : public TOneStatic< CGameGlobalObjects >` is a *measurement*, not a
convenient device. `TOneStatic` has no data members and no virtuals, so every offset in the layout
and `sizeof(CGameGlobalObjects) == 0x164` are unchanged - which is checkable: retail's own
`li r3,356` is the size, and the constructor at `src/MetroidPrime/main.cpp:338` that the header
comment describes did not move a byte (see the gates).

## The second, independent half: `GetAllocSpace` was in the class body

`TOneStatic<T>::GetAllocSpace` was a `static void*` defined **inside** the class, so implicitly
inline, and `include/Kyoto/TOneStatic.hpp` already had the arrangement that reproduces retail for
the other two members: `operator delete` and `ReferenceCount` are defined out of line, and their
bodies were already byte-identical to retail's. Retail has a 12-byte `GetAllocSpace` **twice**
(`0x80008AA4`, `0x80008B30` - `lis`/`addi`/`blr` returning the static storage) and calls it out of
line from `operator new`; with it inside the class body our object had no way to emit either.
Moving `operator new` (both overloads) and `GetAllocSpace` out of the class body is the whole
change to that header, and it is the same pattern the two already-correct members use.

**What I did not measure:** whether mwcceppc would have folded them in if they had stayed in the
class body. The header comment says so honestly rather than asserting it.

## The host, and the one argument I refused to invent

`TOneStatic` is a template; nothing but an allocation site brings `operator new` into a
translation unit. Retail has exactly two such sites in the whole DOL for the two `TOneStatic`
classes, and both are in `CMain::RsMain`, so that is where the uses went. They are written as
`TOneStatic<T>::operator new(sizeof(T), <file>, 0)` with an explicit `sizeof` and explicit
arguments, because an ordinary `new T` can only reach the one-argument overload and retail has no
such body in this range.

**The arguments retail passes are not a filename, and this run worked out why that matters.**
`addi r4,r4,22208` after `lis r4,-32710` is 0x802A56C0, and `readelf -S` puts `.text` at
0x80003840 + 0x3A1C54, so 0x802A56C0 is *code* (`li r4,0x1924 ; li r28,0x100`); the same value goes
to both allocation sites, so it is not a class name either. `operator new` reads only r3, so both
arguments are passed as null. Naming that value is a real open question, and it is left in the
source comment for whoever writes the rest of `RsMain`; it costs the two 48-byte bodies nothing.

`CMain::RsMain` (0x80005C6C, 2152 bytes) is **0.19% and stays that way** - it has the two
allocations and nothing else. The construction of the objects is retail's next instruction and is
**not** written: it needs a placement `operator new`, which this tree does not declare. Both
constructors are already in the file (`main.cpp:338` and `main.cpp:401`), so the pair is there when
the rest of the function is written. This replaces a body that was `{}` and returned nothing.

## What changed (four files, 5 renames in `config/`)

| file | change |
|---|---|
| `include/Kyoto/TOneStatic.hpp` | `operator new` (both) and `GetAllocSpace` moved out of the class body, with the reason |
| `include/MetroidPrime/CGameGlobalObjects.hpp:71-89` | **added** the base class `: public TOneStatic< CGameGlobalObjects >`, with the two retail measurements that pin it |
| `src/MetroidPrime/main.cpp:662-702` | `CMain::RsMain`'s two allocations + the comment |
| `config/G2ME01/symbols.txt:169,171,173,175,176` | **five** renames (below) |

```
0x80008A48  0x30 -> __nw__38TOneStatic<24CGameArchitectureSupport>FUlPCcPCc          48 B -> 100.0%  scope:weak
0x80008AA4  0x0C -> GetAllocSpace__38TOneStatic<24CGameArchitectureSupport>Fv        12 B -> 100.0%  scope:weak
0x80008AD4  0x30 -> __nw__32TOneStatic<18CGameGlobalObjects>FUlPCcPCc               48 B -> 100.0%  scope:weak
0x80008B30  0x0C -> GetAllocSpace__32TOneStatic<18CGameGlobalObjects>Fv             12 B -> 100.0%  scope:weak
0x80008B3C  0x24 -> ReferenceCount__32TOneStatic<18CGameGlobalObjects>Fv            36 B -> 100.0%  scope:weak
```

`ReferenceCount__32TOneStatic<18CGameGlobalObjects>Fv` is the fifth and was not in the plan: it
came along because `operator new` calls it, so the instantiation exists as soon as `operator new`
does. Two of the five (`GetAllocSpace` for each class) are 12-byte `lis`/`addi`/`blr` bodies and
`ReferenceCount` is the 36-byte static-local guard that was **already** matched for
`CGameArchitectureSupport` in run 2.

**These five names are not a coin flip, and the score is not the reason.** The two `__nw__` bodies
are byte-identical except for which `ReferenceCount`/`GetAllocSpace` they `bl`, so pairing them by
score alone would be `PROCESS_LESSONS.md`'s "verification that cannot fail" - run 2 said the same
thing about its tie and was right to refuse it. Three independent things pin them:
1. the retail call sites pass `li r3,356` (0x164, this class's `sizeof`) and `li r3,168` (0xA8,
   `sizeof(CGameArchitectureSupport)`);
2. retail's own `bl` graph closes the family - `0x80008AD4` calls `0x80008B3C` and `0x80008B30`,
   `0x80008A48` calls `0x80008AB0` and `0x80008AA4` - and our relocations reproduce exactly that
   (`objdump -r`: `__nw__32TOneStatic<18CGameGlobalObjects>` at `.text+0x21d4` has
   `R_PPC_REL24 ReferenceCount__32TOneStatic<18CGameGlobalObjects>Fv` and then
   `R_PPC_REL24 GetAllocSpace__32TOneStatic<18CGameGlobalObjects>Fv`);
3. the two `GetAllocSpace` bodies return **different** addresses (retail 0x8041_5AC4 and
   0x8041_5B6C; ours two distinct `sAllocSpace$` symbols), and the two `ReferenceCount` bodies use
   different SDA21 displacements (retail `-28328`/`-28324` vs `-28316`/`-28312`; ours two distinct
   `sReferenceCount$`/`init$` pairs).

## Measured, from `build/report.json`

`main/MetroidPrime/main` **55 -> 60 of 99** functions, `.text` fuzzy **45.90% -> 47.05%**,
`matched_code` 7208 -> 7364 (**+156**). Tree-wide, full per-function diff against the `b30d694`
baseline (`tools/report_diff.py .tmp/baseline-report.json build/report.json`):

```
matched 10082 -> 10087   linked 4918 -> 4918   (+5 functions at 100%, 0 units newly linked)
better 5 (all in main/MetroidPrime/main)   worse 0   gone 0
```

**No function anywhere got worse and no unit lost a match.** The 5 "gone" keys are the five
renamed `fn_` names at the same addresses, which the tool reports as `RENAMED`.

`./tools/goal_check.sh build/goal/item.json` on this tree:

```
ok  no judge-owned path touched
ok  gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
ok  counts: matched 10082 -> 10087   linked 4918 -> 4918
ok  check_symbol_names.py
ok  All:  31.04% fuzzy, 23.34% matched, 11.78% linked (10087 / 28465 functions)
flip  flip_test MetroidPrime/main.cpp: FAIL - judged below as partial progress
ok  target rose: main/MetroidPrime/main: 55 -> 60 / 99 functions
ok  no asm added
goal_check: PARTIAL match-main-fn-80009274 - flip_test ... FAIL, but the target rose
```

Gates on their own: `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`;
`tools/probe_sources.sh` = `750 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0
duplicates)` against a baseline of exactly 250 - **no NEW, no GONE** (0 lines in
`build/gate-probe.log`); `check_symbol_names.py` = `checked 504 units; 0 declared names are
missing`; `gate.sh`'s `hashes vs config.yml` ok for all 86 RELs.

`tools/unit_fit.sh MetroidPrime/main.cpp`: `.text` claimed 17608, ours 9728, **SHORT by 7880**
(run 2: 9512 / SHORT by 8096, so this run closed 216 of the 8096), and the extras list is
**unchanged at 16 functions / 1340 bytes** - no new function that retail's unit object lacks.
`.sbss` is `over by 21` where the pre-change measurement on this tree is `over by 13`; the 8 bytes
are the two new `sReferenceCount$` (4 each) and the over-run itself is inherited, measured by
stashing the three source files, rebuilding and re-running `unit_fit.sh`.

**Decl order is untouched, and structurally rather than by the script's count.** The five new
bodies are weak COMDAT template instantiations and land at `.text+0x21d4`-`0x22b8`, *after* the
last source-declared function (`__sys_free` at `0x21b4`), which is where the pre-existing
`__dl__38TOneStatic<24CGameArchitectureSupport>FPv` and its `ReferenceCount` already sat. They take
no part in the source-order permutation, so `check_decl_order.py --unit "MetroidPrime/main"` still
reports the same inherited 41 (8 shown + "33 more").

## The flip: the same four pre-existing blockers, and I did not touch them

`./tools/flip_test.sh MetroidPrime/main.cpp` -> `FAIL -> reverted (tree rebuilt: DOL
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010)`, and the linker names the same four run 1 and run 2 saw,
unchanged:

```
multiply-defined: 'CErrorOutputWindow::__vt' in CErrorOutputWindow.o
undefined: 'fn_80008C28'      undefined: 'fn_80009224'
undefined: 'rstl::rc_ptr<CMapWorldInfo>::ReleaseData()'
```

WALL: MetroidPrime/main.cpp flip - the same four pre-existing link-level blockers
(`CErrorOutputWindow::__vt` multiply-defined, `fn_80008C28` / `fn_80009224` undefined,
`rc_ptr<CMapWorldInfo>::ReleaseData()` undefined) and .text still SHORT by 7880 bytes over 34
unwritten functions, so no amount of work on any single function in this unit can flip it; treat
this unit as `progress`-shaped. (Re-measured this run, not copied: 7880, not run 2's 8096.)

## `fn_80008B04` - the sixth body, and why it is not reachable today (run 2's lead 2, closed)

`fn_80008B04` (0x2C = 44) is `__dl__32TOneStatic<18CGameGlobalObjects>FPv`, retail's
`TOneStatic<CGameGlobalObjects>::operator delete`, and after this run it is the **only** member of
that family left. It is not a rename away: nothing in `main.o` references it, because
`operator delete` is instantiated by a **`delete`**, and a destructor body alone does not make
one. Measured, the only ways to get a `delete` of a `CGameGlobalObjects` into `main.cpp`:

1. **`CMain::gameGlobalObjects` as a `rstl::single_ptr<CGameGlobalObjects>`.** Retail does this -
   `CMain::RsMain` keeps one on the frame at `r1+20` and destroys it at `0x80006490` with
   `addi r3,r1,20 / li r4,-1 / bl 0x80006AE0`, and `0x80006AE0` is
   `~rstl::single_ptr<CGameGlobalObjects>` (88 bytes, `lwz r3,0(r30) / li r4,1 / bl
   __dt__CGameGlobalObjects_80006518 / extsh. r0,r31 / ble / mr r3,r30 / bl CMemory::Free`).
   **Dead end, and it is a regression, not a cost:** `CMain::~CMain` is currently
   `__dt__5CMainFv`, 60 bytes, **100.0%**. Making the member a `single_ptr` makes `~CMain` destroy
   it, so `__dt__5CMainFv` comes off 100%. Do not do it in a `progress` item.
2. **Writing `~CGameGlobalObjects`.** It would be a real `{}` body (the 15-member teardown run 2
   described), but it is the *dtor* half; retail's `bl 0x80008B04` at `0x80006600` is behind the
   deleting flag, so the body alone never names `operator delete`. It also would not reach 100%
   (run 2 measured the inline/out-of-line mix at +0x148/+0x138/+0x134 against out-of-line calls).
3. **A local `rstl::single_ptr<CGameGlobalObjects>` in `CMain::RsMain`** - retail's own `r1+20`.
   This is what would actually work, and it was **not** done because doing it means introducing a
   frame object that holds a pointer to an object this run does not construct, and paying two more
   weak COMDATs (`~single_ptr<CGameGlobalObjects>` and its `operator=`) into the extras list for
   **one** 44-byte function. That is a contrived host for a single symbol, which is the shape
   `docs/goal-review-prompt.md` rejects. Left for a run that is writing the rest of `RsMain` and
   can justify it with the construction that goes with it.

## The port side

The base class is unconditional, so the PC link sees it too, and that is the one behavioural
consequence: `src/MetroidPrime/PortBoot.cpp:247`'s `new CGameGlobalObjects(*osContext, *memorySys)`
now goes through `TOneStatic<CGameGlobalObjects>::operator new` and returns
`GetAllocSpace()`'s `static uchar sAllocSpace[sizeof(CGameGlobalObjects)]` instead of the heap.
`sizeof` there is the host's, so the buffer is the right size, there is exactly one
`CGameGlobalObjects` in the port, nothing `delete`s one (`grep -rn "delete .*gameGlobalObjects"
src/` = 0 hits), and the allocation is at the same point in the same boot step. Measured
consequence: `tools/probe_sources.sh` reports **the same 250 undefined as the baseline** - no NEW,
no GONE - because all four `TOneStatic` members are header-defined template members and so are
weak COMDATs in every object that needs them. `TOneStatic<CCubeRenderer>` is unaffected:
`src/MetaRender/PortCCubeRenderer.cpp:168` is a placement `new`, so it never selects
`operator new`. `CMain::ShutdownSubsystems` / `~CGameArchitectureSupport` are untouched, and
`__dt__24CGameArchitectureSupportFv` stays at 100.0%.

## What a next run on this unit should know

- **The `TOneStatic` family is now 6 of 7 taken.** Only `fn_80008B04` (44 B) is left and the three
  routes to it are enumerated above with their costs; #1 is a regression, #3 is a contrived host.
  34 functions remain unmatched in the unit and `.text` is SHORT by 7880, so the unit is
  `progress`-shaped and should be requeued as such rather than as a `match`.
- **Run 2's leads 1 and 2 are both closed** (5 functions between them, not the 4 lead 1 predicted
  and not lead 2's `__dt__80006AE0`, which is a `single_ptr` body this tree cannot reproduce -
  retail's is 88 bytes and also frees `this` with `CMemory::Free`, while
  `rstl/single_ptr.hpp`'s out-of-line `~single_ptr()` is `delete mPtr` and is 84 bytes; see #1
  above for the same measurement from the other side).
- **Run 2's lead 3 (`__dt__80006678`, the 88-byte tie) is untouched and still a tie.** Its
  evidence is unchanged and its own advice stands: do not flip a coin, break it from
  `CGameGlobalObjects`'s real member layout. Note that this run's base-class finding is *not* that
  evidence - it says nothing about which of the three identical 88-byte `single_ptr`-shaped bodies
  is which.
- **The generalisable trick, stated so the next run can reuse it:** a *byte-identical* family of
  template bodies is only ambiguous if you identify it by bytes. Identifying it by
  **relocation targets** (`objdump -r` on our own object) and by **the constants the two call
  sites pass** turns it into a 1:1 map, and the two are what made these five renames measurable
  rather than lucky. The two `__nw__` bodies here differ in exactly the two symbols they call.
- `docs/HANDOFF.md` appears in this run's `git diff` with only its state block re-derived. That is
  `tools/check_docs_claims.py`, run as a step of `gate.sh`, rewriting the counts it checks - not an
  edit of mine, and the driver discards it.

---

# run 4 (lane 4, 2026-09-30) - the two `{}` stubs at 0x80007040: 60 -> 61, 1 function / 100 bytes

Run 3's commit (`cf0bc6f`) is on this branch, so run 3's numbers are the baseline and none of its
work needed redoing. Re-measured first, as run 2 and run 3 both had to:

```
baseline  cf0bc6f  main/MetroidPrime/main  60 / 99 functions, matched_code 7364, fuzzy 47.053387%
                        tree                 10092 matched functions, 4918 linked
```

The `TOneStatic` lead run 3 closed is still closed. What follows is a different pair of functions,
found by listing every unmatched function in the unit and asking which of them the object already
contains: **only 10 of the unit's 39 unmatched functions are in `main.o` at all**, and of those the
two at 0x80007040 / 0x800070A4 were `extern "C" void f() {}` - a one-instruction `blr` - sitting at
**4.00%** and **5.00%**. They are the cheapest thing in the unit by a wide margin, and unlike the
`{}`-destructor trick runs 1-3 used they are not already-known classes: they had to be identified
first. That took twenty minutes and it is the reusable part.

## What `fn_80007040` / `fn_800070A4` are

Retail 0x80007040, 0x64 = 100 bytes, and 0x800070A4, 0x50 = 80 bytes. Both unnamed in
`symbols.txt:140-141`, both `extern "C"` stubs in this unit. Together they build `CGameState`'s
`+0x1A0` block, which `include/MetroidPrime/Player/CGameStateBlocks.hpp:103-111` **already
documents** - so the identification cost nothing invented:

```
80007040  stwu r1,-32(r1) / mflr r0 / li r4,4 / stw r0,36(r1) / li r0,0 / addi r5,r1,8
          stw r31,28(r1) / mr r31,r3
          stw r0,0(r3)      <- +0x00        addi r3,r31,16
          stb r0,4(r31)     <- +0x04        stw r0,8(r31)   <- +0x08
          stw r0,12(r31)    <- +0x0C        stw r0,8(r1) / 12 / 16(r1) / stb 20 / 21(r1)
          bl 800070A4       <- (this+0x10, 4, &temp)   then return-this
800070a4  stw r4,0(r3) / addi r10,r3,4 / lwz r9,0(r5) / lwz r8,4(r5) / lwz r7,8(r5)
          lbz r6,12(r5) / lbz r0,13(r5) / mtctr r4 / cmpwi r4,0 / blelr
loop:     cmplwi r10,0 / beq skip / stw r9,0(r10) / stw r8,4 / stw r7,8 / stb r6,12 / stb r0,13
skip:     addi r10,r10,16 / bdnz loop / blr
```

Pinned three ways, all from the tree:

* `who_calls.py 0x80007040` returns `CMain::CheckReset` (r1+104, right after
  `__ct__CGameOptionsFv` on the same slot) and both `CGameState` constructors, the latter as
  `addi r3,r29,416` - i.e. `CGameState + 0x1A0` (`CGameState.hpp:305`, `int mGameModeType`).
* `CGameStateBlocks.hpp:84-97` already measured that block: 0x54 bytes, `+0x00` read by
  `CMainFlow::AdvanceGameState`, four 14-byte records at stride 16 from `+0x14`, next member at
  `+0x1F4`. The writes above are exactly that and no more.
* `__ct__10CGameStateFR16CBitStreamReader` (0x801442C0) and `CMain::CheckReset` (0x80006D68) are
  the only two callers, and both call it **as a constructor that returns `this`** - `fn_80144924`
  (0x80144924, `SGameStateSlots`'s constructor) has the same `mr r3,r31` before its restores, which
  is why `fn_80007040` returns `SGameStateWorlds*` here and not `void`.

`SGameStateBlocks.hpp:109` names the record array `u8 x14_rec[4][16]` and calls it a **view** onto
`CGameState`'s own `char x1a4_[0x50]`, so the two functions are written against their own
`SGameStateRecord { u32,u32,u32,u8,u8 }` / `SGameStateRecords { u32 count; SGameStateRecord[4]; }`
rather than through the view. That is the whole change; nothing else in the tree moved.

## What this run changed (one source file)

| file | change |
|---|---|
| `src/MetroidPrime/main.cpp:43` | `#include "MetroidPrime/Player/CGameStateBlocks.hpp"` (for `SGameStateWorlds`) |
| `src/MetroidPrime/main.cpp:640-710` | `SGameStateRecord` + `SGameStateRecords`, real bodies for `fn_800070A4` and `fn_80007040`, with the measurements above as comments |

**Three spellings of `fn_800070A4` are load-bearing and all three were measured** - this is the
part worth carrying forward:

1. **The records are inline at `+0x04`, not behind a pointer.** `cmplwi r10,0 / beq` tests the
   *cursor*, which starts at `addi r10,r3,4`; a pointer member gives `lwz r0,4(r3)` inside the
   loop instead and unrolls the copy loop to **336 bytes**.
2. **The `if (rec)` has to be written out.** Without it mwcceppc emits one straight unrolled copy
   loop, also 336 bytes, with no `cmplwi` at all.
3. **The cursor must be a variable incremented in the body** (`++rec`), not `self->x04_recs[i]`:
   the indexed form is 19 instructions instead of 20 and computes the cursor with `addic. r4,r3,4`
   *inside* the loop, where retail hoists `addi r10,r3,4` above the source loads.

`fn_80007040` needed two more: the four `self->` zero stores must precede the temporary's five, or
mwcceppc emits them the other way round; and the temporary must be built by five explicit member
stores, because `SGameStateRecord value = {0,0,0,0,0}` is hoisted into `.rodata` and copied
(`lis r4,0 / addi r6,r4,0 / lwz ...`) instead of being zeroed on the stack.

## Measured, from `build/report.json`

`main/MetroidPrime/main` **60 -> 61 of 99** functions, `.text` fuzzy **47.053387% -> 47.966606%**,
`matched_code` 7364 -> 7464 (**+100**). Tree-wide, full per-function diff against the `cf0bc6f`
baseline (every `(unit, function)` pair in `build/report.json`):

```
better 2 (fn_80007040 4.00->100.0, fn_800070A4 5.00->86.0)   worse 0   new 0   gone 0
matched 10092 -> 10093   linked 4918 -> 4918
```

**No function anywhere got worse and no unit lost a match.**

`./tools/goal_check.sh build/goal/item.json` on this tree:

```
ok  no judge-owned path touched
ok  gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
ok  counts: matched 10092 -> 10093   linked 4918 -> 4918
ok  check_symbol_names.py
ok  All:  31.07% fuzzy, 23.36% matched, 11.78% linked (10093 / 28465 functions)
flip  flip_test MetroidPrime/main.cpp: FAIL - judged below as partial progress
ok  target rose: main/MetroidPrime/main: 60 -> 61 / 99 functions
ok  no asm added
goal_check: PARTIAL match-main-fn-80009274 - flip_test ... FAIL, but the target rose
```

Gates on their own: `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`;
`check_symbol_names.py` = `checked 504 units; 0 declared names are missing`; `gate.sh`'s
`hashes vs config.yml` ok for all 86 RELs; the port probe inside `gate.sh` is unchanged at the
baseline's 250 undefined. `tools/unit_fit.sh MetroidPrime/main.cpp`: `.text` claimed 17608, ours
**9900**, **SHORT by 7708** (run 3: 9728 / SHORT by 7880, so this run closed 172 of the 7880); the
extras list is **unchanged at 16 functions / 1340 bytes**, and `.sbss` is `over by 21`, which run 3
already measured as inherited. `check_decl_order.py --unit "MetroidPrime/main"` reports the same
inherited 41 permuted functions (8 shown + "33 more"); the two functions stay in the descending-by-
retail-offset order their addresses call for (`fn_800070A4` 0x800070A4 before `fn_80007040`
0x80007040), so this run adds **nothing** to the permutation.

## The flip: the same four pre-existing blockers, and I did not touch them

`./tools/flip_test.sh MetroidPrime/main.cpp` -> `FAIL -> reverted (tree rebuilt: DOL
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010)`, and mwldeppc names the same four runs 1-3 saw, unchanged:

```
multiply-defined: 'CErrorOutputWindow::__vt' in CErrorOutputWindow.o
undefined: 'fn_80008C28'      undefined: 'fn_80009224'
undefined: 'rstl::rc_ptr<CMapWorldInfo>::ReleaseData()'
```

WALL: MetroidPrime/main.cpp flip - the same four pre-existing link-level blockers
(`CErrorOutputWindow::__vt` multiply-defined, `fn_80008C28` / `fn_80009224` undefined,
`rc_ptr<CMapWorldInfo>::ReleaseData()` undefined) and .text still SHORT by 7708 bytes over 38
unwritten functions, so no amount of work on any single function in this unit can flip it; treat
this unit as `progress`-shaped and requeue it as such. (Re-measured this run: 7708, not run 3's 7880.)

## `fn_800070A4` - 86%, and what is left on it (NEW item)

This one is worth a run of its own, because **the only thing separating it from 100% is the
register allocation of the copy loop**, and the correct set of registers is already reproducible:

```
retail   addi r10,r3,4 / stw r4,0(r3) / lwz r9,0(r5) / r8 / r7 / lbz r6,12(r5) / r0,13(r5)
         mtctr r4 / cmpwi r4,0 / blelr / cmplwi r10,0 / beq / stw r9,0(r10) ... / addi r10,r10,16 / bdnz
ours     addi r9,r3,4  / stw r4,0(r3) / lwz r8,0(r5) / r7 / r6 / lbz r3,12(r5) / r0,13(r5)
         mtctr r4 / cmpwi r4,0 / blelr / cmplwi r9,0 / beq / stw r8,0(r9) ... / addi r9,r9,16 / bdnz
```

Identical instruction for instruction, identical mnemonics, identical offsets - every register is
one lower than retail's, and the 4th field lands in `r3` where retail keeps `r6`. MWCC allocates
these six volatiles strictly downward from the first free one; retail got `r10` for the cursor and
ours gets `r9`, so **one more volatile is live at the allocation point in retail and not in ours**.

**Twenty spellings were tried and measured this run**; two of them are the useful result:

| spelling | result |
|---|---|
| pointer member `SGameStateRecord* x04_recs` + `x04_recs[i]` | 432 bytes - word reloaded in the loop |
| inline array, no `if` | 336 bytes - one unrolled copy loop, no `cmplwi` |
| inline array + `if (self->x04_recs)` indexing | 80 bytes, `cmplwi` right, cursor recomputed in the loop |
| **inline array + explicit `rec` cursor + `if (rec)` + `*rec = value`** | **80 bytes, one instruction of difference in shape per store - 86.0%, cursor in `r9`** |
| `rec = &self->x04_recs[i + 1]` at the end of the body | **cursor in `r10` and fields in `r9/r8/r7/r6` - retail's exact register set - but 100 bytes**, because the cursor is recomputed from `self` each iteration and a `li r11,0` appears |
| `for (int i = 0; i < self->x00_count; ++i)` | cursor `r10`, fields `r9/r8/r7/r6`, but a `cmpw/blt` loop and the 5th field in `r4` |
| `while` / `do-while` / down-count / `!= n` / `register` / `char*` cursor / two cursors / field-by-field / struct copy / five hoisted locals / `&x04_recs[0]` / `SGameStateRecord v = value` / index-only cursor | all 80 or 76 or 100/108/312/432 bytes, none 80-and-identical |

**So the next thing to try is the one spelling that keeps retail's register set**: the cursor must
be hoisted *and* stay loop-carried, which needs one extra live volatile that costs no instruction.
`rec = &self->x04_recs[i + 1]` proves the extra live value (`self` across the loop) is what pushes
MWCC to `r10`; a source that keeps `self` live in the loop without recomputing the cursor is what is
missing. That is a two-line experiment, not an investigation.

NEW: match-main-fn-800070a4 | match | MetroidPrime/main | fn_800070A4 is 86% and its instruction
sequence already matches retail's exactly - only the six copy-loop volatiles are one register lower,
and a spelling that keeps retail's register set (`rec = &self->x04_recs[i+1]` already produces it)
without recomputing the cursor would close it

## `CMain::SetMaxSpeed` - 99.25%, thirteen spellings, only the epilogue's order

`SetMaxSpeed__5CMainFb` (0x800089BC, 96 bytes) was 0.00% when run 1 listed it and is **99.25%
today**, and this run did not move it: its body is byte-for-byte retail's except for the **three
epilogue instructions**, and the difference is their *order*.

```
retail  stb r0,144(r30) / lwz r0,20(r1) / lwz r31,12(r1) / lwz r30,8(r1) / mtlr r0
ours    stb r0,144(r30) / lwz r31,12(r1) / lwz r30,8(r1) / lwz r0,20(r1) / mtlr r0
```

Thirteen spellings of the same body were compiled and measured this run, and **none** flips it:
`if (v) { if (!screenFading) ... }`; `const bool s = v` used in both places; `this->x5c` /
`this->screenFading`; `v != false && screenFading == false`; `!(v == false) && (screenFading ==
false)`; `screenFading = !!v`; `screenFading = v == true`; an empty `else` branch; `goto tail`; the
two assignments as one comma expression; an explicit `return;`; a `register`-qualified cursor
equivalent; `bool& sf = screenFading`. The guard polarity, the nested-`if` shape and the `this`
spelling are all load-bearing for nothing here - the body is unchanged in every case.

**The trigger is measured and it is the tail `stb`**: deleting `screenFading = v;` (keeping the
`lfs/stfs`) gives retail's `lwz r0` **first**, keeping it gives `lwz r0` **last** - and a body that
reaches the same place through an explicit byte RMW (`uchar b = *(uchar*)this + 0x90; b = ...;
*(uchar*)this + 0x90 = b;`) also flips to `lwz r0` first, but with a completely different
instruction sequence, so it is not usable. The shape is **not** unreachable: four 100% functions in
`main.o` end in a `stw r0,...` and restore `r0` first
(`do_insert_before<...ArchitectureMessage...>`, `create_node<...>`, `LoadStringTable`,
`__dl__TOneStatic<CGameArchitectureSupport>`). So the next run should look for a *source* reason,
not a compiler limitation - the one input not yet varied is **which register the tail's value lives
in**, and that is fixed by retail's own `rlwimi r0,r31,5,26,26`.

WALL: CMain::SetMaxSpeed 99.25% - the body is byte-identical to retail and only the epilogue's three
restores differ in order (`lwz r0` last here, first in retail); thirteen spellings this run all leave
`lwz r31 / lwz r30 / lwz r0`, and the trigger is the tail `stb r0,144(r30)` itself

## Leads a next run should have

- **`fn_800070A4` at 86%** - see the NEW item above. 80 bytes, one experiment.
- **Run 2's lead 3 (`__dt__80006678`, the 88-byte tie) is still untouched and still a tie.** Its
  evidence is unchanged; break it from `CGameGlobalObjects`'s real member layout, not from the score.
  Runs 1, 2 and 3 all declined it for the same reason and this run did not touch it.
- **`fn_80009224` is one of the four link blockers and a `symbols.txt` rename would remove it** -
  it is `rstl::rc_ptr<CWorldLayerState>::ReleaseData`, and run 1 noted that our tree emits that
  instantiation as a weak COMDAT in `CGameState.o` (`0x5e68`) instead. Renaming the retail symbol
  to `ReleaseData__Q24rstl28rc_ptr<16CWorldLayerState>Fv` would resolve every reference to it.
  It would **not** add a matched function (the body is not in `main.o`, so objdiff pairs nothing),
  so it is a link fix, not a count - which is why it is here and not a NEW item. Nobody has tried it.
- **The remaining 29 unmatched functions that are not in `main.o` at all** are the real cost here:
  `AddPaksAndFactories` (1936 B), `RsMain` (2148 B), `CheckReset` (1180 B),
  `__ct__CGameArchitectureSupport` (888 B), `StreamNewGameState` (532 B, 152 of 532 bytes written),
  `AddWorldPaks` (384 B, 96.00%), `InitializeSubsystems` (348 B, a `// TODO`), `__dt__CMemoryInStream`,
  `fn_80008C28`, `fn_80008E94`, `reserve<vector<pair<Ui,Ui>>>::reserve`, `GetAverageValue<f>`, and the
  `CGameGlobalObjects` destructor cluster (`fn_800067A8` / `fn_800067E0` / `fn_80006830` /
  `fn_80006850` / `fn_80006874` / `fn_800068F4` / `fn_80006954` / `__dt__80006678` / `__dt__800066D0` /
  `__dt__80006AE0`), which this run read and did not touch - it is run 2's 15-member teardown and is
  its own item.
- **The generalisable trick from this run, for the next `{}` stub in a decomp unit:** a stub at 5%
  usually is not "a hard function", it is a small POD-shaped helper whose *record* has to be
  recovered from the callee's field accesses first. Two measurements recovered it here in twenty
  minutes - `who_calls.py` to find the owning member (`CGameState + 0x1A0`), and the header comment
  that had already measured the block (`CGameStateBlocks.hpp:84-97`). Then `tools/bytescmp.py` on a
  one-instruction stub is not informative (everything is "different"), so read
  `powerpc-eabi-objdump -d` on both sides and compare field offsets, not instructions.
  `tools/probe_cc.sh` does **not** work for `src/MetroidPrime/main.cpp` - it lacks
  `-i extern/musyx/include` and the four `MUSY_*` defines, so the probe compile fails on
  `musyx/musyx.h`; copy the `cflags` line out of `build.ninja` instead (a lane-local
  `.tmp/opencode/probesrc.sh` did it here, ~0.8 s per compile, which is what made 40 experiments
  affordable).

## The port side

No change. `src/MetroidPrime/main.cpp` is not in `files.cmake`, so neither function reaches the host
build, and the two `extern "C"` symbols are not what the port calls - `src/MetroidPrime/Player/
CGameStateCtor.cpp:273` and `CGameStateStreamCtor.cpp:430` declare `void fn_80007040(SGameStateWorlds*
self)` with **C++** linkage, which is a different symbol from the `extern "C"` one here, and that is
pre-existing. The port probe inside `gate.sh` is unchanged at the baseline's 250 undefined, no NEW
and no GONE. Neither function is on the boot path.

---

# run 5 (lane 4, 2026-09-30) - `CTweakGame::GetPakFile` returns by value: `AddWorldPaks` to 100%,
# 61 -> 62, and `AsyncIdle` 85.94% -> 99.17%

Run 4's commit (`ffedecc`) is on this branch, so run 4's numbers are the baseline and none of its
work needed redoing. Re-measured first, as runs 2 and 3 had to:

```
baseline  ffedecc  main/MetroidPrime/main  61 / 99 functions, matched_code 7464, fuzzy 47.966606%
                        tree                 10093 matched functions, 4918 linked
```

Run 4's lead list had nothing in it that reached 100%, and its two `{}`-stub / destructor leads
are closed. The one unfinished thing any earlier run wrote down is in `src/MetroidPrime/main.cpp`'s
own comment on `CMain::AddWorldPaks`, item 2 of three: **`CTweakGame::GetPakFile` must return by
value, not by `const rstl::string&`**. The run that found it deferred it. Nobody has tried it.

## The change (two files, 23 insertions, 9 deletions of code)

| file | change |
|---|---|
| `include/MetroidPrime/Tweaks/CTweakGame.hpp:13` | `const rstl::string& GetPakFile()` -> `rstl::string GetPakFile()`, with the measurement |
| `src/MetroidPrime/main.cpp:791-807` | the two `CMain::AsyncIdle` statement decompositions, with their scores |
| `src/MetroidPrime/main.cpp:773-805` | the comment for both |

No source body changed, no signature changed, no `config/` edit, no new file.

## `AddWorldPaks`: 96.00% -> **100.00%**, and why the return type is the whole of it

The function's *entire* loop body was already instruction-for-instruction retail's; the only
difference was in the **prologue**, four extra instructions and a 16-byte-larger frame
(0xA0 against our 0x90). Side by side (`objdump -d` on `main.elf` and on our `main.o`):

```
retail  stwu r1,-160(r1) / mflr r0 / stw r0,164(r1) / addi r3,r1,92   <- sret slot, hoisted
        stw r31,156 / r30,152 / r29,148 / r28,144
        lwz r4,-28240(r13)          <- gpTweakGame
        bl   GetPakFile__10CTweakGameFv
        addi r3,r1,124 / addi r4,r1,92 / bl __ct__string               <- basePath = the temp
        addi r3,r1,92               / bl   internal_dereference        <- destroy the temp
ours    stwu r1,-144(r1) / mflr r0 / stw r0,148(r1) / stw r31,140 / r30,136 / r29,132 / r28,128
        lwz r3,0(0) / bl GetPakFile / mr r4,r3
        addi r3,r1,108 / bl __ct__string                              <- basePath = the reference
```

**`addi r3,r1,92` before the call, with `r4` holding `this`, is the sret convention**: r3 is the
caller's return slot and r4 is `this`, so retail's `GetPakFile` returns `rstl::string` **by
value**. The callee's own body confirms it independently - 0x80216D5C is
`stwu / mflr / lwz r4,0(r4) / stw / addi r4,r4,16 / b __ct__string / lwz / mtlr / addi / blr`,
which never reads r3 and copy-constructs straight into the slot it was handed. Under a
`const rstl::string&` return there is no temporary, so the copy, the `internal_dereference` and
the extra frame slot all disappear - which is the entire 4% and the whole frame-size difference.

**The risk the earlier note cited does not exist on this tree, and that is the measurement:**
`grep -rn "GetPakFile" src/ include/` finds the declaration, three call sites
(`main.cpp:854`, and `mainHead.cpp:317` / `CMainAsyncIdle.cpp:217`, neither of which is a
`configure.py` unit) and one `extern "C"` reach stub in `PortReachStubs.cpp:604`. **No unit in
the tree defines the function**, so a return-type change cannot move a definition anywhere, and
`main.cpp:854` is the only call in any unit. The return type is not mangled, so the symbol
`_ZN10CTweakGame10GetPakFileEv` - the one the reach stub declares - is unchanged.
`CHECK_SIZEOF(CTweakGame, 0x4)` is untouched because no data member is involved.

So the note's "it wants its own item, not a rider on this one" was costing a lane an hour over a
risk that was not there. **Measure the blast radius of a header edit before deferring it.**

## `AsyncIdle`: 85.94% -> **99.17%**, and the one instruction that is not reachable

Two decompositions, both previously measured by a run that worked in a *split* unit
(`src/MetroidPrime/CMainAsyncIdle.cpp`, which is in the tree but in neither `configure.py` nor
`files.cmake` - a staged split, not applied). **Their claim reproduces in the un-split unit**,
which is the useful part: the split is not needed for this gain.

1. The clamp goes into its own variable whose `5000` arm is the fall-through -
   `uint t = 5000; if (time <= 5000) { t = time; }`. Retail 0x80005BF0 is
   `cmplwi r4,5000 / li r31,5000 / bgt / mr r31,r4`.
2. The flag is initialised *before* the test: `bool flag = false;` with `flag = true` inside the
   `if`, which is what reproduces retail's `li r30,0` / `li r30,1` and the `stw r30,8(r1)` spill.

That is the whole body: after these two, ours and retail are **instruction-for-instruction
identical** across 288 bytes except one argument setup, `mr r5,r30` against `clrlwi r5,r30,24`.

**Seventeen spellings of the flag measured this run** (each a separate compile, ~1.4 s each):

```
bool (retail's and ours)                    99.17      uchar local                  94.79
char local                                  94.79      uint local                   96.18
int local                                   96.18      (uchar)flag                  94.79
(char)flag                                  94.79      (uchar)flag ? true : false    94.79
(bool)(unsigned char)flag                   94.79      (bool)(flag & 0xFF)          94.79
!!(unsigned char)flag                       94.79      flag | 0                     94.79
flag != 0                                   94.79      flag = fn_80008A1C() != 0     87.68
uchar flag = (uchar)fn_80008A1C()           89.49
```

Every narrowing spelling collapses onto **exactly two** scores, 94.79 and 96.18, which is the
signature of mwcceppc emitting the `clrlwi` **and** the `neg/or/srwi` normalisation. Retail has
the `clrlwi` and no normalisation, so retail's argument is a one-byte value the compiler already
knows is 0 or 1.

**The one spelling that reproduces the byte exactly is the parameter type, and it is a
regression - measured, not assumed.** Declaring `CResFactory::AsyncIdle`'s second parameter
`unsigned char` gives **100.00%**, exactly as the earlier notes predicted. It also renames the
callee to `AsyncIdle__11CResFactoryFUiUc`, and the cost is:

```
                                   35/36 fns, matched_code 5532   ->  34/36 fns, matched_code 5408
  main/Kyoto/CResFactory::AsyncIdle__11CResFactoryFUib  268 B at 100.0%  ->  unpaired
  main/MetroidPrime/main                                  62/99, 7848  ->  63/99, 8136
```

`main/Kyoto/CResFactory` is `NonMatching`, so its own body pairs by **name** against
symbols.txt, which calls the symbol `AsyncIdle__11CResFactoryFUib`; renaming the parameter
unpairs a 268-byte function that is currently at 100.0% to gain a 288-byte one, and leaves
`FUiUc` undefined at DOL link. Net -1 function and -124 bytes of `matched_code`, so it was
measured and reverted. Retail's own mangling says the parameter is `bool`, so the `clrlwi` is
mwcceppc narrowing an argument it has already proved is 0 or 1.

WALL: CMain::AsyncIdle 99.17% - the body is instruction-for-instruction retail's and the only
difference is `clrlwi r5,r30,24` against `mr r5,r30` for the `bool` argument; seventeen
argument/local-type spellings this run all add mwcceppc's normalisation, and the one spelling
that is byte-exact (the callee's parameter as `unsigned char`) unpairs a 268-byte function that
is at 100.0% in `main/Kyoto/CResFactory` and is not worth 288 bytes

## Measured, from `build/report.json`

`main/MetroidPrime/main` **61 -> 62 of 99** functions, `.text` fuzzy **47.966606% -> 48.270103%**,
`matched_code` 7464 -> 7848 (**+384**, all of it `AddWorldPaks`; `AsyncIdle` is not 100% so it
contributes none). Tree-wide, full per-function diff against the `ffedecc` baseline
(`tools/report_diff.py .tmp/opencode/baseline-report.json build/report.json`):

```
matched 10093 -> 10094   linked 4918 -> 4918   (+1 functions at 100%, 0 units newly linked)
  +100%    main/MetroidPrime/main :: AddWorldPaks__5CMainFv
no regression
```

**No function anywhere got worse and no unit lost a match.**

`./tools/goal_check.sh build/goal/item.json` on this tree:

```
ok  no judge-owned path touched
ok  gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
ok  counts: matched 10093 -> 10094   linked 4918 -> 4918
ok  check_symbol_names.py
ok  All:  31.07% fuzzy, 23.37% matched, 11.78% linked (10094 / 28465 functions)
flip  flip_test MetroidPrime/main.cpp: FAIL - judged below as partial progress
ok  target rose: main/MetroidPrime/main: 61 -> 62 / 99 functions
ok  no asm added
goal_check: PARTIAL match-main-fn-80009274 - flip_test MetroidPrime/main.cpp: FAIL, but the target rose; commit it and keep the item
```

Gates on their own: `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`;
`tools/probe_sources.sh` = `750 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0
duplicates)` against a baseline of exactly 250 - **no NEW, no GONE**; `check_symbol_names.py` =
`checked 504 units; 0 declared names are missing`; `gate.sh`'s hash check ok for all 86 RELs.
`tools/unit_fit.sh MetroidPrime/main.cpp`: `.text` claimed 17608, ours **9920**, **SHORT by
7688** (run 4: 9900 / SHORT by 7708, so this run closed 20 of the 7708), the extras list
unchanged, `.sbss` over by 21 (run 3 measured that as inherited). `check_decl_order.py --unit
"MetroidPrime/main" --list` = the same inherited **41** permuted functions; both functions stay
in the descending-by-retail-offset order their addresses call for, so this run adds **nothing**
to the permutation. `docs/HANDOFF.md` appears in this run's `git diff` with only its state block
re-derived - that is `tools/check_docs_claims.py`, run as a step of `gate.sh`, and the driver
discards it.

## The flip: the same four pre-existing blockers, and I did not touch them

`./tools/flip_test.sh MetroidPrime/main.cpp` -> `FAIL -> reverted (tree rebuilt: DOL
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010)`, and mwldeppc names the same four runs 1-4 saw:

```
multiply-defined: 'CErrorOutputWindow::__vt' in CErrorOutputWindow.o
undefined: 'fn_80008C28'      undefined: 'fn_80009224'
undefined: 'rstl::rc_ptr<CMapWorldInfo>::ReleaseData()'
```

WALL: MetroidPrime/main.cpp flip - the same four pre-existing link-level blockers
(`CErrorOutputWindow::__vt` multiply-defined, `fn_80008C28` / `fn_80009224` undefined,
`rc_ptr<CMapWorldInfo>::ReleaseData()` undefined) and .text still SHORT by 7688 bytes over 37
unwritten functions, so no amount of work on any single function in this unit can flip it; treat
this unit as `progress`-shaped and requeue it as such. (Re-measured this run: 7688, not run 4's
7708.)

## Run 2's rename lead is exhausted, and two of its ties are resolved - one of them against

Run 2's method (a relocation-aware instruction-shape comparison of `main.elf` against our
`main.o`) was re-run on this tree, both strictly (mnemonic **and** the operands of load/store/
arith, branch targets tokenised, equal instruction count) and mnemonic-only. **There is no sound
rename left in this unit.** Two things the next run should not redo:

- **The scan's bounds are 0x800053B8..0x80009880, not 0x80005C64.** The report's per-function
  `address` field is an **offset** and the section's `metadata.virtual_address` is 0x800053B8;
  0x80005C64 is where `CMainAsyncIdle.cpp`'s *staged* split would cut, and main.cpp still claims
  the whole range. Getting this wrong silently drops the low third of the unit from the scan.
- **`__dt__800066D0` (0x800066D0, 84 B) is a FALSE 1:1 with our `__dt__18CArchitectureQueueFv`**
  and must not be renamed. It is the only new hit the strict scan produces, and it is a
  coincidence: 84 bytes with an identical instruction shape. `tools/who_calls.py 0x800066D0`
  returns exactly one caller, `__dt__80006678` (0x80006678, 88 B), which is itself called only
  from `__dt__CGameGlobalObjects_80006518` at **+0x14C** with `addi r3,r30,332 / li r4,1` - a
  `single_ptr` deleting destructor (`lwz r3,0(r30) / li r4,1 / b <pointee dtor> / extsh. r0,r31 /
  ble / mr r3,r30 / b CMemory::Free`). So 0x800066D0 is the **pointee's** destructor, not a
  queue's, and our tree has no `__dt__19CInGameTweakManagerFv` to pair it with. Renaming it
  would score 100.0% and mean nothing: this is `docs/PROCESS_LESSONS.md`'s "verification that
  cannot fail" reached by the opposite road - a 1:1 shape match that is not a 1:1 identity.
  **The call graph is what makes a shape match an identification.**
- **`fn_80007AA0` (40 B) is not shape-identical to our `push_back<list<CArchitectureMessage>>`**,
  even mnemonic-only, so run 2's ambiguity note for it is stale: there is no tie to decline. It
  is simply an unwritten 40-byte body. (The other run-2 ties - `fn_80006724` already paired
  against a named symbol at 0x80005768, `fn_80006830`, `fn_80008B04` - stand as written.)

Run 2's lead 3 (`__dt__80006678`, the 88-byte tie) is **still a tie and was not touched**, but
this run's `who_calls` output above is the missing half of its evidence: 0x80006678 is called at
+0x14C, and `CGameGlobalObjects.hpp:107` says +0x14C is `rstl::single_ptr<CInGameTweakManager>`
whose destructor in our tree is 84 bytes against retail's 88. The disagreement is now pinned to
one member, and it is *not* a shape question: compare `rstl/single_ptr.hpp`'s out-of-line
`~single_ptr()` (84 B, `delete mPtr`) against retail's 88-byte body, which also frees `this`
with `CMemory::Free`. Breaking that tie means writing a `single_ptr` that matches retail's, not
renaming a symbol.

## The port side

`include/MetroidPrime/Tweaks/CTweakGame.hpp` is shared, and the return-type change is
unconditional (the matching build does not define `TARGET_PC`, so an `#ifdef` would hide it from
exactly the build that needs it). Consequences, measured:

- **Nothing in the port calls `CMain::AddWorldPaks`** (`grep -rn AddWorldPaks src/` returns only
  `mainHead.cpp:316`'s definition and comments in `PortBoot.cpp`, `PortGlobals.cpp`,
  `CMainInitializeSubsystems.cpp`, `mainTail.cpp`, `DolphinCDvdFile.cpp`). The one port copy,
  `mainHead.cpp:317`, is `rstl::string basePath = gpTweakGame->GetPakFile();` and still compiles.
- `CTweakGame::GetPakFile` is a **reach stub** in the port (`PortReachStubs.cpp:604`), so there
  is no body whose behaviour could change: a by-value return would copy from the stub's garbage
  instead of binding a reference to it, and since nothing calls `AddWorldPaks` there is nothing to
  copy. `_ZN10CTweakGame10GetPakFileEv` is unmangled by the return type, so the stub still
  satisfies the reference.
- `tools/probe_sources.sh` reports **the same 250 undefined as the baseline** - no NEW, no GONE.
- `main.cpp` is not in `files.cmake`, so the `AsyncIdle` decompositions do not reach the host
  build at all; `mainHead.cpp:283` carries the port's own copy and was not touched.

## What a next run on this unit should know

- **The remaining 37 unmatched functions, and the cheapest-looking are not the small ones.**
  28 functions in this unit are in `main.o` at all; the rest are unwritten bodies. The closest to
  100% is now `SetMaxSpeed` (99.25%, WALL'd by run 4 with thirteen spellings) and `AsyncIdle`
  (99.17%, WALL'd above with seventeen). The genuinely unwritten and largest are
  `RsMain` (2148 B, 2.38%), `AddPaksAndFactories` (1936 B, 0.21%), `CheckReset` (1180 B, 0.34%),
  `__ct__CGameArchitectureSupport` is done, `StreamNewGameState` (532 B, 18.68%),
  `InitializeSubsystems` (348 B, 12.44%), and the `CGameGlobalObjects` destructor cluster
  (`__dt__CGameGlobalObjects_80006518` 264 B plus `fn_800067A8` / `fn_800067E0` / `fn_80006830` /
  `fn_80006850` / `fn_80006874` / `fn_800068F4` / `fn_80006954` / `fn_800069AC` / `single_ptr_assign_800064D0`),
  which is run 2's 15-member teardown with an inline/out-of-line mix at +0x148/+0x138/+0x134 and
  is its own item.
- **A header edit that a note deferred for blast radius is worth re-measuring**: the note's
  reason here was a claim about four other files that the grep contradicts, and the function had
  **no definition in the tree at all**. `grep -rn` for the symbol before deferring.
- **The fast experiment loop, for the next spelling search in this file**: copy the `cflags` line
  out of `build.ninja` (or, as here, run
  `ninja build/G2ME01/src/MetroidPrime/main.o && ./build/tools/objdiff-cli report generate -o
  build/report.json` and read the two functions out of the report) - **1.4 s per spelling** once
  the file mtime is bumped, which is what made 17 variants affordable. `tools/probe_cc.sh` does
  not work for this file (run 4's note: no `-i extern/musyx/include`, no `MUSY_*` defines).
  Watch out: ninja's mtime granularity silently skips a rebuild if the file is rewritten inside
  the same second, which reads as "every spelling failed".

---

# run 6 (lane 4, 2026-09-30) - the return type closes `fn_800070A4`, and run 2's rename scan was
# broken: 64 -> 66, 2 functions / 120 bytes

Run 5's commit (`aa36756`) is on this branch, so run 5's numbers are the baseline and none of its
work needed redoing. Re-measured first, as runs 2-5 all had to:

```
baseline  aa36756  main/MetroidPrime/main  64 / 99 functions, matched_code 7972, fuzzy 48.433212%
                        tree                 10097 matched functions, 4918 linked
```

Run 4's `fn_800070A4` and `CMain::SetMaxSpeed` leads are both closed (`SetMaxSpeed` was already at
100% when this run started - run 4's WALL was overtaken). What follows is the run-4 NEW item
`match-main-fn-800070a4` **done**, plus one rename that run 2 found and run 5 wrongly declared
absent.

## 1. `fn_800070A4`: 86% -> 100%, and the fix is the return type

Run 4 characterised this exactly right and then stopped one step short. Its words: *"the cursor
must be hoisted and stay loop-carried, which needs one extra live volatile that costs no
instruction ... a source that keeps `self` live in the loop without recomputing the cursor is what
is missing."* The extra live volatile is **`r3` itself**, and the way to keep it live for free is
**to return it**:

```cpp
extern "C" SGameStateRecords* fn_800070A4(SGameStateRecords* self, int n, const SGameStateRecord& value) {
  self->x00_count = n;
  SGameStateRecord* rec = self->x04_recs;
  for (int i = 0; i < n; ++i) {
    if (rec) { *rec = value; }
    ++rec;
  }
  return self;                     // <-- the whole fix
}
```

`tools/bytescmp.py` on the compiled body: **`0 differing instructions of 20 (80 bytes ours vs 80
retail)`**, against 13 for the same body declared `void`. objdiff: 86.00% -> **100.0%**.

**This is a better model of retail, not a trick to move the score.** Retail's epilogue is a bare
`blr` at 0x800070F0 with `r3` still the entry `r3` - the same return-this tail run 4 documented
for `fn_80144924` (`SGameStateSlots`'s constructor) and the same tail its **caller in this very
file** already has: `fn_80007040` (0x80007040, 100%, matched in run 4) is written
`SGameStateWorlds* fn_80007040(SGameStateWorlds* self)` with `return self;`. Two `SGameState*`
constructors in a row, one written to return `this` and one declared `void`, was the tell.

### Why it is the register ladder, and what it is worth to know

Declared `void`, mwcceppc retires `r3` right after `addi r9,r3,4`, and its temporary ladder starts
one register low:

```
declared void   addi r9,r3,4  stw r4,0(r3)  lwz r8,0(r5) r7 r6  lbz r3,12(r5) lbz r0,13(r5) ...
retail          stw  r4,0(r3)  addi r10,r3,4 lwz r9,0(r5) r8 r7  lbz r6,12(r5) lbz r0,13(r5) ...
```

Both the register set *and* the order of the first two instructions fall out of the same cause.
`r3` being live at the point where the fifth field needs a register is what stops the ladder
wrapping down to `r3` and forces the whole thing up one slot to `r10`.

**Thirty-nine spellings were compiled and measured this run** (`probesrc.sh` + `bytescmp.py`,
~0.4 s each; the earlier runs' twenty are in run 4's table and are not repeated). The ones that
say something new:

| spelling | `bytescmp` result |
|---|---|
| **`return self` (ret type `SGameStateRecords*`)** | **0 of 20 - retail** |
| redundant `self->x00_count = n;` **after** the copy in the body | 5 of 21 - retail's registers, one extra `stw` |
| loop bound `i < (int)self->x00_count` | 17 of 22 - **retail's exact prologue and register set**, but `li r11,0` + a `cmpw/blt` tail instead of `mtctr/bdnz` |
| hybrid: store through `self->x04_recs[i]`, guard on the cursor | 7 of 21 (84 B) |
| `if (rec && n)` / `if (rec != (SGameStateRecord*)self)` / `end = rec + n` guard | 16-20 of 22 |
| 5 named value locals, fieldwise stores | 12 of 20 - fixes the `stw`/`addi` order, then puts the fields in `r6,r7,r8` ascending |
| the same, declared in reverse / stored in reverse | 12 and 13 of 20 |
| `n = (int)self->x00_count` in the body | 12 of 22 |
| `SGameStateRecords* const s = self` | 19 of 21 (the count store is emitted twice) |
| `__restrict` on `self`; `rec[0]`; `int m = n` bound; `for (i=n;i>0;--i)`; `++i,++rec` in the for header; `while (n-- > 0)`; `while (n>0) {--n;}`; `for (rec; n>0; ++rec,--n)`; cursor declared before the count store; array-reference local; `SGameStateRecord& dst = *rec; dst = value;` | 13 of 20 - all identical, none reach the return-type spelling |
| `++rec` first, then `rec[-1] = value` | 14 of 19 (76 B) |
| `unsigned n` with an `(unsigned)n` bound | 14 of 20 - `cmplwi` where retail has `cmpwi` |
| merged condition `i < n && rec` | 19 of 21 (84 B) |
| guard hoisted outside the loop | 20 of 79 (316 B, unrolled) |
| 4th unused parameter; comma-expression body; `rec != self`; store to a file-static | compile failed |

**The generalisable rule, and it is worth more than this function:** *when a loop body's temporaries
come out one register lower than retail's, look for what retail keeps live that you retire early -
and a return value is free, where an in-loop use is not.* All four in-loop ways of keeping `self`
alive (a redundant store, a reload in the bound, a `cmpw` guard, a `cmpw` against `self`) cost
exactly one instruction each, which is why run 4 could not find it. Check the **epilogue** first:
a bare `blr` with the entry `r3` still in `r3` is a return-this tail, and MWCC keeps the base
pointer live across the whole function for it.

## 2. `fn_80007AA0` -> `push_back<list<CArchitectureMessage>>`: 1 rename, 40 bytes

`config/G2ME01/symbols.txt:149`, the only `config/` edit this run:

```
fn_80007AA0 = .text:0x80007AA0; // type:function size:0x28
-> push_back__Q24rstl55list<20CArchitectureMessage,Q24rstl17rmemory_allocator>FRC20CArchitectureMessage
   = .text:0x80007AA0; // type:function size:0x28 scope:weak
```

The name and `scope:weak` are read back off our own `main.o` with `powerpc-eabi-nm` (the mangled
name mwcceppc gives the body at `.text+0xea8`, which `nm` reports as `W`), and the size is retail's
0x28. It is 1:1 and it is an identification, not a coin flip:

* retail 0x80007AA0's 10 instructions are **byte-identical** to our `push_back`'s, including the
  `mr r5,r4 / lwz r4,8(r3)` argument shuffle - the only differing bytes are the `bl`
  displacement;
* **the relocation target is the same named function**: our `push_back`'s `bl` at `.text+0xebc`
  has `R_PPC_REL24 do_insert_before__Q24rstl55list<20CArchitectureMessage,...>FPQ...4nodeRC20CArchitectureMessage`,
  and retail's `bl` at 0x80007AB4 goes to `do_insert_before<...>` at 0x80007AC8, which
  `symbols.txt:150` already names and which is at 100%;
* it is the **only** 40-byte body in the unit's whole 17608-byte claim, on both sides;
* `who_calls.py 0x80007AA0` returns exactly one caller, `Push__18CArchitectureQueueFRC20CArchitectureMessage`
  (0x80007A80, 32 bytes, 100%), which is retail's only `push_back` on that list; and
* `src/MetroidPrime/main.cpp:603` already said so before this run: *"`rstl::list` is out of line in
  retail (`fn_80007AA0`)"*.

**Run 5's note on this body is wrong and should not be trusted:** it says `fn_80007AA0` *"is not
shape-identical to our `push_back<...>`, even mnemonic-only, so run 2's ambiguity note for it is
stale: there is no tie to decline."* The two bodies are identical modulo the `bl` displacement -
which is the whole point of run 2's relocation-aware comparison, and which run 5's own scan was
supposed to be doing. The note's conclusion (it is simply an unwritten 40-byte body) was an
artefact of a scanner that was not tokenising branches.

## 3. The rename scan is now **exhausted**, and here is a scanner that works

I rewrote run 2's relocation-aware instruction comparison and ran it over the whole unit
(`.tmp/opencode/scan.py`: `objdump -d --no-show-raw-insn` on `main.elf` and on `main.o`, keep the
mnemonic plus the operands of load/store/arith, replace every branch/call target with a token,
require equal instruction count, then match retail functions scoring <100% against our functions
that are *not already paired*). The one trap, which is almost certainly what broke run 5's copy:
**`--no-show-raw-insn` changes the line format**, so a regex written for the `94 21 ff f0 \tmflr`
form matches nothing and every function normalises to the empty sequence, which then matches every
other empty sequence. The regex has to accept both forms.

Result on this tree - **three hits, and all three are the ones run 2 and run 5 already declined**:

```
RETAIL 0x80006678  88  __dt__80006678   <->  __dt__Q24rstl25single_ptr<11CMemoryCard>Fv  (.text+0x1e1c, 88)
RETAIL 0x800066d0  84  __dt__800066D0   <->  __dt__18CArchitectureQueueFv              (.text+0x1874, 84)
RETAIL 0x80006ae0  88  __dt__80006AE0   <->  __dt__Q24rstl25single_ptr<11CMemoryCard>Fv  (.text+0x1e1c, 88)
```

Two retail bodies against one of ours is a tie between the retail bodies, and run 3's evidence
(0x80006AE0 is called with `addi r3,r1,20 / li r4,-1` from `RsMain` and frees a
`sizeof(CGameGlobalObjects)`=0x164 pointee) says it is not `~single_ptr<CMemoryCard>`; 0x800066D0 is
run 5's measured false 1:1. **So there is no sound rename left in this unit.** A next run should
not re-run this scan; it should take one of the size-unique bodies instead - see the list below.

A cheaper first filter than the scan, and it is exact here: **list the unit's functions grouped by
`size` from `build/report.json` and look for a size that occurs once on the retail side and once on
our side.** That is how `fn_80007AA0` was found in the first place, and after taking it every
remaining unmatched size is either absent from `main.o` or contested.

## Measured, from `build/report.json`

`main/MetroidPrime/main` **64 -> 66 of 99** functions, `.text` fuzzy **48.433212% -> 48.7240%**,
`matched_code` 7972 -> 8092 (**+120**). Tree-wide, full per-function diff against the `aa36756`
baseline (`tools/report_diff.py .tmp/opencode/baseline-report.json build/report.json`):

```
matched 10097 -> 10099   linked 4918 -> 4918   (+2 functions at 100%, 0 units newly linked)
  +100%    main/MetroidPrime/main :: fn_800070A4                                     80 B
  +100%    main/MetroidPrime/main :: push_back__Q24rstl55list<20CArchitectureMessage,
                                    Q24rstl17rmemory_allocator>FRC20CArchitectureMessage   40 B
  RENAMED  fn_80007AA0 -> push_back__Q24rstl55list<...>FRC20CArchitectureMessage (0.00% -> 100.00%)
no regression
```

**No function anywhere got worse and no unit lost a match.**

`./tools/goal_check.sh build/goal/item.json` on this tree:

```
ok  no judge-owned path touched
ok  gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
ok  counts: matched 10097 -> 10099   linked 4918 -> 4918
ok  check_symbol_names.py
ok  All:  31.07% fuzzy, 23.37% matched, 11.78% linked (10099 / 28465 functions)
flip  flip_test MetroidPrime/main.cpp: FAIL - judged below as partial progress
ok  target rose: main/MetroidPrime/main: 64 -> 66 / 99 functions
ok  no asm added
goal_check: PARTIAL match-main-fn-80009274 - flip_test ... FAIL, but the target rose; commit it and keep the item
```

Gates on their own: `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`;
`tools/probe_sources.sh` = `750 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0
duplicates)` against a baseline of exactly 250 - **no NEW, no GONE**; `check_symbol_names.py` =
`checked 504 units; 0 declared names are missing`; `gate.sh`'s hash check ok for all 86 RELs.

`tools/unit_fit.sh MetroidPrime/main.cpp`: `.text` claimed 17608, ours **9948**, **SHORT by 7660**
(run 5: 9920 / SHORT by 7688, so this run closed 28 of the 7688); `.ctors` still SHORT by 4;
`.sbss` over by 21 (run 3 measured that as inherited); and the extras list **fell from 16 functions
/ 1340 bytes to 15 / 1300**, because the rename took `push_back<list<CArchitectureMessage>>` out of
it. **Decl order is unchanged and structurally so:** `check_decl_order.py --unit "MetroidPrime/main"
--list` reads **42** both with and without this run's diff (measured by stashing it and rebuilding),
and neither function is in the permutation - `fn_80007040` is at `.text+0xc40` and `fn_800070A4` at
`.text+0xca4`, which is the descending-by-retail-offset order their addresses call for, and
`push_back` is a weak COMDAT at `.text+0xea8` that takes no part in the source order. (Run 5 read
41; that was `ffedecc`, and the branch has moved since - the number did not change because of this
diff.)

## The flip: the same pre-existing blockers, and I did not touch them

`./tools/flip_test.sh MetroidPrime/main.cpp` -> `FAIL -> reverted (tree rebuilt: DOL
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010)`, and mwldeppc names four:

```
multiply-defined: 'CErrorOutputWindow::__vt' in CErrorOutputWindow.o
undefined: 'fn_80008C28'      undefined: 'lbl_80418EA0'      undefined: 'fn_80009224'
```

All four are pre-existing and none was made worse or better. The set is not quite runs 1-5's:
`lbl_80418EA0` appears where they had `rstl::rc_ptr<CMapWorldInfo>::ReleaseData()`, which is the
fourth of mwldeppc's undefined list - it reports three - so the two are the same list at different
depths, not a change. Run 4's lead that renaming `fn_80009224` to
`ReleaseData__Q24rstl28rc_ptr<16CWorldLayerState>Fv` would remove one of these is still open and
still **not** a count (our tree emits that instantiation as a weak COMDAT in `CGameState.o`, so
objdiff pairs nothing); it is a link fix for whoever eventually flips this unit.

WALL: MetroidPrime/main.cpp flip - four pre-existing link-level blockers
(`CErrorOutputWindow::__vt` multiply-defined, `fn_80008C28` / `lbl_80418EA0` / `fn_80009224`
undefined) and .text still SHORT by 7660 bytes over 33 unwritten functions, so no amount of work on
any single function in this unit can flip it; treat this unit as `progress`-shaped and requeue it as
such. (Re-measured this run: 7660 and 33, not run 5's 7688 and 37.)

## What a next run on this unit should know

- **`fn_800070A4` is done, and the `NEW:` item run 4 filed for it is satisfied** - the answer was
  the return type, and the recipe is above. `SetMaxSpeed` is at 100% too. The rename scan is
  exhausted. Do not spend a run on any of those three.
- **33 functions remain unmatched.** The two smallest size-unique candidates, both of which need a
  *use* in `main.cpp` rather than a rename, and both of which I measured as blocked:
  * **`fn_80008B04`** (44 B) is `__dl__32TOneStatic<18CGameGlobalObjects>FPv`. Run 3 enumerated the
    three routes; #1 (make `CMain::gameGlobalObjects` a `single_ptr`) regresses the 60-byte
    `__dt__5CMainFv` that is at 100%, #2 (write `~CGameGlobalObjects`) is the destructor half and
    would not reach 100%, #3 (a local `single_ptr` in `RsMain`) is the contrived host the reviewer
    rejects. The honest way in is to write the rest of `RsMain`, which is where retail's own
    `r1+20` local is.
  * **The five `rstl::rc_ptr<T>::ReleaseData` bodies retail puts in main's range and our tree emits
    elsewhere** - 0x80009008, 0x80009058, 0x80009224, 0x8000934C, 0x800095E4, all 80 bytes, all the
    same shape (`lwz r4,4(r3) / lwz r3,0(r4) / addic. r0,r3,-1 / stw r0,0(r4) / bgt / lwz r3,0(r31)
    / li r4,1 / bl <~T> / lwz r3,4(r31) / bl CMemory::Free`). All four of our `main.o` copies are
    already paired against retail's *other* rc_ptr bodies, so claiming these five means giving
    `main.cpp` a real use of `rc_ptr<CMapWorldInfo>` / `rc_ptr<CPlayerState>` /
    `rc_ptr<CWorldLayerState>` / `rc_ptr<CWorldTransManager>`, whose holders live in other TUs. Any
    such use invented only to host a symbol is the shape the reviewer rejects; it needs a run that
    is writing `CGameState`'s or `CWorldState`'s teardown.
- **The unwritten bulk, unchanged in kind from run 5:** `RsMain` (2148 B, 2.38%),
  `AddPaksAndFactories` (1936 B, 0.21%), `CheckReset` (1180 B, 0.34%),
  `StreamNewGameState` (532 B, 18.68%), `InitializeSubsystems` (348 B, 12.44%), and the
  `CGameGlobalObjects` destructor cluster (`__dt__CGameGlobalObjects_80006518` 264 B plus
  `single_ptr_assign_800064D0` / `__dt__80006678` / `__dt__800066D0` / `__dt__80006AE0` /
  `fn_80006724` / `fn_800067A8` / `fn_800067E0` / `fn_80006830` / `fn_80006850` / `fn_80006874` /
  `fn_800068F4` / `fn_80006954` / `fn_800069AC` = 1176 B in one contiguous 0x800064D0..0x80006B10
  run). That cluster is the largest single block of unmatched bytes left in the unit and it is one
  item, not thirteen.
- **`CMain::AsyncIdle` stays at 99.17% and I did not re-try it**, but run 5's description of the
  remaining difference is **backwards** and the corrected version is cheaper to act on. `bytescmp`
  on this tree says the two sides are
  `+100 ours 7fc5f378 | mr r5,r30` against `+100 retail 57c5063e | clrlwi r5,r30,24` - **retail has
  the `clrlwi` and we do not**, and the callee is retail's `AsyncIdle__11CResFactoryFUib`
  (`symbols.txt` `b` = `bool`). So the argument MWCC is failing to narrow is a `bool` local, and
  run 5's seventeen spellings all made it *worse* by adding a `neg/or/srwi` normalisation. Run 5's
  conclusion - that the byte-exact spelling is to declare the callee's parameter `unsigned char`
  and that it costs a 268-byte function in `main/Kyoto/CResFactory` - stands as a cost, but the
  direction to try is "give MWCC a value it already knows is 0 or 1 *as the argument expression*",
  not "narrow the argument yourself".

## The port side

No change reaches the host build. `src/MetroidPrime/main.cpp` is **not** in `files.cmake`
(`grep -c 'src/MetroidPrime/main.cpp' files.cmake` = 0), so neither the return-type change nor the
`extern "C"` bodies are compiled for the PC target; `config/G2ME01/symbols.txt` is matching-build
only. The one thing worth naming: `src/MetroidPrime/mainMid.cpp:679` carries its own
`extern "C" void fn_800070A4() {}` stub and was already inconsistent with main.cpp's three-argument
signature before this run, so the return type does not make it worse and I left it alone.
`tools/probe_sources.sh` reports **the same 250 undefined as the baseline** - no NEW, no GONE.

---

# run 7 (lane 4, 2026-09-30) - the run-6 rename was never committed, and `fn_80008B04` was
# blocked on a premise this tree falsifies: 73 -> 75, 2 functions / 84 bytes

Run 6's commit is on this branch as `3240602` and the branch has moved twice more since run 6 wrote
its numbers, so **run 6's counts are stale and its `config/` edit is not in the tree at all**.
Re-measured first, as runs 2-6 all had to:

```
baseline  3240602  main/MetroidPrime/main  73 / 99 functions, matched_code 9016, fuzzy 55.188778%
                        tree                 10106 matched functions, 4918 linked
```

`config/G2ME01/symbols.txt:149` still read `fn_80007AA0` on arrival and `3240602` touched only
`src/MetroidPrime/main.cpp` and two docs, so **run 6's first rename is not lost - it was never
applied.** Its identification is sound and it is taken here (measured again below). The second
thing run 6 declined, `fn_80008B04`, is also reachable, and for a reason run 3 and run 6 both got
backwards.

## 1. `fn_80007AA0` -> `push_back<list<CArchitectureMessage>>`: 1 function, 40 bytes

The only `config/` change for it, and it is run 6's line:

```
config/G2ME01/symbols.txt:149
  fn_80007AA0 = .text:0x80007AA0; // type:function size:0x28
-> push_back__Q24rstl55list<20CArchitectureMessage,Q24rstl17rmemory_allocator>FRC20CArchitectureMessage
   = .text:0x80007AA0; // type:function size:0x28 scope:weak
```

Re-measured on this tree rather than trusted, and it holds on all four points:

* the 40 bytes are **byte-identical**, not just shape-identical - the only differing field is the
  `bl` displacement (`.text+0x13fc` ours vs `0x80007ab4` retail);
* our `bl` at `.text+0x13fc` has `R_PPC_REL24 do_insert_before__Q24rstl55list<20CArchitectureMessage,
  Q24rstl17rmemory_allocator>FPQ34rstl55list<...>4nodeRC20CArchitectureMessage`, and retail's bl at
  0x80007AB4 goes to 0x80007AC8, which `symbols.txt:150` already names and which this unit matches
  at 100% - **the same named callee**, which is the part that makes it an identification;
* the **call graph agrees**: `powerpc-eabi-objdump -r` shows our `Push__18CArchitectureQueueFRC20
  CArchitectureMessage` (`.text+0x13d4`) calling `push_back`, and `tools/who_calls.py 0x80007AA0`
  returns exactly one caller, retail's `Push` at 0x80007A80, at 0x80007A8C;
* 40 bytes is the only body of that size in the unit's whole 17608-byte claim, on both sides, and
  `nm` reports our copy `W` (weak COMDAT), hence `scope:weak`.

The argument shape (`mr r5,r4` duplicating the value, `lwz r4,8(r3)` = the list's tail sentinel) is
`push_back(const T&)` and not `push_front`, because our own C++ `push_back` compiles to `8(r3)` and
nothing in retail's 17608 bytes is a second 40-byte body to tie it with.

## 2. `~CGameGlobalObjects` exists, and the compiler writes `operator delete` for you

`fn_80008B04` (0x80008B04, 0x2C = 44 bytes) is `TOneStatic< CGameGlobalObjects >::operator delete`,
i.e. `__dl__32TOneStatic<18CGameGlobalObjects>FPv`. Run 3 wrote, of the three routes:

> 2. **Writing `~CGameGlobalObjects`.** ... but it is the *dtor* half; retail's `bl 0x80008B04` at
> `0x80006600` is behind the deleting flag, so **the body alone never names `operator delete`.**

and run 6 repeated the claim as "needs a *use*, not a rename". **Both are wrong on this tree, and
this file already contains the counterexample.** `CGameArchitectureSupport` derives from
`TOneStatic< CGameArchitectureSupport >` exactly as `CGameGlobalObjects` does, its destructor is
defined here at `src/MetroidPrime/main.cpp:490`, and **no `operator delete` is written anywhere in
that body** (`grep -n delete src/MetroidPrime/main.cpp` returns one comment and nothing else). What
it compiles to is `__dt__24CGameArchitectureSupportFv` (0x80007DE8, 220 bytes, **already matched at
100%**) ending

```
17b4: extsh. r0,r31 / ble 17c4 / mr r3,r30 / 17c0: bl __dl__38TOneStatic<24CGameArchitectureSupport>FPv
```

with `objdump -r` showing `R_PPC_REL24 __dl__38TOneStatic<24CGameArchitectureSupport>FPv` at
`.text+0x17c0`. So **mwcceppc emits the class-specific `operator delete` call itself, guarded by the
deleting flag, whenever a destructor for a class with a class-specific `operator delete` is defined
in the translation unit.** Route #2 was the right one; it was declined on a premise, not on a cost.

So the change is the same `{}`-body arrangement runs 1 and 2 used for `~CMapWorldInfo` and
`~CWorldLayerState`, and the header's own member declarations produce the teardown:

| file | change |
|---|---|
| `include/MetroidPrime/CGameGlobalObjects.hpp:91-94` | **declared** `~CGameGlobalObjects();` - the class had none |
| `src/MetroidPrime/main.cpp:836-853` | `CGameGlobalObjects::~CGameGlobalObjects() {}` + the comment, in the descending-by-retail-offset run 0x80006518 calls for (immediately after `__dt__80006678`, 0x80006678) |
| `config/G2ME01/symbols.txt:174` | **renamed** `fn_80008B04` -> `__dl__32TOneStatic<18CGameGlobalObjects>FPv` (0x2C, `scope:weak`) |

The name and `scope:weak` are read back off our own `main.o` (`nm` reports `W`), the size is retail's
0x2C, all 11 instructions are byte-identical to retail's 11, our `bl` at `.text+0x27e4` has
`R_PPC_REL24 ReferenceCount__32TOneStatic<18CGameGlobalObjects>Fv` and retail's bl at 0x80008B10
goes to 0x80008B3C - **the same already-matched function**. And the call site agrees: our
`__dt__18CGameGlobalObjectsFv` calls it from `.text+0xfd0`, retail's 0x80006518 calls it from
0x80006600, in both cases the instruction right after `mr r3,r30` behind `extsh. r0,r31 / ble`.
Nothing about this one is a coin flip: **the identifier is the callee.**

Run 3's other two routes still stand as written - #1 (make `CMain::gameGlobalObjects` a
`single_ptr`) regresses the 60-byte `__dt__5CMainFv` that is at 100%, and #3 (a local
`single_ptr<CGameGlobalObjects>` in `RsMain`) is the contrived host the reviewer rejects. Route #2
was always the one, and it needed no contrivance at all.

## Measured, from `build/report.json`

`main/MetroidPrime/main` **73 -> 75 of 99** functions, `.text` fuzzy **55.188778% -> 55.665833%**,
`matched_code` 9016 -> 9100 (**+84** = 40 + 44). Tree-wide, full per-function diff against the
`3240602` baseline (`tools/report_diff.py .tmp/opencode/baseline-report.json build/report.json`):

```
matched 10106 -> 10108   linked 4918 -> 4918   (+2 functions at 100%, 0 units newly linked)
  +100%  main/MetroidPrime/main :: __dl__32TOneStatic<18CGameGlobalObjects>FPv                          44 B
  +100%  main/MetroidPrime/main :: push_back__Q24rstl55list<20CArchitectureMessage,
         Q24rstl17rmemory_allocator>FRC20CArchitectureMessage                                             40 B
  RENAMED fn_80007AA0 -> push_back<...>   (0.00% -> 100.00%)      same address
  RENAMED fn_80008B04 -> __dl__32TOneStatic<18CGameGlobalObjects>FPv   (0.00% -> 100.00%)   same address
no regression
```

**No function anywhere got worse and no unit lost a match.**

`./tools/goal_check.sh build/goal/item.json` on this tree:

```
ok  no judge-owned path touched
ok  gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
ok  counts: matched 10106 -> 10108   linked 4918 -> 4918
ok  check_symbol_names.py
ok  All:  31.09% fuzzy, 23.39% matched, 11.78% linked (10108 / 28465 functions)
flip  flip_test MetroidPrime/main.cpp: FAIL - judged below as partial progress
ok  target rose: main/MetroidPrime/main: 73 -> 75 / 99 functions
ok  no asm added
goal_check: PARTIAL match-main-fn-80009274 - flip_test ... FAIL, but the target rose; commit it and keep the item
```

Gates on their own: `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`;
`tools/probe_sources.sh` = `750 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0
duplicates)` against a baseline of exactly 250 - **no NEW, no GONE**; `check_symbol_names.py` =
`checked 504 units; 0 declared names are missing`; `gate.sh`'s hash check ok for all 86 RELs.

`tools/unit_fit.sh MetroidPrime/main.cpp`, measured twice on this tree by stashing the diff and
rebuilding (run 6's figures were from `aa36756` and are not comparable):

```
                        before      after
  .text  ours           11168      11464      SHORT by 6440 -> 6144  (this run closed 296)
  .sbss  over by           25         25      unchanged - inherited, and NOT this change
  extras             16/1340 B   16/1552 B   push_back's 40 B left the list, the 252 B
                                        __dt__18CGameGlobalObjectsFv joined it
```

`check_decl_order.py --unit "MetroidPrime/main"` reads **8 shown + "44 more" both with and without
this diff** (measured the same way), and structurally so: `~CGameGlobalObjects` sits in the
descending-by-retail-offset run 0x80006518 calls for, and both new bodies are a `push_back` COMDAT
and a `__dl__` COMDAT that take no part in the source order. `docs/HANDOFF.md` appears in this run's
`git diff` with only its state block re-derived - that is `tools/check_docs_claims.py`, run as a step
of `gate.sh`, and the driver discards it.

## The flip: the same four pre-existing blockers, and I did not touch them

`./tools/flip_test.sh MetroidPrime/main.cpp` -> `FAIL -> reverted (tree rebuilt: DOL
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010)`, and mwldeppc names the same four runs 1-6 saw, verbatim:

```
multiply-defined: 'CErrorOutputWindow::__vt' in CErrorOutputWindow.o
undefined: 'fn_80008C28'      undefined: 'lbl_80418EA0'      undefined: 'fn_80009224'
```

WALL: MetroidPrime/main.cpp flip - the same four pre-existing link-level blockers
(`CErrorOutputWindow::__vt` multiply-defined, `fn_80008C28` / `lbl_80418EA0` / `fn_80009224`
undefined) and .text still SHORT by 6144 bytes over 24 unwritten functions, so no amount of work on
any single function in this unit can flip it; treat this unit as `progress`-shaped and requeue it as
such. (Re-measured this run: 6144 and 24 unmatched, not run 6's 7660 and 33 - the branch moved.)

## The 264-byte destructor: what `{}` got, what is still missing, and it is nearly there

Retail's `__dt__CGameGlobalObjects_80006518` (0x80006518, 264 bytes) is **nine** member teardowns in
reverse declaration order and the operator-delete tail. Ours is now 252 bytes and **six of the nine
are byte-identical to retail already** - +0x148 `renderer`, +0x138 `stringTable`, +0x134
`memoryCard`, +0x108 `characterFactoryBuilder`, +0xE4 `simplePool`, +0x04 `resFactory` - which is
the proof that the header's member order and types are right. The three that are not:

| member | retail | ours | what is wrong |
|---|---|---|---|
| +0x150 `x150_tail` | `addi r3,r30,336 / li r4,-1 / bl 0x801F097C` | **absent** | `CGameGlobalObjectsTail` (`CGameGlobalObjects.hpp:61`) has no destructor, so the implicit one is trivial |
| +0x00 `pad0` | `mr r3,r30 / li r4,-1 / bl 0x80309660` (`__dt__14CMemoryCardSysFv`) | **absent** | same: `CGameGlobalObjectsCardInit` (`:55`) has no destructor |
| +0x14C `inGameTweakManager` | `addi r3,r30,332 / li r4,-1 / bl 0x80006678` | `addic. r0,r30,332 / beq / lwz r3,332(r30) / bl <84 B>` | our `rstl::single_ptr< CInGameTweakManager >`'s out-of-line destructor is **84** bytes against retail's 88, so mwcceppc adds the null guard and calls ours |
| +0x130 `gameState` | `addi r3,r30,304 / li r4,-1 / bl 0x80006620` (`__dt__Q24rstl24single_ptr<10CGameState>Fv`, 88 B, already 100% here) | `addic. r0,r30,304 / beq / lwz r3,304(r30) / li r4,1 / bl` | mwcceppc **inlines** `~single_ptr` here and calls it out of line at +0x14C - the two decisions are the wrong way round |

So the class needs two destructor declarations and two flipped inline decisions, and the flag in r4
is the tell for the second pair: retail's out-of-line `single_ptr` calls pass `li r4,-1` and the
inlined ones pass `li r4,1`. **I did not attempt it**: it is four coupled changes to three headers
and the item's own target was reached. It is one item, not four, and it is a 264-byte function
whose six other members are already exact.

Renaming retail's `__dt__CGameGlobalObjects_80006518` to `__dt__18CGameGlobalObjectsFv` is what would
take our 252-byte body out of `unit_fit`'s extras list - **do it only once the body matches**, for
the reason run 5 learned the hard way about `fn_80007AA0`: a rename on an unproven identification
turns an honest "unpaired" into a confident wrong name.

NEW: match-main-dt-cgameglobalobjects | match | MetroidPrime/main | `~CGameGlobalObjects` is now
written and six of retail's nine member teardowns are already byte-identical; what is left is two
destructor declarations (`CGameGlobalObjectsCardInit`, `CGameGlobalObjectsTail`) and two flipped
`~single_ptr` inline decisions at +0x130/+0x14C, with `li r4,-1` vs `li r4,1` as the tell

## `fn_800067E0` / `fn_800067A8` - twenty-two more spellings, both walls for now

The tweak-manager cluster that `match-main-ciengametweakmanager-dtor` left at 90.00% and 69.79% is
**not** one instruction from done, and `tools/bytescmp.py`'s "N differing instructions of M" is a
much better signal than the percentage for a 14-instruction function: a one-instruction change moved
the percentage by 0.5 (69.79 -> 70.86 and -> 71.0 are *different* one-instruction outcomes).

`fn_800067E0` (80 bytes) is **4 of 20 differing on every spelling**, and one of those four is the
`bl` relocation, so the real difference is a 3-instruction permutation in the prologue:
`lwz r31,0(r3)` belongs between `stw r31,12(r1)` and `stw r30,8(r1)` and mwcceppc puts it after
`mr r30,r4`. Eight spellings this run, all 4/20: swap the two declarations; declare `it` first and
assign after; one combined declaration; `for (STweakValue* it = *first; it != *end; ++it)` with `end`
declared first; `STweakValue**& end = last`; `it = it + 1`; `STweakValue* const* end`; and a local
`*last` copy (that one changes the semantics and drops to 14 of 19).

`fn_800067A8` (56 bytes) is the better of the two and the tree's spelling is **8 of 14**. Six
spellings this run; the best is **6 of 14** (69.79% -> **71.0%**), reached twice:

```cpp
STweakValue *f, *l;          // or:  STweakValue *f, *l = *last;
l = *last;                   //           f = *first;
f = *first;
fn_800067E0(&f, &l);
```

which changes mwcceppc's output to retail's slot assignment (`l` at `r1+8`, `f` at `r1+12`, loaded
`l` first). What is left is two instructions: retail loads into **r5** where we load into r0, and
retail's `stw r0,20(r1)` sits at +0x0C rather than +0x08. Also tried, all 8/14 or worse: the two
locals in one declaration with `f` initialised, `const` pointers, a two-element array as the home
slots, taking the addresses before the loads, and swapping the call's argument order (which changes
the function to 20 instructions). I did **not** keep the 71.0% spelling in the tree: it moves no
count, and a diff that only moves a percentage is not this item's business.

WALL: fn_800067E0 90.00% - the loop and the epilogue are retail's; the only difference is that
`lwz r31,0(r3)` is emitted after `mr r30,r4` instead of between `stw r31,12(r1)` and `stw r30,8(r1)`,
and eight prologue/loop spellings this run all leave the same 4 of 20 instructions differing (one of
them the `bl` relocation)

## The block nobody has taken: 0x80008C28..0x80008F40, five functions, 792 bytes

Now the largest **self-contained** unmatched block left in the unit, and its structure is measured
even though its class is not named anywhere in the tree. That last part is why six runs have not
touched it: `tools/who_calls.py` puts its three callers at 0x80003DA0, 0x80003F58 and 0x8000408C,
and **main's claim starts at 0x800053B8**, so every user of the type lives in another unit and
`CGameGlobalObjects`/`CMain` name nothing of it.

* `fn_80008CE0` (0x80008CE0, 136 B) is a constructor: `allocate(44)` (`li r3,44`) then
  `stw r31,0 / r26,4 / r27,8 / r28,12` (four words) then a **copy-constructed `rstl::string`** at
  `+16` from the 5th argument. So the type is 44 bytes = `{ u32, u32, u32, u32, rstl::string }`,
  and `rstl::string` is 0x1C here as `STweakAudio`'s `CHECK_SIZEOF(STweakAudio, 0x20)` already says.
* `fn_80008C28` (0x80008C28, 184 B) is a recursive **tree copy**: it recurses on `*src` and
  `src+4`, builds a node with `(left, right, 0, src+16)` through the constructor above, and stores
  the new node into `left+8` and `right+8`.
* `fn_80008D68` (0x80008D68, 128 B) is that node's **recursive destructor**: recurse on `*(this+0)`
  and `*(this+4)`, then `internal_dereference` the string at `+16`.
* `fn_80008E94` (172 B) and `reserve<vector<pair<Ui,Ui>>>` (172 B) are called from 0x80003F58 and
  0x80003DA0 respectively, so they are the same shape's `insert`/`reserve` and belong to the same
  type - 792 bytes and about five constructors/destructors, not one function.

Whoever takes this should start from the 44-byte `CHECK_SIZEOF` and the two recursive bodies, not
from the names: with the callers outside the unit there is nothing in the tree to name it after, and
a `symbols.txt` rename would be a guess (`docs/PROCESS_LESSONS.md`, "verification that cannot fail").

## The port side

Two changes are `config/`-only (`symbols.txt` is matching-build, and it is not in `files.cmake`), so
nothing reaches the host from them. The destructor is the one to check, and it is clean, measured:

* The declaration in the header is unconditional (the matching build does not define `TARGET_PC`, so
  an `#ifdef` would hide it from exactly the build that needs it), and the **definition is in
  `src/MetroidPrime/main.cpp`, which is not in `files.cmake`** (`grep -c 'src/MetroidPrime/main.cpp'
  files.cmake` = 0; `mainMid.cpp` appears 3 times). So the host build sees the declaration and no
  definition - which is safe only if nothing destroys a `CGameGlobalObjects` there, and nothing does:
  `CMain::gameGlobalObjects` is a raw `CGameGlobalObjects*` (`include/MetroidPrime/CMain.hpp`), the
  port's only `CGameGlobalObjects` is a `new` at `src/MetroidPrime/PortBoot.cpp:247`, and no
  `single_ptr< CGameGlobalObjects >` or by-value instance exists in `src/`.
* Measured rather than assumed: `tools/probe_sources.sh` reports **the same 250 undefined as the
  baseline** - no NEW, no GONE. A new undefined would have shown up there, which is the check.
* `~CGameGlobalObjects` is not on the boot path and changes no behaviour: it is the same declaration
  pattern `~CMapWorldInfo` and `~CWorldLayerState` have had in this file since run 1.
* Run 3's note on the base class still holds and is unaffected: `new CGameGlobalObjects(...)` in
  `PortBoot.cpp` still goes through `TOneStatic<CGameGlobalObjects>::operator new` and
  `GetAllocSpace()`'s static buffer, and nothing `delete`s one, so no `__dl__` is reached on the host.

## What a next run on this unit should know

- **24 functions remain unmatched** (99 - 75). Both renames taken here are closed; do not redo them.
- **Run 3's route #2 for `fn_80008B04` was the right one** and its stated reason was wrong. The
  general rule, because it cost two runs: *when a class has a class-specific `operator delete`
  (here inherited from `TOneStatic<T>`), a destructor **defined** in the translation unit makes
  mwcceppc emit the `operator delete` call itself, guarded by the deleting flag - you do not have to
  write a `delete`, and you must not invent a `single_ptr` to get one.*
- **Unwritten, unchanged in kind:** `RsMain` (2148 B, 2.38%), `AddPaksAndFactories` (1936 B, 0.21%),
  `CheckReset` (1180 B, 0.34%), `StreamNewGameState` (532 B, 18.68%), `InitializeSubsystems`
  (348 B, 12.44%), `__dt__CMemoryInStreamFv` (96 B), and the five `rstl::rc_ptr<T>::ReleaseData`
  bodies retail puts in this range and our tree emits elsewhere (0x80009008 / 0x80009224 /
  0x800095E4 plus the two already-named ones) - still needing a *real* use of an `rc_ptr` holder,
  which is the item's own original thesis and still unclaimed.
- **`single_ptr_assign_800064D0` (72 B, 0x800064D0) is `rstl::single_ptr<CGameGlobalObjects>::operator=`**
  - `lwz r3,0(r3) / li r4,1 / bl ~CGameGlobalObjects / stw r31,0(r30) / mr r3,r30` - and its only
  caller is `RsMain`. It is the same "a local `single_ptr` in `RsMain`" route run 3 declined, and it
  belongs to a run that writes `RsMain` and can justify the local with the construction that goes
  with it. Same for `fn_800068F4` (96 B), also called only from `RsMain`.
- **The fast experiment loop, measured again:** `ninja build/G2ME01/src/MetroidPrime/main.o` +
  `./build/tools/objdiff-cli report generate -o build/report.json` + `python3 tools/bytescmp.py
  <obj> <symbol> <retail_addr> <size>` is **~2 s per spelling**, and 22 spellings were affordable
  because of it. `tools/probe_cc.sh` still does not work for this file (run 4: no
  `-i extern/musyx/include`, no `MUSY_*` defines). Watch ninja's mtime granularity (run 5).

## Lane 4: passed, then failed on the moved tip (2026-09-30 08:14:24Z)

The judged change failed goal_check.sh (exit 1) once rebased onto 7a417cfa54a8; re-do it against the current tip.
---

# run 8 (lane 4, 2026-09-30) - run 7's `~CGameGlobalObjects` re-done against the current tip, the two
# member destructors added, and `fn_80008B04`'s rename taken: **76 -> 77 of 99, 1 function / 44 bytes**

Run 7's change was judged once and failed on the moved tip, so nothing of it was in the tree. Run 7
already characterised the work correctly, so this run did run 7's work and its measurement, and
added the two member-destructor declarations run 7 listed as the remaining piece. Re-measured
first, as every run since run 2 had to:

```
baseline  7a417cf  main/MetroidPrime/main  76 / 99 functions, matched_code 9192, fuzzy 55.557472%
                        tree                 10110 matched functions, 4918 linked, 250 port undefined
```

`build/report.json` in this worktree was **stale on arrival** (it was run 7's build, and read
`__dl__32TOneStatic<18CGameGlobalObjects>FPv` as already at 100.0% at offset 14156 - which run 7's
report never said). The first `./tools/decomp_build.sh` is what made the numbers real:
**76 / 99**, not the 73 / 99 run 7 measured and not the 77 the stale report showed. Run 7's own
conclusions were right; its numbers were not.

## What changed (four files)

| file | change |
|---|---|
| `include/MetroidPrime/CGameGlobalObjects.hpp:81-95` | **declared** `~CGameGlobalObjects();` - the class had none |
| `include/MetroidPrime/CGameGlobalObjects.hpp:64,79` | **declared** `~CGameGlobalObjectsCardInit()` and `~CGameGlobalObjectsTail()` - run 7's remaining piece |
| `src/MetroidPrime/main.cpp:893` | `CGameGlobalObjects::~CGameGlobalObjects() {}` + the comment |
| `src/MetroidPrime/PortGlobals.cpp:67,1134-1152` | PC-side bodies for the two member destructors, plus the include |
| `config/G2ME01/symbols.txt:174` | **renamed** `fn_80008B04` -> `__dl__32TOneStatic<18CGameGlobalObjects>FPv` |

```
config/G2ME01/symbols.txt:174   (the only config/ edit)
  fn_80008B04 = .text:0x80008B04; // type:function size:0x2C
-> __dl__32TOneStatic<18CGameGlobalObjects>FPv = .text:0x80008B04; // type:function size:0x2C scope:global
```

`scope:global`, not `scope:weak`: retail's symbol table has it `T`, and the sibling
`__dl__38TOneStatic<24CGameArchitectureSupport>FPv` at 0x80008A78 is also `scope:global` in this
file and already matches at 100%. **The `scope:weak` run 6 and run 7 both wrote is wrong**; it is
harmless (objdiff ignores it) but it is a fact about retail's table and it is now right.

## 1. `~CGameGlobalObjects` exists, and the compiler writes `operator delete` for you - re-measured

Run 3 wrote, of `fn_80008B04`: *"retail's `bl` at 0x80006600 is behind the deleting flag, so the
body alone never names `operator delete`"*, and run 6 repeated it as *"needs a use, not a name"*.
Run 7 falsified both, and **this run re-measured it from scratch rather than trusting run 7**: the
clean tree's `main.o` has **no** `__dl__32TOneStatic<18CGameGlobalObjects>FPv` at all
(`nm -S` on the pre-change object lists only the `CGameArchitectureSupport` `__dl__`), and it
appears the moment the destructor is defined. That is the measurement, not the inference.

```
before the destructor   nm | grep TOneStatic  ->  __dl__38TOneStatic<24CGameArchitectureSupport>FPv only
after                   ... plus __dl__32TOneStatic<18CGameGlobalObjects>FPv  (.text+0x27d8, 0x2C, W)
our dtor's last two relocations:
  0x0fd8  R_PPC_REL24  __dt__26CGameGlobalObjectsCardInitFv
  0x0fe8  R_PPC_REL24  __dl__32TOneStatic<18CGameGlobalObjects>FPv
retail:  0x800065E8  mr r3,r30 / li r4,-1 / bl 0x80309660 ; 0x800065FC mr r3,r30 / bl 0x80008B04
```

**The identifier is the callee, and it is checked:** our `__dl__` is 44 bytes, retail's is 44, and
`tools/bytescmp.py` reports **1 differing instruction of 11** - the `bl` field. Our `bl` at
`.text+0x27fc` has `R_PPC_REL24 ReferenceCount__32TOneStatic<18CGameGlobalObjects>Fv` and retail's
`bl` at 0x80008B10 goes to 0x80008B3C, which `symbols.txt:176` already names and this unit
matches at 100%: **the same named callee.** Not a coin flip, and not a shape coincidence.

## 2. The two member destructors - the piece run 7 measured and did not do

Run 7's table said `+0x150 x150_tail` and `+0x00 pad0` were **absent** from our 252-byte body
because `CGameGlobalObjectsTail` and `CGameGlobalObjectsCardInit` have no destructor, so the
implicit one is trivial. Declaring both is one line each and it is measurable:

```
                                   before      after
  __dt__18CGameGlobalObjectsFv      252 B       276 B    (+24 = the two calls)
  retail                           264 B       264 B
  of which byte-identical           6 of 9      7 of 9    (+0x150 tail, +0x00 pad0)
```

Retail's `+0x150` is `addi r3,r30,336 / li r4,-1 / bl 0x801F097C` and `+0x00` is
`mr r3,r30 / li r4,-1 / bl 0x80309660` - and **`mr r3,r30` with no offset is the tell for `+0x00`**:
the member is at offset 0, so the address-setup instruction is a `mr`, not an `addi`. mwcceppc emits
exactly that once the destructor is declared, and `__dt__26CGameGlobalObjectsCardInitFv` /
`__dt__22CGameGlobalObjectsTailFv` show up as the two new relocations. Neither body is in main's
claim (`fn_801F097C` and `__dt__14CMemoryCardSysFv` are both outside 0x800053B8..0x80009880), so
they stay **declared, not defined**, and the calls stay `bl`.

## What is still missing on the 264 bytes, and why it is now a closed question

The destructor is 276 against retail's 264 and **53 of 69 instructions differ**. Seven of the nine
member teardowns are now byte-identical; the two that are not are the same two run 7 found, and
**this run measured the mechanism rather than restating it**:

```
retail +0x14C  addi r3,r30,332 / li r4,-1 / bl 0x80006678     <- out of line, no null guard
ours          addic. r0,r30,332 / beq / lwz r3,332(r30) / li r4,1 / bl   <- inlined, 5 instrs
retail +0x130  addi r3,r30,304 / li r4,-1 / bl 0x80006620     <- out of line, no null guard
ours          addic. r0,r30,304 / beq / lwz r3,304(r30) / li r4,1 / bl   <- inlined, 5 instrs
```

**This is not a tuning problem, it is a class of member type, and the tree already has the switch.**
`rstl/single_ptr` has an opt-in out-of-line form behind `RSTL_SINGLE_PTR_OUT_OF_LINE`
(`include/rstl/single_ptr.hpp:15`), used by exactly one TU, `Kyoto/DolphinCDvdFile.cpp`. Retail
calls `~single_ptr<CGameState>` and `~single_ptr<CInGameTweakManager>` **out of line** here and
inlines `~single_ptr<IRenderer>` (+0x148) and `~single_ptr<CMemoryCard>` (+0x134) - so retail
itself uses **both** forms in one destructor, and the macro is all-or-nothing per TU. Getting both
decisions right in one class needs a per-instantiation control the template does not have, which
is why this run stopped rather than guessed: **it is a change to `rstl/single_ptr.hpp`'s design,
not a spelling.** See the NEW item below.

`rstl/single_ptr.hpp` also confirms the two out-of-line bodies are already right in this tree:
`__dt__Q24rstl24single_ptr<10CGameState>Fv` (0x80006620, 88 B) is **already matched at 100% here**
and `__dt__80006678` (88 B) is written by hand 300 lines up in `main.cpp` and also matches. The
only thing missing is the *call* being a call.

## Measured, from `build/report.json`

`main/MetroidPrime/main` **76 -> 77 of 99** functions, `.text` fuzzy **55.557472% -> 55.80736%**,
`matched_code` 9192 -> 9236 (**+44**). Tree-wide, full per-function diff against the clean
`7a417cf` baseline (`tools/report_diff.py .tmp/opencode/baseline-report.json build/report.json`):

```
matched 10110 -> 10111   linked 4918 -> 4918   (+1 function at 100%, 0 units newly linked)
  +100%    main/MetroidPrime/main :: __dl__32TOneStatic<18CGameGlobalObjects>FPv                            44 B
  RENAMED  main/MetroidPrime/main :: fn_80008B04 -> __dl__32TOneStatic<18CGameGlobalObjects>FPv  (0.00% -> 100.00%)
no regression
```

**No function anywhere got worse and no unit lost a match.**

`./tools/goal_check.sh build/goal/item.json` on this tree:

```
  ok  no judge-owned path touched
  ok  gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok  counts: matched 10110 -> 10111   linked 4918 -> 4918
  ok  check_symbol_names.py
  ok  All:  31.10% fuzzy, 23.39% matched, 11.78% linked (10111 / 28465 functions)
  flip  flip_test MetroidPrime/main.cpp: FAIL - judged below as partial progress
  ok  target rose: main/MetroidPrime/main: 76 -> 77 / 99 functions
  ok  no asm added
  goal_check: PARTIAL match-main-fn-80009274 - flip_test ... FAIL, but the target rose; commit it and keep the item
```

**The gate caught a real regression and the fix is in this diff.** With the two member destructors
declared and no host body, `gate.sh`'s port probe reported **251 undefined against a baseline of
250 - GREW**, and named the symbol: `NEW CGameGlobalObjectsCardInit::~CGameGlobalObjectsCardInit()`.
`src/MetroidPrime/PortGlobals.cpp` now defines both bodies, and the probe is back at **exactly 250,
no NEW and no GONE**. Worth carrying forward: *a declared-but-undefined destructor in a header
shared with the port is a link-gap regression, not a free change* - the PC build never sees
`main.cpp` (`grep -c 'src/MetroidPrime/main.cpp' files.cmake` = 0) so it needs its own definition,
and `probe_sources.sh` is what says so.

Gates on their own: `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`;
`check_symbol_names.py` = `checked 504 units; 0 declared names are missing`; `gate.sh`'s hash check
ok for all 86 RELs.

`tools/unit_fit.sh MetroidPrime/main.cpp`, measured twice on this tree by stashing the diff and
rebuilding (the numbers are only comparable within one measurement, because the branch has moved
under every earlier run):

```
                         before      after
  .text  ours           11168      11488    SHORT by 6440 -> 6120   (closed 320)
  .ctors  SHORT by          4          4    unchanged
  .sbss   over by          25         25    unchanged - inherited, measured by the stash
  extras              15 / 1300 B  16 / 1576 B  the new 276-byte __dt__18CGameGlobalObjectsFv joined
```

`check_decl_order.py --unit "MetroidPrime/main"` reads **53 permuted both with and without this
diff**, measured the same way, and structurally so: `~CGameGlobalObjects` sits immediately after
`__dt__80006678` (0x80006678), which is the descending-by-retail-offset run 0x80006518 calls for,
and it is the only new function in the permutation - the inherited 52 are run 7's
`docs/research/decl_order.md:48` "not a flip candidate" set.

## The flip: the same pre-existing blockers, plus two, and I did not touch them

`./tools/flip_test.sh MetroidPrime/main.cpp` -> `FAIL -> reverted`, and mwldeppc names runs 1-7's
set unchanged:

```
multiply-defined: 'CErrorOutputWindow::__vt' in CErrorOutputWindow.o
undefined: 'fn_80008C28'   undefined: 'lbl_80418EA0'   undefined: 'fn_80009224'
undefined: 'rstl::rc_ptr<CMapWorldInfo>::ReleaseData()'
```

(`sInfiniteLoopTime` multiply-defined also appears in `build/flip-ninja.log`; the log is longer than
the four lines mwldeppc prints on stdout, which is why runs 5 and 6 saw different subsets of the
same list. Same set, not a change.)

WALL: MetroidPrime/main.cpp flip - the same pre-existing link-level blockers
(`CErrorOutputWindow::__vt` multiply-defined, `fn_80008C28` / `lbl_80418EA0` / `fn_80009224` /
`rc_ptr<CMapWorldInfo>::ReleaseData()` undefined) and .text still SHORT by 6120 bytes over 22
unwritten functions, so no amount of work on any single function in this unit can flip it; treat
this unit as `progress`-shaped and requeue it as such. (Re-measured this run: 6120 and 22, not run
7's 6144 and 24 - the branch moved.)

## Two leads a next run should have, and one that is now closed

- **`~CGameGlobalObjects` is 276 bytes against retail's 264 and needs a change to
  `rstl/single_ptr.hpp`, not a spelling.** Seven of nine member teardowns are byte-identical; the
  last two need `~single_ptr<CGameState>` and `~single_ptr<CInGameTweakManager>` called out of line
  while `~single_ptr<IRenderer>` and `~single_ptr<CMemoryCard>` stay inlined, and the existing
  `RSTL_SINGLE_PTR_OUT_OF_LINE` macro is all-or-nothing per TU. **Do not rename
  `__dt__CGameGlobalObjects_80006518` -> `__dt__18CGameGlobalObjectsFv` until the body matches** -
  run 5 learned the hard way about `fn_80007AA0` that renaming an unproven body turns an honest
  "unpaired" into a confident wrong name, and this one is still 12 bytes out.
- **The block nobody has taken: 0x80008C28..0x80008F40, five functions, 792 bytes.** Run 7 measured
  its structure (`fn_80008CE0` = a 44-byte node constructor, `fn_80008C28` = a recursive tree copy,
  `fn_80008D68` = that node's recursive destructor, `fn_80008E94` and `reserve<vector<pair<Ui,Ui>>>`
  = the same type's insert/reserve). It is still the largest self-contained unmatched block in the
  unit. The blocker run 7 named is real and unchanged - **the three callers are at 0x80003DA0,
  0x80003F58 and 0x8000408C, all below main's claim at 0x800053B8**, so nothing in the tree names
  the type - but that only rules out a *rename*. The bodies can be written under retail's own
  `fn_` names as `extern "C"`, exactly as `fn_800067E0` / `fn_80006874` / `fn_80007040` above them
  already are, which is a real 792 bytes and does not need the class to be named at all.
- **The rename scan is still exhausted, re-measured on this tree.** `.tmp/opencode/scan2.py` is
  run 6's relocation-aware comparison with the regex fixed (it accepts both the `94 21 ff f0 \tmflr`
  and the `--no-show-raw-insn` line forms - getting that wrong normalises every body to the empty
  sequence and matches everything, which is what made run 5 wrongly call `fn_80007AA0` unwritten):
  **0 strict hits**, retail functions under 100% against unpaired functions in our object, equal
  instruction count, branch targets tokenised. A next run should not re-run it. Note the scan's
  bounds are `0x800053B8..0x80009880` (the report's `address` is an offset from
  `metadata.virtual_address`), **not** `0x80005C64`, which is where the staged
  `CMainAsyncIdle.cpp` split would cut.
- **`single_ptr_assign_800064D0` (72 B) and `fn_800068F4` (96 B)** are both called only from
  `RsMain` and both need retail's own `r1+20` local `rstl::single_ptr<CGameGlobalObjects>`. Runs 3,
  6 and 7 all declined this as a contrived host, and this run agrees: it wants a run that is
  writing the rest of `RsMain` (2148 B, 2.38%) and can justify the local with the construction that
  goes with it. Route #1 from run 3 - making `CMain::gameGlobalObjects` a `single_ptr` - is still a
  regression: `__dt__5CMainFv` is at 100% and would come off it.

## The port side

Three host bodies were needed, all in `src/MetroidPrime/PortGlobals.cpp` (not a `configure.py`
unit, so a definition there cannot collide with a retail object at DOL link time nor perturb any
unit's `.text`): `~CGameGlobalObjects` does **not** need one (nothing in the port destroys a
`CGameGlobalObjects`: `PortBoot.cpp:247`'s `new` is the only instance and nothing `delete`s it, and
`TOneStatic<CGameGlobalObjects>::operator new` never returns, so the destructor is unreachable),
but the two member destructors do, and the probe measured it. Bodies are empty and the comment says
why: retail's are two memory-card work-area frees and a module-map tree walk, neither of which the
port has. Measured consequence: **the same 250 undefined as the baseline, no NEW, no GONE.**
`~CGameGlobalObjects` is not on the boot path and changes no behaviour.

## What a next run on this unit should know

- **22 functions remain unmatched** (99 - 77); both renames this file ever had in this unit are
  taken, the scan is exhausted, `fn_800070A4` and `SetMaxSpeed` are done, and the `TOneStatic`
  family is 7 of 7. **This unit should be requeued as `progress`, not `match`.**
- **Do not redo:** the `TOneStatic` family (run 3), `fn_80008B04` (this run), `fn_80007040` /
  `fn_800070A4` (run 4 / run 6), `AddWorldPaks` (run 5), `~CMapWorldInfo` (run 1),
  `~CWorldLayerState` (run 1), the seven run-2 renames.
- **The fast experiment loop, measured again on this tree:** `bash .tmp/opencode/probe_main.sh
  <out.o>` compiles `src/MetroidPrime/main.cpp` with this unit's exact `build.ninja` flags in
  **~13 s** and `python3 tools/bytescmp.py <out.o> <symbol> <retail_addr> <size>` diffs the body
  instruction by instruction. `tools/probe_cc.sh` does **not** work for this file (no
  `-i extern/musyx/include`, no `MUSY_*` defines). `probesrc.sh` (the older helper in the same
  directory) compiles an arbitrary file in ~0.4 s. Watch ninja's mtime granularity - a file
  rewritten inside the same second reads as "the build did not run".
- **`~CWorldLayerState` etc. need a host body; the measure is `probe_sources.sh`, not the build.**
  A header declared-but-undefined destructor that `main.cpp` defines is invisible to the matching
  build and costs the port one undefined symbol per destructor, which the gate calls a regression.

## Lane 4: passed, then failed on the moved tip (2026-09-30 09:04:50Z)

The judged change failed goal_check.sh (exit 1) once rebased onto a10cad0e7eef; re-do it against the current tip.

---

# run 9 (lane 1, 2026-09-30) - `fn_800068F4` is the block's **clear**, not its destructor:
# **90 -> 91 of 99, 1 function / 96 bytes**

Runs 1-8's numbers were all against older tips and none of them is reusable: the branch has
moved under this item four times. Re-measured first, as runs 2-8 all had to:

```
baseline  8f26ac3  main/MetroidPrime/main  90 / 99 functions, matched_code 10852, fuzzy 64.98501%
                        tree                 10222 matched functions, 5004 linked, 250 port undefined
```

Runs 6 and 7's `fn_80007AA0` rename and run 8's `~CGameGlobalObjects` + `fn_80008B04` rename are
all **already in the tree** (`fn_80008C28` / `fn_80008CE0` / `fn_80008D68` all read 100.0% on
arrival), so nothing of theirs needed redoing. `CMain::AsyncIdle` (99.17%) and `fn_80006724`
(78.21%) are still the two near-misses and both are still where runs 5 and 7 left them.

## What changed: one source file, 46 lines added, nothing edited

| file | change |
|---|---|
| `src/MetroidPrime/main.cpp:1048-1093` | the `fn_80004864` declaration, `fn_800068F4`'s body, and the measurements below |

No `config/` edit, no header edit, no new file, no `asm`.

## What `fn_800068F4` is: `CGameState::x1f4`'s clear

Retail 0x800068F4, 0x60 = 96 bytes, and it was **UNPAIRED** on arrival - retail's bytes were in the
report under an `fn_` name with nothing of ours beside them. It is the same walk as the destructor
at 0x800047E0, with the two lines after the call swapped, and that is the whole identification:

```
retail 0x800068F4                    retail 0x800047E0 (the destructor)
  addi r4,r1,12 / addi r3,r1,20         addi r3,r1,20 / addi r4,r1,12
  lwz r0,4(r31)  / lwz r5,12(r31)        lwz r0,4(r30) / lwz r5,12(r30)
  mulli r0,r0,12 / add r5,r5,r0          mulli r0,r0,12 / add r5,r5,r0
  stw r5,12(r1) / stw r5,8(r1)           stw r5,12(r1) / stw r5,8(r1)
  stw r0,16(r1) / stw r0,20(r1)         stw r0,16(r1) / stw r0,20(r1)
  bl 80004864                            bl 80004864
  li r0,0 / stw r0,4(r31)   <-- clears   lwz r3,12(r30) / bl CMemory::Free  <-- frees
                                         extsh. r0,r31 / ble / mr r3,r30 / bl CMemory::Free
```

The prefix is **byte-for-byte the same 13 instructions**, including the four stores and both
`addi r3,r1,20 / addi r4,r1,12` argument setups. The two bodies differ only after the call, so
this is the same source shape with `x04_count = 0` where the destructor has `Free(x0c_data)` and
the deleting-destructor tail. That makes it the block's **clear**: the elements are destroyed, the
storage is kept, the count is zeroed.

Three independent measurements pin the object, none of them a guess:

* **`fn_80004864` (0x80004864, 0x38 = 56 bytes) reads its two arguments as pointers to pointers**
  (`lwz r5,0(r4)` / `lwz r0,0(r3)`, stores them on its own frame at r1+8 / r1+0xC) and calls
  `fn_8000489C`, which walks `first` to `last` in **strides of 12** calling `__dt__6CTokenFv` on
  each element's first word (`mr r3,r31 / li r4,0 / bl 8030154c` at 0x800048C8, `addi r31,r31,12`
  at 0x800048D4). So the call is `fn_80004864(&first, &last)` and the element is a 12-byte record.
* **Both callers pass the same object and it is `CGameState::x1f4`.** `CMain::RsMain` at
  0x8000637C-0x80006384 (`lwz r3,-28360(r13) / addi r3,r3,500 / bl 800068F4`) and `fn_80143E88`
  at 0x80143EA0-0x80143EA4 (the same three instructions). `-28360(r13)` is `gpGameState`
  (`main.cpp:568`'s own comment, from `CGameGlobalObjects`'s constructor), and **500 = 0x1F4** is
  `CGameState::x1f4` (`include/MetroidPrime/Player/CGameState.hpp:307`), the `SGameStateBlock` the
  constructor zeroes with `stw r0,504/508/512(r30)` at 0x801442CC/D4/D8.
* **`x04_count` and `x0c_data` are already measured.** `include/MetroidPrime/Player/
  CGameStateBlocks.hpp:22-26` says so for every `SGameStateBlock`: "+0x00 unknown, **+0x04 the
  element count**, +0x08 the capacity, **+0x0C the data pointer**". The body's
  `lwz r0,4(r31)` / `lwz r5,12(r31)` are those two words, and `mulli r0,r0,12` is the 12-byte
  stride `fn_8000489C` walks.

So `fn_80004864` stays a `bl`: its body is at 0x80004864, **below this unit's claim at
0x800053B8**, so it is declared and called, exactly as `fn_80007AA0` and `single_ptr_assign_800064D0`
already are in this file.

## Two spellings are load-bearing, and both are measured - the notes above did not have either

`tools/bytescmp.py` on the compiled body: **1 differing instruction of 24 (96 bytes ours vs 96
retail)**, and that one is the `bl` displacement to `fn_80004864` (which resolves to 0 at this
unit's link, as every other out-of-range `bl` in this file does). objdiff: **100.0%**.

1. **The two copies need `volatile`, or mwcceppc folds them.** Retail stores the same two values
   four times (r5 = `x0c_data + count*12` into r1+0x0C and r1+0x08; r0 = `x0c_data` into r1+0x10
   and r1+0x14) and passes r1+0x14 / r1+0x0C. Written as four plain locals - two passed, two
   dead copies - mwcceppc proves the copies redundant and emits **two** `stw`s, 22 instructions
   where retail has 24, **90.875%**. `volatile` on the two copies is what stops the register
   allocator from doing that. Sixteen spellings were compiled and measured before this: four
   plain locals in every declaration order, an array of four, a four-member struct, two
   two-element arrays, `const` copies, `+ 0`, and a redundant re-assignment after the call - all
   90.875%, all with the same 22 instructions.
2. **The `u8* end` temporary decides the accumulator register.** Written as
   `last = base + count * 12; lastCopy = last;` the sum lands in r0 (`add r0,r5,r0`) where retail
   has it in the register `x0c_data` was loaded into (`add r5,r5,r0`), and the body is **98.75%**
   with the right 24 instructions. Computing the end pointer into a temporary **first** and
   assigning *both* copies from **that** (not from `last`) keeps the base register as the
   accumulator: **100.0%**. Same instruction count either way, so this is only visible in
   `bytescmp` / the score - which is why it is written down here rather than left to be re-found.

The `volatile` alone, with the temporary absent, scores 95.83% (`add r5,r5,r0` right but the
register that holds the end pointer is clobbered by the reload of `x0c_data`); the temporary
alone, without the `volatile`, scores 90.875% with the wrong accumulator. **Both are needed.**

## Measured, from `build/report.json`

`main/MetroidPrime/main` **90 -> 91 of 99** functions, `.text` fuzzy **64.98501% -> 65.53021%**,
`matched_code` 10852 -> **10948 (+96)**. Tree-wide, full per-function diff against the clean
`8f26ac3` baseline (`tools/report_diff.py .tmp/opencode/L1/baseline-report.json build/report.json`):

```
matched 10222 -> 10223   linked 5004 -> 5004   (+1 functions at 100%, 0 units newly linked)
  +100%    main/MetroidPrime/main :: fn_800068F4                                   96 B
no regression
```

**No function anywhere got worse and no unit lost a match.**

`./tools/goal_check.sh build/goal/item.json` on this tree:

```
  ok  no judge-owned path touched
  ok  gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok  counts: matched 10222 -> 10223   linked 5004 -> 5004
  ok  check_symbol_names.py
  ok  All:  31.21% fuzzy, 23.51% matched, 11.81% linked (10223 / 28465 functions)
  flip  flip_test MetroidPrime/main.cpp: FAIL - judged below as partial progress
  ok  target rose: main/MetroidPrime/main: 90 -> 91 / 99 functions
  ok  no asm added
  goal_check: PARTIAL match-main-fn-80009274 - flip_test ... FAIL, but the target rose; commit it and keep the item
```

Gates on their own: `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`;
`check_symbol_names.py` = `checked 505 units; 0 declared names are missing from their object`
(run 8's tree said 504 - one more unit has appeared on the branch since, not this change);
`gate.sh`'s hash check ok for all 86 RELs; the port probe inside `gate.sh` is **749 files, 0
failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)** against a baseline of 250 - no NEW
and no GONE.

`tools/unit_fit.sh MetroidPrime/main.cpp`, measured twice on this tree by stashing the diff and
rebuilding:

```
                          before      after
  .text  ours           12892      12988    SHORT by 4716 -> 4620  (closed 96 - exactly this function)
  .ctors  SHORT by          4          4    unchanged
  .sbss   over by          25         25    unchanged - inherited
  extras        16 / 1340 B         same    no new function that retail's unit object lacks
```

**Decl order is correct, and checked structurally rather than by the script's count.**
`check_decl_order.py --unit "MetroidPrime/main"` reads **65 -> 66** permuted, and the +1 is
`fn_800068F4` itself - it joins the inherited set rather than adding a new inversion, because it
is in the descending-by-retail-offset run its address calls for: `powerpc-eabi-nm -n` on our own
object shows `fn_80006830` (0x1230), `fn_80006850` (0x1250), `fn_80006874` (0x1274),
**`fn_800068F4` (0x12f4)**, i.e. retail 0x80006830 < 0x80006850 < 0x80006874 < 0x800068F4. A
script over the whole unit's symbol list finds 24 inversions among paired functions, all
pre-existing and all in `docs/research/decl_order.md`'s "not a flip candidate" set;
`docs/HANDOFF.md` appears in this run's `git diff` with only its state block re-derived - that is
`tools/check_docs_claims.py`, run as a step of `gate.sh`, and the driver discards it.

## The flip: the same four pre-existing blockers, unchanged, and I did not touch them

`./tools/flip_test.sh MetroidPrime/main.cpp` -> `FAIL -> reverted (tree rebuilt: DOL
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010)`, and `build/flip-ninja.log` names:

```
multiply-defined: 'sInfiniteLoopTime' in auto_10_80418EC4_sbss.o
multiply-defined: 'CErrorOutputWindow::__vt' in CErrorOutputWindow.o
undefined: 'lbl_80418EA0'
undefined: 'fn_80177FF0'
```

**This is not runs 1-8's set** (`fn_80008C28` / `lbl_80418EA0` / `fn_80009224` /
`rc_ptr<CMapWorldInfo>::ReleaseData()`) - the branch has moved and three of those four are now
defined by units that landed since. The set is the *same kind* of thing: pre-existing link-level
holes in main's neighbourhood, none of them made worse or better by this change, which touches
one function body and adds one declaration. `sInfiniteLoopTime` is `main.cpp:113`'s own global
(`float sInfiniteLoopTime;`) and is multiply-defined with a `.sbss` claim; `fn_80177FF0` is
declared at `main.cpp:830` with a `MakeCMemoryCard()` helper.

WALL: MetroidPrime/main.cpp flip - four pre-existing link-level blockers
(`sInfiniteLoopTime` and `CErrorOutputWindow::__vt` multiply-defined, `lbl_80418EA0` and
`fn_80177FF0` undefined) and .text still SHORT by 4620 bytes over 8 unwritten functions, so no
amount of work on any single function in this unit can flip it; treat this unit as
`progress`-shaped and requeue it as such. (Re-measured this run: 4620 and 8 - not run 8's 6120
and 22, and the blocker list is not run 8's, because the branch moved.)

## The port side

No change reaches the host build and none was needed. `src/MetroidPrime/main.cpp` is **not** in
`files.cmake` (`grep -c 'src/MetroidPrime/main.cpp' files.cmake` = 0), so the `extern "C"`
declaration and body are matching-build only; the new `SGameStateBlock` reference adds no member
to any shared header and so cannot move a byte of `MetroidPrime/CGameState` or of any other unit.
Measured rather than assumed: the port probe is **250 undefined against a baseline of 250 - no
NEW, no GONE**, and the DOL rebuilds to `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`. `fn_800068F4`
is not on the boot path and changes no behaviour.

## What a next run on this unit should know

- **8 functions remain unmatched** (99 - 91), and the tree has moved a long way since run 8's list.
  Re-run `python3 .tmp/opencode/L1/show.py`-style reporting on `build/report.json` before
  believing any of the numbers above or below; `docs/goal-notes/match-main-fn-80009274.md` is
  the history, not the current state.
- **The remaining nine, measured on this tree:**
  `InitializeSubsystems__5CMainFv` (348 B, 12.44% - the body is `ARInit` plus a `// TODO`, and
  `src/MetroidPrime/PortBoot.cpp:495-590` already has retail's whole call list in host form,
  including the exact `lbl_803A56C0 + 0x187` / `+ 0x19D` printf strings, `ARInit/ARAlloc/ARQInit`,
  `OSGetCurrentThread` +0x304/+0x308, the 8 KB stack-guard fill with `0x7338CF85`,
  `OSProtectRange`/`DCFlushRange`, and the six trailing `Initialize` calls. **That comment is the
  best starting point for writing it** and no earlier run used it.);
  `fn_80006724` (132 B, 78.21% - run 7's `WALL:`, unchanged, its two dead iterator copies are the
  whole 22%); `CMain::AsyncIdle` (288 B, 99.17% - one instruction, `clrlwi r5,r30,24` against
  `mr r5,r30`, and run 6's correction of run 5's direction stands: give MWCC a value it *already*
  knows is 0 or 1 as the argument expression, do not narrow it yourself);
  `__dt__15CMemoryInStreamFv` (96 B, **UNPAIRED** - retail's `CMemoryInStream` deleting destructor
  is a plain `{}`-body arrangement: `li r4,0 / stw vtable / bl __dt__12CInputStreamFv` then the
  `extsh./ble/mr r3,r30/bl CMemory::Free` tail, and the class has an inline virtual dtor already,
  so it needs only a use in `main.cpp` - and `AddPaksAndFactories` builds one at r1+0xB4);
  `CMain::CheckReset` (1180 B, 0.34%); `CMain::RsMain` (2148 B, 2.38%);
  `CGameGlobalObjects::AddPaksAndFactories` (1936 B, 0.21% - a long straight-line run of
  `AddPakFileAsync` / `AddFactory` / `internal_dereference` pairs with `lbl_803A56C0 + N` strings,
  one `while (!AreAllPaksLoaded())` loop with two virtual calls, and an inlined
  `CMemoryInStream` construct/destroy);
  `CMain::StreamNewGameState` (532 B, 18.68%).
- **The generalisable result, and it is worth more than the one function:** retail's
  "store each of these two values twice, and pass the *second* copy of each" pattern appears in
  at least four bodies in this unit (`fn_800068F4`, `fn_80006724`, `fn_800067E0`'s neighbourhood
  and the destructor at 0x800047E0), and **it is `volatile` on the copies that reproduces it** -
  without it mwcceppc folds them and the body comes out two instructions short at ~90%. Runs 4
  and 7 measured fourteen and eight spellings of `fn_80006724` and `fn_800067E0` **without
  trying `volatile`**; that is the spelling to try there next. Note that on `fn_80006724` the
  remaining gap after `volatile` is likely the accumulator register, i.e. the `u8* end`-style
  temporary from above.
- **The fast experiment loop, measured again on this tree.** `.tmp/opencode/L1/probe.py` replaces
  one function's body from a file of `@@@`-separated variants, runs
  `ninja build/G2ME01/src/MetroidPrime/main.o` + `./build/tools/objdiff-cli report generate`, and
  prints **both the objdiff score and the emitted disassembly** - the disassembly is what tells
  you *why* a variant scored what it did, and this run needed it for every one of 25 variants at
  ~14 s each. `tools/probe_cc.sh` does **not** work for this file (no `-i extern/musyx/include`,
  no `MUSY_*` defines). Watch ninja's mtime granularity: the script sleeps 1.1 s before each
  build. When replacing a function body by script, locate the closing brace by **counting**, not
  by searching for `\n}\n` - a `}` inside a comment or a string will end the search early and
  silently delete the rest of the file. That cost this run one rebuild.
