# match-cscriptcamerawaypoint

> **Superseded: the unit is `Matching` and `goal_check.sh` says PASS (attempt 2, 2026-09-30).**
> Everything below is the first run's record, kept as written; the last section has the verdict, the
> change and the evidence. The first run's claim that the six `CScriptWaypoint` symbols "only real
> definitions at those addresses" could resolve is wrong - see "Attempt 2".

**Partial.** All five functions of the unit are now at 100% and the unit's matched count in
`build/report.json` rose 4/5 -> 5/5 (global `All:` 9930 -> 9931 matched functions, no function
anywhere got worse). `tools/flip_test.sh` still fails, but **at the link, not on content**: six
`CScriptWaypoint` symbols our object references exist nowhere in the link. Details and evidence
below. Kept in `configure.py` as `NonMatching`.

## The one function, and what it needed

`CScriptCameraWaypoint::NextWaypoint` was at 87.36%. Retail's 25 instructions
(`./tools/dis.sh 0x800A5594 0x64`) decompose into three facts the old source got wrong:

1. **It calls `CEntity::FindConnectedObject_if`, not `CheckConnectedObject_if`.**
   `800a55cc: bl 80047d54 <FindConnectedObject_if__7CEntity...>`. The old source called
   `CheckConnectedObject_if`. (Confirmed in `config/G2ME01/symbols.txt:1342` vs `:1337`.)

2. **The predicate is a named local, not a temporary.** Retail constructs it at `r1+8` with the
   standard derived-constructor prologue (store base vptr, then store derived vptr) and destroys
   it at scope exit, so it must be a local object rather than a bound-to-`const&` temporary.

3. **The destructor must be declared *inline in the class body*.** This is the non-obvious one, and
   it is a general MWCC rule (see "codegen lesson" below). Retail's scope-exit sequence is
   `lis r4,__vt__29@ha / addi r3,r1,8 / addi r0,r4,__vt__29@l / li r4,0 / stw r0,8(r1) /
   bl __dt__21CValidEntityPredicateFv` - i.e. the *derived* destructor inlined with the delete flag
   known to be 0. With the destructor declared out-of-line, MWCC emits
   `addi r3,r1,8 / li r4,-1 / bl __dt__29CValidCameraWaypointPredicateFv` instead, which is the
   whole 12.64% gap.

## Changes made (`src/MetroidPrime/ScriptObjects/CScriptCameraWaypoint.cpp` only)

- `~CValidCameraWaypointPredicate() override;` + an out-of-line definition ->
  `~CValidCameraWaypointPredicate() override {}` in the class body (out-of-line definition deleted).
- `NextWaypoint` now takes a named local `CValidCameraWaypointPredicate predicate;`.
- `CheckConnectedObject_if` -> `FindConnectedObject_if`.

Nothing else touched. No `asm`, no `configure.py` change, no other unit touched.

## Measurements

```
$ ./tools/decomp_build.sh MetroidPrime/ScriptObjects/CScriptCameraWaypoint
main/MetroidPrime/ScriptObjects/CScriptCameraWaypoint: 100.00% fuzzy, 100.00% matched (5 / 5 functions)
All:  30.60% fuzzy, 22.70% matched, 11.74% linked (9931 / 28465 functions)
```

`build/report.json`, per function, all 100.0: `IsValid__29CValidCameraWaypointPredicate...` (64),
`__ct__21CScriptCameraWaypoint...` (72), `__dt__21CScriptCameraWaypointFv` (96),
`__dt__29CValidCameraWaypointPredicateFv` (96), `NextWaypoint__21CScriptCameraWaypointCFR13CStateManager` (100).
`matched_code` 328 -> 428 of 428. Diffed against `build/report.base.json` per function:
**0 worse, 1 better** (NextWaypoint 87.36 -> 100.0), 0 symbols lost.

## What stops the flip

```
$ ./tools/flip_test.sh MetroidPrime/ScriptObjects/CScriptCameraWaypoint.cpp
    build failed:
      FAILED: [code=1] build/G2ME01/main.elf
      ### mwldeppc.exe Linker Error:
      #   undefined: 'CScriptWaypoint::CScriptWaypoint(TUniqueId,const '
      #   undefined: 'CScriptWaypoint::~CScriptWaypoint()'
      #   undefined: 'CScriptWaypoint::AcceptScriptMsg(CStateManager&,const '
      #   undefined: 'CScriptWaypoint::AddToRenderer(const CStateManager&) const'
  FAIL  -> reverted
```

The base class is not decompiled and is not a unit. Its six methods live in an **unclaimed `.text`
gap**, 0x8007359C..0x80073938, so the only symbols that exist for them are dtk's `fn_*` names:

| our object's reference (from `nm -u`) | retail name at that address |
| --- | --- |
| `__ct__15CScriptWaypointF9TUniqueIdRCQ24rstl66basic_string<...>RC11CEntityInfoRC12CTransform4f` | `fn_800737E4` |
| `__dt__15CScriptWaypointFv` | `fn_80073844` |
| `AcceptScriptMsg__15CScriptWaypointFR13CStateManagerRC10CScriptMsg` | `fn_80073754` |
| `AddToRenderer__15CScriptWaypointFCR...` (vtable +0x28) | `fn_80073598` |
| `Render__15CScriptWaypointFCFR...` (vtable +0x2c) | `fn_80073594` |
| `FollowWaypoint__15CScriptWaypointCFR13CStateManager` (vtable +0x80) | `fn_8007359C` |

Exactly six symbols are unresolvable; `comm -23` of our object's undefined list against
`build/G2ME01/main.elf`'s symbol list returns those six and nothing else, and the **retail** object's
equivalent list is empty. `config/G2ME01/symbols.txt` contains no `__ct__/__dt__/AcceptScriptMsg/
AddToRenderer/Render/FollowWaypoint__15CScriptWaypoint*` name at all, so there is no C++ spelling
that would resolve: only real definitions at those addresses would. Hence this is not fixable from
`CScriptCameraWaypoint.cpp`.

### The two *remaining* object differences are benign (do not chase them)

`tools/compare_unit.sh MetroidPrime/ScriptObjects/CScriptCameraWaypoint` reports only:

- `.text` 0x1d8 vs retail 0x1ac: +44 bytes, all of it the weak COMDAT
  `GetHealthInfo__6CActorCFv` that the linker discards.
- `.data` 0x94 vs retail 0x98: 4 trailing zero bytes. This is 8-byte alignment padding after the
  predicate vtable (`__vt__29CValidCameraWaypointPredicate` ends at 0x803B3374, next object at
  0x803B3378), and mwldeppc reproduces it.

Both are attested by a unit that **does** flip with the identical shape:
`Kyoto/Streams/DolphinCLZOInputStream.cpp` is `Matching` with retail `.data` 0x10 vs ours 0xc
(same 4 trailing zero bytes) and retail `.text` 0x1e8 vs ours 0x248 (same discarded weak thunk).
`tools/unit_fit.sh` on this unit therefore only warns.

## Codegen lesson (measured on a scratch TU with the project's exact cflags)

MWCC 2.7 GameCube, class deriving from `CValidEntityPredicate`:

- destructor declared **out-of-line** -> scope-exit call is `li r4,-1; bl __dt__D`, and the
  out-of-line thunk `__dt__D` **is** emitted (weak if the class also introduces a virtual).
- destructor declared **inline in the class body** (`~D() override {}`) or implicit -> the
  scope-exit call is inlined as `store vt_D; li r4,0; bl base dtor`, and the thunk is **also**
  emitted - but only as a **weak** symbol `W __dt__D`. (`nm` on a `~D() override {}` class shows
  `W __dt__D`; an out-of-line one shows `T`.)

So "inline the destructor to get the inlined call" does *not* lose the function: the thunk is still
there, and this unit still links 5/5. Spellings tried and their `NextWaypoint` scores, all with the
out-of-line destructor (all identical at 87.36%): `override` / no `override` / explicit `virtual`;
non-`const` local, `const` local, `CValidCameraWaypointPredicate predicate()`, `IsValid` inline in
the class, `IsValid` out-of-line, the class in an anonymous namespace, the result stored in a named
`const TUniqueId` in a nested block. Only the destructor declaration moves the number, and only to
100%.

Two knock-on notes for other units, **not** fixed here:

- `CScriptEffect.cpp:208` (`const CEffectWaypointPredicate predicate;` in `AcceptScriptMsg`) and
  `CScanDisplay.cpp` (`CScanTargetPredicate` in `PrepareScanDisplay`) hit the same destructor-inlining
  wall. Those are the only three places in the whole DOL where a `CValidEntityPredicate` subclass's
  destructor is inlined - scanning `main.elf`'s `.rela.text` for a `__vt__*@l` store within 40 bytes
  before a `bl __dt__*` finds 177 sites and only those three are scope-exit destructor calls.
  Declaring `~CEffectWaypointPredicate() override {}` in the class body should lift
  `AcceptScriptMsg` above 46.7%; worth a `NEW:` when someone has budget for `CScriptEffect`.
- `CScriptTeamAiMgr.cpp:92` passes `CTeamAiPredicate()` as a temporary. Retail's vtable
  (`__vt__16CTeamAiPredicate`, 0x803B5620, 0x10 bytes) has no trailing-null word and retail's call
  site is the out-of-line `li r4,-1; bl __dt__16CTeamAiPredicateFv` shape - so that one is already
  right. **Do not** "fix" it into a named local.

## Gates

- `./tools/decomp_build.sh`: `All: 30.60% fuzzy, 22.70% matched, 11.74% linked (9931 / 28465 functions)`
  - up from 9930, nothing else moved.
- `sha1sum build/G2ME01/main.dol`: `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (unchanged; the unit
  stays `NonMatching`, so this is only the "nothing else broke" check, not evidence for this unit).
- `python3 tools/check_symbol_names.py`: `checked 503 units; 0 declared names are missing from their object`.
- `./tools/probe_sources.sh`: `probe: 749 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)`.
- `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CScriptCameraWaypoint`:
  `ok: 1 unit(s) checked, none emits its functions out of retail order`.
- `git diff --stat`: 1 file, +5/-7.

`python3 tools/check_docs_claims.py` now reports the HANDOFF state block stale
(`matched 9931 / 28465`, `DOL units 8520 / 16726`) - the driver rewrites those; I did not touch
`docs/`.

## NEW:

NEW: match-cscriptwaypoint-base | match | MetroidPrime/ScriptObjects/CScriptWaypoint | the six CScriptWaypoint methods at 0x80073594, 0x80073598, 0x8007359C, 0x80073754, 0x800737E4, 0x80073844 sit in the unclaimed .text gap 0x8007359C..0x80073938 as fn_80073594/3598/359C/3754/37E4/3844, so every unit that inherits from CScriptWaypoint cannot link when flipped; carve 0x80073594..0x80073844 (and 0x8007359C..0x80073938 for the rest of the gap) and decompile it.
---

# Attempt 2 (lane 5, 2026-09-30) - PASS. The unit is `Matching`.

The `**Partial.**` line above is the first run's verdict and is now superseded: the unit reaches
`Matching`, `flip_test.sh` **passes**, and `goal_check.sh` says `PASS match-cscriptcamerawaypoint`.
The six `CScriptWaypoint` symbols were the whole blocker, and the first run's conclusion about them
was wrong.

## What the first run got wrong, and why it cost a run

It wrote: *"`config/G2ME01/symbols.txt` contains no `__ct__/__dt__/AcceptScriptMsg/AddToRenderer/
Render/FollowWaypoint__15CScriptWaypoint*` name at all, so there is no C++ spelling that would
resolve: only real definitions at those addresses would."* Both halves of that are wrong, and the
second is the expensive one:

- Those six addresses **are already in the link**. `build/G2ME01/main.elf` defines
  `fn_80073594 / fn_80073598 / fn_8007359C / fn_80073754 / fn_800737E4 / fn_80073844` - they live in
  dtk's `auto_03_8007359C_text`, so "unclaimed gap" means *nobody has renamed them*, not *nobody
  links them*.
- `symbols.txt` **is** where a C++ spelling comes from. It is a rename table: `main.elf` has no
  `__dt__15CScriptWaypointFv` only because line 2136 still says `fn_800737E4`. Renaming the six
  lines makes the existing definitions carry the names our object asks for. No carve, no
  `configure.py` edit for the base class, no new unit, and the DOL bytes do not move - a rename
  changes a symbol table, not a section.

The check that would have caught it in one command: compare the unit's **own** object, which is
`build/G2ME01/src/<unit>.o`, against dtk's `build/G2ME01/obj/<unit>.o`. The first run read
`build/G2ME01/obj/...`, which is dtk's *retail* object even while the unit is `NonMatching`; it
therefore saw the `fn_*` names and concluded they were ours. `build/G2ME01/src/...` is the one the
linker uses for a `Matching` unit.

## The change (3 files, 4 hunks of substance)

1. **`config/G2ME01/symbols.txt`**, six renames in place (addresses, `size:` and `align:` kept):

   | was | is | address |
   | --- | --- | --- |
   | `fn_80073594` | `Render__15CScriptWaypointCFRC13CStateManager` | 0x80073594 |
   | `fn_80073598` | `AddToRenderer__15CScriptWaypointCFRC13CStateManager` | 0x80073598 |
   | `fn_8007359C` | `FollowWaypoint__15CScriptWaypointCFR13CStateManager` | 0x8007359C |
   | `fn_80073754` | `AcceptScriptMsg__15CScriptWaypointFR13CStateManagerRC10CScriptMsg` | 0x80073754 |
   | `fn_800737E4` | `__dt__15CScriptWaypointFv` | 0x800737E4 |
   | `fn_80073844` | `__ct__15CScriptWaypointF9TUniqueIdRCQ24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>RC11CEntityInfoRC12CTransform4f` | 0x80073844 |

2. **`src/MetroidPrime/Carve80073594.c`**, the two definition names, and the comment above them.
   This carve already owns 0x80073594..0x8007359C and is `Matching`; it defined the two functions as
   `fn_80073598` / `fn_80073594` because "retail names none of these" - true of the *symbol table*,
   false of the *code*. They are `CScriptWaypoint::AddToRenderer` and `::Render`, one `blr` each, so
   the file now spells those two names verbatim, still as C. That is the file's existing job and
   the repo's existing arrangement: `MetroidPrime/Weapons/Carve801D6930.c` does the same for
   `SetTargetId__10CAuxWeaponF9TUniqueId`. Writing them as real out-of-line C++ methods would mangle
   them a second time and objdiff would pair nothing.
3. **`configure.py`**, `NonMatching` -> `Matching` for the target unit (the flip's own edit).

No `asm`, no `splits.txt` / `files.cmake` edit, no other unit touched.

## The names are forced, not guessed

Every one of the six is pinned by bytes, not by resemblance:

- **The four vtable slots.** Retail's `__vt__21CScriptCameraWaypoint` (0x803B32E0) and our object's
  `.data` are byte-identical, so the slot order is the C++ declaration order. dtk's
  `obj/.../CScriptCameraWaypoint.o` has `R_PPC_ADDR32 fn_80073754` at `.data+0x18`,
  `fn_80073598` at `+0x28`, `fn_80073594` at `+0x2c`; our
  `src/.../CScriptCameraWaypoint.o` has `AcceptScriptMsg__15CScriptWaypoint...`,
  `AddToRenderer__15CScriptWaypoint...`, `Render__15CScriptWaypoint...` at exactly those offsets.
  `include/MetroidPrime/ScriptObjects/CScriptWaypoint.hpp` derives from `CActor` and overrides
  nothing between them, so the slots are `AcceptScriptMsg` (+0x18), `AddToRenderer` (+0x28),
  `Render` (+0x2c), `FollowWaypoint` (+0x80 = `fn_8007359C`).
- **The ctor and the dtor.** Retail's `__dt__21CScriptCameraWaypointFv` (0x800A5658) is
  instruction-for-instruction `fn_800737E4` apart from the vtable it stores, and it does
  `bl 800737e4`; retail's `__ct__21CScriptCameraWaypoint...` (0x800A56B8) is `fn_80073844`'s shape
  and does `bl 80073844`. Our object's `.text` relocations for `__dt__15CScriptWaypointFv` and
  `__ct__15CScriptWaypointF...` sit at the same offsets. **So the 0x60-byte function is the
  destructor and the 0xF4-byte one is the constructor - the reverse of what the addresses suggest,
  and the reverse of what a first reading of `0x60 = small = ctor` would give.**
- **The vtable both of them store** is `lbl_803B20B0` (0x803B20B0, `size:0x84`, the same size and
  the same slot-for-slot layout as `__vt__21CScriptCameraWaypoint`). It is `CScriptWaypoint`'s: its
  `+0x0c` slot is `0x8009ca8c`, which `symbols.txt` already names
  `TypesMatch__15CScriptWaypointCFi`. Not renamed here (nothing needs it) - but
  `lbl_803B20B0 = __vt__15CScriptWaypoint` is a free correctness win for whoever carves the class.
- **The bodies agree**: `fn_8007359C` is `CheckConnectedObject(mgr, 'ARRV', 'NEXT')` returning a
  `TUniqueId`, which is what a `FollowWaypoint` is; `fn_80073844` calls `__ct__6CActorF...` then
  `SetUseInSortedLists(0)` / `SetCallTouch(0)`; `fn_800737E4` calls `__dt__6CActorFv` then
  `Free(this)`.

## Measurements

```
$ ./tools/flip_test.sh MetroidPrime/ScriptObjects/CScriptCameraWaypoint.cpp
TEST MetroidPrime/ScriptObjects/CScriptCameraWaypoint.cpp
  PASS  -> kept as Matching
kept: 1 / 1   failed: 0   skipped: 0
```

```
$ ./tools/goal_check.sh build/goal/item.json
goal_check: item match-cscriptcamerawaypoint (match) target=MetroidPrime/ScriptObjects/CScriptCameraWaypoint
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 10035 -> 10035   linked 4903 -> 4908
  ok    check_symbol_names.py
  ok    All:  30.96% fuzzy, 23.20% matched, 11.76% linked (10035 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptObjects/CScriptCameraWaypoint.cpp: PASS, Object(Matching) in configure.py
goal_check: PASS match-cscriptcamerawaypoint
```

`build/report.json`: `main/MetroidPrime/ScriptObjects/CScriptCameraWaypoint` `complete: true`,
5/5 functions at 100.0 (`matched_code` 428/428); `main/MetroidPrime/Carve80073594` `complete: true`,
2/2 at 100.0 (8/8 bytes). `tools/report_diff.py` against the judge's baseline prints six
`RENAMED` lines and nothing else - the carve's two stay 100.00% -> 100.00% and the four in
`auto_03_8007359C_text` stay 0.00% -> 0.00% - so nothing was lost and `matched` held at 10035
while `linked` rose **4903 -> 4908**, which is the count the one rule accepts.

`matched` did not move and cannot: the target was already 5/5. `linked` is the real result.

## Gates

- `sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` **with the unit
  `Matching` and our object in the link** - the one rule's test, and the first time it has held for
  this unit.
- `./tools/gate.sh`: every step `ok` (`hashes vs config.yml`, `per-function diff`, `module wiring`,
  `raw offsets`, `decl order`, `files.cmake`, `port probe`, `port link gap`), `GATE PASS 3e8a1c0+4`.
  All 86 RELs therefore still `cmp`-equal and sha1-matched. Run bare it reports `GATE FAIL: docs`
  only because `docs/HANDOFF.md`'s count block is stale; `goal_check.sh` runs the same gate with
  `MP_GATE_DOCS_WRITE=1` and it passed, and the driver rewrites those numbers anyway.
- `python3 tools/check_symbol_names.py`: `checked 503 units; 0 declared names are missing from their object`.
- `./tools/probe_sources.sh`: `probe: 749 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)`.
- `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CScriptCameraWaypoint` and
  `--unit MetroidPrime/Carve80073594`: both `ok` (the carve's two stay in descending address order,
  `AddToRenderer` 0x80073598 before `Render` 0x80073594).
- `./tools/unit_fit.sh MetroidPrime/Carve80073594.c`: `.text claimed 8 ours 8 retail 8 fits`,
  `no extra functions`.
- `git diff --stat`: 3 files.

## The general recipe this establishes (for any other blocked flip)

When a `Matching` candidate fails `flip_test` with `undefined: 'CSomeBase::SomeMethod(...)'`:

1. Read **our** object, `build/G2ME01/src/<unit>.o`, and get the exact mangled name from
   `nm -u` / `readelf -rW` (that is also how you get the ctor's full `rstl::string` spelling right).
2. If dtk's object for the same unit (`build/G2ME01/obj/<unit>.o`) has the matching `.data`
   relocation under an `fn_*` name, the address is known and the function has a real name:
   **rename the `fn_*` line in `config/G2ME01/symbols.txt`.** No carve needed.
3. If that address is inside an existing `Carve*.c` unit instead, the carve has to spell the new
   name - still as C, still verbatim. Look for a sibling that already does
   (`grep -rl 'F9TUniqueId' src/MetroidPrime/*/Carve*.c`).
4. The rename only works if nothing else *defines* the same symbol; dtk regenerates every `auto_*`
   object from the same table, so their references follow the rename automatically. Verified here by
   the clean full `ninja` link at every step.

## For `match-cscriptwaypoint-base` (still queued, still worth doing)

The class is not decompiled and the rename does not change that - the four bodies are still dtk's
0.00% in `main/auto_03_8007359C_text` (0x8007359C..0x80073938). What this run adds:

- **`fn_800735D8` (0x800735D8, 0x11C) is `NextWaypoint__15CScriptWaypointCFR13CStateManager`.**
  Forced: it is at `lbl_803B20B0+0x7c`, and `__vt__21CScriptCameraWaypoint+0x7c` is
  `NextWaypoint__21CScriptCameraWaypointCFR13CStateManager`; the two vtables are identical through
  `+0x78` (`0x8004ac98`).
- `lbl_803B20B0` (0x803B20B0, 0x84) is `__vt__15CScriptWaypoint` - rename it in the same change.
- `lbl_803B2134` (0x803B2134, 0x14) is a 3-word vtable whose only virtual is `fn_800736F4`
  (0x60); some `CValidEntityPredicate`-shaped class, still unidentified.
- Two of the eight (0x80073594, 0x80073598) are already reproduced, as C, by
  `src/MetroidPrime/Carve80073594.c`. When the class is carved properly that carve's range merges
  into it and the `.c` goes away.

## NEW:

None. The first run's `NEW: match-cscriptwaypoint-base` still stands and is unchanged by this one -
the rename makes the *link* work, it does not decompile the class. No other `NEW:` is filed: the
recipe above is a method, not a target, and I did not find a second concrete blocked unit.
