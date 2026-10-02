# progress-example-destroyq24rstl116pointeriter — `module:DigitalGuardian` (item: worked answer for the `rstl::destroy(It, It)` shape)

**Outcome: PASS.** `./tools/goal_check.sh build/goal/item.json` printed
`goal_check: PASS progress-example-destroyq24rstl116pointeriter`. Module matched functions
`25 -> 26 / 420` (`ok target rose`); matched overall `13186 -> 13187`, linked `6245 -> 6246`;
no asm added; gate.sh clean.

## What was done

`fn_14_6144` (DigitalGuardian `.text` 0x6144..0x617C, 0x38 = 56 bytes,
`config/G2ME01/rels/DigitalGuardian/symbols.txt:112`, previously inside dtk's
`auto_00_0000010C_text`) is now a **`Matching` unit of its own**, carved in the four files a
carve takes:

- `src/MetroidPrime/ScriptObjects/DigitalGuardianDestroy.cpp` — new, the only function.
- `config/G2ME01/rels/DigitalGuardian/splits.txt` — `DigitalGuardianDestroy.cpp: .text
  start:0x00006144 end:0x0000617C`.
- `configure.py` — `Object(Matching, "MetroidPrime/ScriptObjects/DigitalGuardianDestroy.cpp",
  mw_version="GC/2.7")` in the `Rel("DigitalGuardian", ...)` block.
- `files.cmake` — `src/MetroidPrime/ScriptObjects/DigitalGuardianDestroy.cpp` (guarded body; see
  below).

The claim is exactly the one function: `fn_14_60C0` (0x60C0, 0x84) ends where it starts and
`fn_14_617C` (0x617C, 0x60) starts where it ends; both neighbours are unclaimed, so dtk fills
them from retail. `tools/audit_rel_claim.py DigitalGuardian` reports
`DigitalGuardianDestroy.cpp 0x00006144..0x0000617C 1/1 functions ok` and
`0 claim(s) with a problem`.

## The declaration that produced the shape (copy this)

```cpp
extern "C" {
#ifdef __MWERKS__
struct SDigitalGuardianIterator {
  void* current;
  SDigitalGuardianIterator(void* p) : current(p) {}
};
void fn_14_617C(SDigitalGuardianIterator begin, SDigitalGuardianIterator end);
void fn_14_6144(SDigitalGuardianIterator begin, SDigitalGuardianIterator end) {
  fn_14_617C(begin, end);
}
#endif
}
```

It is `rstl::destroy(It, It) { destroy_impl(begin, end); }`
(`include/rstl/construct.hpp:111-114`) instantiated for this module's iterator and this module's
own `destroy_impl`. Two one-pointer class values arrive by value, each is copied into the frame
once, and the addresses of those two copies go to the callee.

## Measurements (all this run)

- **The twin, byte-compared**: DOL 0x800067A8, size 0x38, `destroy<Q24rstl116pointer_iterator<
  11CTweakValue,Q24rstl48vector<...>>>__4rstl...` (`config/G2ME01/symbols.txt:128`), Matching in
  `main/MetroidPrime/main`. `python3 tools/dol_read.py 0x800067A8 0x38` is these 14
  instructions with only the `bl` displacement different. `src/MetroidPrime/ScriptObjects/
  Carve801FD52C.cpp`'s `fn_801FD5B0` is the same 14 instructions again (DOL, Matching).
- **Compiler version is the whole of the remaining difference.** Compiled with the module's own
  command line, 0x38-byte object, one defined symbol:
  - `GC/1.3.2`: `stwu / mflr / **stw r0,0x14(r1)** / lwz r5,0x0(r4) / lwz r0,0x0(r3) / ...` — wrong.
  - `GC/2.7`: `stwu / mflr / lwz r5,0x0(r4) / **stw r0,0x14(r1)** / ...` — retail, byte for byte.
  Hence the per-object `mw_version="GC/2.7"` (the same override `CLumiteRelTail.cpp` needs for
  `fn_39_738`, `configure.py:1684`). No spelling moves that store.
- **Spellings measured** (GC/2.7, same body): class with a one-argument constructor =
  byte-exact (used); POD struct with no constructor = byte-exact too; named locals (`SIt b =
  begin; SIt e = end; callee(b, e);`) = 0x40 bytes with four `stw`s, wrong; a `void*`/`void**`
  parameter pair = the two `addi`s and the two `stw`s vanish. So the *class type* is what makes
  the by-value pass go through memory; the constructor is not required.
- **Host side**: the body is `#ifdef __MWERKS__`-only (the `CLumiteRelTail.cpp` arrangement),
  because `fn_14_617C` is unclaimed retail code nothing on the host defines. Measured: `g++ …
  -DTARGET_PC -include platform/compat.h -c src/…/DigitalGuardianDestroy.cpp` then `nm` on the
  object prints **nothing** (no defined and no undefined symbol). The gate's probe confirms the
  port is untouched: `probe: 849 files, 0 failed, 0 errors; link: LINKED (286 undefined,
  0 duplicates)`.
- **The module's verdict**: `build/G2ME01/DigitalGuardian/DigitalGuardian.rel` sha1
  `a3798856ec6b175272529f6a6295a29140662bcc` == `config/G2ME01/config.yml`, and the file is
  `cmp`-equal to `orig/G2ME01/files/RelProd/DigitalGuardian.rel`. DOL sha1 still
  `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`. `tools/audit_forceactive.py DigitalGuardian` /
  `check_symbol_names.py` report `0 dropped by -strip_partial` and 0 missing names, so the new
  function survives the link (it is reached by `fn_14_60C0`'s `bl`, in the auto object).
- **`build/report.json` after**: unit `DigitalGuardian/MetroidPrime/ScriptObjects/
  DigitalGuardianDestroy`, `complete: true`, 1/1 functions, 56/56 bytes, fuzzy 100.00%,
  `.text` virtual address 24900 = 0x6144. The auto unit it came out of re-split:
  `auto_00_0000010C_text` 211 functions -> `auto_00_0000010C_text` 95 + the new
  `auto_00_0000617C_text` 115.
- **It is now the worked answer the item wanted**: `tools/twin_scan.py`'s `rel_example` for this
  shape resolves to `DigitalGuardian/MetroidPrime/ScriptObjects/DigitalGuardianDestroy` /
  `fn_14_6144`, for **63** unmatched copies of the shape (38 of them in REL modules, 2016 bytes;
  the rest DOL) — the `twin_scan.py rel_example` pointer the item promised.
- **`unit_fit.sh`**: `.text claimed 56 ours 56 retail 56 fits`, "no extra functions".
  `check_decl_order.py --unit MetroidPrime/ScriptObjects/DigitalGuardianDestroy.cpp`: nothing to
  reorder (one function).

## Notes for the next lane

- The `#ifdef __MWERKS__` guard is not optional while `fn_14_617C` has no host definition: an
  unguarded body adds one undefined name to the port link and `link_check.sh --strict` fails.
  The alternative is to claim 0x6144..0x61DC and write `fn_14_617C` too (it is the module's
  `destroy_impl`, a 0x68-stride loop calling `internal_dereference__Q24rstl66basic_string…`), but
  that is a second function and a second measured body, not this item.
- `tools/audit_forceactive.py <Module>` is the cheap pre-check for a candidate carve: it prints
  every claimed range's functions and which of them are NOT in the module's FORCEACTIVE list.
  Not being listed did not drop anything here (the caller's `bl` keeps the function), but
  `RUNNING_THE_DECOMP.md` #3 is the case where it does.
- Files touched: `src/MetroidPrime/ScriptObjects/DigitalGuardianDestroy.cpp` (new),
  `config/G2ME01/rels/DigitalGuardian/splits.txt`, `configure.py`, `files.cmake`. `total_functions`
  is still 28465.
