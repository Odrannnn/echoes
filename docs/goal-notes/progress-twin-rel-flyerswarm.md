# progress-twin-rel-flyerswarm

**Result: PASS.** `./tools/goal_check.sh build/goal/item.json` -> `PASS
progress-twin-rel-flyerswarm`. `module:FlyerSwarm` matched functions **9 -> 17 / 43**, project
`matched_functions` **13300 -> 13308**, `linked` **6348 -> 6356**; the module's sha1 is unchanged
(`e8205e9220dfe2bacd6173fa095c009ddc56f715`, `build/G2ME01/FlyerSwarm/FlyerSwarm.rel`), all 86
module hashes hold and every `.rel` is `cmp`-equal to `orig/G2ME01/files/RelProd/`, `main.dol` is
still `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.

```
$ ./tools/goal_check.sh build/goal/item.json
goal_check: item progress-twin-rel-flyerswarm (progress) target=module:FlyerSwarm
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13300 -> 13308   linked 6348 -> 6356
  ok    check_symbol_names.py
  ok    All:  37.35% fuzzy, 30.78% matched, 13.65% linked (13308 / 28465 functions)
  ok    target rose: module:FlyerSwarm: 9 -> 17 / 43 functions
  ok    no asm added
goal_check: PASS progress-twin-rel-flyerswarm
```

## What landed

One new `Matching` unit, `MetroidPrime/ScriptObjects/CFlyerSwarmRelTail.cpp`, claiming **`.text
0x1708..0x198C`** - eight functions, one contiguous run, all 8/8 at 100.00% in `build/report.json`
(`complete_units: 1`):

| retail | fn | size | what it is |
| --- | --- | --- | --- |
| 0x1708 | `fn_21_1708` | 0x8C | the count+array vector's deleting destructor (`destroy_elements`, then `Free` on the flag) |
| 0x1794 | `fn_21_1794` | 0x44 | that vector's copy constructor |
| 0x17D8 | `fn_21_17D8` | 0x68 | `uninitialized_copy_n` for the 0x24-byte element |
| 0x1840 | `fn_21_1840` | 0x20 | `construct`'s forwarder |
| 0x1860 | `fn_21_1860` | 0x28 | `construct_impl` (`new (place) Element(src)`) |
| 0x1888 | `__ct__10SFlyerElemFRC10SFlyerElem` | 0x4C | the element's out-of-line copy constructor (**renamed** - see below) |
| 0x18D4 | `fn_21_18D4` | 0x58 | a second deleting destructor, tears down the member at +0x18 |
| 0x192C | `fn_21_192C` | 0x60 | `CFlyerSwarm`'s deleting destructor (stores `lbl_21_data_4`, calls the imported base dtor `fn_80_89F4`) |

Files: `src/MetroidPrime/ScriptObjects/CFlyerSwarmRelTail.cpp` (new);
`config/G2ME01/rels/FlyerSwarm/splits.txt` (the claim); `configure.py` (one `Object(Matching, ...,
mw_version="GC/2.7")` in the existing `Rel("FlyerSwarm", ...)` block);
`config/G2ME01/rels/FlyerSwarm/symbols.txt` (one rename: `fn_21_1888` ->
`__ct__10SFlyerElemFRC10SFlyerElem`); `config/G2ME01/config.yml` (FlyerSwarm gains
`force_active: [fn_21_1708]`); `files.cmake` (one line + comment);
`docs/research/raw_offsets.md` (one new section; `check_raw_offsets.py` requires it - see below).

**`FlyerSwarm` had no `force_active:` key before this change and now has one.** `fn_21_1708` is the
only function in this range nothing references - not the module's code and not its `.data` - so
mwldeppc drops a claimed copy of it and every function after it shifts (the SandBoss and
AIMannedTurret entries in `RUNNING_THE_DECOMP.md` record the same failure). Verified by the sha1,
not by objdiff: the module is byte-identical to retail with the key in place.

## The three things that had to be measured, in the order they blocked

1. **`mw_version="GC/2.7"` is the whole of two of the eight, and it is the same finding
   `CLumiteRelTail.cpp` and `CSandBossRelTail.cpp` already carry.** Under the module's default
   **GC/1.3.2**, `fn_21_1794` puts retail's `lwz r0,0x0(r4)` four instructions later than retail
   (17 instructions, otherwise identical - a scheduling difference, no spelling reaches it) and
   the element copy at 0x1888 is a nine-`lwz/stw` **word** copy. Under 2.7 both are instruction for
   instruction retail's, with no spelling change at all. This family's out-of-line library blocks
   are the later compiler's - **try 2.7 before spending spellings on a REL tail that only differs
   in prologue scheduling.**
2. **One of the eight is a copy constructor, and no free-function spelling reaches its bytes.**
   `fn_21_1888` is 0x4C bytes copying six floats with **two registers in flight**
   (`lfs f1,0 / lfs f0,4 / stfs f1,0 / lfs f1,8 / ...`) and then three words with the middle one
   through `r5`. Measured, all wrong: a plain struct assignment (sequential `lfs f0/stfs f0`, six
   pairs), an explicit memberwise assignment in retail's order (same, plus the compiler's own
   schedule), and `new (dest) Element(src)` (adds the `new` expression's `cmplwi r3,0 / beqlr`
   test, which retail does not have - so retail's source is *not* a placement `new`). The source
   those bytes come from is the element's own out-of-line copy constructor, which cannot be named
   `fn_21_1888`, so `symbols.txt` renames the retail symbol to the mangled name our object emits -
   the practice `RUNNING_THE_DECOMP.md` records for `CAi` ("rename every function in the range to
   the mangled name our object emits"). `fn_21_1860` then spells it as `new (place) Element(src)`,
   whose null test is the `new` expression's own and which is byte-identical to the `if (place)
   call(place, src)` form that already matched. Its twin is the DOL's
   `__ct__Q212CAreaOctTree4NodeFRCQ212CAreaOctTree4Node` (`WorldFormat/CAreaOctTree_Tests.cpp`),
   instruction for instruction.
3. **`fn_21_1708` is only reachable by modelling the class, not by writing its loop.** Its 0x8C
   bytes are the compiler's codegen for `destroy_elements()` over an element whose destructor is
   empty but user-declared, and the register assignment is the whole of the diff: retail keeps the
   count in **r6** and the cursor in **r3**; seven spellings of the same loop as a free function
   (member-index loop, pointer loop, empty body, `int n` first, `unsigned`, `rstl::destroy`,
   pointer + counter in both orders) all put the count in **r5** and the cursor in **r6**. Declaring
   the class with `destroy_elements()` written the way `include/rstl/reserved_vector.hpp` writes it
   and calling it from the free function reaches r6/r3 and matches. This is the item's own hint
   ("a twin that is a member function only matches written as a member of a declared class") with
   the measurement behind it.

## The element and the vector, and what is *not* claimed

The module's 0x24-byte record (six floats, three words) is not declared anywhere in this tree - the
module reaches it as a raw pointer in `CFlyerSwarm.cpp` and in the unclaimed code at 0x16AC - so the
file declares it (`SFlyerElem`) only as far as the copy, the loop and the destructor need, and
`SFlyerVec` only as the count at +0 with the array behind it. **The element copy is a member copy
constructor, so the record's *type* is still unidentified**: a later run that finds the real class
(SwarmBasics's boid record is 0xB8 bytes, `CFlyerSwarm.cpp`'s accessor, so not this one) can move
both declarations into the real header and keep the bytes.

One raw offset, `fn_21_18D4`'s `reinterpret_cast<char*>(self) + 0x18` - **kind A, opaque receiver**
(the function takes a `void*` and the class is not modelled anywhere), the same shape
`src/MetroidPrime/Player/CMorphBall.cpp`'s `fn_800CD460` carries verbatim with the same `+24`. The
gate's `check_raw_offsets.py` fails a new file with an undocumented site, so
`docs/research/raw_offsets.md` gained a section and its re-derived total line (tool prints
`186 raw-offset site(s) in 80 file(s)`).

## Gates, all measured on this tree

```
$ python3 - <<'PY'   # 86 modules: built sha1 vs config.yml vs orig, and cmp
modules checked: 86  mismatches: []  missing: []
$ ./tools/unit_fit.sh MetroidPrime/ScriptObjects/CFlyerSwarmRelTail.cpp
   .text      claimed    644   ours    644   retail    644   fits
   no extra functions: our object defines only what the retail unit object does
$ python3 tools/audit_rel_claim.py FlyerSwarm
ok   .../CFlyerSwarmRelTail.cpp   0x00001708..0x0000198C  8/8 functions
0 claim(s) with a problem
FlyerSwarm: preplf 43 text symbols, plf 43, 0 dropped by -strip_partial
$ ./tools/probe_sources.sh  (via gate.sh)
probe: 852 files, 0 failed, 0 errors; link: LINKED (286 undefined, 0 duplicates)
$ python3 tools/check_symbol_names.py
checked 585 units; 0 declared names are missing from their object
$ python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CFlyerSwarmRelTail.cpp
ok: 0 unit(s) checked, none emits its functions out of retail order   # REL units are not resolved by this tool; the sha1 is the check
```

`build/report.json` after the change: `FlyerSwarm/MetroidPrime/ScriptObjects/CFlyerSwarmRelTail`
**8/8, 100.00% fuzzy, `complete_units: 1`**; the module's auto unit
`auto_00_000000D8_text` went 33 -> 21 functions and a new `auto_00_0000198C_text` (4 functions)
holds the rest; `total_functions` is still **28465**; project `matched_functions` **13308**.

## Follow-ups (measured, not attempted)

`python3 tools/twin_scan.py --list | grep ^FlyerSwarm/` now lists **11** twins left in
`auto_00_000000D8_text`; the contiguous runs where *every* function has a twin are

| range | fns | bytes | what |
| --- | --- | --- | --- |
| 0x1B0C..0x1C58 | 3 | 332 | `fn_21_1B0C` (0x3C, `__dt__5CMainFv`), `fn_21_1B48` (0x94, `__ct__16CActorParametersFRC16CActorParameters`), `fn_21_1BDC` (0x7C, `__ct__16CLightParametersFRC16CLightParameters`) - the last two are *copy constructors* of parameter objects, the same class of work item 2 above, and it ends exactly at `_unresolved` (0x1C58) |
| 0xCAC..0xD08 | 2 | 92 | `fn_21_CAC` (0x3C, `__dt__5CMainFv`), `fn_21_CE8` (0x20, `__sys_free`) |
| 0x1664..0x16AC | 2 | 72 | `fn_21_1664` (0x20, `__sys_free`), `fn_21_1684` (0x28, `construct_impl<CPASAnimState>`) |

Of these only `fn_21_CE8` is in the module's `ldscript.lcf` FORCEACTIVE list, so each claim needs
its own `config.yml` `force_active:` entries - which is now a known, measured step for this module.
The single-function twins left are 0x14E4, 0x15B8, 0x59C and 0x5D8. Nothing was left at a sub-100%
score, so there is no `WALL:` line.

`NEW:` - none filed. The remaining work is the same target (`module:FlyerSwarm`) this item already
names, and its shape is recorded above and in the new file's header; the digital-guardian/emperoring
precedent is that the driver requeues the id rather than a new one being seeded.

Note for the driver: `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` show as modified in
`git status` - that is `goal_check.sh`'s own `MP_GATE_DOCS_WRITE=1 ./tools/gate.sh` rewriting the
derived counts (`matched 13308`, `linked 6356`, REL units `1773`, probe `852`), not an edit of mine.
