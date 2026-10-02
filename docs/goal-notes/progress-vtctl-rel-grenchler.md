# progress-vtctl-rel-grenchler - module:Grenchler, the vtable-name trial's control

Lane 13, 2026-10-02. Result: **PASS** (`./tools/goal_check.sh build/goal/item.json`).

```
goal_check: item progress-vtctl-rel-grenchler (progress) target=module:Grenchler
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13352 -> 13368   linked 6400 -> 6416
  ok    check_symbol_names.py
  ok    All:  37.42% fuzzy, 30.85% matched, 13.72% linked (13368 / 28465 functions)
  ok    target rose: module:Grenchler: 25 -> 41 / 377 functions
  ok    no asm added
goal_check: PASS progress-vtctl-rel-grenchler
```

**`module:Grenchler` 25 -> 41**, i.e. **16 functions in five new `Matching` units**, with the
module's sha1 still `de128c91f467b5b61b4de4a2ed6b217044a975c5` (exactly what
`config/G2ME01/config.yml:177` records) and `build/G2ME01/Grenchler/Grenchler.rel` `cmp`-identical
to `orig/G2ME01/files/RelProd/Grenchler.rel`.

| unit | range | fns | bytes | what it is |
| --- | --- | --- | --- | --- |
| `CGrenchler1F18.cpp` | 0x1E74..0x1FAC | 6 | 0x138 | two deleting destructors (`fn_27_1E74`, `fn_27_1ED0`, each storing the module's own 12-byte `.data:0xEA0`/`.data:0xEAC` vtable) and four virtual predicates |
| `CGrenchler8F34.cpp` | 0x8F34..0x9018 | 5 | 0xE4 | a deleting destructor whose only non-trivial member is an `rstl::string` at +0x2C, three one-bit virtual predicates and `fn_27_8FC0` |
| `CGrenchlerAC5C.cpp` | 0xAC5C..0xACF0 | 2 | 0x94 | the two virtuals the item named: `fn_27_AC5C` (vtable slot 18) and `fn_27_AC9C` (slot 21) |
| `CGrenchlerD104.cpp` | 0xD104..0xD178 | 3 | 0x74 | `fn_27_D104`, `fn_27_D10C` (the empty `FluidFXThink` override, slot 23) and `fn_27_D110` |
| `CGrenchlerRel.cpp` (pre-existing) | 0x0..0x168 | 18 | 0x168 | untouched |

Of the item's twelve named candidates, three landed (`fn_27_D10C`, `fn_27_AC5C`, `fn_27_AC9C`);
the other thirteen functions came with them, because a claim is one contiguous range and the
neighbours are the same shape. **The item's "smallest first" order is not the cheapest order**:
`fn_27_D10C` (4 bytes) sits in a three-function run with an 8-byte leaf that needed a
`force_active:` entry, and the largest single win here (6 functions) is a run the list does not
mention at all.

Files: the four new sources above, `config/G2ME01/rels/Grenchler/splits.txt` (four entries, in
address order), `configure.py` (four `Object(Matching, ...)` lines in the existing `Rel("Grenchler")`
block), `files.cmake` (four lines with comments), and `config/G2ME01/config.yml` (two
`force_active:` entries). Nothing under `tools/`, `build/goal/` or `docs/research/`.

## Measured, not recalled

```
$ sha1sum build/G2ME01/Grenchler/Grenchler.rel orig/G2ME01/files/RelProd/Grenchler.rel
de128c91f467b5b61b4de4a2ed6b217044a975c5  build/G2ME01/Grenchler/Grenchler.rel
de128c91f467b5b61b4de4a2ed6b217044a975c5  orig/G2ME01/files/RelProd/Grenchler.rel
$ cmp build/.../Grenchler.rel orig/.../Grenchler.rel          # identical, exit 0

$ python3 tools/audit_rel_claim.py Grenchler
ok   MetroidPrime/ScriptObjects/CGrenchlerRel.cpp   0x00000000..0x00000168  18/18 functions
ok   MetroidPrime/ScriptObjects/CGrenchler1F18.cpp  0x00001E74..0x00001FAC   6/6 functions
ok   MetroidPrime/ScriptObjects/CGrenchler8F34.cpp  0x00008F34..0x00009018   5/5 functions
ok   MetroidPrime/ScriptObjects/CGrenchlerAC5C.cpp  0x0000AC5C..0x0000ACF0   2/2 functions
ok   MetroidPrime/ScriptObjects/CGrenchlerD104.cpp  0x0000D104..0x0000D178   3/3 functions
ok   REL/global_destructor_chain.c                  0x00015738..0x000157AC   2/2
ok   REL/REL_Setup.cpp                              0x000157AC..0x00015950   5/5
0 claim(s) with a problem
Grenchler: preplf 377 text symbols, plf 377, 0 dropped by -strip_partial

$ tools/unit_fit.sh  (each of the four)
CGrenchler1F18.cpp  claimed 312  ours 312  retail 312  fits / no extra functions
CGrenchler8F34.cpp  claimed 228  ours 228  retail 228  fits / no extra functions
CGrenchlerAC5C.cpp  claimed 148  ours 148  retail 148  fits / no extra functions
CGrenchlerD104.cpp  claimed 116  ours 116  retail 116  fits / no extra functions

$ MP_GATE_DOCS_WRITE=1 ./tools/gate.sh   # goal_check's own run
configure ok / ninja + build.sha1 ok / hashes vs config.yml ok / report ok
per-function diff  SPLIT  Grenchler/auto_00_00000168_text: 322 function(s) moved into the four
                   units and the five auto remainders (exact count match - a split, not a loss)
module wiring ok / dol_read ok / docs claims ok / gs offsets ok / raw offsets ok
decl order ok / files.cmake ok / module order ok / port probe ok / port link gap ok
GATE PASS  f96db2d9+10 changed
```

`dtk shasum -c config/G2ME01/build.sha1` printed `87 files OK` on every run of this item;
`main.dol` was never touched (`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`). `check_raw_offsets.py`
counts no raw offset in any of the four files (every member is a named struct field);
`check_decl_order.py` reports `ok: 1134 unit(s) checked, 37 permuted, all accounted for` (REL units
are not in its scope - see below).

## The recipe that worked (reusable on the rest of this module)

Each range is a set of `extern "C"` free functions with a struct per function whose gaps are
`uchar mUnknownNN[0xNN - 0xNN]` padding, so nothing is a raw pointer offset. Callees that are
module-local or DOL symbols are declared under their **retail mangled names** (`extern "C" void
fn_27_990C(...)`, `extern "C" const void* GetDamageVulnerability__10CPatternedCFv(...)`), which is
what lets a `bl` resolve to exactly the symbol the module's own bytes use. The bodies sit inside
`#ifdef __MWERKS__` with an empty host branch, so `files.cmake` can list the file (required:
`check_files_cmake.py` fails a configured `Matching` object that is neither listed nor EXCLUDED)
without adding undefined names to the port link. Definitions are in **descending retail order** -
see the trap below.

## Six measurements worth keeping

**1. `force_active` is needed for an unreferenced function, and dtk's own FORCEACTIVE list is not
enough.** `fn_27_D104` (0xD104, 8 bytes, `lwz r3,0x254(r3); blr`) has no `bl` and no data
relocation anywhere in the module, so mwldeppc dead-stripped it and the module linked **8 bytes
short** (121460 against retail's 121468) with all three functions at 100%. `fn_27_8F34` (0x8F34,
0x5C) is the same case: the module linked exactly its 0x5C short (`.text` 0x158F4 against
0x15950). Both are now `force_active:` entries in `config/G2ME01/config.yml`'s Grenchler block.
The module's generated `build/G2ME01/Grenchler/ldscript.lcf` FORCEACTIVE block carries every
*vtable-slot* function (`fn_27_D10C`, `fn_27_8F90`, `fn_27_8F9C`, `fn_27_8FB4`, `fn_27_AC5C`,
`fn_27_AC9C`, the four predicates) but not these two. **Measure the module's `.text` size against
retail's, not just the hash** - the hash tells you it broke, the size tells you which way.

**2. A float literal in a unit emits a private `.rodata` constant and grows the module's
`.rodata`.** `fn_27_AC5C` dispatches with the module's `.rodata:0x180` (`lbl_27_rodata_180`,
`.float 0`). Written `0.f`, the object carried its own 4-byte `.rodata` (`@97`) and the module's
`.rodata` came out **0x94C against retail's 0x944** with every function at 100% and the `.text`
exact. Declaring `extern "C" const float lbl_27_rodata_180;` and passing *that* removes the local
constant and reproduces retail's `lis r6,lbl_27_rodata_180@ha / lfs f1,lbl_27_rodata_180@l(r6)`.
Same reason `CGrenchlerD104.cpp` and `CGrenchler1F18.cpp` reference `lbl_27_rodata_1C4` and
`lbl_27_rodata_168` instead of spelling numbers.

**3. `extrwi.` + `bne` over a `li r3,0; blr` is `== 0`, not `!`.** `fn_27_1F80` tests one bit of
the byte at +0xD50 and returns a comparison. Both spellings have the same value, but MWCC lays the
blocks out differently: `!mBit2` puts `return false` **at the end behind a `beq`** (57.18%,
measured), `mBit2 == 0` puts it directly after the test **behind a `bne` that jumps over it**,
which is retail (100.00%). Probed under GC/1.3.2 and GC/2.7; the two compilers agree.

**4. A `bool` returned from a float comparison is the GT bit for `>` and the LT bit for `<`.**
Retail's `fcmpo` + `mfcr r0` is followed by `rlwinm r3,r0,2,31,31` in both `fn_27_1F64` and
`fn_27_1F80`; `<` compiles to `srwi r3,r0,31` (91.43% and 57.18%, measured) and `>` to retail's
word. `k < member` is not the same as `member > k` to this compiler: it swaps the loads
(`lfs f0` for the member, `lfs f1` for the constant), which is a third wrong spelling.

**5. An explicit member destructor is what produces the `addic.` address test.** `fn_27_8F34`'s
`addic. r0,r30,0x2c / beq / addi r3,r30,0x2c / bl internal_dereference__Q24rstl66basic_string...Fv`
comes from `self->mName.~basic_string();` on an `rstl::string` member - the same mechanism
`Carve802201F8.cpp` documents. Calling the mangled `internal_dereference__...Fv` directly gives the
`addi`/`bl` without the test, and leaving it to scope exit gives no test at all. `rstl/string.hpp`
itself adds no sections to the object (probed with the unit's own flags).

**6. A vtable dispatch needs a class, and the map gives the slot but not the class.** `fn_27_AC5C`
dispatches through `lwz r12,0x54(r12)`; a stand-in class with **twenty** declared (never defined)
virtuals puts the twentieth at +0x54 and emits retail's `lwz r12,0(r4)` form, where a hand-loaded
vtable is `lwz r3,0(r3)` (`CIngPuddleRel.cpp`'s measurement). The class name never reaches the
object, so nothing has to agree with `fn_27_AB94`'s mangled name. **`rel_class_map.py`'s "base"
column is a best fit and must not be trusted as a name**: Grenchler's 234-slot table fits
`CScriptDoor` on 9 inherited slots, so its "overrides `GetOrbitPosition`" labels are plausible but
unproven; what is measured is the slot *number* and the DOL symbol each slot's neighbours name.

## Traps that fired here

**Ascending source order permutes the unit silently.** The 1F18 file was first written with
`fn_27_1E74`/`fn_27_1ED0` at the *top* (they were added second, after the four predicates).
Every one of the six scored 100.00% in objdiff, `unit_fit.sh` said `fits`, and the module's `.text`
was the right *size* - but its first word at 0x1E74 was `fn_27_1F18`'s `lwz r0,0xa78(r3)`, so the
`.rel` differed from byte 8017 (0x1E74) and only the sha1 said so. `check_decl_order.py` reports
`0 unit(s) checked` for REL units, so the module's sha1 and `cmp` are the only instruments here.

**A unit that claims a range must not add data.** `fn_27_1F18`'s two destructors store
`&lbl_27_data_EA0` / `&lbl_27_data_EAC`, which are *references* to the module's own `.data`
objects, not definitions; `nm -u` on the object shows them as undefined, which is what the module
link wants.

## What is left in this module

The item's remaining nine named candidates are each one function in its own contiguous run, so
each is a one-function claim (`fn_27_1A2C` 0x1A2C, `fn_27_F180` 0xF180, `fn_27_DA18` 0xDA18,
`fn_27_14F00` 0x14F00, `fn_27_10BF8` 0x10BF8, `fn_27_1700` 0x1700, `fn_27_82E4` 0x82E4,
`fn_27_1513C` 0x1513C, `fn_27_152B8` 0x152B8) - except where the neighbour below them is the same
shape, which is where the multi-function runs came from here:

| run | fns | bytes | note |
| --- | --- | --- | --- |
| 0x14F00..0x14FE0 | 2 | 0xE0 | `fn_27_14F00` (a deleting destructor storing `.data:0xED4` and destroying a `CRELFileToken` at +0x2D8) + `fn_27_14F70` (`sZeroVector`/`sNoRotation` stores) |
| 0x1513C..0x1536C | 3 | 0x230 | `fn_27_1513C` + `fn_27_151E0` (a `CAABox` built from a +/- constant around `+0x54`) + `fn_27_152B8` (`AddMaterial`/`Think`/`UpdateAnimation` and a self-delete past a float threshold) |
| 0x10A50..0x10C70 | 5 | 0x220 | `fn_27_10A50`, `fn_27_10AAC`, `fn_27_10AB4`, `fn_27_10B58`, `fn_27_10BF8` |
| 0xF128..0xF1C8 | 3 | 0xA0 | `fn_27_F128` (three floats through a pointer at +0x14FC) + `fn_27_F148` (a `rstl::vector` index, the only one of the three with stack temporaries) + `fn_27_F180` |

The three one-bit predicates in `fn_27_8F90`/`8F9C`/`8FB4` came out 100% first try with
`return self->mFlags.bN;` (a `u8` return) and `return self->mFlags.bN != 0;` (a `bool` return) -
the `!= 0` is what produces the `neg`/`or`/`srwi` conversion retail has in `fn_27_8F9C` only.

No `WALL:` (nothing was left at a sub-100% score) and no `NEW:` - the remaining work is this same
module and this same target, which the driver requeues as-is.

Note for the driver: `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` show as modified in
`git status`; those are `goal_check.sh`'s own gate rewriting the derived counts, not edits of mine.
