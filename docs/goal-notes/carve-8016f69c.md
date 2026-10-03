# carve-8016f69c — `match` on `MetroidPrime/Carve8016F69C`

**DONE. `Matching` 1/1 at 100.00%, `flip_test` PASS, `goal_check.sh` PASS.**

## What I did

Carved `fn_8016F69C` (0x8016F69C, 0x20 = 32 bytes, 8 instructions) out of dtk's unclaimed
`auto_03_8016DF3C_text` range as the new `Matching` unit `src/MetroidPrime/Carve8016F69C.c`.
Four files, all four of them:

| file | what changed |
| --- | --- |
| `src/MetroidPrime/Carve8016F69C.c` | new — the unit |
| `config/G2ME01/splits.txt` | new range 0x8016F69C..0x8016F6BC, in address order between `MetroidPrime/CRumbleManager.cpp` and `MetroidPrime/Carve8016FD4C.c` |
| `configure.py:767` | `Object(Matching, "MetroidPrime/Carve8016F69C.c"),` on one line, in address order |
| `files.cmake:741` | `src/MetroidPrime/Carve8016F69C.c` in the host source list |
| `src/MetroidPrime/PortLinkStubs.cpp` (end) | `stub_carve8016f69c_0`, the announced host stand-in for the callee |

The claimed range is exactly the item's range and nothing else. It does not span an unclaimed gap:
below it 0x8016DF3C..0x8016F69C and above it 0x8016F6BC..0x8016FD4C are still dtk's, and both stay
unclaimed.

## What the function is, and how it was measured

`fn_8016F69C` is **`CBouncyGrenade::Touch(CActor&, CStateManager&)` forwarding to
`CActor::Touch`** — a frame and one `bl`, with all three arguments untouched (no register moves
before the `bl`, which is what a qualified base call produces).

Retail's bytes, re-read off the disc this run:

```
python3 tools/dol_read.py 0x8016F69C 0x20
.text @ 0x8016f69c  (file 0x16c49c, 32 bytes)
hex : 94 21 ff f0 7c 08 02 a6 90 01 00 14 4b ed ce bd 80 01 00 14 7c 08 03 a6 38 21 00 10 4e 80 00 20
```

`4b ed ce bd` decodes to `bl 8004c564` (`powerpc-eabi-objdump` on `build/G2ME01/main.elf`), and
`symbols.txt:1473` names 0x8004C564 `Touch__6CActorFR6CActorR13CStateManager`, **size 0x4** — those
four bytes are a single `blr`.

The vtable evidence, which is what names the class rather than guessing it from the shape:

- `0x8016F69C` occurs **exactly once** in `main.dol`. Reading its file offset (0x3B24B4) back
  through the section table puts it at `.data` **0x803B53D4**; `- 0x40` is **0x803B5394**, and what
  that address holds is `TypesMatch__14CBouncyGrenadeCFi` (0x8009C904, `symbols.txt:3064`) — so it
  is CBouncyGrenade's vtable, and `__ct__14CBouncyGrenade...` is at 0x8016F874 in this same run.
- `CActor`'s table starts at **0x803B1AEC** (base holds `TypesMatch__6CActorCFi`, 0x8009CC4C,
  `symbols.txt:3079`), and at **base + 0x40** — 0x803B1B2C — it holds 0x8004C564,
  `Touch__6CActorFR6CActorR13CStateManager`. Two independently identified tables with the same
  0x40 offset is what makes 0x40 the `Touch` slot, matching `include/MetroidPrime/CActor.hpp:93`
  (`virtual void Touch(CActor&, CStateManager&);`).
- **Trap worth recording** (it cost me one wrong draft): a DOL file offset is *not*
  `0x80004100 + off - 0x100`. The load segments are not contiguous, so that formula puts
  `.data` addresses 0x1E00 low. Every data address in the final comments was re-derived through
  `readelf -S`. Anyone quoting a `.data` address out of a DOL must do the same.

The item's named shape twin `fn_80004438` (`src/MetroidPrime/Carve80004438.c`, `Matching`) is these
eight instructions with a different `bl`, as is `__sys_free` (0x80008A28, `src/MetroidPrime/main.cpp`)
and `CIngBoostBallGuardian1464C.cpp`'s `fn_30_1464C`. That measures the *frame*, not the identity —
a 0x20-byte forwarder also has the shape of `rstl::destroy` — and the vtable slot plus the callee's
name are what settle it as `Touch`.

## Why plain C, and why the callee is declared under retail's mangled name

`symbols.txt:6041` carries only the `fn_8016F69C` placeholder, so the definition reproduces that
symbol verbatim and has to stay C (a C++ one would mangle to `_Z13fn_8016F69C...` and objdiff would
pair nothing). A plain C definition site emits the `bl` against whatever the callee is spelled, so
the callee is declared as `extern void Touch__6CActorFR6CActorR13CStateManager(...)` — retail's own
mangled name. `ScriptObjects/CIngBoostBallGuardian1464C.cpp:31` declares the same name the same way
for its own REL copy of this call.

The callee is **not** claimed here: 0x8004C564 falls inside `MetroidPrime/CActor.cpp`'s existing
claim (0x80049ED8..0x8004E84C), so dtk's object for that unit supplies the bytes in the DOL link.

## The host stand-in, and why it is not a stub that hides work

Listing the new `.c` in `files.cmake` adds one undefined name to the port link, and
`tools/link_gap.py` fails on any newly missing symbol that is not documented. So
`src/MetroidPrime/PortLinkStubs.cpp` gained `stub_carve8016f69c_0` bound to
`Touch__6CActorFR6CActorR13CStateManager`, announced as an empty-body stand-in in the paragraph
style of `stub_carve8016bea8_0` / `stub_carve801834c8_0` above it. **Here the empty body is
retail's behaviour, not a loss**: retail's own function is 4 bytes and does nothing. `PortLinkStubs.cpp`
is not in `configure.py`, so the definition cannot reach `main.dol`, and the gate's `port link dups`
step stays at 0.

## Measured

```
build/report.json (after):
  total_functions      28465  (unchanged)
  matched_functions    13615 -> 13616
  complete_units       1040 -> 1041
  main/MetroidPrime/Carve8016F69C: total_code 32, matched_code 32, total_functions 1,
                                       matched_functions 1, metadata.complete true

./tools/unit_fit.sh MetroidPrime/Carve8016F69C.c
   .text      claimed     32   ours     32   retail     32   fits
   no extra functions: our object defines only what the retail unit object does

./tools/check_decl_order.py --unit MetroidPrime/Carve8016F69C.c
ok: 0 unit(s) checked, none emits its functions out of retail order

./tools/flip_test.sh MetroidPrime/Carve8016F69C.c
TEST MetroidPrime/Carve8016F69C.c
  PASS  -> kept as Matching
kept: 1 / 1   failed: 0   skipped: 0

./tools/goal_check.sh build/goal/item.json
goal_check: item carve-8016f69c (match) target=MetroidPrime/Carve8016F69C
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13615 -> 13616   linked 6663 -> 6664
  ok    check_symbol_names.py
  ok    All:  37.69% fuzzy, 31.13% matched, 13.99% linked (13616 / 28465 functions)
  ok    flip_test MetroidPrime/Carve8016F69C.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-8016f69c
```

The dtk-emitted object is byte-exact against the disc read above
(`build/G2ME01/asm/MetroidPrime/Carve8016F69C.s`, `bl Touch__6CActorFR6CActorR13CStateManager`
encoded `4B ED CE BD`). Note `tools/carve_diff.sh` reports `NOT byte-exact` on that one
instruction: it disassembles the *unlinked* object, where the branch still shows the placeholder
`bl c`, and compares it against the linked `main.elf`. `flip_test.sh` is the verdict that counts
and it is PASS.

Single function, so the "descending by retail offset" rule is satisfied trivially, but the file is
written that way and `check_decl_order.py` agrees.

## Nothing new is blocked

`NEW:` — none filed. The item is complete; the neighbouring unclaimed functions
(`fn_8016F6BC` 0xE8, `fn_8016F7A4` 0xD0, `fn_8016F318` 0x384, …) in the same
`auto_03_8016DF3C_text` run are seedable carves of the same shape, but the seeder already covers
them and a `NEW:` line may only name one real unit, so there is nothing to file here.