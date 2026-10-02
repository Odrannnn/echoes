# carve-801fd8e0

**Kind:** `match` **Target:** `MetroidPrime/ScriptObjects/Carve801FD8E0` — **PASS**

New `Matching` unit `src/MetroidPrime/ScriptObjects/Carve801FD8E0.c`, claiming
`.text 0x801FD8E0..0x801FD924` (0x44 = 68 bytes, 2 functions): `fn_801FD8E0` (`symbols.txt:8279`,
0x20, 8 instructions) and `fn_801FD900` (`symbols.txt:8280`, 0x24, 9 instructions). Definitions are
descending by address. `tools/flip_test.sh` PASS, `tools/goal_check.sh` exit 0.

## The two functions, and what fixes their shape

`rstl::destroy`'s two halves for a 0x24-byte element. `fn_801FD8E0` is a one-call forwarder to
`fn_801FD900`, which materialises `li r4,-1` and calls the element's deleting destructor
`fn_801FD924` (0x801FD924, 0x74, **unclaimed**). The twins the item named are in the already
`Matching` `src/MetroidPrime/Player/Carve80004C4C.c`: `fn_80004D3C` (`destroy`, 0x20) and
`fn_80004C6C` (`destroy_impl`, 0x24). Measured against the built object
`build/G2ME01/obj/MetroidPrime/Player/Carve80004C4C.o`: **7 of 8 words identical** for the first
pair, **8 of 9** for the second, the one differing word in each being the `bl`. So the body is the
twin's body and nothing else: `void fn_801FD900(void* self) { fn_801FD924(self, -1); }` and
`void fn_801FD8E0(void* self) { fn_801FD900(self); }`.

Callers, measured from the bytes: `fn_801FD890` (0x801FD890, 0x50, unclaimed) calls `fn_801FD8E0`
at 0x801FD8B4 with an `addi r31,r31,0x24` stride, and the `Matching` `Carve801FF720.cpp`'s
`fn_801FF7EC` calls it at 0x801FF810 (`mr r3,r31` / `bl`, r4 dead at that site). Those two fix the
element size at 0x24 and the argument list at **one** pointer - the callee is not a two-argument
forwarder, even though r4 survives untouched.

## Byte check against retail, not against the relinked `main.elf`

`build/G2ME01/main.elf` holds **our** bytes after a flip, so a comparison against it is circular (the
trap written up in `docs/goal-notes/carve-801fdb5c.md`). Read the retail disc instead -
`orig/G2ME01/sys/main.dol`, text section at 0x801FD8E0..0x801FD924 - and compare with the object's
`.text`: **66 of 68 bytes identical**, the two exceptions being the two `bl` words
(`48 00 00 15` retail vs `48 00 00 01` + relocation in the object). Both decode to the right targets:
0x801FD8EC + 0x14 = 0x801FD900, 0x801FD910 + 0x14 = 0x801FD924. The linked result is what
`flip_test` confirms.

`tools/carve_diff.sh 0x801FD8E0 0x44 build/G2ME01/obj/.../Carve801FD8E0.o` says `NOT byte-exact` on
this tree, and the two instructions it names are exactly those two `bl`s - it reads `main.elf` for
"retail" (which is us) and the object still carries unresolved relocations. Read that verdict as an
artefact of the tool's inputs, not as a mismatch; `flip_test` is the acceptance test.

## The PortLinkStubs duplicate, and the callee that needed a stub

- `stub_184` (`fn_801FD8E0`) existed for `Carve801FF720.cpp`; the carve now defines that symbol for
  the port's link too, so it is **retired** (rule 3 of the carve vein). The blocker comment at
  `stub_183` was rewritten to cover `fn_801FEA98` alone, with the pair's 293-undefined measurement
  annotated as historical rather than restated.
- The carve's one new callee `fn_801FD924` gets `stub_195` (new, empty stand-in, nothing else
  references it). Measured, with the block removed: `python3 tools/link_gap.py --rebuild` exits 1,
  `286  MISSING`, `gap grew: fn_801FD924 is not in port_link_gap_list.md`; with it in place: exit 0,
  `285  MISSING symbol(s), all accounted for in port_link_gap_list.md`. The file header's count stays
  163 functions / 31 unmangled fn_-lbl_ (an exchange), which the header now records.

## Verification (measured on this tree)

- `./tools/goal_check.sh build/goal/item.json` -> exit 0, `PASS carve-801fd8e0`
- counts: matched **12667 -> 12669**, linked **6026 -> 6028**;
  `All: 35.53% fuzzy, 29.37% matched, 13.04% linked (12669 / 28465 functions)` - `total_functions`
  still **28465** after the `splits.txt` edit
- `flip_test MetroidPrime/ScriptObjects/Carve801FD8E0.c: PASS` (DOL sha1
  `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, all 86 RELs cmp-equal)
- gate: probe `802 files, 0 failed, 0 errors`; `link: LINKED (291 undefined, 0 duplicates)` - the
  undefined count did not move
- `./tools/unit_fit.sh MetroidPrime/ScriptObjects/Carve801FD8E0.c`: `claimed 68, ours 68, retail 68,
  fits`, `no extra functions`
- `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/Carve801FD8E0.c`:
  `0 unit(s) checked` - carve units are not covered, so it proves nothing here. Checked the object
  directly instead: `powerpc-eabi-nm --defined-only -n` reads `fn_801FD8E0 @ 0x0`,
  `fn_801FD900 @ 0x20`, i.e. descending source order coming out as retail's ascending `.text`
- `python3 tools/check_symbol_names.py`: `532 units; 0 declared names missing`

## Files

- `src/MetroidPrime/ScriptObjects/Carve801FD8E0.c` (new, 96 lines, definitions descending)
- `configure.py:752` - `Object(Matching, ...)` on one line, in address order between
  `Carve801F97C8.c` and `Carve801FDB5C.c`
- `config/G2ME01/splits.txt:1201-1202` - `.text start:0x801FD8E0 end:0x801FD924`
- `files.cmake:574` - the source
- `src/MetroidPrime/PortLinkStubs.cpp:805-826` (`stub_183` comment; `stub_184` retired below it) and
  `1012-1030` (`stub_195`), plus the two header clauses at `12-15` and `37-40`

After the claim, dtk splits the auto object in two and the tree's own listings say so:
`build/G2ME01/asm/auto_03_801FA3CC_text.s` now ends at `fn_801FD890` (0x801FD890, 0x50, i.e. at
0x801FD8E0) and `build/G2ME01/asm/auto_03_801FD924_text.s` starts at `fn_801FD924`.

No `docs/HANDOFF.md` / `RUNNING_THE_DECOMP.md` edit by me. The counts in those files inside this
worktree were rewritten by `goal_check.sh`'s own docs step (`MP_GATE_DOCS_WRITE`), which the driver
discards.

## Left alone, and why

`fn_801FD924` (0x801FD924, 0x74) stays retail's: it stores the vtable `lbl_803B7BF0` into +0x0,
tears down an `rstl::basic_string` at +0x4 and the member at +0x14 through `fn_801FD6F0`, then frees
`self` behind `extsh. r0,r31 / ble`. It is the natural next carve here (0x801FD924..0x801FD998, with
`fn_801FD998` at 0x84 after it), but it is a body of its own rather than a 0x20-byte forwarder, so it
is left to the seeder rather than filed as a `NEW:` item on an unmeasured guess.

No `WALL:`, no `NEW:` lines.

---

# carve-801fd8e0 — lane 12, 2026-10-02 (re-run in `wt-mp2-goal-L12`)

**PASS, re-measured from scratch on this tree.** The section above is the earlier run's report from a
different worktree; this tree (`goal/lane-12`, head `1e022368 match: carve-80024d24`) had **no**
`Carve801FD8E0.c` and its `build/report.json` read `matched_functions 12672`, so nothing here was
inherited. The unit was written again from the four-file recipe plus the stub exchange.

## What was re-measured, independently

Read the region out of the retail disc rather than out of `build/G2ME01/main.elf`, which holds our
bytes: parsed `orig/G2ME01/sys/main.dol` directly (text section 2 = address 0x80003840, file offset
0x640, size 0x3A1C60; the ELF's `.text` VMA is 0x80003840, file offset 0x920).

- 0x801FD8E0..0x801FD924 is 0x44 = 68 bytes, 2 functions (`config/G2ME01/symbols.txt:8279-8280`).
- `fn_801FD8E0` vs `fn_80004C4C` (0x80004C4C, the `destroy` half in the already-Matching
  `Player/Carve80004C4C.c`): **8 of 8 words identical**; against the item's named twin `fn_80004D3C`
  (0x80004D3C, that file's `construct` half) also **8 of 8** - every one-call forwarder of this shape
  is the same eight words, only the `bl` differs, so naming the twin proves the shape and not which
  end of the element's life it is. `fn_801FD900` vs `fn_80004C6C`: **8 of 9 words identical**, the
  exception being the `bl` (`4bfffdd1` -> 0x80004A4C there, `48000015` -> 0x801FD924 here).
- Call sites read out of the disc: `fn_801FD890` calls `fn_801FD8E0` at 0x801FD8B4 with
  `addi r31,r31,0x24`; `fn_801FF7EC` (Matching `Carve801FF720.cpp`) calls it at 0x801FF810. `r4` is
  dead at both, so the argument list is one pointer and the element size is 0x24.
- The callee `fn_801FD924` (0x801FD924, 0x74, unclaimed) stores the vtable `lbl_803B7BF0` at +0x0,
  calls `fn_801FD6F0` for the member at +0x14 and `fn_802FE9B8` for the one at +0x4, then frees `self`
  behind `extsh.`/`ble` on its flag - the deleting destructor, `li r4,-1` at 0x801FD908.

## The change (this tree)

- `src/MetroidPrime/ScriptObjects/Carve801FD8E0.c` (new, 81 lines) - definitions **descending**
  (`fn_801FD900` then `fn_801FD8E0`), plain C so the `fn_` names do not mangle.
- `configure.py:754` one-line `Object(Matching, ...)`, between `Carve801F97C8.c` and
  `Carve801FDB5C.c` (address order).
- `config/G2ME01/splits.txt:1207-1208` `.text start:0x801FD8E0 end:0x801FD924`.
- `files.cmake:576` the source.
- `src/MetroidPrime/PortLinkStubs.cpp`: the `fn_801FD8E0` stub (`stub_184`) **retired** (its two
  definition lines removed; the `stub_183` comment rewritten to cover `fn_801FEA98` alone and to
  record the exchange) and **`stub_196` = `fn_801FD924`** appended. **The number is 196, not the
  earlier report's 195**: in this tree `stub_195` is already `fn_80008C28` for `Carve800052A0.c`.
  Header clauses updated: the total stays **164 functions / 4 data objects** (an exchange) and the
  breakdown stays 32 unmangled `fn_`/`lbl_`; measured after the edit, `grep -cE 'asm\("'` = 168 and
  `grep -cE 'asm\("(fn_|lbl_)'` = 32, both unchanged.

## Verification (measured on this tree, after the edits)

- `./tools/goal_check.sh build/goal/item.json` -> exit 0, `PASS carve-801fd8e0`,
  `counts: matched 12672 -> 12674   linked 6031 -> 6033`
- `./tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FD8E0.c` -> PASS; DOL sha1
  `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, all 86 RELs cmp-equal, `total_functions` still 28465
- linked region vs retail disc: `main.elf` `.text` at 0x801FD8E0..0x801FD924 is **68 of 68 bytes
  identical** to the DOL at that address. The object's own `.text` is 66 of 68 - the two differing
  bytes are the two `bl` words, which still carry relocations in the object.
- `./tools/unit_fit.sh ...Carve801FD8E0.c` -> `claimed 68, ours 68, retail 68, fits`,
  `no extra functions`
- `python3 tools/check_symbol_names.py` -> `checked 532 units; 0 declared names are missing`
- probe: `804 files, 0 failed, 0 errors; link: LINKED (291 undefined, 0 duplicates)` - the port's
  undefined count did not move (the exchange), and `port link dups` reports 0 duplicates.
- `python3 tools/link_gap.py --rebuild` with the `stub_196` block removed: **exit 1**, `286 MISSING`,
  `gap grew: fn_801FD924 is not in port_link_gap_list.md`; with it in place: **exit 0**, `285 MISSING`,
  `ok: 285 MISSING symbol(s), all accounted for in port_link_gap_list.md`.
- Layout after the claim, from `build/report.json`: `auto_03_801FA3CC_text` now ends at `fn_801FD890`
  (= 0x801FD8E0), the new unit is 68 bytes / 2 functions, both 100.00%, and `auto_03_801FD924_text`
  starts at `fn_801FD924` (0x801FD924..0x801FDBE0, 8 functions).
- `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/Carve801FD8E0.c` ->
  `ok: 0 unit(s) checked` - carve units are still not covered by it, so it proves nothing here.
  Checked the object instead: `powerpc-eabi-nm --defined-only -n` reads `fn_801FD8E0 @ 0x0`,
  `fn_801FD900 @ 0x20`, i.e. descending source order came out as retail's ascending `.text`.

## Left alone, and what the next run should know

- `fn_801FD924` (0x801FD924, 0x74) stays retail's, above this claim. It is the natural next carve
  here, but it is a real destructor body rather than a forwarder, so it is the seeder's call rather
  than a `NEW:` filed on an unmeasured guess - the same conclusion the earlier report reached.
- `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` in this worktree show as modified: that is
  `goal_check.sh`'s own docs step (`MP_GATE_DOCS_WRITE`) rewriting the derived counts (probe 803 ->
  804, matched 12672 -> 12674, and the 96k-line generated listing inside `HANDOFF.md` with it), which
  the driver discards. No hand edit to them by this run.

No `WALL:`, no `NEW:` lines.

---

# carve-801fd8e0 — lane 12, 2026-10-02 (third run, tree `wt-mp2-goal-L12` at `1ca7ef72`)

**PASS, re-derived from scratch.** The two sections above describe runs whose changes are **not in
this tree**: `git log --all --oneline | grep 801fd8e0` is empty, `build/goal/judge/HEAD` and
`git rev-parse HEAD` are both `1ca7ef72 match: carve-801fd638`, and before this run the tree had no
`Carve801FD8E0.c`, no `configure.py` entry, no `splits.txt` claim, no `files.cmake` line and no
`stub_198`. The 15:33 `goal_check` PASS in `build/goal/check.out` belongs to a tree state the driver
has since reset/replayed away (`build/goal/rebase.patch`, 15:35). So nothing was inherited, and the
earlier runs' numbers are stale for this tree: their baselines were matched 12672/12674, this tree's
judge baseline at `1ca7ef72` is **12680**.

## What moved since those runs (re-read this, not their conclusions)

- **The containing `auto_*` unit is not the one the item names.** `item.json` and both sections above
  say the range is "now in dtk's `main/auto_03_801FA3CC_text`". On this tree that unit ends at
  0x801FBD68 and 0x801FD8E0 sits inside **`main/auto_03_801FD67C_text`** (0x801FD67C..0x801FDBE0,
  1380 B, 16 functions) — `carve-801f97c8`, `carve-801fbc58` and `carve-801fd638` landed in between.
- **The stub numbers differ.** `stub_184` is still `fn_801FD8E0`, but the next free number here is
  **`stub_198`**, not 195/196: this tree has `stub_196` = `fn_801FBD68` and `stub_197` = `fn_801FD67C`.
- **The twin comparison, against the disc instead of the built object.** The first section compared
  `fn_801FD8E0` with `build/G2ME01/obj/MetroidPrime/Player/Carve80004C4C.o` and found "7 of 8 words
  identical, the one differing word the `bl`". Read out of `orig/G2ME01/sys/main.dol` instead (text
  section 1: file offset 0x640 at address 0x80003840, size 0x3A1C60): `fn_801FD8E0` is **8 of 8
  words identical, the `bl` included** — ours and the twin's `bl` are both `48000015`, because each
  callee sits 0x14 bytes past its own `bl`. The object-side difference is its unresolved relocation.
  `fn_801FD900` vs `fn_80004C6C` is **8 of 9**, the differing word being the `bl` at offset 0x10
  (`4bfffdd1` -> 0x80004A4C there, `48000015` -> 0x801FD924 here).
- **Those eight bytes cannot tell `destroy` from `construct`.** `fn_801FD8E0` is 8 of 8 against
  *both* `fn_80004D3C` (the construct forwarder) and `fn_80004C4C` (the destroy forwarder). What
  settles it is the callee chain (`li r4,-1` -> deleting destructor) and the callers (0x24-strided
  element walks).

## Measured on this tree

- 0x801FD8E0..0x801FD924 = 0x44 = 68 B, 2 functions (`config/G2ME01/symbols.txt:8279-8280`).
- Callers, read out of the disc: `fn_801FD890` (0x801FD890, 0x50, unclaimed) walks 0x24-strided
  elements (`bl fn_801FD8E0` at 0x801FD8B4, `addi r31,r31,0x24`, `lwz r0,0(r30)`/`cmplw`/`bne`);
  `fn_801FF7EC` in the `Matching` `ScriptObjects/Carve801FF720.cpp` does the same walk (`bl` at
  0x801FF810, stride 0x24, `r4` dead). Element size 0x24, argument list one pointer.
- Callee `fn_801FD924` (0x801FD924, 0x74, unclaimed) is the deleting destructor: vtable 0x803B7BF0
  into +0x0, `fn_801FD6F0` on +0x14 with `li r4,-1`, the `rstl::basic_string` at +0x4 released
  through `internal_dereference__Q24rstl66basic_string<...>`, `Free__7CMemoryFPCv` behind
  `extsh. r0,r31 / ble`.
- Body: `fn_801FD900(void* self) { fn_801FD924(self, -1); }` and
  `fn_801FD8E0(void* self) { fn_801FD900(self); }`, declared **descending**.

## The change (this tree)

- `src/MetroidPrime/ScriptObjects/Carve801FD8E0.c` (new, 102 lines) — measured header, one `extern
  void fn_801FD924(void*, int);`, definitions descending.
- `configure.py:758` — one-line `Object(Matching, ...)`, right after `Carve801FD638.c` (the nearest
  claimed range below, 0x801FD638..0x801FD67C). Note the list is *not* strictly address-ordered —
  `Carve801FDB5C.c` (claim 0x801FDBE0) already sits before `Carve801FD638.c` (claim 0x801FD638) —
  so this entry follows its neighbour rather than a sort.
- `config/G2ME01/splits.txt:1216-1217` — `.text start:0x801FD8E0 end:0x801FD924`, between the
  `Carve801FD638.c` and `Carve801FDB5C.c` entries (that file *is* in claim-address order).
- `files.cmake:580` — the source, same position as `configure.py`.
- `src/MetroidPrime/PortLinkStubs.cpp` — the exchange: **`stub_184` (`fn_801FD8E0`) retired** (two
  definition lines deleted; the `stub_183` comment at 812-830 rewritten to cover `fn_801FEA98`
  alone, with its `0x20 bytes` / `symbols.txt:8279` slip for `fn_801FD900` corrected to `0x24` /
  `:8280`), and **`stub_198` = `fn_801FD924`** appended at 1087-1115. The header counts were already
  stale by one (`grep -cE 'asm\("'` read 169 against the header's 168; unmangled 33 against 32)
  because `carve-801fd638` added `fn_801FD67C` without updating them; they now read **169 supplied /
  165 functions + 4 data / 33 unmangled / 45 game method**, which is what the file measures after the
  exchange (one out, one in).

## Verification (measured on this tree)

- `./tools/goal_check.sh build/goal/item.json` -> exit 0, `PASS carve-801fd8e0`
- counts: matched **12680 -> 12682**, linked **6039 -> 6041**; `All: 35.54% fuzzy, 29.38% matched,
  13.05% linked (12682 / 28465 functions)` — `total_functions` still **28465** after the `splits.txt`
  edit
- `./tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FD8E0.c` -> PASS; DOL sha1
  `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`; ninja `87 files OK`; gate `hashes vs config.yml ok`
- built `build/G2ME01/main.dol` vs the retail disc at 0x801FD8E0..0x801FD924: **68 of 68 bytes
  identical** (the object's own `.text` is 66 of 68 — the two `bl` words carry relocations)
- `./tools/unit_fit.sh ...Carve801FD8E0.c` -> `claimed 68, ours 68, retail 68, fits`,
  `no extra functions`
- `python3 tools/check_symbol_names.py` -> `checked 532 units; 0 declared names are missing`
- probe: `808 files, 0 failed, 0 errors; link: LINKED (291 undefined, 0 duplicates)` — the undefined
  count did not move (291 = the judge baseline), and the gate's `port link dups` reads 0 duplicates
- `python3 tools/link_gap.py --rebuild` **with** `stub_198`: exit 0, `285 MISSING`, `all accounted
  for`; with the block cut out (file restored afterwards): exit 1, `286 MISSING`, `gap grew:
  fn_801FD924 is not in port_link_gap_list.md`. Both measured this run.
- layout after the claim, from `build/report.json`: `main/auto_03_801FD67C_text` is now 612 B /
  6 functions, the new `main/MetroidPrime/ScriptObjects/Carve801FD8E0` is 68 B / 2 functions, both
  100.00% and complete, and `main/auto_03_801FD924_text` starts at 0x801FD924 (700 B / 8 functions);
  the gate's per-function diff reports `10 function(s) moved ... exact count match - a split, not a
  loss`
- `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/Carve801FD8E0.c` ->
  `ok: 0 unit(s) checked` — carve units are still not covered, so it proves nothing here. Checked the
  object directly instead: `powerpc-eabi-nm --defined-only -n` reads `fn_801FD8E0 @ 0x0`,
  `fn_801FD900 @ 0x20`, i.e. descending source order came out as retail's ascending `.text`
- `./tools/carve_diff.sh 0x801FD8E0 0x44 build/G2ME01/obj/.../Carve801FD8E0.o` -> `17 instructions,
  68 bytes` on both sides, `differing instructions: 2`, `NOT byte-exact`; the two named instructions
  are exactly the two `bl`s, which read as `bl c` / `bl 30` in the object because their relocations
  are unresolved. That verdict is an artefact of the tool's inputs — `flip_test` and the DOL
  comparison above are the acceptance tests.

## Left alone, and for the next run

- `fn_801FD924` (0x801FD924, 0x74) stays retail's. Its 0x74 bytes need the `.data` vtable
  `lbl_803B7BF0`, `fn_801FD6F0` (0x801FD6F0, 0x84) and `fn_801FD774`, plus the string release — a
  real destructor body, not a forwarder, so it is the seeder's call rather than a `NEW:` filed on an
  unmeasured guess. The stub here is an empty stand-in that says so.
- `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` in this worktree show as modified: that is
  `goal_check.sh`'s own docs step (`MP_GATE_DOCS_WRITE`) rewriting the derived counts, which the
  driver discards. No hand edit to them by this run.

No `WALL:`, no `NEW:` lines.
