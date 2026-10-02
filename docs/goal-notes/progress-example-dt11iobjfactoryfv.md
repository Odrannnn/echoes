# progress-example-dt11iobjfactoryfv

`kind: progress`, `target: module:Blogg`. **Module matched_functions 8 -> 9 (of 217)**, the new
unit `Matching` at 100.00%, `All:` 13165 -> 13166 matched, linked 6245 -> 6246,
`total_functions` still 28465, DOL `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` unchanged, and the
module's sha1 `2def4cc1f6d7898d534976efe607da2cc09708f5` still equals `config/G2ME01/config.yml`
and is `cmp`-equal to `orig/G2ME01/files/RelProd/Blogg.rel`. `./tools/goal_check.sh
build/goal/item.json` -> `PASS` (`target rose: module:Blogg: 8 -> 9 / 217 functions`).

The item's target function is landed, and it is the worked answer the reason asked for: this is
the first unit in this tree whose body **stores a module-local vtable**, and `tools/twin_scan.py`
now reports it as the `rel_example` for 82 twins (`main/auto_03_80004798_text
__dt__9CGameModeFv` and 81 more) - for this shape there was none, which is what the item's reason
recorded.

## The function, and why it is written as a member

`.text 0x1B30..0x1B78`, 0x48 = 72 bytes, dtk's `fn_7_1B30`. It is the deleting destructor of one
of module 7's own polymorphic classes: `build/G2ME01/Blogg/asm/auto_04_00000000_data.s` has that
class's vtable at `.data:0x5A0`, 0x14 bytes, `{0, 0, fn_7_1B30, 0, fn_7_B798}`, and the body's
only `lis`/`addi` pair is the `stw r0,0(r31)` of that address (`build/G2ME01/Blogg/obj/
auto_00_00000108_text.o` relocated HA/LO to it). Two module functions install the same vtable as
the last store of a destructor, after destroying the two `CDamageVulnerability` members their
derived classes carry: `fn_7_0` (.text 0x0) and `fn_7_17A8` (.text 0x17A8). `fn_7_B798` (8 bytes,
`li r3,0; blr`) is a second virtual. The callee is `Free__7CMemoryFPCv`, not `__dl__FPv`.

**The declaration that produces the shape** - copy this, it is the whole recipe:

```cpp
#include "Kyoto/Alloc/CMemory.hpp"   // inline `operator delete` -> CMemory::Free; load-bearing

class CBloggVulnerabilityBase {
public:
  virtual ~CBloggVulnerabilityBase() = 0;
};

CBloggVulnerabilityBase::~CBloggVulnerabilityBase() {}
```

Two measured details, both cost a compile here:

1. **Without `Kyoto/Alloc/CMemory.hpp` the deleting destructor calls `__dl__FPv`**, and retail's
   relocation is `Free__7CMemoryFPCv`. The header's `inline void operator delete(void*) {
   CMemory::Free(ptr); }` is what makes the two agree; the same include is why
   `CAssetFactory.cpp`'s copy relocates to `Free__7CMemoryFPCv` and no source names it.
2. **The destructor is declared pure.** MWCC emits the deleting-destructor body either way, but it
   also emits the class's vtable as a *weak* symbol (`V __vt__…`, 0xC bytes in `.data`). The
   module's own `.data:0x5A0`, renamed below, is a strong definition of the same name, so the weak
   copy is overridden and `-strip_partial` drops it - measured, not assumed: `audit_rel_claim.py`
   reports `preplf 217 text symbols, plf 217, 0 dropped`, and `unit_fit.sh` prints
   `.data claimed - ours 12 <- NOT CLAIMED BY splits.txt; the bytes live in a neighbour`, which is
   this case and is correct here. Do **not** claim the vtable's `.data` range: its last word is
   `fn_7_B798`, a module function a unit of this size does not define.

## Config changes (intended list, not a copy from another tree)

- `config/G2ME01/rels/Blogg/splits.txt`: one claim added, `MetroidPrime/ScriptObjects/
  CBloggVulnerabilityBase.cpp: .text start:0x1B30 end:0x1B78`. dtk re-split the gap around it:
  `Blogg/auto_00_00000108_text` went 207 -> 31 functions (0x108..0x1B30) and a new
  `Blogg/auto_00_00001B78_text` took the remaining 175 (0x1B78..0xBEFC).
- `config/G2ME01/rels/Blogg/symbols.txt`: **two renames, both needed and both intended** -
  `.text:0x1B30` `fn_7_1B30` -> `__dt__23CBloggVulnerabilityBaseFv`, and `.data:0x5A0`
  `lbl_7_data_5A0` -> `__vt__23CBloggVulnerabilityBase`. The vtable's third word and the destructor
  have to be the same symbols in the link, and those are the names MWCC emits for the declaration
  above. The class name is **ours**: retail's REL carries no local names at all
  (`strings orig/G2ME01/files/RelProd/Blogg.rel` has no `__vt__`/`fn_` string, and the module's
  preplf names come from the DOL for imports and are synthetic for locals). Nothing else in the
  module refers to either old name, and the REL's import/export tables carry no strings, so the
  rename cannot move the module's bytes - the sha1 and `cmp` say it did not.
- `configure.py`: one `Object(Matching, "MetroidPrime/ScriptObjects/CBloggVulnerabilityBase.cpp")`
  added to the existing `Rel("Blogg", ...)` block, with the comment carrying this measurement.
- `files.cmake`: the new source listed, with an empty host branch (everything inside
  `#ifdef __MWERKS__`), as `CIngBoostBallGuardianBits.cpp` explains.

## Measured

```
sha1sum build/G2ME01/main.dol                    6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
86 RELs against config/G2ME01/config.yml         0 differ
Blogg.rel                                        2def4cc1f6d7898d534976efe607da2cc09708f5 == config.yml
Blogg.rel cmp orig/G2ME01/files/RelProd/         identical
our object .text 0x48 vs the linked .plf          identical (only the unlinked `bl` differs)
our .rel .text 0x1B30..0x1B78 vs retail's         identical, incl. the `bl` displacement 4800AA29
total_functions                                  28465 (unchanged)
All: 37.15% fuzzy, 30.58% matched, 13.51% linked 13166 / 28465   from 13165
goal_check.sh build/goal/item.json               PASS, target rose 8 -> 9 / 217
check_decl_order.py --unit Blogg                 ok, 4 units
unit_fit.sh CBloggVulnerabilityBase.cpp          .text claimed 72 ours 72 retail 72 fits; no extra functions
audit_rel_claim.py Blogg                         0 problems, 1/1 for the new unit, 0 of 217 dropped
check_symbol_names.py                            585 units, 0 missing
check_files_cmake.py                             clean
probe_sources.sh                                 849 files, 0 failed; link LINKED (286 undefined, 0 duplicates)
report.json Blogg/…CBloggVulnerabilityBase       1/1 functions, 72/72 bytes
```

## Follow-ups (not done here)

- `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` show in `git status` because
  `./tools/goal_check.sh` (the judge) rewrote their derived counts and the Blogg row from this
  tree's `build/report.json` - they are machine-made, not edits of mine.
- `fn_7_310C` (72 B, `.text` 0x310C, vtable `.data:0x594` = `{0, 0, fn_7_310C}`, 0xC) is the same
  shape with a *single* virtual slot, i.e. the `__vt__11IObjFactory` layout exactly; the recipe
  above applies with two more renames. Filed as a `NEW:` line below.
- `src/MetroidPrime/ScriptObjects/CBloggRel.cpp:17` still says `fn_7_0` "installs
  `lbl_7_data_5A0`". The statement is still true of retail's bytes but the symbol is now
  `__vt__23CBloggVulnerabilityBase`; I did not touch another lane's Matching unit for a comment.
- The two derived classes' destructors (`fn_7_0` at 0x0, `fn_7_17A8` at 0x17A8) install *their*
  vtables (`.data:0x540`, `.data:0x5B4`) and would each need their own class name and rename pair
  if a later lane claims them; they are the same recipe with more stores.

NEW: progress-twin-rel-blogg-310c | progress | module:Blogg | fn_7_310C (72 B, .text 0x310C) is the same deleting-destructor shape with its own module-local vtable at .data:0x594 ({0,0,fn_7_310C}); the worked declaration and the two symbols.txt renames are in docs/goal-notes/progress-example-dt11iobjfactoryfv.md
