# `progress-unit-careaocttreetests` — `WorldFormat/CAreaOctTree_Tests`, 5/9 → 7/9

`tools/goal_check.sh build/goal/item.json` → **`goal_check: PASS progress-unit-careaocttreetests`**,
with `target rose: main/WorldFormat/CAreaOctTree_Tests: 5 -> 7 / 9 functions`,
`counts: matched 11817 -> 11819   linked 5727 -> 5727`, and a clean `gate.sh` (DOL sha1
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, all 86 RELs, `check_symbol_names.py`, port probe).

## What landed

**`config/G2ME01/symbols.txt` — two renames, lines 10269-10270.**

```
-fn_802461A0 = .text:0x802461A0; // type:function size:0x4C
-fn_802461EC = .text:0x802461EC; // type:function size:0x6C
+__ct__Q212CAreaOctTree4NodeFRCQ212CAreaOctTree4Node = .text:0x802461A0; // type:function size:0x4C
+__as__Q212CAreaOctTree10SRayResultFRCQ212CAreaOctTree10SRayResult = .text:0x802461EC; // type:function size:0x6C
```

These are the case `docs/RUNNING_THE_DECOMP.md` documents under "Pairing a function the retail
symbol table has no name for": dtk cannot name a TU-local weak template instantiation, so the base
object calls it `fn_...`; objdiff pairs by name, so a byte-identical function under our own mangled
name scores 0%. **The code already matched — only the name differed.**

Measured with `python3 tools/fnmap.py WorldFormat/CAreaOctTree_Tests`:

| retail | size | ours | |
|---|---|---|---|
| `fn_802461A0` | 76 | `__ct__Q212CAreaOctTree4NodeFRCQ212CAreaOctTree4Node` | IDENTICAL |
| `fn_802461EC` | 108 | `__as__Q212CAreaOctTree10SRayResultFRCQ212CAreaOctTree10SRayResult` | IDENTICAL |

Both go to **100.00%** after the rename: `LineTestExInternal` aside, the unit is
`_close_enough` 100, `BoxLineTest` 100, `LineTestEx` 100, `LineTest` 100, `LineTestInternal` 100,
plus these two at 100. `matched_code` 3732 → 3916, unit fuzzy 92.44% → 95.09%.

Scope was kept to what the item needs, which meant **not** renaming `fn_80246258` — see below.

Renaming is hash-safe here, and that was checked rather than assumed: the three `fn_80246*` strings
appear **0 times** in `build/G2ME01/main.dol` (they are TU-local and never reach the linked
symbol table), and `gate.sh` re-derives the DOL sha1 anyway. Left at the default scope: no
`scope:weak` was added, because a weak symbol that nothing references is dead-stripped, which would
have moved retail's bytes.

## What is still unmatched, and why

`LineTestExInternal` (2820 B, **94.88%**) and `fn_80246258` (196 B, unnamed).

### `LineTestExInternal` — one structural difference, and it is a wall

Normalized instruction diff of the two objects (branch targets stripped) shows **one** structural
difference: retail expands `rstl::optional_object<CCollisionSurface>::assign` in place at
`candidate.mSurface = triangle;`, we emit it out of line and `bl` it at our 0x2d8. Retail's
in-place expansion is the `m_valid` test (0x80245964 `lbz r0,0x3e0(r1)`), the `construct` call on
the invalid path (0x80245980), and the eleven-word `CCollisionSurface` copy on the valid one
(0x80245990). Our `assign` is **0x9C = 156 bytes**, over the project-wide `inline_max_size(125)`.
Everything else in the diff is register allocation downstream of that (r31/r19/r21 ↔ r17/r16/r20,
f3↔f1 in the `CVector3f` default-ctor blocks, and `cmplwi r3,0` ↔ `clrlwi. r0,r3,24`).

Two fixes tried, both measured dead ends:

1. **`#pragma inline_max_size`, swept.** The pragma is **TU-global, not positional** — a value
   placed immediately above `LineTestExInternal` produced numbers *identical* to the same value at
   the top of the file, which is what exposed it. Swept 126,127,128,129,130,131,132,133,134,142,
   150,156,158,165,175,190:

   | value | matched | unit fuzzy | `LineTestExInternal` | `LineTestInternal` |
   |---|---|---|---|---|
   | 126-133 (default 125) | 7 | 95.09 | 94.88 | 100.00 |
   | 134-142 | 5 | 88.15 | 87.77 | 92.74 |
   | 150-190 | 5 | 85.54 | 84.56 | 89.53 |

   126..133 is byte-identical to the default, i.e. `assign` still is not inlined at 156 bytes, and
   everything ≥134 is worse and also costs `LineTestInternal` its match. **No value in the 125-156
   window both inlines `assign` and leaves the TU alone.** The pragma was reverted; the comment in
   the source records this so the next run does not repeat the sweep.

2. **Reshaping `assign` in `include/rstl/optional_object.hpp`** from the early-`return` form to
   Prime 1's `else` form. That header is included by **92 sources** and **578 DOL units are
   `Matching`**. A full `./tools/decomp_build.sh` after the edit:

   ```
   WARNING: 87 computed checksum(s) did NOT match
   ninja: build stopped: subcommand failed.
   ```

   **87 REL checksums broke.** Reverted (`cp /tmp/oo_base.hpp include/rstl/optional_object.hpp`,
   `git diff --stat include/` empty). Worth recording as a general fact: the shared
   `optional_object.hpp` is effectively frozen for this unit — a change there is a project-wide
   change, not a unit change.

WALL: LineTestExInternal 94.88% - retail inlines optional_object::assign in place; ours is 156B
over the 125B inline limit and every inline_max_size value 126-190 was measured worse or identical.

### `fn_80246258` — deliberately not renamed

It is `rstl::optional_object<CCollisionSurface>::operator=(const optional_object&)`; ours
(`__as__Q24rstl36optional_object<17CCollisionSurface>FRCQ24rstl36optional_object<17CCollisionSurface>`)
is 196 B, the same size, but **not byte-identical** — `cmp` on the two 392-hex-digit extracts
differs at byte 179. It is a pure register-choice difference in the eleven-word `CCollisionSurface`
memberwise copy (retail `lwz r0,8(r4) / stw r0,8(r31)`, ours `lwz r3,8(r4)`), which is codegen for
`*get_ptr() = other.data()`, not a source shape. **Renaming it would have paired it at ~99% and
risied the item** for a function that is not a match; left unnamed deliberately.

## Notes for the next attempt

- Prime 1's `prime-ref/src/WorldFormat/CAreaOctTree_Tests.cpp` is a **complete donor** — the
  429-line file here is already a faithful adaptation of it (only `GetTriangle` vs
  `GetMasterListTriangle`, `CMath::AbsF` vs `fabs`, naming and includes differ). There is no logic
  left to port; the remaining gap is codegen, not source.
- `include/rstl/optional_object.hpp` and `include/rstl/construct.hpp` are **frozen**: 92 sources
  include them and any edit breaks REL checksums (measured: 87). Do not retry the `assign` shape.
- This unit cannot flip while `LineTestExInternal` is 94.88%; `tools/unit_fit.sh` would also flag
  the extra weak instantiations we emit (`__dt__` for `SRayResult` and `optional_object`, four
  `construct`/`construct_impl`/`assign` helpers) that retail's object does not define.
- `total_functions` is still 28465; `splits.txt` was not touched.

## Doc claims

`python3 tools/check_docs_claims.py` reports the HANDOFF state block is stale
(`matched 11819 / 28465`, `DOL units 10271 / 16726`). **Not edited deliberately** — the goal-unit
prompt says the driver discards `docs/HANDOFF.md` edits and rewrites the derived counts from the
tree, and the judge above passed with those two claims flagged.