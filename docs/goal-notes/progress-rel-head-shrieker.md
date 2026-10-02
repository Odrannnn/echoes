# progress-rel-head-shrieker — 17/17, module sha1 unchanged, but the gate's `ninja` step is broken at HEAD

Goal item `progress-rel-head-shrieker` (`kind: progress`, `target: module:Shrieker`), lane 4,
`wt-mp2-goal-L4` at `afb51fb`. The module work is **done and measured**. The item cannot be
judged clean because `gate.sh`'s `ninja + build.sha1` step **fails before and after my change**
— see "The blocker" at the bottom, which is not mine and not fixable from this item.

## What landed

Three files, the four-file carve minus the fourth (this file is deliberately **not** in
`files.cmake`, so `check_files_cmake.py` needs no change — see the measured reason below):

- `src/MetroidPrime/ScriptObjects/CShriekerRel.cpp` (new) — 17 functions, `.text 0x0..0x154`.
- `configure.py` — a `Rel("Shrieker", [Object(Matching, "MetroidPrime/ScriptObjects/CShriekerRel.cpp")])`
  block, with the measured differences from the sibling heads in the comment.
- `config/G2ME01/rels/Shrieker/splits.txt` — one new entry, `MetroidPrime/ScriptObjects/CShriekerRel.cpp`
  claiming **only** `.text 0x00000000 end:0x00000154`. The `REL/global_destructor_chain.c` and
  `REL/REL_Setup.cpp` entries (`tools/wire_rel_setup.py`, 5/5) are untouched, so the module's
  total claim is now 17 ours + 5 setup + 2 destructor-chain.

`docs/HANDOFF.md`, `docs/RUNNING_THE_DECOMP.md` and `docs/LANE_BRIEFING.md` are **not** edited, per
the goal-unit-prompt. The Attempted-modules rows I drafted for the latter are at the bottom of this
file for the orchestrator to apply.

## Measured, not recalled

The **module hash is the result**, and it holds:

```
$ python3 - <<'PY'   # sha1 of build/G2ME01/<Module>/<Module>.rel vs config/G2ME01/config.yml
modules checked: 86  mismatches: 0
$ sha1sum build/G2ME01/Shrieker/Shrieker.rel orig/G2ME01/files/RelProd/Shrieker.rel
34929da0e094650d473a6db25b6265bbd6d515ba  build/G2ME01/Shrieker/Shrieker.rel
34929da0e094650d473a6db25b6265bbd6d515ba  orig/G2ME01/files/RelProd/Shrieker.rel
$ cmp build/G2ME01/Shrieker/Shrieker.rel orig/G2ME01/files/RelProd/Shrieker.rel   # -> equal
$ for f in orig/G2ME01/files/RelProd/*.rel; do cmp -s "build/G2ME01/$(basename $f .rel)/$(basename $f)" "$f" || echo DIFF $f; done
# no output, all 86 equal
```

`34929da0e094650d473a6db25b6265bbd6d515ba` is what `config/G2ME01/config.yml:351` already records.
**It is unchanged from before the change** — the claim reproduces the bytes rather than replacing them.

The rest, all green:

```
$ python3 tools/audit_rel_claim.py Shrieker
ok   MetroidPrime/ScriptObjects/CShriekerRel.cpp          0x00000000..0x00000154  17/17 functions
ok   REL/global_destructor_chain.c                        0x0000760C..0x00007680  2/2 functions
ok   REL/REL_Setup.cpp                                    0x00007680..0x00007824  5/5 functions
0 claim(s) with a problem
Shrieker: preplf 152 text symbols, plf 152, 0 dropped by -strip_partial

$ ./tools/unit_fit.sh MetroidPrime/ScriptObjects/CShriekerRel.cpp
   .text      claimed    340   ours    340   retail    340   fits
   no extra functions: our object defines only what the retail unit object does

$ python3 tools/check_decl_order.py            # ok: 939 unit(s) checked, 28 permuted (pre-existing), all accounted
$ python3 tools/check_symbol_names.py          # checked 484 units; 0 declared names are missing
$ python3 tools/check_files_cmake.py           # every configured DOL object is in files.cmake or excluded with a reason
$ python3 tools/check_module_wiring.py         # 87 units of our own code in 67 modules, Shrieker present
$ python3 tools/check_raw_offsets.py           # ok: 152 raw-offset site(s) in 61 file(s), all documented
$ ./tools/probe_sources.sh                    # probe: 736 files, 0 failed, 0 errors; link: LINKED (254 undefined, 0 duplicates)
$ ./tools/goal_check.sh-equivalent per-function diff vs build/goal/judge/report.base.json
worse functions: 0        module:Shrieker matched_functions  7 -> 24
```

`build/G2ME01/report.json` for `Shrieker/MetroidPrime/ScriptObjects/CShriekerRel`:
**100.00% fuzzy, 100.00% matched code, 17/17 functions, `metadata.complete: true`**, no function
below 100%. Module `Shrieker` totals: 152 `total_functions` across its 7 units, of which
17 + 5 + 2 = **24 matched** (was 7). `All:` `matched_functions` 9509 -> 9526.

`docs claims` passes under the judge's `MP_GATE_DOCS_WRITE=1` (it rewrites the derived state block
itself); `MP_GATE_DOCS_WRITE=1 ./tools/gate.sh` reports **`docs claims ok`**, and the only failing
step is `ninja`.

## The three differences from `CMysteryFlyerRel.cpp`, established by diffing the bytes

The `fn_<id>_<off>` dtk labels say nothing about which function is which, so I disassembled
**our own two objects** and diffed them instruction by instruction rather than reading the names:

```
$ powerpc-eabi-objdump -d build/G2ME01/src/.../CMysteryFlyerRel.o | <mnemonic column>
$ powerpc-eabi-objdump -d build/G2ME01/src/.../CShriekerRel.o  | <mnemonic column>
$ diff
MysteryFlyer: 92 insns   Shrieker: 85 insns
1c1   < li r3,1                     > addi r3,r3,2244          # 2244 = 0x8C4
3c3   < addi r3,r3,2072             > li r3,1                  # 2072 = 0x818
46,52d45  < 6 x (lfs/stfs) + blr                             # the three-float copy, absent here
(remaining differences are `bl` displacements and symbol names only)
```

1. **The first two accessors are swapped in role.** This module opens `addi r3,r3,0x8c4`
   (the member address) where MysteryFlyer opens `li r3,1` and puts `addi r3,r3,0x818` second.
   So the member offset is **0x8C4**, not 0x818 — this is the one thing that had to be read off
   the disassembly rather than copied.
2. **No three-float copy.** MysteryFlyer's `fn_45_B4` is 7 instructions (0x1C = 28 bytes) at 0xB4;
   this module's vtable entry `fn_69_B4` therefore sits at **0xB4**, not 0xD0. That 28 bytes is
   the whole of the 0x170 -> 0x154 difference.
3. **0x4C..0xB4 is the same fourteen-accessor block**, unchanged: the `lbl_8041AAB8` store at
   +0x448, the `lbl_8041B758` accessor, the `+0x34c` bit-3 read, `+0x754`, `+0x44f`, and five
   predicates.

So no spelling had to be discovered: every body is one `CMysteryFlyerRel.cpp` or
`CGrenchlerRel.cpp` already reproduces at 100%. **`fn_69_10` is instruction for instruction
`fn_45_10`** (the item's hint was right), and it is **not** an `optional_object` template problem:
retail *calls* the converting constructor out of line at **`fn_69_6EE4`** (0x6EE4, 0x3C — six word
copies out of `r4+0x00..r4+0x14`, then `stb 1, 0x18(r3)`, i.e. six words of `CAABox` plus a
validity byte at +0x18), and that function stays unclaimed. So it is one call by its dtk name, with
the constructor declared `void fn_69_6EE4(void* out, const CAABox& box)` (const ref; a by-value
spelling grows the frame to 0x40) and the one-method local `class CPhysicsActor { public: CAABox
GetBoundingBox() const; };` stand-in. **Do not include `MetroidPrime/CPhysicsActor.hpp`** — it
reaches `Collision/CMaterialList.hpp`, whose file-scope `static EMaterialTypes SolidMaterial` adds
0x28 bytes of `.data` and breaks the module hash with every function at 100%. The unit fits at
exactly 340 bytes with no `.data` and no `.bss`, which is the measurement that confirms it.

The two callees are named by what they are, and **no `symbols.txt` rename was needed** — the item's
warning about a mangled `SetLoader_*` name does not apply to this module:

- `fn_80218C30` is already the plain name in `config/G2ME01/symbols.txt:9492`. It is 8 bytes at
  0x80218C30, `stw r3, gLoader_Shrieker@sda21(r0); blr`, immediately after
  `LoadShrieker__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at 0x80218C04 (0x2C bytes, so it
  ends exactly there) — the store-slot-address setter, matching
  `src/MetroidPrime/ScriptLoader/Shrieker.cpp` (a `Matching` unit) which reads the slot.
- The loader record is **`lbl_69_bss_60` at `.bss:0x60`**, four bytes — **not `.bss:0x0`** as in
  MysteryFlyer. This module's `.bss` holds nine objects and `.bss:0x0` is a 16-byte float block read
  and written far above the head. Because the split claims `.text` only, dtk's `.bss` object must
  define it, hence `extern` under MWCC and a host definition.

**No dead-strip hazard, measured**: the module's `ldscript.lcf` puts all fourteen of
`fn_69_0`..`fn_69_B4` in its `FORCEACTIVE` block, and `.data:0x314` (CShrieker's own 0x148-byte
vtable) stores every one of them, including `fn_69_10` and `fn_69_B4`. `RELMain`/`RELExit` are the
module entry points and `fn_69_124` is called from `RELMain`. `audit_rel_claim.py`'s preplf/plf
counts confirm it: 152 in, 152 out, 0 dropped by `-strip_partial`.

**`fn_69_B4` is vtable entry 0x3C of `.data:0x314`, calling slot 0x38 = `HealthInfo__3CAiFv`**, so
it is written as a member call on a thirteen-virtual stand-in class. That spelling is the family's,
measured in `CIngPuddleRel.cpp`: loading the vtable by hand compiles to `lwz r3,0(r3)` where retail
has `lwz r12,0(r3)`.

**Not in `files.cmake`, and the reason is measured, not assumed**: the file calls `fn_69_154` (the
module's entity loader) and `fn_80218C30`, neither of which the port can link, and it defines
`RELMain`/`RELExit`, which collide in a flat link. `check_files_cmake.py` prints "33 further units
are out because they define a module entry point" and counts this one; the port's undefined count
is unmoved at 254 and `probe_sources.sh` links with 0 duplicates.

`fn_69_154` (0x154, 0x7D8), the module's own entity loader, and the ~150 functions above it stay
retail — behavioural class code needing the CActor/CPatterned hierarchy, as the item said to leave.

## Two things that cost time, worth keeping

**`flip_test.sh` cannot test a `Rel(...)` unit.** It resolves the source root with
`s.rfind('MusyX(', 0, m.start())` and an open-paren count, which misfires on any object declared
after a `MusyX(...)` call that leaves a paren imbalance — and every `Rel(...)` block in
`configure.py` sits after one. It reports:

```
TEST MetroidPrime/ScriptObjects/CShriekerRel.cpp
    no source file (extern/musyx/src/MetroidPrime/ScriptObjects/CShriekerRel.cpp) - configure.py
    would link the retail object and this would pass while proving nothing
  FAIL
```

**This is pre-existing and not specific to this file**: `./tools/flip_test.sh
MetroidPrime/ScriptObjects/CGrenchlerRel.cpp` — the already-landed, `Matching`, sha1-verified
Grenchler head — fails with the identical message. For a REL module the acceptance test is the
module sha1, which is what I ran and what holds above. Do not read this FAIL as a defect in the unit.
Worth a `NEW:` in the tools queue, not in this one.

**`check_raw_offsets.py` measures 0 sites for this file, so it must have no doc section.** The
family's other heads each need a `docs/research/raw_offsets.md` section; I wrote one for
`CShriekerRel.cpp` and `check_raw_offsets.py` then failed it the other way round —
"documented but has no raw offsets any more - delete the section". The checker keys on
`reinterpret_cast`/`(char*)` plus a `+ 0x..`, and **this file reaches all four of its offsets
through `static_cast< char* >` or a subscript, which it does not key on** (Grenchler's file is seen
only because of its separate `+0x54` three-float copy, which this module does not have). So the
section is deleted and `check_raw_offsets.py` is clean at 152 sites in 61 files. This is a real
undercount in the tool, not a doc omission — the four members it misses are `+0x8C4`, `+0x754`,
`+0x44F`, `+0x34C`.

## The blocker — `ninja` fails, and it is not mine

`gate.sh`'s `ninja + build.sha1` step fails, so the DOL never links and `main.dol` is never
produced, so `hashes vs config.yml` is skipped and no `All:` line is printed. The error:

```
### mwldeppc.exe Linker Error:
#   undefined: 'sndStreamMixParameter'
#   Referenced from 'CDSPStreamManager::UpdateVolume(int,int)' in CDSPStreamManager.o
```

**This reproduces with my change stashed** (`git stash -u`, rebuild, same error, `git stash pop`),
so it is present at `afb51fb` and is not caused by this item. Its cause, measured:

```
$ build/binutils/powerpc-eabi-nm -n build/G2ME01/src/musyx/runtime/stream.o | grep -i 'MixParameter\|streamKill'
00000a3c T streamKill
00001e60 T sndStreamMixParameterEx          <-- wrong function
$ build/binutils/powerpc-eabi-nm -n build/G2ME01/obj/musyx/runtime/stream.o | grep -i 'MixParameter\|streamKill'
00000a3c T streamKill
00001e4c T sndStreamMixParameter            <-- what retail has
$ sed -n '336p;758p' extern/musyx/src/musyx/runtime/stream.c
#elif MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 2)   # streamKill
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 2)     # sndStreamMixParameter
```

`configure.py:1275` is `Object(Matching, "musyx/runtime/stream.c")` — flipped by **`ada6d97`
"match: match-stream"** — but the four `#if` guards in `extern/musyx/src/musyx/runtime/stream.c`
that that item's own notes describe were **never committed** (`git log -- extern/musyx/src/musyx/runtime/stream.c`
shows only the two vendoring commits, neither of them the guard change). With the unit `Matching`,
dtk stops supplying the retail object, so the DOL loses `sndStreamMixParameter` entirely and
`CDSPStreamManager.cpp:426` — the one caller — has nothing to bind to. `docs/goal-notes/match-stream.md`
documents the correct four-line fix in detail; the queue already owns it twice
(`musyx-dsp-sndstreammixparam` and `musyx-version-patch`, both `match` on
`musyx/runtime/stream.c`), so I have **not** filed another `NEW:` for it and have **not** fixed it
here: an unrelated fix in this diff is exactly what the reviewer rejects.

`build/goal/run.log` shows items 5 and 6 (`progress-rel-head-lumite`, twice) failing the same two
checks — `gate.sh` and `decomp_build.sh printed no All: line` — for this same reason, after
`ada6d97` landed at 22:14:59. So the loop has been unable to pass *any* item on this lane since
then. **This item is blocked on that one commit, not on anything in Shrieker.**

## NEW

None filed. `fn_69_10` was the only function the item flagged, and it took no spelling work.

## For the orchestrator: the Attempted-modules rows

`docs/RUNNING_THE_DECOMP.md` is off-limits to me, so here is the row for the "Attempted modules"
table (line ~1971, after `CGraphicsTimeProvider`'s predecessors), with every number measured above:

> | `Shrieker` | **Head landed, 2026-09-29 (lane 4, goal item `progress-rel-head-shrieker`) -
> `CShriekerRel.cpp`, `.text 0x0..0x154`, **17/17 at 100.00%**, module sha1
> `34929da0e094650d473a6db25b6265bbd6d515ba` unchanged against `config/G2ME01/config.yml` and
> `cmp`-equal to `orig/G2ME01/files/RelProd/Shrieker.rel`, all 86 RELs holding,
> `audit_rel_claim.py` 0 problems (`17/17 functions`; 152 preplf text symbols, 152 in the plf, 0
> dropped by `-strip_partial`), `unit_fit.sh` `.text claimed 340 ours 340 retail 340, fits` and
> `no extra functions`, `check_decl_order.py` ok, `check_raw_offsets.py` ok, `check_files_cmake.py`
> ok, `check_module_wiring.py` ok, `check_symbol_names.py` 0 missing, `probe_sources.sh` 736 files
> 0 failed. **`flip_test.sh` could not be run — it is a DOL tool and misfires on `Rel(...)` blocks;
> see the notes.** Module 69, one of the 27. Nothing had to be discovered: the block is
> `CMysteryFlyerRel.cpp`'s with three measured differences, established by disassembling our own two
> objects and diffing them (92 instructions against 85) rather than read off the `fn_<id>_<off>`
> names, which say nothing about which function is which. (1) The first two accessors are swapped
> in role — this module opens `addi r3,r3,0x8c4` (2244) where MysteryFlyer opens `li r3,1` and
> puts `addi r3,r3,0x818` (2072) second, so the member offset is **0x8C4**. (2) It has **no**
> three-float copy, so its vtable entry `fn_69_B4` sits at **0xB4** rather than 0xD0, and that
> 28 bytes is the whole 0x170 -> 0x154 difference. (3) 0x4C..0xB4 is the same fourteen-accessor
> block. `fn_69_10` is instruction for instruction `fn_45_10`, with this module's own out-of-line
> `optional_object<CAABox>` constructor **`fn_69_6EE4`** (0x6EE4, 0x3C) as its callee — unclaimed, so
> one call by its dtk name, not a template instance. `fn_69_B4` is vtable entry 0x3C of the
> 0x148-byte table at `.data:0x314` (which also stores `fn_69_10`), calling slot 0x38 =
> `HealthInfo__3CAiFv`, reproduced by the same thirteen-virtual stand-in class. **No dead-strip
> hazard**: `ldscript.lcf` puts all fourteen of `fn_69_0`..`fn_69_B4` in `FORCEACTIVE` **and**
> `.data:0x314` stores every one, so no `force_active:` entry is needed. The import is the plain
> DOL symbol **`fn_80218C30`** (0x80218C30, 8 bytes, `stw r3, gLoader_Shrieker@sda21(r0); blr`,
> immediately after `LoadShrieker__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at 0x80218C04,
> which is 0x2C bytes and so ends exactly there), so **no `symbols.txt` rename and no DOL change**;
> the loader slot is **`lbl_69_bss_60` at `.bss:0x60`**, *not* `.bss:0x0` — this module's `.bss` holds
> nine objects and `.bss:0x0` is a 16-byte float block used far above the head. `fn_69_154`
> (0x154, 0x7D8), the module's entity loader, and the ~150 functions above it stay retail — class
> code needing the CActor/CPatterned hierarchy. **Not added to `files.cmake`**, for the reason the
> other heads measure. **152 is the module's complete text symbol count** on the
> `audit_rel_claim.py` convention: 17 ours + 128 unclaimed + 5 setup + 2 destructor-chain, which is
> the `total_functions` of its named plus `auto_*` units summed from `build/report.json` —
> `CShriekerRel` 17, `auto_00_00000154_text` 127, `auto_fn_69_7124_text` 1, `REL/REL_Setup` 5,
> `REL/global_destructor_chain` 2 |

And for the "Current module status" table further down (the `| \`MysteryFlyer\` | 23 functions: ... |`
shape, around line 2028):

> | `Shrieker` | **24 functions: the module head `.text 0x0..0x154` (17 ours) + 5 setup + 2
> destructor-chain**, of 152 total; the other 128 text functions unclaimed | landed 2026-09-29 (lane 4,
> goal item `progress-rel-head-shrieker`), module 69, one of the 27. `fn_69_0`, `fn_69_8`,
> `fn_69_10`, `fn_69_4C`, `fn_69_5C`, `fn_69_64`, `fn_69_6C`, `fn_69_74`, `fn_69_84`, `fn_69_90`,
> `fn_69_9C`, `fn_69_A4`, `fn_69_AC`, `fn_69_B4`, `RELExit`, `RELMain`, `fn_69_124`, all 100.00%;
> module sha1 `34929da0e094650d473a6db25b6265bbd6d515ba` unchanged and all 86 holding, `cmp` clean
> against `orig`; the module's own count 7 -> **24 of 152**; `All:` `matched_functions` 9509 ->
> **9526**, 0 functions anywhere worse. `unit_fit.sh` fits; `audit_rel_claim.py` 0 problems.
> **`flip_test.sh` does not work on a `Rel(...)` unit** (its `MusyX(` paren heuristic resolves the
> path to `extern/musyx/src/...`), so the module sha1 is the acceptance test. The import is the plain
> `fn_80218C30`; the loader slot is `lbl_69_bss_60` at `.bss:0x60`, not `.bss:0x0`. `fn_69_154`
> (0x154, 0x7D8), the entity loader, stays retail. Not added to `files.cmake`. |

---

# Run 2 (item 8, 2026-09-29, lane 4) — re-queued, re-landed from the transcript, identical numbers

Same item, same HEAD (`afb51fb`), same verdict: **the work is done and measured, and the gate's
`ninja` step still fails for a reason that is not this item's.** Every number below was re-measured
this run; nothing here is recalled from run 1.

## What changed: nothing but the re-land. The diff is byte-identical to run 1's.

The driver had `git reset --hard` + cleaned run 1's files, so `src/.../CShriekerRel.cpp`, the
`Rel("Shrieker", ...)` block and the `splits.txt` entry were all gone. **They cost nothing to
restore**, and that is the one thing worth reading here:

> **The driver writes a full agent transcript for every run, and it survives the reset.**
> `build/goal/agent/<item-id>-L<lane>-<n>-<timestamp>.jsonl` is one JSON object per line; every
> `write`/`edit` tool call is there verbatim in `part.state.input`. Replaying them reconstructs the
> exact tree a previous run produced. For this item: one `write` (line 103) plus six `edit`s
> (107, 117, 234, 241, 293, 299). The four edits at 234/241 and 293/299 were to
> `docs/research/raw_offsets.md` and `docs/RUNNING_THE_DECOMP.md` — **both were already reverted
> by run 1 and neither is applied here**, so only the `CShriekerRel.cpp` edits, the `splits.txt`
> edit and the `configure.py` edit were replayed.
>
> ```
> $ cd build/goal/agent   # in the DRIVER worktree, not this one
> $ ls *shrieker*
> progress-rel-head-shrieker-L4-7-20260929T210534.jsonl
> ```
>
> So a requeue costs minutes instead of an hour, and **no spelling work is repeated at all**.
> This is the general answer to "this item has been tried before": the previous attempt's
> transcript, not the previous attempt's notes, is where the recoverable work is. The notes
> record *what* was decided; the transcript holds *the bytes*.

The three files re-applied:

- `src/MetroidPrime/ScriptObjects/CShriekerRel.cpp` (new, 223 lines) — 17 functions, `.text 0x0..0x154`.
- `configure.py:2336` — the `Rel("Shrieker", [Object(Matching, "MetroidPrime/ScriptObjects/CShriekerRel.cpp")])`
  block with run 1's measured commentary.
- `config/G2ME01/rels/Shrieker/splits.txt:9` — `MetroidPrime/ScriptObjects/CShriekerRel.cpp`
  claiming only `.text start:0x00000000 end:0x00000154`.

`git status --porcelain` is exactly those three lines; **no `asm` added**, nothing in `docs/`,
`tools/`, `files.cmake` or `symbols.txt` touched.

## Re-measured this run (not recalled)

```
$ ./tools/decomp_build.sh
[1/11] MWCC build/G2ME01/src/MetroidPrime/ScriptObjects/CShriekerRel.o
[2/11] LINK build/G2ME01/Shrieker/Shrieker.preplf
[3/11] LINK build/G2ME01/Shrieker/Shrieker.plf          <- the module links
[4/11] REPORT
[5/11] LINK build/G2ME01/main.elf                        <- the DOL, and it fails; see below

$ sha1sum build/G2ME01/Shrieker/Shrieker.rel
34929da0e094650d473a6db25b6265bbd6d515ba     == config/G2ME01/config.yml:350, and == orig
$ for f in orig/G2ME01/files/RelProd/*.rel; do cmp -s ... || echo DIFF $f; done   # no output: all 86 equal

$ python3 tools/audit_rel_claim.py Shrieker
ok   MetroidPrime/ScriptObjects/CShriekerRel.cpp   0x00000000..0x00000154  17/17 functions
ok   REL/global_destructor_chain.c                 0x0000760C..0x00007680   2/2 functions
ok   REL/REL_Setup.cpp                             0x00007680..0x00007824   5/5 functions
0 claim(s) with a problem
Shrieker: preplf 152 text symbols, plf 152, 0 dropped by -strip_partial

$ ./tools/unit_fit.sh MetroidPrime/ScriptObjects/CShriekerRel.cpp
   .text      claimed    340   ours    340   retail    340   fits
   no extra functions

$ python3 tools/check_decl_order.py        # ok: 940 unit(s) checked, 28 permuted, all 28 accounted
$ python3 tools/check_symbol_names.py      # checked 484 units; 0 declared names are missing
$ python3 tools/check_files_cmake.py       # every configured DOL object is in files.cmake or excluded with a reason
$ python3 tools/check_module_wiring.py     # 87 units of our own code in 67 modules, Shrieker present
$ python3 tools/check_raw_offsets.py       # ok: 152 raw-offset site(s) in 61 file(s)   <- still 0 for this file, so still no doc section
$ ./tools/probe_sources.sh                 # probe: 736 files, 0 failed, 0 errors; link: LINKED (254 undefined, 0 duplicates)
```

`build/G2ME01/report.json`: `Shrieker/MetroidPrime/ScriptObjects/CShriekerRel` is
**100.0% fuzzy, 100.0% matched code, 17/17 functions, `complete_units: 1`** — no function below
100. Module `Shrieker` sums to **24 matched of 152** (17 ours + 5 `REL_Setup` + 2 destructor-chain),
up from 7. `All:` `matched_functions` **9509 -> 9526**, `total_functions` still **28465**.
Per-function diff against `build/goal/judge/report.base.json`: **0 functions worse, 0 improved
outside the new unit** (the two units `new` vs the baseline are
`Shrieker/MetroidPrime/ScriptObjects/CShriekerRel` and `Shrieker/auto_00_00000154_text`, which is
the retail range this claim vacated).

Every number in run 1's notes reproduced exactly on a from-scratch replay. Nothing in them was
wrong.

## The blocker is unchanged, and re-confirmed the only way that counts

`gate.sh`'s `ninja + build.sha1` step still dies on the DOL link, so `main.dol` is never produced
and `hashes vs config.yml` is skipped. Re-confirmed **this run** by stashing the whole change and
rebuilding, which is the only check that can distinguish "my diff broke it" from "it was already
broken":

```
$ git stash -u && ./tools/decomp_build.sh
### mwldeppc.exe Linker Error:
#   undefined: 'sndStreamMixParameter'
#   Referenced from 'CDSPStreamManager::UpdateVolume(int,int)' in CDSPStreamManager.o
$ git stash pop
```

Identical error with the change removed, so it is present at `afb51fb` and belongs to run 1's
section "The blocker — `ninja` fails, and it is not mine": `configure.py:1275` is
`Object(Matching, "musyx/runtime/stream.c")`, flipped by `ada6d97` "match: match-stream", while
the four `#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 2)` guards that
`docs/goal-notes/match-stream.md` specifies were never committed
(`git log -- extern/musyx/src/musyx/runtime/stream.c` still shows only the two vendoring commits).
With the unit `Matching`, dtk stops supplying the retail object, so the DOL loses
`sndStreamMixParameter` and the one caller in `CDSPStreamManager.cpp:426` has nothing to bind to.
`build/G2ME01/Shrieker/Shrieker.plf` — the part this item owns — **links and hashes**, every run.

The queue already owns the fix twice (`musyx-dsp-sndstreammixparam`, `musyx-version-patch`, both
`match` on `musyx/runtime/stream.c`), so still no third `NEW:` for it and still not fixed here.

## NEW

None filed, for the same reason as run 1: the only function the item flagged, `fn_69_10`, needed no
spelling work, and the one real blocker is already twice in the queue.

## The Attempted-modules rows

Unchanged from run 1 and still not applied by me (`docs/RUNNING_THE_DECOMP.md` is off-limits to the
lane). Run 1's two drafted rows — the "Attempted modules" table row and the "Current module status"
table row, both written out in full above with every number measured — carry over verbatim, and
this run re-measured every figure in them.

---

# Run 3 (lane 9, 2026-10-02) — re-landed from run 1's transcript; judge PASSES

The stream.c blocker is gone (`ef9e308b` committed the MusyX guards). Replayed the `write`
(CShriekerRel.cpp), the splits.txt edit and the configure.py Rel block from
`build/goal/agent/progress-rel-head-shrieker-L4-7-*.jsonl` (configure.py anchor had moved; inserted
the block after the Grenchler Rel by hand).

One real fix needed: `lbl_8041AAB8` was renamed in symbols.txt to `skDamageHitTime__10CPatterned`
(dtk "Failed to find symbol lbl_8041AAB8 in any module" at `dtk rel make`). Changed the extern in
CShriekerRel.cpp; module sha1 `34929da0...` still equals config.yml.

`./tools/goal_check.sh build/goal/item.json`: PASS - gate.sh ok, matched 12635 -> 12652,
module:Shrieker 7 -> 24 / 152, All: 12652 / 28465, no asm added.
Files: src/MetroidPrime/ScriptObjects/CShriekerRel.cpp (new), configure.py (Rel block),
config/G2ME01/rels/Shrieker/splits.txt (+3). Docs rows from run 1 remain unapplied (off-limits).
