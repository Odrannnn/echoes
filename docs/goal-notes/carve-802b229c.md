# carve-802b229c — `Kyoto/Animation/Carve802B229C` (kind `match`) — DONE

## What I did

Carved `fn_802B229C` (retail `.text 0x802B229C..0x802B22C0`, 0x24 = 36 bytes, 9 instructions) out
of the unclaimed dtk range `auto_03_802B2090_text` as its own `Matching` unit. Four files, all in
this worktree:

- `src/Kyoto/Animation/Carve802B229C.c` — new, 122 lines, the C those bytes are the compilation of
  plus a `TARGET_PC`-only host body for the callee (below).
- `configure.py:1153-1160` — `Object(Matching, "Kyoto/Animation/Carve802B229C.c"),` on its own line,
  in address order between `Carve802B2088.c` and `Carve802B2568.c`, with a comment.
- `config/G2ME01/splits.txt:2710-2711` — `.text start:0x802B229C end:0x802B22C0`, in address order
  between the same two neighbours.
- `files.cmake:1174` — `src/Kyoto/Animation/Carve802B229C.c`, in address order.

The body is one function:

```c
float fn_802B229C(void* self, const void* quadratic, const void* start) {
    return fn_802B22C0((char*)self + 4, quadratic, start);
}
```

## What I measured

**The twin is exact, `bl` aside.** The item's twin is `GetResourceIdByName__11CResFactoryCFPCc`
(retail 0x80006B80, 0x24), matched 100.00% inside `MetroidPrime/main.cpp`; its source is the
inline body at `include/Kyoto/CResFactory.hpp:64-65`. Both sides disassembled this run out of
`build/G2ME01/main.elf`, before the claim was listed:

| off | retail `fn_802B229C` | twin `GetResourceIdByName__11CResFactoryCFPCc` |
| --- | --- | --- |
| 0x00 | `94 21 ff f0 stwu r1,-16(r1)` | identical |
| 0x04 | `7c 08 02 a6 mflr r0` | identical |
| 0x08 | `38 63 00 04 addi r3,r3,4` | identical |
| 0x0C | `90 01 00 14 stw r0,20(r1)` | identical |
| 0x10 | `48 00 00 15 bl 802b22c0` | `48 2f 60 b5 bl 802fcc44` |
| 0x14 | `80 01 00 14 lwz r0,20(r1)` | identical |
| 0x18 | `7c 08 03 a6 mtlr r0` | identical |
| 0x1C | `38 21 00 10 addi r1,r1,16` | identical |
| 0x20 | `4e 80 00 20 blr` | identical |

8 of 9 words identical; only the `bl` LI differs.

**Return type.** The callee settles it. `fn_802B22C0` (0x802B22C0, 0x140) is a Newton iteration on
a quadratic over three `float*` (`lfs 0x0(r3)` / `lfs 0x4(r3)`, `lfs 0x0(r4)`, `lfs 0x0(r5)`),
`li r0,0x4` / `mtctr r0` / `bdnz` over five `fmadds`/`fsubs`/`fdivs` passes, each
`fabs`-and-`frsp`-compared against 1e-5f with a `bltlr` out, and `-1.0f` into `f1` when all five
fall through — so it answers in `f1` and the forwarder needs no instruction of its own. Constants
read out of the DOL's `.sdata2` (the 0x8041E410 block) this run: `lbl_8041E418` = 2.0f,
`lbl_8041E41C` = 0.5f, `lbl_8041E420` = 1e-5f, `lbl_8041E424` = **-1.0f**.

**Argument count, measured not guessed.** The first compile failed: I had written the forwarder
with four parameters and the callee with three, and mwcceppc reported `function call ... does not
match`. The callee touches only `r3`/`r4`/`r5`, so the forwarder takes three.

**Verdicts.**

```
$ ./tools/carve_diff.sh 802b229c 24 build/G2ME01/src/Kyoto/Animation/Carve802B229C.o
retail: 9 instructions, 36 bytes
ours  : 9 instructions, 36 bytes
  +4   retail: 802b22ac bl 802b22c0 <fn_802B22C0> ours: 00000010 bl 10 <fn_802B229C+0x10>
differing instructions: 1
NOT byte-exact
```

The one difference is the `bl`, unrelocated in a relocatable object; every other word is identical.

```
$ ./tools/unit_fit.sh Kyoto/Animation/Carve802B229C.c
   .text      claimed     36   ours     36   retail     36   fits
   no extra functions: our object defines only what the retail unit object does
$ python3 tools/check_decl_order.py --unit Carve802B229C
ok: 1 unit(s) checked, none emits its functions out of retail order
$ ./tools/flip_test.sh Kyoto/Animation/Carve802B229C.c
  PASS  -> kept as Matching
$ sha1sum build/G2ME01/main.dol
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
$ ./tools/goal_check.sh build/goal/item.json
goal_check: PASS carve-802b229c
  ok    gate.sh ...  ok    counts: matched 13634 -> 13635   linked 6682 -> 6683
  ok    check_symbol_names.py
  ok    All:  37.71% fuzzy, 31.14% matched, 14.01% linked (13635 / 28465 functions)
  ok    flip_test Kyoto/Animation/Carve802B229C.c: PASS, Object(Matching) in configure.py
```

`build/report.json`: `main/Kyoto/Animation/Carve802B229C` = **1 / 1 functions, 100.0%,
`complete: true`**; `total_functions` is still **28465**. `dtk dol split` re-split the old range
into `auto_03_802B2090_text` 0x802B2090..0x802B229C (2 functions) and
`auto_03_802B22C0_text` 0x802B22C0..0x802B2568 (7 functions); **no link-order cycle**, because the
claim touches no unit boundary.

## The one thing that was not in the item: the port link gap

The first `goal_check` run came back `FAIL`, on one check only, and the reason is worth carrying:

```
FAIL  gate.sh
      link_check: STRICT FAIL - regression gate: 288 undefined against a baseline of 287 (GREW),
      0 duplicate(s), 0 compile error(s)
      link_check: 5 symbol(s) this change ADDED to the gap:  ... NEW  fn_802B22C0
      link gap not accounted for:
        gap grew: fn_802B22C0 is not in port_link_gap_list.md
      GATE FAIL: probe link-gap
```

(Only one of those five `NEW` names is mine — `fn_8022A408`, `SCarve801FECACElement`,
`CSpawnSystemKeyframeData` and `CExplosion::CExplosion` are all already in
`build/goal/judge/undef.base.txt`; that list and `docs/research/port_link_baseline.txt` are two
different scrapes of the same link and they disagree on four symbols. Net growth was 287 -> 288,
i.e. exactly `fn_802B22C0`.)

**A carve's callee is a new undefined symbol in the port link, and that is a gate.** The DOL gets
`fn_802B22C0` from `auto_03_802B22C0_text.o`, which only `mwldeppc` links; the host link has nothing
that defines it, so `tools/probe_sources.sh --strict` fails. The repo's two remedies are a
`PortLinkStubs.cpp` block (what `stub_801e515c_0` is for `Carve801E515C.c`) or a `TARGET_PC` block
**in the carve file itself**. I took the second, because
`src/MetroidPrime/Carve8019C394.c:95-111` says why it is the better one:

> the host definitions go here, next to the declarations they complete, and not in
> `PortLinkStubs.cpp`: that file is generated (`tools/gen_link_stubs.py`), and
> `docs/research/boot_path_stubbable.tsv` is the input a regeneration would need.

`TARGET_PC` is defined for the port and for the syntax probes (`CMakeLists.txt:152,167`,
`tools/probe_sources.sh:49`) and **not** for the matching build, so the block cannot reach
`main.dol` — re-measured after adding it: `carve_diff` still 36/36 with only the `bl` differing,
`main.dol` sha1 unmoved, `probe: 1035 files, 0 failed ... 287 undefined, 0 duplicates`.

The host body is a transcription of retail's own `fn_802B22C0` instructions, not a claim that the
320 bytes are decompiled — no unit claims 0x802B22C0..0x802B2400 and they stay dtk's. It is a pure
float solver, so unlike most stand-ins here it can be run honestly on the host; it still logs
`[port-stub]`-style `[port-host] fn_802B22C0` on entry, because nothing on the PC boot path calls
it and the rule is that a stand-in announces itself.

## Nothing blocked me; nothing was already done

`build/report.json` on the clean tree had no `main/Kyoto/Animation/Carve802B229C` unit and
`fn_802B229C` unsourced, so this is not `STALE:`. No `WALL:` line: the function is 100.00% and
flipped. No `NEW:` lines — the port-link-gap finding above is a rule, not a queue item (it has no
target unit that would raise a count), so it belongs here.

## Caveat for the reviewer

The diff is a fifth file wider than the "a carve is four files" rule: the carve's own source
carries the host definition of its callee, which is the same shape of edit as
`Carve8019C394.c` and `Carve8019AC78.c`. Without it the item's own gate fails. If the reviewer
would rather see it in `PortLinkStubs.cpp`, moving the block is a mechanical follow-up and the
rest of the change stands.