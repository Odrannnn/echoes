# carve-801f14f0 — `MetroidPrime/Carve801F14F0`

`kind: match`, target `MetroidPrime/Carve801F14F0`. **PASS** — the unit is `Matching` and
`tools/flip_test.sh` kept it there.

## What I did

Carved `fn_801F14F0` (retail `.text:0x801F14F0`, 0x20 = 32 bytes, 8 instructions) out of dtk's
unclaimed `auto_03_801F0D24_text` run as its own `Matching` unit. A carve is four files, all in
this change:

| file | what |
| --- | --- |
| `src/MetroidPrime/Carve801F14F0.c` | new, the body and the header |
| `config/G2ME01/splits.txt` | `MetroidPrime/Carve801F14F0.c: .text start:0x801F14F0 end:0x801F1510` |
| `configure.py` | `Object(Matching, "MetroidPrime/Carve801F14F0.c")` between `Carve801EF84C.cpp` and `Carve801F3690.c` |
| `files.cmake` | `src/MetroidPrime/Carve801F14F0.c` in the same slot |

Plus one port stand-in, `stub_carve801f14f0_0` in `src/MetroidPrime/PortLinkStubs.cpp:1987-2013`,
for the callee the carve calls — see "the callee" below.

One `Object(...)` per line, in address order, `.c` (not `.cpp`): `symbols.txt:8020` carries only
the `fn_<addr>` placeholder, so a C++ definition would mangle to `_Z11fn_801F14F0v` and objdiff
would pair nothing.

## What I measured

**The bytes.** `build/G2ME01/asm/auto_03_801F0D24_text.s:568-577`, before the claim existed:

```
801F14F0  stwu r1,-0x10(r1) | mflr r0 | stw r0,0x14(r1) | bl fn_801F1510
801F1500  lwz r0,0x14(r1) | mtlr r0 | addi r1,r1,0x10 | blr
```

`tools/carve_diff.sh 0x801F14F0 0x20 build/G2ME01/obj/MetroidPrime/Carve801F14F0.o fn_801F14F0`
reports `retail: 8 instructions, 32 bytes` / `ours: 8 instructions, 32 bytes` and names one
differing instruction — `+3 retail: bl 801f1510 <fn_801F1510> ours: bl c <fn_801F14F0+0xc>`. That
is the **relocation** in the unrelocated `.o` (`48 00 00 01`, the offset to the section's end),
not a code difference; the linked form is `48 00 00 15`. The other seven instructions are
byte-identical. `build/report.json` gives the real verdict:
`main/MetroidPrime/Carve801F14F0` `matched_functions: 1 / 1`, `matched_code_percent: 100.0`,
`complete: true`.

**The twin, and why two parameters.** The item named `fn_80004438`
(`src/MetroidPrime/Carve80004438.c:95-97`, 0x80004438, 0x20, `Matching`) and it is exact: the same
eight instructions in the same order, `bl fn_801F1510` for `bl fn_80004458`. Its body is one line,
`fn_80004458(self)`.

`fn_801F14F0` needs **two** parameters and I checked rather than assumed it. It writes no register
between the prologue and its `bl`, so `r3` and `r4` are both forwarded. Its only retail caller,
`fn_801F14A8` (0x801F14A8, 0x48, still unclaimed), sets `r3` from
`r31 + *(r31)*0x1C + 4` at 0x801F14BC-0x801F14C8 and calls at 0x801F14CC without touching `r4` —
so the second parameter is real, the caller's own second argument. A one-parameter spelling would
have compiled to the same instructions anyway (MWCC passes a second `void*` in `r4` regardless),
but the two-parameter declaration is what the caller actually does.

**The claim is exactly the item's range.** Nothing above or below it is claimed:
`fn_801F1510` (0x44) sits one function above and `fn_801F14A8` (0x48) sits immediately below,
both still in `auto_03_801F0D24_text`. `config/G2ME01/splits.txt` grew by three lines and
`total_functions` is **28465** before and after (`All: ... (13624 / 28465 functions)`).

`python3 tools/check_decl_order.py` → `ok: 1294 unit(s) checked, 37 permuted, all 37 accounted for`
— the 37 are pre-existing and listed in `decl_order.md`; a single function is descending by
construction. `tools/unit_fit.sh MetroidPrime/Carve801F14F0.c` → `.text claimed 32 ours 32
retail 32 fits` / `no extra functions`.

## The callee

`fn_801F1510` (0x801F1510, 0x44 = 68 bytes) is **above** this claim, so it is declared and never
defined here. Retail's 17 instructions (`auto_03_801F0D24_text.s:580-598`): `cmplwi r3,0 /
beqlr`, then a member-wise copy from `r4` to `r3` of a 0x19 = 25-byte aggregate with the loads and
stores interleaved — two `short`s at +0x00/+0x04, four `float`s at +0x08/+0x0C/+0x10/+0x14 and a
`char` at +0x18, ending `blr` with no frame. That is an assignment operator's body; its own 0x44
bytes are a separate spelling job, which is why the claim stops where it does. The `bl` at
0x801F14FC is retail's, so the carve cannot drop it.

For the DOL, dtk's `auto_*` object still defines the range above the claim. For the **port link**
nothing did, so the carve opened a new undefined symbol. Measured with
`python3 tools/link_gap.py --rebuild`:

```
before the stub:  gap grew: fn_801F1510 is not in port_link_gap_list.md   (279 MISSING)
after  the stub:  ok: 278 MISSING symbol(s), all accounted for
```

So `stub_carve801f14f0_0` (empty body, announced, keyed to the unit that asks for it, following
`stub_carve8016f69c_0` above it) is what keeps `tools/gate.sh`'s `port link gap` step green. The
`port link dups` step is also green: `fn_801F14F0` was never stubbed — `grep -rn 'fn_801F14F0'
src/ include/` returned only this carve.

The stub file's three derived header counts move `grep -cE 'asm\("'` 213 -> 214; the other two
terms are unmoved (`^extern "C" void stub_[A-Za-z0-9_]*\(\) asm\(` stays 201 — this stub takes
parameters, so it does not match that term — and `^extern "C" char stub_data_*` stays 10). The
header paragraph was left untouched, per the convention the neighbouring stubs state.

## Verification

`./tools/goal_check.sh build/goal/item.json`, in this worktree, on this tree:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13623 -> 13624   linked 6671 -> 6672
  ok    check_symbol_names.py
  ok    All:  37.70% fuzzy, 31.13% matched, 14.00% linked (13624 / 28465 functions)
  ok    flip_test MetroidPrime/Carve801F14F0.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-801f14f0
```

`sha1sum build/G2ME01/main.dol` → `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`. The gate's
per-function diff reads `SPLIT main/auto_03_801F0D24_text: 24 function(s) moved into
main/MetroidPrime/Carve801F14F0, main/auto_03_801F1510_text (exact count match - a split, not a
loss)` — the residual `auto_*` range is renamed by the carve, which is what report_diff scores as
a split. `decl order`, `files.cmake`, `port link gap` and `reach stubs` all `ok` in the gate log.

The judge (`MP_GATE_DOCS_WRITE=1`) rewrote the three derived numerals in `docs/HANDOFF.md`'s state
block; that is the judge's own write, not mine, and the driver discards edits to that file.

## Reusable note

A twin of `fn_80004438` (frame + one unconditional `bl`, no load, no test, no return value) is
worth checking for a **second** parameter before writing it: the twin is one-argument but the
caller's `r4` may be the copy's own second argument. Here it was — `fn_801F14A8` sets only `r3` and
forwards `r4` — and `fn_801F1510`'s body reads two registers, so the two-parameter declaration is
the honest one. The bytes are the same either way; only the header's claim differs.

`carve_diff.sh`'s "differing instructions: 1" on a `bl` is the relocation, not a mismatch. Compare
the linked form or read `report.json`'s `matched_code_percent` for the verdict.

NEW: carve-fn_801F1510 | match | MetroidPrime/Carve801F1510 | `fn_801F1510` (0x801F1510..0x801F1554, 0x44 = 68 bytes) is a leaf 25-byte-aggregate assignment - `cmplwi r3,0 / beqlr` then member-wise copy with interleaved loads and stores - and is the stand-in `stub_carve801f14f0_0` stands in for today