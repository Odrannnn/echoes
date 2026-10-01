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
the count here fails the gate. **161 sites in 68 files** (`python3 tools/check_raw_offsets.py`
prints `161 raw-offset site(s) in 68 file(s)`, and the 68 `##` headings below sum to 161; measured
2026-09-30 after the `CMain::StreamNewGameState` body added `src/MetroidPrime/main.cpp` and one
other file. **This line was already stale
before that head, by 3 in 3** - it read 153 in 62 and the tool measured 156 in 65 - and the three
lanes before it had each *appended* their own sentence to this line rather than replacing it, so
it carried a triplicated, sentence-broken pair of totals that were each individually correct and
none of them the whole paragraph. Their revision history is folded into the next paragraph
instead of being repeated here a fourth time).
**This total has gone stale before, and
the doc already recorded how**: `tools/check_raw_offsets.py` measures **165 sites in 69 files**
after the `CMorphBall` head that wrote out `fn_800C9380`/`fn_800C93B0` added a fourth Kind A site
to `src/MetroidPrime/Player/CMorphBall.cpp` (that file's own heading is 3 -> 4; it read 1 -> 3 for
`fn_800CD4B8`/`fn_800CD460` in the head before, and the file count was already 69 before that,
the line above having been written at 68). Quoted from the tool, as above.
**This total has gone stale before, and
the doc already recorded how**: `tools/check_raw_offsets.py` compares the *per-file* counts in
the headings and never this total, so a summary left behind by earlier heads goes unnoticed. It
read 120 in 42 before the `CDarkTrooperRel` head, then 130 in 47 while the tool measured 139 in
50, then 150 in 59 while the tool measured 152 in 61, and earlier 142 in 53 while the tool
already measured 145 in 56. Run the tool and quote it; the headings are the part that is enforced.

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

### Kind A - opaque receivers, kept as retail writes them

## `src/MetroidPrime/Player/CMorphBall.cpp` (4 sites)

- `+12`, in `fn_800CEFD8` (retail 0x800CEFD8, 21insns, matching 100%). **Kind A**, so this one is
  kept rather than fixed. `fn_800CEFD8` is an out-of-line teardown helper taking the object as a
  bare `void*` and is reached from no C++ of ours - it is called only by `fn_800CEF84`, its
  predecessor in the same `CMorphBall.o` chain - so there is no modelled class to write the field
  through and no `this` to reach. The offset is retail's own `lwz r3,12(r30)`: the pointer at +12
  is freed before the object, and the object's own free is gated on the sign of the `short`
  deleting-destructor flag. Rule 1 applies: turning it into a fabricated member would be inventing
  a class, and the surrounding two links (`fn_800CEF84`, `fn_800CEF2C`, both 100%) reach the same
  fields the same way.
- `+12`, in `fn_800CD4B8` (retail 0x800CD4B8, 38 insns). **Kind A**, for the same reason and the
  same shape: it is the block-teardown link of the same teardown chain, written out of a bare
  `void*` because `CMorphBall` has no header field for the pointer it releases. Retail's own
  `lwz r3,12(r30)` at 0x800CD4D8 loads it once and passes it to both release paths -
  `CMemory::Free` when bit 4 of the flag byte at +0 is set, `fn_8033D2F4` when it is clear - so the
  offset is load-bearing for which allocator runs, not a cosmetic read.
- `+24`, in `fn_800CD460` (retail 0x800CD460, 22 insns, matching 100%). **Kind A** again: the
  outermost link of that chain, reached only from `CMorphBall::FindClosestSpiderBallWaypoint`
  (`bl fn_800CD460` at 0x800CD208, the sole call site in the DOL). Retail's `addi r3,r30,24` hands
  the word at +0x18 to `fn_800CD4B8` with a literal -1, which is the same "pass the next link's
  receiver" idiom as `fn_800CEF84` passing `self`.
- `+0x50`, in `fn_800C93B0` (retail 0x800C93B0, 18 insns, matching 100%). **Kind A**: the byte is
  the light's "is in the scene" flag on an object retail copies into a caller's stack local, and
  `CLight` has no header in this port (`include/Kyoto/` has no `CLight.hpp`), so there is no member
  to name and no `this` to reach it through. It is load-bearing in the only sense retail's is: the
  flag selects between two different copies - `fn_80045E18` (all 0x50 bytes, the registering copy)
  when it is clear and the weak `__as__6CLightFRC6CLight` (0x4D bytes, stopping short of +0x50, so
  it leaves the flag alone) when it is set - and the `stb` that sets it is the second half of the
  registering branch.

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

## `src/MetroidPrime/CPhysicsActor.cpp` (3 sites)

Three, all in this one file, and all reaching `CPhysicsActor`'s own modelled members - **kind B**,
so debt, not a pass.

- `+0x238` and `+8`, in `fn_800EA17C` (retail 0x800EA17C, `SetCollisionPrimitive`). The copy
  destination is `mCollisionPrimitive + 8`. `GetCollisionPrimitive` is `addi r3,r3,0x230`, so the
  member is at 0x230, and the copy starts 8 bytes in because it skips the vtable at +0 and
  `CCollisionPrimitive::x4_` at +4. The source offset `+8` is the same skip on the argument.
  Blocker: `mCollisionPrimitive` **is** in the header, but it is `private` and retail's copy is
  not `mCollisionPrimitive = prim` - it copies 32 bytes and leaves `x4_` alone, which the implicit
  assignment does not do. The fix is a `SetCollisionPrimitive` member declared where the private
  one already is (`include/MetroidPrime/CPhysicsActor.hpp:121`, declared and never defined), not a
  header layout change.
- `+0x208` and `+0x1f0`, in `fn_800EA984` (retail 0x800EA984). `mAngularImpulse` and
  `mMoveAngularImpulse`, adjacent `CAxisAngle` members at `CPhysicsActor.hpp:229,231`, proven to
  those two offsets by `ClearImpulses` matching at 100%. Blocker: both are `private` and the
  header has an accessor for the first (`SetAngularImpulseWR`) but none for the second; adding the
  missing accessor is a header edit with no layout consequence, which is why it is not done here.

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

## `src/MetroidPrime/ScriptObjects/CIngBoostBallGuardianRel.cpp` (1 site)

`+0x54` (the three floats `fn_30_74` copies out). **As with `CAtomicAlphaRel.cpp`,
`CMysteryFlyerRel.cpp`, `CEmperorIngStage3Rel.cpp`, `CIngSpaceJumpGuardianRel.cpp` and
`CMediumIngRel.cpp` above, the checker undercounts this file**: `+0x44F` (`fn_30_24`), `+0x34C`
(`fn_30_4C`'s bit 3), `+0x754` (`fn_30_64`, the address of a member), `+0xAEC` (`fn_30_0`) and
`+0xBD8` (`fn_30_8`) are all reached through a plain `static_cast< char* >` or a subscript, which
the checker does not key on, so the true count is six sites over six members. It is the same
generated accessor block as the rest of the family and **the same debt a sixth time**, with three
differences worth recording, each read off the disassembly rather than assumed from a sibling:
this module opens with **two address accessors a word apart**, `addi r3,r3,0xaec` and
`addi r3,r3,0xbd8`, where the rest of the family opens with one address accessor and a
`li r3,1`; it has **no** `GetBoundingBox` wrapper and **no** `lbl_8041AAB8` float store at +0x448,
so it covers fewer members than the rest of the family; and its module-local `.rodata` constant
accessor sits at 0x18 (`lbl_30_rodata_64`, `.float 1`) rather than 0x10. **Kind A, opaque
receiver**: free functions over a `void*` because `CIngBoostBallGuardian` is declared only inside
`src/MetroidPrime/TypesMatch.cpp` and has no header here, and the only object carrying the offsets
is the module's own retail bytes. Blocker: the same CActor/CPatterned/CAi hierarchy that module
30's entity loader `fn_30_130` (0x130, 0x4B4) needs before its other 301 class functions can move.

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

## `src/MetroidPrime/ScriptObjects/CMinorIngRel.cpp` (1 site)

`+0x44F` (the byte `fn_44_28` returns). **As with `CAtomicAlphaRel.cpp`, `CMediumIngRel.cpp` and
the rest of the family above, the checker undercounts this file**: **six** members are reached by
literal offset - `+0x960` (`fn_44_0`), `+0xA4C` (`fn_44_8`), `+0x448` (`fn_44_18`), `+0x44F`
(`fn_44_28`), `+0x34C` (`fn_44_48`) and `+0x754` (`fn_44_60`) - and the tool measures one. Four are
invisible for the reasons recorded above: `+0x960` and `+0xA4C` go through a plain
`static_cast< char* >`, which `BYTE_CAST` does not key on; `+0x34C` is a subscript, which `IGNORE`
skips; and `+0x448` is `reinterpret_cast`-written but shares its line with `lbl_8041AAB8`, which
`IGNORE` also skips. It is the same generated accessor block as the rest of the family and **the same
debt**, with two measured differences worth recording: the claim starts at **0x0** (MinorIng's
`.text` opens on the block, with no destructor chain below it, so unlike `CChozoGhostRel.cpp` there
is nothing to skip), and the block is `CAtomicAlphaRel.cpp`'s re-ordered - the third function is
`li r3,1` where AtomicAlpha's third is the float store, and there is one `li r3,0` predicate after
the byte read where AtomicAlpha has two. **Kind A, opaque receiver**: free functions over a `void*`
because `CMinorIng` is declared only inside `src/MetroidPrime/TypesMatch.cpp` and has no header
here, and the only object carrying the offsets is the module's own retail bytes. Blocker: the same
CActor/CPatterned/CAi hierarchy that module 44's entity loader `fn_44_110` (0x110, 0x844) needs
before its other 194 class functions can move.

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

## `src/MetroidPrime/ScriptObjects/CElitePirateRel.cpp` (2 sites)

`+0x54` (the three floats `fn_15_BC` copies out) and `+0x44F` (the byte `fn_15_5C` returns). **As
with `CAtomicAlphaRel.cpp` above, the two sites understate the file**: `+0xA50` (`fn_15_0`),
`+0x9C0` (`fn_15_8`), `+0x754` (`fn_15_9C`), `+0x448` (`fn_15_4C`'s `lbl_8041AAB8` float store) and
`+0x34C` (`fn_15_84`'s flag bit) go through a plain `static_cast< char* >` or a subscript, which the
checker does not key on, so the true count is seven sites over seven members. It is
`CMysteryFlyerRel.cpp`'s accessor block re-ordered, with two leading member-address accessors and
one extra `li r3,0` predicate. **Kind A, opaque receiver**: free functions over a `void*`, because
`CElitePirate` has no header here. Blocker: the CActor/CPatterned/CAi hierarchy that module 15's
entity loader `fn_15_178` (0x178, 0xACC) needs before the other 217 text functions can move.

## `src/MetroidPrime/ScriptObjects/CParasiteRel.cpp` (2 sites)

`+0x54` (the three floats `fn_47_74` copies out) and `+0x44F` (the byte `fn_47_10` returns). **As
with `CAtomicAlphaRel.cpp` above, the two sites understate the file**: `+0x754` (`fn_47_54`) and
`+0x448` (`fn_47_0`'s `lbl_8041AAB8` float store) are reached through a plain `static_cast< char* >`,
which the checker does not key on, so the true count is four sites over four members. It is the
same generated accessor block as the rest of the family and the same debt again, minus
AtomicAlpha's two leading member-address accessors (+0x8C8, +0x7D8) and its `+0x34C` bit-3 test,
plus two `li r3,0` predicates. **Kind A, opaque receiver**: free functions over a `void*` because
`CParasite` has no header here, and the only object carrying the offsets is the module's own retail
bytes. Blocker: the same CActor/CPatterned hierarchy that module 47's entity loader `fn_47_148`
(0x148, 0x618) needs before the rest of the module can move.

## `src/MetroidPrime/ScriptObjects/CSplitterRel.cpp` (1 site)

`+0x54` (the three floats `fn_75_B4` copies out). **The one site understates the file more than any
other head's**: `+0xD6C` and `+0xE5C` (`fn_75_0`, `fn_75_8`), `+0x754` (`fn_75_A4`), `+0x448`
(`fn_75_54`'s `lbl_8041AAB8` float store), the `+0x34C` bit-3 test (`fn_75_8C`) and the `+0x44F` byte
(`fn_75_64`) are all reached through a plain `static_cast< char* >`/`unsigned char*`, which the
checker does not key on, so the true count is seven sites over seven members. **Kind A, opaque
receiver**: free functions over a `void*` because `CSplitter` has no header here. Blocker: the
CActor/CPatterned hierarchy that module 75's entity loader `fn_75_FC` (0xFC, 0x370) needs.

## `src/MetroidPrime/ScriptObjects/CSplinterRel.cpp` (1 site)

`+0x54` (the three floats `fn_74_5C` copies out). **As with `CSplitterRel.cpp` immediately above,
the one site understates the file**: `+0x7C4` (`fn_74_0`), `+0x754` (`fn_74_4C`), `+0x448`
(`fn_74_8`'s `lbl_8041AAB8` float store), the `+0x34C` bit-3 test (`fn_74_40`) and the `+0x44F` byte
(`fn_74_18`) are all reached through a plain `static_cast< char* >`/`unsigned char*`, which the
checker does not key on, so the true count is six sites over six members. It is the same generated
accessor block as the rest of the family and the same debt again, and **it is the shortest head in
the family so far** - fourteen functions, `.text 0x0..0x118`, where `CIngRel.cpp` is seventeen and
`CAtomicAlphaRel.cpp` eighteen - because three accessor kinds are absent: no
`optional_object<CAABox>` wrapper, no `lbl_8041B758` accessor and no module-local `.rodata`
constant either, so the 0x0C slot at 0x40 is the `+0x34C` bit read and it opens with a single
member-address accessor (`+0x7C4`) where Ing and AtomicAlpha open with two. **Kind A, opaque
receiver**: free functions over a `void*` because `CSplinter` has no header here, and the only
object carrying the offsets is the module's own retail bytes. Blocker: the same
CActor/CPatterned/CAi hierarchy that module 74's entity loader `fn_74_118` (0x118, 0x724) needs
before its other 264 class functions can move.

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

## `src/MetroidPrime/ScriptObjects/CSandBossRel.cpp` (1 site)

`+0x54` (the three floats `fn_55_BC` copies out). **As with `CAtomicAlphaRel.cpp`,
`CMysteryFlyerRel.cpp`, `CIngSpaceJumpGuardianRel.cpp` and `CRezbitRel.cpp` above, the checker
undercounts this file**: `+0x754` (`fn_55_A4`, the address of a member), `+0x448` (`fn_55_4C`'s
float store), `+0x44F` (`fn_55_5C`) and `+0x34C` (`fn_55_8C`'s bit 3) are all reached through a
plain `static_cast< char* >` or a typed-pointer subscript, which the checker does not key on, so
the true count is five sites over five members. It is the same generated accessor block as the rest
of the family and **the same debt a seventh time**, with two differences worth recording, both
measured by diffing `build/G2ME01/SandBoss/asm/auto_00_00000000_text.s` against
`CMysteryFlyerRel.cpp`'s rather than by the `fn_<id>_<off>` names, which say nothing about which
function is which. It opens with the always-true predicate **twice** (`fn_55_0`, `fn_55_8`) where
MysteryFlyer opens with the always-true predicate and then `+0x818`, so this block covers one member
fewer at the front, and it runs **three** `li r3,0` predicates in a row (`fn_55_64`, `fn_55_6C`,
`fn_55_74`) where MysteryFlyer runs two - which pushes `kInvalidUniqueId` from 0x74 to 0x7C and every
accessor above it by 8 bytes, so the block runs 0x0..0x104 and the claim ends at 0x178 rather than
MysteryFlyer's 0x170. `fn_55_10` needs no offset because it calls `CPhysicsActor::GetBoundingBox`
through the same one-method local stand-in (the real header adds 0x28 bytes of `.data` and breaks
the module hash). **Kind A, opaque receiver**: free functions over a `void*` because `CSandBoss`
has no header here, and the only object carrying the offsets is the module's own retail bytes.
Blocker: the same CActor/CPatterned hierarchy that module 55's entity loader `fn_55_178` (0x178,
0x33C) needs before its other 298 class functions can move.

## `src/MetroidPrime/ScriptObjects/CSwampBossStage1Rel.cpp` (1 site)

`+0x54` (the three floats `fn_78_A4` copies out). **As with `CAtomicAlphaRel.cpp`,
`CMysteryFlyerRel.cpp`, `CIngSpaceJumpGuardianRel.cpp`, `CRezbitRel.cpp` and `CSandBossRel.cpp`
above, the checker undercounts this file**: `+0x754` (`fn_78_8C`, the address of a member),
`+0x34C` (`fn_78_74`'s bit 3) and `+0x44F` (`fn_78_44`) are all reached through a plain
`static_cast< char* >` or a typed-pointer subscript, which the checker does not key on, so the
true count is four sites over four members. It is the same generated accessor block as the rest of
the family and **the same debt an eighth time**, with three differences worth recording, all
measured by diffing `build/G2ME01/SwampBossStage1/asm/auto_00_00000000_text.s` against
`CMysteryFlyerRel.cpp`'s rather than by the `fn_<id>_<off>` names, which say nothing about which
function is which. It has **no** `+0x818` member accessor (it opens `li r3,1` and goes straight to
the `GetBoundingBox` wrapper) and **no** `+0x448` float store, so it covers two members fewer than
MysteryFlyer's, and it runs **three** `li r3,0` predicates in a row (`fn_78_4C`, `fn_78_54`,
`fn_78_5C`) where MysteryFlyer runs two - so the block runs 0x0..0xEC and the claim ends at 0x160
rather than MysteryFlyer's 0x170. `fn_78_8` needs no offset because it calls
`CPhysicsActor::GetBoundingBox` through the same one-method local stand-in (the real header adds
0x28 bytes of `.data` and breaks the module hash). **Kind A, opaque receiver**: free functions over
a `void*` because `CSwampBossStage1` has no header here, and the only object carrying the offsets
is the module's own retail bytes. Blocker: the same CActor/CPatterned hierarchy that module 78's
entity loader `fn_78_160` (0x160, 0x314) needs before its other 229 class functions can move.

## `src/MetroidPrime/ScriptObjects/CSwampBossStage2Rel.cpp` (1 site)

`+0x54` (the three floats `fn_79_B4` copies out). **As with `CAtomicAlphaRel.cpp`,
`CMysteryFlyerRel.cpp`, `CIngSpaceJumpGuardianRel.cpp`, `CRezbitRel.cpp`, `CSandBossRel.cpp` and
`CSwampBossStage1Rel.cpp` above, the checker undercounts this file**: `+0x754` (`fn_79_9C`, the
address of a member), `+0x34C` (`fn_79_84`'s bit 3) and `+0x44F` (`fn_79_54`) are all reached
through a plain `static_cast< char* >` or a typed-pointer subscript, which the checker does not key
on, and `fn_79_44`'s `+0x448` store is a fourth of the same kind, so the true count is **five
sites over five members**. It is the same generated accessor block as the rest of the family and
**the same debt a ninth time**, and this one is the one place where a sibling module's block is
*not* interchangeable with `CMysteryFlyerRel.cpp`'s: counted rather than eyeballed, this block is
**63 instructions over 15 accessors, the same 63 over 15 as MysteryFlyer's 0x0..0xFC**, and the two
multisets differ by exactly one `addi r3, r3, 0x818` lost and one `li r3, 0x0` gained - so the
`+0x818` member accessor is swapped for a fourth `li r3, 0x0` predicate (`fn_79_5C`, `fn_79_64`,
`fn_79_6C`) and nothing else moves. `CSwampBossStage1Rel.cpp` above, by contrast, is 59 over 14:
it has neither the `+0x818` accessor **nor** the `+0x448` float store this one has, which is the
whole of the five-instruction difference between the two heads. `fn_79_8` needs no offset because
it calls `CPhysicsActor::GetBoundingBox` through the same one-method local stand-in (the real
header adds 0x28 bytes of `.data` and breaks the module hash). **Kind A, opaque receiver**: free
functions over a `void*` because `CSwampBossStage2` has no header here, and the only object
carrying the offsets is the module's own retail bytes. Blocker: the same CActor/CPatterned
hierarchy that module 79's entity loader `fn_79_170` (0x170, 0x2F0) needs before its other 220
class functions can move.

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

## `src/MetroidPrime/main.cpp` (1 site)

`+0x54`, the base of `SGameStateStreamSource` in `StreamSource()` - the view of the six
`CGameState` members `CMain::StreamNewGameState` copies out of the old save. The five
*member* offsets inside the view are **not** sites: they are declared as real members of a
struct whose own layout is checked, which is the arrangement the rest of this file already
uses (`SGameStateBlock` in `CGameStateBlocks.hpp`, and `SGameGlobalObjectsPtr` in this same
file). The one that cannot be avoided is the `+0x54` that places the view over the object:
`CGameState`'s members are `private`, so naming them from a `CMain` member function needs
either a `friend` declaration - a shared-header change - or a cast. Blocker: the same
`friend`. It is worth 96 bytes of a 532-byte function
(`StreamNewGameState__5CMainFb`, matched 100.00%), so a `friend void
CMain::StreamNewGameState(bool);` in `CGameState.hpp` would delete this site, and nothing
else would change - no layout, no member renamed, no other unit affected.

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

## `src/MetroidPrime/ScriptObjects/CDarkCommandoRel.cpp` (1 site)

The one site the checker sees is `+0x54` (`fn_3_E0`'s three-float copy). **The one site badly
understates the file, for the same reason as `CDarkTrooperRel.cpp` and `CTryclopsRel.cpp` above**:
`+0x448` (`fn_3_7C`'s float store, `lbl_8041AAB8`), `+0x44f` (`fn_3_8C`'s byte) and `+0x34c`
(`fn_3_BC`'s flag bit) are written as a `static_cast< const unsigned char* >(self)[0x44F]`-style
subscript rather than the `reinterpret_cast` the checker keys on, so the true count is **four sites
over four members**. **Kind A, opaque receiver**: free functions over a `void*`, because
`CDarkCommando` has no header here and its vtable is reached through the thirteen-slot stand-in
`fn_3_FC` needs. Blocker: the class needs the CActor/CPatterned hierarchy, which is what module 3's
`fn_3_19C` (0x19C, 0x33C), its own entity loader, needs before the other 167 class functions can
move. When `CDarkCommando` gets a header these move into it.

## `src/MetroidPrime/ScriptObjects/CGrenchlerRel.cpp` (1 site)

The one site the checker sees is `+0x54` (`fn_27_AC`'s three-float copy). **It understates the
file in the same way as `CMediumIngRel.cpp` and `CDarkCommandoRel.cpp` above**: `+0x7C0`
(`fn_27_0`), `+0x754` (`fn_27_8C`), `+0x44F` (`fn_27_44`) and `+0x34C` (`fn_27_74`'s bit 3) are
reached through a plain `static_cast< char* >` or a subscript, which the checker does not key on,
so the true count is **five sites over five members**. It is `CMediumIngRel.cpp`'s block in the
same order plus three predicates, and **no** `lbl_8041AAB8` store at +0x448, so it covers one
member fewer than the rest of the family. `fn_27_8` needs no offset because it calls
`CPhysicsActor::GetBoundingBox` through the same one-method local stand-in (the real header adds
0x28 bytes of `.data` and breaks the module hash). **Kind A, opaque receiver**: free functions
over a `void*`, because `CGrenchler` has no header here and the only object carrying the offsets
is the module's own retail bytes. Blocker: the same CActor/CPatterned hierarchy that module 27's
`fn_27_168` (0x168, 0xE00), its own entity loader, needs before its class functions can move.

## `src/MetroidPrime/ScriptObjects/CIngRel.cpp` (1 site)

The one site the checker sees is `+0x54` (`fn_29_74`'s three-float copy). **It understates the
file in the same way as `CRezbitRel.cpp` and `CGrenchlerRel.cpp` above**: `+0x9DC` (`fn_29_0`),
`+0xAC8` (`fn_29_8`), `+0x754` (`fn_29_64`), `+0x44F` (`fn_29_24`) and `+0x34C` (`fn_29_4C`'s bit 3)
are reached through a plain `static_cast< char* >` or a typed-pointer subscript, which the checker
does not key on, so the true count is **six sites over six members**. It is the same generated
accessor block as the rest of the family and **the same debt again**, with three differences worth
recording, all measured by diffing dtk's `build/G2ME01/Ing/asm/auto_00_00000000_text.s` against
`CRezbitRel.cpp`'s rather than read off the `fn_<id>_<off>` names, which say nothing about which
function is which: it opens with **two** member-address accessors (`+0x9DC` then `+0xAC8`) where
every sibling opens with at most one, so the `li r3,1` lands third at 0x10; its 0x0C slot at 0x18
holds a **module-local** `.rodata` constant (`lbl_29_rodata_64`, `.float 1`) rather than a DOL one,
the same distinction `CDarkCommandoRel.cpp` and `CChozoGhostRel.cpp` already carry; and it has
**neither** the `GetBoundingBox` wrapper **nor** the `lbl_8041AAB8` store at +0x448, so the
three-float copy sits at 0x74 and the claim ends at 0x130, 0x38 below Rezbit's. **Kind A, opaque
receiver**: free functions over a `void*`, because `CIng` has no header here and the only object
carrying the offsets is the module's own retail bytes. No `CAABox` stand-in is needed here at all,
since nothing in the unit names one. Blocker: the same CActor/CPatterned/CAi hierarchy that module
29's `fn_29_130` (0x130, 0xFC8), its own entity loader, needs before its other 250 class functions
can move.

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

## `src/MetroidPrime/ScriptObjects/CSpacePirateRel.cpp` (1 site)

`+0xA94` (`fn_72_18`, the `TUniqueId` copy: `lhz r0, 0xa94(r4)` / `sth r0, 0x0(r3)`). **As with
`CSplinterRel.cpp` and `CIngRel.cpp` above, the one site understates the file**: `+0x920`
(`fn_72_8`), `+0x754` (`fn_72_54`), `+0x448` (`fn_72_30`'s `lbl_8041AAB8` float store) and the
`+0x34C` bit-3 test (`fn_72_48`) are all reached through a plain
`static_cast< char* >`/`unsigned char*`, which the checker does not key on, so the true count is
five sites over five members. It is the same generated accessor block as the rest of the family and
the same debt again. **Kind A, opaque receiver**: free functions over a `void*` because
`CSpacePirate` has no header here, and the only object carrying the offsets is the module's own
retail bytes. **`+0xA94` is the one member of a head in this family that is not a generated
accessor**: it is a `TUniqueId`, measured by `fn_72_7970` and `fn_72_AF70` (both `lhz` it and hand
it to `GetObjectById__13CStateManagerCF9TUniqueId` through a pointer, the second also comparing it
word for word against `kInvalidUniqueId`), and the getter has to take an explicit out-pointer
because retail stores through r3 - a two-byte struct returned in r3 would be one instruction. That
is also the only `reinterpret_cast` in the file, which is why it is the one site the checker sees.
Blocker: the same CActor/CPatterned hierarchy that module 72's entity loader `fn_72_140`
(0x140, 0xACC) needs before its other 241 class functions can move.

## `src/Kyoto/Animation/CAnimCharacterSet.cpp` (1 site)

`+36`, `CAnimationSet::mDefaultTransition` in `fn_8028EBB8` (retail `.text:0x8028EBB8`), written
as `reinterpret_cast< rstl::rc_ptr< IMetaTrans >* >(base + 36)->~rc_ptr()`. The other five
`CAnimationSet` members in the same function and both members of `CAnimCharacterSet` in
`fn_8028E954` go through the class's own `const` accessors and `const_cast`, which cost no
instruction; this one does not, and that is the measurement. mwceppc inlines `~rc_ptr` into its
null test on `this` followed by the `ReleaseData` call, and when `this` is a known member of the
enclosing class it keeps the address in one register for both, so the tail is
`addic. r3,r30,36 / beq / bl` - two instructions short of retail's
`addic. r0,r30,36 / beq / addi r3,r30,36 / bl`. Spelled as a byte offset the compiler cannot see
that the pointer is a member, so it recomputes the address into `r3` after the `addic.` has used
it, and the three instructions match. This is the whole blocker: the member is modelled and its
accessor exists, so this is not an unmodelled member - it is a register allocation mwceppc only
chooses when it cannot see through the access. **Kind B, unmodelled member** in the checker's
taxonomy and the mildest case of it: the fix is a header change with no layout consequence, once
`CAnimationSet` is repaired. It is 1 of 165 sites in 69 files (measured 2026-10-01, added with
`progress-unit-canimcharacterset`).
