# carve-801ef84c

Goal item `carve-801ef84c` (kind `match`, target `MetroidPrime/Carve801EF84C`).
**PASS** — `tools/goal_check.sh build/goal/item.json` exits 0.

## What this change is

Four files, one carve, nothing else:

| file | change |
| --- | --- |
| `src/MetroidPrime/Carve801EF84C.cpp` | new, 436-byte claim, 3 functions |
| `config/G2ME01/splits.txt` | `MetroidPrime/Carve801EF84C.cpp: .text start:0x801EF84C end:0x801EFA00` |
| `configure.py` | `Object(Matching, "MetroidPrime/Carve801EF84C.cpp")` at line 768 |
| `files.cmake` | `src/MetroidPrime/Carve801EF84C.cpp` at line 771 |

Each entry is in address order in its file. `config/G2ME01/splits.txt` still has the 28465
`total_functions` dtk reports at split time (`INFO Loading and analyzing 87 modules (using 16
threads) / INFO Initial analysis completed in 1.088s (found 28465 functions)`).

`.text 0x801EF84C..0x801EFA00`, 0x1B4 = 436 bytes, 3 functions, definitions in the source in
descending address order:

```
fn_801EF84C  0x801EF84C  0x64   25 instructions
fn_801EF8B0  0x801EF8B0  0x8     2 instructions
fn_801EF8B8  0x801EF8B8  0x148  82 instructions
```

`total_functions` in `build/report.json` is 28465 before and after.

## What the three functions are

Read off retail's own call edges, not guessed from the shapes:

- **`fn_801EF84C`** is the deleting destructor of a two-word `rstl::auto_ptr<T>`: `mHas` at +0
  (`lbz r0,0(r30) / cmplwi`), the owned pointer at +4 (`lwz r3,4(r30) / li r4,1 / bl`), then the
  `extsh.` flag test and `Free__7CMemoryFPCv`. Byte-shape twin
  `__dt__Q24rstl32auto_ptr<20CScannableObjectInfo>Fv`, 0x80110D8C, 0x64,
  `build/G2ME01/asm/MetroidPrime/Factories/CScannableObjectInfo.s:197-226`. This copy's `T` is
  whatever `fn_801EF730` (0x801EF730, 0x54, unclaimed, same run) destroys, so the class is written
  out as the two words the bytes read — the trade `src/MetroidPrime/ScriptObjects/CAtomicAlpha7E0.cpp`
  already makes for its own copy of the same shape. The flag is a **short**; `int` gives `cmpwi`.
- **`fn_801EF8B0`** is `lwz r3,0x8(r3) / blr`. Twin
  `GetParmDeleteIOWin__7MakeMsgFRC20CArchitectureMessage` (`src/MetroidPrime/Decode.cpp:4`,
  `build/G2ME01/asm/MetroidPrime/Decode.s:457-461`). It returns a pointer to a vector header:
  `src/MetroidPrime/Player/CScanDisplay.cpp` calls it at 0x80112854 and the next ten instructions
  read `+0x4` as a count and `+0xc` as items with `slwi ...,3`.
- **`fn_801EF8B8`** is `rstl::vector< rstl::pair< uint, uint > >::vector(const vector&)`,
  `include/rstl/vector.hpp:127-137`. Twin: the COMDAT
  `__ct__Q24rstl55vector<Q24rstl11pair<Ui,Ui>,Q24rstl17rmemory_allocator>FRC...` at 0x8018FE6C,
  0x148, `build/G2ME01/asm/MetroidPrime/CSlideShow.s:4174-4263`.
  `src/Kyoto/Animation/CAnimSourceReader.cpp` calls it at 0x802A3AA0 with `addi r3,r29,0x38`,
  which is `CAnimSourceReaderBase`'s POI vector — the two `rstl::pair<uint,uint>` vectors
  `include/rstl/pair.hpp` records.

### Callees

- `Free__7CMemoryFPCv` (0x802CE388) — claimed by `Kyoto/Alloc/CMemory.cpp`; defined for the host by
  `src/Kyoto/Alloc/PortMwccNew.cpp:39`.
- `allocate__Q24rstl17rmemory_allocatorFi` (0x802FDAB8) — retail's CodeWarrior mangling of
  `rstl::rmemory_allocator::allocate(int)`. Declared, never defined here; the port supplies it as
  `stub_179` (`src/MetroidPrime/PortLinkStubs.cpp:847`). No duplicate: the port's own symbol is
  `_ZN4rstl17rmemory_allocator8allocateEi`.
- `fn_801EF730` (0x801EF730) — inside the same unclaimed run, so nothing in the DOL defines it.
  Declared for the matching build and given a `#ifndef __MWERKS__` announced stand-in at the end of
  the carve, the arrangement `src/MetroidPrime/Cameras/Carve801E7C14.c:119-140` uses. Without it
  `tools/link_gap.py` fails the gate (the symbol is new to the port's link: nothing referenced it
  before, only dtk's `auto_*` objects did).

So the carve **adds no new stand-in except `fn_801EF730`**, and it claims `.text` only.

## Why it is a `.cpp` and not the `.c` the item named

The item says plain C so the `fn_` names do not mangle. `extern "C"` does that job equally well
(`src/MetroidPrime/Player/CGameStateBlockCopyCtor.cpp:34` is a `Matching` carve of exactly this
shape and is a `.cpp`), and C cannot reach retail's register assignment here. Five C spellings were
built and measured against retail's own bytes (109 words, compared by address with
`build/binutils/powerpc-eabi-objdump -d build/G2ME01/main.elf` against
`build/G2ME01/asm/MetroidPrime/Carve801EF84C.s`). Counts are **differing words of 109**:

| C spelling | differing | where |
| --- | --- | --- |
| block loop, `int remaining = self->mCount` | 53 | count in r0, dest in r6; `mr r3,r0` before the `srwi.` |
| same, `static CopyElems(src,count,dest)` (3 args) | 63 | **not inlined** — a 5th function `CopyElems` at +0xF0 in the object, which also shifts the DOL |
| `static inline`, `const SPair*` source, `int remaining` | 47 | count in r6 |
| `static inline`, `const SPair*` source, `--count` | 34 | src r6, count r3, dest r5 |
| `static inline`, plain `SPair*` source, `--count` | 34 | src r6, count r3, dest r5 |

Retail keeps the count in **r3** and derives *both* `srwi. r0,r3,3` and `andi. r3,r3,7` from it.
`rstl::uninitialized_copy_n` (`include/rstl/construct.hpp:141-151`) takes the count as its second
argument and carries it there — exactly the finding `CGameStateBlockCopyCtor.cpp:8-16` records for
the byte-element instantiation, whose object is byte-identical to retail's. Writing the body against
the real `rstl` headers reaches 0 differing words. The two C spellings that did keep the count in
r3 got there by **outlining** the helper, which puts a fifth function in the object and fails
`tools/unit_fit.sh` ("no extra functions"), so they were not usable.

`rstl::vector`'s own copy constructor is not reachable either: MWCC 2.7 rejects explicit
instantiation of a member (`template V::vector(const V&);` is a syntax error) and `template class
rstl::vector<...>` does not emit this COMDAT — which is why `src/MetroidPrime/CSlideShow.cpp:405-411`
forces the instantiation through a real function. Writing the body out emits no symbol beyond
`fn_801EF8B8` itself (`powerpc-eabi-nm` on the object lists three `T` and three `U`, nothing else).

The unit name is unchanged: `MetroidPrime/Carve801EF84C`.

## Why its own unit, and no gap spanned

Below: `fn_801EF7B0` (0x801EF7B0, 0x9C) ends **exactly** at 0x801EF84C. Above: 0x801EFA00 is where
the `Matching` `MetroidPrime/CStaticGeometryMap.cpp` claim begins. What is left of the run below is
`auto_03_801EF598_text`, which the build re-emits as 0x801EF598..0x801EF84C — so no claim spans an
unclaimed gap, and `fn_801EF730` stays retail's inside it. No `PortLinkStubs.cpp` duplicate: none of
`fn_801EF84C`, `fn_801EF8B0`, `fn_801EF8B8`, `fn_801EF730` or the two mangled callees was in that
file before this change.

## Measured

- `sha1sum build/G2ME01/main.dol` → `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`
- `build/report.json`, unit `main/MetroidPrime/Carve801EF84C`: `complete: true`,
  `fuzzy_match_percent 100.0`, `matched_functions 3 / 3`, `total_code 436`, `matched_code 436`
- matched **13468 → 13471**, linked **6516 → 6519** (`goal_check.sh`, step 2)
- `./tools/flip_test.sh MetroidPrime/Carve801EF84C.cpp` → `PASS  -> kept as Matching`,
  `kept: 1 / 1   failed: 0   skipped: 0`
- `./tools/unit_fit.sh MetroidPrime/Carve801EF84C.cpp` → `.text claimed 436 ours 436 retail 436
  fits` and `no extra functions: our object defines only what the retail unit object does`
- `python3 tools/check_symbol_names.py` → `checked 586 units; 0 declared names are missing`
- `python3 tools/check_decl_order.py` → `ok: 1186 unit(s) checked, 37 permuted, all 37 accounted for
  in decl_order.md` (this unit is not among them; `--unit MetroidPrime/Carve801EF84C.cpp` prints
  `0 unit(s) checked`, so the whole-file run is the one that checks it)
- `python3 tools/check_files_cmake.py` → `every configured DOL object is either in files.cmake or
  excluded with a reason`
- `build/gate-link.log` → `279 MISSING symbol(s), all accounted for in port_link_gap_list.md`
  (the judge's `build/goal/judge/undef.base.count` is 286, so the port's gap did not grow; it fell,
  because the carve defines three symbols retail's `auto_*` objects used to supply)
- `./tools/goal_check.sh build/goal/item.json` → `goal_check: PASS carve-801ef84c`, exit 0

`tools/goal_check.sh` rewrites the derived counts in `docs/HANDOFF.md` and
`docs/RUNNING_THE_DECOMP.md` itself (`MP_GATE_DOCS_WRITE=1`); those two edits were reverted so the
diff is only the four carve files, which is what the brief asks for.

## Caveats / follow-ups

- Nothing else in the tree was touched, and no `NEW:` item is filed: the item is finished, not walled.
- The item brief said the unit would be `Carve801EF84C.c`. It is `Carve801EF84C.cpp` because plain C
  measures 34 of 109 words short (table above) and `extern "C"` is what actually keeps the `fn_`
  names unmangled. The unit name and the address range are exactly what was queued.
- `fn_801EF730` (0x801EF730, 0x54) is the natural follow-up in the same run: it is the deleting
  destructor `fn_801EF84C` calls, it is 84 bytes of the same shape as the matched
  `__dt__19CStaticInterferenceFv` twin, and carving it would retire the stand-in added here. It is
  not filed as a `NEW:` item because it does not yet have a measured byte-exact body.