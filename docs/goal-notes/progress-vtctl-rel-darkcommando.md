# progress-vtctl-rel-darkcommando

**STATUS: DONE. Module 3 (DarkCommando) matched_functions 25 -> 33** (+8), project total
13382 -> 13390 and linked 6430 -> 6438, module sha1 unchanged and `cmp`-identical to the disc,
`All:` unchanged at 37.45% fuzzy / 30.88% matched. Five new `Matching` units, 8 functions, all
8/8 at 100.00%. **`tools/goal_check.sh build/goal/item.json` -> `goal_check: PASS`.**

The item was filed as a control trial of carving vtable-adjacent class code out of a module that
already has its head decompiled. It works, and the shape that works is the recipe's: one
contiguous range per unit, one file per unit, a claim only where our object reproduces the bytes
exactly, everything else left unclaimed so dtk fills it from retail.

## The eight functions, and the five ranges I claimed

All five claims are in `config/G2ME01/rels/DarkCommando/splits.txt`, in ascending `.text` order,
each with its own `Object(Matching, ...)` line in `configure.py`'s `Rel("DarkCommando", [...])`
block and its own source file. **Nothing else is claimed**, so the bytes around each range are
still retail and the module still hashes.

| range | bytes | functions | file |
| --- | --- | --- | --- |
| `0x000091F0..0x00009218` | 0x28 | `fn_3_91F0` (8), `fn_3_91F8` (0x20) | `CDarkCommandoRender.cpp` |
| `0x00006250..0x00006258` | 0x08 | `fn_3_6250` | `CDarkCommandoMemberPtr.cpp` |
| `0x0000648C..0x000064AC` | 0x20 | `fn_3_648C` | `CDarkCommandoRenderers.cpp` |
| `0x00009864..0x00009890` | 0x2C | `fn_3_9864` (0xC), `fn_3_9870` (0x20) | `CDarkCommandoVecList.cpp` |
| `0x00009970..0x000099A8` | 0x38 | `fn_3_9970`, `fn_3_998C` (0x1C each) | `CDarkCommandoVecCopy.cpp` |

`python3 tools/audit_rel_claim.py DarkCommando`:

```
ok   MetroidPrime/ScriptObjects/CDarkCommandoRel.cpp      0x00000000..0x0000019C  18/18 functions
ok   MetroidPrime/ScriptObjects/CDarkCommandoMemberPtr.cpp 0x00006250..0x00006258  1/1 functions
ok   MetroidPrime/ScriptObjects/CDarkCommandoRenderers.cpp 0x0000648C..0x000064AC  1/1 functions
ok   MetroidPrime/ScriptObjects/CDarkCommandoRender.cpp   0x000091F0..0x00009218  2/2 functions
ok   MetroidPrime/ScriptObjects/CDarkCommandoVecList.cpp  0x00009864..0x00009890  2/2 functions
ok   MetroidPrime/ScriptObjects/CDarkCommandoVecCopy.cpp   0x00009970..0x000099A8  2/2 functions
ok   REL/global_destructor_chain.c                        0x00009ED4..0x00009F48  2/2 functions
ok   REL/REL_Setup.cpp                                    0x00009F48..0x0000A0EC  5/5 functions

0 claim(s) with a problem
DarkCommando: preplf 193 text symbols, plf 193, 0 dropped by -strip_partial
```

## What I measured (every number re-measured in this run)

| what | command | result |
| --- | --- | --- |
| **the judge** | `./tools/goal_check.sh build/goal/item.json` | **`goal_check: PASS progress-vtctl-rel-darkcommando`**; `target rose: module:DarkCommando: 25 -> 33 / 193 functions`; `counts: matched 13382 -> 13390 linked 6430 -> 6438`; `no asm added` |
| **the whole gate** | inside `goal_check.sh` (`MP_GATE_DOCS_WRITE=1 ./tools/gate.sh build/goal/judge/report.base.json`) | **`GATE PASS 30fb7d7b+10 changed`**, every step `ok`: configure, `ninja + build.sha1`, hashes vs config.yml, report, per-function diff, module wiring, dol_read, docs claims, gs offsets, raw offsets, decl order, files.cmake, module order, port probe, port link gap, reach stubs |
| build | `./tools/decomp_build.sh` | `All:  37.45% fuzzy, 30.88% matched, 13.75% linked (13390 / 28465 functions)` - the `All:` line is unchanged from the baseline |
| module sum | `build/report.json` | **25 -> 33**, out of 193 functions |
| module sha1 | `sha1sum build/G2ME01/DarkCommando/DarkCommando.rel` | `d9c41deb88a9f9cbe7b0230c3aa1b1b0f822fd3e`, equals `config/G2ME01/config.yml` |
| `.rel` identity | `cmp build/G2ME01/DarkCommando/DarkCommando.rel orig/G2ME01/files/RelProd/DarkCommando.rel` | clean, exit 0 |
| all 86 RELs | the gate's own re-hash | `hashes vs config.yml ok` |
| DOL | `sha1sum build/G2ME01/main.dol` | `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, unchanged |
| per-unit scores | `tools/fast_try.sh` + `build/report.json` | `CDarkCommandoRender 2/2`, `CDarkCommandoVecList 2/2`, `CDarkCommandoVecCopy 2/2`, `CDarkCommandoMemberPtr 1/1`, `CDarkCommandoRenderers 1/1` - **every one 100.00% fuzzy and 100.00% matched code, `metadata.complete: true`** |
| object fits | `./tools/unit_fit.sh` on all five | `.text fits` on every one (40/44/56/8/32 bytes claimed = ours = retail); `no extra functions` |
| decl order | `python3 tools/check_decl_order.py` | `ok: 1149 unit(s) checked, 37 permuted, all 37 accounted for in decl_order.md` |
| names | `python3 tools/check_symbol_names.py` | `checked 585 units; 0 declared names are missing from their object` |
| raw offsets | `python3 tools/check_raw_offsets.py` | `ok: 191 raw-offset site(s) in 82 file(s), all documented in raw_offsets.md` - **my five files add none** |
| files.cmake | `python3 tools/check_files_cmake.py` | `every configured DOL object is either in files.cmake or excluded with a reason` |
| module order | `python3 tools/gen_module_order.py --check` | `86 modules, unchanged` |
| per-function diff | `python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json` | `no regression`; the only output is `+100%` on my eight functions, three `LINKED` lines, and one `SPLIT` line resolving 75 moved functions by exact count |

`docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` are **not** in my diff: the judge's own
`MP_GATE_DOCS_WRITE=1` rewrote their derived counts and module list during `goal_check.sh`, and I
reverted both files (`git checkout --`) after the run, since the goal brief says the driver
discards edits to them and the judge re-derives them from the tree.

## Why each spelling was chosen (all of them a transfer, none a guess)

Every function here is byte-identical to something already decompiled, and I compared the
disassembly rather than inferring from the class family. That is the whole reason all five units
matched first try.

1. **`CDarkCommandoRender.cpp`** - `fn_3_91F0` is `addi r3,r3,0x17c; blr`, the same one
   instruction as `CIngSnatchingSwarmBounds.cpp`'s `fn_33_39B8` (`addi r3,r3,0x1c0`, module 33,
   Matching 4/4), so it is written the same way - a `char` range member that decays to its own
   address. `fn_3_91F8` is `CDarkCommandoRenderers`-style pass-through: `stwu r1,-0x10(r1)`,
   `mflr`, `stw`, the call, `lwz`, `mtlr`, `addi`, `blr`. That frame is instruction for
   instruction `fn_33_3A34`'s around the qualified `CActor::PreRender`, and the call is
   **qualified** - the imported name is `Render__6CActorCFRC13CStateManager`, with no `Fv` suffix
   a virtual call carries, so retail calls the base implementation rather than dispatching. Hence
   `reinterpret_cast<CActor*>(self)->CActor::Render(mgr)` with `self` already in r3 and `mgr` in
   r4, which is why nothing is moved.
2. **`CDarkCommandoMemberPtr.cpp`** - `fn_3_6250` is the same accessor shape at +0xC04. The
   class carries the range to 0xC2C because `fn_3_6284` demonstrably reaches `this + 0xC2C` and
   the byte at +0xCB4.
3. **`CDarkCommandoRenderers.cpp`** - `fn_3_648C` is the same frame around a qualified
   `CPatterned::AddToRenderer` (`AddToRenderer__10CPatternedCFRC13CStateManager`, again no `Fv`).
4. **`CDarkCommandoVecList.cpp`** - `fn_3_9864` and `fn_3_9870` are **byte-identical to
   `fn_14_1AB24` and `fn_14_1AB30`**, which `DigitalGuardianVecList.cpp` already decompiles at 2/2.
   Verified side by side: `38 00 00 00 / 98 03 00 0C / 4E 80 00 20`, then
   `3C 80 00 00 / C4 04 00 00 / D0 03 00 00 / C0 04 00 04 / ... / 4E 80 00 20`. So the spellings
   are `self->x0C = 0; return self;` and `CVector3f fn_3_9870(const void* self) { return
   CVector3f::Up(); }`. The `lfsu` on the first word is what says the three reads are consecutive
   and only appears because the whole 12-byte value is copied.
5. **`CDarkCommandoVecCopy.cpp`** - `fn_3_9970` and `fn_3_998C` are this module's own `fn_3_E0`
   again (0xE0, already 18/18 in `CDarkCommandoRel.cpp`) against +0x1B0 instead of +0x54.
   Retail reuses **f0** across all three loads, so the source is three element copies and not a
   `CVector3f` copy or a three-argument construction, either of which hoists into f0/f1/f2. So the
   member is declared as three named floats and written out as three stores. Modelling it as a
   `CVector3f` member and `return self->mVec` was the tidier spelling and I did not use it: it
   risks eliding to `mr r3,r4; addi r3,r3,0x1b0; blr` (8 bytes, not 28), and there was no reason
   to spend a build finding out.

**Nothing is dead-stripped and nothing needs `force_active:`.** All eight functions are already in
`build/G2ME01/DarkCommando/ldscript.lcf`'s FORCEACTIVE block, and `audit_rel_claim.py` reports
`193 text symbols, 0 dropped by -strip_partial`.

## Two things worth knowing before the next carve in this module

1. **`unit_fit.sh` prints `.data claimed - ours 40 <- NOT CLAIMED BY splits.txt` for
   `CDarkCommandoRender.cpp` and `CDarkCommandoRenderers.cpp`.** The 0x28 bytes are
   `SolidMaterial` and nine companions from `Collision/CMaterialList.hpp`, which `CActor.hpp` and
   `Enemies/CPatterned.hpp` reach. **This is not new**: `CIngSnatchingSwarmBounds.cpp` and
   `CIngSnatchingSwarmAi.cpp` carry the identical 40 unclaimed bytes, and module 33's hash holds.
   `CDarkCommandoRel.cpp` avoids them by declaring its own one-method `CPhysicsActor` stand-in
   rather than including `MetroidPrime/CPhysicsActor.hpp` - and it had to, because that unit
   claims `.text 0x0..0x19C` and its module's sha1 **broke** on exactly those 0x28 bytes with
   every function at 100.00%. So the hazard is real but it is confined to a unit that claims the
   module's `.data`, and none of mine does; both files record this in a comment.
2. **`report_diff.py`'s SPLIT resolution is what carries a carve like this, and it still scales.**
   The module's `auto_00_0000019C_text` shrank 167 -> 92 and the tool resolved all 75 moved
   functions by exact count across ten destination units ("a split, not a loss"). Its
   `_exact_subset` search is **bounded to 12 candidate units** (`len(counts) > 12` returns None,
   and then those functions are reported `GONE` and the gate fails). This run used 10. A seventh
   or eighth claim in the same module would cross that bound, so batch the claims and measure -
   do not add them one at a time and assume.

## Files touched

- `src/MetroidPrime/ScriptObjects/CDarkCommandoRender.cpp` (new, 2 functions)
- `src/MetroidPrime/ScriptObjects/CDarkCommandoVecList.cpp` (new, 2 functions)
- `src/MetroidPrime/ScriptObjects/CDarkCommandoVecCopy.cpp` (new, 2 functions)
- `src/MetroidPrime/ScriptObjects/CDarkCommandoMemberPtr.cpp` (new, 1 function)
- `src/MetroidPrime/ScriptObjects/CDarkCommandoRenderers.cpp` (new, 1 function)
- `config/G2ME01/rels/DarkCommando/splits.txt` - five new claims, ascending, nothing else changed
- `configure.py` - five `Object(Matching, ...)` lines in `Rel("DarkCommando", [...])`, one per
  line (a multi-line `Object(` would make `flip_test.sh` vacuous), each with a comment recording
  the measurement that chose the spelling
- `files.cmake` - all five listed, with the reason each does not move the port's undefined count
- `docs/research/raw_offsets.md` - **not** touched; none of the five files contains a raw offset,
  which is why the stand-ins are `char x_padN[...]` ranges with named members rather than
  `this + 0x...`

`files.cmake` was the one non-obvious requirement: `tools/check_files_cmake.py` fails a
configured, on-disk unit that is in no manifest, so a new REL carve has to be listed or listed in
`tools/check_files_cmake.py`'s `EXCLUDED` - and `tools/` is a path this item may not touch. Each
of the five files defines neither `RELMain` nor `RELExit`, so the `MODULE_ENTRY` exemption does
not apply, and each one's only relocation outside itself is a DOL global the port already links
(`sUpVector__9CVector3f` from `src/Kyoto/Math/CVector3f.cpp`,
`Render__6CActorCFRC13CStateManager` from `CIngSnatchingSwarmBounds.cpp`,
`AddToRenderer__10CPatternedCFRC13CStateManager` from `src/MetroidPrime/Enemies/CPatterned.cpp`),
which is why `port probe` and `port link gap` stayed `ok`. **`fn_3_6258`, `fn_3_6284`, `fn_3_852C`
and `fn_3_9184` also call DOL globals (`GetAimPosition__10CPatternedCFRC13CStateManagerf`,
`PassThruVulnerability__20CDamageVulnerabilityFv`, `GetDamageVulnerability__10CPatternedCFv`) -
all three are also defined by the port today, so they would list too; they are left for the reason
in the next section.**

## What is left, and what I did not try

`build/report.json` puts the module at **33 / 193**. The remaining 160 functions are
`auto_*` units: `auto_00_000064AC_text` (55), `auto_00_0000019C_text` (92),
`auto_00_00006258_text` (3), `auto_00_00009218_text` (6), `auto_00_00009890_text` (2),
`auto_00_000099A8_text` (1), `auto_fn_3_9A1C_text` (1). They are the class's own methods, and the
standing blocker for this family is unchanged: they need the `CActor` / `CPatterned` hierarchy
this tree does not model, so they are behavioural class code rather than accessors.

Two **unclaimed** candidates from the item's list, with what I read off them and did not attempt:

- `fn_3_6258` (0x6258, 0x2C) - `stwu/mflr/stw/stw r31 / mr r31,r3 / bl
  GetAimPosition__10CPatternedCFRC13CStateManagerf / lwz/lwz/mtlr/addi/blr`. **`mr r31,r3`
  followed by no use of r31 at all**, which I could not explain, so I did not guess a signature.
  The likely reading is that it is `CDarkCommando::GetAimPosition(mgr, dt)` forwarding to
  `CPatterned::GetAimPosition` with the sret pointer already in r3 and the arguments already in
  r4/r5/f1, so the call needs no shuffling - but the dead `mr r31,r3` is unexplained and the
  frame it produces is the thing being matched, so this needs its own run with objdiff in the
  loop.
- `fn_3_6284` (0x6284, 0x4C) - reads the byte at +0xCB4 and takes bits 4, 3 and 1 of it in that
  order (`extrwi. r0,r4,1,27` / `extrwi. r0,r4,1,28` / `extrwi. r0,r4,1,30`), then calls
  `PassThruVulnerability` or returns `this + 0xC2C` or calls `GetDamageVulnerability`. Plausible
  as one `switch` over a `bool : 1` bitfield class in the `0xCB4` byte, in the style
  `CIngSnatchingSwarmBounds.cpp` already writes for its own `0x73C` byte, but the branch order
  has to come out right and the `0xCB4` byte needs its own field map first. Note `fn_3_6064`
  (0x6064, 0xC) reads bit 2 of the byte at **+0xCB5** and is a two-line bitfield accessor in the
  style of `fn_3_BC`, and `fn_3_602C` (0x602C, 0x04) is a bare `blr` - neither was in this item's
  candidate list.

NEW: progress-vtctl-rel-darkcommando-rest | progress | module:DarkCommando | 160 of the module's 193 functions are still dtk retail bytes; the reachable next slice is fn_3_6064 (0x6064..0x6070, reads bit 2 of the byte at +0xCB5, same shape as fn_3_BC) plus fn_3_602C (0x602C, a bare blr) as one more claim, then fn_3_6258 / fn_3_6284, which need the +0xCB4 byte's field map first

Not committed, as instructed.