# progress-cgamestate-map-lowerbound

**Result: the item FAILS its own acceptance test. The unit's matched count did not move.**
`MetroidPrime/Player/CGameState` **89 -> 89** of 116 matched functions, global
`matched_functions` **9477 -> 9477** (`build/report.json`, and
`build/goal/judge/report.base.json` -> `build/report.json` via `tools/gate.sh`). Both target
functions are now written from the retail disassembly and both are *short of 100%*:

| function | before | after | residue |
| --- | --- | --- | --- |
| `fn_80145BDC` (0x80145BDC, 188 B) | no body | **95.74%** | 9 of 47 instructions |
| `fn_80145B90` (0x80145B90, 76 B) | no body | **99.05%** | 4 of 19 instructions |
| `fn_8014601C` (0x8014601C, 76 B) | 99.05% | 99.05% | unchanged |

A `progress` item is judged on the count rising strictly. It did not. The tree is left with the
real bodies in place (they are honest, not stubs, and nothing got worse) so the next attempt can
keep them; the driver resets the tree either way.

## Re-measured first, and the queue's `reason` was wrong about one thing

The reason said "`fn_80145B90` (0x80145B90) and `fn_8014601C` (0x8014601C) are byte-for-byte the
same 76B wrapper". **They are not.** `tools/dol_read.py` on the retail DOL:

```
80145bb4 = 48 00 00 29      fn_80145B90  (short bl, displacement 0x29 -> 0x80145BDC)
80146040 = 4b ff fb 9d      fn_8014601C  (relocated long bl)
```

Those four bytes are the *only* difference between the two functions; every other byte of the two
76-byte bodies is identical. So the reviewer's earlier correction of the tree's own comment was
right and my first version of the comment repeated the error. The corrected comment is in the diff.

Also re-measured: the unit was already 89/116 at this head (`1a17d46`), not the 79 the older
`progress-cgamestate-bodiless-runs` notes recorded.

## What the walk is

`fn_80145BDC` is `red_black_tree<rstl::string, rstl::pair<const rstl::string,
CEnvironmentVariable>, 0, select1st<...>, rstl::less<rstl::string>, rmemory_allocator>::find_node`
- the tree's own `find_node`, spelled out over two local structs because the map's copy of that
template gets inlined into its callers and retail's does not. **The tree's member type is now
established**: `mVariables` is `rstl::map<rstl::string, CEnvironmentVariable>`
(`include/MetroidPrime/Player/CGameStateEnvVarManager.hpp:25`), and the two offsets the walk reads
are `red_black_tree`'s own - the comparator is `mCmp` at `this + 1` and the root is
`mHeader::mRootNode` at `this + 0x10` (retail `addi r3,r28,1` at 0x80145c10, `lwz r31,16(r3)` at
0x80145bec). The node is `mLeft`/`mRight`/`mParent`/`mColor` then the `rstl::pair`, so the key is
`node + 0x10` and the child links `node + 0` and `node + 4` (0x80145c28, 0x80145c30).

## The two walls, measured

`python3 tools/bytescmp.py build/G2ME01/src/MetroidPrime/Player/CGameState.o fn_80145BDC 0x80145BDC 0xBC`

**Wall 1 - `fn_80145B90` at 99.05%, the epilogue reload order.** Three instructions, nothing else:

```
  +34  ours 83e1000c  | retail 80010014  | lwz  r31,12(r1)
  +38  ours 83c10008  | retail 83e1000c  | lwz  r30,8(r1)
  +3C  ours 80010014  | retail 83c10008  | lwz  r0,20(r1)
```

Retail reloads the link register first; every spelling here reloads the saved callee registers
first. The fourth differing instruction (the `bl`) is a relocation and objdiff ignores it, so the
99.05% is these three. This is the same wall the earlier notes recorded as
`progress-cgamestate-epilogue` for `fn_8014601C`/`fn_801426E0`/`fn_8014680C`; **this function is
a fourth member of it, not a new one.** ~20 wrapper spellings measured, all 4 differing of 19:
`void*`/`u32*`/`void**` out parameter; `const void*` vs `const rstl::string&` key; two-word struct
assign; node into a local first; header word into a local first; the header word through
`+ 8` / `+ 2` on a `u32*` / through `mHeader.mLeftmost`; the two stores in either order (that one
is much worse - 13 of 16); non-void returns of `out` / the node / `words[1]` (all 80 or 64 bytes,
wrong size).

**Wall 2 - `fn_80145BDC` at 95.74%, the prologue load order.** Seven instructions, and the residue
is one load that is *early* in retail and *late* in ours:

```
  +10  ours 93c10018  | retail 83e30010  | stw  r30,24(r1)
  +14  ours 3bc00000  | retail 93c10018  | stw  r30,24(r1)   <- retail's li r30,0
  +18  ours 93a10014  | retail 3bc00000  | li   r30,0
  +1C  ours 7c9d2378  | retail 93a10014  | stw  r29,20(r1)
  +20  ours 93810010  | retail 7c9d2378  | mr   r29,r4
  +24  ours 7c7c1b78  | retail 93810010  | stw  r28,16(r1)
  +28  ours 83e30010  | retail 7c7c1b78  | mr   r28,r3
  +3C  ours 48000001  | retail 4bee179d  | bl   __cl__rstl81less   (relocation, ignored)
  +78  ours 48000001  | retail 4bee1761  | bl   __cl__rstl81less   (relocation, ignored)
```

Read as a diff it is a one-instruction slide: retail emits `lwz r31,16(r3)` (the root) at
+0x10, **immediately after `stw r31,12(r1)` and before any other register is set up**; ours emits
it at +0x28, after `mr r28,r3`. The *register assignment* is already retail's - r31 the root, r30
the needle, r29 the key, r28 the tree - and the instruction sequence from +0x30 on is identical.
Only the position of that one load inside the prologue differs. **Two of the nine differences are
relocations**, so the real residue is seven instructions of prologue scheduling.

`fn_80145BDC` spellings measured, all 9 differing of 47 unless noted (the best is 95.74% objdiff;
one early layout was 89.13% and two were 81.5-91.6% before the struct layout was corrected):

- declaration order of the four locals: root / needle / self / key (**the best**), root / self /
  needle / key, root / key / needle / self, key / self / needle / root, needle / key / self /
  root, key / root / needle / self, needle / self / root / key, root / needle / key / self
  (retail's r31,r30,r29,r28 order - measurably **worse**, 15 differing)
- `self` as a reference vs a pointer vs a `const` reference bound first
- the root read through `mRoot` vs a nested `mHeader.mRootNode` vs a `get_root()` accessor
- the root read through a pointer-to-pointer deref, and through a `volatile`-qualified tree
  (a volatile read cannot be sunk - it did not move the load)
- `const void* key` vs `const rstl::string& key` as the parameter type
- a `for`-header initialiser instead of a `while`
- the whole thing written as `map::find` returning the eight-byte iterator by value (44 bytes -
  the wrapper's two stores are inlined, wrong shape)

**The `bl` residue is not the blocker.** Both calls are relocations and objdiff ignores
relocation fields; `bytescmp` counts them, objdiff does not. That is the same correction the
reviewer made to the earlier attempt.

## Not attempted, and why

- **`fn_80142288`/`fn_801422D4`** (0x80142288, 76 B + 108 B) in the same unit are still bodiless
  and are the cheapest remaining target in it (the disassembly is a four-word stack range plus a
  12-byte `subf`/`srawi`/`addze` trip count). Out of scope for this item, which names the map
  walk.
- The unit's four known regswap/epilogue walls are unchanged by this diff; `fn_80145B90` joins
  the epilogue one.

## Verification

```
sha1sum build/G2ME01/main.dol                       6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/decomp_build.sh                             All: 29.13% fuzzy, 21.26% matched, 11.37% linked
                                                    (9477 / 28465 functions) - unchanged
python3 tools/check_symbol_names.py                 checked 484 units; 0 declared names are missing
./tools/probe_sources.sh                            735 files, 0 failed; LINKED (254 undefined, 0 duplicates)
python3 tools/check_docs_claims.py                  docs claims agree with the tree
python3 tools/check_decl_order.py                   main/MetroidPrime/Player/CGameState "permuted and
                                                    not in decl_order.md" - IDENTICAL with and without
                                                    this diff (stashed, re-run, diffed the first two
                                                    lines: no change). Pre-existing, not mine.
./tools/gate.sh build/goal/judge/report.base.json
  report ok / per-function diff  matched 9477 -> 9477  linked 4798 -> 4798
                    (+0 functions at 100%, 0 units newly linked)
  module wiring ok / dol_read ok / docs claims ok / gs offsets ok / raw offsets ok
  files.cmake ok / module order ok / port probe ok / port link gap ok
  GATE FAIL: decl-order   (the pre-existing CGameState entry, above)
```

Files touched: `src/MetroidPrime/Player/CGameState.cpp` only. Two local structs in an anonymous
namespace (`SGameStateVarNode`, `SGameStateVarHeader`, `SGameStateVarTree`), the definition of
`fn_80145BDC`, the definition of `fn_80145B90`, and the two comments on `fn_80145BDC` /
`fn_8014601C` corrected. No header change was needed - the item's open question ("the tree's
member type is not established") is answered by `CGameStateEnvVarManager.hpp:25` as it stands.

WALL: fn_80145B90 99.05% - the epilogue reloads r31/r30 before r0 and retail reloads r0 first; ~20 wrapper spellings all left 3 instructions differing, and it is the same wall as fn_8014601C/fn_801426E0/fn_8014680C
WALL: fn_80145BDC 95.74% - the instruction sequence and register assignment are retail's; only the `lwz r31,16(r3)` root load sits at the end of our prologue instead of right after `stw r31,12(r1)`, and ~25 body spellings did not move it

NEW: progress-cgamestate-findnode-prologue | progress | MetroidPrime/Player/CGameState | fn_80145BDC (0x80145BDC, 188B) is 95.74% and fn_80145B90 (0x80145B90, 76B) is 99.05% with real bodies written; the only residue is prologue/epilogue scheduling - retail emits `lwz r31,16(r3)` before any other register setup and reloads r0 before r31/r30, and ~25 spellings across the two did not move either

---

# Attempt 2 (2026-09-29) — BOTH WALLS CRACKED. Unit 89 -> 92.

`MetroidPrime/Player/CGameState` **89 -> 92** of 116 matched functions; global
**9506 -> 9510** (`build/report.json`, measured through `tools/gate.sh build/goal/judge/report.base.json`).
All three functions the item names are at **100.00%**:

| function | before | after |
| --- | --- | --- |
| `fn_80145BDC` (0x80145BDC, 188 B) | 95.74% | **100.00%** |
| `fn_80145B90` (0x80145B90, 76 B) | 99.05% | **100.00%** |
| `fn_8014601C` (0x8014601C, 76 B) | 99.05% | **100.00%** |

`tools/goal_check.sh build/goal/item.json` on this tree:

```
  ok    no judge-owned path touched
  FAIL  gate.sh
          GATE FAIL: decl-order
  ok    counts: matched 9506 -> 9510   linked 4835 -> 4836
  ok    check_symbol_names.py
  ok    All:  29.19% fuzzy, 21.34% matched, 11.60% linked (9510 / 28465 functions)
  ok    target rose: main/MetroidPrime/Player/CGameState: 89 -> 92 / 116 functions
  ok    no asm added
```

The one failing check is `decl-order`, and it is **pre-existing and not mine** - see below.

## The item was blocked on a build break, not on the walls

**HEAD did not build.** `./tools/decomp_build.sh` at `ada6d97` fails at the link:

```
### mwldeppc.exe Linker Error:
#   undefined: 'sndStreamMixParameter'
#   Referenced from 'CDSPStreamManager::UpdateVolume(int,int)'
```

The judge had already recorded this: `build/goal/judge/record-gate.log` says `ninja + build.sha1 FAIL`
/ `undefined: 'sndStreamMixParameter'` / `baseline recorded at ada6d97`, and `hashes vs config.yml skipped`.
**Every lane working this head was measuring a tree that could not link.** Nothing in the previous
attempt's notes mentions this, because `tools/fast_try.sh` builds one object and never links - a
perfect instance of `docs/PROCESS_LESSONS.md`'s "a green check that cannot fail".

**Root cause, measured.** Commit `ada6d97` ("match: match-stream") flipped
`configure.py:1275` to `Object(Matching, "musyx/runtime/stream.c")` and committed only three files:

```
$ git show --name-only --format= ada6d97
configure.py
docs/HANDOFF.md
docs/goal-notes/match-stream.md
```

`docs/goal-notes/match-stream.md` describes a four-guard change to
`extern/musyx/src/musyx/runtime/stream.c` - and `git show ada6d97 -- extern/musyx/.../stream.c` is
**empty**. The guards were never committed. `tools/run_goal.sh:446` explains why:

```sh
stage_change() {
  ...
  ( cd "$WT" && git add -A -- src include config docs configure.py files.cmake CMakeLists.txt ) || true
}
```

`git add -- src` does not match `extern/musyx/src/...` (a pathspec is anchored at the repo root).
Confirmed directly:

```
$ git add -A --dry-run -- src include config docs configure.py files.cmake CMakeLists.txt
add 'docs/HANDOFF.md'
add 'src/MetroidPrime/Player/CGameState.cpp'
$ git add -A --dry-run -- src | grep -c extern
0
```

So `stage_change` silently dropped the source half of that item and committed the `Matching` flip
alone, which removed `sndStreamMixParameter` from the link. **`goal/decomp` is broken the same way** -
`git show goal/decomp:extern/musyx/src/musyx/runtime/stream.c | sed -n '758p'` is still
`#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 2)`, and lane 3 is carrying the identical uncommitted
fix in its worktree.

**I have put the four guards back** (the diff is byte-identical to the one lane 3 carries, and to the
one `docs/goal-notes/match-stream.md` describes), because without it nothing in this lane can be
judged. With it: `main.dol` is `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, exact.

## Wall 1 (the epilogue, 99.05%) — retail returns the pair BY VALUE

The previous run measured ~20 wrapper spellings and called it a register-allocation wall. It is not:
**retail's `fn_80145B90` is a two-argument function returning an 8-byte struct by value.** The
`r3`/`r4`/`r5` assignment is the hidden struct-return pointer plus two explicit arguments, not three
arguments. Returning it by value is what puts `lwz r0,20(r1)` first in the epilogue.

The lead that cracked it, which the earlier notes did not have: LR-first is the **majority** epilogue
order in this tree, not a rare spelling. Counting every function in `build/G2ME01/src` by the order of
the three reloads before its `mtlr`:

```
totals: {'LR-FIRST': 753, 'LR-LAST': 248}
LR-FIRST  frame=r1,-16(r1)  nsave=2  n=483
LR-LAST   frame=r1,-16(r1)  nsave=2  n=193
```

and **this unit already contains a 76-byte, 1-call, 2-store, LR-first function that is byte-identical
to retail's `fn_80145B90`** - its own `red_black_tree<...>::find`
(`include/rstl/red_black_tree.hpp:176`, `return iterator(find_node(key), &mHeader, false);`). Reading
that function's source is what gave the by-value shape. I also measured that **no** three-argument
`void` spelling reaches LR-first (7 variants, all LR-LAST at 76 B with the register assignment already
retail's), so the out-parameter form is a dead end rather than an unsolved wall.

New spellings, for the record, all measured and all **not** reaching it:
`u32* words` / two-word struct / node in a local / `u32*` out / `const` words ptr / header in a local /
`tree` as `u32*` / `char*` out / node via a `u32` var / struct store / byte offset 4 / explicit return /
stack pair / local struct copy / char cursor / `if (key==key)` / header via `u32` var / `const void* tree` /
key twice / `char*` header / memberwise a,b / memberwise b,a (64 B) / tree+8 via `u32*` / ptr members /
`void*` temp / u32 lvalue / `cast both` / key local / array idx - **all LR-LAST**.
LR-first but the wrong size: trailing void call (80 B), trailing call on `r0` (84 B), `sink2(tree+8)`
(80 B), branch converge (92 B), call helper (92 B), pure tail (88 B), last store `r3` (84 B, LR-LAST).

Two details that are load-bearing and would otherwise cost the next run a run each:

- **The second word is the *address* of the header member, not its value.** `&tree.x08_header`, not
  `tree.x08_header`. Taking the value drops the function to 56 bytes in a 32-byte frame - the `addi`
  disappears. That is retail's `addi r0,r31,8` at 0x80145BBC.
- The by-value return needs a **two-argument constructor call in the return statement**
  (`return SEnvVarIter(walk(tree,key), &tree.x08_header);`). Building the struct field-by-field into a
  local and returning that is back to 56 bytes.

## Wall 2 (the prologue, 95.74%) — the container is taken by `const&`

The previous run read the residue correctly (one `lwz r31,16(r3)` sliding from +0x10 to +0x28) and
tried ~25 body spellings. The lever is the **parameter type**: with `STree* self` mwcceppc sinks the
root load to the end of the prologue; with `const STree& self` it emits it where retail has it.
Measured on the same body, probe-isolated:

| spelling | differing |
| --- | --- |
| root via `header.mRoot` / `get_root()` / `u32*` / declaration order / for-header | 11 of 47 |
| **`const STree& self`** | **4 of 47** |

and those last 4 are the comparator's `this` offset plus the two `bl` relocations, which objdiff
ignores. `red_black_tree::find_node` is itself a `const` member, which is why retail has this shape.

Two corrections to the previous notes, both measured:

- **It is `find_node`, not `find_lower_bound`.** `red_black_tree.hpp:241`'s `find_lower_bound` returns
  `result` unconditionally; retail re-tests the needle against the key with a **second** comparator
  call (0x80145C54) and returns null unless it is an exact match. That second call is what the walk's
  `+0x78` is, and getting it wrong costs 9 instructions.
- The comparator takes **the key, not the node**: `addi r4,r31,16` / `addi r5,r30,16`. The key is the
  node's fifth word. My first probe passed the node and sat at 70% - a wrong probe, not a wrong walk.

The item's open question ("the tree's member type is not established") was already answered by
`CGameStateEnvVarManager.hpp:25` (`rstl::map<rstl::string, CEnvironmentVariable>`); the shapes are
spelled out locally in `CGameState.cpp` because the map's own copy of the template is inlined into its
callers while retail keeps one out-of-line copy.

## The pre-existing `decl-order` failure is not mine

`GATE FAIL: decl-order` is `main/MetroidPrime/Player/CGameState  permuted and not in decl_order.md`.
Stashed my diff, re-ran, and the first lines of the report are byte-identical with and without it:

```
$ git stash push -- src/MetroidPrime/Player/CGameState.cpp && python3 tools/check_decl_order.py --unit main/MetroidPrime/Player/CGameState
main/MetroidPrime/Player/CGameState    would break on a flip
   first 8 functions, ours vs retail:  SetCinematicState... SetCinematicState...
$ git stash pop && python3 tools/check_decl_order.py --unit main/MetroidPrime/Player/CGameState
main/MetroidPrime/Player/CGameState    would break on a flip
   first 8 functions, ours vs retail:  SetCinematicState... SetCinematicState...
```

Both wrappers are declared in the order 0x8014601C, 0x80145B90, 0x80145BDC - which is **not** retail
descending (0x8014601C, 0x80145BDC, 0x80145B90), and `fn_80145B90` and `fn_80145BDC` are the wrong way
round. It costs nothing today: `nm` on the linked ELF shows 0x80145b90 / 0x80145bdc / 0x8014601c, and
`main.dol` is byte-exact, because `configure.py`'s section anchors place them. **If this unit is ever
flipped, that pair must swap** - and note that `fn_80145B90` and `fn_80145BDC` are different sizes
(76 vs 188), so a permutation is not a silent no-op.

## Not attempted

- `CPersistentOptionsMapLookup.cpp` carries its own three-argument `fn_80145B90` at 99.05% (its
  comments already record the same wall). It is `NonMatching` and not in the DOL link. The by-value
  spelling above fixes it too, but that is a different unit and a different item.
- `fn_80142288`/`fn_801422D4` (0x80142288, 76 B + 108 B) are still bodiless and remain the cheapest
  remaining target in this unit.

## Verification

```
sha1sum build/G2ME01/main.dol          6ef9b491d0cc08bc81a124fdedb8bfaec34d0010   (retail)
./tools/decomp_build.sh                All: 29.19% fuzzy, 21.34% matched, 11.60% linked (9510 / 28465)
python3 tools/check_symbol_names.py    checked 484 units; 0 declared names are missing
./tools/probe_sources.sh               736 files, 0 failed, 0 errors; LINKED (254 undefined, 0 duplicates)
python3 tools/check_docs_claims.py     docs claims agree with the tree
python3 tools/bytescmp.py ... CGameState.o fn_80145BDC 0x80145BDC 0xBC
    +3C  ours 48000001 | retail 4bee179d   bl   (relocation)
    +78  ours 48000001 | retail 4bee1761   bl   (relocation)
    2 differing instructions of 47 (188 bytes ours vs 188 retail)
./tools/gate.sh build/goal/judge/report.base.json
    report ok / per-function diff  matched 9506 -> 9510  linked 4835 -> 4836 (+4 at 100%)
    configure, ninja + build.sha1, hashes vs config.yml, module wiring, dol_read, gs offsets,
    raw offsets, files.cmake, module order, port probe, port link gap: ok
    GATE FAIL: decl-order  (pre-existing, identical with and without this diff)
```

Files touched: `src/MetroidPrime/Player/CGameState.cpp` (the walk, both wrappers, the local shapes and
their comments) and `extern/musyx/src/musyx/runtime/stream.c` (**the build unblock, not this item's
work - see the root cause above**; it is outside what `stage_change` can stage, so it will not be
committed by the driver and the branch will still be broken after this lands unless that is fixed
separately). `docs/HANDOFF.md` was rewritten by the judge's own `check_docs_claims.py --write`.

NEW: fix-stage-change-extern | progress | MetroidPrime/Player/CGameState | `tools/run_goal.sh:446` `stage_change` cannot stage `extern/`, so `match-stream` (ada6d97) committed a `Matching` flip for musyx/runtime/stream.c with its four-guard source fix dropped, leaving `goal/decomp` and lanes 1-2 unlinkable (`undefined: sndStreamMixParameter`); the fix is the one-liner pathspec but agents may not edit `tools/`

## Gate fix round

`decl-order` was the only failing gate step. The log's own fix is "declare the unit's functions
descending by retail offset", and that is what this round did - **the list alternative was not
taken, because the permutation was introduced by this item, not inherited** (see below).

`src/MetroidPrime/Player/CGameState.cpp`: moved `fn_80145BDC` and `fn_80145B90` down to after
`fn_80145C98`, so the five functions retail knows in that region now run 0x8014601C,
0x80145F8C, 0x80145C98, 0x80145BDC, 0x80145B90 in source - descending. `fn_80145BDC` is now
forward-declared at its old position (line 234) because `fn_8014601C` calls it; a declaration
emits no code, so it is exempt from the ordering either way. The `fn_80145B90` half of the
`fn_8014601C`/`fn_80145B90` comment stayed with `fn_8014601C` and a pointer replaced it. No
statement, signature or comment claim changed - only where five definitions sit.

Measured before/after on the same tree (`python3 tools/check_decl_order.py --unit CGameState`,
101 functions common with retail): **5 misplacements, all inside positions 83-87** -
`fn_80145C98`, `FindEnvironmentVariable`, `fn_8014601C`, `fn_80145B90`, `fn_80145BDC` against
retail's `fn_80145B90`, `fn_80145BDC`, `fn_80145C98`, `FindEnvironmentVariable`, `fn_8014601C`.
After the move, 0. The 15 functions from position 86 up were displaced by the 2 misplacements;
that is the usual "a two-position defect is not a two-function defect" shape, as the `mainMid`
entry in `docs/research/decl_order.md` records.

**The "pre-existing, identical with and without this diff" claim in the Verification block above
(and in the section it heads) was wrong, and only measurement separates it.** That check compared
the *first 8 rows* of the `--unit` diagnostic, and the defect is 74 rows further down, so the two
runs could not have differed - a verification that cannot fail. Two facts, both from the tree:

- `git show HEAD:src/MetroidPrime/Player/CGameState.cpp` has **no body** for `fn_80145B90` (the
  name appears once, in a comment) and only a forward declaration for `fn_80145BDC` at line 185.
  `check_decl_order.py` pairs by name, so at HEAD it could not see either; the two functions this
  item matched are the two it moved.
- The three functions that remain in the region were in correct order at HEAD: source 191
  (`fn_8014601C`, 0x8014601C), 197 (`FindEnvironmentVariable`, 0x80145F8C), 226 (`fn_80145C98`,
  0x80145C98) - already descending, i.e. not permuted before this diff.

So this was a defect of this item, and listing the unit in `decl_order.md` would have recorded a
false "inherited" claim in a doc whose only job is to be right about that.

## Gate fix round - verification

```
python3 tools/check_decl_order.py     ok: 939 unit(s) checked, 28 permuted, all 28 accounted for
                                      in decl_order.md     (was: 1 unaccounted, CGameState)
./tools/decomp_build.sh               All: 29.19% fuzzy, 21.34% matched, 11.60% linked
                                      (9510 / 28465)  - unmoved by the reorder
main/MetroidPrime/Player/CGameState   92 / 116 functions matched - unmoved
sha1sum build/G2ME01/main.dol         6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/gate.sh build/goal/judge/report.base.json
```

The reorder moves no code, so the counts holding is the check that matters: objdiff pairs by
name, `unit_fit.sh` compares sizes, and the DOL sha1 is retail's - none of the three can see a
permutation. `check_decl_order.py` is the only thing that can, and it is now clean.

Still open, and **not** moved by this round: `docs/research/decl_order.md`'s own prose says "18
units" and quotes `837 unit(s) checked, 18 permuted, all 18 accounted for`. The tool has printed
far more than that for a while - it now prints 939/28 - so that paragraph was already stale before
this item and is left alone here (the paragraph itself says to take the tool's number).

---

# Attempt 3 (2026-09-29, lane 2) — reproduced on a NEW head. Unit 89 -> 92, gate PASS.

`tools/goal_check.sh build/goal/item.json` → **PASS**. The three functions the item names are at
100.00% and the unit rose strictly, which is the whole acceptance test for a `progress` item.

| | this head `afb51fb` |
| --- | --- |
| `main/MetroidPrime/Player/CGameState` | **89 -> 92** / 116 |
| global `matched_functions` | **9509 -> 9513** (`build/goal/judge/report.base.json` -> `build/report.json`) |
| global `linked` | 4852 -> 4853 |

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 9509 -> 9513   linked 4852 -> 4853
  ok    check_symbol_names.py
  ok    All:  29.20% fuzzy, 21.35% matched, 11.64% linked (9513 / 28465 functions)
  ok    target rose: main/MetroidPrime/Player/CGameState: 89 -> 92 / 116 functions
  ok    no asm added
goal_check: PASS progress-cgamestate-map-lowerbound
```

**Everything above the `## Verification` heading in attempt 2 still holds and was re-measured, not
recalled.** Attempt 2's work was not in this tree (HEAD has moved on through `match-cfidget` and
`match-cmetaanimrandom`; `git show HEAD:src/MetroidPrime/Player/CGameState.cpp` has no body for
`fn_80145B90`), so this run rebuilt it from the same reasoning. The two walls were not re-searched:
the by-value `const_iterator` return and the `const SGameStateVarTree&` parameter went in on the
first build and both landed. This entry records only what is new.

## The build was still broken at this head

Same defect, same cause, still unfixed at `afb51fb`:

```
$ ./tools/decomp_build.sh
#   undefined: 'sndStreamMixParameter'
#   Referenced from 'CDSPStreamManager::UpdateVolume(int,int)'
$ cat build/goal/judge/record-gate.log | tail -3
ninja + build.sha1          FAIL
    #   undefined: 'sndStreamMixParameter'
baseline recorded at afb51fb
```

I re-applied the same four `MUSY_VERSION` guards to `extern/musyx/src/musyx/runtime/stream.c`
(lines 336, 758, 793 and 874). `extern/` is still outside what `stage_change` stages, so the driver
will not commit it and **the branch will still be unlinkable after this item lands.** One detail
that cost ten minutes here and would cost the next run the same: the new guard around
`sndStreamMixParameterEx`/`sndStreamFrq` must close **after** `#pragma pop`, not before it —
MWCC treats `#pragma pop` without a matching `push` as a hard error (it cannot see across the
`#endif`), and the build stops with `preceding '#pragma push' is missing`.

## New, and the thing attempt 2's notes do not say: the walk already exists in the object

`build/G2ME01/src/MetroidPrime/Player/CGameState.o` **already contains this exact walk** as a COMDAT
copy of the template member, under its own name:

```
$ ./build/binutils/powerpc-eabi-objdump -d build/G2ME01/src/MetroidPrime/Player/CGameState.o
00004ad8 <find_node__Q24rstl462red_black_tree<Q24rstl66basic_string<c,...>,Q24rstl104pair<...,
             20CEnvironmentVariable>,0,Q24rstl125select1st<...>,Q24rstl81less<...>,
             Q24rstl17rmemory_allocator>CFRCQ24rstl66basic_string<c,...>>:
```

Its 47 words are **byte-identical to retail's `fn_80145BDC`** apart from the two `bl` relocations
(`48000001` here against `4bee179d`/`4bee1761` retail). Compared word for word against
`./tools/dis.sh 0x80145BDC 0xBC`. So retail's 0x80145BDC *is* that instantiation, the tree is
already right, and the only thing missing is **retail's name on it** — objdiff pairs by name, so
`fn_80145BDC` scored 0% while those exact bytes sat in the object.

**Do not "fix" this by calling the template.** `fn_80145BDC { return tree->find_node(key); }` — the
obvious spelling, and the one I wrote first — compiles to a five-instruction tail call
(`stwu/mflr/stw/bl/lwz/mtlr/addi/blr`) because mwcceppc does **not** inline the out-of-line
`find_node` it has already emitted. That scores **16.72%**, and it drags `fn_80145B90`/`fn_8014601C`
down with it. The walk has to be written out, over local node/header/tree shapes, which is what
attempt 2 concluded and what this run did. The three functions are 92/116 only because of that.

The corollary is worth keeping: **"our object does not define what retail defines" is a name
question as often as a code question.** Before searching for a spelling, check whether the bytes
are already in the object under a mangled or COMDAT name.

## Decl order (the gate step attempt 2 had to fix a second time)

The two definitions must sit **between `fn_80145C98` (0x80145C98) and `AddVariable` (0x80145B0C)**,
not after `AddVariable`: 0x8014601C, 0x80145F8C, 0x80145C98, 0x80145BDC, 0x80145B90, 0x80145B0C is
descending, and putting them after `AddVariable` makes 0x80145B0C -> 0x80145BDC ascend. That is
what I did first, and `check_decl_order.py` reported

```
main/MetroidPrime/Player/CGameState    would break on a flip
declaration order not accounted for:
  main/MetroidPrime/Player/CGameState  permuted and not in decl_order.md
```

`fn_80145BDC`'s forward declaration stays next to `fn_8014601C`, which calls it; a declaration
emits no code and is exempt either way. After the move: `939 unit(s) checked, 28 permuted, all 28
accounted for in decl_order.md`.

## Verification

```
sha1sum build/G2ME01/main.dol                       6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/decomp_build.sh                             All: 29.20% fuzzy, 21.35% matched, 11.64% linked
                                                    (9513 / 28465 functions)
./tools/goal_check.sh build/goal/item.json          PASS  (output above)
./tools/probe_sources.sh                            736 files, 0 failed, 0 errors;
                                                    LINKED (254 undefined, 0 duplicates)
python3 tools/check_symbol_names.py                 checked 484 units; 0 declared names are missing
python3 tools/check_decl_order.py                   ok: 939 unit(s) checked, 28 permuted, all 28 accounted
python3 tools/check_docs_claims.py                  docs claims agree with the tree
python3 tools/bytescmp.py ... CGameState.o fn_80145BDC 0x80145BDC 0xBC
    +3C  ours 48000001 | retail 4bee179d   bl   (relocation)
    +78  ours 48000001 | retail 4bee1761   bl   (relocation)
    2 differing instructions of 47 (188 bytes ours vs 188 retail)
python3 tools/bytescmp.py ... CGameState.o fn_80145B90 0x80145B90 0x4C
    1 differing instruction of 19 - the bl (relocation); 76 bytes ours vs 76 retail
python3 tools/bytescmp.py ... CGameState.o fn_8014601C 0x8014601C 0x4C
    1 differing instruction of 19 - the bl (relocation); 76 bytes ours vs 76 retail
```

Files touched: `src/MetroidPrime/Player/CGameState.cpp` (the local shapes, the walk, both wrappers,
their comments) and `extern/musyx/src/musyx/runtime/stream.c` (**the build unblock, not this item's
work**). `docs/HANDOFF.md` was rewritten by the judge's own `check_docs_claims.py --write`.

`./tools/unit_fit.sh` still reports this unit's 97 COMDAT extras and an 8288-byte `.text` overage;
that is unchanged from HEAD and is not what this item is judged on (`progress` items are judged on
`report.json`'s per-function matches, and the unit stays `NonMatching`).

## Still open

- `fn_80142288`/`fn_801422D4` (0x80142288, 76 B + 108 B) are still bodiless and remain the cheapest
  remaining target in this unit.
- `CPersistentOptionsMapLookup.cpp` carries its own three-argument `fn_80145B90` at 99.05%. The
  by-value spelling fixes it too; different unit, different item.

NEW (repeat of the id filed in attempt 2, still unfixed at `afb51fb` - dedupe): fix-stage-change-extern | progress | MetroidPrime/Player/CGameState | `tools/run_goal.sh:446` `stage_change` cannot stage `extern/`, so `match-stream` (ada6d97) committed a `Matching` flip for musyx/runtime/stream.c with its four-guard source fix dropped and every lane has been building an unlinkable tree since (`undefined: sndStreamMixParameter`); the fix is a one-line pathspec but agents may not edit `tools/`
