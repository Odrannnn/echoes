# progress-cgamestate-memcpy

`kind: progress`, target `MetroidPrime/Player/CGameState`, head `efcdbce`, lane L2, 2026-09-29.
**`fn_80142800` (0x80142800, 276 bytes) is written and at 100.00%.** Unit **88 -> 89 / 116**,
global `matched` **9441 -> 9442**, `linked` 4777 unchanged, no function anywhere worse, unit stays
`NonMatching`. Diff is one file, one function: `src/MetroidPrime/Player/CGameState.cpp`.

## The wall in the item text is gone, and the reason it was a wall is not the copy

The item said "94.57%... the residue is a `memcpy` call site that must not become a hand-written
loop". Re-measured from scratch at this head: the function was **bodiless** (no score in
`build/report.json`, absent from `CGameState.o`), so there was no 94.57% to improve - it had to be
written. The `memcpy` reading is what kept the previous attempts at 45-94%: **`memcpy` is an
out-of-line `bl` in this build** (the probe object emits `bl memcpy` and the function is 144 bytes
against retail's 276, 18 instructions away), because `libc/string.h` declares it as an ordinary
external function and the
build's `-inline deferred,noauto` does not expand it. Retail's bytes are not a `memcpy` call at all
but mwcceppc's own **8-way-unrolled byte copy**, which is what a copy *loop* compiles to here - the
`Matching` unit `src/MetroidPrime/Player/CGameStateBlockCopyCtor.cpp` (retail `fn_80004AA0`)
reproduces the identical loop with `for (int n = count; n != 0; --n) { *to++ = *from++; }`.

## The body, and the three things in it that are load-bearing

`extern "C" SGameStateBlock* fn_80142800(SGameStateBlock* self, const SGameStateBlock* src)`:
self-assignment returns; `fn_80142914(self)` (`x04_count = 0`); an empty source frees
`self->x0c_data` and zeroes `+0x04/+0x08/+0x0C`; otherwise `fn_801465EC(self, src->x04_count)`
and a byte copy of `src->x04_count` bytes, then `self->x04_count = src->x04_count`; return `self`.

1. **The emptiness test is signed**: `static_cast< int >(src->x04_count ) == 0`. Retail's is
   `cmpwi r4,0` (0x80142830); the same test on the `u32` field emits `cmplwi` and the function
   sits 4 instructions off. Measured: int-cast 3 diffs, u32 4, `!x` 4, `x <= 0` 4.
2. **The copy is bounded by a pointer range**, `const uchar* const end = from + src->x04_count;
   while (from != end)`. Retail computes `end` and compares *pointers* (0x80142868 `add`,
   0x8014286C `cmplw`, 0x80142870 `subf r3,r5,r0`), and the `subf` is what makes the compiler
   emit the unrolled copy. An int-count loop reproduces retail's size (276 bytes) but is **40
   instructions** away; a pointer range spelled `while (from != from + count)` collapses the
   expansion to 168 bytes (29 diffs).
3. **The destination pointer is declared first** (`to`, then `from`, then `end`). The three loads
   come out in retail's order either way - source, count, destination, 0x8014285C-0x80142864 - but
   the unrolled loop's register assignment is not: with `from` first the source lands in `r4` and
   the destination in `r5` and every `lbz`/`stb` of the loop is swapped (30 diffs vs 3).

Full sweep, `tools/bytescmp.py` ranking differing instructions of 69 (relocations counted, and the
three `bl` fields are the only ones that can differ - objdiff ignores them):

| spelling | differing instrs | our bytes |
|---|---|---|
| `to` first, pointer range, signed test | **3** | 276 |
| same, `from` first | 30 | 276 |
| same, `void*` locals | 30 | 276 |
| same, `for (; from != end; ++from, ++to) *to = *from;` | 30 | 276 |
| pointer range spelled `from != from + count` | 23 | 164 |
| int-count loop, signed test | 50 | 272 |
| `memcpy(self->x0c_data, src->x0c_data, src->x04_count)` | 18 | 144 |

(All seven rows measured against the same head, except that the first four were run before the
declaration order was fixed - the order changes no instruction, only where the function sits.)

## The probe is the reason this item took minutes, and it is re-creatable in 8 lines

Two throwaway scripts under `.tmp/opencode/` (untracked, disposable - **recreate them if the tree
was cleaned**): `probe_gs.sh` compiles the unit in **0.54 s** using `build.ninja`'s exact
`mwcc_sjis` cflags through `wibo` + `sjiswrap.exe` + `mwcceppc.exe` (note: an argv *array*, because
bash word-splitting breaks `-pragma "cats off"` into two arguments and the compile then dies in
0.05 s with a wall of missing headers), and `try.py <variants.py>` swaps a function body for each
variant, re-probes and prints the `bytescmp` count, restoring the source in a `finally`. This is
the harness the earlier `NEW: progress-cgamestate-probe` item asked for; **do not file it again**,
it is measured and working. `tools/probe_cc.sh` still cannot compile this unit (it omits
`-i extern/musyx/include` and the four `-DMUSY_*` defines).

## Decl order, fifth time: this edit was permuted before the check, and the check caught it

I put `fn_80142800` **after** `fn_80142914` in the source, which is right by address (0x80142914 >
0x80142800) and wrong for the compiler: mwcceppc emits in reverse, so it emitted
`fn_80142914, fn_80142800` and `check_decl_order.py` named the unit. **A new function belongs
before every definition with a higher retail address** - i.e. *ascending* in the source. The three
loads and the register assignment were correct with the bad order too, and objdiff still said
100.00%; only `check_decl_order` saw it. Swapping the two definitions fixed it and changed no
instruction.

## Gates, all re-measured on this tree

    ./tools/decomp_build.sh MetroidPrime/Player/CGameState
      All: 29.09% fuzzy, 21.23% matched, 11.37% linked (9442 / 28465 functions)
      main/MetroidPrime/Player/CGameState: 80.17% fuzzy, 51.72% matched (89 / 116 functions)
    python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json
      matched 9441 -> 9442   linked 4777 -> 4777   (+1 function at 100%, no regression)
    ./tools/gate.sh build/goal/judge/report.base.json
      every step ok except "docs claims", which asks for
      'matched 9442 / 28465' and 'DOL units 8072 / 16726' in the HANDOFF state block.
      `python3 tools/check_docs_claims.py --write` then reports "docs claims agree with the tree",
      which is what the judge does (MP_GATE_DOCS_WRITE=1). I reverted the file and left docs alone.
    sha1sum build/G2ME01/main.dol                   6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
    python3 tools/check_symbol_names.py            484 units, 0 declared names missing
    python3 tools/check_decl_order.py --all        935 checked, 28 permuted, all accounted for
    ./tools/probe_sources.sh                       734 files, 0 failed; LINKED (254 undefined, 0 dups)
    port undefined                                 254 -> 254 (build/goal/judge/undef.base.count)

`flip_test.sh` deliberately not run: the unit is 89/116 and cannot flip.

## A previous item's agent kept writing into this worktree after the driver released it

**This is the finding worth reading.** `build/goal/run.log` records item 3
(`progress-cgamestate-466f4` - this item's own `deps` entry) as "agent 'worker' exited 1 after
432s" at 19:29:02Z, then "resetting the worktree after an agent error" at 19:30:02Z, then
launching me at 19:30:02Z. But `git reflog` shows four `reset: moving to HEAD` entries at 21:35,
21:41 x2 and 21:42 local - during my run - and at 21:37 the file already contained a rewrite of
**`fn_8014601C`** that I had not made: an `EnvVarNodeCast` template plus `fn_8014601C` re-spelled
from a two-word out-parameter to a **returned** `rstl::map<...>::const_iterator`, with the
`const_iterator(it)` copy-construct on the return. My tree was clean at 21:31.

**Measured: that leftover is worth a function.** With it in the tree the unit read 90/116 and
`fn_8014601C` read **100.00%** instead of 99.05%; with only my `fn_80142800` it reads 89/116 and
`fn_8014601C` is back at 99.05% (I rebuilt each state). Its reasoning is also the answer to the
`NEW: progress-cgamestate-epilogue` wall: the function is not an epilogue problem at all, it is a
*return-slot* problem - `red_black_tree::const_iterator` declares its own constructor, so
mwcceppc hands the two-word return slot to the callee in r3, and that is the only shape that
reloads the link register first.

**I reverted it** (`git apply -R` of that one hunk) because it is not this item's work and a
diff carrying it reads as an unrelated fix. The patch is at
`.tmp/opencode/leftover-fn-8014601C.patch` in my worktree - untracked, so the driver's `git clean`
will take it; the shape is in the paragraph above and the next attempt of
`progress-cgamestate-466f4` should not have to re-derive it. **The driver should check that a
released agent's process is really gone before re-launching the worktree**: an agent that is judged
on the next item's diff is a silent wrong answer, and nothing in the judge or the gate flags it.

## Not done, and why

* **`fn_801466F4` (66.63%)** is this item's `deps` entry and is untouched - it is the 36-byte
  block's `reserve`, a different function.
* **No `NEW:` item filed.** Every remaining function in this unit is already covered by an
  existing one (`progress-cgamestate-bodiless-runs-remaining`, `-map-lowerbound`,
  `-loadcompressed`, `-cgmfrontend-dtor`, `-466f4`, `-epilogue`), and `fn_801465EC` is the one the
  queue tells this item to skip. Filing a sixth name for the same bodies would cost a lane for
  nothing, and the driver would have to triage it out by hand.
* Still bodiless at this head, unchanged by this edit: `fn_80142288` 76B, `fn_801422D4` 108B,
  `fn_80142760` 124B, `fn_80142944` 104B, `LoadCompressedMultiplayerOptions` 140B,
  `LoadCompressedGameOptions` 148B, `LoadGameFileState` 488B, the `CGMFrontEnd` ctor/dtor and
  `fn_80143CD4` (140+180+80B), `fn_80144818` 200B, the `fn_80145B90`/`fn_80145BDC` map pair,
  `fn_80146338` 440B, `fn_801465EC` 264B.

## Files touched

- `src/MetroidPrime/Player/CGameState.cpp` - `fn_80142800`, declared between `fn_801429AC` and
  `SetCompressedGameOptions`, immediately after `fn_80142914`, with the sweep table's reasoning in
  the comment
- `.tmp/opencode/{probe_gs.sh,score.sh,try.py,declorder.py,v1..v6.py}` - the untracked probe
  harness (not part of the diff)
