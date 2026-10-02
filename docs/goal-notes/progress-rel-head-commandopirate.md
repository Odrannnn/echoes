# progress-rel-head-commandopirate

**STATUS: DONE.** Module 9's head, `.text 0x0..0x168`, 17 functions, `Matching` at 100.00%.

## What I did

Wrote `src/MetroidPrime/ScriptObjects/CCommandoPirateRel.cpp` from the dtk disassembly
(`build/G2ME01/CommandoPirate/asm/auto_00_00000000_text.s`) and the landed heads
(`CMysteryFlyerRel.cpp`, `CIngSpaceJumpGuardianRel.cpp`), added the split
`config/G2ME01/rels/CommandoPirate/splits.txt` -> `.text 0x0..0x168`, added the
`Rel("CommandoPirate", [Object(Matching, ...)])` block in `configure.py`, and the
`docs/research/raw_offsets.md` section the gate requires for a new file with raw offsets.

**No name had to be discovered.** `config/G2ME01/rels/CommandoPirate/symbols.txt` already names
`RELMain`, `RELExit` and all 17 head functions `fn_9_*`; the module needed **no `symbols.txt`
rename and no DOL change** and no `__MWERKS__`-only rename, unlike FishCloud/AtomicAlpha.

## What I measured

| what | command | result |
| --- | --- | --- |
| unit | `objdiff-cli report generate` | `CommandoPirate/MetroidPrime/ScriptObjects/CCommandoPirateRel` **17/17 at 100.00%**, `metadata.complete: True` |
| module hash | `sha1sum build/G2ME01/CommandoPirate/CommandoPirate.rel` | `81c9c6e8c24e82d585506f5c5faf965ede214b24`, **matches `config/G2ME01/config.yml`** |
| `.rel` identity | `cmp ... build/G2ME01/... orig/G2ME01/files/RelProd/CommandoPirate.rel` | clean |
| all 86 RELs | the gate's own hash pass | `mismatched: none (86 ok)` |
| DOL | `sha1sum build/G2ME01/main.dol` | `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` unchanged |
| no filled gaps | `python3 tools/audit_rel_claim.py CommandoPirate` | `17/17 functions` in the claim, `0 claim(s) with a problem`, `256 text symbols, 0 dropped by -strip_partial` |
| object fits | `./tools/unit_fit.sh MetroidPrime/ScriptObjects/CCommandoPirateRel.cpp` | `.text claimed 360 ours 360 retail 360 fits`; `no extra functions` |
| decl order | `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CCommandoPirateRel` | `ok: 1 unit(s) checked, none emits its functions out of retail order` |
| counts | `tools/report_diff.py` | `matched 9437 -> 9454`, `linked 4773 -> 4790`, `+17 functions at 100%, 1 units newly linked`, **no WORSE / GONE / UNLINKED / FELL**; module's own count **7 -> 24 of 256** |
| port link | `tools/gate.sh` | `port probe ok`, `port link gap ok`, `unique undefined symbols 254`, `unchanged from baseline (254 undefined, 0 duplicates)` |
| names | `python3 tools/check_symbol_names.py` | `checked 484 units; 0 declared names are missing` |
| raw offsets | `python3 tools/check_raw_offsets.py` | `ok: 151 raw-offset site(s) in 60 file(s)` after the section was added |
| gate | `./tools/gate.sh` | every step `ok` except `docs claims` - see below |

`flip_test.sh` prints `FAIL -> reverted` for this unit, and that is **expected and is not a
regression**: the tool only understands DOL units (it looks for
`extern/musyx/src/MetroidPrime/ScriptObjects/...` and reports
`no source file (...) - configure.py would link the retail object and this would pass while
proving nothing`). For a REL unit the acceptance test named by `AGENTS.md` is the module sha1
plus the `cmp`, and both hold. The revert rebuilt the tree and the DOL sha1 was still
`6ef9b491...` afterwards. I re-ran `decomp_build.sh` and the report after it to be sure the tree
was not left altered: still 17/17, `81c9c6e8...`, `6ef9b491...`.

### `docs claims` is the one red step, and the judge rewrites it

`gate.sh` without `MP_GATE_DOCS_WRITE=1` reports four missing derived claims in `docs/HANDOFF.md`
(`matched 9454`, `linked 4790`, `REL units 1383`, `**82 units of our own code in 65 modules**`
plus `CommandoPirate` in the module list). I ran `MP_GATE_DOCS_WRITE=1 python3
tools/check_docs_claims.py --write`, which made all four correct and the check pass, then
**reverted `docs/HANDOFF.md`** - `goal_check.sh` runs the gate with `MP_GATE_DOCS_WRITE=1` and
`goal-unit-prompt.md` says the driver discards edits to that file. So the diff I leave has no
`HANDOFF.md` change and the judge supplies the counts itself.

## The ranges I claimed

Only `.text 0x0..0x168`, the contiguous run the object reproduces - nothing else:

```
fn_9_0 0x00  fn_9_8 0x08  fn_9_10 0x10  fn_9_1C 0x1C  fn_9_58 0x58  fn_9_68 0x68
fn_9_70 0x70  fn_9_78 0x78  fn_9_80 0x80  fn_9_90 0x90  fn_9_9C 0x9C  fn_9_A4 0xA4
fn_9_AC 0xAC  fn_9_C8 0xC8  RELExit 0xF4  RELMain 0x118  fn_9_138 0x138
```

`fn_9_168` (0x168, 0x76C) and the 137 functions above it stay with dtk - the module's entity
loader and its members, which need the CActor/CPatterned/CAi hierarchy this tree does not model.
`audit_rel_claim.py` confirms 0 filled gaps, so the 17 are genuinely ours and not dtk's retail
bytes counted into the claim.

## The three things that were not what the family usually has

The FN_XX_10 hint in `item.json` was right that this module has the `GetBoundingBox` wrapper
near 0x0, and right that it is *not* an `optional_object<CAABox>` template problem. The spelling
is `CMysteryFlyerRel.cpp`'s `fn_45_10` verbatim, and the module's own out-of-line converting
constructor is **`fn_9_F9BC`** at 0xF9BC (six word copies out of `r4+0x00..r4+0x14`, then
`stb 1, 0x18(r3)` - six words of `CAABox` and a validity byte at +0x18). `const CAABox&` is
load-bearing, as the hint says. The one-method local `CPhysicsActor` stand-in was used and
`MetroidPrime/CPhysicsActor.hpp` was **not** included; `MetroidPrime/TGameTypes.hpp` is included
for `kInvalidUniqueId`.

**The block is in `CIngSpaceJumpGuardianRel.cpp`'s order, not `CMysteryFlyerRel.cpp`'s**, and I
confirmed that by reading the bytes rather than by pattern-matching the family:

- it opens with **two** leading accessors - `fn_9_0` is `addi r3, r3, 0x928` and `fn_9_8` is
  `li r3, 1` - and module 34 opens the same way;
- `fn_9_10` is a **module-local `.rodata` constant**, `.rodata:0x400`, `.float 50`, where the
  family puts the wrapper. The spelling is `CScriptRubiksPuzzle.cpp`'s: a `float` return of an
  `extern "C" const float`, with the split claiming `.text` only so dtk's `.rodata` object keeps
  defining `lbl_9_rodata_400` and the reference is an ordinary cross-object relocation;
- the `lbl_8041B758` accessor the family carries at 0x90 **is not here at all** - `fn_9_90` is
  the +0x34c bit-3 flag test instead. That is the one function whose *identity* differs from
  module 34's, and its body is the same `& 8` test.

**The loader record is 8 bytes, not four.** `lbl_9_bss_28` is `.bss:0x28, size:0x8`, and it
mirrors the DOL's `gLoader_CommandPirate`, which `src/MetroidPrime/ScriptLoader/CommandPirate.cpp`
already spells as `SLoaderSlot { FScriptLoader* value; unsigned int padding; }`. Only the first
word is written by `fn_9_138`. Unlike FishCloud there is **no** type for it in
`include/MetroidPrime/ScriptLoaderRel.hpp`, so it is spelled out locally
(`SCommandPirateLoaderSlot`) rather than taken from a header that does not describe it. Declaring
it as a bare `FScriptLoader` would have been the shorter spelling and would have compiled, but it
would have misdescribed an 8-byte retail object.

**The setter is the plain DOL symbol `fn_802188B0`** (0x802188B0, 8 bytes, `stw r3,
gLoader_CommandPirate@sda21(r0); blr`, in `build/G2ME01/asm/auto_03_802188B0_text.s`,
`symbols.txt:9463`), immediately after `LoadCommandPirate` at 0x80218884. It is deliberately
unclaimed in the DOL - `CommandPirate.cpp` records that REL modules import it by its retail name
- so it is `extern "C"` and not a mangled `SetLoader_*`. `CheckSymbolNames` confirms 0 missing.

## What is left

`fn_9_168` (0x168, 0x76C) is the module's entity loader and 137 more functions are its members.
That is class code needing the CActor/CPatterned/CAi hierarchy, which is the standing blocker for
this family - it is not this item's to solve, and no spelling was tried against it.

No `NEW:` line. Nothing new is blocked: the head is done, the module hash holds, and the next step
for this module is the same modelling job every other scripted-actor module is waiting on, which
is a lesson rather than a queue item.

`docs/RUNNING_THE_DECOMP.md`'s "Attempted modules" table was **not** edited: `goal-unit-prompt.md`
says the driver discards edits to that file before judging, and the table rows there are the
orchestrator's to write. The row's content, if it wants one, is the table above plus
`81c9c6e8c24e82d585506f5c5faf965ede214b24` unchanged, `7 -> 24 of 256` for the module.

---

# Run 2 (lane 8, 2026-10-02) - re-measured and landed here

**STALE for the tree this ran in, DONE overall.** The run above ran in `../wt-mp2-goal` and its
change was never committed, so on `goal/lane-8` (`60ac016b`, which is `goal/decomp` at the time of
this run) `src/MetroidPrime/ScriptObjects/CCommandoPirateRel.cpp` did not exist,
`config/G2ME01/rels/CommandoPirate/splits.txt` had only the two shared units, and `configure.py`
had no `Rel("CommandoPirate", ...)`. Measured, not recalled: `ls` failed on the source,
`grep -n CommandoPirate configure.py files.cmake config/G2ME01/splits.txt` returned nothing, and
`build/report.json` gave the module **7 matched of 256** (2 `global_destructor_chain` + 5
`REL_Setup`, nothing of ours).

I therefore re-measured every claim in the run above on this tree and rebuilt it. **All of them held,
and none had to be discovered a second time** - the head is 17/17 at 100.00% and the module's sha1
is unchanged. This section is the record for *this* tree; the run above's recipe was the starting
point, and nothing below contradicts it except one number.

## What I changed

- `src/MetroidPrime/ScriptObjects/CCommandoPirateRel.cpp` - **new**, 258 lines, definitions in
  descending retail order `fn_9_138` (0x138) -> RELMain (0x118) -> RELExit (0xF4) -> `fn_9_C8`
  (0xC8) -> ... -> `fn_9_0` (0x0).
- `config/G2ME01/rels/CommandoPirate/splits.txt` - one claim added, **first**, so the file stays in
  ascending address order (the two existing entries are 0x10480 and 0x104F4):
  ```
  MetroidPrime/ScriptObjects/CCommandoPirateRel.cpp:
      .text       start:0x00000000 end:0x00000168
  ```
- `configure.py` - the `Rel("CommandoPirate", [Object(Matching, "MetroidPrime/ScriptObjects/
  CCommandoPirateRel.cpp")])` block, appended after `PirateRagDoll`, with the measured reasoning in
  comments (the block order, the `.rodata:0x400` constant, the out-of-line ctor, the eight-byte
  loader slot, the plain `fn_802188B0` import needing **no** `symbols.txt` rename and no DOL
  change, and the FORCEACTIVE/dead-strip measurement).
- `docs/research/raw_offsets.md` - the section `check_raw_offsets.py` requires for the new file.
- **No `files.cmake` entry**, measured, not assumed: `tools/check_files_cmake.py` exempts a REL unit
  that defines `RELMain`/`RELExit` (46 units are out for exactly that reason), and this file also
  calls `fn_9_168` and `fn_802188B0`, which the port cannot link. The checker reported
  "every configured DOL object is either in files.cmake or excluded with a reason".

## What I measured on this tree

| what | command | result |
| --- | --- | --- |
| the item before the change | `build/report.json` | module `CommandoPirate` **7 / 256** matched |
| unit | `build/report.json` after the build | `CommandoPirate/MetroidPrime/ScriptObjects/CCommandoPirateRel` **17/17 at 100.00%**, `matched_functions: 17`, `total_code 360/360`, `matched_functions_percent 100.0` |
| module hash | `sha1sum build/G2ME01/CommandoPirate/CommandoPirate.rel` | `81c9c6e8c24e82d585506f5c5faf965ede214b24`, **matches `config/G2ME01/config.yml:58`** |
| `.rel` identity | `cmp ... orig/G2ME01/files/RelProd/CommandoPirate.rel` | identical |
| DOL | `sha1sum build/G2ME01/main.dol` | `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` unchanged |
| claim | `python3 tools/audit_rel_claim.py CommandoPirate` | `CCommandoPirateRel.cpp 0x0..0x168 17/17 functions`; `global_destructor_chain 2/2`; `REL_Setup 5/5`; **`0 claim(s) with a problem`**; `preplf 256 text symbols, plf 256, 0 dropped by -strip_partial` |
| object fits | `./tools/unit_fit.sh MetroidPrime/ScriptObjects/CCommandoPirateRel.cpp` | `.text claimed 360 ours 360 retail 360 fits`; `no extra functions` |
| decl order | `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CCommandoPirateRel` | `ok: 1 unit(s) checked, none emits its functions out of retail order` |
| counts | `tools/report_diff.py` (base = this tree stashed and rebuilt, new = this change) | `matched 13050 -> 13067`, `linked 6152 -> 6169`, `+17 functions at 100%, 1 units newly linked`, **`no regression`**; `SPLIT ... 248 function(s) accounted for ... (exact count match - a split, not a loss)` |
| module's own count | `build/report.json` | **7 -> 24 of 256** |
| names | `python3 tools/check_symbol_names.py` | `checked 578 units; 0 declared names are missing from their object` |
| raw offsets | `python3 tools/check_raw_offsets.py` | failed with `1 sites, no section in raw_offsets.md`; after the section, **`ok: 171 raw-offset site(s) in 75 file(s)`** |
| manifest | `python3 tools/check_files_cmake.py` | `every configured DOL object is either in files.cmake or excluded with a reason` |
| the judge | `./tools/goal_check.sh build/goal/item.json` | **`PASS progress-rel-head-commandopirate`** - `gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)`, `target rose: module:CommandoPirate: 7 -> 24 / 256 functions`, `no asm added` |

`goal_check.sh` rewrites `docs/HANDOFF.md`'s derived counts as a side effect; I reverted that file
afterwards (`git checkout -- docs/HANDOFF.md`), so the diff I leave has no `HANDOFF.md` change and
the judge supplies the counts itself.

## One number in the run above was wrong, and this run measured it

Line 111 above says "137 more functions are its members". **It is 230, not 137.** Measured two
independent ways:

- `build/report.json` on this tree: `CommandoPirate/auto_00_00000168_text` is
  `total_functions: 231`, plus one in `auto_fn_9_F9F8_text`. `fn_9_168` is the first of the 231.
- `config/G2ME01/rels/CommandoPirate/symbols.txt`: of 256 `type:function` entries, **17 are below
  0x168**, 231 are in `[0x168, 0xF9F8)` and 8 are at or above `0xF9F8` (the tail unit reports only
  1, the rest are not `.text` functions dtk pairs).

So: 17 claimed by us, 24 matched in the module (17 + 2 + 5), **232 functions still retail**, of
which 231 sit above `0x168` and 1 is the tail. My source header, `configure.py` comment and
`raw_offsets.md` section all say 230 members above `fn_9_168`. Nothing about the code depended on
it, but the figure was quoted from memory in three files, which is exactly what AGENTS.md warns
about.

## Two other figures worth recording, because "thirteen accessors" was also wrong

The head is 17 functions: 3 of them are `RELExit`/`RELMain`/`fn_9_138`, so **14** sit above
`RELExit` - thirteen generated accessors plus `fn_9_C8`, the vtable entry. The module's own
`build/G2ME01/CommandoPirate/ldscript.lcf` FORCEACTIVE block (lines 20-33) lists **all fourteen**,
`fn_9_138` is a direct `bl` from RELMain, and RELMain/RELExit are reached from `_prolog`/`_epilog` by
the already-wired `REL_Setup` unit - so no `force_active:` entry in `config/G2ME01/config.yml` is
needed, and `audit_rel_claim.py`'s `0 dropped by -strip_partial` on 256 symbols confirms it after
the link rather than by argument.

`fn_9_C8`'s slot is 0x38 and the entry above it at 0x3C is `fn_9_C8`: counting `.4byte`s from
`.data:0x828` (0x148 bytes) gives index 14 = `HealthInfo__3CAiFv` at 0x38 and index 15 = `fn_9_C8`
at 0x3C. Two leading words then 13 virtuals puts the thirteenth at 0x38, which is why the
stand-in class has 13. Identical to `CIngSpaceJumpGuardianRel.cpp`'s layout, so no new virtual
counting was needed.

## What the notes above did not list, and what I found

There is very little: the run above's recipe covered the whole head. What I added by reading rather
than by pattern-matching:

- the vtable byte-count above (0x148, two leading words) - not in the run above, which asserted the
  layout by analogy; it is what makes the thirteen-virtual stand-in justified here rather than
  inherited.
- the loader slot's **width**: `.bss` `lbl_9_bss_28` is `size:0x8` and the module's whole `.bss` is 0x3C (`auto_05_00000000_bss.s`, `0x0..0x3C`),
  confirmed in `build/G2ME01/CommandoPirate/asm/auto_05_00000000_bss.s` (which also shows why
  `lbl_9_bss_0` at 0x0 is **not** the slot, unlike several modules).
- `lbl_9_rodata_400`'s value: `.float 50`, from `auto_03_00000000_rodata.s` line 284. The run above
  recorded the offset but not the value.
- the `SCommandPirateLoaderSlot` definition compiles and produces retail's bytes with the plain
  `.value = fn_9_168;` spelling - a whole-struct assignment would not compile at all, and a bare
  `FScriptLoader lbl_9_bss_28` would compile but describe a four-byte retail object as four bytes.

Everything else the run above listed was reproduced verbatim: `fn_9_1C` is
`fn_9_F9BC(out, self->GetBoundingBox())` with the out-of-line converting constructor left
unclaimed; `fn_9_10` is a `float` return of an `extern "C" const float` and the split claims
`.text` only so dtk's `.rodata` keeps defining `lbl_9_rodata_400`; `fn_9_90` is `& 8` on the byte at
`+0x34c` (dtk's `extrwi r3, r0, 1, 28` is opcode 21 read as a rotate, not bit 28); `fn_9_AC` is
three subscript stores, not a `CVector3f` copy; `fn_9_80` is `*id = kInvalidUniqueId` on a
`TUniqueId*`; and `fn_802188B0` is a plain `extern "C"` DOL symbol.

**I did not attempt `fn_9_F9BC`** (0xF9BC, 0x3C), the out-of-line
`optional_object<CAABox>(const CAABox&)`. It is a second, discontiguous claim and so a second
unit, and its body interleaves the loads two words ahead of the stores (`li r0,1 / lwz r5,0(r4) /
stb r0,0x18(r3) / lwz r0,4(r4) / stw r5,0(r3) / ...`), which is what an inlined
`rstl::optional_object<CAABox>` return produces elsewhere (`CLumiteRel.cpp` records the same for
`fn_39_0`). That is a separate item, not this head, and spelling it by hand would be transcription
rather than decompilation. It is **one function**, so its payoff is small too.

## What is left

`fn_9_168` (0x168, 0x76C) is the module's entity loader and **230** functions above it are its
members - 231 in all above `0x168`, plus the one in `auto_fn_9_F9F8_text`. That is class code
needing the CActor/CPatterned/CAi hierarchy, the standing blocker for this family. No spelling was
tried against it in this run either.

No `NEW:` line: nothing new is blocked. The head is done and the hash holds, and the next step for
module 9 is the same modelling job every other scripted-actor module is waiting on - a lesson, not
a queue item. (`fn_9_F9BC` is the only candidate a lane could pick up, and at one function it does
not clear the bar for a `NEW:` line.)

`docs/RUNNING_THE_DECOMP.md`'s "Attempted modules" table was **not** edited, again: the prompt says
the driver discards edits to that file before judging. The row's content, if the orchestrator wants
one, is the measurement table above plus `81c9c6e8c24e82d585506f5c5faf965ede214b24` unchanged and
`7 -> 24 of 256` for the module.
