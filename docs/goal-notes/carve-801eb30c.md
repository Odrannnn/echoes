# carve-801eb30c — `MetroidPrime/ScriptObjects/Carve801EB30C` (match)

**Result: `goal_check: PASS`.** The unit is `Matching`, `complete`, **2/2 functions at 100.00%**,
`main/MetroidPrime/ScriptObjects/Carve801EB30C` — 104 bytes of 104, `matched_functions` 2, and
`flip_test.sh` kept it. Measured totals: **matched 13487 → 13489, linked 6535 → 6537**,
`total_functions` **28465** unchanged, port **286 undefined / 0 duplicates** (unchanged from the
branch head's 286), `probe_sources.sh` **924 files, 0 failures**.

## What the two functions are, and how each was identified

Both were written from their byte-shape twins; the first twin is *exact* (same 18 instructions,
same operand schedule, only the `bl` target differs), so the identification is measured rather
than guessed.

| function | addr | size | twin | twin's source |
| --- | --- | --- | --- | --- |
| `fn_801EB30C` | 0x801EB30C | 0x48 = 72 B | `rstl::reserved_vector<SDSPStreamVoice,4>::push_back(const SDSPStreamVoice&)` (0x8033D030, `symbols.txt:15280`, `size:0x48`) | `include/rstl/reserved_vector.hpp:57-60`, `construct(data() + mCount, in); ++mCount;` |
| `fn_801EB354` | 0x801EB354 | 0x20 = 32 B | `fn_80004438` (0x80004438, 0x20) | `src/MetroidPrime/Carve80004438.c:97`, `rstl::destroy`'s whole body is one call |

`fn_801EB30C`'s twin's bytes, at `build/G2ME01/asm/Kyoto/Audio/CDSPStreamManager.s:1713-1732`, are
word for word the 18 instructions here, with `bl "construct<15SDSPStreamVoice>__4rstlFPvRC15SDSPStreamVoice"`
in place of `bl fn_801EB354`. `fn_801EB354`'s is `stwu / mflr / stw / bl / lwz / mtlr / addi / blr`:
no callee-saved register is touched, which is what says the second argument is not used after the
call, and there is no load, test or returned value — the same reading `Carve80004438.c` already
records for its own twin.

## The element and the array, measured from retail and not assumed

`fn_801EB374` (0x801EB374, `symbols.txt:7909`, 0x4C = 76 B), the function `fn_801EB354` calls, is
`rstl::construct`'s body: `cmplwi r3,0` / `beqlr` is the same null guard
`construct<15SDSPStreamVoice>__4rstlFPvRC15SDSPStreamVoice` (0x8033D078) opens with, then a
member-wise copy of a 0x20-byte element — a `lhz` at +0 and seven `lfs` at +4..+0x1C. The 0x20
stride is confirmed from three more places: `slwi r0, r0, 5` here, `addi r5, r5, 0x20` in
`fn_801EB3C0` (0x801EB3C0, `symbols.txt:7910`, 0x3C), which walks the same array comparing that
same 2-byte key at +0, and `slwi r0, r3, 5` + `addi r4, r4, 0x4` in
`RemoveSafeZone__16CSafeZoneManagerFRC9TUniqueId` (0x801EB1E8). The capacity is 0x40 = 64, from
`cmpwi r0, 0x40` / `bge` at 0x801EB29C in the caller.

**The call sites are retail's and cannot be dropped.** `grep -rn 'bl fn_801EB30C' build/G2ME01/asm/`
returns exactly one hit (0x801EB2AC, inside `fn_801EB230`, 0x801EB230, `symbols.txt:7906`, 0xDC);
`fn_801EB354` is called once, from `fn_801EB30C` at 0x801EB330; `fn_801EB374` once, from
`fn_801EB354` at 0x801EB360. So `fn_801EB374` is **above** the claim and is declared, never
defined here — for the DOL dtk's own `auto_*` object supplies it, for the port link it is the
announced empty-body stand-in `stub_carve801eb30c_0` added below. Nothing here claims 0x801EB374
is decompiled.

## The one spelling finding worth keeping (a codegen rule, not a wall)

`fn_801EB30C` compiles to 18 instructions under **every** spelling tried, but three of them differ
between them, and always the same three: the `add r3, r31, r0` / `addi r3, r3, 0x4` pair.

**Retail orders it scaled-index-add first, constant-offset second**
(`slwi r0,r0,5` ; `add r3,r31,r0` ; `addi r3,r3,4`). MWCC folds the `+4` into the index term instead
(`slwi r3,r0,5` ; `addi r3,r3,4` ; `add r3,r31,r3`) whenever the source makes the +4 part of the
same expression as the scaled index. What forces retail's order is spelling the array the way
`rstl::reserved_vector` spells it — a `uchar mData[]` member, indexed through a pointer **cast** to
the element type:

* **18/18 byte-exact** (measured with this unit's own flags, `.tmp/opencode/carve/try.sh`):
  `(Elem*)self->data + self->count`; `self->data + self->count * 0x20` through a `unsigned char*`
  local; `(Elem*)((unsigned char*)self->data + self->count * 0x20)`.
* **3 of 18 differing** (same 3 either way): `&self->data[self->count]` on an `Elem[0x40]` member,
  `(void*)(self->data + self->count * 0x20)`, and `(unsigned char*)self + count * 0x20 + 4`.
* **19 instructions** (r30 spilled as well): a local holding the count across the call,
  `unsigned int n = self->count;`. Retail has no such local, and this is the mechanical reason the
  member is read twice instead.

So the rule is the generalisation of the `IsAllocValid` operand-order finding in
`RUNNING_THE_DECOMP.md`: *MWCC's register assignment and instruction order are a function of how
the expression is written.* Here the shape that decides it is a byte-array member indexed through a
cast, which is why the twin's own source needed no adaptation at all. What is **not** settled, and
was not needed: whether the `+4` is the array's data offset or the element's own first member —
`fn_801EB374` never dereferences `r3` before the copy writes it, so retail's own bytes do not say,
and the header's `mCount`-then-`mData` layout is the reading, not a measurement.

## Files touched (the four of a carve, plus the port stand-in)

* `src/MetroidPrime/ScriptObjects/Carve801EB30C.c` — new, 2 functions, descending by address
  (`fn_801EB354` at 0x801EB354 first, then `fn_801EB30C` at 0x801EB30C), plain C so the `fn_`
  names do not mangle.
* `config/G2ME01/splits.txt` — `MetroidPrime/ScriptObjects/Carve801EB30C.c: .text start:0x801EB30C
  end:0x801EB374`, placed in address order between `Carve801E8AEC.c` (ends 0x801E8AF4) and
  `MetroidPrime/CActorField25.cpp` (starts 0x801ECD8C). `total_functions` still **28465**.
* `configure.py` — `Object(Matching, "MetroidPrime/ScriptObjects/Carve801EB30C.c")`, one line.
* `files.cmake` — `src/MetroidPrime/ScriptObjects/Carve801EB30C.c`, one line.
* `src/MetroidPrime/PortLinkStubs.cpp` — `stub_carve801eb30c_0() asm("fn_801EB374")`, announced
  empty body, keyed to the unit that asks for it (the `stub_carve80256d1c_0` precedent), so the
  port's undefined count is **286 → 286** rather than 287. Without it `link_gap.py` reports
  `gap grew: fn_801EB374 is not in port_link_gap_list.md` and the gate's `port link gap` step fails.

`docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` show as modified in `git status`: those are
`gate.sh`'s own `MP_GATE_DOCS_WRITE=1` rewrites of the derived counts (923 → 924 probe files, state
block), not edits of mine.

## Verification, in the order it was run

```
python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/Carve801EB30C
  ok: 0 unit(s) checked, none emits its functions out of retail order
./tools/unit_fit.sh MetroidPrime/ScriptObjects/Carve801EB30C.c
  .text  claimed 104  ours 104  retail 104  fits
  no extra functions: our object defines only what the retail unit object does
./tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801EB30C.c
  PASS  -> kept as Matching      kept: 1 / 1   failed: 0   skipped: 0
./tools/link_check.sh
  link_check: compile errors 0 / unique undefined symbols 286 / duplicate definitions 0
./tools/goal_check.sh build/goal/item.json
  goal_check: PASS carve-801eb30c
```

`gate.sh` inside the judge was green on every step: configure, ninja + `build.sha1` (DOL
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, 86 RELs), hashes vs `config.yml`, per-function report
diff (no `WORSE`/`GONE`/`UNLINKED`/`FELL`), module wiring, decl order, `files.cmake`, raw offsets,
gs offsets, docs claims, port probe (924 files, 0 failures, link LINKED at 286 undefined /
0 duplicates), port link gap, port link dups, reach stubs.

## Note for the tool

`tools/carve_diff.sh` reads `build/G2ME01/main.elf`, so **once the carve unit is `Matching` the
elf holds our bytes, not retail's** — it reports our own object as the "retail" side and says
`NOT byte-exact` for a perfect carve. It is only usable before the flip. For a check that survives
the flip I used `build/G2ME01/asm/MetroidPrime/ScriptObjects/Carve801EB30C.s`, which dtk writes
from the target's retail bytes and which I read against the pre-claim
`build/G2ME01/asm/auto_03_801E8AF4_text.s:2792-2823`: **all 26 instructions identical**, including
the `bl` displacements' operands (`slwi r0, r0, 5` / `add r3, r31, r0` / `addi r3, r3, 0x4` in that
order). That file is generated by the configure step, so it is only present once the split exists.

No link-order cycle: the claim is in the **middle** of dtk's `auto_03_801E8AF4_text`, which starts
at 0x801E8AF4 exactly where `Carve801E8AEC.c` ends — but that boundary already existed, and a
carve at an auto object's *end* is the case that cycles. Measured: `dtk dol split` and the link
both succeeded.

No `NEW:` line: `fn_801EB374` is a leaf whose 19 instructions are `construct`'s member-wise copy of
a layout retail never names, and nothing in the port reaches it — it is a carve-shaped item of its
own if anyone wants it, but it is not this item's claim and filing it here would be scope creep
rather than a new blocker.
