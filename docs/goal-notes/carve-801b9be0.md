# carve-801b9be0 - `MetroidPrime/Carve801B9BE0` (match, lane 11)

**Result: PASS.** `tools/goal_check.sh <abs>/build/goal/item.json` -> `goal_check: PASS carve-801b9be0`
(gate.sh ok, `counts: matched 12582 -> 12585 linked 5944 -> 5947`, check_symbol_names ok,
`All: 35.44% fuzzy, 29.26% matched, 12.97% linked (12585 / 28465 functions)`,
`flip_test MetroidPrime/Carve801B9BE0.c: PASS, Object(Matching) in configure.py`).

## What was done

Wrote 3 unsourced functions in `src/MetroidPrime/Carve801B9BE0.c`, claiming
`.text 0x801B9BE0..0x801B9C68` (0x88 = 136 bytes) out of dtk's `main/auto_03_801B94B8_text` gap
(0x801B94B8..0x801BC900). A carve is four files, all four in this change:

- `configure.py` - `Object(Matching, "MetroidPrime/Carve801B9BE0.c"),` (one line) between
  `MetroidPrime/Carve801B94B4.c` and `MetroidPrime/Carve801BC900.c`.
- `config/G2ME01/splits.txt` - `MetroidPrime/Carve801B9BE0.c:` /
  `.text start:0x801B9BE0 end:0x801B9C68`, at the same position (address order).
- `files.cmake` - `src/MetroidPrime/Carve801B9BE0.c`, same position.
- `src/MetroidPrime/Carve801B9BE0.c` - definitions **descending by address**: `fn_801B9C28`,
  `fn_801B9C00`, `fn_801B9BE0`.

The item's three twins are exact, and each was re-measured here with
`build/binutils/powerpc-eabi-objdump -d --start-address=<a> --stop-address=<b> build/G2ME01/main.elf`.
None of the three touches a data address, so **only the `bl` destination differs from the twin**:

| function | addr | size | insns | twin | body written |
|---|---|---|---|---|---|
| `fn_801B9BE0` | 0x801B9BE0 | 0x20 | 8 | `__sys_free` 0x80008A28 0x20, `src/MetroidPrime/main.cpp` | `fn_801B9C00(dst, src);` - r3/r4 forwarded untouched |
| `fn_801B9C00` | 0x801B9C00 | 0x28 | 10 | `fn_80004D5C` 0x80004D5C 0x28, `src/MetroidPrime/Player/CGameStateBlockConstruct.cpp` | `if (dst != 0) { fn_801B9C28(dst, src); }` |
| `fn_801B9C28` | 0x801B9C28 | 0x40 | 16 | `fn_80026F28` 0x80026F28 0x40, `src/MetroidPrime/CAnimData.cpp` | `dst->x00 = src->x00; fn_801B9C68(&dst->x04, &src->x04); return dst;` |

So the group is the null-guarded copy-construction chain (`rstl::construct` shape) for one element:
the single caller in the DOL, `fn_801B9B18` (0x801B9B18, `size:0xC8`, ending exactly where the claim
begins), sets `r3 = element, r4 = source` and calls `fn_801B9BE0` inside a loop that advances **both**
pointers by 0x70 (`addi r27,r27,0x70` / `addi r28,r28,0x70`, 0x801B9B90..0x801B9BA4, stride also
visible as `mulli r0,r3,0x70`) - which is what fixes the element's size and the +0x4 member's offset.

`fn_801B9C68` (0x801B9C68, `size:0x148` = 328 bytes) is the +0x4 member's own copy constructor and is
**0x0 bytes past this claim's end**, so it stays retail's and the carve declares it `extern`. It did
need a port stub - see below.

## Verification (all measured in this lane's tree)

- `./tools/decomp_build.sh -r` -> `All: 35.44% fuzzy, 29.26% matched, 12.97% linked
  (12585 / 28465 functions)`; `total_functions` is still **28465** after the `splits.txt` edit.
- `build/report.json`: `main/MetroidPrime/Carve801B9BE0` is **3 / 3** functions, all `100.00%`
  (64, 40, 32 bytes), `complete: true`. The old gap split cleanly rather than collapsing:
  `main/auto_03_801B94B8_text` 54 -> **8** functions, `main/auto_03_801B9C68_text` **43**
  (8 + 3 + 43 = 54).
- `./tools/flip_test.sh MetroidPrime/Carve801B9BE0.c` -> `PASS -> kept as Matching`,
  `kept: 1 / 1 failed: 0 skipped: 0` (DOL sha1 and all 86 RELs hold with our object in the link).
- `sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
- `./tools/unit_fit.sh MetroidPrime/Carve801B9BE0.c` -> `.text claimed 136, ours 136, retail 136,
  fits`; `no extra functions`.
- `python3 tools/check_decl_order.py --unit MetroidPrime/Carve801B9BE0` -> ok (descending).
- `python3 tools/check_symbol_names.py` -> `checked 528 units; 0 declared names are missing`.
- `./tools/carve_diff.sh 0x801B9BE0 0x88 build/G2ME01/obj/MetroidPrime/Carve801B9BE0.o` ->
  `retail: 34 instructions, 136 bytes`, `ours: 34 instructions, 136 bytes`,
  `differing instructions: 3`, and all three are the call sites (`bl fn_801B9C00`,
  `bl fn_801B9C28`, `bl fn_801B9C68`) - unrelocated `bl` targets in a `.o`, which is why the tool
  prints `NOT byte-exact`; the link is what resolves them, and `flip_test` is the verdict.

## The one thing outside the four carve files: a port stub for `fn_801B9C68`

With the carve listed and no stub, the judge's gate failed:
`GATE FAIL: probe link-gap` -> `gap grew: fn_801B9C68 is not in port_link_gap_list.md`,
`port link gap  287  MISSING`, probe reporting `NEW fn_801B9C68` (`292 undefined` against the 291
baseline). Same treatment `Carve801F97C8.c` gave `fn_801F9848`, in `src/MetroidPrime/PortLinkStubs.cpp`:

```cpp
extern "C" void stub_186() asm("fn_801B9C68");
extern "C" void stub_186() {}
```

with a measured comment above it, and that file's header counts moved 159 -> **160** supplied,
155 -> **156** functions, 22 -> **23** unmangled `fn_`/`lbl_`. It is an empty stand-in, **not** a
claim that `fn_801B9C68` is decompiled, and it cannot reach `main.dol` (`PortLinkStubs.cpp` is not
listed in `configure.py`). Measured after: the probe prints
`776 files, 0 failed, 0 errors; link: LINKED (291 undefined, 0 duplicates)` and
`link_check: STRICT PASS - regression gate: 291 undefined against a baseline of 291 (no growth),
0 duplicate(s), 0 compile error(s)`.

## Caveats / for the next run

- **Environment hazard, cost real time here.** Several of this lane's shell invocations ran with
  another lane's `MP_GOAL_TREE` in the environment. `tools/goal_check.sh` *cd*s to `$MP_GOAL_TREE`
  before it resolves its item argument, so with a relative item path it judged **that other
  worktree** (printed another lane's item id, baseline and counts) - the run looks plausible and is
  about the wrong tree. Pin `MP_GOAL_TREE` / `MP_GOAL_JUDGE` / `MP_GOAL_BASE` / `MP_GOAL_LOGDIR` to
  the lane's own tree and pass an **absolute** item path. The verdict above was produced that way.
- `gate.sh` rewrites `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` (docs-claims step,
  `MP_GATE_DOCS_WRITE=1`); those writes were reverted, so the change is exactly the four carve files
  plus the stub.
- `fn_801B9C68` (0x801B9C68, 0x148 = 328 bytes) is still unsourced and is now a port stand-in: a
  0x10-byte header (`lhz` at +0, word at +0x4, `lfs`/`stfs` at +0x8, word at +0xC), then its
  2-byte-element run from +0x10 - eight `lhz`/`sth` pairs per `mtctr` iteration plus an `andi.`
  remainder loop - then words and floats from +0x20. The seed paired no twin for it and it was not
  spelled here, so **no `NEW:` line is filed** for it.

No `WALL:`, no `STALE:`, no `NEW:`.

---

# Second attempt, 2026-10-02 (lane 11, tree `wt-mp2-goal-L11` at `0fdd9e3d`) - PASS again

**Result: PASS.** `./tools/goal_check.sh <abs>/build/goal/item.json` ->
`goal_check: PASS carve-801b9be0`: `gate.sh` ok (DOL sha1, 86 RELs, report diff, wiring, docs
claims, port probe), `counts: matched 12590 -> 12593   linked 5952 -> 5955`,
`check_symbol_names.py` ok, `All:  35.45% fuzzy, 29.27% matched, 12.98% linked
(12593 / 28465 functions)`, `flip_test MetroidPrime/Carve801B9BE0.c: PASS, Object(Matching) in
configure.py`.

## Why the item came back (and what changed for this run)

The previous run passed its judge, but the driver could not carry that change onto `0fdd9e3d`:
`run.log` -> `carve-801b9be0 does not apply on 0fdd9e3 - releasing it for a fresh attempt`, with
`U src/MetroidPrime/PortLinkStubs.cpp` among the conflicts. That attempt's stub was `stub_186`,
and the new head's own `stub_186` (`fn_8000408C`, added for `Carve80004010.c`) had landed in the
same place. So this run's stub is `stub_187`, appended **after** that block, and the header counts
moved 160 -> **161** supplied, 156 -> **157** functions, breakdown's unmangled fn_/lbl_ 23 -> 24.
(The 86 REL loader / 46 game method fields were left as found - they cannot be derived from the
file; a raw prefix count of `fn_`/`lbl_` declarations is 24 before this change and 25 after, so
that field was already one behind its own accounting.)

## Re-measured on this tree before writing (nothing recalled)

- `objdump -d 0x801B9BE0..0x801B9C68 build/G2ME01/main.elf`: 8 / 10 / 16 instructions, 0x20 /
  0x28 / 0x40, exactly the previous run's readings; the three twins at 0x80008A28 (`__sys_free`),
  0x80004D5C (`fn_80004D5C`) and 0x80026F28 (`fn_80026F28`) are instruction-for-instruction the
  same apart from the `bl` target, re-disassembled here to confirm.
- Caller `fn_801B9B18` (0x801B9B18, ends exactly where the claim begins) sets `r3 = element,
  r4 = source` and calls `fn_801B9BE0` in a loop advancing **both** pointers by 0x70, with
  `mulli r0,r3,0x70` in the count - that is where the element size and the +0x4 member come from.
- `build/report.json` after: `main/MetroidPrime/Carve801B9BE0` **3 / 3**, 100.00% (64, 40, 32
  bytes); the gap split cleanly - `main/auto_03_801B94B8_text` 54 -> **8** functions,
  `main/auto_03_801B9C68_text` **43** (8 + 3 + 43 = 54); `total_functions` still **28465**.
- `tools/unit_fit.sh MetroidPrime/Carve801B9BE0.c` -> `.text claimed 136, ours 136, retail 136,
  fits`; `no extra functions`. `python3 tools/check_decl_order.py --unit
  MetroidPrime/Carve801B9BE0` -> ok.
- `tools/carve_diff.sh 0x801B9BE0 0x88 build/G2ME01/obj/MetroidPrime/Carve801B9BE0.o` ->
  `retail: 34 instructions, 136 bytes`, `ours: 34 instructions, 136 bytes`,
  `differing instructions: 3` - the three `bl`s, unrelocated in a `.o`.
- `tools/flip_test.sh MetroidPrime/Carve801B9BE0.c` -> `PASS -> kept as Matching`,
  `kept: 1 / 1 failed: 0 skipped: 0`; `sha1sum build/G2ME01/main.dol` ->
  `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
- Port link, the stub's own measurement: without the `stub_187` block,
  `python3 tools/link_gap.py --rebuild` exits 1 with `287 MISSING` and
  `gap grew: fn_801B9C68 is not in port_link_gap_list.md`; with it, the same command prints
  `286 MISSING` / `ok: 286 MISSING symbol(s), all accounted for in port_link_gap_list.md`, and the
  gate's probe reports `LINKED (291 undefined, 0 duplicates)`.

## Final shape of the change

`configure.py` + `config/G2ME01/splits.txt` + `files.cmake` + `src/MetroidPrime/Carve801B9BE0.c`
(the four carve files, claim `.text 0x801B9BE0..0x801B9C68`, definitions descending
`fn_801B9C28` / `fn_801B9C00` / `fn_801B9BE0`) plus the `stub_187` block in
`src/MetroidPrime/PortLinkStubs.cpp`. `fn_801B9C68` is still unsourced; the tail auto unit
(0x801B9C68..0x801BC900, 43 functions) is untouched.

The judge rewrote `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` again
(`MP_GATE_DOCS_WRITE=1`); both were reverted, so the diff is those four files plus the stub.

No `WALL:`, no `STALE:`, no `NEW:`.
