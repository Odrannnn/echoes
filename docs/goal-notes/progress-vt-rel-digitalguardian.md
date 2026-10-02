# progress-vt-rel-digitalguardian

`progress` on `module:DigitalGuardian`. The module's summed `matched_functions` went
**26 -> 36** (judge: `target rose: module:DigitalGuardian: 26 -> 36 / 420 functions`), the
project's `matched_functions` **13382 -> 13392**, `linked` **6430 -> 6440**, all 86 REL
sha1s in `config/G2ME01/config.yml` still hold, and `main.dol`'s is untouched. Ten new
functions across seven new units, all byte-exact and all `Matching`.

## What was added

Ten of the twelve candidates the item named. Each is a virtual this module's own class
defines; all ten were found by reading `build/G2ME01/DigitalGuardian/asm/*.s`, and each claim
covers one contiguous range whose neighbours stay unclaimed so dtk fills them from retail.

| unit | range | functions |
| --- | --- | --- |
| `DigitalGuardianScannable.cpp` | `0x9DBC..0x9DFC` | `fn_14_9DBC` (`GetScannableObjectInfo`) |
| `DigitalGuardianVulnerability.cpp` | `0x9FE4..0xA004` | `fn_14_9FE4` (`GetDamageVulnerability`) |
| `DigitalGuardianPreRender.cpp` | `0xA384..0xA3A4` | `fn_14_A384` (`PreRender`) |
| `DigitalGuardianPreThink.cpp` | `0xA688..0xA6A8` | `fn_14_A688` (`PreThink`) |
| `DigitalGuardianTouchBounds.cpp` | `0xD774..0xD7DC` | `fn_14_D774` (`GetTouchBounds`), `fn_14_D7BC` (`GetDamageVulnerability`) |
| `DigitalGuardianDoorWrappers.cpp` | `0x179E0..0x17A64` | `fn_14_179E0`, `fn_14_17A00` (`AddToRenderer`), `fn_14_17A44` |
| `DigitalGuardianStateVulnerability.cpp` | `0x1A12C..0x1A168` | `fn_14_1A12C` (`GetDamageVulnerability`) |

Carve is the usual four: `configure.py` (seven `Object(Matching, ...)` entries),
`config/G2ME01/rels/DigitalGuardian/splits.txt`, `files.cmake`, and the sources. **No
`config/G2ME01/splits.txt` edit**, so `total_functions` is untouched at 28465.

Seven units for ten functions because the module recipe allows one contiguous range per unit
and only two of these ranges hold more than one function. Each neighbouring function is
unclaimed, so dtk fills it from retail and the module's sha1 is unaffected by what the ranges
do not cover.

**Everything is inside `#ifdef __MWERKS__`.** Every one of these units' only relocation is a
call to a DOL function the port does not define (`PassThruVulnerability__20CDamageVulnerabilityFv`,
`PreRender__10CPatternedFR13CStateManager`, `PreThink__10CPatternedFfR13CStateManager`,
`AddToRenderer__10CPatternedCFRC13CStateManager`, `Render__10CPatternedCFRC13CStateManager`,
`GetScannableObjectInfo__10CPatternedCFv`), so the guard keeps the host objects empty and the
port's undefined count where it was - the same arrangement `DigitalGuardianDestroy.cpp`
records. The bodies also define no `RELMain`/`RELExit`, so `check_files_cmake.py`'s
`MODULE_ENTRY` exemption is not needed for any of them.

## What the three non-trivial functions are

The other seven are pure forwarders (prologue, one or two calls, epilogue) and came out of the
first spelling tried. These three did not.

### `fn_14_9DBC` - two things had to be right, and one of them is invisible

The body is a select on one flag bit, and retail reads it as `lbz r0,0x128c(r3)` +
`extrwi. r0,r0,1,25`. That is **a byte, not a word**, holding a packed struct, and the tested
bit is the **second declared `bool : 1`** of it. Measured here, each with this unit's exact
MWCC command line and the retail 0x40 bytes to compare: third `bool` gives `extrwi` shift 27,
fourth 28, fifth 30, sixth 31, seventh a `clrlwi`; a `u32` storage unit at the same offset
gives `lwz 0x1290` instead of `lbz 0x128c` because the struct is then 4-aligned. This is
the same encoding `CLumiteRel.cpp` records for the `+0x34C` flag family
(`extrwi r3,r0,1,28`, the fourth `bool : 1` of its byte), one position along.

The second thing is the **shape of the select**. Retail has *both* tests branching forward to
the base call (`beq` / `cmplwi`+`beq` to the same label), with the module's own answer reached
over one `b` past it. That is a nested `if` whose body is the early return:

```cpp
if (bit) { if (ptr) { return ptr->x8_link; } }
return base();
```

The `||`-guard spelling - `if (!bit || !ptr) { return base(); } return ptr->x8_link;` -
produces the *same instructions* but with the two branches the other way round (`bne` and `b`
swapped), so it is not byte-exact. Worth knowing: this pair is indistinguishable by
instruction count and by fuzzy score, and only `cmp` on the `.rel` catches it.

### `fn_14_1A12C` - the state member is signed

Same two-armed select as the vulnerability forwarders, but the member arm is a
`this`-relative address (r3 is `this + 0x734` and never reloaded), so the return is a pointer
and the function is 0x3C bytes. Retail's two compares are **`cmpwi`**, the signed form. The
same body over a `u32` member compiles to `cmplwi` - one byte in each of the two compares,
and the module's sha1 with it. It is the only difference; the `x == 1 || x == 3` spelling
produced retail's `cmpwi` / `beq` / `cmpwi` / `bne` arrangement as written.

### `fn_14_D774` - the twin of an existing Matching unit

`fn_14_D774` (0x48 bytes) is `fn_14_1AB50` - already `DigitalGuardianContact.cpp` and
`Matching` - with this module's *other* offset set (0x164 rather than 0x718), because the
module has two classes. So the two-8-byte-`union`-of-two-`u32` record from that unit is
correct here too, and the same measurement (two loads and two stores per 8 bytes, ascending)
holds. `fn_14_D7BC`, the 0x20 bytes after it, is a third copy of the pure
`GetDamageVulnerability` forwarder.

**One byte cost the module its hash and nothing else caught it.** The scannable's record was
first declared with a single `u32` ahead of `x8_link`, which put the read at `+4` instead of
`+8`. That built, objdiff reported **100.00% fuzzy / 100.00% matched**, and only the
`build/G2ME01/ok` checksum rule (`DigitalGuardian.rel: FAILED`, 1 byte at file offset 40615)
failed. A lesson worth keeping: on a REL carve, `cmp` the `.rel` - the per-unit percentage is
not a byte check at this granularity.

## Not done

- `fn_14_10C` (0x10C..0x138, 0x2C bytes) - a **vtable dispatch**, not a forward:
  `lwz r12,0x0(r3)` / `lwz r12,0x38(r12)` / `mtctr` / `bctrl`. Reproducing it needs the
  module's own class declared with the vtable in the right slot order, which is a different
  kind of work from a forwarder and is the whole of what the other candidates did not need.
- `fn_14_1AB98` (0x1AB98..0x1AD8, 0x40 bytes) - also a vtable dispatch, this one
  `lwz r12,0x54(r12)` after `lwz r12,0x0(r4)` with `f1` preloaded from `.rodata`, i.e. it
  forwards to slot 21 of *another* object's vtable.

Both are in the module's `ldscript.lcf` FORCEACTIVE list, so neither is dead-stripped, and
both have their neighbours unclaimed. They are left in `auto_00_0000010C_text` and
`auto_00_0001AB98_text` respectively.

## Verified

```
./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13382 -> 13392   linked 6430 -> 6440
  ok    check_symbol_names.py
  ok    All:  37.45% fuzzy, 30.89% matched, 13.75% linked (13392 / 28465 functions)
  ok    target rose: module:DigitalGuardian: 26 -> 36 / 420 functions
  ok    no asm added
goal_check: PASS progress-vt-rel-digitalguardian
```

No `NEW:` lines filed: both remaining candidates are the vtable-dispatch case, which needs a
class declaration rather than more spellings, and that is not a bounded follow-up item.