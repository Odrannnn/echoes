# progress-rel-head-darkcommando

**STATUS: DONE.** Module 3 (DarkCommando)'s head, `.text 0x0..0x19C`, 18 functions,
`Matching`, 18/18 at 100.00%, module sha1 unchanged.

## What I did

**A previous attempt on this item left a complete, uncommitted change in this worktree** (its
agent run at 19:28:13Z "exited 1 after 201s" and the driver reported "resetting the worktree",
but the reset did not remove the files). I did not throw it away: I read it against the
disassembly, verified every claim in it, built it, measured it, corrected one inaccurate
doc claim in it, and it is the change I am reporting.

The change is four files:

- **`src/MetroidPrime/ScriptObjects/CDarkCommandoRel.cpp`** (new, 258 lines) - the module head
  in C++: eighteen functions from `fn_3_0` to `fn_3_16C`, in **descending** retail text order.
- **`config/G2ME01/rels/DarkCommando/splits.txt`** - the one claim
  `MetroidPrime/ScriptObjects/CDarkCommandoRel.cpp: .text start:0x00000000 end:0x0000019C`.
- **`configure.py`** - the `Rel("DarkCommando", [Object(Matching, ...CDarkCommandoRel.cpp)])`
  block, with the measured differences from the family recorded in its comment.
- **`docs/research/raw_offsets.md`** - the section the gate's `check_raw_offsets.py` requires
  for a new file with raw member offsets.

**Not** in `files.cmake`, for the reason every other landed REL head records: the file calls
`fn_3_19C` and `fn_80235E00`, which the port cannot link, and a flat host link cannot hold
`RELMain`/`RELExit`. `tools/check_files_cmake.py` agrees (`0 on-disk sources are in no manifest
at all (dead)`; the 31 units it excludes for defining a module entry point are a known gap).
**No assembly.** **`No `asm` text anywhere in the diff`** (the judge greps added `src/`/`include/`
lines for `\basm\b|__asm` after stripping comments; a header comment citing
`build/G2ME01/DarkCommando/asm/...` is stripped as a comment, not counted as assembly).

## What I measured

Everything below was re-measured in this run; nothing is quoted from the earlier attempt.

| what | command | result |
| --- | --- | --- |
| build | `./tools/decomp_build.sh` | exit 0; `All:  29.09% fuzzy, 21.23% matched, 11.37% linked (9459 / 28465 functions)` |
| unit | `build/report.json` | `DarkCommando/MetroidPrime/ScriptObjects/CDarkCommandoRel` **18/18**, `metadata.complete: True` |
| module count | `tools/report_diff.py build/goal/judge/report.base.json build/report.json` | `matched 9441 -> 9459  linked 4777 -> 4795  (+18 functions at 100%, 1 units newly linked)`, **no regression**; module sum **7 -> 25 / 193** |
| module hash | `sha1sum build/G2ME01/DarkCommando/DarkCommando.rel` | `d9c41deb88a9f9cbe7b0230c3aa1b1b0f822fd3e`, **matches `config/G2ME01/config.yml`** |
| `.rel` identity | `cmp build/G2ME01/DarkCommando/DarkCommando.rel orig/G2ME01/files/RelProd/DarkCommando.rel` | clean |
| all 86 RELs | the gate's independent re-hash | `mismatched: none` |
| DOL | `sha1sum build/G2ME01/main.dol` | `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` unchanged |
| no filled gaps | `python3 tools/audit_rel_claim.py DarkCommando` | `CDarkCommandoRel.cpp 0x00000000..0x0000019C 18/18 functions`, `global_destructor_chain.c 2/2`, `REL/REL_Setup.cpp 5/5`, **`0 claim(s) with a problem`**, `193 text symbols, 0 dropped by -strip_partial` |
| object fits | `./tools/unit_fit.sh MetroidPrime/ScriptObjects/CDarkCommandoRel.cpp` | `.text claimed 412 ours 412 retail 412 fits`; `no extra functions` |
| decl order | `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CDarkCommandoRel` | `ok: 1 unit(s) checked, none emits its functions out of retail order` |
| names | `python3 tools/check_symbol_names.py` | `checked 484 units; 0 declared names are missing from their object` |
| raw offsets | `python3 tools/check_raw_offsets.py` | `ok: 151 raw-offset site(s) in 60 file(s)`; `--list` shows **1** site in this file, line 208 `+0x54`, which is what the new section says |
| docs claims | `python3 tools/check_docs_claims.py` | `docs claims agree with the tree` |
| wiring / files.cmake / module order | the three tools | all `ok`; `83 unit(s) of our own code in 65 module(s)`, DarkCommando in the list |
| **the whole gate** | `MP_GATE_DOCS_WRITE=1 ./tools/gate.sh build/goal/judge/report.base.json` | **GATE PASS `efcdbce+5 changed`** - every step `ok`, including `ninja + build.sha1`, `hashes vs config.yml`, `port probe`, `port link gap`, `reach stubs` |

`docs/HANDOFF.md` is **not** in my diff: I reverted it and confirmed the judge's own
`MP_GATE_DOCS_WRITE=1` rewrites it (`--write` re-applied the three count changes and the module
list, then reported `docs claims agree with the tree`).

`tools/flip_test.sh` is not the acceptance test for a REL unit - it only understands DOL units
and prints `no source file (...) - configure.py would link the retail object and this would pass
while proving nothing`. For a REL module the acceptance test named by `AGENTS.md` is the module
sha1 plus the `cmp`, and both hold with this unit's own object in the link.

## The ranges I claimed

Only `.text 0x0..0x19C`, the contiguous run the object reproduces. Nothing else:

```
fn_3_0   0x000  fn_3_8   0x008  fn_3_14  0x014  fn_3_7C  0x07C  fn_3_8C  0x08C
fn_3_94  0x094  fn_3_9C  0x09C  fn_3_A4  0x0A4  fn_3_AC  0x0AC  fn_3_BC  0x0BC
fn_3_C8  0x0C8  fn_3_D0  0x0D0  fn_3_D8  0x0D8  fn_3_E0  0x0E0  fn_3_FC  0x0FC
RELExit  0x128  RELMain  0x14C  fn_3_16C 0x16C
```

`fn_3_19C` (0x19C, 0x33C) is the module's own entity loader and the 167 functions above it are
its methods. They stay retail: behavioural class code that needs the CActor/CPatterned/CAi
hierarchy this tree does not model, which is the standing blocker for this whole family.
`audit_rel_claim.py` reports 0 problems, so the 18 are genuinely ours, not dtk's retail bytes.

## The three things that are not what the family usually has

I diffed `build/G2ME01/DarkCommando/asm/auto_00_00000000_text.s` against the landed heads
before writing anything, and **Lumite's `fn_39_0` is byte-identical to `fn_3_14`** (and Lumite's
`fn_39_68/78/80/88/A8/B4/BC/C4/CC/D4/F0` to `fn_3_7C/8C/94/9C/A4/AC/BC/C8/D0/D8/E0/FC`).
Module 39's head is the cleanest reference for this one.

1. **`fn_3_14` is 0x68 bytes with the conversion INLINED**, so it is not MysteryFlyer's
   `fn_45_10`. The `item.json` hint is right that this is not an `optional_object` template
   problem, but the spelling to copy is not MysteryFlyer's: `CMysteryFlyerRel.cpp`'s `fn_45_10`,
   `CTryclopsRel.cpp`'s `fn_81_10` and `CIngSpaceJumpGuardianRel.cpp`'s `fn_34_1C` are each
   0x3C bytes (verified in their `symbols.txt`) and end in `bl <module>_ctor`. Here the flag
   store and the six-word `CAABox` copy are in the body, flag **first**:
   `li r0,1 ; stb r0,0x18(r31)` then six `lwz`/`stw` pairs. The spelling that reproduces it is
   the real return type - `rstl::optional_object<CAABox> fn_3_14(const CPhysicsActor* self)
   { return self->GetBoundingBox(); }` - because `optional_object(const T&)` sets `m_valid` in
   its mem-init and then placement-constructs, and `CAABox` carries
   `RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE` (`include/Kyoto/Math/CAABox.hpp:116`) so the
   construction is a word-wise copy, not a call. `CAABox GetBoundingBox() const` on the
   one-method local `class CPhysicsActor` stand-in is load-bearing for the frame: `this` in r4,
   the same register `self` arrives in, and the box returned through r1+0x8 - the 0x30 frame and
   the 0x34 saved-LR slot retail has. **`MetroidPrime/CPhysicsActor.hpp` is deliberately not
   included**: it reaches `CMaterialList.hpp`, whose file-scope statics put 0x28 bytes of
   `.data` in this object and break the module hash with every function at 100%.

2. **`fn_3_8` is this module's own `.rodata:0x0`, `.float 50`**, and there is **no
   `lbl_8041B758` accessor anywhere in the block** - every other module head has one at 0x90.
   The spelling is a `float` return of an `extern "C" const float`, the same one
   `CIngSpaceJumpGuardianRel.cpp` uses for its `lbl_34_rodata_0` and `CScriptRubiksPuzzle.cpp`
   for its `lbl_4_rodata_0`. **The split claims `.text` only**, so `lbl_3_rodata_0` stays defined
   in dtk's `.rodata` object and the reference is an ordinary cross-object relocation - no
   `.rodata` claim is needed and none was made.

3. **`fn_3_A4` is `li r3,0` (false), not `li r3,1`.** The family alternates true and false, so
   the third predicate reads as `true`; the disc image and the dtk disassembly both say
   `38 60 00 00`. Writing it as `true` costs exactly one byte of the module hash and holds
   `fn_3_A4` at 99.50% while the other 17 are at 100.00%. The block also runs **three**
   `li r3,0` predicates, not two, which is what pushes `kInvalidUniqueId` from 0x74 to 0xAC.

## The two callees, named by what they are

- **`fn_80235E00`** is the DOL's 0x80235E00, `size:0x8`, and
  `build/G2ME01/asm/*80235E00*` prints it as `stw r3, gLoader_DarkCommando@sda21(r0) ; blr`;
  `config/G2ME01/symbols.txt:10047` names it and line 20823 names the slot
  `gLoader_DarkCommando = .sbss:0x80419660`. So it stores the *address* of a loader slot, which
  is why the registration hands it `&lbl_3_bss_1C`. It is `extern "C"`, **not** a mangled
  `SetLoader_*` name: the slot is `.bss:0x1C, size:0x4` (four bytes, one loader) and the symbol
  is the plain unmangled DOL symbol. It is deliberately unclaimed in the DOL -
  `src/MetroidPrime/ScriptLoader/DarkCommando.cpp` records that REL modules import it by its
  retail name - so it stays in dtk's auto unit. `check_symbol_names.py` reports 0 missing.
- **`lbl_3_bss_1C`** is `.bss:0x1C`, **not `.bss:0x0` as in MysteryFlyer**, because this
  module's `.bss` holds three objects (`lbl_3_bss_0` 0xC at 0x0, `lbl_3_bss_C` 0x10 at 0xC,
  `lbl_3_bss_1C` 0x4 at 0x1C in `auto_05_00000000_bss.s`). The unit's split claims `.text`
  only, so dtk's `.bss` object defines it and a second definition under MWCC is what produced
  mwldeppc's internal linker error on ScriptPlayerProxy - hence `extern` under `__MWERKS__` and
  a host definition. The only reader is `LoadDarkCommando` in the `Matching` DOL unit, so there
  is no second reader and no pmf.

**No dead-strip hazard**: the module's `ldscript.lcf` `FORCEACTIVE` block lists all fifteen of
`fn_3_0`..`fn_3_FC`, and `.data` stores every one of them, so nothing needs a `force_active:`
entry. (I corrected the previous attempt's `configure.py` comment here: it attributed the
storage of all fifteen to `.data:0x364`, which is the class vtable - 0x148 bytes, holding
`fn_3_FC` and `fn_3_14` - while the other thirteen are in the generator's one-entry vtable
records. Verified: each of the fifteen appears as a `.4byte` in `auto_04_00000000_data.s`,
`fn_3_FC` in three of them.)

## What is left, and what I did not file

`fn_3_19C` (0x19C, 0x33C) and the 167 class functions above it. That is the CActor/CPatterned/
CAi hierarchy, the same blocker every scripted-actor module in this family is waiting on. It is
a modelling job, not a spelling, and no spelling was tried against it.

**No `NEW:` line.** The head is done and the module hash holds; the next step for this module is
the same hierarchy every other one needs, which is a lesson rather than a queue item.

**`docs/RUNNING_THE_DECOMP.md`'s "Attempted modules" table was not edited** -
`goal-unit-prompt.md` says the driver discards edits to that file before judging. The row's
content, if the orchestrator wants one: DarkCommando, head `.text 0x0..0x19C`, 18 functions,
`Matching` 18/18, `d9c41deb88a9f9cbe7b0230c3aa1b1b0f822fd3e` unchanged, module count 7 -> 25 of
193, the three measured deviations above.

## One thing the next lane should know

**`progress-rel-head-commandopirate` in this lane failed the judge on `gate.sh` alone** even
though its own agent reported every step `ok`, and its 17 functions all counted
(`matched 9437 -> 9454`, module 7 -> 24). Its `check-gate.log` has been overwritten, so the
failing step is not recoverable from this worktree, and the whole gate passes on the tree I am
leaving. Two candidates I could not distinguish, recorded so it is not re-derived: a transient
in the gate's own `link_check.sh`/port-build contention (`build/goal/judge/record-link.log`
shows `another link_check is already building ... waiting for its result`, and `gate.sh` starts
one in the background while a second may already be running), or the `docs claims` step when
`docs/HANDOFF.md` is *not* already written - `goal_check.sh` does pass `MP_GATE_DOCS_WRITE=1`,
and I verified that `--write` alone re-derives the three counts and the module list and then
reports `docs claims agree with the tree`, so that path self-heals. A re-run of the same change
on a quiet tree is the way to settle it.
