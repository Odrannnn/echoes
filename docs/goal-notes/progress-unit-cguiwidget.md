# progress-unit-cguiwidget

`GuiSys/CGuiWidget` (DOL unit, stays `NonMatching`): **21/25 -> 23/25** functions matched.
Unit fuzzy 94.14% -> 99.08%; DOL matched 11820 -> 11822 (the `All:` fuzzy line does not move —
the gain is 2 of 28465 functions).

The item's `reason` said `Create` and `CreateGroup` were at 100.0%. Re-measured on the clean
tree they were **99.976746%** and **99.97959%**, both one instruction short, and both for the
same reason. So the two cheapest items were not the ones listed first.

## What changed (`src/GuiSys/CGuiWidget.cpp` only)

### 1. `Create` and `CreateGroup` 99.98% -> 100.0% (2 functions)

The single differing instruction in each was the `addi r4,r4,N` of the string address handed
to `operator new` (`__nw__FUlPCcPCc`):

```
retail  lis r4,0x803b ; addi r4,r4,-6152 ; addi r4,r4,306
ours    lis r4,0      ; addi r4,r4,0      ; addi r4,r4,73
```

i.e. retail materialises `&"??(??" + 306` **relative to the start of this unit's string pool**,
not to a per-translation-unit base. Retail's object has no `.rodata` at all; the pool is
`lbl_803AE7F8`, 0x140 bytes at 0x803AE7F8 (`config/G2ME01/symbols.txt:17686`), and both
`R_PPC_ADDR16_HA/LO` relocations are against it. Our unit emitted a 0x50-byte `.rodata` holding
just its own two literals at `@stringBase0 + 0`, so the offset was 73 (the warning string is
73 bytes, so `"??(??"` sat at 73) rather than 306.

Fix, the same one `src/GuiSys/CGuiCamera.cpp` already uses: spell the pool out byte for byte and
point `rs_new` at `lbl_803AE7F8 + 0x132` via `CMEMORY_NEW_FILE`. The eight
`kGUIModelDrawFlags_*` names ahead of it belong to the same 0x140-byte block but are referenced
by nothing in this unit, so they cannot be interned from source.

### 2. `ReadWidgetHeader` 57.13% -> 97.69%

Two spellings, both measured, neither guessed:

- **`short selfId` / `short parentId`, not `const short`.** With `const`, mwcceppc emits an
  `extsh` on each id before the argument store. Retail just `mr`s the returned value into the
  argument register (r5/r6), with no extension. 57.13% -> 63.36%.
- **The three flags are read as `in.ReadUint8() ? true : false`, not `in.ReadBool()`.**
  `ReadBool()` is `ReadUint8() != 0`; mwcceppc keeps that `bool` as a byte, sign-extends it with
  `clrlwi x,y,24` at each use, and defers the `neg/or` normalisation until *after* the
  `CColor` constructor call. Retail normalises each flag at the read site
  (`lbz; neg; or; srwi x,r0,31`) and never sign-extends. The `? true : false` form makes
  mwcceppc emit exactly retail's sequence. 63.36% -> **97.69%**.

Verified retail reads **four** bytes here, not three: the first is consumed and discarded
(`lwz r5,8(r28); addi r0,r5,1; stw r0,8(r28)` — pointer advanced, value never loaded), then
visible/active/cullFaces. Prime 1's source has the same four reads, so the existing
`in.ReadBool(); // Legacy animation-controller setting is no longer used.` was right; the count
was never the problem.

Still 97.69% and **not** matched: identical instruction count (77) and identical instruction
sequence, differing only in register numbers for three temporaries and in the order of
`lwz r8,0(r7)` vs `addi r7,r1,32`. Register allocation only. Spellings tried and rejected, all
by measured score: `Get<bool>()` (57.13), `const bool` locals (58.75), `int` locals for the flags
(58.75), `int`/`unsigned short`/`long` for the ids (96.26/90.97/96.26), `!!in.ReadUint8()` and
`static_cast<bool>(...)` (58.75), `in.ReadInt8() != 0` (67.78), named `parms` local then return
(87.00), `? false : true` (86.91), `const CColor`/`const EGuiModelDrawFlags` (97.69, no change),
`CColor` read first (62.19), flags read before `CColor` (62.32), dropping the legacy read
(60.16). `short` for the ids and `? true : false` for the flags is the plateau; I did not find
the spelling that moves the last three registers.

### 3. `ParseBaseInfo` 93.35% -> 94.62% (not matched)

Same pool: the `printf` warning string is `lbl_803AE7F8 + 233`. With the literal spelled inline
it resolved against `@stringBase0` and the offset folded to 0, so the function was one
instruction short (77 vs 79) *and* the missing `addi r3,r3,233` was counted as a mismatch. Using
`printf(lbl_803AE7F8 + 0xE9)` restores the third instruction and the exact addend. 93.35% -> 94.62%.

Still 94.62%: identical 79 instructions, differing only in register allocation plus the
prologue order. Retail spills `frame` to r0 and loads `parms.mParentId` into r4 before the
`stmw`; ours loads the id into r0 and moves `frame` into r3/r4 at the call. Spellings tried:
splitting the id into its own statement, `const bool isWorker` + `Get<short>()` +
`const CTransform4f xform` local (95.44% — the best, kept out because it is not matched and the
gain is 0.8% of one function), `ReadUint8() ? true : false` for `isWorker` (94.62, no change).

## Notes for the next attempt

- **`lbl_803AE7F8` is the pool for this whole region, 0x140 bytes.** `lbl_803AE938` (0x30) and
  `@stringBase0` at 0x803AE980 (0x11) are the next blocks. Any other unit whose retail object
  relocates against 0x803AE7F8 needs the same treatment, and a unit that also needs 0x803AE938
  would have to spell both.
- **The `? true : false` trick is a general finding, not specific to this unit.** When retail
  normalises a `bool` at its read site and mwcceppc instead emits `clrlwi x,y,24` before the
  `neg/or`, spelling the read `expr ? true : false` fixes it. Worth trying in any other unit
  with a `clrlwi`-heavy `ReadBool` block.
- **`const short` is the second general finding.** mwcceppc emits `extsh` for a `const short`
  local that it does not emit for a plain `short`; retail's code has no such extension. Any unit
  holding a `short` returned from a call and passing it on should try dropping the `const`.
- **ParseBaseInfo's `isWorker` is a dead read when false**, but the `mWorkerId` store inside the
  `if` is real: deleting it would raise the unit's average while making the function worse.

## Gates (all measured, `./tools/goal_check.sh build/goal/item.json` = PASS)

```
sha1sum build/G2ME01/main.dol   6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
probe_sources.sh                744 files, 0 failed; link LINKED (324 undefined, 0 duplicates)
check_symbol_names.py           515 units, 0 missing
check_decl_order.py             main/GuiSys/CGuiWidget: none out of retail order
decomp_build.sh                 All: 11822 / 28465 functions (was 11820)
goal_check.sh                   PASS; target 21 -> 23 / 25; no asm added
```

`linked` is unchanged at 5727: correct for a `progress` item, since the unit stays
`NonMatching` and no `.s` was involved. `docs/HANDOFF.md`'s state block was refreshed by
`gate.sh`'s own sync, not by hand.

NEW: progress-unit-cguiwidget-pool | progress | GuiSys/CGuiWidget | ReadWidgetHeader is 97.69% and
ParseBaseInfo 94.62%, both identical instruction sequences to retail and differing only in
register allocation; `Create`/`CreateGroup` are done, so this is the remaining 2 of 25.
