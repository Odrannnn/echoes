# progress-twin-rel-sandboss

**Result: PASS.** `goal_check.sh build/goal/item.json` -> `PASS progress-twin-rel-sandboss`.
`module:SandBoss` matched functions **24 -> 32** (of 322); project `matched_functions`
**13098 -> 13106**, `linked` **6196 -> 6204**; the module's sha1 is unchanged
(`0a28cddbe1a04c8c4ca2bf6a6750d0467b9e805e`, `build/G2ME01/SandBoss/SandBoss.rel`), all 87
checksums OK, `ninja` exit 0.

## What landed

One new `Matching` unit, `MetroidPrime/ScriptObjects/CSandBossRelTail.cpp`, claiming **.text
0x10548..0x1073C** - eight functions, all 8/8 at 100.00% in `build/report.json`:

| retail | fn | size | what it is |
| --- | --- | --- | --- |
| 0x10548 | `fn_55_10548` | 0x40 | two-member copy; second member copy-constructed at +0x4 |
| 0x10588 | `fn_55_10588` | 0x3C | deleting destructor of a memberless object |
| 0x105C4 | `fn_55_105C4` | 0x40 | copy of the class whose only data is the flag byte at +0x30 |
| 0x10604 | `fn_55_10604` | 0x20 | forwarder to `fn_55_10624` |
| 0x10624 | `fn_55_10624` | 0x28 | null-guarded forwarder to `fn_55_106E0` |
| 0x1064C | `fn_55_1064C` | 0x3C | deleting destructor of a memberless object |
| 0x10688 | `fn_55_10688` | 0x58 | the 0x2C-byte payload's constructor |
| 0x106E0 | `fn_55_106E0` | 0x5C | the payload copy (member-for-member) |

Files: `src/MetroidPrime/ScriptObjects/CSandBossRelTail.cpp` (new),
`config/G2ME01/rels/SandBoss/splits.txt` (the claim), `configure.py` (one `Object(Matching, ...)`
with `mw_version="GC/2.7"`), `config/G2ME01/config.yml` (`force_active`), `files.cmake` (one line).

## Three things that had to be measured, in the order they blocked

1. **`mw_version` is the whole of the first two functions' diff.** Under the module's default
   **GC/1.3.2**, `fn_55_10548` is **62.5%** and `fn_55_105C4` is **77.5%**: retail schedules the
   argument setup (`lhz`/`lbz`) *in front of* the `stw r31,0xc(r1)` prologue save and 1.3.2 puts
   the save first. Six spellings of `fn_55_105C4` under 1.3.2 - `store`-then-`test`, `test`-then-
   `store`, a local flag, a `bool` member, `void` with no return, `return self` - all landed at
   36.125/45.25/56.25/75.0/77.5%, none at 100. **Under `mw_version="GC/2.7"` both are 100.00%
   unchanged, no spelling change at all.** This is the same finding `CLumiteRelTail.cpp` records
   for `fn_39_738`, so it is a property of this module family's tail, not of one function: the
   out-of-line instantiation blocks were compiled by the later compiler. **Try 2.7 before
   spending spellings on a REL tail that only differs in prologue scheduling.**
2. **The two destructors are dead-stripped.** With everything at 100.00% the module still linked
   **0x78 bytes short**: nothing in the module calls `fn_55_10588` or `fn_55_1064C`, so mwldeppc
   drops them and every function after them shifts. `config/G2ME01/config.yml`'s per-module
   `force_active:` list is the fix (dtk 1.8.4), exactly as `docs/RUNNING_THE_DECOMP.md` records
   for `ForgottenObject`; both names are listed there now. Verified by the sha1, not by objdiff -
   every function was 100.00% while the module was 120 bytes short.
3. **`check_files_cmake.py` fails a `Matching` unit that is neither in `files.cmake` nor in its
   `EXCLUDED` list**, and only a `RELMain`/`RELExit` unit is exempt (the SandBoss head is exempt
   for that reason; `tools/` is not editable from a goal item). The fix is the arrangement
   `CLumiteRelTail.cpp` uses: the bodies are inside `#ifdef __MWERKS__` with an empty host branch,
   so listing the file adds no undefined reference. Confirmed by the gate's own
   `check_files_cmake.py` run and by `goal_check`'s gate step.

## The twin list, judged

The 81 listed twins were a useful map but not a work list: the six that landed here
(`fn_55_10548`, `fn_55_10588`, `fn_55_10604`, `fn_55_10624`, `fn_55_1064C`, and `fn_55_106E0`'s
shape) are all one container instantiation. `fn_55_106E0`/`fn_55_10688` had **no** twin and were
written from the bytes; both needed the two adjacent words copied **in reverse source order**
(`x24` before `x20`), which is what retail's `lwz 0x24 / lwz 0x0 / stw 0x24 / stw 0x20` shows.

## Left for the next run (measured, not attempted)

- **0x1073C..0x107B0**: `fn_55_1073C` (0x24) and `fn_55_10760` (0x50). `fn_55_10760` reads
  `lbl_55_rodata_B8` (module rodata, still retail's) and `sRightVector__9CVector3f` (DOL), so the
  claim would carry a data relocation; the shape is otherwise the same float/word copy.
- **0x10AE8..0x10C78**: four contiguous destructors, `fn_55_10AE8` (0x60), `fn_55_10B48` (0x7C),
  `fn_55_10BC4` (0x54), `fn_55_10C18` (0x60), with twins `__dt__18CErrorOutputWindowFv`,
  `__dt__15CGameProjectileFv` and `__dt__Q24rstl36vector<i,...>Fv`. These tear down members
  (vtables, `rstl` containers) so they need the class layouts, not just a spelling.
- **0x108E4** (0x204, `CPlasmaProjectile`'s destructor, twin `__dt__17CPlasmaProjectileFv` in
  `src/MetroidPrime/TypesMatch.cpp`) is the biggest single item left in this block; its callees
  are module-local copies (`fn_55_10BC4`, `fn_55_10AE8`), so it wants those two first.

No `WALL:` line: nothing was left at a sub-100% score, and the one function that looked like a
scheduling wall (77.5% over six spellings) was the compiler version, not the source.

`NEW:` - none filed. The three findings above are lessons (notes), and the remaining work is the
same target (`module:SandBoss`) this item already names.

Note for the driver: `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` show as modified in
`git status` - that is `goal_check.sh`'s own `MP_GATE_DOCS_WRITE=1 ./tools/gate.sh` rewriting the
derived counts, not an edit of mine.
