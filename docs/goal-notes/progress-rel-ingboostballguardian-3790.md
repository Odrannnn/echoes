# progress-rel-ingboostballguardian-3790

`kind: match`, `target: IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardian3790`.
**Module 30 matched_functions 54 -> 57 (of 318)**, the new unit `Matching` at **3 / 3,
100.00% fuzzy and 100.00% matched**, and the project's **matched 13456 -> 13459, linked
6504 -> 6507**, `total_functions` still 28465. DOL sha1 `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`,
`IngBoostBallGuardian.rel` `sha1sum` `956265e8ccf3f489e9cb3a70ec22d0357ce17cca` equal to
`config/G2ME01/config.yml` **with this object in the link**, all 86 RELs `cmp`-equal to
`orig/G2ME01/files/RelProd/` (`87 files OK`). `probe_sources.sh` **906 files, 0 failures**, link
`LINKED (286 undefined, 0 duplicates)`, `check_symbol_names.py` **0 missing of 585 units**,
`unit_fit.sh` **`.text claimed 252 / ours 252 / retail 252, fits`** with no extra functions,
`check_decl_order.py --unit` **ok: 1 unit(s) checked**, `nm -n` in retail order
(`fn_30_3790` 0x0, `fn_30_37E0` 0x50, `fn_30_3838` 0xA8).
`./tools/flip_test.sh IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardian3790.cpp`
-> **PASS** (kept as Matching) and `./tools/goal_check.sh build/goal/item.json` -> **PASS** with
every line `ok`.

## The carve: five files, all of it in one change

- `src/MetroidPrime/ScriptObjects/CIngBoostBallGuardian3790.cpp` (new, three functions, 0xFC bytes)
- `config/G2ME01/rels/IngBoostBallGuardian/splits.txt` - `.text start:0x00003790 end:0x0000388C`,
  keyed `IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardian3790.cpp:`
- `configure.py` - one `Object(Matching, ..., source=...)` in the existing
  `Rel("IngBoostBallGuardian", ...)` block
- `files.cmake` - the source path, with an empty host branch
- `config/G2ME01/config.yml` - **`force_active: [fn_30_3790]`** on module 30, see below

The module prefix on the object name and the `source=` are the `CIngBoostBallGuardian388C` entry's
convention and are load-bearing: `tools/goal_check.sh` resolves a `match` target by searching
`configure.py` for the queue's spelling. The entry sits **after** the 388C entry so that the extra
close paren in that entry's comment has already balanced the count `flip_test.sh`'s `unit_info`
walks from the last call of its helper - measured, the count for this entry is 216 `(` against
218 `)`, and without the balance `unit_info` looks under `extern/musyx/src/` and declines.

**No `mw_version` override**, unlike 388C: measured with the module's own cflags, GC/1.3.2 (the
module default, which the other fifteen module-30 units build under) and GC/2.7 produce the same
252 bytes. These bodies are branch and load/store only, with nothing of the two-deep float pipeline
that made 388C version-sensitive.

## What the three functions are

One null-guarded deleting-destructor chain, and the shape is the one
`src/MetroidPrime/Carve8000447C.cpp:193` and `src/MetroidPrime/Cameras/Carve801E7C14.c:98` already
reproduce in the DOL: `if (self) { <teardown>; if (flag > 0) Free__7CMemoryFPCv(self); } return self;`

- the flag is a **`short`** - retail's `extsh. r0,r31 / ble` with the flag held in r31 across the
  inner call; an `int` would give `cmpwi r31,0`;
- all three **return their receiver**, which is the `mr r3,r30` inside the epilogue;
- `fn_30_3790` releases the `rstl::string` at +0, `fn_30_37E0` the sub-object at +4 on
  `addi r3,r30,4 / li r4,-1`, and `fn_30_3838` the pointer at +0xC (`lwz r3,0xC(r30)` then
  `bl Free__7CMemoryFPCv`, so +0xC is owned outright - an `rstl::rc_ptr` release branches first).

## The finding that reproduces `fn_30_3790`: the branch target, not the spelling

Retail's `mr. r30,r3 / beq` lands on the **epilogue**, so the one null test guards the teardown
*and* the flag test. An explicit destructor call does not give that: `p->~T()` emits its own null
test whose `beq` skips **only** the `bl`, and the two tests together are 0x54 bytes against
retail's 0x50. Retail's inner call is therefore an **unguarded member call**, not a destructor
call, and the source has to say so. Measured, against `rel[0x3790+0xDC : 0x37E0+0xDC]`:

| spelling | bytes | |
|---|---|---|
| `self->~basic_string<char>()` | - | mwcceppc: "template parameter/argument list mismatch" |
| `self->~rstl::string()` | - | mwcceppc: "expression syntax error" |
| `(*self).~basic_string<char>()` | - | mwcceppc: same mismatch, on the `.` form |
| `struct D : rstl::basic_string<char> {}` + `self->~D()` | 0x54 | one class level per test, two `beq` |
| `S& r = *self; if (self) { r.~S(); ... }` | 0x54 | a reference does not drop the test |
| `S* p = self; p->~S();` | 0x54 | |
| **`typedef rstl::basic_string<char> S;` + `self->internal_dereference();`** | **0x50, byte-exact** | |

`internal_dereference` is private in `include/rstl/string.hpp:115`, so the last row is reached the
way `Carve8000447C.cpp:96-127` reaches `rstl::rc_ptr`'s: **declare the class template locally, with
only the member the bytes call, and never define it.** `rstl::char_traits` and
`rstl::rmemory_allocator` are declared alongside because the mangled name spells the template
arguments. The proof that the name came out right is the built object's `.rela.text`, which reads
`internal_dereference__Q24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>Fv`
- retail's `config/G2ME01/symbols.txt:13842`, 0x802FE9B8 - and which resolves against the DOL in
the module link. Nothing is emitted for the declaration, no `rstl` header is included, and
`rstl_strings.cpp` still owns the only definition. `rstl::destroy(p)` was also measured and is the
same 0x54, so it is not the answer either. `mwcceppc` has **no `__asm__` symbol renaming**:
`extern void f(void*) __asm__("<retail's name>");` is read as the global-register-variable extension
and fails with "type cannot be made into a global register variable" (measured), so declaring the
mangled name directly is not available.

## The finding that broke the module: `fn_30_3790` is dead-stripped

With the carve in and no `force_active` entry, **the module's sha1 broke** while the unit itself
read 3/3 at 100%. `dtk rel info` on both `.rel`s gave `.text` **0x16698 against retail's 0x166E8**,
80 bytes short, and the byte stream from 0x3790 on was retail's from 0x37E0 on: mwldeppc dropped
`fn_30_3790` and everything after it shifted. `nm` over every object dtk writes into
`build/G2ME01/IngBoostBallGuardian/obj/` shows **no `U fn_30_3790`** - the chain is entered from
nowhere in the module - and it was in no FORCEACTIVE list. `config/G2ME01/config.yml`'s module-30
block now carries `force_active: [fn_30_3790]`, which is where mwldeppc's FORCEACTIVE comes from,
the same fix FlyerSwarm's `fn_21_1708` and PlantScarabSwarm's `fn_49_2C00` use there. Re-measured
after: `nm` on the new `ldscript.lcf` finds `fn_30_3790` once and `87 files OK`.

`fn_30_37E0` and `fn_30_3838` need nothing, and the reason is worth recording because it is not the
obvious one: `fn_30_37E0` has four `U` references from dtk's own objects, and `fn_30_3838` has
**zero** - it is live only through the reference `fn_30_37E0` now makes from our own object, which
dead-strip follows. So a claim can be safe with no `U` anywhere.

This is **not** the only dead-strip hazard in the module - 170-odd of its functions carry no `U`
reference from any dtk object - but it is the only one among the small unclaimed ones. A census of
the unclaimed, relocation-free module-30 functions of 0x60 bytes or less (measured over the
`.rela.text` of `build/G2ME01/IngBoostBallGuardian/obj/auto_*.o` and the module's `symbols.txt`)
leaves two worth a lane, `fn_30_10F40` (0x10F40, 0x5C) and `fn_30_33B0` (0x33B0, 0x5C), plus a
scatter of 4..0x14-byte stubs (`fn_30_4028` 0x8, `fn_30_B764` and `fn_30_B768` 0x4 each,
`fn_30_BAA8` / `fn_30_BC20` / `fn_30_BE88` / `fn_30_E4E0` 0xC each, `fn_30_C05C` 0x14). There is
**no** leaf multi-function run under 0x120 bytes left, which is why no second NEW from the
destructor shape is filed.

## Two corrections

1. **The item's `reason` says 0x3790..0x388C is "0xAC bytes". It is 0xFC = 252** (0x388C - 0x3790);
   0xAC is 0x37E0..0x388C. The three sizes are 0x50 + 0x58 + 0x54 = 0xFC and each was measured.
2. **`build/report.json` was stale when this item started.** It read `matched_functions 13459 /
   complete_units 930`, which is neither the branch head nor this change. The judge baseline
   `build/goal/judge/report.base.json`, recorded by `run_goal.sh` on HEAD `2bdb9ddc`, says
   **13456 / 930**, and a clean `git checkout`-then-`decomp_build.sh -r` of this worktree
   reproduced **13456** exactly. Anyone reading the leftover report to decide whether a count rose
   will read a rise that did not happen; `goal_check.sh`'s own baseline is the one to quote.

## For the next run in module 30

The deleting-destructor spelling above is settled: `if (self) { <teardown>; if (flag > 0)
Free__7CMemoryFPCv(self); } return self;`, the flag a **`short`**, the teardown an **unguarded**
call, and any function the module does not call needs a `force_active:` entry in
`config/G2ME01/config.yml` **in the same change** or the module's sha1 breaks with the unit still
at 100%. For a private member of a retail template, declare the template locally rather than
calling the destructor.

NEW: progress-rel-ingboostballguardian-10f40 | match | IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardian10F40 | fn_30_10F40 (0x10F40, 0x5C) is unclaimed, has no relocation in its byte range and is 4x unrolled (measured on build/G2ME01/IngBoostBallGuardian/obj/auto_00_0000D2E0_text.o at section offset 0x3C60): `lwz r6,0(r3) / lwz r0,0(r4) / b` into a body copying a 0x1C-byte record (lfs/stfs at +0 and +4, lwz/stw at +8..+18, `addi r6,r6,28 / addi r5,r5,28`, `cmplw r6,r0 / bne`), returning r5 - so it is one element copy in a free extern "C" function with an explicit 4x unroll, which is the spelling that reaches a plain one-register schedule (unlike fn_30_388C, this needs no GC/2.7). fn_30_33B0 (0x33B0, 0x5C) is unclaimed and relocation-free too and is the member-wise copy the progress-rel-ingboostballguardian-fn30f78 note describes - the module-30 leaf census is otherwise empty below 0x60 bytes, so these two are what is left of that shape
