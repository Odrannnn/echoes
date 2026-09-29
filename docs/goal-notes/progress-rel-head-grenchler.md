# progress-rel-head-grenchler - CGrenchlerRel.cpp, module 27's head (18 functions)

**Result: the module head is claimed and the module still hashes.** `Grenchler/MetroidPrime/ScriptObjects/CGrenchlerRel`
is `Matching` at **18/18**, and `build/G2ME01/Grenchler/Grenchler.rel` is `de128c91f467b5b61b4de4a2ed6b217044a975c5`,
which is exactly what `config/G2ME01/config.yml:141` records for module 27 and byte-identical to
`orig/G2ME01/files/RelProd/Grenchler.rel`. The summed `module:Grenchler` count rose **7 -> 25**
(the 7 were the `REL_Setup` and `global_destructor_chain` tails); the whole-tree
`matched_functions` rose 9476 -> 9494 and `linked` 4798 -> 4816, i.e. +18 both ways.

## What I wrote

- **`src/MetroidPrime/ScriptObjects/CGrenchlerRel.cpp`** (new, 205 lines) - `.text 0x0..0x168`:
  the fifteen short accessors the REL loader generator emits, `fn_27_8` (this module's
  `GetBoundingBox` wrapper), `fn_27_C8`'s vtable call on slot 0x38, `RELExit`, `RELMain`, and the
  loader registration `fn_27_138` that `RELMain` calls. Definitions are in **descending** retail
  order, as the family requires.
- **`config/G2ME01/rels/Grenchler/splits.txt`** - a third entry, placed first (address order, the
  way `DarkCommando/splits.txt` and `ElitePirate/splits.txt` do it):
  `MetroidPrime/ScriptObjects/CGrenchlerRel.cpp: .text start:0x00000000 end:0x00000168`. The two
  existing entries (`REL/global_destructor_chain.c` at 0x15738, `REL/REL_Setup.cpp` at 0x157AC) are
  untouched, so nothing overlaps.
- **`configure.py`** - `Rel("Grenchler", [Object(Matching, "MetroidPrime/ScriptObjects/CGrenchlerRel.cpp")])`
  appended after DarkCommando's, with a comment recording the measured diff against MediumIng.
- **`docs/research/raw_offsets.md`** - a section for the new file, which `tools/check_raw_offsets.py`
  requires and `tools/gate.sh` runs.

I did not touch `docs/HANDOFF.md` (the judge rewrites its derived counts), `files.cmake` (the file
defines a module entry point, so `check_files_cmake.py` counts it out and reports the exclusion
rather than failing - it printed "32 further units are out because they define a module entry
point", up from 31), or anything under `tools/`.

## Measurements, not recollections

```
$ python3 tools/audit_rel_claim.py Grenchler
ok   MetroidPrime/ScriptObjects/CGrenchlerRel.cpp         0x00000000..0x00000168  18/18 functions
ok   REL/global_destructor_chain.c                        0x00015738..0x000157AC  2/2 functions
ok   REL/REL_Setup.cpp                                    0x000157AC..0x00015950  5/5 functions
0 claim(s) with a problem
Grenchler: preplf 377 text symbols, plf 377, 0 dropped by -strip_partial

$ sha1sum build/G2ME01/Grenchler/Grenchler.rel orig/G2ME01/files/RelProd/Grenchler.rel
de128c91f467b5b61b4de4a2ed6b217044a975c5  build/G2ME01/Grenchler/Grenchler.rel
de128c91f467b5b61b4de4a2ed6b217044a975c5  orig/G2ME01/files/RelProd/Grenchler.rel

$ ./tools/unit_fit.sh MetroidPrime/ScriptObjects/CGrenchlerRel.cpp
   .text      claimed    360   ours    360   retail    360   fits
   no extra functions: our object defines only what the retail unit object does

$ MP_GATE_DOCS_WRITE=1 ./tools/gate.sh
configure ok / ninja + build.sha1 ok / hashes vs config.yml ok / report ok
per-function diff   SPLIT   Grenchler/auto_00_00000000_text: 360 function(s) accounted for across
                              2 new unit(s) in Grenchler (exact count match - a split, not a loss)
module wiring ok / dol_read ok / docs claims ok / gs offsets ok / raw offsets ok
decl order ok / files.cmake ok / module order ok / port probe ok / port link gap ok
GATE PASS  016668a+5 changed
```

The `per-function diff` line is a **SPLIT**, not a loss: carving 0x0..0x168 out of dtk's
`auto_00_00000000_text` moves 360 functions from the auto unit into ours, and all 360 are accounted
for. The gate prints that itself and passes it.

Other gates, all measured:

```
sha1sum build/G2ME01/main.dol                 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010   (as documented)
./tools/probe_sources.sh                      735 files, 0 failed, 0 errors; link: LINKED (254 undefined, 0 duplicates)
python3 tools/check_symbol_names.py           checked 484 units; 0 declared names are missing from their object
./tools/decomp_build.sh                       All: 29.12% fuzzy, 21.26% matched, 11.38% linked (9494 / 28465 functions)
dtk shasum -c config/G2ME01/build.sha1        87 lines, every one OK, exit 0 (DOL + all 86 RELs)
python3 tools/check_module_wiring.py          85 units of our own code in 66 modules (was 84 in 65)
python3 tools/check_raw_offsets.py            ok: 152 raw-offset site(s) in 61 file(s), all documented
python3 tools/check_decl_order.py             ok: 938 unit(s) checked, 28 permuted, all 28 accounted for
python3 tools/check_files_cmake.py            every configured DOL object is either in files.cmake or excluded
```

`total_functions` is still 28465 (`decomp_build.sh`'s `All:` line and `config.yml` untouched for
that), and the `splits.txt` edit added no `total_functions` - the head was already inside dtk's
`auto_00_00000000_text`'s 360, which is why the split check is an exact count match.

## What is measured rather than assumed

**The block is `CMediumIngRel.cpp`'s in the same order, plus three predicates.** I diffed
`build/G2ME01/Grenchler/asm/auto_00_00000000_text.s` over 0x0..0x168 against that file's claimed
0x0..0x150 rather than reading the `fn_<id>_<off>` names, which say nothing about which function
is which. The two agree on: opening `addi r3,r3,0x7c0` then the `GetBoundingBox` wrapper; **three**
`li r3,0` predicates in a row; **no** `lbl_8041AAB8` float store at +0x448. They differ in exactly
one place - MediumIng goes from `addi r3,r3,0x754` straight to its three-float copy, and this
module runs `li r3,1`, `li r3,0`, `li r3,0` first. That 0x18 is the whole of the difference
between the claims (0x168 vs 0x150, 18 functions vs 15). So no spelling had to be discovered: every
body is one `CMediumIngRel.cpp` or `CMysteryFlyerRel.cpp` already reproduces at 100%.

**`fn_27_8` is the out-of-line-constructor case, not the inlined one.** `build/.../auto_00_00000000_text.s`
shows it ending in `bl fn_27_13C6C`, and `fn_27_13C6C` itself is 0x3C bytes: six `lwz`/`stw` pairs
out of `r4+0x00..r4+0x14` plus `stb 1, 0x18(r3)`. So it is
`void fn_27_8(void* out, const CPhysicsActor* self) { fn_27_13C6C(out, self->GetBoundingBox()); }`
with the ctor declared `extern "C" void fn_27_13C6C(void* out, const CAABox& box)` and the ctor at
0x13C6C left unclaimed - exactly `CMysteryFlyerRel.cpp`'s `fn_45_10` and `CMediumIngRel.cpp`'s
`fn_41_8`. (DarkCommando's `fn_3_14` is the other case: it *inlines* the same body and is 0x68 bytes
with no call. I read the disassembly, so I did not assume either.) The const reference on the
ctor's second parameter is load-bearing: without it the frame grows to 0x40.

**The loader setter is the plain `fn_80218A38`, no rename needed.** `config/G2ME01/symbols.txt:9474`
gives `fn_80218A38 = .text:0x80218A38`, and `build/G2ME01/asm/auto_03_80218A38_text.s` is
`stw r3, gLoader_Grenchler@sda21(r0); blr` - it stores the **address** of a loader slot. It sits
immediately after `LoadGrenchler__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at 0x80218A0C,
which is 0x2C bytes, so it ends exactly at 0x80218A38. That is why the registration hands it
`&lbl_27_bss_40`. It is `extern "C"`: an alias would be a different symbol and the call would
resolve to nothing. (The reason text warned it might be a mangled `SetLoader_*` name; it is not
for this module, same as MediumIng's `fn_80218A6C`.)

**The loader slot is `.bss:0x40`, not `.bss:0x0`.** `build/G2ME01/Grenchler/asm/auto_05_00000000_bss.s`
holds seven objects; `lbl_27_bss_40` is the four-byte slot the registration stores through, and
`.bss:0x0` is an unrelated 8-byte object used far above the head. The unit's split claims `.text`
only, so dtk's `.bss` object defines the slot and a second definition under MWCC is what produced
mwldeppc's internal linker error on ScriptPlayerProxy - hence `extern` under `__MWERKS__` and a host
definition.

**No dead-strip hazard, and that is a measurement, not a hope.**
`config/G2ME01/rels/Grenchler/ldscript.lcf` lists all fifteen of `fn_27_0` .. `fn_27_C8` in its
FORCEACTIVE block, and `.data:0xA54` (CGrenchler's vtable) and `.data:0xED4` (CPhysicsActor's)
store every one of them, so no `force_active:` entry in `config.yml` is needed. `RELMain`/`RELExit`
are the module's entry points and `fn_27_138` is called from `RELMain`.

**The vtable call is a member call, and the class is reached through a stand-in.** `.data:0xA54`
stores `fn_27_C8` at offset 0x3C, above slot 0x38 named `HealthInfo__3CAiFv`; the second table
stores it at the same 0x3C above `HealthInfo__6CActorFv`. Thirteen virtuals after two leading words
put the thirteenth at 0x38, so `self->Slot12()` on a thirteen-slot local class compiles to
retail's `lwz r12, 0x0(r3); lwz r12, 0x38(r12); mtctr r12; bctrl`. Hand-loading the vtable compiles
to `lwz r3,0(r3)` instead (measured 99.09% in `CIngPuddleRel.cpp`).

**`CPhysicsActor` is the one-method local stand-in, not the real header.** Including
`MetroidPrime/CPhysicsActor.hpp` reaches `Collision/CMaterialList.hpp`, whose file-scope
`static EMaterialTypes SolidMaterial` puts 0x28 bytes of `.data` in the object; retail's head has
none and the module sha1 broke on exactly that with every function at 100%. Only the mangled name
`GetBoundingBox__13CPhysicsActorCFv` has to agree, and one const member gives it. (A second,
file-scope definition of the same stand-in class already exists in
`CEmperorIngStage3Rel.cpp`, which *is* in `files.cmake`; like the ones the reviewer noted on
DarkCommando, the two are identical one-method classes and neither defines the member, so nothing
collides. `link: LINKED (254 undefined, 0 duplicates)` is unchanged from the baseline.)

## What I left, and why

Everything from `fn_27_168` (0x168, 0xE00) up - the module's own entity loader and its ~300 class
methods - stays retail. It is behavioural class code needing the CActor/CPatterned/CAi hierarchy
this tree does not model, exactly as every other head in this family leaves. dtk fills it.

`docs/RUNNING_THE_DECOMP.md`'s "Attempted modules" table row is **not** in this change: the driver
discards edits to that file before judging, and the brief says so explicitly. The row to add is
`Grenchler | 18 functions, .text 0x0..0x168, Matching, module sha1 held | MediumIng's block in the
same order plus three predicates (li 1, li 0, li 0) before the three-float copy; setter is the
plain fn_80218A38; slot .bss:0x40; no dead-strip hazard`.

NEW: rel-head-class-code-needs-actor-hierarchy | progress | fn_27_168 | every REL head in this family
now stops at the module's own entity loader (Grenchler's is 0x168, 0xE00, ~300 class methods) and
none of the heads after it can move until a CGrenchler/CActor/CPatterned header exists; the accessors
are all landed, so this is now the whole remaining module.
