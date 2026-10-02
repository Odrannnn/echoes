# progress-twin-rel-metareeswarm

**Result: PASS.** `./tools/goal_check.sh build/goal/item.json` -> `PASS progress-twin-rel-metareeswarm`:

```
ok  no judge-owned path touched
ok  gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
ok  counts: matched 13335 -> 13339   linked 6383 -> 6387
ok  check_symbol_names.py
ok  All:  37.39% fuzzy, 30.82% matched, 13.69% linked (13339 / 28465 functions)
ok  target rose: module:MetareeSwarm: 12 -> 16 / 61 functions
ok  no asm added
```

Independently of the judge, measured on this tree: `build/G2ME01/MetareeSwarm/MetareeSwarm.rel` is
`e9b5a7bd0c482e1bfecfd104e9d805bf579df554` - the value `config/G2ME01/config.yml` already records,
unchanged - and `cmp`-equal to `orig/G2ME01/files/RelProd/MetareeSwarm.rel`; all **86** modules'
sha1s equal config.yml (**0 mismatches**); `main.dol` is still
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`; `python3 tools/audit_rel_claim.py MetareeSwarm` prints
`0 claim(s) with a problem` and `preplf 61 text symbols, plf 61, 0 dropped by -strip_partial` (no
dead-strip); `tools/unit_fit.sh` says `fits` / `no extra functions` for both new units;
`tools/check_files_cmake.py` and `tools/check_decl_order.py --all` are clean (1131 units checked,
37 permuted, all 37 the pre-existing `decl_order.md` list).

## What landed

Two new `Matching` units in module 43, four functions, all at 100.00% in `build/report.json`
(`MetareeSwarm/MetroidPrime/ScriptObjects/CMetareeSwarmRelTwins` 2/2, `...RelTwins2` 2/2). Module
`matched_functions` **12 -> 16 / 61**; the old `auto_00_00001FB8_text` (17 fns, 0x1FB8..0x291C)
split into `auto_00_00001FB8_text` (8), `auto_00_000024A0_text` (3) and `auto_00_000027B0_text` (2).

| retail | fn | size | what it is | item's twin |
| --- | --- | --- | --- | --- |
| 0x2690 | `fn_43_2690` | 0x68 | `rstl::uninitialized_copy` over `vector<IGameArea::Dock>` | `CGameArea.cpp` (`0x80060188`) |
| 0x26F8 | `fn_43_26F8` | 0xB8 | `vector<0x1C-byte record>::reserve` | `CCharacterInfo.cpp` (`0x802936EC`) |
| 0x23D4 | `fn_43_23D4` | 0xAC | `vector<CWorldState>::reserve` | `CGameState.cpp` (`0x801466F4`) |
| 0x2480 | `fn_43_2480` | 0x20 | `rstl::destroy<CWorldState*>` forwarder | `main.cpp` `__sys_free` (shape only) |

Files: `src/MetroidPrime/ScriptObjects/CMetareeSwarmRelTwins.cpp` (new, 118 lines),
`src/MetroidPrime/ScriptObjects/CMetareeSwarmRelTwins2.cpp` (new, 82),
`config/G2ME01/rels/MetareeSwarm/splits.txt:15,18` (the two claims),
`configure.py:1515-1534` (two `Object(...)` lines in the existing `Rel("MetareeSwarm", ...)`
block plus its comment), `files.cmake:1215,1221`. No `symbols.txt` rename, no `config.yml` change,
no DOL unit touched.

Each function is an `extern "C"` free function under the module's dtk name: `uninitialized_copy`
is `static inline` and `reserve`/`destroy` are outlined, so retail keeps *local* copies no other
unit can name - a hand-written `fn_43_*` is what gives those retail symbols a partner, the same
device `CCharacterInfo.cpp`/`CTargetReticles.cpp` use. The two `reserve`s reproduce the DOL's own
instantiations instruction for instruction (the 0x23D4 one compared against
`build/G2ME01/obj/MetroidPrime/Player/CGameState.o` with branch targets masked: 43 instructions,
0 diffs); `fn_43_2690` is the DOL's `0x80060188` apart from its `construct` call, which is the
module's own `fn_43_1F70`.

## Four things that had to be measured, in the order they blocked

1. **`fn_43_26F8`'s element is a 0x1C-byte record, not the tree's `CEffectComponent`.** The item's
   twin is shape-only: `include/Kyoto/Animation/CEffectComponent.hpp` is 0x34 bytes (`rstl::string`
   is 0x10 here) with two string members. Measured first attempt, element = `CEffectComponent`:
   `fn_43_26F8` **59.89%** - `mulli r3,rN,0x34`, four saved registers (`stmw r27`), and a destroy
   loop emitting `internal_dereference<basic_string>` calls. Retail's own `0x1C` step and *empty*
   destroy loop fix the element as seven floats with no destructor; with that record the same
   spelling is 100.00% under the module's default GC/1.3.2.
2. **`mw_version="GC/2.7"` is load-bearing for `CMetareeSwarmRelTwins.cpp`, and measured.** Under
   GC/1.3.2 `fn_43_26F8` is exact and `fn_43_2690` scores **92.31%**: the only diff is the
   prologue, `lwz r31,0(r3)` landing *after* the `stw r30`/`stw r29` saves where retail hoists it
   above them. 2.7 emits retail's order with no spelling change - the same save-order class
   `CLumiteRelTail.cpp` records for `fn_39_738`, and the third module tail that is 2.7 code. Try
   2.7 before spending spellings on a module block.
3. **`CMetareeSwarmRelTwins2.cpp` does *not* need the override** - measured exact under both
   GC/1.3.2 and GC/2.7, so it is left at the module default.
4. **The `0x2690`/`0x26F8` spellings come from the twins, not from invention.** `fn_43_2690` is the
   DOL's `uninitialized_copy` spelling (`*first` hoisted into r31, `*last` re-read in the loop
   test); `fn_43_26F8` is `rstl/vector.hpp`'s `reserve` statement order, including `newData` as an
   out-parameter so `allocate`'s return lands in r31 and the swap stores `mItems` then `mCapacity`.

## What is still in this module (measured, not attempted)

`python3 tools/twin_scan.py --list` gives **14** remaining twin rows in `MetareeSwarm/` (the item's
list was taken on the branch head). The reachable ones, by the current split:

- `auto_00_00001FB8_text` (0x1FB8..0x23D4): `fn_43_20F4` (0x6C), `fn_43_21B4` (0x64),
  `fn_43_2218` (0x54), `fn_43_226C` (0x58), `fn_43_22C4` (0x94), `fn_43_2358` (0x7C).
  **0x22C4/0x2358 are the real classes' copy constructors** (`__ct__16CActorParametersFRC...`,
  `__ct__16CLightParametersFRC...`, the DOL's 0x800DFE98/0x800DFF2C, same sizes). Measured here:
  mwcceppc rejects an out-of-line definition of either (`object ... redefined`, the same error
  `CSplitterRelTwins.cpp` records), and a probe that copies a `CLightParameters` to force the
  emission **inlines the copy instead of outlining it** (`probe_cc.sh`, no `__ct__` symbol in the
  object). So they need the overlay-class + `symbols.txt` rename device (Splitter's `0x38B0`), and
  the member list has to include the `CLightParameters` sub-object and the 0x18 bitfield byte.
  **0x21B4/0x2218/0x226C are referenced by nothing**: no `bl` to any of them in the module's asm
  and they are absent from dtk's `ldscript.lcf` FORCEACTIVE list, so a `Matching` unit claiming
  them would have them dead-stripped and break the module sha1 (the `ForgottenObject` measurement
  in `RUNNING_THE_DECOMP.md`); they would need a `force_active:` entry.
- `auto_00_000027B0_text` (0x27B0..0x291C): `fn_43_27B0` (0x64, the outlined copy over the same
  0x1C-byte record - the per-element `cmplwi r5,0` guard is the placement-new one) and
  `fn_43_2814` (0x108, `reserve` over a 1-byte element; twin `CGameArea.cpp`). A third unit of the
  same shape as the two here.
- `auto_00_000024A0_text` (0x24A0..0x2690): `fn_43_24A0` (0x90), `fn_43_2530` (0xAC),
  `fn_43_25DC` (0xB4) - no twin rows.

Nothing was left at a sub-100% score, so there is no `WALL:` line.

## Caveats

- `tools/check_decl_order.py --unit <path>` prints `0 unit(s) checked` for a unit declared in a
  `Rel(...)` block (the Splitter note records the same limitation). Both files are written in
  descending retail order by hand and the module's sha1 is the acceptance test that checks it.
- `fn_43_1F70` is declared with two parameters in `CMetareeSwarmRelTwins.cpp` and with one in
  `CMetareeSwarmDes.cpp`. That is deliberate and noted in the file: retail's `uninitialized_copy`
  call passes r4, the Des carve's own call site does not, and its body ignores r4 either way.
  Changing the Des carve's prototype would have moved `fn_43_1F38` (96.79%, `NonMatching`) and the
  gate ratchets per-function scores, so it was left alone.

## NEW

None filed. The remaining work is the same target (`module:MetareeSwarm`) this item already names,
and the runs above need no new unit, module or symbol - only the overlay-class device for the two
copy constructors and a `force_active:` entry for the three unreferenced destructors.
