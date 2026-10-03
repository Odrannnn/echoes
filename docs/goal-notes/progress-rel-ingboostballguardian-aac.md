# progress-rel-ingboostballguardian-aac

`kind: match`, `target: IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardianAAC`.
**Module 30 matched_functions 63 -> 64 (of 318)**, the new unit `Matching` at 1/1 / 100.00% fuzzy,
100.00% matched code, `matched_code 40 / 40`, `complete_code 40`, `complete_units 1` (`fn_30_AAC`,
`virtual_address 2732` = 0xAAC), `All:` **13466 -> 13467 matched** and **6514 -> 6515 linked**,
`All: 37.54% fuzzy, 30.97% matched, 13.84% linked (13467 / 28465 functions)`, DOL
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, `IngBoostBallGuardian.rel`
`956265e8ccf3f489e9cb3a70ec22d0357ce17cca` matching `config/G2ME01/config.yml` and
(unchanged) `config/G2ME01/build.sha1:31`, and `cmp`-equal to
`orig/G2ME01/files/RelProd/IngBoostBallGuardian.rel` (`cmp` -> no output, IDENTICAL).
`tools/unit_fit.sh` -> `.text claimed 40 ours 40 retail 40 fits`, **no extra functions**.
`python3 tools/check_decl_order.py --unit ...AAC` -> `ok: 0 unit(s) checked` (one function, so
there is no emission order to get wrong).
`./tools/flip_test.sh IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardianAAC.cpp`
-> **PASS** (kept as Matching) and `./tools/goal_check.sh build/goal/item.json` -> **PASS**.
`check_symbol_names.py` -> `checked 585 units; 0 declared names are missing from their object`.
Port: probe 914 files (the count `tools/gate.sh` wrote into `docs/HANDOFF.md`), 286 undefined, 0
duplicates, 0 compile errors.

Base figures are the judge's `build/goal/judge/report.base.json`, not the previous item's note.

## What I did

Claimed `.text 0xAAC..0xAD4` of module 30 as a new unit: `fn_30_AAC`, 0x28 = 40 bytes. All four
parts of the carve in one change:

- `src/MetroidPrime/ScriptObjects/CIngBoostBallGuardianAAC.cpp` (new, one function)
- `config/G2ME01/rels/IngBoostBallGuardian/splits.txt` - one `.text` entry, placed in address order
  between the 0xA91C and the 0xAD4 entries
- `configure.py` - one `Object(Matching, ..., source=..., mw_version="GC/2.7")` on one line, right
  after the AD4 entry, with the long comment the surrounding entries carry
- `files.cmake` - the source path, with an empty host branch

`docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` show as modified in `git status`, but I did not
touch them: `tools/gate.sh` (inside `goal_check.sh`) rewrites the derived counts from the tree and
does so whether or not I revert them - I reverted them once, re-ran `goal_check.sh`, got PASS, and
the same two files came back modified. Nothing else is in the diff.

## What the function is, measured

It is the null-guarded forwarding call in front of `fn_30_AD4`, and the whole body is ten
instructions, read off `build/G2ME01/IngBoostBallGuardian/asm/auto_00_00000000_text.s:822-834`:

```
0xAAC stwu r1,-0x10(r1) / mflr r0 / cmplwi r3,0x0 / stw r0,0x14(r1) / beq +8
0xAC0 bl fn_30_F78
0xAC4 lwz r0,0x14(r1) / mtlr r0 / addi r1,r1,0x10 / blr
```

- **The test is on r3 and the callee gets r3 and r4 untouched** - no `addi`, no `mr` - so
  `if (self) fn_30_F78(self, other);` is the entire function. There is no address arithmetic and
  no return value: the epilogue never touches r3 after the `bl`, and `fn_30_F78`'s last
  instruction before its own `blr` is an `stfs`, so the return type is `void`.
- **`fn_30_A8C` (0xA8C, 0x20) forwards just as blindly** - `stwu` / `mflr` / `bl fn_30_AAC` /
  `lwz` / `mtlr` / `addi` / `blr`, no argument setup at all - so the two-argument signature is
  measured from `fn_30_F78`'s own body, a 0x38-byte member-wise copy: `lfs`/`stfs` at +0x00, +0x04,
  +0x08, +0x0C, +0x10, +0x14, `lwz`/`stw` pairs at +0x18 .. +0x2C, then `lfs`/`stfs` at +0x30 and
  +0x34. That is `RelRecord38` in the source. Nothing in this unit dereferences it.
- **The spelling was byte-exact on the first build**: objdiff reports 100.00% fuzzy / 40 of 40
  bytes, and `powerpc-eabi-objdump -d` on
  `build/G2ME01/src/IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardianAAC.o`
  gives the ten instructions above with one `R_PPC_REL24 fn_30_F78` at object offset 0x14.

## The carve's four checks

- **The claim spans no unclaimed gap.** `fn_30_A8C` is 0x20 bytes and ends exactly at 0xAAC
  (`config/G2ME01/rels/IngBoostBallGuardian/symbols.txt:25`), and `fn_30_AD4` starts at 0xAD4, which
  is the entry above. `total_functions` stays 28465.
- **The callee still resolves.** `fn_30_F78` is claimed by no unit, and this carve starts no new
  auto unit at 0xF78, so after the split `nm` still finds it as `T` at object offset 0x448 of
  `build/G2ME01/IngBoostBallGuardian/obj/auto_00_00000B30_text.o`, an object the module links.
  Measured before the carve and after: `auto_00_00000B30_text.o` is the only auto object defining
  it, at 0x448 both times.
- **No dead-strip hazard.** `fn_30_AAC` is not in
  `build/G2ME01/IngBoostBallGuardian/ldscript.lcf`'s FORCEACTIVE list, but
  `powerpc-eabi-objdump -r` finds an `R_PPC_REL24 fn_30_AAC` in `auto_00_00000000_text.o` and in
  `auto_00_00000130_text.o` (the call at 0xA98, inside `fn_30_A8C`) - measured after the carve
  too. `auto_00_00000130_text.o` is the auto unit this carve splits in two, so it is in the module's
  link and the reference survives. No `force_active:` entry, no `config/G2ME01/config.yml` change,
  no `symbols.txt` rename; the split claims `.text` only.
- **The gate saw a split, not a loss.** `tools/gate.sh`'s per-function diff prints `SPLIT
  IngBoostBallGuardian/auto_00_00000130_text: 1 function(s) moved into
  IngBoostBallGuardian/IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardianAAC
  (exact count match - a split, not a loss)`, and `build/report.json` shows that auto unit going
  9 -> 8 functions while the new unit contributes 1, so module 30's total stays 318.

## One stale artefact worth recording

`build/G2ME01/IngBoostBallGuardian/obj/IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardianF78.o`
is left over from an earlier attempt and **is byte-identical to retail's `fn_30_F78`** (measured:
`objcopy -O binary --only-section=.text` of that object against the 0x74 bytes at object offset
0x448 of `auto_00_00000B30_text.o`, `cmp` -> BYTE-IDENTICAL 116/116). There is no `configure.py`
entry for it and no source in the tree, so nothing links it. I did not touch it - claiming
`fn_30_F78` is a separate unit of work, filed below.

NEW: progress-rel-ingboostballguardian-f78 | match | IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardianF78 | fn_30_F78 (.text 0xF78..0xFEC, 0x74 bytes, symbols.txt:33) is unclaimed and an earlier attempt's stale build/G2ME01/IngBoostBallGuardian/obj/.../CIngBoostBallGuardianF78.o is already byte-identical to it (cmp, 116/116), so the source is a member-wise assignment of the RelRecord38 layout this item's source comment records - six floats, six words, two floats - with no callee and no dead-strip hazard (auto_00_00000130/0001466C call it); the claim spans no gap since fn_30_F30 is 0x48 bytes and ends exactly at 0xF78