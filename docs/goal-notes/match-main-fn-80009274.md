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
