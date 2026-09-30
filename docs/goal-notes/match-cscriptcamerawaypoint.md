# match-cscriptcamerawaypoint

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