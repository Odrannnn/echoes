# progress-rel-ingboostballguardian-c510

`kind: match`, `target: IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardianC510`.
**Module matched_functions 65 -> 67 (of 318)**, one new unit
`IngBoostBallGuardian/IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardianC510`
`Matching` at **100.00% fuzzy / 100.00% matched, 2/2 functions**, with `fn_30_C510` and `fn_30_C568`
both byte-identical to retail - proved by the module's own sha1, not by objdiff.
`All:` **13493 -> 13495 matched**, **6541 -> 6543 linked**, `total_functions` still **28465**,
DOL `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, `IngBoostBallGuardian.rel`
`956265e8ccf3f489e9cb3a70ec22d0357ce17cca` and `cmp`-equal to `orig/G2ME01/files/RelProd/`, all 86
RELs re-checked (**86 match `config/G2ME01/config.yml`, 0 bad**).
`python3 tools/report_diff.py build/report.base.json build/report.json` -> **no regression**,
`+2 functions at 100%, 1 units newly linked`, the two MOVED lines being the split itself.
`./tools/goal_check.sh build/goal/item.json` -> **`PASS`**, `flip_test` -> `PASS -> kept as
Matching`. Both `gate.sh` runs (mine and the judge's, which runs with `MP_GATE_DOCS_WRITE=1`) rewrote
`docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md`; both are reverted here, so the diff is four
files and the verdict does not depend on them.

## What I did

One contiguous `.text` range claimed, 0xC510..0xC5A4 (0x94 = 148 bytes, two functions), as the
four-part carve in one change:

| file | what |
| --- | --- |
| `src/MetroidPrime/ScriptObjects/CIngBoostBallGuardianC510.cpp` | new, `fn_30_C568` and `fn_30_C510` |
| `config/G2ME01/rels/IngBoostBallGuardian/splits.txt` | one entry, in address order (0xC510, between `Predicates` at 0xC240 and `C6AC` at 0xC6AC) |
| `configure.py` | one `Object(Matching, "IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardianC510.cpp", source=...)`, one per line, inside the existing `Rel("IngBoostBallGuardian", ...)` block |
| `files.cmake` | one entry after `CIngBoostBallGuardianC6AC.cpp`, host branch empty by design |

**No `config.yml` change**, as the queue item's reason predicted. Both functions are in
`build/G2ME01/IngBoostBallGuardian/ldscript.lcf`'s FORCEACTIVE list (lines 124 and 125) *and*
`powerpc-eabi-nm -u` over every object dtk writes into `build/G2ME01/IngBoostBallGuardian/obj/`
names both as undefined in `auto_04_00000000_data.o`, the module's vtable store - two independent
reasons, so no `force_active:` entry and no `symbols.txt` rename.

No claim spans an unclaimed gap: the range starts exactly where `fn_30_C4E0` ends (0xC510,
`symbols.txt:206`) and stops exactly where `fn_30_C5A4` begins (0xC5A4, `symbols.txt:209`). After
the carve dtk starts a new auto unit at 0xC5A4 (`obj/auto_00_0000C5A4_text.o`), the proof the split
took. `tools/unit_fit.sh`: `.text claimed 148 ours 148 retail 148, fits`, `no extra functions`.
`nm -S` on the built object: exactly two defined symbols, `fn_30_C510` at +0x00 size 0x58 and
`fn_30_C568` at +0x58 size 0x3C, i.e. descending retail offsets in the object - **which is the
decl-order rule satisfied**, though `tools/check_decl_order.py` reports `0 unit(s) checked` for REL
units as it does for every REL unit in this tree, so the order was read off `nm -S` instead.
`python3 tools/check_symbol_names.py`: `checked 587 units; 0 declared names are missing from their
object`. `python3 tools/check_raw_offsets.py`: `202 raw-offset site(s) in 86 file(s), all
documented` - unchanged, because the unit spells both members as named fields of a local class.
`./tools/probe_sources.sh`: `927 files, 0 failed, 0 errors; link: LINKED (287 undefined, 0
duplicates)`. `python3 tools/check_files_cmake.py`: every configured DOL object is in files.cmake or
excluded with a reason.

## The body

```cpp
bool fn_30_C568(CIngBoostBallGuardianIds* self, CStateManager& mgr) {
  return mgr.GetObjectById(self->x1088) != nullptr;
}

bool fn_30_C510(CIngBoostBallGuardianIds* self, CStateManager& mgr) {
  bool result = false;
  if (fn_30_C568(self, mgr) && self->x108A != self->x1088) {
    result = true;
  }
  return result;
}
```

with a local class whose only members are `TUniqueId x1088` and `TUniqueId x108A` after
`char x_pad0[0x1088]` - the arrangement `CIngSnatchingSwarmBounds.cpp` already uses for the same
reason (this tree models none of module 30's entity code; `fn_30_130`, the module's own entity
loader, is still retail's).

## Two corrections to what the queue item's reason said, both measured

**1. `GetObjectById` does not have to be declared as an `extern "C"` two-argument call.** The
reason said it did. It does not: `include/MetroidPrime/CStateManager.hpp:178` declares
`const CEntity* GetObjectById(TUniqueId uid) const`, that declaration mangles to exactly
`GetObjectById__13CStateManagerCF9TUniqueId`, and MWCC's PPC ABI hands a by-value two-byte class
parameter over **by reference**, so the honest call gives retail's bytes. The inherited fiction
(`unsigned short` members + a hand-declared `extern "C"` callee, spelled with `!= 0` because
`nullptr` needs an include) also gives all 148 bytes - both spellings are in the table below - but
the source uses the header's, because a real type is better than a name shaped like one.
`fn_33_3678` in module 33's `Matching` `CIngSnatchingSwarmAi.cpp` is the same shape through the same
declaration (`lhz r0,0x412(r3)` / `addi r4,r1,0x8` / `sth r0,0x8(r1)` / `bl`), and that unit's
object is the one in the link (`IngSnatchingSwarm` sha1 matches `config.yml`).

**2. `GC/3.0a5` does not merely differ in 16 words; it rejects this tree's flags.** The reason said
"differs in 16". With this tree's cflags `GC/3.0a5` fails in `rstl/single_ptr.hpp`
(`-multibyte` deprecated, treated as an error), so nothing at all can be claimed for it. Nothing in
this item depends on that version.

## Spellings measured (`.text` against the built object, which is byte-identical to retail)

Every variant compiled with the flags in `build.ninja` for this object; "words" is out of the 37 the
two functions are.

| spelling | result |
| --- | --- |
| **as committed, `GC/1.3.2` (the module default)** | **148 bytes, 37/37 words** |
| as committed at `GC/2.0`, `GC/2.0p1`, `GC/2.5`, `GC/2.6`, `GC/2.7` | 148 bytes, 37/37 each |
| as committed at `GC/3.0a5` | compile error in `rstl/single_ptr.hpp` |
| `return fn_30_C568(...) && x108A != x1088;` (short-circuit, no flag) | 148 bytes, **37/37** |
| `x1088 != x108A` (operand order reversed) | 148 bytes, **35/37**, first diff at 0x28 |
| `mgr.GetObjectById(*&self->x1088)` (member address, no named copy) | 148 bytes, **37/37** |
| `TUniqueId id = self->x1088; mgr.GetObjectById(id);` (named copy) | **152 bytes, 28/37** |
| `== nullptr` instead of `!= nullptr` | **144 bytes, 31/37** |
| `self->x108A.value != self->x1088.value` instead of the operator | 148 bytes, 37/37 |
| `bool found = fn_30_C568(...); if (found) result = ...;` (call lifted out) | **140 bytes, 7/37** |
| inherited spelling: `unsigned short` members, `extern "C"` callee, `!= 0` | 148 bytes, 37/37 |
| inherited spelling with `nullptr` (no include) | compile error: undefined identifier `nullptr` |

So: the frame slot at `0x8(r1)` is the **ABI's** copy of a by-value parameter, not a source choice
(passing the member's address gives the same bytes; making the copy a named local costs four); the
`&&` may be written as a flag or returned directly; what is load-bearing is that **the call stays
inside the second operand's guard** (lifting it out drops to 7 of 37 words) and that **`+0x108A` is
compared against `+0x1088` in that order**. `!= nullptr` rather than `== nullptr` is worth four bytes.

## For the merge

`config/G2ME01/rels/IngBoostBallGuardian/splits.txt`, `configure.py` and `files.cmake` are shared
files and will conflict textually with every other module-30 lane. My entries are: `splits.txt`
0xC510 between `CIngBoostBallGuardianPredicates.cpp` and `CIngBoostBallGuardianC6AC.cpp`;
`configure.py` one `Object(...)` line (with its comment) after
`MetroidPrime/ScriptObjects/CIngBoostBallGuardianC6AC.cpp`; `files.cmake` one line after
`CIngBoostBallGuardianC6AC.cpp`. **No `config/G2ME01/config.yml` change**, and no per-object
`mw_version` override, which is the difference from `CIngBoostBallGuardian33B0.cpp` and the other
record-copy units in this module.

0xC510 was claimed by nothing: `grep -c 'start:0x0000C510'` over
`config/G2ME01/rels/IngBoostBallGuardian/splits.txt` in `MetroidPrime2Port`, `wt-mp2-goal`,
`wt-mp2-goal-L2`..`L13`, `wt-sync-trial` and `wt-upsync` is 0 everywhere. **0xC5A4 is now 0
everywhere too** - the `progress-rel-ingboostballguardian-c5a4` lane's carve is not on disk in any
tree I can see, so that unit is claimable again and does not collide with this one (disjoint
ranges: 0xC510..0xC5A4 here, 0xC5A4..0xC68C there). Re-check both before merging.

## Not filed as `NEW:`

Nothing in this run is a new blocker: the range was already proven by
`progress-rel-ingboostballguardian-c5a4` and this run only landed it. The remaining unclaimed gaps in
this module (0xC240..0xC2AC, 0xC2AC..0xC510, 0xC68C..0xC6AC and the rest) are the same work the
queue already tracks, and `fn_30_130`, the module's entity loader, is the blocker
`docs/research/raw_offsets.md` already names for every module-30 offset - a lesson, not an item.