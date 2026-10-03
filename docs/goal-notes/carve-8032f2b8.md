# carve-8032f2b8 — `Kyoto/Math/Carve8032F2B8` flipped to `Matching`

## What I did

A four-file carve of the two functions the driver named, both of them read off named retail twins
rather than guessed, then verified with `tools/flip_test.sh`.

- **new** `src/Kyoto/Math/Carve8032F2B8.c` — `.text` 0x8032F2B8..0x8032F31C, 0x64 = 100 bytes,
  2 functions, plain C so the `fn_<addr>` symbols are reproduced verbatim.
- `configure.py:923` — `Object(Matching, "Kyoto/Math/Carve8032F2B8.c")` on one line, between
  `Carve8032E444.c` and `Carve803359F4.c` (address order).
- `config/G2ME01/splits.txt` — new stanza between the same two, `.text start:0x8032F2B8 end:0x8032F31C`.
- `files.cmake:908` — `src/Kyoto/Math/Carve8032F2B8.c` between the same two.

No `PortLinkStubs.cpp` entry existed for either symbol (`grep -rn "fn_8032F2B8\|fn_8032F310" src/
include/` outside the new file finds nothing), so there was none to remove.

## Measured, not recalled

**Addresses / sizes** — `config/G2ME01/symbols.txt:14959-14960`:
`fn_8032F2B8 = .text:0x8032F2B8; size:0x58`, `fn_8032F310 = .text:0x8032F310; size:0xC`.
**Instructions** — `build/G2ME01/asm/auto_03_8032EE68_text.s:334-366` (dtk's own emission, the
unclaimed range this claim sits inside: 0x8032EE68..0x8032F8F8).

**The two twins, confirmed by `tools/dis.sh`, not by resemblance:**

| ours | twin | evidence |
|---|---|---|
| `fn_8032F2B8` (22 insns, 0x58) | `__dt__Q24rstl32single_ptr<18CGameGlobalObjects>Fv` (0x80006AE0, 0x58) | `./tools/dis.sh` on both: same 22 instructions in the same order, same `bl 802ce388 <Free__7CMemoryFPCv>`; only `li r4,1 / bl __dt__18CGameGlobalObjectsFv` becomes `li r4,1 / bl __dt__24CSpawnSystemKeyframeDataFv` |
| `fn_8032F310` (3 insns, 0xC) | `DisableFog__Q29CGameArea8CAreaFogFv` (0x80056DE8, 0xC) | both are `li r0,0 / stw r0,0(r3) / blr` |

The twin shapes are the MWCC deleting-destructor convention already written out in this tree
(`src/MetroidPrime/Factories/Carve80032674.c:30-35`): `mr. r30,r3 / beq` guards the receiver,
`extsh. r0,r31 / ble` re-tests the incoming **short**, `Free(self)` only for a positive flag, and
`mr r3,r30` returns the receiver on every path. So `if (flag > 0)` sits **inside** `if (self)` —
otherwise the `beq` lands on the `extsh.` instead of the epilogue.

**What the callers say the arguments are** (`grep -rn 'bl fn_8032F2B8\|bl fn_8032F310'
build/G2ME01/asm/`, three calls, all inside this same unclaimed `auto_03` file):

- `fn_8032F2B8` at 0x8032F080 and 0x8032F1D4, **both `li r4,0x1`** — the deleting flag. At
  0x8032F080 it runs on the `+0x4` member of the object whose head word was just set to the
  base-class destructor vtable `lbl_803BB3F8` (0x8032F06C), guarded by the caller's own
  `cmplwi r3,0` at 0x8032F074.
- `fn_8032F310` at 0x8032EFC0, right after `__nw__FUlPCcPCc` (0x8032EFB4) and its `mr. r31,r3 / beq`
  — handed the freshly allocated object, clears its first word.

That makes `fn_8032F2B8` the teardown of a `rstl::single_ptr<CSpawnSystemKeyframeData>` member, and
`__dt__24CSpawnSystemKeyframeDataFv` (0x80322DC8, 0x58, `symbols.txt:14693`, weak) is that class's
own destructor. It sits inside the `.text` 0x80322C1C..0x80323038 that
`Kyoto/Particles/CGenDescription.cpp` claims, so the DOL link resolves it. Declared, never defined.

**Directory** is retail's own, from the nearest claimed ranges: below is
`Kyoto/Math/Carve8032E444.c` (0x8032E444..0x8032E44C), above is `Kyoto/Math/Carve803359F4.c`
(0x803359F4..0x80335A0C). The claim starts and ends inside one dtk `auto_*` file, so `dol split`
gets no link-order cycle; nothing above or below is touched.

## Results

```
$ ./tools/flip_test.sh Kyoto/Math/Carve8032F2B8.c
  PASS  -> kept as Matching
kept: 1 / 1   failed: 0   skipped: 0
```

```
$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13487 -> 13489   linked 6535 -> 6537
  ok    check_symbol_names.py
  ok    All:  37.56% fuzzy, 31.00% matched, 13.86% linked (13489 / 28465 functions)
  ok    flip_test Kyoto/Math/Carve8032F2B8.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-8032f2b8
```

`build/report.json`, unit `main/Kyoto/Math/Carve8032F2B8`: `total_functions 2`,
`matched_functions 2`, `fuzzy_match_percent 100.0`, `total_code "100"`, `matched_code "100"`,
`complete_units 1`; per function `fn_8032F310 100.0`, `fn_8032F2B8 100.0`.

Supporting checks, all run on the final tree:

- `sha1sum build/G2ME01/main.dol` → `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (unchanged).
- `./tools/unit_fit.sh Kyoto/Math/Carve8032F2B8.c` → `.text claimed 100 ours 100 retail 100 fits`,
  "no extra functions: our object defines only what the retail unit object does".
- `./tools/probe_sources.sh` → `924 files, 0 failed, 0 errors; link: LINKED (287 undefined,
  0 duplicates)`. **287 undefined is exactly the baseline**
  (`docs/research/port_link_baseline.txt:4`), so the count did not move; both callees are declared,
  never defined here.
- `python3 tools/check_symbol_names.py` → `checked 587 units; 0 declared names are missing`.
- `./tools/carve_diff.sh 0x8032F2B8 0x64 build/G2ME01/src/Kyoto/Math/Carve8032F2B8.o` →
  `retail: 25 instructions, 100 bytes / ours: 25 instructions, 100 bytes`, differing instructions: 2,
  and both are the unresolved `bl` relocations in the relocatable `.o` (`bl 28 <fn_8032F2B8+0x28>` /
  `bl 38 <fn_8032F2B8+0x38>`). The linked bytes are exact — that is what `flip_test.sh` proves.

## Notes / nothing blocked

`tools/check_decl_order.py --unit Kyoto/Math/Carve8032F2B8.c` printed `0 unit(s) checked` on this
tree: the tool only inspects units it can place in `docs/research/decl_order.md` (a list of units
that *would* break on a flip), and a fresh `Matching` unit is not on it. It is not evidence either
way. The file is written descending by address (`fn_8032F310` at 0x8032F310, then `fn_8032F2B8` at
0x8032F2B8), and `flip_test.sh` — the only check that sees a permutation — passes.

`docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` show a diff in this worktree (probe count
923 → 924, matched 13487 → 13489). **`tools/gate.sh` rewrote those derived counts itself** when
`goal_check.sh` ran; I did not edit them. The driver discards edits to both before judging.

No `NEW:` lines: nothing was blocked, and there is no measured wall to hand on.