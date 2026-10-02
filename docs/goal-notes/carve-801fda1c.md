# carve-801fda1c — `MetroidPrime/ScriptObjects/Carve801FDA1C`

**STATUS: DONE.** `fn_801FDA1C` (`.text 0x801FDA1C..0x801FDA54`, 0x38 = 56 bytes, 14 instructions)
and `fn_801FDA54` (`.text 0x801FDA54..0x801FDAA4`, 0x50 = 80 bytes, 20 instructions) are carved as
one `Matching` unit, `src/MetroidPrime/ScriptObjects/Carve801FDA1C.c`. objdiff reports
**100.00% fuzzy / 100.00% matched, 2 / 2 functions**, `tools/flip_test.sh` prints
`PASS -> kept as Matching`, and `./tools/goal_check.sh build/goal/item.json` prints
`goal_check: PASS carve-801fda1c` (matched 13046 -> 13048, linked 6149 -> 6151).

## The four files of the carve, and the source

| file | change |
| --- | --- |
| `config/G2ME01/splits.txt` | `MetroidPrime/ScriptObjects/Carve801FDA1C.c:` `.text start:0x801FDA1C end:0x801FDAA4` |
| `configure.py` | `Object(Matching, "MetroidPrime/ScriptObjects/Carve801FDA1C.c")` between `Carve801FD924.cpp` and `Carve801FDAA4.c` |
| `files.cmake` | `src/MetroidPrime/ScriptObjects/Carve801FDA1C.c` in the same position |
| `src/MetroidPrime/ScriptObjects/Carve801FDA1C.c` | new; `fn_801FDA54` first, `fn_801FDA1C` second (**descending by address**) |

The two bodies are the shapes this tree already matches, and each was confirmed word for word with
objdump against retail's own bytes in `build/G2ME01/main.elf` before the claim existed (dtk's
`build/G2ME01/asm/auto_03_801FD998_text.s` carries the same two `.fn` blocks):

- `fn_801FDA1C` is **14 of 14 words identical to `fn_801FBD30`** (0x801FBD30, 0x38,
  `ScriptObjects/Carve801FBC58.c`, `Matching`), the `bl` word `48 00 00 15` included, because the
  callee sits 0x14 bytes past the `bl` in both; and 14 of 14 to `fn_801FDBE0` (0x801FDBE0, 0x38,
  `ScriptObjects/Carve801FDB5C.c`, `Matching`) as well. Its body is the by-value struct-parameter
  copy the `bl` forces (`lwz r5,0(r4)` / `addi r4,r1,8` / `lwz r0,0(r3)` / `addi r3,r1,0xc` /
  `stw` x2), spelled here as a forwarder taking the iterator struct by value. That spelling is the
  one compiled here (flip_test PASS); `Carve801FBC58.c`'s `const void*` spelling is the other
  measured route to the same 14 words, and it was not compiled in this run.
- `fn_801FDA54` is **19 of 20 words identical to `fn_801FDC18`** (0x801FDC18, 0x50,
  `ScriptObjects/Carve801FDB5C.c`, `Matching`), the one difference the element stride
  (`addi r31,r31,0x2c` here, `0x30` there); and 19 of 20 to `fn_801FD5E8` (0x801FD5E8, 0x50,
  `ScriptObjects/Carve801FD5E8.c`, `Matching`), differing only in `addi r31,r31,0x24`. The `bl`
  word `48 00 00 2d` is the same in all three, since each callee sits 0x2C past its own `bl`.
  Body: `char* cur = begin.current; while (cur != end.current) { fn_801FDAA4(cur); cur += 0x2C; }`
  with both parameters the one-pointer iterator struct by value.
- Its one callee, `fn_801FDAA4` (0x801FDAA4, 0x20), is defined for real by the already-`Matching`
  `ScriptObjects/Carve801FDAA4.c`, whose `.text` starts exactly where this claim ends. **No stand-in
  was needed and none was added**; the 0x2C stride is the element size that file measured from this
  very function's callers (`Carve801FDAA4.c:43-50`).
- Both claim boundaries are function edges: `0x801FDA1C + 0x38 = 0x801FDA54`,
  `0x801FDA54 + 0x50 = 0x801FDAA4`; in front of the claim `fn_801FD998` (0x801FD998, 0x84) is still
  unclaimed and stays dtk's, behind it `fn_801FDAA4` is another unit's claim.

## Verified (commands and their output, this run)

- `./tools/decomp_build.sh main/MetroidPrime/ScriptObjects/Carve801FDA1C` ->
  `main/MetroidPrime/ScriptObjects/Carve801FDA1C: 100.00% fuzzy, 100.00% matched (2 / 2 functions)`,
  `All: 36.98% fuzzy, 30.42% matched, 13.41% linked (13048 / 28465 functions)`, `87 files OK`.
- `sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (retail).
- `./tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FDA1C.c` ->
  `PASS -> kept as Matching`, `kept: 1 / 1 failed: 0 skipped: 0`.
- `./tools/unit_fit.sh MetroidPrime/ScriptObjects/Carve801FDA1C.c` ->
  `.text claimed 136 ours 136 retail 136 fits`, `no extra functions`.
- `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/Carve801FDA1C` ->
  `ok: 1 unit(s) checked, none emits its functions out of retail order`.
- `python3 tools/check_symbol_names.py` -> `checked 578 units; 0 declared names are missing`.
- `python3 tools/check_files_cmake.py` -> `every configured DOL object is either in files.cmake or
  excluded with a reason`, `0 on-disk sources are in no manifest at all`.
- `./tools/goal_check.sh build/goal/item.json` -> `PASS`, with `gate.sh` ok (DOL sha1, 86 RELs,
  per-function diff, wiring, docs claims, probe), `counts: matched 13046 -> 13048 linked 6149 ->
  6151`, and `flip_test ... PASS, Object(Matching) in configure.py`. Gate's per-function diff:
  `LINKED main/MetroidPrime/ScriptObjects/Carve801FDA1C`, `+100%` for both functions,
  `MOVED main/auto_03_801FD998_text :: fn_801FDA1C -> ...` and the same for `fn_801FDA54`,
  `no regression`.
- Gate port steps: probe `824 files, 0 failed, 0 errors; link: LINKED (286 undefined, 0
  duplicates)`; port link gap `280 MISSING, all accounted for`; `link_check: unique undefined
  symbols 286, duplicate definitions 0` — **unchanged** from the judge's baseline
  (`build/goal/judge/undef.base.count` = 286). Nothing was retired and nothing was opened.

## Where the item's reason does not describe this tree

- It says the two `.fn` blocks are in dtk's `auto_03_801FDA1C_text.s` "which this item's split
  created". **That file does not exist here.** The unclaimed range in this tree is
  `build/G2ME01/asm/auto_03_801FD998_text.s` (0x801FD998..0x801FDAA4, three `.fn` blocks:
  `fn_801FD998`, `fn_801FDA1C`, `fn_801FDA54`); the reason was written by `carve-801fd998` in a
  tree that carried its own split at 0x801FD998. After this carve that unit holds exactly one
  function, `fn_801FD998`, and both of ours moved to the new unit.
- It says the carve "retires `stub_801fd998_0`". **No such symbol exists in this tree** — `grep -rn
  stub_801fd998` matches only `build/goal/item.json`, and `src/MetroidPrime/PortLinkStubs.cpp` has
  no stand-in for `fn_801FDA1C` or `fn_801FDA54`. Consistent with that, the port's undefined count
  did not move (286 -> 286, 0 duplicates). The stub was a plan of `carve-801fd998`'s tree, not a
  thing this item could retire.

## For the next lane

- **`carve-801fd998` (`fn_801FD998`, 0x801FD998, 0x84) is still the queued next step and got
  cheaper:** its callee `fn_801FDA1C` is now ours (`Matching`, this item), so the "one stand-in for
  its callee `fn_801FDA1C`" that its NEW line budgets for
  (`docs/goal-notes/carve-801fd924.md:166`) is no longer a cost — its only remaining callee,
  `fn_801FDAA4`, is ours too. Not re-filed: that NEW line is already in the queue.
- `fn_801FD6F0` and `fn_801FD4B0`/`fn_801FDCAC` are the other unclaimed neighbours; each already has
  a `NEW:` line (`docs/goal-notes/carve-801fd67c.md:168`, `.../carve-801fd998.md:164`,
  `.../carve-801fdae8.md:1044`), so none was filed again here.
- `./tools/goal_check.sh` rewrites `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` (it runs
  `gate.sh` with `MP_GATE_DOCS_WRITE=1`). Both were reverted with `git checkout --` after the last
  PASS, so the tree carries only the four carve files above; the derived counts are re-derived by
  the judge on its own run.
- No `WALL:` — the unit flipped whole, on the first spelling tried.
