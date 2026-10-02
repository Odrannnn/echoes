# progress-twin-rel-ingboostballguardian

`kind: progress`, `target: module:IngBoostBallGuardian`. **Module matched_functions 24 -> 33
(of 318)**, all four new units `Matching` at 100.00%, `All:` 13089 -> 13098 matched, linked
6187 -> 6196, `total_functions` still 28465, DOL `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`
and all 86 RELs sha1-equal to `config/G2ME01/config.yml`. `./tools/goal_check.sh
build/goal/item.json` -> `PASS`.

## What I did

Claimed four more contiguous `.text` ranges in module 30 out of the unclaimed gaps, each its own
unit (one unit cannot claim two discontiguous ranges), each reproducing three or two functions
that are **byte-identical to a family this tree already reproduces at 100%**.

The family is `src/MetroidPrime/ScriptObjects/CLumiteRelTail.cpp`'s `fn_39_778`/`fn_39_798` - the
0x20 forwarder and the 0x28 guard - and its copy in this module is `fn_30_111C`/`fn_30_113C`. The
other three pairs are the same two shapes at three more places in module 30:

| range | functions | shape | callee left retail |
|---|---|---|---|
| `0x10DC..0x1164` | `fn_30_10DC` (0x40, the flag copy), `fn_30_111C`, `fn_30_113C` | `fn_39_738`/`778`/`798` | `fn_30_1164` (0x13C) |
| `0xA8C..0xAD4` | `fn_30_A8C`, `fn_30_AAC` | `fn_39_778`/`798` | `fn_30_F78` (0x74) |
| `0x4C10..0x4C58` | `fn_30_4C10`, `fn_30_4C30` | `fn_39_778`/`798` | `fn_30_4C58` (0xC0) |
| `0x16088..0x160D0` | `fn_30_16088`, `fn_30_160A8` | `fn_39_778`/`798` | `fn_30_388C` (0x54) |

`tools/twin_scan.py --list` independently pairs `fn_30_10DC` with `fn_39_738` (64 bytes, same
instructions once the call targets are masked), so the relationship is measured rather than read
off a name. The other six were found by scanning the module's asm for the two instruction shapes
(a 0x20-byte `stwu/mflr/stw/bl/epilogue` and a 0x28-byte one with `cmplwi r3,0` between the `mflr`
and the `stw`), which is how the item's twin list was extended.

Four files, all four of the carve's parts in one change:

- `src/MetroidPrime/ScriptObjects/CIngBoostBallGuardianFlagCopy.cpp` (new, 3 functions)
- `src/MetroidPrime/ScriptObjects/CIngBoostBallGuardianForwarderF78.cpp` (new, 2)
- `src/MetroidPrime/ScriptObjects/CIngBoostBallGuardianForwarder4C58.cpp` (new, 2)
- `src/MetroidPrime/ScriptObjects/CIngBoostBallGuardianForwarder388C.cpp` (new, 2)
- `config/G2ME01/rels/IngBoostBallGuardian/splits.txt` - four `.text` entries, one per unit
- `configure.py` - four `Object(Matching, ..., mw_version="GC/2.7")` in the existing
  `Rel("IngBoostBallGuardian", ...)` block, each with a comment carrying its measurement
- `files.cmake` - the four paths, each with an empty host branch (`#ifdef __MWERKS__`)

## The three spellings that mattered, and why each is load-bearing

1. **The guard is `if (self == 0) return;`, not `if (self) { call(); }`.** Retail's `cmplwi r3,0`
   sits *above* the saved-LR store, so only the early return schedules it there. This is
   `CLumiteRelTail.cpp`'s measured result for `fn_39_798` and it carries over unchanged.
2. **The flag copy is `fn_39_738`'s body**, and **`GC/2.7` per object** is what puts the
   `lbz r0,0x4c(r4)` above the `stw r31`. At the `Rel()` default of GC/1.3.2 every spelling that
   carries the r31 spill puts the spill first. Same difference `CLumiteRelTail.cpp` records for
   `fn_39_738`; I did not have to rediscover it, and I would not have reached it from the bytes
   alone.
3. **Descending declaration order.** `nm` confirms each object emits its functions at retail
   offsets in ascending order (`fn_30_A8C` at 0x0 size 0x20, `fn_30_AAC` at 0x20 size 0x28), which
   is what "declared descending in the source" means under mwcceppc's reverse emission.
   `tools/check_decl_order.py --unit <each>` and `tools/unit_fit.sh <each>` both clean
   (`claimed 136 / ours 136 / retail 136, fits` and `72/72/72, fits` for the three pairs, no extra
   functions).

## Dead-strip: measured, and the reason no `force_active:` entry was needed

None of the nine functions is in `build/G2ME01/IngBoostBallGuardian/ldscript.lcf`'s FORCEACTIVE
list. The recipe's trap (`ScriptCoin`'s tail) is a `Matching` unit whose `.text` gets dropped when
nothing references it. Each pair is safe because its 0x20 forwarder is **called from an unclaimed
function**, so dtk's own object holds the reference:

- `fn_30_10DC` <- 0x288 (inside `fn_30_130`, the module's entity loader, unclaimed)
- `fn_30_A8C` <- 0xA54 (inside `fn_30_A24`) and 0xF54 (inside `fn_30_F78`)
- `fn_30_4C10` <- 0x4BFC (inside `fn_30_4BD8`) and 0x1108C (inside `fn_30_1105C`)
- `fn_30_16088` <- 0x16014 (inside `fn_30_15FF4`, the function directly above the claim)

`config/G2ME01/config.yml` is unchanged - no `force_active:` list was added and no `symbols.txt`
rename was needed.

## Measured results

`build/report.json` for module 30 after the change:

```
MetroidPrime/ScriptObjects/CIngBoostBallGuardianRel               17 / 17  100.0  complete
MetroidPrime/ScriptObjects/CIngBoostBallGuardianFlagCopy           3 /  3  100.0  complete
MetroidPrime/ScriptObjects/CIngBoostBallGuardianForwarderF78       2 /  2  100.0  complete
MetroidPrime/ScriptObjects/CIngBoostBallGuardianForwarder4C58      2 /  2  100.0  complete
MetroidPrime/ScriptObjects/CIngBoostBallGuardianForwarder388C      2 /  2  100.0  complete
REL/global_destructor_chain                                        2 /  2  100.0  complete
REL/REL_Setup                                                      5 /  5  100.0  complete
(auto_* units: 285 unmatched, still retail)
```

`tools/report_diff.py` against the judge baseline: `matched 13089 -> 13098, linked 6187 -> 6196
(+9 functions at 100%, 4 units newly linked)`, **0 `WORSE`, 0 `GONE`, 0 `UNLINKED`, 0 `FELL`**. The
two `SPLIT` lines are the auto units being divided, exact count matches.

`goal_check.sh`: gate.sh ok, counts ok, `check_symbol_names.py` ok, `All: 37.07% fuzzy, 30.50%
matched, 13.45% linked (13098 / 28465 functions)`, `target rose: module:IngBoostBallGuardian:
24 -> 33 / 318 functions`, `no asm added`. **PASS.**

Note: `flip_test.sh` is not usable on these units and its FAIL is not a real failure. Its
`unit_info` helper looks for a `MusyX(` that has not closed before the entry, and because
`Object(...)` here spans lines with its own parentheses it computes `extern/musyx/src/...` and
reports "no source file". The same happens for the already-`Matching`
`CIngBoostBallGuardianRel.cpp` and for `CLumiteRelTail.cpp`, so it is the helper's root heuristic,
not this change. For a REL unit the module's sha1 is the acceptance test anyway, and it holds with
these objects in the link.

## What is left in this module, and where the next run should start

285 functions remain, all retail bytes in the gaps and in `auto_*` units. The shapes worth trying,
all contiguous and all with a DOL twin that is already matched somewhere:

- `0x1164..0x12A0` - `fn_30_1164` (0x13C), the copy constructor the first unit's guard calls:
  three floats, a byte and a word pair, then `optional_object<CToken>`-shaped members. Same
  structure as `CLumiteRelTail.cpp`'s left-retail `fn_39_7C0` and as Lumite's whole teardown group.
- `0xF78..0x1054` - `fn_30_F78` (0x74): six floats then words, a plain member-wise copy
  constructor, the easiest of the four declared-but-unclaimed callees.
- `0x4BD8..0x4C10` - `fn_30_4BD8` (0x38) and `0x4C58..0x4D18` - `fn_30_4C58` (0xC0) plus
  `fn_30_4D74` (0x84); the twins are `push_back_unsafe__...vector<CJointCollisionDescription>...`
  and `__dt__...vector<CJointCollisionDescription>...` in
  `src/MetroidPrime/CCollisionActorManager.cpp`.
- `0x388C..0x38E0` - `fn_30_388C` (0x54), the `CHealthInfo` copy the fourth unit's guard calls;
  twin `__ct__11CHealthInfoFRC11CHealthInfo` in `src/MetroidPrime/ScriptObjects/CScriptActor.cpp`.
  `0x3790..0x38E0` is a four-function all-twin run (`fn_30_3790`, `fn_30_37E0`, `fn_30_3838`,
  `fn_30_388C`).
- The `0x1F08..0x20A0` and `0xCB4..0xF30` runs: `__as__11CMayaSpline` /
  `__ct__Q24rstl52vector<15CMayaSplineKnot,...>` pairs, twins in
  `src/MetroidPrime/ScriptObjects/CScriptEffect.cpp`. **Both are template instantiations, so they
  need this tree's `rstl` headers to line up before a spelling will do** - read that source first.
- `0x10E04..0x110C4`, a five-function all-twin run of `reserve`/`uninitialized_copy`
  instantiations on three different element types (0x2C0 bytes, the largest twin run in the
  module). Same warning as above, and the `rstl::vector` internals are the part that has to be
  exact.

**A lesson worth keeping (not a `NEW:` item).** The twin list this item was seeded with has 75
entries and every one is a *different* function; only `fn_30_10DC` was in a contiguous run with
another twin. The productive move was to scan the module's own asm for the *instruction shape* of
a family that is already reproduced somewhere in the tree, rather than to walk the twin list -
three of the four units came from that, and none of the three was on the list. For any REL module
whose head has landed, the sibling `*Rel*.cpp` heads in `src/MetroidPrime/ScriptObjects/` are the
cheapest catalogue of shapes the tree already knows how to spell.

NEW: progress-rel-ingboostballguardian-fn30f78 | match | IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardianF78 | fn_30_F78 (0xF78, 0x74) is a member-wise copy constructor - six floats then words - already declared as the callee of CIngBoostBallGuardianForwarderF78's guard, so a unit claiming 0xF78..0x1054 needs no new declaration and is the easiest of the four declared-but-unclaimed callees
NEW: progress-rel-ingboostballguardian-fn30888 | match | IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardian388C | fn_30_388C (0x388C, 0x54) is the module's CHealthInfo copy constructor, twin __ct__11CHealthInfoFRC11CHealthInfo in src/MetroidPrime/ScriptObjects/CScriptActor.cpp, and 0x3790..0x38E0 is a four-function all-twin run
---

# Second run (lane 6, 2026-10-02) — six new units, +17 functions, goal_check PASS

**This run started from a tree that did NOT contain the four units the run above landed.** This
lane's `c7f9598a` has only `CIngBoostBallGuardianRel` in the `Rel("IngBoostBallGuardian", ...)`
block, and `build/goal/judge/report.base.json` measures module 30 at **24 / 318** - the same 24 the
run above started from, not the 33 it ended on. So nothing above was available to build on and the
work below is independent of it; the spellings in the sources above (the GC/2.7 `fn_39_738` spill
order, the `if (self == 0) return;` guard) are the prior run's, and nothing below depends on them.

**Result: module 30 matched_functions 24 -> 41 (of 318), six new units all `Matching` at 100.00%,
`All:` 13098 -> 13115 matched and 6196 -> 6213 linked, `total_functions` still 28465, DOL
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` and `87 files OK` (all 86 RELs).**
`./tools/goal_check.sh build/goal/item.json` -> **PASS**.

## The productive move: only claim the functions with no relocation at all

The run above worked from instruction *shapes*. This run worked from a measurement instead, and it
is a strictly better filter: **a REL function with no relocation in its byte range can be
reproduced without declaring a single callee, because the bytes are the whole of the claim.**
`auto_00_00000130_text.o`'s `.rela.text` is dense (2254 entries), but 33 of its 244 functions fall
in gaps. Read them straight out of the object:

    python3 - <<'PY'
    import collections, struct
    from pathlib import Path
    e = Path('build/G2ME01/IngBoostBallGuardian/obj/auto_00_00000130_text.o').read_bytes()
    shoff, = struct.unpack('>I', e[0x20:0x24]); sz, n, _ = struct.unpack('>HHH', e[0x2E:0x34])
    S = [struct.unpack('>10I', e[shoff+i*sz:shoff+i*sz+40]) for i in range(n)]
    offs = set()
    for o in range(S[2][4], S[2][4]+S[2][5], 12):
        d, i, _ = struct.unpack('>IIi', e[o:o+12]); offs.add(d)
    ...  # print name, size, value for every STT_FUNC whose [value, value+size) misses offs
    PY

Group the survivors by contiguity and each group is one unit. The seven multi-function groups:

| range | functions | what they are |
|---|---|---|
| `0xB788..0xB7E0` | 5 | one-bit reads out of the flag bytes at +0x11ED/+0x11EE |
| `0xC1A4..0xC240` | 7 | the state/flag predicates: bit, word-or, `f1 > f0`, three `== const`, `!= 0` |
| `0xC6AC..0xC6CC` | 2 | one flag bit and one level test |
| `0xC05C..0xC0F4` | 2 | `fn_30_C05C` + the 0x84-byte switch `fn_30_C070` — **not attempted**, see below |
| `0xC070` alone, `0x194C`, `0x2094`, `0xA91C`, `0x10B90`, `0x10F40`, `0x108D4`, `0x57E0`, `0x6710`, `0xCD78`, `0xD1C4`, `0xC05C` alone | 1 each | leaf singletons |

The other 26 are 0xC-byte bit accessors (`fn_30_2094`, `fn_30_BAA8`, `fn_30_BC20`, `fn_30_BE88`,
`fn_30_C1A4`, `fn_30_C6AC`, `fn_30_E4E0` ...), and each of those is its own carve: four files for
one function. That is the cost curve of a REL module, not a blocker.

## What was claimed, and what each one cost to spell

| range | functions | the spelling that was load-bearing |
|---|---|---|
| `0xB788..0xB7E0` | 5 | see below — two separate discoveries |
| `0xC1A4..0xC240` | 7 | `w == K` is `subfic`/`cntlzw`/`srwi 5`; `w != 0` is `neg`/`or`/`srwi 31`; `w == 0` is `neg`/`andc`/`srwi 31` and is **not** the same body |
| `0xC6AC..0xC6CC` | 2 | the same `== const` and one-bit-mask idioms |
| `0x194C..0x1988` | 1 | the parameter order is `(6 floats, 6 ints, 2 floats)` — read off the ABI, r3 is the object so the ints start at r4 |
| `0x2094..0x20A0` | 1 | `*(int*)((char*)self + 4) = 0;` gives `li r0,0` + one `stw` |
| `0xA91C..0xA95C` | 1 | the cross product's subtrahends are written `b_comp * a_comp`, and all six operands must be named locals |

**MWCC's one-bit mask on a byte is off by one bit, and retail's bytes contain that.** Measured by
compiling `(static_cast<const unsigned char*>(p)[k] & M) != 0` for all eight byte masks:

    M = 0x01 -> clrlwi rD, r0, 31          M = 0x10 -> rlwinm rD, r0, 28, 31, 31
    M = 0x02 -> rlwinm rD, r0, 31, 31, 31  M = 0x20 -> rlwinm rD, r0, 27, 31, 31
    M = 0x04 -> rlwinm rD, r0, 30, 31, 31  M = 0x40 -> rlwinm rD, r0, 26, 31, 31
    M = 0x08 -> rlwinm rD, r0, 29, 31, 31  M = 0x80 -> rlwinm rD, r0, 25, 31, 31

The rotate is always `32 - log2(M)`, so the predicate that gets compiled tests **bit k-1, not bit
k**, and at M = 0x01 the rotate is 0 and nothing of the byte survives - the function is constant.
GC/1.3.2 and GC/2.7 are byte-identical here, so **this is not a compiler-version effect and the
GC/2.7 override the run above needed is not the answer to anything here.** Consequence: a source
that reads "bit 7 of this byte" must be written `& 0x80` to get retail's bytes only where retail's
own bytes have the rotate-25 form; `fn_30_B788` (`clrlwi r3, r0, 31`) needed `& 1`, not `& 0x80`,
and `fn_30_B7A0` (rotate 25) needed `& 0x80`. Both are in the same unit and they are **different
constants for what reads as the same predicate** - that is the fact to carry forward.

**One function in that unit needs the shift spelling, not the mask spelling.** `fn_30_B7AC`'s two
tests are `beq` then `bne` on `rlwinm. r0, r0, 29, 31, 31` and `rlwinm. r0, r0, 31, 31, 31`. The
obvious reading, `(b1 & 8) == 0 && (b2 & 2) == 0`, compiles to `rlwinm. r0, r0, 0, 28, 28` and
`rlwinm. r0, r0, 0, 30, 30` - four bytes off, and *no* arrangement of `&`/`==`/`&&`/`!`/`||`
recovers it (48 combinations measured across six shapes x four mask pairs; the branch polarity was
right every time and the mask was never). The spelling that does is `(b1 >> 3) & 1` and
`!((b2 >> 1) & 1)` - the `>> k & 1` shape is what MWCC emits for a one-bit **bitfield** read, and
it keeps the bit at position 31 instead of 28. **When retail's mask is `rlwinm. rD, r0, N, 31, 31`
with N != 0 and the branch follows, reach for `(x >> k) & 1` before reaching for `x & (1 << k)`.**

## Things measured that are worth not re-deriving

- **The fast loop.** Compiling one candidate source and diffing its `.text` against the module's
  takes seconds and replaces a 30-minute `decomp_build.sh`. The module's `.text` starts at file
  offset **0xDC** inside `build/G2ME01/IngBoostBallGuardian/IngBoostBallGuardian.rel`, so retail
  byte range `[lo, hi)` is `Path(rel).read_bytes()[lo + 0xDC : hi + 0xDC]`; the MWCC command line
  is the `cflags =` block of the unit's stanza in `build.ninja`. That is how all six units were
  brought to a byte-exact source before being wired into the carve at all.
- **`objdiff` can report 60% for an object that is byte-identical to retail, and the object under
  `build/G2ME01/<Module>/obj/` is not always the one that was linked.** With `fn_30_B788` and
  `fn_30_B7AC` wrong, `build/report.json` said 3 of 5 matched; the *linked* `.rel` differed from
  retail in exactly six bytes and nowhere else. Trust the linked module, not the obj copy: the
  run above's hash failure was reproduced from it in one `cmp -l`.
- **The dead-strip check is one `grep` against two objects**, not the ldscript: no claimed function
  is in `build/G2ME01/IngBoostBallGuardian/ldscript.lcf`'s FORCEACTIVE list, but every one of the
  seventeen is named by a relocation in `auto_04_00000000_data.o` (the module's own vtable,
  `.data`+0x0C/+0x18/+0xA8..+0xF0/+0x210..+0x240) or in `auto_00_00000130_text.o` /
  `auto_00_00011CD8_text.o` (which call into the gaps). So dtk's own objects hold every reference,
  no `force_active:` entry is needed, and `config/G2ME01/config.yml` is unchanged.
- **`tools/check_decl_order.py --unit` reports "0 unit(s) checked" for REL units** - it does not
  see them. The substitute is `powerpc-eabi-nm -n` on each built object: all six emit their
  functions in ascending retail order (`fn_30_B788`@0, `fn_30_B794`@0xc, `fn_30_B7A0`@0x18,
  `fn_30_B7AC`@0x24, `fn_30_B7D4`@0x4c; and so on for the other five), which is what descending
  source order has to produce. `unit_fit.sh` is clean for all six (claimed == ours == retail, no
  extra functions).
- **`tools/check_raw_offsets.py` is the gate step that actually bites a REL carve**, and it
  bites per-file: a new source with a raw offset fails until
  `docs/research/raw_offsets.md` has a `##` section for it with the measured site count. Three
  sections and the totals paragraph were added (185 sites in 79 files, up from 176 in 76). It
  undercounts subscript and `static_cast< char* >` reaches, exactly as
  `CIngBoostBallGuardianRel.cpp`'s own section records - `fn_30_B7AC`, `fn_30_C1A4`, `fn_30_C6AC`,
  `fn_30_2094` and the whole of `CIngBoostBallGuardianBits.cpp` get past it on subscripts.

## What is left, and where the next run should start

285 - 17 = **268 functions remain unmatched** in module 30, all retail bytes in the gaps. The
leaf-function list above is the whole of the cheap pool; ranked by what is left:

- **`0x388C..0x38E0`, `0xF78..0xFEC`, `0x33B0..0x340C` - the three member-wise copy constructors,
  and these are the interesting ones.** All three are leaf, all three reproduce retail's bytes only
  with MWCC's **two-deep load/store pipeline** (`lfs f1,0; lfs f0,4; stfs f1,0; lfs f1,8; stfs f0,4;`
  ...), which four spellings do **not** produce, all measured on scratch files:
  `*self = other` on a POD struct emits an out-of-line `__as__` with plain pairs and leaves the
  named function a `bl`; member-by-member `self->m = other.m` gives plain pairs; the same through a
  **mem-initializer list** in a hand-written copy constructor also gives plain pairs; and reading
  the members through `float*`/`int*` subscript yields `lwz`/`stw` word copies instead of
  `lfs`/`stfs`. So the pipeline comes from something not yet tried - a constructor reached through a
  **base-class or member-object sub-object** is the next thing to try, and the register rotation
  (f0/f1 alternating, r0/r5 alternating) says MWCC is holding two values per class, which is what
  nested sub-objects would do. Three functions, three units.
- **`0x10B90..0x10C24` (0x94) and `0x108D4..0x10B90` (0x2BC, the largest leaf in the module)** -
  0x350 bytes of pure data movement with no relocation at all. Unattempted.
- **`0x57E0..0x5888` (0xA8), `0x6710..0x67B4` (0xA4), `0xCD78..0xCE14` (0x9C), `0xD1C4..0xD230`
  (0x6C), `0xC070..0xC0F4` (0x84, a five-way switch on the word at +0x0AE4), `0x10F40..0x10F9C`** -
  leaf, unattempted, and each is a single carve.
- **`0xC05C..0xC070` (0x14)** - `lwz r3,0x115c(r3); neg r0,r3; andc r0,r0,r3; srwi r3,r0,31`, which
  is *always zero* for any input (`(x-1) & x` has bit 0 clear for every x). So the body is a
  constant `false` and something is being modelled wrongly upstream of it; worth one attempt at
  `return false;` / `return w > 0;` / `return w != 0;` spellings, not more.
- **The 75-entry twin list in `item.json` is the wrong door for this module.** Every entry is a
  `rstl` template instantiation or a class destructor whose *call targets* differ, so none of them
  is a byte-exact twin - the relocation filter finds 33 leaf functions where the twin list finds
  zero usable ones. The run above's lesson generalises further than it was stated: **for a REL
  module, scan the retail object's relocation table before reading the twin list at all.**

NEW: progress-rel-ingboostballguardian-copys | match | IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardian388C | fn_30_388C (0x388C, 0x54) is the module's CHealthInfo copy constructor and 0xF78..0xFEC (fn_30_F78, 0x74) plus 0x33B0..0x340C (fn_30_33B0, 0x5C) are two more member-wise copy constructors; all three are leaf and all three need MWCC's two-deep load/store pipeline, which *self = other, member-by-member assignment and a mem-initializer list all fail to produce (measured) - the next thing to try is nested sub-objects, since the f0/f1 and r0/r5 alternation is MWCC holding two values per class
