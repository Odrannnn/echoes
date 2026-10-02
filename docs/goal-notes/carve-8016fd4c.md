# carve-8016fd4c (match) - target `MetroidPrime/Carve8016FD4C`

**`goal_check.sh` PASS** (run twice, the second after the final comment polish):
`no judge-owned path touched`; gate.sh green (DOL sha1, 86 RELs, report diff, wiring, docs claims,
port probe); `counts: matched 12672 -> 12674   linked 6031 -> 6033`;
`check_symbol_names.py` clean; `All:  35.53% fuzzy, 29.37% matched, 13.05% linked (12674 / 28465)`;
`flip_test MetroidPrime/Carve8016FD4C.c: PASS, Object(Matching) in configure.py`.

## What I claimed

`.text 0x8016FD4C..0x8016FD94`, **0x48 = 72 bytes, 2 functions**, both byte-exact - exactly the run
the item asked for, nothing narrower and nothing wider:

| fn | addr | size | instructions | what it is |
| --- | --- | --- | --- | --- |
| `fn_8016FD4C` | 0x8016FD4C | 0x20 | 8 | `rstl::construct< CHealthInfo >`, the `(dest, src)` forwarder |
| `fn_8016FD6C` | 0x8016FD6C | 0x28 | 10 | its null-guarded branch: `if (self != 0) new (self) CHealthInfo(*src)` |

`report.json`: `main/MetroidPrime/Carve8016FD4C` **2 / 2 functions, 100.00% fuzzy, complete=true,
72 bytes**. The claim splits the unclaimed dtk run into `main/auto_03_8016DF3C_text`
(0x8016DF3C..0x8016FD4C, 19 functions, 7696 B), this unit, and `main/auto_03_8016FD94_text`
(0x8016FD94..0x80171DC4, 15 functions, 4172 B) - gate.sh reports it as
`SPLIT ... exact count match - a split, not a loss`. `measures.total_functions` still **28465**.

## The carve (four files, address order) plus one port-link shim

| file | entry |
| --- | --- |
| `config/G2ME01/splits.txt:870-871` | `MetroidPrime/Carve8016FD4C.c: .text start:0x8016FD4C end:0x8016FD94` (between `CRumbleManager.cpp` 0x8016DC58..0x8016DF3C and `CWorldLayerState.cpp` 0x80171DC4..) |
| `configure.py:720` | `Object(Matching, "MetroidPrime/Carve8016FD4C.c"),` (between `CInGameTweakManagerCtor.cpp` @0x8016C230 and `Carve80179E08.c` @0x80179E08) |
| `files.cmake:522` | `src/MetroidPrime/Carve8016FD4C.c` (between `CInGameTweakManagerReadFromMemoryCard.cpp` @0x8016BDE4 and `Carve80171DD4.c` @0x80171DD4) |
| `src/MetroidPrime/Carve8016FD4C.c` | the source, 2 definitions, **descending** by address |
| `src/Kyoto/Alloc/PortMwccNew.cpp:67-80` | `extern "C" void __ct__11CHealthInfoFRC11CHealthInfo(void* self, const void* src)` - see the port section |

No `PortLinkStubs.cpp` duplicate existed (grepped: the file stubs the **Itanium** name
`_ZN11CHealthInfoC1ERKS_` as `stub_43`, which a `TARGET_PC` build never references because
`include/MetroidPrime/CHealthInfo.hpp:12` declares the copy constructor only under
`MP_RETAIL_OUT_OF_LINE_COPIES && !TARGET_PC`).

## How the two functions were identified

Both are twins the seeder named, and both twins are in already-`Matching` units, so the shape was
never in doubt - only the callee was:

- `fn_8016FD4C` is `fn_80004D3C` (0x80004D3C, 0x20) in `src/MetroidPrime/Player/Carve80004C4C.c`:
  frame, one `bl`, both argument registers forwarded. Third copy of the same eight instructions:
  `__sys_free` (0x80008A28, `src/MetroidPrime/main.cpp`).
- `fn_8016FD6C` is `fn_80004D5C` (0x80004D5C, 0x28) in
  `src/MetroidPrime/Player/CGameStateBlockConstruct.cpp`: `cmplwi r3,0x0 / stw r0,0x14(r1) /
  beq +8 / bl / epilogue`. The only difference is the `bl` target.
- The callee is the measurement that names the element: the retail `bl` at 0x8016FD80 goes to
  `__ct__11CHealthInfoFRC11CHealthInfo`, 0x80070D60, 0x54 bytes, `symbols.txt:2082` - it copies
  0x20 bytes member by member (3 floats, word at +0x0C, halves at +0x10/+0x12, word at +0x14,
  halves at +0x18/+0x1A, byte at +0x1C), which is `CHECK_SIZEOF(CHealthInfo, 0x20)` in
  `include/MetroidPrime/CHealthInfo.hpp` exactly.
- The caller confirms it: `fn_8016FCB8` (0x8016FCB8, 0x94, unclaimed) tests the byte at +0x20 and
  either calls `bl fn_8016FD4C` on the object and sets that byte to 1 (0x8016FCD8), or copies the
  same 0x20 bytes memberwise itself (0x8016FCE8..0x8016FD34). So the pair is the "not constructed
  yet" branch of a 0x24-byte holder, and `fn_8016FC88` (0x8016FC88, 0x30) is the wrapper that
  returns the destination.

## Verified (all measured this run, in this tree)

```
sha1sum build/G2ME01/main.dol   6ef9b491d0cc08bc81a124fdedb8bfaec34d0010  (expected value, our object in the link)
ninja CHECK                     86 files OK                               (RELs unchanged)
flip_test.sh MetroidPrime/Carve8016FD4C.c   PASS -> kept as Matching   kept 1/1
unit_fit.sh MetroidPrime/Carve8016FD4C.c    claimed 72, ours 72, retail 72, fits; no extra functions
check_decl_order.py --unit Carve8016FD4C    1 unit(s) checked, none out of retail order
check_symbol_names.py                       532 units; 0 declared names missing
check_files_cmake.py                        every configured DOL object is in files.cmake or excluded
All:  35.53% fuzzy, 29.37% matched, 13.05% linked (12674 / 28465 functions)
```

Retail's bytes were read from `orig/G2ME01/sys/main.dol` (`tools/dol_read.py 0x8016FD4C 0x48`),
**not** from `build/G2ME01/main.elf` (that ELF is our own build). Our object's `.text` is 0x48 bytes
and differs from retail's 0x48 in **exactly the two `bl` words** (`48000001` + relocations in the
unlinked object); the matching DOL sha1 is what proves both resolve to 0x8016FD6C and 0x80070D60.

## The port link: one shim, and it is load-bearing (measured both ways)

The carve's `bl __ct__11CHealthInfoFRC11CHealthInfo` is a real reference nothing on a host link
defines, so `src/Kyoto/Alloc/PortMwccNew.cpp` - next to the four existing shims of exactly this
class (`__nw__FUlPCcPCc`, `Free__7CMemoryFPCv`, `__dt__6CTokenFv`, `__dt__13CFontImageDefFv`) -
gained the definition, and it is the **real copy**, not an empty stand-in:

```cpp
extern "C" void __ct__11CHealthInfoFRC11CHealthInfo(void* self, const void* src) {
  new (self) CHealthInfo(*static_cast< const CHealthInfo* >(src));
}
```

Measured, not assumed:

- `nm build-port-link/.../Carve8016FD4C.c.o` -> `U __ct__11CHealthInfoFRC11CHealthInfo`,
  `T fn_8016FD4C`, `T fn_8016FD6C`; `PortMwccNew.cpp.o` -> `T __ct__11CHealthInfoFRC11CHealthInfo`.
- `link_check.sh --strict` with the shim: **291 undefined, 0 duplicates, 0 compile errors,
  "STRICT PASS ... 291 undefined against a baseline of 291 (no growth)"**; probe
  `804 files, 0 failed, 0 errors; link: LINKED (291 undefined, 0 duplicates)`.
- Counterfactual, run with the shim wrapped in `#if 0` and then reverted: **292 undefined
  (`GREW`)** and the added-symbol list names `NEW __ct__11CHealthInfoFRC11CHealthInfo`. So the shim
  is worth exactly the one symbol the carve opens.
- Nothing else in the port references the pair: `nm -A` over every `build-port-link` object names
  `fn_8016FD4C`/`fn_8016FD6C` only in this carve's own object, so the shim is only what resolves the
  object's own `bl` (its public-copy semantics are real anyway, and are the same memberwise copy a
  `TARGET_PC` build of the class already makes inline - e.g. `Enemies/CAi.cpp`).

## Note for the next run (a mistake I made, and the rule that caught it)

I first wrote the two definitions **ascending** (fn_8016FD4C above fn_8016FD6C). mwcceppc emits in
reverse source order, so the object's `.text` came out `fn_8016FD6C` first and
`./tools/decomp_build.sh` failed immediately with `build/G2ME01/main.dol: FAILED` on the sha1 - the
whole-DOL check catches this before `flip_test` does, so the rule ("source order is *descending* by
address" - highest address defined **first**) is enforced at build time, not silently. After the
swap, everything above passed unchanged.

## Not done, and why

- No `WALL:` - both functions matched at 100.00% in the first successful build.
- No `NEW:` - the rest of this neighbourhood (`fn_8016FC88` 0x30, `fn_8016FCB8` 0x94,
  `fn_8016FD94` 0xBC, and the remaining 17 + 15 functions of the two `auto_*` runs) is
  unsourced-without-a-measured-twin or over the seeder's 64-byte limit; nothing I measured says any
  of it reaches 100%. The copy constructor itself (0x80070D60, 0x54) is inside
  `CScriptActor.cpp`'s existing claim, so it cannot be carved at all.
- The judge rewrote `docs/HANDOFF.md` / `docs/RUNNING_THE_DECOMP.md` (its own
  `check_docs_claims.py --write`: `12672 -> 12674`, `6031 -> 6033`, probe `803 -> 804 files`). I did
  not edit them; `run_goal.sh` checks them out of the baseline before judging.

---

## Lane 10 rerun, 2026-10-02 (`wt-mp2-goal-L10`, item re-queued)

**The carve above never landed in this lineage.** This worktree had no `src/MetroidPrime/Carve8016FD4C.c`, no
`Carve8016FD4C.c` entry in `configure.py` / `splits.txt` / `files.cmake`, a clean `git status`, and
`main/auto_03_8016DF3C_text` still held both functions (36 functions, 11940 bytes). So the work was re-done
from the bytes - not copied - and every number below is measured in this tree.

`goal_check.sh build/goal/item.json`: **PASS** (run three times, the last on the final file):
`no judge-owned path touched`; gate.sh green; `counts: matched 12685 -> 12687   linked 6044 -> 6046`;
`All:  35.54% fuzzy, 29.39% matched, 13.06% linked (12687 / 28465 functions)`;
`flip_test MetroidPrime/Carve8016FD4C.c: PASS, Object(Matching) in configure.py`.

Same conclusion as the run above - the same range, the same 2 functions, the same port shim - with these
extra measurements this run:

| check | measured |
| --- | --- |
| `sha1sum build/G2ME01/main.dol` | `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` |
| `build/report.json` `main/MetroidPrime/Carve8016FD4C` | 2 / 2 functions, 100.00% fuzzy, 72 bytes |
| `tools/carve_diff.sh 0x8016FD4C 0x48 build/G2ME01/src/MetroidPrime/Carve8016FD4C.o` | 18 vs 18 instructions; 2 differing, both `bl` words unresolved in an unlinked object |
| `powerpc-eabi-objdump -r` on that object | `R_PPC_REL24 fn_8016FD6C` at 0xC, `R_PPC_REL24 __ct__11CHealthInfoFRC11CHealthInfo` at 0x34 |
| `tools/unit_fit.sh MetroidPrime/Carve8016FD4C.c` | claimed 72, ours 72, retail 72, fits; no extra functions |
| `python3 tools/check_decl_order.py --unit Carve8016FD4C` | 1 unit checked, none out of retail order |
| `tools/link_check.sh --strict` | 291 undefined = baseline, 0 duplicates, 0 compile errors, STRICT PASS |
| `tools/probe_sources.sh` (via gate) | `810 files, 0 failed, 0 errors; link: LINKED (291 undefined, 0 duplicates)` |
| gate report diff | `SPLIT main/auto_03_8016DF3C_text: 17 function(s) moved into main/MetroidPrime/Carve8016FD4C, main/auto_03_8016FD94_text (exact count match - a split, not a loss)` |

The split, from `build/report.json`: `main/auto_03_8016DF3C_text` 36 fn / 11940 B -> **19 fn / 7696 B**; new
`main/auto_03_8016FD94_text` **15 fn / 4172 B**; `measures.total_functions` still **28465**.

### Re-derived this run rather than inherited

- Retail's bytes re-read from the disc: `tools/dol_read.py 0x8016FD4C 0x48`. `fn_8016FD4C` = frame +
  `bl +0x14`; `fn_8016FD6C` = `stwu`/`mflr`/`cmplwi r3,0`/`stw`/`beq +8`/`bl`/epilogue, as written.
- The `bl` at 0x8016FD80 goes to `__ct__11CHealthInfoFRC11CHealthInfo` (0x80070D60, `symbols.txt:2082`,
  0x54): 3 `lfs`/`stfs`, `lwz`/`stw` at +0x0C/+0x14, `lha`/`sth` at +0x10/+0x12/+0x18/+0x1A, `lbz`/`stb`
  at +0x1C = the 0x20 bytes of `CHECK_SIZEOF(CHealthInfo, 0x20)` (`include/MetroidPrime/CHealthInfo.hpp:40`).
- Caller `fn_8016FCB8` (0x8016FCB8, 0x94) re-read: `lbz r0,0x20(r3)` / `cmplwi` / `bne` -> `bl fn_8016FD4C`,
  `li r0,1`, `stb r0,0x20(r31)`; its else branch copies the same 0x20 bytes memberwise. The flag is at
  +0x20, so the earlier "0x24-byte holder" is an inference and is not repeated in the source comment.
- Twin comparison done by word, not by eye: `fn_80004D3C` (0x80004D3C, 0x20) is **identical in all eight
  words** to `fn_8016FD4C` (both `bl`s are +0x14, so even the `bl` word matches) - the earlier note's "apart
  from the `bl`" is only true of the *target*; `fn_80004D5C` (0x80004D5C, 0x28) differs from `fn_8016FD6C`
  in **one word, index 5** (the `bl`).
- Source file sha1 `597cb7b0eda17093b240fad7840d96e748d57ff5`; compile target reached 100% on the first
  correct (descending) ordering and stayed there through every gate.

### The port shim, measured both ways in this tree

`src/Kyoto/Alloc/PortMwccNew.cpp:85-100` defines `__ct__11CHealthInfoFRC11CHealthInfo` (placement-new copy of
the 0x20 bytes) and adds the `MetroidPrime/CHealthInfo.hpp` include. Nothing else hosts that name: `nm -A`
over all **813** objects of `build-port-link` names it in exactly two - `Carve8016FD4C.c.o` (`U`) and
`PortMwccNew.cpp.o` (`T`). Counterfactual, run then reverted (file sha1 back to
`2987a6652c6ffefbd6307a6d0f24e246a1e01f73`, `link_check.sh --strict` back to 291): with the definition
`#if 0`-ed out, **292 undefined (GREW)** and `NEW __ct__11CHealthInfoFRC11CHealthInfo`; with it, **291 =
baseline, STRICT PASS**. So the shim is worth exactly the one symbol the carve opens. No
`PortLinkStubs.cpp` duplicate existed for the MWCC name - that file's `stub_43` is the Itanium
`_ZN11CHealthInfoC1ERKS_`, which a `TARGET_PC` build never references (the copy is declared out of line
only under `MP_RETAIL_OUT_OF_LINE_COPIES && !TARGET_PC`, `CHealthInfo.hpp:12-15`).

### Not done, and why

- No `WALL:` - both functions reached 100.00%, so there is no sub-100% score to report.
- No `NEW:` - nothing measured this run shows another function of this neighbourhood reaching 100%;
  `fn_8016FC88` (0x30), `fn_8016FCB8` (0x94), `fn_8016FD94` (0xBC) and the rest of the two `auto_*` runs
  are unsourced without a measured twin.
- `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` appear modified in `git status`: that is
  `goal_check.sh`'s own gate step (`check_docs_claims.py --write`; `12685 -> 12687`, port link 291), not an
  edit by this lane. No other doc was touched.
