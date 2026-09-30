# match-main-cmapworldinfo-dtor

`kind: match`, `target: MetroidPrime/main.cpp`. Two of the three deleting destructors the item
named are now written out of line and reproduce retail byte for byte; the third is a measured wall.

## What changed

| file | change |
|---|---|
| `include/MetroidPrime/CMapWorldInfo.hpp:17` | **added** `~CMapWorldInfo();` - the class had no destructor at all |
| `include/MetroidPrime/CErrorOutputWindow.hpp:16` | `~CErrorOutputWindow() override {}` -> `override;` |
| `src/MetroidPrime/main.cpp:700` | `CMapWorldInfo::~CMapWorldInfo() {}` |
| `src/MetroidPrime/main.cpp:531` | `CErrorOutputWindow::~CErrorOutputWindow() {}` |
| `src/MetroidPrime/PortGlobals.cpp:1086` | PC-side bodies for both, plus the includes |

Both bodies are `{}`. That is the real body, not a stub: mwcceppc generates the whole 124/96 bytes -
the member teardowns in reverse declaration order for `CMapWorldInfo`, the vptr store plus the
flag-zeroed base-destructor call for `CErrorOutputWindow`, and the `extsh. r0,r31 / ble / bl
CMemory::Free` deleting tail both share. Measured against the object, instruction for instruction:

```
CMapWorldInfo (0x800090A8, 0x7C)  our .text+0x2e0, 124 bytes, no differing instruction
  addi r3,r30,56 / li r4,-1 / bl __dt__Q24rstl62vector<Q24rstl18pair<9TEditorId,b>,...>Fv
  addi r3,r30,40 / li r4,-1 / bl (same)
  addi r3,r30,20 / li r4,-1 / bl __dt__Q24rstl38bit_vector<Q24rstl17rmemory_allocator>Fv
  mr r3,r30      / li r4,-1 / bl (same)
  extsh. r0,r31 / ble / mr r3,r30 / bl Free__7CMemoryFPCv
```

`+0x38 / +0x28 / +0x14 / +0x00` is the header's member order read back, and it is also what pins
`sizeof(CMapWorldInfo) == 0x4C`: two 0x14 `rstl::vector<rstl::pair<TEditorId,bool>>` and two 0x14
`rstl::bit_vector<>` and a `bool`. The vptr store in the other one relocates against
`__vt__18CErrorOutputWindow` (`.data` 0x803B5910, the same symbol retail's constructor reaches).

**No source-level modelling was needed and none was invented.** The class headers already had the
right members in the right order; only the destructor's linkage was wrong.

## Measured, from `build/report.json`

`main/MetroidPrime/main` **35 -> 39 of 99** functions, `.text` fuzzy **33.64% -> 35.87%**,
`matched_code` 4356 -> 4748 bytes. Four functions went from unmatched to an exact match, and the
fourth is free:

```
__dt__13CMapWorldInfoFv                                   124 B  -> 100.0%
__dt__18CErrorOutputWindowFv                               96 B  -> 100.0%
__dt__Q24rstl38bit_vector<Q24rstl17rmemory_allocator>Fv    88 B  -> 100.0%   (emitted as the member dtor's callee)
__dt__Q24rstl37vector<Ui,Q24rstl17rmemory_allocator>Fv     84 B  -> 100.0%   (same, the bit_vector's storage)
```

Tree-wide, comparing the full per-function report before and after (`git checkout` of the four
files, full rebuild, diff of every `(unit, function)` pair):

```
fns 10023 -> 10027   matched_code 1512080 -> 1512472   units 724 -> 724   fuzzy 30.8503 -> 30.8563
dol 48.4664 -> 48.4767  fns 8612 -> 8616  units 561 -> 561
modules 6.2351 (unchanged)   game 57.3818 -> 57.3956  fns 8728 -> 8732  units 548 -> 548
sdk 98.6673 (unchanged)
functions worse: 0    functions better: 4 (the four above)
```

Gates, all on this tree: `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`;
`tools/probe_sources.sh` = `749 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0
duplicates)`, no NEW/GONE against `docs/research/port_link_baseline.txt`; `check_symbol_names.py` =
`0 declared names are missing`; 86 RELs `cmp`-equal. `flip_test.sh MetroidPrime/main.cpp` **FAILs**
and the reasons are all pre-existing - see below.

## The flip, and why it is out of reach

`./tools/flip_test.sh MetroidPrime/main.cpp` -> `FAIL -> reverted`. The linker names the blockers:

```
multiply-defined: 'CErrorOutputWindow::__vt' in CErrorOutputWindow.o
undefined: 'fn_80008C28'      undefined: 'fn_80009224'
undefined: 'rstl::rc_ptr<CMapWorldInfo>::ReleaseData()'
```

`tools/unit_fit.sh` says the same from the other side: **`.text` claimed 17608, ours 8124, SHORT by
9484**, plus 20 functions (1708 bytes) ours has and retail's does not, and `.ctors` SHORT by 4. The
unit still carries 60 unmatched functions, most of them unnamed retail bodies (`fn_80007AA0`,
`fn_80008B30`, `AddPaksAndFactories` at 0.21%, `RsMain` at 0.19%, `CheckReset` at 0.34%).

`tools/check_decl_order.py --unit "MetroidPrime/main"` also reports the unit **permuted** - our first
eight emitted functions are `__dt__19CStaticInterferenceFv`, `__dt__Q24rstl12CPlayerState...`,
`__dt__12CPlayerStateFv`, `StreamNewGameState`, `EnsureWorldPaksReady`, `AddWorldPaks`, `__pl__rstl...`,
`AsyncIdle` where retail's order is the reverse. That is inherited: `docs/research/decl_order.md:48`
already lists `main/MetroidPrime/main` as "33 functions, mostly `CMain`'s; not a flip candidate", and
the entry is still accurate at 39. **Both new destructors were placed in the descending-by-retail-
offset run their addresses call for** - `CErrorOutputWindow`'s at 0x800078F8 just above
`AddPaksAndFactories` (0x800070FC), `CMapWorldInfo`'s at 0x800090A8 just above
`StreamNewGameState` (0x800053B8) - so the two additions do not make the permutation worse.

`rstl::rc_ptr<CMapWorldInfo>::ReleaseData()` is the one new name in that list, and it is **not** a
consequence of this change: retail has it at 0x80009058, inside this unit's claimed range, and
nothing in the tree defines it. It was already a hole before.

## The third destructor: `CMemoryInStream`, blocked

`__dt__15CMemoryInStreamFv` (0x800055CC, 96 bytes) is the same generated shape and **is** reachable
this way - I did it, and it is 100.0% in `main.o` when the destructor is declared out of line. It
cannot stay that way, and the reason is measured, not guessed.

Making `~CMemoryInStream` out of line in `include/Kyoto/Streams/CMemoryInStream.hpp` turns it into
the class's key function, which stops the weak COMDAT copy that other units emit and makes them
call `__dt__15CMemoryInStreamFv` instead of inlining. `Kyoto/PVS/CPVSVisSet.cpp` is
`MatchingFor("G2ME01")` and 100%, and `MakePVSVisOctree__13CPVSVisOctreeFPCci` is one of its seven
functions. The two shapes, same function, same call site:

```
inlined (what must stay)          out of line (what the change produced)
  bl __ct__15CMemoryInStreamFPCvUl   bl __ct__15CMemoryInStreamFPCvUl
  ...                                ...
  lis r4,__vt__15CMemoryInStream@ha   bl __ct__13CPVSVisOctreeFRC6CAABoxiiPCc
  addi r3,r1,32                       addi r3,r1,32
  addi r0,r4,__vt__15CMemoryInStream@l li r4,0
  li r4,0                              bl __dt__12CInputStreamFv
  stw r0,32(r1)                        <- retail's is `li r4,-1 ; bl __dt__15CMemoryInStreamFv`
  bl __dt__12CInputStreamFv
```

Retail's own `MakePVSVisOctree` (0x802CEC40, 0x9C) **inlines** it - `4b d3 69 19 bl 800055cc` is
the only destructor call in the body - so retail's `Kyoto/PVS/CPVSVisSet.o` had no key function
either. The result of the change is concrete: the DOL sha1 went to
`8bb9f5b6caaa8a3d0aad3112bb9516c901aa442e`, `87 computed checksum(s) did NOT match`, and every
symbol from 0x802CECDC up moved (-12, then -24, -60, -72, -1100 ...). `main/MetroidPrime/main` was
not the cause; `Kyoto/PVS/CPVSVisSet` was, and it is a unit that has to keep its hash.

So: revert the header, keep the destructor inline, and the 96 bytes stay retail's. That is the
correct outcome for a `MatchingFor` unit, and it is the whole of the block.

**The general lesson, because it will be re-learned otherwise:** an inline `virtual ~X() {}` in a
header is load-bearing for *every* other unit that instantiates the teardown, and removing it is a
DOL-wide byte move. `~CErrorOutputWindow` survived it only because nothing else in the port or the
tree inlines `CErrorOutputWindow`'s teardown; `~CMemoryInStream` did not.

WALL: __dt__15CMemoryInStreamFv 100.0% - reachable only by making the destructor out of line, which
breaks the `MatchingFor` unit Kyoto/PVS/CPVSVisSet (DOL sha1 8bb9f5b6..., 87 checksums fail), and
retail's own CPVSVisSet object inlined the same teardown so retail had no key function either.

## The port side, and why `PortGlobals.cpp`

Both header changes are unconditional (the matching build does not define `TARGET_PC`, so an
`#ifdef` would hide them from exactly the build that needs them). The cost is that the PC link lost
two symbols, and `tools/probe_sources.sh` says so by name against
`docs/research/port_link_baseline.txt`:

```
NEW  CMapWorldInfo::~CMapWorldInfo()
NEW  vtable for CErrorOutputWindow
link: NOT LINKED (252 undefined, 0 duplicates)  against a baseline of 250 (GREW)
```

The vtable is the second-order effect and is the interesting one: on ELF a declared-but-undefined
destructor **is** the class's key function, so `CErrorOutputWindow` stopped having a weak vtable in
every TU that uses it and started wanting one emitted. `src/MetroidPrime/PortGlobals.cpp` is where
those go - it is not a `configure.py` unit, so a definition there cannot collide with a retail
object at DOL link time nor perturb any unit's `.text` (the file's own header records the
measurement that a definition inside a `NonMatching` unit does: `__ct__CGameArchitectureSupport`
84.51% -> 81.54%). After the two bodies:

```
probe: 749 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)
```

No NEW, no GONE. `CMapWorldInfo`'s PC body lets the compiler destroy the four members and free;
`CErrorOutputWindow`'s is empty for the same reason `CMainFlow::~CMainFlow()` is empty in
`src/MetroidPrime/CMainFlowDtor.cpp`. Neither is on the boot path, and the boot-progress gate is
not what this item is judged on.

## What a next run on this unit should know

- 60 functions are still unmatched, and the unit is **SHORT by 9484 bytes** before any of them is
  written. It is not close to flipping, and it was already listed as "not a flip candidate" before
  this item. Treat any further work here as `progress`-shaped.
- The three cheapest-looking ones are `AddPaksAndFactories` (0.21%, 1936 bytes) and `RsMain` (0.19%,
  2148 bytes) - both are essentially unwritten, which is why they score near zero - and
  `SetMaxSpeed__5CMainFb` (0.00%, 96 bytes), an ordinary accessor that nobody has written at all.
- `__dt__15CMemoryInStreamFv` is **not** in that list to try again: it is a wall, measured, above.
- `main.cpp` is a *shared* unit for a `rstl::list<CArchitectureMessage>` and friends: it carries
  `__dt__24CGameArchitectureSupportFv` (100%), `UpdateTicks` (98.51%), `__ct__5CMain` (88.80%) and
  `AddWorldPaks` (96.00%). Any edit here must leave those four where they are, which is why the
  before/after per-function diff in this file is the check to run, not the DOL sha1 alone - the
  sha1 is green whether or not `main.cpp` is in the link at all.
