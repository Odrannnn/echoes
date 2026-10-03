# carve-8010ee5c - `MetroidPrime/Carve8010EE5C` is `Matching` and flipped

**Result: PASS.** `tools/goal_check.sh build/goal/item.json` exits 0, last line
`goal_check: PASS carve-8010ee5c`:

```
goal_check: item carve-8010ee5c (match) target=MetroidPrime/Carve8010EE5C
goal_check: baseline .../wt-mp2-goal-L7/build/goal/judge/report.base.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13495 -> 13497   linked 6543 -> 6545
  ok    check_symbol_names.py
  ok    All:  37.57% fuzzy, 31.00% matched, 13.87% linked (13497 / 28465 functions)
  ok    flip_test MetroidPrime/Carve8010EE5C.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-8010ee5c
```

`total_functions` is still **28465** after the `splits.txt` edit. `build/report.json` has the unit at
`fuzzy_match_percent 100.0`, `matched_code 144 / 144`, `matched_functions 2 / 2`. The gate's own diff
is a split, not a loss, and names both functions at 100%:

```
  SPLIT   main/auto_03_8010EE5C_text: 4 function(s) accounted for across 2 new unit(s) in main (exact count match - a split, not a loss)
matched  13495 -> 13497   linked 6543 -> 6545   (+2 functions at 100%, 1 units newly linked)
  LINKED   main/MetroidPrime/Carve8010EE5C
  +100%    main/MetroidPrime/Carve8010EE5C :: fn_8010EE5C
  +100%    main/MetroidPrime/Carve8010EE5C :: fn_8010EECC
no regression
```

## The carve

`.text 0x8010EE5C..0x8010EEEC`, `0x90` = 144 bytes, **2 functions**, `symbols.txt:4729-4730`, taken out
of dtk's `auto_03_8010EE5C_text` (whose copy of the bytes is
`build/G2ME01/asm/auto_03_8010EE5C_text.s:8-50`; the same range is now
`build/G2ME01/asm/MetroidPrime/Carve8010EE5C.s`).

| function | retail | what it is | its measured twin |
| --- | --- | --- | --- |
| `fn_8010EE5C` | 0x70 = 112 B, 28 insns | the class's `GetTouchBounds() const`: a degenerate `rstl::optional_object<CAABox>` at `mPosition` | `GetTouchBounds__11CGameCameraCFv` (0x801B062C, 0x70, `src/MetroidPrime/Cameras/CGameCamera.cpp:372-374`, `Matching`) - the same 28 instructions apart from the `bl` |
| `fn_8010EECC` | 0x20 = 32 B, 8 insns | the class's `AcceptScriptMsg`, a frame and one `bl`, nothing else | `fn_80004438` (0x80004438, 0x20, `src/MetroidPrime/Carve80004438.c`, `Matching`) |

Both twins are exact: I diffed them instruction by instruction against
`build/G2ME01/asm/MetroidPrime/Cameras/CGameCamera.s` and
`build/G2ME01/asm/MetroidPrime/Carve80004438.s` before writing anything, and the only difference in
either case is the `bl` target.

**What the two functions *are* is measured from the vtable, not inferred from the shape.** Both are
`.4byte` entries of the *same* object, `lbl_803B4BE0` (`build/G2ME01/asm/auto_07_803B4BB0_data.s:31-64`,
0x803B4BE0, 0x80 bytes), the vtable of an unnamed `CActor` subclass - its destructor entry is
`fn_8010EEEC`, and `fn_8010EEEC` itself stores `lbl_803B4BE0` into `*self` at 0x8010EF18 and calls
`__dt__6CActorFv`. Counting entries from the top of that table:

- entry **18** (line 50) is `fn_8010EE5C`. Entry 18 is where `GetTouchBounds` sits in every camera
  vtable, measured in three at once: `build/G2ME01/asm/auto_07_803B75A8_data.s:136`,
  `auto_07_803B7640_data.s:94` and `auto_07_803B8578_data.s:48`, each immediately after
  `GetDamageVulnerability__6CActorCFRC9CVector3fRC9CVector3fRC11CDamageInfo` exactly as here.
- entry **6** (line 38) is `fn_8010EECC`, the `AcceptScriptMsg` slot every `CActor` subclass
  overrides. Its body leaves the receiver in r3 and r4/r5 untouched, so it forwards to
  `CActor::AcceptScriptMsg` (0x8004B71C, 0x2A0, `symbols.txt:1446`) unchanged.

`addi r4,r4,0x54` passed twice is `mPosition`, measured from both ends:
`include/MetroidPrime/CActor.hpp:290` (`mutable CVector3f mPosition;  // x54`, `GetTranslation()` at
`:143`) and the callee `__ct__6CAABoxFRC9CVector3fRC9CVector3f` (0x802F8CC4, `symbols.txt:13712`),
which takes `CVector3f const&` twice. The return value is sized from the stores: `stb r0,1` at +0x18
and six word copies at +0x00..+0x17 over a `CHECK_SIZEOF(CAABox, 0x18)` box
(`include/Kyoto/Math/CAABox.hpp:122`) - `include/rstl/optional_object.hpp:79-80` puts
`uchar m_data[sizeof(T)]` first and the `ATTRIBUTE_ALIGN(4)` `bool m_valid` behind it, so the value is
`rstl::optional_object<CAABox>`, 0x1C bytes, arriving in r3 with `this` in r4.

**What the carve does not decide: which point the class uses.** Retail's three camera copies of this
override all return a box degenerate at their own origin, and nothing here is evidence that this
class's `mPosition` is not the intended point either.

## Two spellings the next run should not spend a lane on

Both measured in this run on scratch objects compiled with the flags `build.ninja` uses
(`build/scratch/carve8010ee5c.sh`, kept in the tree's gitignored scratch dir).

1. **The return slot has to be an explicit first argument.** Written the way the C++ twin reads - a
   named `OptBox` local and `return` - MWCC 2.7 in `-lang=c` materialises the local in the frame and
   copies it to `*sret` on the way out: **44 instructions in a 0x50 frame** against retail's 28 in
   0x30, with the six stores shifted. Taking it as `void fn_8010EE5C(OptBox* ret, void* self)` gives
   retail's register use exactly (r3 parked in r31 across the call, flag stored before the copy).
2. **The copy must be two 12-byte member assignments, not one 24-byte one.** `ret->box = box`
   compiles to two loads then two stores, three times over - **8 differing instructions**, because
   retail interleaves load/store one word at a time. `ret->box.min = box.min; ret->box.max =
   box.max;` is retail's sequence word for word, and the whole carve then measures **2 differing
   instructions - exactly the two `bl` relocations**, which is the expected result for an unlinked
   object (`powerpc-eabi-nm` on ours: `U __ct__6CAABoxFRC9CVector3fRC9CVector3f`,
   `U AcceptScriptMsg__6CActorFR13CStateManagerRC10CScriptMsg`; the unchanged `main.dol` sha1 below is
   the proof the relocations resolve to retail's addresses).

Also re-measured, because the item did not state it: **the descending-source-order rule is about the
emitted `.text`, not the file.** `fn_8010EE5C` first in the file gives `fn_8010EECC` at offset 0 in
the object; `fn_8010EECC` first gives `fn_8010EE5C` at 0 and `fn_8010EECC` at 0x70. Descending means
the *higher* address first.

## The one thing the gate caught that nothing else would have

`fn_8010EE5C` needs `self + 0x54`, and `tools/check_raw_offsets.py` is a gate step, so the carve
failed the first judge run with:

```
raw offsets not accounted for:
  src/MetroidPrime/Carve8010EE5C.c   1 sites, no section in raw_offsets.md
```

That is rule 4 working, and the fix is documentation rather than a workaround: the site is **Kind A,
an opaque receiver**, because the owning class is unnamed (see above), so `docs/research/raw_offsets.md`
gained a `## src/MetroidPrime/Carve8010EE5C.c (1 site)` section, and its bold total line was
re-derived from the tool rather than appended to (**203 sites in 87 files**, measured; the line had
read 191 in 82 and the paragraph above it carried three mutually inconsistent totals). The site is
the **third** raw offset in a `.c` unit - `check_raw_offsets.py --list | grep -E "\.c\s+[0-9]+$"`
prints exactly `Cameras/Carve801E7C14.c`, `Carve800E9C14.c` and this file, 1 site each, 3 of the 203 -
so the other two `.c` sites are the same debt in a different guise and neither is cheaper to remove.

Do **not** try to dodge the checker instead. `(const Vec3f*)((unsigned long)self + 0x54)` still
matches: `BASE` keys on the literal `self + 0x`, cast or no cast.

## One stub, and why the box ctor did not need one

This unit is in `files.cmake`, so **the PC build compiles it with the host compiler**, not mwcceppc
(`build-port-link/build.ninja:3072-3078`: `C_COMPILER__mp_game_unscanned_RelWithDebInfo`,
`-DTARGET_PC`). `__MWERKS__` is therefore the discriminator for the two builds, the same split
`src/MetroidPrime/Carve80004438.c:78-81` and `src/MetroidPrime/ScriptObjects/Carve801FEAE0.cpp:250`
already use. Measured on the built port object
(`build-port-link/CMakeFiles/mp_game.dir/src/MetroidPrime/Carve8010EE5C.c.o`):

```
                 U AcceptScriptMsg__6CActorFR13CStateManagerRC10CScriptMsg
                 U _ZN6CAABoxC1ERK9CVector3fS2_
0000000000000010 T fn_8010EE5C
0000000000000000 T fn_8010EECC
```

- `CAABox::CAABox(CVector3f const&, CVector3f const&)` **is** in the port -
  `src/Kyoto/Math/CAABox.cpp:16` - so under `#else` the carve calls the host compiler's own name for
  it and gets a **real** box. Only the matching build needs retail's linker name, and dtk's own
  object supplies that definition there. No stub, no gap-list entry.
- `CActor::AcceptScriptMsg` has **no** definition anywhere in `src/`, in either build, so it needs one
  announced empty-body stand-in: `stub_carve8010ee5c_0` at the end of
  `src/MetroidPrime/PortLinkStubs.cpp`. The header paragraph of that file deliberately still reads
  `192 functions, 10 data objects` where the tree now holds **193 / 10 / 203 `asm(` entries** (all
  three re-measured with the three terms the header names); the new stub's own comment records that,
  following `stub_carve80256d1c_0`.

The gate's port checks came out flat, which is the result to quote:

```
build/gate-dups.log     link_check: unique undefined symbols 287 / duplicate definitions 0 / unchanged from baseline (287 undefined, 0 duplicates)
build/gate-probe.log    probe: 928 files, 0 failed, 0 errors; link: LINKED (287 undefined, 0 duplicates)
build/gate-link.log     ok: 279 MISSING symbol(s), all accounted for in port_link_gap_list.md
```

`docs/research/port_link_gap_list.md` is untouched: the carve **closes** no name that was on the list
(neither `fn_8010EE5C` nor `fn_8010EECC` was in it) and **opens** none that is not now stubbed, so
the table needed no row and no deletion. `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` show as
modified in `git status`: that is the judge's own `MP_GATE_DOCS_WRITE=1` step, not a lane edit.

## Files

| file | change |
| --- | --- |
| `src/MetroidPrime/Carve8010EE5C.c` | new, 160 lines; two definitions, **descending by address**, plain C so the `fn_` names do not mangle |
| `configure.py:738` | `Object(Matching, "MetroidPrime/Carve8010EE5C.c"),` on one line, in address order between `Carve8010EE54.c` (0x8010EE54) and `Carve801174E8.c` (0x801174E8) |
| `config/G2ME01/splits.txt:755-756` | `.text start:0x8010EE5C end:0x8010EEEC`, between the same two units |
| `files.cmake:706` | `src/MetroidPrime/Carve8010EE5C.c`, in address order in the carve block |
| `src/MetroidPrime/PortLinkStubs.cpp:1698-1728` | new `stub_carve8010ee5c_0` for `AcceptScriptMsg__6CActorFR13CStateManagerRC10CScriptMsg`, announced empty body |
| `docs/research/raw_offsets.md:41-45, 1110-1135` | the bold total re-derived from the tool (203 in 87) and the new 1-site section |

No `symbols.txt` edit (both placeholders already carry the right sizes). Nothing was copied from
another tree or commit. No `asm` in the diff.

## Verification (every number measured in this run)

```
tools/carve_diff.sh 0x8010EE5C 0x90 <ours.o>      retail: 36 instructions, 144 bytes
                                                 ours  : 36 instructions, 144 bytes
                                                 differing instructions: 2  <- exactly the two `bl`s
./tools/unit_fit.sh MetroidPrime/Carve8010EE5C.c  .text claimed 144 ours 144 retail 144 fits;
                                                   no extra functions
python3 tools/check_decl_order.py --unit main/MetroidPrime/Carve8010EE5C
    ok: 1 unit(s) checked, none emits its functions out of retail order
python3 tools/check_symbol_names.py              checked 587 units; 0 declared names are missing
python3 tools/check_raw_offsets.py               ok: 203 raw-offset site(s) in 87 file(s)
build/report.json                                 main/MetroidPrime/Carve8010EE5C  fuzzy 100.0
                                                 matched_code 144 / 144  matched_functions 2 / 2
build/gate-order.log                              ok: 1198 unit(s) checked, 37 permuted, all 37 accounted for
sha1sum build/G2ME01/main.dol                    6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
86 RELs                                           the gate's ninja CHECK edge (build.sha1) and the
                                                 config.yml re-hash both pass, 0 mismatched
./tools/flip_test.sh MetroidPrime/Carve8010EE5C.c PASS -> kept as Matching (1/1, failed 0, skipped 0)
```

`build/report.json` before the change had **no** entry for this unit (the range was inside
`main/auto_03_8010EE5C_text`), so the rise from 13495 to 13497 is this carve and nothing else.

## Next: the rest of this auto unit, and the class

The 0x120 bytes above this claim, `0x8010EEEC..0x8010F084`, are still unclaimed and hold exactly
**two** functions (0x60 + 0x138 = 0x198, and the next symbol in `symbols.txt` is
`IsAnythingSet__13CMapWorldInfoFv` at 0x8010F084): `fn_8010EEEC` (0x60 = 96 B, the destructor -
`lis/addi lbl_803B4BE0`, `stw` it into `*self`, `bl __dt__6CActorFv`, then `Free__7CMemoryFPCv` behind
the flag's `extsh`/`ble`) and `fn_8010EF4C` (0x138 = 312 B, the `CActor` constructor - a
`CModelDataNull`, a `CActorParameters`, two `__shl2i` for the model flags, a
`Scannable__16CActorParameters` copy and the base `__ct__`). `fn_8010EEEC` is a deleting destructor
whose receiver is the unnamed class, so it is the same Kind A debt as this carve; `fn_8010EF4C` is
the richest read of the class's own `CActor` base that exists in retail, because it names all seven of
its constructor arguments.

`fn_8010EDB4` (entry 5, `Think`, in the same vtable) is the same class and would be a fourth anchor.
Together those would give the class enough to read its layout out of the vtable and its constructor,
which is what would retire the `+0x54` site in `docs/research/raw_offsets.md` properly, rather than
leaving it as documented debt.
