# Raw-offset code: the policy, and the measured debt

**The decision (2026-09-25).** A raw offset - reaching a field as `this + 0x428` instead of
`this->field` - is **not** a decompilation. It reproduces retail's bytes and nothing else: it is
invisible to a reader, it silently assumes 32-bit layout, and upstream `PrimeDecomp/echoes`
rejects it. This file is the policy; `tools/check_raw_offsets.py` is what keeps it true, and it
runs in `tools/gate.sh`.

There are three kinds, and they are not equally acceptable:

| kind | what it is | verdict |
| --- | --- | --- |
| **A - opaque receiver** | a free function whose parameter is an object we have not modelled, so the body adds an offset to a `const void*` | **keep.** This is retail's own shape: the function takes a pointer, and there is no `this` to write. Turning it into a member would be inventing a class. |
| **B - unmodelled member** | a member function of a class that *is* modelled, reaching one of its own members by offset | **debt.** The member exists in retail and is missing from our header. Allowed while the header cannot take it, never silently, and listed below with its blocker. |
| **C - unmodelled class** | a whole class's accessors written as raw offsets, one per member | **not acceptable.** The class needs a struct. This is the `ScriptFrontEndDataNetwork` case and it is the one outstanding piece of work this file creates. |

## The rules

1. **Kind A stays as it is.** Do not "fix" it into a fabricated member or a fake `this`. A lane
   that models the class may replace it, and then the offsets go with the class.
2. **Kind B is a named follow-up, not a footnote.** Each one below says what the member is and
   what stops the header from having it. The recurring blocker is the one in
   `RUNNING_THE_DECOMP.md`: adding a member shifts every later member, and several of those
   classes carry filler words written for the current layout, so a header change is a
   per-class repair, not a one-line edit.
3. **Kind C is a bug in the port's terms.** 26 accessors of one class by raw offset is not a
   decompilation of that class, and it will be wrong on a 64-bit build. Model the struct.
4. **A new file with a raw offset fails the gate.** That is the whole point of the checker: the
   debt stays visible instead of accumulating one access at a time.
5. **Porting from upstream may not add one.** If a unit we port from `PrimeDecomp/echoes` can
   only be reproduced with a raw offset, that is a blocker on the port and belongs in this file,
   not in a commit.
6. **The SDK shim layer is out of scope** (`src/Dolphin/`, `src/Runtime/`, `libc/`). Those files
   reproduce Nintendo's code, where the offset is the specification rather than our guess, and
   they are measured by `tools/probe_sources.sh`.

## The debt, measured

`python3 tools/check_raw_offsets.py --list` prints this table with line numbers; the counts in
the headings below are what the checker compares against, so editing a source without updating
the count here fails the gate. **142 sites in 53 files** (`python3 tools/check_raw_offsets.py`
prints `142 raw-offset site(s) in 53 file(s)`; measured 2026-09-29 after the `CRezbitRel` head
added the 53rd file, up from 141 in 52 after `CMediumIngRel`, which read 140 in 51 after
`CIngSpaceJumpGuardianRel`).
**This total has now gone stale twice, and
the doc already recorded how**: `tools/check_raw_offsets.py` compares the *per-file* counts in
the headings and never this total, so a summary left behind by earlier heads goes unnoticed. It
read 120 in 42 before the `CDarkTrooperRel` head, then 130 in 47 while the tool measured 139 in
50. Run the tool and quote it; the headings are the part that is enforced.

**One head of this family is invisible to the checker entirely, and the reason is worth recording
before the next one hits it.** `src/MetroidPrime/ScriptObjects/CMetroidRel.cpp` (module 40's head,
landed 2026-09-29) reaches **five** members by raw offset - `+0x8C8` (`fn_40_0`), `+0x754`
(`fn_40_A4`), `+0x448` (`fn_40_4C`), `+0x44F` (`fn_40_5C`) and `+0x34C` (`fn_40_8C`) - and the tool
measures **zero**, so this file has no `##` section and the total above is unchanged by it. Four of
the five go through `static_cast< char* >` or a subscript, which `BYTE_CAST` does not key on. The
fifth, `+0x448`, *is* written with `reinterpret_cast` and is dropped for a different reason: the same
line names `lbl_8041AAB8`, and `IGNORE` skips any line containing a `lbl_` symbol on the theory
that it is a data reference, not a field access. So a section cannot be written for this file - the
checker fails any documented path it does not measure ("delete the section") - and the debt has to
live here instead. Every other head in this family is at least partly counted because one of its
accessors happens to be the three-float copy, which the checker *does* see; `CMetroidRel.cpp` has
**no** three-float copy (`fn_40_B4` is an eight-byte `li r3,0` predicate), which is the whole reason
it drops to zero. **Kind A, opaque receiver**, like the rest: free functions over a `void*` because
`CMetroidAlpha` has no header here, and the only object carrying the offsets is the module's own
retail bytes.

### Kind C - to be modelled, highest priority

## `src/MetroidPrime/ScriptObjects/ScriptFrontEndDataNetwork.cpp` (26 sites)

Offsets 0x08, 0x1c, 0x20, 0x24, 0x28, 0x2c, 0x30, 0x34, 0x38, 0x3c, 0x40, 0x44, 0x48, 0x4c,
0x50, 0x54, 0x58, 0x5c, 0x60, 0x64. The class is `CFrontendDataNetwork`; the accessors are
`Get/Set` pairs over vectors of floats plus a flag byte, and one accessor per member. The
`CScriptForgottenObject`-shaped fix is not available here because there is no struct at all: this
needs the class modelled from the retail layout, the same job as
`docs/research/TypesMatch_unnamed_ids.txt` did for the `TypesMatch` ids. Until then the module
reproduces retail but the class does not exist in C++.

### Kind B - unmodelled members, each with its blocker

## `src/MetroidPrime/CStateManager.cpp` (1 site)

`+0x9c`, the list link written as a `uintptr_t` because retail's is a host pointer. Blocker:
`CStateManager` is 63 of 239 functions and its header is still being repaired; the fix is a
member of the right type, not a cast.

## `src/MetroidPrime/Enemies/CSwarmBasicsHealthInfo.cpp` (1 site)

`+0x428`, `CSwarmBasics::HealthInfo(CStateManager&)`. Blocker: `CSwarmBasics`' own layout, which
depends on `CPatterned` and `CAi`.

## `src/MetroidPrime/Enemies/CSwarmBasicsOrbitPosition.cpp` (1 site)

`+0x194`, a `CVector3f` member reached by const pointer. Blocker: as above.

## `src/MetroidPrime/ScriptObjects/CFlyerSwarm.cpp` (1 site)

`+0x184`, a `u8*` to the boids array. Blocker: the `CFlyerSwarm` layout, which is the module's
own class and is 7 functions in.

## `src/MetroidPrime/ScriptObjects/CScriptCoinTouchBounds.cpp` (1 site)

`+0x2f9`, a bit tested in `CScriptCoin`'s `GetTouchBounds` override. It is **past the end of
`CPhysicsActor`** (`CHECK_SIZEOF` says 0x2d0), so it belongs to `CScriptCoin` itself and is not
inherited state. Blocker: `CScriptCoin`'s own layout is not modelled - the class exists here as a
slot-only vtable in the Metaree/Puffer style, with no data members at all.

#### Kind A - opaque receivers, kept deliberately

These take a `const void*` because the receiver's class is not modelled, so the offset is the
only way to write the body - and it is what retail does. They are listed so that nobody spends
a lane "fixing" them.

## `src/MetroidPrime/ScriptObjects/CScriptMetaree.cpp` (3 sites)

`+0x54` (three floats), `+0x34c` (a bitfield byte, `>> 3 & 1`), `+0x44f` (a byte). Three free
functions over an unmodelled `CMetaree`.

## `src/MetroidPrime/ScriptObjects/CMetareeSwarmRel.cpp` (4 sites)

`+0x184` (twice: a `char*` to an array of 0xB8-byte records, read by `fn_43_0` and `fn_43_3C`),
`+0x17C` (the record count, `lwz r0,0x17c(r3)` against a `cmpw r4,r0` bound check) and `+0xB2` (the
flags byte `fn_43_0` tests, `>> 7 & 1`). One module id on from `CScriptMetaree` and the same shape:
free functions over a `const void*` because `CMetareeSwarm` is not modelled, and the record's three
floats are 0x10 apart, so they are three fields rather than a 12-byte vector. Blocker: the class
needs the CActor/CPatterned hierarchy, which is what module 43's `fn_43_D8` needs too and why the
rest of the module stays retail.

## `src/MetroidPrime/ScriptObjects/CIngPuddleRel.cpp` (1 site)

`+0x460`, the one place `fn_32_0` - CIngPuddle's vtable entry at offset 0x38 - returns
`this + 0x460`. Free function over a `const void*` for the reason above: `CIngPuddle` is declared
only inside `src/MetroidPrime/TypesMatch.cpp` and has no header here. Blocker: the member is 0x460
bytes into a `CPhysicsActor`, and modelling it is the same CActor/CPhysicsActor job that
`fn_32_A8` - the module's own entity loader - needs before the other 57 functions can move.

## `src/MetroidPrime/ScriptObjects/CIngSnatchingSwarmRel.cpp` (1 site)

`+0x1F4`, the one place `fn_33_0` - CIngSnatchingSwarm's vtable entry at offset 0x38 - returns
`this + 0x1F4`. Free function over a `const void*` for the reason above: `CIngSnatchingSwarm` is
declared only inside `src/MetroidPrime/TypesMatch.cpp` and has no header here. Blocker: the member
is 0x1F4 bytes into a `CActor` (its `TypesMatch__18CIngSnatchingSwarmCFi`, 0x8009C5F4, is
`cmpwi r4,0x1e` against `TypesMatch__6CActorCFi` - one class nearer than `CIngPuddle`'s
`CPhysicsActor` parent), and modelling it is the same CActor job that `fn_33_A8` - the module's own
entity loader, 0x5D4 bytes - needs before the other 91 functions can move.

## `src/MetroidPrime/ScriptObjects/CAtomicAlphaRel.cpp` (2 sites)

`+0x54` (the three floats `fn_2_80` copies out of its second argument) and `+0x44F` (the byte
`fn_2_20` returns). **The two sites below understate the file: seven members are reached by literal
offset** - +0x8C8, +0x7D8, +0x448, +0x44F, +0x34C, +0x754, +0x54 - and the five the checker misses
are the same debt in a spelling it cannot see, because it keys on a cast to a byte or arithmetic
type in the same statement, and those five are reached through a plain `char*` rather than a
`BYTE_CAST`-like one - four as `static_cast< char* >(self) + 0x8C8` / `+0x7D8` / `+0x754` /
`+0x448`, and one by indexing a typed pointer (`static_cast< const unsigned char* >(self)[0x34C]`).
Free functions over a `void*` for the reason above:
`CAtomicAlpha` is not modelled, and the only object carrying the offsets is the module's own
retail bytes. The whole block is the fourteen-accessor set
`AtomicBetaAccessors.cpp` already carries, so this is the same debt a third time - though **not
byte for byte**: twelve of the fourteen are the same bodies, and AtomicAlpha's two *leading*
accessors are extra, at +0x8C8 and +0x7D8, where AtomicBeta opens with the float store. Blocker:
the class needs the CActor/CPatterned hierarchy, which is what module 2's `fn_2_13C` (0x13C,
0x420), its own entity loader, needs before the other 47 unclaimed functions can move.

## `src/MetroidPrime/ScriptObjects/CMysteryFlyerRel.cpp` (1 site)

`+0x54` (the three floats `fn_45_B4` copies out). As with `CAtomicAlphaRel.cpp` above, **the one
site understates the file**: +0x818, +0x754, +0x448, +0x44F and +0x34C are reached through a plain
`char*` or a typed-pointer subscript the checker does not key on. The block is AtomicAlpha's
accessor set again with one leading accessor (+0x818) instead of two, plus `fn_45_10`, which needs
no offset because it calls `CPhysicsActor::GetBoundingBox` through a one-method local stand-in (the
real header adds `.data` and breaks the module hash). Blocker: the same CActor/CPatterned hierarchy
module 45's entity loader `fn_45_170` needs.

## `src/MetroidPrime/ScriptObjects/CEmperorIngStage3Rel.cpp` (4 sites)

`+0x44F` (the byte `fn_18_44` returns), `+0x34C` (the bitfield byte, `& 8`), `+0x54` (the three floats
`fn_18_98` copies out), and **`+0x48C` / `+0x37C`** (`fn_18_E0`: a pointer at +0x48C, then the word at
+0x37C of what it points at, compared with 6). **As with `CAtomicAlphaRel.cpp` and
`CMysteryFlyerRel.cpp` above, the checker undercounts this file**: `+0x754` (`fn_18_88`, the address
of a member) is reached through a plain `char*` the checker does not key on, so the true count is
five sites over four members. It is the same generated accessor block as the rest of the family and
**the same debt a fourth time**, with one difference worth recording: module 18's variant has **no
`lbl_8041AAB8` float store at +0x448 and no `lbl_8041B758` float accessor**, so this module's four
accessors do not cover the +0x448 member the other modules' do - it is read by retail's own class
code further up, which stays unclaimed. `fn_18_8` needs no offset because it calls
`CPhysicsActor::GetBoundingBox` through the same one-method local stand-in. Free functions over a
`void*` for the reason above: the actor type is not modelled, and the only object carrying the
offsets is the module's own retail bytes. Blocker: the same CActor/CPatterned hierarchy that module
18's entity loader `fn_18_F8` (0xF8, 0x11C) needs.

## `src/MetroidPrime/ScriptObjects/CIngSpaceJumpGuardianRel.cpp` (1 site)

`+0x54` (the three floats `fn_34_B4` copies out). **As with `CAtomicAlphaRel.cpp`,
`CMysteryFlyerRel.cpp` and `CEmperorIngStage3Rel.cpp` above, the checker undercounts this file**:
`+0x448` (`fn_34_58`'s float store), `+0x44F` (`fn_34_68`), `+0x34C` (`fn_34_98`'s bit 3), `+0x754`
(`fn_34_A4`, the address of a member) and `+0x8D0` (`fn_34_0`) are all reached through a plain
`static_cast< char* >` or a subscript, which the checker does not key on, so the true count is six
sites over six members. It is the same generated accessor block as the rest of the family and **the
same debt a fourth time**, with two differences worth recording: this module's `fn_34_10` returns a
**module-local `.rodata` constant** (`.rodata:0x0`, `.float 60`) rather than the family's DOL
`lbl_8041B758`, and the `lbl_8041B758` accessor Tryclops carries at 0x98 is not here at all.
`fn_34_1C` needs no offset because it calls `CPhysicsActor::GetBoundingBox` through the same
one-method local stand-in. **Kind A, opaque receiver**: free functions over a `void*` because
`CIngSpaceJumpGuardian` is declared only inside `src/MetroidPrime/TypesMatch.cpp` and has no header
here, and the only object carrying the offsets is the module's own retail bytes. Blocker: the same
CActor/CPatterned/CAi hierarchy that module 34's entity loader `fn_34_170` (0x170, 0x330) needs
before its other 125 class functions can move.

## `src/MetroidPrime/ScriptObjects/CMediumIngRel.cpp` (1 site)

`+0x54` (the three floats `fn_41_94` copies out). **As with `CAtomicAlphaRel.cpp`,
`CMysteryFlyerRel.cpp`, `CEmperorIngStage3Rel.cpp` and `CIngSpaceJumpGuardianRel.cpp` above, the
checker undercounts this file**: `+0x7C0` (`fn_41_0`), `+0x754` (`fn_41_8C`), `+0x44F` (`fn_41_44`)
and `+0x34C` (`fn_41_74`'s bit 3) are all reached through a plain `static_cast< char* >` or a
subscript, which the checker does not key on, so the true count is five sites over five members.
It is the same generated accessor block as the rest of the family and **the same debt a fifth
time**, with one difference worth recording: this module has **no** `lbl_8041AAB8` float store at
+0x448 and **no** `li r3,1` predicate, so it covers one member fewer than the rest of the family,
and its `fn_41_8` - the `optional_object<CAABox>` wrapper - sits at 0x8 rather than in the middle
of the block, because the family's leading accessors are re-ordered here (measured by diffing
dtk's `auto_00_00000000_text.s` against `CMysteryFlyerRel.cpp`'s, not by the `fn_<id>_<off>` names,
which say nothing about which function is which). `fn_41_8` needs no offset because it calls
`CPhysicsActor::GetBoundingBox` through the same one-method local stand-in (the real header adds
0x28 bytes of `.data` and breaks the module hash). **Kind A, opaque receiver**: free functions
over a `void*` because `CMediumIng` has no header here, and the only object carrying the offsets
is the module's own retail bytes. Blocker: the same CActor/CPatterned/CAi hierarchy that module
41's entity loader `fn_41_150` (0x150, 0x868) needs before its other 160 class functions can move.

## `src/MetroidPrime/ScriptObjects/CTryclopsRel.cpp` (2 sites)

`+0x448` (the float `fn_81_4C` stores, `lbl_8041AAB8`) and `+0x44f` (the byte `fn_81_5C` returns).
**The two sites below understate the file, in the same way and for the same reason as
`CAtomicAlphaRel.cpp` above: seven members are reached by literal offset** - +0x448, +0x44F, +0x34C,
+0x754, +0x54, and the two `float*`/`unsigned char*` casts the checker does see - and the other
five are reached through a plain `static_cast< char* >`, which the checker keys on casts to a byte
or arithmetic type and does not match. Free functions over a `void*` for the reason above:
`CTryclops` is declared only inside `src/MetroidPrime/TypesMatch.cpp` and has no header here. The
whole block is the thirteen-accessor set `CAtomicAlphaRel.cpp` already carries, so this is the same
debt a fourth time, and **not byte for byte**: `Tryclops`' `.text 0x4C..0xD8` and `AtomicAlpha`'s
`.text 0x10..0x9C` are both 0x8C = 140 bytes and 35 instructions with an identical multiset, and
the diff is that Tryclops runs three `li r3,0; blr` predicates immediately after the byte read
where AtomicAlpha runs two, its third sitting later beside the `li r3,0x1`.
Blocker: the class needs the CActor/CPatterned hierarchy, which is what module 81's `fn_81_178`
(0x178, 0x30C), its own entity loader, needs before the other 93 unclaimed functions can move.
`fn_81_0` adds `+0x7C4` (the claim was extended to 0x0 the same day; `fn_81_10` needs no offset,
it calls `GetBoundingBox` through the same stand-in `CMysteryFlyerRel.cpp` uses).

## `src/MetroidPrime/ScriptObjects/CPillBugRel.cpp` (2 sites)

`+0x448` (the float `fn_48_0` stores, `lbl_8041AAB8`) and `+0x44f` (the byte `fn_48_10` returns).
**The two sites understate the file, as in `CTryclopsRel.cpp` above**: `+0x754` (`fn_48_54`) and
`+0x54` (`fn_48_74`'s three-float copy) go through a plain `static_cast< char* >`, which the
checker does not match. **Kind A, opaque receiver**: free functions over a `void*`, because
`CPillBug` has no header here and its vtable is reached through the thirteen-slot stand-in
`fn_48_90` needs. Blocker: the class needs the CActor/CPatterned/CAi hierarchy, which is what
module 48's `fn_48_130` (0x130, 0x5A4), its own entity loader, needs before the other 59 class
functions can move. When `CPillBug` gets a header these move into it.

## `src/MetroidPrime/ScriptObjects/CRezbitRel.cpp` (1 site)

`+0x54` (the three floats `fn_53_AC` copies out). **As with `CAtomicAlphaRel.cpp`,
`CMysteryFlyerRel.cpp` and `CMediumIngRel.cpp` above, the one site understates the file**: `+0xAC0`
(`fn_53_0`), `+0x754` (`fn_53_9C`), `+0x448` (`fn_53_4C`), `+0x44F` (`fn_53_5C`) and `+0x34C`
(`fn_53_84`'s bit 3) are all reached through a plain `static_cast< char* >` or a
typed-pointer subscript, which the checker does not key on, so the true count is six sites over
six members. It is the same generated accessor block as the rest of the family and **the same debt
a sixth time**, with two differences worth recording: this module's two leading accessors are
`+0xAC0` and then the always-true predicate (`fn_53_0`, `fn_53_8`) where `CMysteryFlyerRel.cpp`'s
are the always-true predicate and then `+0x818`, and the block runs **two** `li r3,0` predicates
where MysteryFlyer runs three - so the three-float copy sits at 0xAC rather than 0xB4 and the claim
ends at 0x168, not 0x170. Both are measured by diffing dtk's
`build/G2ME01/Rezbit/asm/auto_00_00000000_text.s` against `CMysteryFlyerRel.cpp`'s, not by the
`fn_<id>_<off>` names, which say nothing about which function is which. `fn_53_10` needs no offset
because it calls `CPhysicsActor::GetBoundingBox` through the same one-method local stand-in (the
real header adds 0x28 bytes of `.data` and breaks the module hash). **Kind A, opaque receiver**:
free functions over a `void*` because `CRezbit` has no header here, and the only object carrying
the offsets is the module's own retail bytes. Blocker: the same CActor/CPatterned hierarchy that
module 53's entity loader `fn_53_168` (0x168, 0x330) needs before its other 146 class functions
can move.

## `src/MetroidPrime/ScriptObjects/CPlantScarabSwarmRel.cpp` (4 sites)

`+0x184` (twice: a `char*` to an array of 0xB8-byte records, read by `fn_49_0` and `fn_49_3C`),
`+0x17C` (the record count) and `+0xB2` (the flags byte `fn_49_0` tests, `>> 7 & 1`). **The same
four offsets as `CMetareeSwarmRel.cpp` above, at the same four places in two of the same
functions** - module 49's head is module 43's head instruction for instruction, so this is
literally the same debt twice rather than a new one. Blocker: the same. `CPlantScarabSwarm` is
not modelled, and the module's `fn_49_D8` (0xD8, 0x6A0), its own entity loader, needs the
CActor/CPatterned hierarchy before the other 61 unclaimed functions can move.

## `src/MetroidPrime/ScriptObjects/CScriptWallCrawler.cpp` (2 sites)

`+0x54` (three floats) and `+0x44f` (a byte), same shape as `CScriptMetaree` - these are the
generated-loader helpers a REL module's setup code calls.

## `src/MetroidPrime/ScriptObjects/CScriptPuffer.cpp` (1 site)

`+0x44f`, a `bool`. The same generated-loader helper as `WallCrawler` and `Metaree`, which is
why the same offset appears in all three: it is one shared function, emitted per module.

## `src/MetroidPrime/ScriptObjects/CScriptPlayerProxyAccessors.cpp` (1 site)

`+0x15c`, a `void*` member. One accessor, over an unmodelled `CScriptPlayerProxy`.

## `src/Kyoto/Graphics/DolphinCModel.cpp` (3 sites)

`+0x24`, `+0x28`, `+0x2c` in `CModel::CModel`: the section count, the material-set count and the
section-size table of the **CMDL file header**, read out of the resource's byte buffer before any
of it is parsed. The receiver is file data, not a class, so this is kind A by construction; the
code is upstream PrimeDecomp/echoes' (f2dcbf4) unchanged, arriving with the 2026-09-28 merge.
A `SCMDLHeader` struct would name the fields, and would have to be byte-exact with the file.

## `src/MetroidPrime/CMainResetGameState.cpp` (1 site)

`+0x130`, `CGameGlobalObjects::gameState`. **The member is in the header and the offset is still
wrong**, which is why this one is not fixed by naming it: `include/MetroidPrime/CGameGlobalObjects.hpp`
has `CCharacterFactoryBuilder characterFactoryBuilder` (0x28 bytes) commented out at line 69, so
`GameState()` in this tree hands out `objects + 0x108`. Retail's function needs `objects + 0x130`
in three places (`lwz r3,84(r31) ; addi r3,r3,304` twice and `lwz r3,304(r3)`), and the header's own
comment says "everything below it reads 0x28 lower than retail until it does". Blocker: the class
`CCharacterFactoryBuilder` is not modelled, and adding it back would move `gameState`,
`memoryCard` and every later member of a class that `src/MetroidPrime/main.cpp` also uses - the
same per-class offset repair as the rest of kind B. The unit is `NonMatching` at 98.61% and
`gameStateSlot()` is `static inline`, so the whole offset lives in one three-line function.

**Also debt, and the checker cannot see it (2026-09-28):** since the second upstream sync the
port's named `CGameState` layout is `TARGET_PC`-only and MWCC gets upstream's, which has no
`gameOptions`/`x144`/`x178`/`mGameModeType`. So this unit reaches them through
`gameStateAt<T>(state, 0x80 / 0x144 / 0x178 / 0x1A0)`, a template helper the checker's pattern
does not match. It goes away when this unit is rewritten against upstream's `CGameState`.

## `src/WorldFormat/CAreaRenderOctTree.cpp` (4 sites)

`+12`, `+16`, `+20`, `+64` in `CAreaRenderOctTree::CAreaRenderOctTree`: the mesh count, node count,
bounds and bitmap table of the **AROT file header**, read out of the area's byte buffer. Kind A,
like `DolphinCModel.cpp` above; the code is upstream PrimeDecomp/echoes' (c3537e0) unchanged,
arriving with the second upstream sync. The unit is not in the port's `files.cmake`.

## `src/MetroidPrime/ScriptObjects/CGeomBlobV2Accessors.cpp` (3 sites)

`+0x15C` (twice: the `void*` member `fn_25_2544` and `fn_25_254C` return) and `+0x198` (the float
`fn_25_2554` stores). Kind B, unmodelled class: two pointer getters at the same offset and one
float setter, the whole of module 25's accessor block that dtk's FORCEACTIVE list keeps alive.
Blocker: the class needs the CActor/CPatterned hierarchy, which is what module 25's `fn_25_2490`
(0x2490, 0xB4), its own entity loader, needs before the other 115 class functions can move - it
allocates 0x1E0 bytes and calls `fn_25_4290`. When `CGeomBlobV2` gets a header these move into it.

**This block is not the thirteen-accessor family the other landed heads have**, and that is worth
recording because the family's shape is what a reader will reach for first: there is no
`kInvalidUniqueId` store, no `li r3,0` predicate run, no `+0x44f` byte, no `+0x34c` flag and no
`lbl_8041AAB8` / `lbl_8041B758` float pair anywhere in module 25. `fn_25_0` (0x0, 0x1FC) is a real
bone-blend loop, so this module's head is not at 0x0 either.

## `src/MetroidPrime/ScriptObjects/CGeomBlobV2AccessorsTail.cpp` (2 sites)

`+0x18` (the byte `fn_25_2578`, a vtable entry, clears) and `+0x190` (the float `fn_25_256C` stores).
Kind B, same class and same blocker as the section above - the two files are one contiguous
accessor block split in two only because `fn_25_255C` and `fn_25_2564`, the two float *getters*
between them, are not in dtk's FORCEACTIVE list and are dead-stripped if a unit claims them. See
"`CGeomBlobV2`'s accessor block is six functions in two units, and `unit_fit.sh` said it fit" in
`docs/RUNNING_THE_DECOMP.md` for the measurement.

<!-- generated:rel-accessor-carves -->

## The scripted-actor accessor carves (19 modules + `DarkSamus`)

## `src/MetroidPrime/Player/CPersistentOptionsMapInsert.cpp` (1 site)

Offsets 12. A carved accessor from a REL module's generated scripted-actor block; the owning class is not modelled, so the member is reached as a raw offset. See the rationale below the table.

## `src/MetroidPrime/ScriptObjects/AtomicBetaAccessors.cpp` (2 sites)

Offsets 0x54, 0x44F. A carved accessor from a REL module's generated scripted-actor block; the owning class is not modelled, so the member is reached as a raw offset. See the rationale below the table.

## `src/MetroidPrime/ScriptObjects/CDarkSamus.cpp` (3 sites)

Offsets 0x54, 0x34c, 0x44f. A carved accessor from a REL module's generated scripted-actor block; the owning class is not modelled, so the member is reached as a raw offset. See the rationale below the table.

## `src/MetroidPrime/ScriptObjects/CDarkSamusFlags.cpp` (3 sites)

Offsets 0x90c, 0xcd0, 0xd4c. A carved accessor from a REL module's generated scripted-actor block; the owning class is not modelled, so the member is reached as a raw offset. See the rationale below the table.

## `src/MetroidPrime/ScriptObjects/CDarkSamusMembers.cpp` (3 sites)

Offsets 0xa40, 0xa80, 0xab8. A carved accessor from a REL module's generated scripted-actor block; the owning class is not modelled, so the member is reached as a raw offset. See the rationale below the table.

## `src/MetroidPrime/ScriptObjects/CDarkSamusState.cpp` (14 sites)

Offsets 0x984, 0xdec. A carved accessor from a REL module's generated scripted-actor block; the owning class is not modelled, so the member is reached as a raw offset. See the rationale below the table.

## `src/MetroidPrime/ScriptObjects/DigitalGuardianAccessors.cpp` (2 sites)

Offsets 0x54, 0x44F. A carved accessor from a REL module's generated scripted-actor block; the owning class is not modelled, so the member is reached as a raw offset. See the rationale below the table.

## `src/MetroidPrime/ScriptObjects/EmperorIngStage1Accessors.cpp` (2 sites)

Offsets 0x54, 0x44F. A carved accessor from a REL module's generated scripted-actor block; the owning class is not modelled, so the member is reached as a raw offset. See the rationale below the table.

## `src/MetroidPrime/ScriptObjects/EmperorIngStage2TentacleAccessors.cpp` (2 sites)

Offsets 0x54, 0x44F. A carved accessor from a REL module's generated scripted-actor block; the owning class is not modelled, so the member is reached as a raw offset. See the rationale below the table.

## `src/MetroidPrime/ScriptObjects/EyeBallAccessors.cpp` (2 sites)

Offsets 0x54, 0x44F. A carved accessor from a REL module's generated scripted-actor block; the owning class is not modelled, so the member is reached as a raw offset. See the rationale below the table.

## `src/MetroidPrime/ScriptObjects/GlowbugAccessors.cpp` (2 sites)

Offsets 0x54, 0x44F. A carved accessor from a REL module's generated scripted-actor block; the owning class is not modelled, so the member is reached as a raw offset. See the rationale below the table.

## `src/MetroidPrime/ScriptObjects/GunTurretAccessors.cpp` (2 sites)

Offsets 0x54, 0x44F. A carved accessor from a REL module's generated scripted-actor block; the owning class is not modelled, so the member is reached as a raw offset. See the rationale below the table.

## `src/MetroidPrime/ScriptObjects/CScriptIngBlobSwarmRel.cpp` (3 sites)

Offsets 0x17C, 0x184, 0xB2. Two free functions in `IngBlobSwarm`'s module head, written as
`fn_31_0` and `fn_31_3C` next to the `RELMain`/`RELExit` that share the unit: 0x17C is the
blob count, 0x184 the blob-array pointer, 0xB2 the one-bit flag at the end of a 0xB8-stride
element. **Kind A, opaque receiver** - each takes `void* self`, so there is no `this` to
write, and it is retail's own shape. It is a debt in the narrower sense that the owner is
`CIngBlobSwarm`, which this tree does not model: modelling it needs the
`CActor`/`CPatterned` hierarchy, which is exactly what the module's other 53 functions are
waiting on. When `CIngBlobSwarm` gets a header these two move into it and the offsets go
with the class.

## `src/MetroidPrime/ScriptObjects/CDarkTrooperRel.cpp` (2 sites)

`+0x448` (the float `fn_12_8` stores, `lbl_8041AAB8`) and `+0x44f` (the byte `fn_12_18` returns) -
the same two the loader generator emits at the head of every scripted-actor module, and the same two
`CPillBugRel.cpp` above already carries. **The two sites understate the file, for the same reason
as `CTryclopsRel.cpp` and `CPillBugRel.cpp` above**: `+0x7c0` (`fn_12_0`), `+0x754` (`fn_12_50`),
`+0x54` (`fn_12_70`'s three-float copy) and `+0x34c` (`fn_12_38`'s flag bit) go through a plain
`static_cast< char* >` or a subscript, which the checker does not match. **Kind A, opaque receiver**:
free functions over a `void*`, because `CDarkTrooper` has no header here and its vtable is reached
through the thirteen-slot stand-in `fn_12_8C` needs. Blocker: the class needs the
CActor/CPatterned/CAi hierarchy, which is what module 12's `fn_12_12C` (0x12C, 0x614), its own entity
loader, needs before the other 151 class functions can move. When `CDarkTrooper` gets a header these
move into it.

## `src/MetroidPrime/ScriptObjects/IngSpiderballGuardianAccessors.cpp` (2 sites)

Offsets 0x54, 0x44F. A carved accessor from a REL module's generated scripted-actor block; the owning class is not modelled, so the member is reached as a raw offset. See the rationale below the table.

## `src/MetroidPrime/ScriptObjects/KraleeAccessors.cpp` (2 sites)

Offsets 0x54, 0x44F. A carved accessor from a REL module's generated scripted-actor block; the owning class is not modelled, so the member is reached as a raw offset. See the rationale below the table.

## `src/MetroidPrime/ScriptObjects/KrocussAccessors.cpp` (2 sites)

Offsets 0x54, 0x44F. A carved accessor from a REL module's generated scripted-actor block; the owning class is not modelled, so the member is reached as a raw offset. See the rationale below the table.

## `src/MetroidPrime/ScriptObjects/OctapedeSegmentAccessors.cpp` (2 sites)

Offsets 0x54, 0x44F. A carved accessor from a REL module's generated scripted-actor block; the owning class is not modelled, so the member is reached as a raw offset. See the rationale below the table.

## `src/MetroidPrime/ScriptObjects/PuddleSporeAccessors.cpp` (2 sites)

Offsets 0x54, 0x44F. A carved accessor from a REL module's generated scripted-actor block; the owning class is not modelled, so the member is reached as a raw offset. See the rationale below the table.

## `src/MetroidPrime/ScriptObjects/RipperAccessors.cpp` (2 sites)

Offsets 0x54, 0x44F. A carved accessor from a REL module's generated scripted-actor block; the owning class is not modelled, so the member is reached as a raw offset. See the rationale below the table.

## `src/MetroidPrime/ScriptObjects/ShredderAccessors.cpp` (2 sites)

Offsets 0x54, 0x44F. A carved accessor from a REL module's generated scripted-actor block; the owning class is not modelled, so the member is reached as a raw offset. See the rationale below the table.

## `src/MetroidPrime/ScriptObjects/SpankWeedAccessors.cpp` (2 sites)

Offsets 0x54, 0x44F. A carved accessor from a REL module's generated scripted-actor block; the owning class is not modelled, so the member is reached as a raw offset. See the rationale below the table.

## `src/MetroidPrime/ScriptObjects/SporbAccessors.cpp` (2 sites)

Offsets 0x54, 0x44F. A carved accessor from a REL module's generated scripted-actor block; the owning class is not modelled, so the member is reached as a raw offset. See the rationale below the table.

## `src/MetroidPrime/ScriptObjects/StoneToadAccessors.cpp` (2 sites)

Offsets 0x54, 0x44F. A carved accessor from a REL module's generated scripted-actor block; the owning class is not modelled, so the member is reached as a raw offset. See the rationale below the table.

## `src/MetroidPrime/ScriptObjects/WallWalkerAccessors.cpp` (2 sites)

Offsets 0x54, 0x44F. A carved accessor from a REL module's generated scripted-actor block; the owning class is not modelled, so the member is reached as a raw offset. See the rationale below the table.

## `src/MetroidPrime/ScriptObjects/WispTentacleAccessors.cpp` (2 sites)

Offsets 0x54, 0x44F. A carved accessor from a REL module's generated scripted-actor block; the owning class is not modelled, so the member is reached as a raw offset. See the rationale below the table.


### Why these are raw offsets, and what would remove them

Every section above is the same situation: a REL module's scripted-actor
accessor block, carved out as its own `Matching` unit, whose owning class **has no
struct in this tree**. The accessors are the generated `Get/Set` pairs retail emits at
the head of every scripted-actor module - fourteen of them for most modules, byte for
byte the same block - and they are correct in the only sense available: they reproduce
retail's bytes.

They are still wrong on a 64-bit host, and they are still wrong in the way this
document exists to record. A PC port cannot use `self + 0x54`; it needs the member.
So each of these is a unit of real progress (`matched`, `linked`, and a function the
port can now call) that also adds to the debt this file measures.

**What would remove them.** Not more disassembly - the offsets are not in doubt, they
are retail's. Each module needs a struct for its actor type, laid out from retail's own
writes, and then the accessors become ordinary member access. That is a modelling job
per module, not a decompilation job, and it is the same job as Kind C above: the class
has to come from the retail layout rather than from the accessor block that reads it.
The 19 scripted-actor modules here are a good batch to do together, because they share
one generated block and therefore one layout.
