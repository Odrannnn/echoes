# cmorphball-unclaimed-vtables-and-8033d2f4 (match, `MetroidPrime/Player/CMorphBall`)

Lane 6, 2026-10-01. **Result: `goal_check` PARTIAL** — the unit's matched count rose
**108 -> 109 of 158**, every other check green, no asm, no judge-owned path touched.
**`fn_800CD460` reached 100.00%** (0x800CD460, 88 B) — the cheapest function the previous run
named, and the one that run could not land because of `fn_8033D2F4`.

## What I changed

Three files. No `configure.py`, no `config/`, no `splits.txt`, no `files.cmake`, no `.s`, no asm,
nothing under `tools/` or `build/goal/`.

1. **`src/MetroidPrime/PortGlobals.cpp:995-1067`** — host definition of **`fn_8033D2F4`**, beside the
   `fn_8033D2EC` it already hosts eight bytes earlier in the same unclaimed gap. No new file and
   **no `files.cmake` line**, which is smaller than the `Port*.cpp` + entry the previous run's
   `NEW:` line proposed and lands the same thing: `fn_8033D2F4` is what made the previous run's
   `fn_800CD4B8`/`fn_800CD460` bodies unwriteable.
2. **`src/MetroidPrime/Player/CMorphBall.cpp:431-490`** — `fn_800CD4B8` and `fn_800CD460`, with the
   `extern "C" void fn_8033D2F4(void*)` declaration at `:457`.
3. **`docs/research/raw_offsets.md`** — the enforced per-file heading for `CMorphBall.cpp` went
   `1 site` -> `3 sites`, with the two new Kind A entries. **`tools/check_raw_offsets.py` fails a
   source edit that adds a raw offset without this**, and both new bodies reach their fields by
   retail's own `+12` / `+24`. The summary total line was stale before this change and is corrected
   from the tool's own output (162 -> 164 sites; the file count was already 69, not 68).

## `fn_8033D2F4` is a real definition, and why it is host-only

Measured, not assumed:

```
$ for f in $(find build/G2ME01/obj -name '*.o'); do nm --defined-only $f | grep -q ' fn_8033D2F4$' && echo $f; done
build/G2ME01/obj/auto_03_8033D2EC_text.o          # also lbl_803B1750 -> auto_07_803B16AC_data.o
                                                 #       lbl_803B36F0/FC -> auto_07_803B36F0_data.o
$ grep -c "auto_03_8033D2EC_text.o" build.ninja
1                                                 # it IS in the main.elf link
```

So the **DOL never needed anything from us** — dtk's auto-split already supplies all four
symbols, and `main.dol`'s sha1 is unchanged. The *host* link did need them, and only because this
change writes out a body that calls one. `link_gap.py` would have failed `gap grew: fn_8033D2F4 is
not in port_link_gap_list.md`.

What the function is, read off `./tools/dis.sh 0x8033D2EC 0x134`: the free half of MWCC's
small-block allocator, in the unclaimed `.text` gap `0x8033D2EC..0x8033D420`. Its family is
`fn_8033D2EC` (pool base), `fn_8033D2F4` (free, this one), `fn_8033D358` (alloc, 32-byte aligned)
and `fn_8033D3BC` (alloc, 4-byte aligned) — a LIFO allocator with a 16-entry table
(`cmplwi r5,16 / bge`) and a 0x4000 ceiling (`cmplwi r4,16384 / bgt`). The body written is retail's
own instruction sequence over the same five pieces of state; it calls nothing and allocates
nothing, so it is bookkeeping, not a stub. **Two deliberate deviations**, both recorded in the
file's own header: the two address words are `uintptr_t` and not `uint` (narrowing a host pointer
to `uint` truncates it, and review rule 4 is about exactly that), and the table index is masked
because retail's `lwzx r0,r4,r0` reads `table[-1]` when the count is 0.

## Measured

```
$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11522 -> 11523   linked 5590 -> 5590
  ok    check_symbol_names.py
  ok    All:  33.03% fuzzy, 25.90% matched, 12.18% linked (11523 / 28465 functions)
  flip  flip_test MetroidPrime/Player/CMorphBall.cpp: FAIL - judged below as partial progress
            build failed: mwldeppc undefined: 'CAnimRes::kDefaultCharIdx', 'fn_800C88C0',
                                               'fn_800C33DC', 'fn_800CD35C', 'fn_800CD244',
                                               'fn_800C9380'
  ok    target rose: main/MetroidPrime/Player/CMorphBall: 108 -> 109 / 158 functions
  ok    no asm added
goal_check: PARTIAL cmorphball-unclaimed-vtables-and-8033d2f4 - flip_test ...: FAIL, but the
target rose; commit it and keep the item
```

Per function (`build/report.json`): `fn_800CD460` **absent -> 100.0**, `fn_800CD4B8` **absent ->
96.31579**. Unit `matched_code` **14672 -> 14760** of 66600 (exactly `fn_800CD460`'s 88 bytes),
`.text` fuzzy **28.991352 -> 29.343304**, `matched_functions` **108 -> 109**. The functions with
**no body in our object at all** went from ten to eight (`fn_800CD460` and `fn_800CD4B8` are the two
that left that list).

`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, unchanged. All 86
RELs unchanged (`hashes vs config.yml ok`). `build/gate-probe.log`: `probe: 754 files, 0 failed,
0 errors; link: LINKED (249 undefined, 0 duplicates)` — **249 against the judge's recorded 249**,
i.e. the reference this change adds is closed by the definition it adds, and the flip test's own
`build-port-link/link_undefined.txt` is 249 lines. `build/gate-link.log`: `ok: 245 MISSING
symbol(s), all accounted for in port_link_gap_list.md`. `docs claims agree with the tree`.
`python3 tools/check_raw_offsets.py`: `ok: 164 raw-offset site(s) in 69 file(s), all documented`.
`config/G2ME01/splits.txt` untouched, so its 28465 `total_functions` are as they were.

**The flip's `undefined:` list is 6 distinct names, down from 8 — both blockers this item names are
gone.** Measured at both ends rather than inferred, by running `tools/flip_test.sh` on the stashed
tree and then on this one and diffing the `undefined:` sets:

```
$ # at HEAD (sources stashed)
CAnimRes::kDefaultCharIdx  fn_800C33DC  fn_800C88C0  fn_800C9380
fn_800CD244  fn_800CD35C  fn_800CD460  fn_800CD4B8          # 8
$ # after this change
CAnimRes::kDefaultCharIdx  fn_800C33DC  fn_800C88C0  fn_800C9380
fn_800CD244  fn_800CD35C                                       # 6
$ diff -> 7,8d6   (fn_800CD460 and fn_800CD4B8 removed)
```

`fn_800CD460` was already undefined at HEAD, because `FindClosestSpiderBallWaypoint` calls it and
the flipped link substitutes our object, which did not define it. What is left is
`CAnimRes::kDefaultCharIdx` plus five of the unit's still-unwritten functions.

`tools/unit_fit.sh`: `.text claimed 66600 ours 25004 retail 66600 SHORT by 41596`, and the
"present in ours but not in the retail unit object" list is **unchanged** by this diff — measured by
stashing the two source edits, rebuilding, and diffing the two lists: identical, 51 functions /
5272 bytes of COMDAT weak template and inline-virtual copies in both.

`python3 tools/check_decl_order.py --unit main/MetroidPrime/Player/CMorphBall` still reports the
unit permuted, as it did at HEAD and as `docs/research/decl_order.md` already lists it; the gate's
`decl order` step is `ok`.

## The flip is still unreachable, and that is unchanged

Recorded here so the next lane does not re-derive it: the unit has **49 functions below 100%
covering 51840 of its 66600 bytes**, `UpdateEffects` (4448 B), `Render` (4024),
`__ct__CMorphBall` (3956), `CollidedWith` (3940) and `ComputeBoostBallMovement` (2812) are the
largest of them. This is a `progress`-shaped unit with ~52 kB to write; the item stays queued.

## `fn_800CD4B8` is 96.31579%, and I stopped there deliberately

**No matched-function gain is available below 100%**, and I verified that: objdiff's
`matched_functions` and `matched_code` both count only functions at exactly 100.0 — summing the
sizes of this unit's 100.0 functions gives 14672, which is exactly its reported `matched_code`.
So `fn_800CD4B8` contributes nothing to the judge's count whatever it scores, and spending the
remaining budget on it would buy nothing. What it does buy is `matched_code` fidelity and a real
body, which is what landed.

**The three instructions that differ, from ours at 0x54b0 against retail at 0x800cd4b8:**

| retail | ours | note |
| --- | --- | --- |
| `rlwinm. r0,r0,27,31,31` | `rlwinm. r0,r0,0,27,27` | both are `flags & 0x10`; retail leaves the result at bit 31, MWCC puts it at bit 27 |
| `rlwimi r4,r0,2,28,29` | `rlwimi r4,r0,4,26,27` | both land outside bits 0-7, so the following `stb` writes the byte unchanged in **both** |
| `rlwimi r3,r3,1,26,26` | `ori r0,r3,64` | see below |

The first two are register form only: the extracted value is the same and the stored byte is the
same. **The third is a real divergence and is stated here rather than hidden:** retail's
`rlwimi r3,r3,1,26,26` writes word bit 26, which the following `stb` discards, so retail's second
store leaves the flag byte alone; our `*flags |= 0x40` sets byte bit 6. The whole of retail's
sequence from 0x800cd4fc onward is byte-neutral apart from the `rlwinm.` that sets CR0 — the branch
is the only observable effect. I could not find a C++ spelling that reaches it: MWCC has no
consistent model here, because `lbz`/`stb` leave the byte in bits 0-7 while the rotate immediates it
emits (`MB=28`, `MB=26`) address bits 28 and 26 of the same word.

**Spellings measured this run, so the next one does not repeat them** (each built and scored with
`tools/fast_try.sh`; `fn_800CD460` stayed at 100.0 in all of them):

- `*flags |= 0x40` — **96.31579%, kept.** Best of the four.
- `*flags |= ((*flags >> 1) & 1) << 2` — 93.68421.
- `*flags = (*flags & ~0x04) | ((*flags & 2) >> 1)` — not measured (my loop's replacement string was
  already gone after the first variant; the score printed for it was the previous variant's).
- `*flags |= ((*flags >> 5) & 1) << 6` — not measured, same reason.
- retail's arithmetic transcribed literally on a `uint` view of the byte —
  `(bits & ~0x30000000u) | (((count - 1) << 2) & 0x30000000u)` and
  `(after & ~0x04000000u) | ((after << 1) & 0x04000000u)` — **91.8421%**, worse than keeping the
  byte-level mask/shift arithmetic.

The previous run's 93.03% is beaten by 3.29 points, and its rejected spellings (count at bits 0-1
written back at bits 4-5 with `!= 0`, 91.58%; `uint*` instead of `uchar*`, 81.84%; a C++ bit-field,
worse still) are still worse than this one.

## Not done, and why — the three `.data` vtables

`lbl_803B1750` (0x10 B), `lbl_803B36F0` and `lbl_803B36FC` (0xC B each) are the only part of this
item's name I did not land. They are needed only by `fn_800C88C0` and `fn_800C33DC`, the two 92-byte
vtable destructors, and those two are worth **zero** to the judge:

- the previous run measured both at **53.91%** (19 instructions against retail's 23) across three
  spellings, because MWCC will not emit retail's two `lis`/`addi` pairs with a dead `beq` between
  them for two stores to one address;
- a function at 53.91% adds 0 to `matched_functions` and 0 to `matched_code` (both count only
  100.0 functions — measured above);
- host-defining them means inventing three vtable objects whose slots are retail function
  addresses, which is the "fake an asset" case rather than a transcription.

So the trade is recorded rather than banked, and the three symbols stay unclaimed.

## Files

- `src/MetroidPrime/Player/CMorphBall.cpp:431-490` — `fn_800CD4B8`, `fn_800CD460`, the
  `fn_8033D2F4` declaration, and the reading of which allocator each branch takes.
- `src/MetroidPrime/PortGlobals.cpp:995-1067` — host `fn_8033D2F4` and its five pieces of state.
- `docs/research/raw_offsets.md` — `CMorphBall.cpp` heading `1 site` -> `3 sites` (gate-enforced),
  plus the corrected summary total.

Not committed, per the brief.