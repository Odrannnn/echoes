# progress-prime1-cscriptplatform-slavevec

`MetroidPrime/ScriptObjects/CScriptPlatform` — `progress` item, unit stays `NonMatching`.

The item was `fn_800A14DC` (100 B), undefined in our object and called from `AddSlave+0x144`,
`BuildSlaveList+0xfc` and `AddRider(vector)+0x22c`. It is now defined, and so is the reserve it
needs, and `AddSlave` is written for real.

## Result

| | before | after |
|---|---|---|
| unit `matched_functions` | **20** / 60 | **23** / 60 |
| unit `matched_code` | 1916 B | **2352 B** (of 18000) |
| unit `fuzzy_match_percent` | 21.83% | **26.58%** |
| tree `matched_functions` | 10011 / 28465 | **10014** / 28465 |
| `AddSlave` | 0.93% | **98.83%** (432 B of retail's 428) |

**+3 functions at 100%, 0 worse, 0 asm added** — `tools/gate.sh`'s own per-function diff line,
quoted:

```
per-function diff   matched  10011 -> 10014   linked 4896 -> 4896   (+3 functions at 100%, 0 units newly linked)
```

Gained, all three new: `fn_800A14DC`, `fn_800A46F0`, `fn_800A47A8`. Nothing in the unit, or
anywhere in the tree, went below a score it had.

## What the three symbols are

Read out of `build/G2ME01/obj/MetroidPrime/ScriptObjects/CScriptPlatform.o` (retail) with
`objdump -d -r`; the relocation at each `bl` names the callee, which is what turns a guess into
a measurement. The chain the previous item's notes could not see is two deep, not one:

| symbol | DOL address | size | what it is | called from |
|---|---|---|---|---|
| `fn_800A14DC` | 0x800A14DC | 100 B | `rstl::vector<SRiders>::push_back_unsafe`, out-of-line | `AddSlave+0x144`, `BuildSlaveList+0xfc`, `AddRider(vector)+0x22c` |
| `fn_800A46F0` | 0x800A46F0 | 184 B | `rstl::vector<SRiders>::reserve`, out-of-line | `AddSlave+0x104`, `AddRider(vector)+0x220` |
| `fn_800A47A8` | 0x800A47A8 | 152 B | `rstl::uninitialized_copy<pointer_iterator<SRiders>, SRiders*>` | `fn_800A46F0+0x68` |

So `AddSlave` cannot be written without `fn_800A46F0` either, and `fn_800A46F0` cannot be
written without `fn_800A47A8`; that is the whole reason the previous item stopped at a note.

## The two codegen rules this needed (both measured, both worth keeping)

- **A retail symbol with no name in the map is matched by writing it `extern "C"` with that
  name.** `fn_800A14DC` has no parameter suffix in retail's object, i.e. the original had no
  mangleable signature there, so the only way objdiff can pair it with a function of ours is a
  symbol of exactly that name. `extern "C" void fn_800A14DC(...)` emits it verbatim; the repo
  already does this in `src/MetroidPrime/CAnimData.cpp:368` (`fn_800A27B44`, 100% matched).
  A member or template name never matches — `rstl::vector<SRiders>::push_back_unsafe` is
  `push_back_unsafe__Q24rstl43vectorI7SRiders,Q24rstl17rmemory_allocator>FRC7SRiders`, and
  calling it instead of writing `fn_800A14DC` gives a 32-byte forwarder (`bl`, prologue, blr)
  and 0%. The repo already does this in `src/MetroidPrime/CAnimData.cpp:368`
  (`fn_80027B44`, 100% matched, verified in `build/report.json`).
- **Our `rstl` already generates both of the other two bodies instruction for instruction.**
  Measured by forcing the out-of-line instantiations into the object
  (`tmp_reserve(v,n){ v.reserve(n); }`, 184 B weak `reserve__Q24rstl43vectorI7SRiders...Fi`):
  identical to retail 0x800A46F0 except the one `bl` target name, and the weak
  `uninitialized_copy<...>` is identical to retail 0x800A47A8. Only the names were missing, so
  both are written out by hand in `CScriptPlatform.cpp` and the templates are left alone.
- **`fn_800A46F0` must not take the fourth argument retail passes.** Retail calls
  `fn_800A47A8(r3=begin, r4=end, r5=out, r6=oldEnd)` (0x800A4740-0x800A4758: the two iterator
  temporaries at `r1+8..r1+23`, then the call). No instruction of the body reads `r6`, and
  spelling the argument (`slaves.mItems + slaves.mCount`) costs four instructions — a reload
  of `mCount`, of `mItems` and a second `mulli`/`add` — because the value is already in a
  register. Measured: 3 parameters → 184 B, byte-exact; 4 parameters → 200 B.
- `fn_800A14DC`'s `add. r5,r6,r0` + `beq` null test is CodeWarrior's placement-new guard, not a
  hand-written `if (p)`: `rstl::construct(&slaves.mItems[slaves.mCount++], slave)` reproduces it,
  and the implicit `SRiders` copy constructor is what produces the `sth`/`cmplwi`/`stb`/`beq`/
  `lfs`/`stfs` optional field copy plus the `bl __ct__12CTransform4fFRC12CTransform4f`.

## AddSlave: written, 98.83%, one instruction of register allocation away

Retail's body (0x800A1330, 428 B) is the Prime 1 `AddSlave`
(`prime-ref/src/MetroidPrime/ScriptObjects/CScriptPlatform.cpp:480`) with Echoes' decay timer
and the "already a slave" update branch:

```cpp
rstl::vector< SRiders >::iterator slave =
    rstl::find(mDynamicSlaves.begin(), mDynamicSlaves.end(),
               SRiders(id, CTransform4f::Identity(), rstl::optional_object< float >()));
if (slave == mDynamicSlaves.end()) {
  if (CActor* actor = TCastToPtr< CActor >(mgr.ObjectById(id))) {
    actor->AddMaterial(kMT_PlatformSlave, mgr);
    CTransform4f xf = GetTransform().GetQuickInverse() * actor->GetTransform();
    fn_800A46F0(mDynamicSlaves, mDynamicSlaves.mCount + 1);
    fn_800A14DC(mDynamicSlaves, SRiders(id, xf, rstl::optional_object< float >(decayTimer)));
  }
} else {
  slave->mDecayTimer = decayTimer;
}
```

Everything matches except, at 0x800A11C8, retail loads the found iterator into `r3`
(`lwz r3,32(r1); cmplw r3,r5`) and keeps it into the update branch, while ours loads it into
`r0` and reloads `r3` in the branch target — one extra instruction, 432 B against 428. The
update branch itself (0x800A147C: `addi r0,r3,4; cmplw r0,r31` for the self-check, then the
valid test, then the value/`stb` stores) is the repo's `optional_object::operator=` un-inlined
and already matches; the `else` branch's `b 12c8` in retail is our `b` to the epilogue.

Spellings measured with `tools/bytescmp.py` against 0x800A1330 (retail 428 B), so nobody repeats
them:

| spelling | ours | note |
|---|---|---|
| inline `SRiders` probe via `rstl::find`, timer forwarded by reference | 408 B | frame 336 vs 352; retail **copies** the timer to the stack first, so this cannot be right |
| + `rstl::optional_object< float >(decayTimer)` (the one kept) | **432 B** | frame 352, every stack slot and every call site identical; only the `r0`/`r3` allocation left |
| named `end` local compared against `find`'s result | 444 B | whole prologue reordered, worse |
| hand-written `while` loop instead of `rstl::find` | 352 B | drops the `SRiders` probe entirely, frame 256 |
| `if (slave != end) { update } else if (actor) { ... }` | 432 B | same single instruction, opposite branch polarity |
| `SRiders probe` as a named local | 432 B | probe lands at `r1+256` instead of `r1+208` |

## Verified

```
sha1sum build/G2ME01/main.dol       -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/decomp_build.sh             -> All: 30.84% fuzzy, 23.11% matched, 11.74% linked (10014 / 28465 functions)
./tools/probe_sources.sh            -> probe: 749 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)
python3 tools/check_symbol_names.py -> checked 503 units; 0 declared names are missing
python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CScriptPlatform -> ok, none out of retail order
./tools/gate.sh build/goal/judge/report.base.json
```

`tools/bytescmp.py` per function (only the `bl` relocation field differs in each, which objdiff
resolves by symbol name and scored 100%):

```
fn_800A14DC  0x800A14DC  100 B   1 of  25 instructions differ (the bl)
fn_800A46F0  0x800A46F0  184 B   3 of  46 instructions differ (three bls)
fn_800A47A8  0x800A47A8  152 B   1 of  38 instructions differ (the bl)
```

`gate.sh` is ok on every step except `docs claims`, which reports only

```
missing: 'matched    10014 / 28465 functions'  (HANDOFF state block: total matched)
missing: 'DOL units  8603 / 16726 functions'  (HANDOFF state block: DOL matched)
```

Those are the derived state-block numbers this change moved. The judge rewrites them itself —
`tools/gate.sh:115` runs `check_docs_claims.py ${MP_GATE_DOCS_WRITE:+--write}` and
`goal_check.sh` invokes the gate with `MP_GATE_DOCS_WRITE=1` — so per the brief I did not
hand-edit `docs/HANDOFF.md`.

Diff is one file, `src/MetroidPrime/ScriptObjects/CScriptPlatform.cpp`; no `.s`, no `tools/`,
no `docs/`, no `build/goal/`. Not committed.

## For whoever takes the rest of this unit

- **DOL address = 0x800A0200 + object offset** for this unit. Getting that wrong makes
  `tools/bytescmp.py` compare against the previous function and print a long, plausible,
  completely bogus diff — it cost me one round here. `build/report.json`'s per-function
  `metadata.virtual_address` gives the address directly.
- `BuildSlaveList` (0x800A2934, 468 B) and `AddRider(vector)` (0x800A38D0, 660 B) are the other
  two callers of `fn_800A14DC` and are still the `// TODO` bodies. Both now have their callees
  (`fn_800A14DC`, `fn_800A46F0`) defined, so they are the next real work in this unit.
- `tools/unit_fit.sh` reports 28 functions (3048 B) in our object that retail's does not have —
  all of them COMDAT template instantiations (`erase<...>`, the `vector` copy constructors,
  `__ct__11CMayaSplineFRC11CMayaSpline`, the `optional_object`/`single_ptr` destructors). None
  of the three new functions is among them. The unit is at 23/60 and cannot be flipped in this
  state; this item does not try.
- Still blocked, unchanged from the previous item's notes: `__dt__` (77.78%) needs the member
  order above 0x424 re-derived (`progress-prime1-cscriptplatform-dtor`), and the erase chain
  `fn_800A1004 -> fn_800A1050 -> fn_800A1148 -> fn_800A1180` is still missing
  (`progress-prime1-cscriptplatform-ridervec`).

## NEW:

NEW: progress-prime1-cscriptplatform-callers | progress | MetroidPrime/ScriptObjects/CScriptPlatform | fn_800A14DC and the reserve chain (fn_800A46F0/fn_800A47A8) now match at 100% and AddSlave is written at 98.83% (one instruction: retail keeps the found iterator in r3 across the branch at 0x800A11C8, ours reloads it), but the other two callers - BuildSlaveList (0x800A2934, 468 B) and AddRider(vector) (0x800A38D0, 660 B) - are still the TODO bodies, so five more functions in this unit are one written body each from counting
