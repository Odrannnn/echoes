# progress-vt-rel-scriptsafezone — module:ScriptSafeZone, +3 functions (7 -> 10)

## Result

Three new `Matching` units, all verified by `tools/flip_test.sh`, all inside the module's one
remaining unclaimed range. The module's `.rel` is **byte-identical to retail** and its sha1 still
matches `config/G2ME01/config.yml`:

    $ cmp build/G2ME01/ScriptSafeZone/ScriptSafeZone.rel orig/G2ME01/files/RelProd/ScriptSafeZone.rel
    (no output)
    $ sha1sum build/G2ME01/ScriptSafeZone/ScriptSafeZone.rel
    0c63ccf483d04844250fe193644a09ba16be92b2   # what config.yml records for module 66

`goal_check.sh build/goal/item.json` → **PASS** (run after the third unit landed):

    goal_check: item progress-vt-rel-scriptsafezone (progress) target=module:ScriptSafeZone
      ok    no judge-owned path touched
      ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
      ok    counts: matched 13355 -> 13358   linked 6403 -> 6406
      ok    check_symbol_names.py
      ok    All:  37.41% fuzzy, 30.85% matched, 13.71% linked (13358 / 28465 functions)
      ok    target rose: module:ScriptSafeZone: 7 -> 10 / 128 functions
      ok    no asm added
    goal_check: PASS progress-vt-rel-scriptsafezone

`total_functions` is still **28465** after the `splits.txt` edit, and the gate's per-function diff
calls the unit reshuffle a split, not a loss:

    per-function diff  SPLIT  ScriptSafeZone/.../CScriptSafeZoneTail: 32 function(s) moved into
      CScriptSafeZoneHealth, CScriptSafeZoneVulnerability, auto_00_000067CC_text,
      auto_00_00007750_text (exact count match - a split, not a loss)

## What I added

| unit (configure.py) | claimed range | function | body |
| --- | --- | --- | --- |
| `.../CScriptSafeZonePrefix.cpp` | `.text 0x0..0x2C` (44 B) | `fn_66_0` | CActor `GetHealthInfo` slot: dispatch vtable offset 0x38 |
| `.../CScriptSafeZoneVulnerability.cpp` | `.text 0x67C4..0x67CC` (8 B) | `fn_66_67C4` | `addi r3,r3,0x208 / blr` |
| `.../CScriptSafeZoneHealth.cpp` | `.text 0x7748..0x7750` (8 B) | `fn_66_7748` | `addi r3,r3,0x1E8 / blr` |

The module before this item had 7 matched functions (2 in `CScriptSafeZone` = RELExit/RELMain, 5 in
`REL_Setup`) and 120 unclaimed; it has 10 now.

`fn_66_0` is the head unit that already existed in the split with **no source**: the range is one
function and nothing had claimed it. Its bytes are word for word `fn_71_0` / `fn_56_0` / `fn_20_0`
in SnakeWeedSwarm / Sandworm / FishCloud, all of which are already `Matching` in this tree, so the
spelling `CHealthInfo* fn_66_0(CScriptSafeZoneVTable* self) { return self->Slot12(); }` is measured
three times over and not guessed. Measured here too:

    $ ./tools/flip_test.sh MetroidPrime/ScriptObjects/CScriptSafeZonePrefix.cpp
      PASS  -> kept as Matching
    $ ./tools/unit_fit.sh MetroidPrime/ScriptObjects/CScriptSafeZonePrefix.cpp
       .text      claimed     44   ours     44   retail     44   fits
       no extra functions: our object defines only what the retail unit object does

The other two are vtable entries found by `tools/rel_class_map.py`: both live in `lbl_66_data_B8`
(`.data:0xB8`, 0x7C bytes = 31 words → 29 virtuals), whose base is `6CActor`. `fn_66_67C4` is word
16 (offset 0x40) = `GetDamageVulnerability__6CActorCFv`; `fn_66_7748` is word 15 (offset 0x3C) =
`HealthInfo__6CActorFv`. Both are also in `lbl_66_data_10`, so **no `force_active:` entry is needed
in `config/G2ME01/config.yml`** - the module's `.data` references them, which is what kept
`fn_66_0` too.

Both accessor units are `addi/blr` over a **modelled** class layout (a `char mPad[n]` and the one
member the accessor names), the same arrangement as `CModelDataModelSlots.cpp`, and they are
`Matching` without the rest of the class. The offsets are cross-checked by two neighbours, so they
are not free choices: `fn_66_772C` (28 B, immediately below `fn_66_7748`) loads
`*(float*)(self+0x1E8)` and stores it at `*(float*)(self+0x1EC)`, which is `healthA` into `healthB`
in `CHealthInfo`'s own order, and 0x1E8 + `CHECK_SIZEOF(CHealthInfo, 0x20)` = 0x208 exactly, where
`fn_66_67C4` reads. `CActor` here is `CHECK_SIZEOF(CActor, 0x158)`, so the 0x90 bytes between are
this module's own and no header names them.

## The carve (four files, in one change)

- `config/G2ME01/rels/ScriptSafeZone/splits.txt` — `CScriptSafeZoneTail.cpp` shrinks
  `.text 0x70..0x8C60` → `0x70..0x67C4` and keeps `.ctors 0x0..0x4`; the two new claims go in
  ascending order; `0x67CC..0x7748` and `0x7750..0x8C60` are **left unclaimed** (dtk fills them),
  the Metaree/Splitter arrangement. `total_functions` checked at 28465 after the edit.
- `configure.py` — three `Object(Matching, ...)` lines, one per line, in the `Rel("ScriptSafeZone")`
  list, with a comment naming every range and why the rest stays retail.
- `files.cmake` — the three sources listed (none defines `RELMain`/`RELExit`, so
  `tools/check_files_cmake.py` requires them to be here; it passes).
- the three sources themselves.

## Not taken, with the measurements so the next run does not repeat them

**`fn_66_4B90` (0x4B90, 32 B) and `fn_66_27E0` (0x27E0, 48 B) both `bl 0x8C60`, and 0x8C60 is this
module's own `_unresolved`** - `REL/REL_Setup.cpp`'s OSReport stub
(`"\nError: Unlinked function called in module %s.\n"`). Retail left the imported callee
unresolved and pointed it at the stub, so there is no symbol to call: a C++ spelling would be a
call to a function that does not exist and mwldeppc would fail the link with `undefined:`, not
reproduce `bl 0x8C60`. `fn_66_27E0` is `SetActive`-shaped around it (`mr r3,r4`, then
`neg/or/srwi` to normalise the callee's bool return to 0/1), `fn_66_4B90` is a bare pass-through.
No way to write either without inventing a callee.

**`fn_66_772C` (0x772C, 28 B), the function immediately below `fn_66_7748`**, is the obvious next
claim (it would make `CScriptSafeZoneHealth.cpp` a two-function unit, 0x772C..0x7750). Its bytes:

    lfs f0, 488(r3)        ; *(float*)(self + 0x1E8)  -> healthA
    li r4, 0
    stfs f0, 492(r3)       ; *(float*)(self + 0x1EC) = f0 -> healthB = healthA
    lbz r0, 444(r3)        ; byte at 0x1BC
    rlwimi r0, r4, 6, 25, 25   ; clear bit 7 of that byte
    stb r0, 444(r3)
    blr

I did not attempt it, and it is not a wall - I did not measure a single spelling. The obstacle is
the **first float of the record has no accessor in this tree**: `CHealthInfo` declares
`healthA`/`healthB` private with `SetHP`/`GetHP` on `healthB` only
(`include/MetroidPrime/CHealthInfo.hpp`), so `SetHP(GetHP())` is not the copy retail makes and any
line I wrote for it would be invented rather than measured. The `rlwimi` half is reachable -
`CActorSetDirtyFlags.cpp` records that mwcceppc packs a 32-bit bitfield group MSB-first and emits
`mb = 24 + index within the byte`, and `SH = 31 - mb` (`rlwimi r0,r4,6,25,25` is mb 25, so the field
is the second one-bit field of the group at 0x1BC) - but the float copy has to be modelled first,
and that is a header change belonging to another item.

## Where the names come from

`tools/rel_class_map.py ScriptSafeZone` reads two vtables out of the module's relocation table:

    vtable sec 5 +0x10: 37 slots, 14 defined here; base 23CScriptTriggerEllipsoid (22 agree)
    vtable sec 5 +0xB8: 29 slots, 10 defined here; base 6CActor (19 agree)

Neither module class is named on disc, so the two new accessors are declared as members of a local
layout struct rather than as `CSafeZone`'s methods. That is a choice and it is recorded as one in
each file's comment; the offsets are measurements and the member *names* are guesses. Correcting
`CActor.hpp`'s layout to carry the 0x90 bytes these records live after is a port-side model change
and out of scope here.

## Commands

    export MP_TOOLCHAIN_DIR=/run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrimePort
    ./tools/decomp_build.sh ScriptSafeZone
    ./tools/flip_test.sh MetroidPrime/ScriptObjects/CScriptSafeZonePrefix.cpp        # PASS
    ./tools/flip_test.sh MetroidPrime/ScriptObjects/CScriptSafeZoneVulnerability.cpp # PASS
    ./tools/flip_test.sh MetroidPrime/ScriptObjects/CScriptSafeZoneHealth.cpp        # PASS
    ./tools/unit_fit.sh <each of the three>   # .text fits, no extra functions, every time
    python3 tools/check_decl_order.py --unit CScriptSafeZone   # 4 units, none permuted
    ./tools/goal_check.sh build/goal/item.json               # PASS

Note for the driver: `goal_check.sh` runs `gate.sh` with `MP_GATE_DOCS_WRITE=1`, which rewrites the
derived counts in `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md`. I reverted both files after the
check, as the brief requires; the state block in `docs/HANDOFF.md` therefore still reads the
pre-item figures (13355 matched / 6403 linked, ScriptSafeZone 7).