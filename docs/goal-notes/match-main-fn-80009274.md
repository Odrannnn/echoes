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
