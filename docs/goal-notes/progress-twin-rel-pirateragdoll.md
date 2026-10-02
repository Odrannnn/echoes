# progress-twin-rel-pirateragdoll

`kind: progress`, `target: module:PirateRagDoll`. **Result: the module's matched count went
12 -> 15 of 48; three functions at 100.00% in a new `Matching` unit that flips.**

## What landed

One new unit, `src/MetroidPrime/ScriptObjects/CPirateRagDollLayout.cpp`, claiming module 50's
`.text 0xC2C..0xD80` - the run of three adjacent twins the item listed, taken whole:

| function | bytes | retail function | twin the item named |
| --- | --- | --- | --- |
| `fn_50_C2C` | 0x30 | `CQuaternion::BuildInverted()` | `BuildInverted__11CQuaternionCFv`, `main/MetroidPrime/CAnimData` |
| `fn_50_C5C` | 0xF0 | `CCharLayoutInfo::GetFromParentUnrotated(const CSegId&)` | `GetFromParentUnrotated__15CCharLayoutInfoCFRC6CSegId`, `main/MetroidPrime/CRagDoll` |
| `fn_50_D4C` | 0x34 | `TSegIdMap<CCharLayoutNode>::ContainsDataFor(const CSegId&)` | `ContainsDataFor__28TSegIdMap<15CCharLayoutNode>CFRC6CSegId`, `main/MetroidPrime/CRagDoll` |

Per-function, from `build/report.json` before and after: `fn_50_C2C` 63.17 -> **100.00**,
`fn_50_C5C` 83.15 -> **100.00**, `fn_50_D4C` 95.31 -> **100.00**; the unit
`PirateRagDoll/MetroidPrime/ScriptObjects/CPirateRagDollLayout` 82.19%, 0/3 ->
**100.00%, 3/3**. Project `matched_functions` **13389 -> 13392**, `linked` 6437 -> 6440,
`total_functions` 28465 unchanged.

The four carve files, all in the same change: `configure.py` (a third `Object(Matching, ...)` in
module 50's `Rel(...)`, plus a comment on the two non-obvious choices below),
`config/G2ME01/rels/PirateRagDoll/splits.txt` (`.text 0xC2C..0xD80`),
`files.cmake`, and the source. `config/G2ME01/config.yml` is **unchanged** - see below.
**No asm added.** The unit's twins' source matched unchanged in all three cases: only the
statement spelling differs, because the module's names are `fn_50_*` and not the mangled names
objdiff pairs in the DOL (and `CQuaternion::BuildInverted` is already defined by the DOL's own
`MetroidPrime/CAnimData.cpp`, so the member definition is not available).

## Verified

```
./tools/decomp_build.sh                 All: 13392 / 28465 functions, 87 files OK
sha1sum build/G2ME01/main.dol           6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
python3 tools/check_symbol_names.py     0 missing names
python3 tools/check_decl_order.py --unit CPirateRagDollLayout   ok
python3 tools/check_files_cmake.py      every configured DOL object is in files.cmake or excluded
./tools/unit_fit.sh .../CPirateRagDollLayout.cpp
    .text claimed 340 ours 340 retail 340 fits; no extra functions
./tools/flip_test.sh MetroidPrime/ScriptObjects/CPirateRagDollLayout.cpp
    PASS -> kept as Matching (1 kept / 0 failed / 0 skipped)
MP_GOAL_TREE=$PWD ./tools/goal_check.sh build/goal/item.json   goal_check: PASS
```
All 86 RELs are `cmp`-equal to `orig/G2ME01/files/RelProd/` and their sha1s match
`config/G2ME01/config.yml` (the build's own `87 files OK`, plus `goal_check`'s `gate.sh`).

## Three measurements worth more than the three functions

**1. `mw_version="GC/2.7"` is load-bearing on this unit.** Under the module default GC/1.3.2 the
unit is 82.19%, 0/3, and every differing byte is in `fn_50_C5C`'s prologue: 1.3.2 sinks the
argument setup (`lbz r0, 0x0(r5)`, the `lwz` of the node map) *behind* the
`stw r31`/`stw r30`/`stw r29` saves, while retail interleaves them - `lbz` first, then the saves
one at a time between the setup instructions. 2.7 emits retail's order. Same per-object override
`CSandBossRelTail.cpp` and `CLumiteRelTail.cpp` already use, for the same reason.

**2. `force_active:` is not free in a module other modules import from - do not reach for it
here.** None of these three is in `build/G2ME01/PirateRagDoll/ldscript.lcf`'s FORCEACTIVE block,
which reads exactly like the ScriptCoin dead-strip case the docs record, so the list was added to
module 50 in `config/G2ME01/config.yml` first. PirateRagDoll then hashed, and **DarkCommando,
CommandoPirate, DarkTrooper and SpacePirate all stopped hashing**, one byte each:

```
cmp -l orig/G2ME01/files/RelProd/DarkCommando.rel build/G2ME01/DarkCommando/DarkCommando.rel
44156  70 154          # octal: 0x38 -> 0x6c, at file offset 0xAC5B
```
The same single byte at the same offset in all four, and those four are precisely the modules that
`bl fn_50_1938` - module 50's only export. The list shifts the position of that name in module 50's
string table by 52 bytes. Reverted; **no config.yml change ships with this item.**
Nothing needs stripping anyway: dtk's `auto_00_0000010C_text` shrinks to 0x10C..0xC2C and keeps
**undefined references** to `fn_50_C2C` and `fn_50_C5C` (`powerpc-eabi-nm -u` prints both), and
`fn_50_D4C` is reached from `fn_50_C5C`'s own `bl`. The lesson generalises past this module: check
who imports a module before adding a `force_active:` list to it.

**3. Calling `self->mNodes->ContainsDataFor(parent)` from `fn_50_C5C` emits a fourth function.**
mwcceppc declines to inline the template member and writes an out-of-line **weak** copy under its
mangled name, `ContainsDataFor__28TSegIdMap<15CCharLayoutNode>CFRC6CSegId`, which the `bl` then
targets. That is a 0x34-byte function inside a 0x154-byte claim: the module's `.text` came out
**0x3500 against retail's 0x34CC** and its sha1 stopped matching, with `fn_50_D4C` dead in the
object. Writing the test as a direct call to `fn_50_D4C` fixes it, and
`powerpc-eabi-objdump -r -j .text` then shows the relocation's target is `fn_50_D4C`. The call is
the same call either way - r3 the map, r4 the stack `CSegId`.
`unit_fit.sh`'s "no extra functions" line is what catches this class; the percentage does not,
because the three intended functions still score.

One more that is a spelling rather than a finding: `mNodes` has to be spelled
`rstl::object_owner<TSegIdMap<CCharLayoutNode>>`, so `(*self->mNodes)[id]` is `operator*` then
`operator[]` as `CCharLayoutInfo::GetSegmentData` spells it. With a bare
`TSegIdMap<CCharLayoutNode>*` member, `fn_50_C2C` and `fn_50_D4C` are exact and `fn_50_C5C` is 31
bytes out in its first eight instructions.

## Left in the module

33 functions of 48 still unmatched. The item listed two more twin runs in `auto_00_00000DC0_text`
that are **not** done and are not claimed: `.text 0x23AC..0x2664` (6 adjacent, 696 B) and
`0x26F8..0x2908` (4 adjacent, 528 B). Both are `__dt__` template instantiations whose classes this
tree declares only as far off (`rstl::vector<i>`, `CErrorOutputWindow`, `CMain`,
`rstl::reserved_vector<...>`), so each needs a layout mirror on the model used here, and the run's
dead-strip situation must be checked against `ldscript.lcf` before a claim - not with
`force_active:`, for the reason in measurement 2. `fn_50_298C` (`IsValid__23CValidWaypoint-
PredicateCFRC13CStateManager9TUniqueId`, 0x40) and `fn_50_29CC` (`reserve__Q24rstl67vector<...>Fi`,
0xB8) are the two smallest twins left with a source, at the head of the 0x29CC run.
