# progress-prime1-careaocttree_tests

`WorldFormat/CAreaOctTree_Tests` **3/9 -> 5/9 matched functions**; DOL-wide matched
**10006 -> 10008** (`tools/report_diff.py` against `build/goal/judge/report.base.json`:
`+2 functions at 100%, 0 units newly linked, no regression`). The unit stays `NonMatching` —
`GATE PASS` on the whole gate, `All:` 30.83% -> 30.91% fuzzy.

Per function, before% -> after%, and whether Prime 1's source needed editing:

| function | before | after | Prime 1's source |
|---|---|---|---|
| `BoxLineTest` | 77.07 | **100.00** | needed edits (see below) |
| `LineTestInternal` | 0.20 | **100.00** | matched unchanged except one header accessor |
| `LineTestExInternal` | 0.14 | **94.88** | matched unchanged; 5% left, see "what is left" |
| `LineTest`, `LineTestEx`, `_close_enough` | 100 | 100 | untouched |
| `fn_802461A0` / `fn_802461EC` / `fn_80246258` | 0 | 0 | not attempts — see "the three unnamed" |

## What each function needed

**`BoxLineTest` (428 bytes, 0x80246FE4).** Prime 1's body matched this repo's *shape* but not
its codegen. Two spellings differ and both mattered, measured with a relocation-aware
instruction diff (`.tmp/opencode/cmpfn.py`, the same idea as `tools/bytescmp.py` but it prints
the non-relocation differences only — with relocations counted it reads 86 differing
instructions, of which 80 are `bl`/`lis`+`addi` fields the linker fills):

1. Prime 1 takes `CVector3f lineRefPoint = line.GetRefPoint();` and
   `CUnitVector3f lineNormal = line.GetNormal();` **by value**. This repo's tree had
   `const CVector3f&` / `const CUnitVector3f&`. Retail's prologue copies all 24 bytes of both
   into the frame (`stfs f6,20(r1)` ... `stfs f3,8(r1)`), so the values are copies. With
   references the compiler keeps reloading through `r31` and the 428 bytes come out permuted.
   Taking them by value is the whole fix for this function: 0 differing instructions.
2. The `1.f / lineNormal[i]` reciprocal is computed **inside each branch**, after the sign
   test, not hoisted above it. Hoisting it is what the tree had. Retail's negative arm is
   `fcmpo f3,0.0; bge +0x114` -> `lfs 1.0; lfs lT; fdivs f5,f1,f3` — the divide is on the taken
   path only. Moving it back inside took the last 4 differing instructions.

**`LineTestInternal` (2816 bytes, 0x8024631C).** Prime 1's body is **exactly** Echoes's, modulo
the callee names — same `SSubdivision` table (all 16 entries, same order), same
`skSubdivisionIndices[8][8]`, same `skAxisBits`/`skNextAxis`, same `GetChildFlags() == 0xa`
two-child special case, same 11-word Möller–Trumbore in the leaf loop, same `m_valid`-then-
`mT` ordering. It matched to 0 differing non-relocation instructions on the first build that
compiled.

The one thing that had to change was **not in the .cpp**: this repo's
`CAreaOctTree::TriListReference` declared `ushort GetSize() const` / `ushort GetAt(int) const`;
Prime 1 declares them `const ushort` and adds the `explicit TriListReference(const ushort*)`
constructor. That return-type difference alone cost 6 instructions in the leaf loop (the two
`lhz` results land in different registers and the loop counter's `cmpw` operand changes), and
`report.json` read 99.39% on the first attempt. Changing the header to Prime 1's spelling took
it to 100.00% with no source change at all. **`const` on a by-value return is not decoration
here — it changes mwcceppc's register choice two hundred bytes into the function.**

`CVector3i::operator[]` was added to `include/Kyoto/Math/CVector3i.hpp` because Prime 1 indexes
`subdivision.axes[i]` and this repo's `CVector3i` only had `GetX/GetY/GetZ`. Prime 1's own
`operator[]` is the same one-liner.

**`CCollisionPrimitiveData::GetTriangle`.** Declared in
`include/WorldFormat/CCollisionPrimitiveData.hpp` with no definition anywhere in the tree — the
line tests are its first callers, and `docs/goal-notes/progress-prime1-cmetroidareacollider.md`
already recorded it as the blocker on that unit. Retail has it out of line at **fn_80257A14**,
0x80257A14, 0xE4 = 228 bytes, in the unclaimed gap between `CAreaRenderOctTree` and
`CProjectileWeapon`. Decoded from the disassembly and matched to Prime 1's
`CAreaOctTree::GetMasterListTriangle` with two changes: the third index is at
`mSurfaceIndices[index * 3 + 1]` (Echoes strides the array by 3 shorts, Prime 1 by 3 through a
separate `mPolyEdges`), and the winding bit is **bit 24** (`0x01000000`, `and r5,r8,0x100` after
`lis r5,0x100`), where Prime 1 has bit 25 (`0x2000000`).

## What is left on `LineTestExInternal` (94.88%)

Not a wall in the `WALL:` sense — I stopped with the item passing rather than because the
spelling space is exhausted, and the remaining difference is **register allocation plus one
outlining decision**, both of which I measured:

- **475 of 704 instructions differ, but 56 are relocations** (the `bl`s to `CVector3i(i,i,i)`,
  `Passes`, `GetChild`, `__as__SRayResult`, `GetPlane`). The rest is a cascade from the leaf
  loop, where retail keeps the triangle-list pointer in **r24** and the count in **r25** while
  this build uses **r25** for the pointer and **r24** for the count (`+00c8`, `+00d4`, `+029c`,
  `+02a4` are the four root instructions; everything after is downstream). `LineTestInternal`
  hit the *same* swap and the *same* `const ushort` fix resolved it — but `LineTestExInternal`
  has one more live value across the loop (the `SRayResult candidate`, 0x48 bytes), and with it
  the allocator picks the other way. This is the "remaining diff is only register allocation"
  case, which the brief says to stop on rather than keep spelling.
- **The one non-allocation difference**: at `+0x2c8` retail **inlines**
  `optional_object<CCollisionSurface>::operator=(const CCollisionSurface&)` (the
  `!m_valid` test, then an 11-word copy, ~0x80 bytes) where this build emits a `bl` to the weak
  COMDAT `__as__Q24rstl36optional_object<17CCollisionSurface>FRC17CCollisionSurface`. That is
  the `inline_max_size` threshold, and I could not move it: measured 126/130 -> 94.88% (no
  change), **134/136/138 -> 87.77%**, 140 -> 92.74%, 150/160/400 -> 84.56%. Every value above
  133 inlines something else in the TU as well and costs more than it gains. 125 (the project
  default) is the optimum and is what the tree carries. I also tried making `assign()` and
  `operator=(const T&)` explicitly `inline` in `include/rstl/optional_object.hpp` and putting
  `#pragma inline_max_size(400)` directly on `assign`: **no change to the object at all** — the
  flags did not move it. I reverted both; the header is untouched in the diff.

**fn_802461A0 (76) / fn_802461EC (108) / fn_80246258 (196)** are the `SRayResult` copy
constructor, its copy-assignment, and `optional_object<CCollisionSurface>`'s copy-assignment —
our object emits all three under their real names (`__ct__Q212CAreaOctTree4NodeFRCQ212CAreaOctTree4Node`,
`__as__Q212CAreaOctTree10SRayResultFRCQ...`, `__as__Q24rstl36optional_object<17CCollisionSurface>FRCQ...`).
They are **not** new work: the byte diffs are **0 real differing instructions** for the first two
and **7** for the third (a 7-instruction register shuffle in the middle of its word-copy run,
`lwz r3,8(r4)` vs `lwz r0,12(r4)` and the two `stw`s it feeds). They read 0.00% only because
retail has no name for them and `objdiff` pairs by name. Renaming them in
`config/G2ME01/symbols.txt` was tried and **reverted**: `tools/apply_rename.py` accepts the
three, but `python3 tools/check_symbol_names.py` then reports all three as "declared but
CAreaOctTree_Tests.o does not define it" and exits 1, which is a gate step. **Naming them needs
a real signature match against the retail-derived object, not a guess.**

## The port link gap, and where `GetTriangle` and the Node accessors had to go

`tools/gate.sh`'s `port probe` step runs `tools/link_check.sh --strict`, which **fails if the
port's undefined count grows** against `docs/research/port_link_baseline.txt` (250). Reconstructing
the two traversals makes `CAreaOctTree_Tests.cpp` — which *is* in `files.cmake` — call three
things nothing in the port build defines, and the first two attempts measured:

- everything in `src/WorldFormat/CAreaOctTree.cpp` (where the two `CAreaOctTree::Node` accessors
  live, and where `GetTriangle` naturally belongs): **250 -> 253**.
- `GetTriangle` moved to `src/WorldFormat/CCollisionSurface.cpp`, accessors still in
  `CAreaOctTree.cpp`: **250 -> 252**.
- accessors also copied into `src/MetroidPrime/PortGlobals.cpp` (the documented home for
  PC-side definitions whose own TU is not in the port build, same pattern as its eight
  `TypesMatch` bodies): **250 -> 251**.
- `GetTriangle` in `CCollisionSurface.cpp` as well: **250, 0 duplicates** — the current tree.

`src/WorldFormat/CAreaOctTree.cpp` cannot simply be added to `files.cmake`:
`tools/check_files_cmake.py` has it in `EXCLUDED`, and listing an excluded path fails with
`stale: ... is in EXCLUDED but is now listed in files.cmake`. I did not edit
`tools/check_files_cmake.py` — it is a judge-owned path, and the brief forbids that outright.
**This is the one thing in this item that wanted a `tools/` edit and did not get one**, and it
is why two retail-identical bodies now exist twice (`GetChild`/`GetTriangleArray`, plus the
file-static `BoxFromIndex` they need). Both copies carry a comment saying which one is the
copy and what removes it. The exclusion's own stated measurement — "opens 1 symbol
(`CCollisionPrimitiveData`'s constructor), closes 0" — is taken with these callers absent and is
now understated: listing the file would close three and open one.

`docs/HANDOFF.md`'s state block is the judge's (`MP_GATE_DOCS_WRITE=1`), not mine.

## Verification

```
python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json
  matched  10006 -> 10008   linked 4896 -> 4896   (+2 functions at 100%, 0 units newly linked)
  +100%    main/WorldFormat/CAreaOctTree_Tests :: BoxLineTest__FRC6CAABoxRC5CLineRfRf
  +100%    main/WorldFormat/CAreaOctTree_Tests :: LineTestInternal__...LineTestInternal__...
  no regression

MP_GATE_DOCS_WRITE=1 ./tools/gate.sh build/goal/judge/report.base.json
  GATE PASS  b21d98e+7 changed          (every step ok, including hashes vs config.yml,
                                         decl order, files.cmake, port probe, port link gap)
```

`python3 tools/check_decl_order.py --unit CAreaOctTree_Tests` — `ok: 1 unit(s) checked, none
emits its functions out of retail order`. `tools/unit_fit.sh WorldFormat/CAreaOctTree_Tests.cpp`
reports 9 COMDAT template members our object emits and retail's does not (the three unnamed
functions above, plus `assign`/`construct`/`construct_impl`/two destructors) and 548 bytes of
sections over the claimed range; per that tool's own docstring these are the harmless COMDAT
kind, and the unit is not being flipped. The unit **cannot** flip: the three unnamed functions
have no name in `symbols.txt`, and naming them fails `check_symbol_names.py`.

## For the next run

- **`WALL:` is deliberately absent.** `LineTestExInternal` at 94.88% is register allocation plus
  the one `optional_object` outlining, and the brief says to stop there rather than file a wall
  for a function that has not sat at one score across several spellings. What is worth
  recording: **`inline_max_size` 125 is a cliff for this TU, not a slope** — 126..130 changes
  nothing, 134+ is strictly worse, and no value or per-function pragma moved the
  `optional_object` assignment. If a later attempt wants that 5%, it needs a different route
  (e.g. spelling the leaf loop's `SRayResult candidate` differently to relieve register
  pressure), not a pragma.
- `fn_802461A0` / `fn_802461EC` / `fn_80246258` are worth 3 more functions on this unit and
  their bodies are already byte-correct (0 and 7 differing instructions). They are blocked on
  **naming**, and the block is `check_symbol_names.py` refusing a name the retail-derived object
  does not define. That is a `tools/` question, not a codegen one.
- The `CVector3i::operator[]` / `const ushort` finding generalises: **when a Prime 1 function
  lands at 99.x% with a small, localised set of differing instructions, diff the *header's*
  declarations before rewriting the body.** Prime 1's `const ushort GetSize() const` was worth
  0.61% on its own, in a function whose source was already character-for-character correct.
