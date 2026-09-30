# match-main-cmain-0x91-bitfield

`kind: match`, `target: MetroidPrime/main`. Verdict **PARTIAL**: the flip is out of reach, the
target rose 35 -> 36 functions, and `goal_check.sh` passed every other check.

## The item's premise is wrong, and that is the finding

The item was filed on the belief that retail's `CGameArchitectureSupport::UpdateTicks` "tests
bit 6 of the byte at `CMain`+0x91" and that mapping that byte "needs the retail constructor's
full bitfield map". **There is no unmapped byte and no missing field.** `CMain`'s bitfield map
in `include/MetroidPrime/CMain.hpp` is already right, and `sizeof` is not the issue.

Retail 0x80007C64 is:

```
80007c60:	lwz     r3,gpMain
80007c64:	lbz     r0,145(r3)          <- +0x91
80007c68:	rlwinm. r0,r0,25,31,31
80007c6c:	beq     80007c78
```

`rlwinm rA,rS,25,31,31` does select bit `31-25`=6 of `rS`. But "bit 6" is **not a field
identity** - it is the first field of whichever byte `lbz` loaded, and mwcceppc emits the
identical opcode pair for the first and the ninth one-bit field. Measured, not reasoned
(`tools/probe_cc.sh` on a 16-`bool : 1` struct, `.tmp/opencode/bf/bf.cpp`):

```
f0:  lbz r0,0(r3) ; rlwinm r3,r0,25,31,31      <- b0, first field of byte +0
f8:  lbz r0,1(r3) ; rlwinm r3,r0,25,31,31      <- b8, first field of byte +1
```

Byte-identical apart from the displacement. So the *only* thing that distinguishes `+0x90` from
`+0x91` in this test is the `lbz` offset, and reading the rotate mask as a bit index invents a
field retail's constructor never writes.

That is confirmed from the constructor side (0x80008940-0x800089A0): eight `rlwimi` pairs
writing the eight fields at +0x90, then `stw r8,148(r3)` = +0x94, with nothing in between.
The ninth field (`gameFrameDrawn`) is never cleared by the constructor, exactly as the header
says, and `SetGameFrameDrawn` (0x800089AC) is the ninth because it is the ninth.

## What was actually wrong: the reader, not the header

`src/MetroidPrime/main.cpp:431` read `gpMain->GetFinished()` - the **first** field at +0x90 -
where retail reads +0x91. The fix is one accessor, `GetGameFrameDrawn()`. `mainMid.cpp:282`
(the port's copy of this same retail function) already had it right, with a comment saying so;
only the DOL unit's copy was stale. The fix carries that reasoning over, plus the probe above
so the next reader does not re-derive the false bit-6 reading from the opcode.

The `+0x91` byte therefore needed no new member, no `sizeof(CMain)` measurement, and no
change to the header. **`include/MetroidPrime/CMain.hpp` is untouched by this change.**

## The second difference: one instruction of declaration order

With the accessor fixed, 26 reloc-normalised differences remained; 24 were relocations. Two
were real, and both are the same class of thing - a `li` materialised one slot late:

retail 0x80007CB4 is `li r28,1` **before** `bl CreateFrameBegin` at 0x80007CBC. Spelled after
the `Push` that contains it, `bool keepLooping = true;` materialised at 0x80007CDC instead, and
the tail of the function walked one slot out of step (`lfs f31` landing after `addi r29,r1,16`
rather than before it). Moving the declaration above the `Push` fixes it.

Result: `UpdateTicks__24CGameArchitectureSupportFv` **98.51% -> 100.00%** (552 B), unit
**35 -> 36 / 99** functions, `All:` matched 10025 -> 10026, linked unchanged at 4896.

## Verified

```
tools/decomp_build.sh              All: 30.85% fuzzy, 23.15% matched, 11.74% linked (10026 / 28465)
tools/gate.sh build/goal/judge/report.base.json    ok (docs block is the driver's to rewrite;
                                                    goal_check runs with MP_GATE_DOCS_WRITE=1)
tools/goal_check.sh build/goal/item.json           PARTIAL, as above
sha1sum build/G2ME01/main.dol      6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
86 RELs vs orig/G2ME01/files/RelProd/             all cmp-equal
tools/probe_sources.sh             749 files, 0 failed; port link 250 undefined, 0 duplicates
python3 tools/check_symbol_names.py               503 units, 0 missing
python3 tools/check_decl_order.py --unit MetroidPrime/main.cpp   ok
```

No function anywhere got worse (gate's per-function diff: `+1 functions at 100%`, nothing fell).
No asm. Diff is `src/MetroidPrime/main.cpp` only, 27 insertions / 2 deletions.

## What still stops the flip

Not one thing - 63 of 99 functions in the unit have no body at all, and most of those are
`0.00%` with **no** `fuzzy_match_percent` in the report, i.e. our object does not define the
symbol at all: the `rstl` container destructors and `ReleaseData`s
(`__dt__Q24rstl36vector<...>`, `ReleaseData__Q24rstl53rc_ptr<...>`, and a dozen more), plus
`fn_80008C28`, `fn_80009224`, `fn_80009008`, `fn_80008A48` and their neighbours, and the three
large bodies `RsMain` (0.19%), `AddPaksAndFactories` (0.21%) and `CheckReset` (0.34%).

`flip_test.sh` fails at link with `undefined: fn_80008C28` and eleven siblings - that is the
first error, not the whole story. It **reverted cleanly** and rebuilt the tree to the correct
DOL sha1, which is what makes the partial verdict safe to commit.

Naming the undefined functions is the useful next step, but it is `progress` work on the same
unit and belongs in its own item rather than a rider here.

## For whoever picks up the "bit N of a rotate mask" question

`rlwinm rA,rS,SH,31,31` tests bit `31-SH`, and mwcceppc uses `SH` 25..31 for the eight fields
of a byte in order - so **`SH=25` is the first field of that byte, not "bit 6"**. Reading the
mask as a field number will keep inventing members. Compare the `lbz`/`stb` displacement
instead, or count the `rlwimi` pairs in the constructor, which is unambiguous.
