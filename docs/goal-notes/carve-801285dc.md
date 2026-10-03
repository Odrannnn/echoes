# carve-801285dc — `MetroidPrime/Carve801285DC.c`, `Matching`, 2/2 at 100.00%

**Result: PASS.** `./tools/goal_check.sh build/goal/item.json` → `goal_check: PASS carve-801285dc`,
with `counts: matched 13485 -> 13487  linked 6533 -> 6535`. `build/report.json` for the new unit
reads `{'fuzzy_match_percent': 100.0, 'matched_code': '80', 'total_code': '80', 'total_functions': 2,
'matched_functions': 2, 'complete_units': 1}`, both functions `100.0`.

## What was carved

`.text 0x801285DC..0x8012862C`, 0x50 = 80 bytes, 2 functions (`config/G2ME01/symbols.txt:5008-5009`;
`fn_8012862C` at `:5010` is **not** claimed):

| function | address | size | insns |
| --- | --- | --- | --- |
| `fn_801285DC` | 0x801285DC | 0x28 | 10 |
| `fn_80128604` | 0x80128604 | 0x28 | 10 |

The carve is four files, each placed in address order: `src/MetroidPrime/Carve801285DC.c` (new),
`configure.py:738` (`Object(Matching, "MetroidPrime/Carve801285DC.c")`, one line, between
`Carve8012CB4C.c` and `Player/Carve8014FFCC.c`), `config/G2ME01/splits.txt:798-799`
(`.text start:0x801285DC end:0x8012862C`, between `CGameCollision.cpp` and `CGroundMovement.cpp`),
`files.cmake:710`. `total_functions` stays **28465**. No `asm` added anywhere. Definitions are
**descending by address** (604, 5DC); `tools/check_decl_order.py --unit MetroidPrime/Carve801285DC.c`
→ `ok`.

Directory: the nearest claimed range below is `MetroidPrime/CGameCollision.cpp`
(`.text 0x80123510..0x801285DC` — it ends *exactly* where this claim starts), the one above is
`MetroidPrime/CGroundMovement.cpp` (`.text 0x8012878C..0x8012CA24`).

## What the two functions are

Both are argument-shuffling forwarders: `mr r3,r4` then `mr r4,r5` and one `bl`. The caller's `r3` is
dead; the caller's `r4`/`r5` become the callee's `r3`/`r4`. Nothing is loaded, spilled or computed,
which is why the frame is retail's smallest (16 bytes, LR only). The two bodies are the **same ten
words**; only the `bl` differs.

- `fn_801285DC` → `bl 0x802860BC <AddAverageToFront__13CollisionUtilFRC18CCollisionInfoListR18CCollisionInfoList>`
  (`48 15 DA CD`)
- `fn_80128604` → `bl 0x8012862C <fn_8012862C>` (`48 00 00 15`)

Source read this session with
`build/binutils/powerpc-eabi-objdump -d --start-address=0x801285DC --stop-address=0x8012862C build/G2ME01/main.elf`;
before the claim the same bytes are in `build/G2ME01/asm/auto_03_801285DC_text.s:9-20` and `:23-34`.

## Twin comparison

The item's twin is `fn_800273DC` (0x800273DC, 0x28, **100.00%** in `src/MetroidPrime/CAnimData.cpp` —
retail's own `rstl::less<rstl::basic_string<char> >::operator()`, per the name in `symbols.txt`;
`build/G2ME01/asm/MetroidPrime/CAnimData.s:1704-1714`). It is a twin of the **frame**, not of the
body: the same eight of ten words (`stwu r1,-0x10(r1)`, `mflr r0`, `stw r0,0x14(r1)`, `bl`,
`lwz r0,0x14(r1)`, `mtlr r0`, `addi r1,r1,0x10`, `blr`) and the same slot offsets. The two words it
spends where these two spend `mr r3,r4 / mr r4,r5` are `srwi r3,r3,31`, which is only that functor
narrowing an `int` return value to a `bool` — a `void` carrier has no reason to emit it. So nothing
about the bodies was carried across: the argument shuffle and the two callee names are read off this
copy's own bytes.

## `tools/carve_diff.sh`

```
./tools/carve_diff.sh 0x801285DC 0x50 build/G2ME01/obj/MetroidPrime/Carve801285DC.o
retail: 20 instructions, 80 bytes
ours  : 20 instructions, 80 bytes
  +5   retail: 801285f0 bl 802860bc <AddAverageToFront__...> ours: 00000014 bl 14 <fn_801285DC+0x14>
  +15  retail: 80128618 bl 8012862c <fn_8012862C>      ours: 0000003c bl 3c <fn_80128604+0x14>
```

20/20 instructions and 80/80 bytes; the only two differences are the two `bl` relocations, which the
link resolves. `tools/unit_fit.sh MetroidPrime/Carve801285DC.c` →
`.text claimed 80 ours 80 retail 80 fits` + `no extra functions`.

## Two host-side definitions, and why they are stand-ins

**This is the part a reviewer should look at, so it is stated plainly: the two names this carve calls
are real for the DOL and absent from the port, and `src/MetroidPrime/PortGlobals.cpp` now defines
both as stand-ins that print their own name once and do nothing else.** They exist only because
listing this carve in `files.cmake` put two symbols into the port's undefined set, which
`tools/link_check.sh --strict` fails on (`288 undefined against a baseline of 287` on the first
`goal_check` run — measured, not inferred).

- `AddAverageToFront__13CollisionUtilFRC18CCollisionInfoListR18CCollisionInfoList` is retail's own
  mangled name (MWCC's old mangling is `[A-Za-z0-9_]` only, so the C file can spell it verbatim —
  the same trick `src/MetroidPrime/Carve800E1548.c:77` uses on `Free__7CMemoryFPCv`), for
  `CollisionUtil::AddAverageToFront` at 0x802860BC. **A forwarder to the real function was tried
  first and does not work**: `src/Collision/CollisionUtil.cpp` is a `NonMatching` object
  (`configure.py:445`) that is **not in `files.cmake`**, so the port has no such function. The
  forwarder traded this undefined symbol for `_ZN13CollisionUtil17AddAverageToFrontERK18CCollisionInfoListRS0_`
  being undefined instead, and `tools/gate.sh`'s `link-gap` step rejected it
  (`gap grew: _ZN13CollisionUtil17AddAverageToFrontERK18CCollisionInfoListRS0_ is not in
  port_link_gap_list.md`). Both were measured this run.
- `fn_8012862C` (0x8012862C, 0x160 = 352 bytes) is unclaimed by any unit, so dtk's own object
  supplies its bytes in the DOL. It is a two-pass filter over an array of 0x60-byte entries, then
  `AddAverageToFront`. 0x60 is `CCollisionInfo`'s size and 0x20 the list capacity
  (`include/Collision/CCollisionInfoList.hpp`), so the shape is readable, but transliterating it
  means naming the entry layout and the `lbl_8041BE40` seed — a claim's job, not this item's. So it
  announces itself instead, exactly like `ReportedCameraManagerStandIn` in the same file.

This follows the precedent set by `fn_8028139C` at the end of the same file, which exists for
precisely this reason, and by the 189 stubs in `src/MetroidPrime/PortLinkStubs.cpp`. Neither
definition is inside a `configure.py` unit, so neither can touch `main.dol`.

## Gate, as measured

- `./tools/goal_check.sh build/goal/item.json` → `PASS` (`gate.sh` ok, `check_symbol_names.py` ok,
  `flip_test.sh` PASS, counts up)
- `sha1sum build/G2ME01/main.dol` → `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`
- all 86 RELs `cmp`-equal with their `config.yml` sha1s (gate.sh step `hashes vs config.yml  ok`)
- `build-port-link/link_summary.txt` → `286 0 0 1` (undefined / duplicates / compile errors /
  linker_ran): **286 undefined, one below the recorded baseline of 287**, 0 duplicates
- `tools/check_symbol_names.py` → `checked 587 units; 0 declared names are missing from their object`
- probe count rose 922 → 923 files, 0 failures

## Lessons (for the notes, not filed as items)

- **The carve recipe's "declare extern, never define" rule is only half the job when the callee is
  not in the port build.** Two of the last few carve items got away without host-side definitions
  only because their callees happened to be implemented in the port. A carve item whose callee is
  either (a) a retail-mangled symbol whose `.cpp` is not in `files.cmake`, or (b) an unclaimed
  `fn_` address, will fail `link_check --strict` and then `link-gap`, and the fix belongs in
  `PortGlobals.cpp`.
- **`gate.sh`'s `link-gap` step is the one that catches a "helpful" forwarder.** It compares against
  `docs/research/port_link_gap.md`, so defining a retail-mangled name by calling the host C++ name
  moves a *different* symbol into the gap. Read the gate output, not the linker.
- **`objdiff`/`carve_diff` reporting `NOT byte-exact` with only `bl` targets differing is expected**
  for any carve that calls something; the relocation is what the linker fills.

NEW: fn-8012862c | match | MetroidPrime/Carve8012862C | fn_8012862C (0x8012862C, 0x160 = 352 bytes,
unclaimed, `symbols.txt:5010`) is a carveable nearest-slope filter over 0x60-byte CCollisionInfo
entries that ends in AddAverageToFront; carving it would also let Carve801285DC.c's fn_80128604
reach the port without the announcing stand-in now in src/MetroidPrime/PortGlobals.cpp