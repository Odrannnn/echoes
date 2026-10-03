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
the count here fails the gate. **203 sites in 87 files** (`python3 tools/check_raw_offsets.py --list`
prints `total: 203 raw-offset sites in 87 file(s)`, and the 87 `##` headings below sum to 203;
measured 2026-10-03 by the `carve-8010ee5c` item, which added
`src/MetroidPrime/Carve8010EE5C.c` with its single `+0x54` - the twelfth site of that shape in a
`.c` carve, and the eleventh file to carry one).

**This line has been wrong before, five times over, and the failure was always the same one.**
`tools/check_raw_offsets.py` compares the *per-file* counts in the headings and never this
total, so a summary left behind by an earlier head goes unnoticed: it read 120 in 42, then
130 in 47 while the tool measured 139 in 50, then 142 in 53 while the tool measured 145 in
56, then 150 in 59 while the tool measured 152 in 61, then 161 in 68, then 165 in 69. Each
was individually defensible when written and none of them was the whole paragraph, because
every head *appended* its own sentence here instead of replacing the one before - which is how
three different totals stood in this section at once (161 in 68, 165 in 69, and a narrative of
the earlier drift), each of them once true and none of them the number the tool prints.
**Superseded 2026-10-02:** all three are gone and the single line above is measured rather than
accumulated. Run the tool and quote it; the headings are the part that is enforced, and a reader
who needs the total should re-derive it instead of trusting this paragraph.

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

## `src/Kyoto/Audio/CSfxManager.cpp` (1 site)

- `+0x1810`, in `fn_8029FC34` (retail 0x8029FC34, 43 insns). **Kind A**, so kept rather than
  fixed. `fn_8029FC34` is one of the four unnamed auxiliary-effect record helpers this file now
  carries (`fn_8029FC34`, `fn_8029FCE0`, `fn_8029FD30`, `fn_8029FDAC`, retail 0x8029FC34-0x8029FDAC);
  it takes the record as a bare `void*`, and the record is an Echoes auxiliary-effect manager with
  no header in this port, so there is no member to name and no `this` to reach it through. The
  offset is retail's own two instructions: `addic. r0,r30,6160` then `beq`, which tests the
  *address* `record + 0x1810` for null before the `lwz r5,6160(r30)` that reads the record count
  through it - so the source spells the null test on the address and reloads the address for the
  load, which is what reproduces those bytes. Rule 1 applies: a `SRecordList` member would be
  inventing a class, and the neighbouring `fn_8029FD30` walks the same list from a `void*` for the
  same reason.

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

## `src/MetroidPrime/ScriptObjects/CScriptCoinRestHead.cpp` (1 site)

`+0x2f9`, bit 0 of the same byte, in `fn_58_1BA4` - `CScriptCoin`'s `Touch` override (module 58,
`.text 0x1BA4..0x1C38`, `Object(Matching, "ScriptCoin/...", source=...)` in `configure.py`;
goal item `reclaim-scriptcoin-rest-head-fn`). **The same offset and the same bit as the entry
above**, which is the cross-check that both are one flag byte: retail's `CScriptDebris::Touch`
tests `+0x2f9` too, and `CScriptDebris`'s header calls it `mDieOnProjectile`. Blocker: as above -
`CScriptCoin` is not modelled, so there is no class to put the bit in, and this unit's body must
be a free `extern "C"` definition anyway because the module's symbol table carries the `fn_58_1BA4`
placeholder (a member definition would mangle). The read is the whole first condition, and the
rest of the function reaches `CEntity`'s members through the real class types.

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

## `src/MetroidPrime/ScriptObjects/CLumiteRel.cpp` (1 site)

`+0x54` (the three floats `fn_39_D4` copies out). **As with `CAtomicAlphaRel.cpp`,
`CMysteryFlyerRel.cpp`, `CEmperorIngStage3Rel.cpp` and `CIngSpaceJumpGuardianRel.cpp` above, the
checker undercounts this file**: `+0x448` (`fn_39_68`'s float store), `+0x44F` (`fn_39_78`),
`+0x34C` (`fn_39_A8`'s bit 3) and `+0x754` (`fn_39_B4`, the address of a member) are all reached
through a plain `static_cast< char* >` or a subscript, which the checker does not key on, so the
true count is five sites over five members. It is the same generated accessor block as the rest of
the family, with two differences worth recording, both read off the disassembly rather than assumed
from a sibling: this head has **no leading accessor above the `GetBoundingBox` wrapper** - `fn_39_0`
is at 0x0 - and its predicate run is three `li r3,0` in a row at 0x80/0x88/0x90 before `fn_39_BC`'s
`li r3,1`, where the family alternates. `fn_39_0` needs no offset because it calls
`CPhysicsActor::GetBoundingBox` through the same one-method local stand-in (the real header adds
`.data` and breaks the module hash). Blocker: the same CActor/CPatterned hierarchy module 39's
entity loader `fn_39_190` (0x190, 0x5A8) needs before the module's other 139 functions can move.

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

## `src/MetroidPrime/ScriptObjects/CIngBoostBallGuardian194C.cpp` (1 site)

`+0x18`, the six-word block `fn_30_194C` writes between the two float groups. Free function over a
`void*` for the reason in `CIngPuddleRel.cpp` above: it is module 30's structure initialiser and
takes the object being built in r3, so there is no `this` and no class here. Blocker: the object is
CIngBoostBallGuardian's own 0x38-byte sub-record and this tree models none of the module's entity
code - the same job `fn_30_130`, the module's own entity loader, needs before any of it can move.

## `src/MetroidPrime/ScriptObjects/CIngBoostBallGuardianC6AC.cpp` (1 site)

`+0x6B4`, the word `fn_30_C6B8` compares against 3. Free function over a `const void*` for the
reason above. Blocker: the member is 0x6B4 bytes into the module's entity and this tree models none
of it; `fn_30_C6AC`, its neighbour in the same unit, reaches the flag byte at `+0x11ED` through a
subscript, which the checker does not key on.

## `src/MetroidPrime/ScriptObjects/CIngBoostBallGuardianPredicates.cpp` (7 sites)

`+0x115C`, `+0x11C0`, `+0x083C`, `+0x1130` and the three `+0x0AE4` reaches behind `fn_30_C1B0`,
`fn_30_C1D8`, `fn_30_C22C` and the equality predicates `fn_30_C1F0`/`C204`/`C218` - seven sites, one
per function. Free functions over a `const void*` for the reason above. **It is the same debt as
`CIngBoostBallGuardianRel.cpp` below and `CIngPuddleRel.cpp` above, now seven members wide**: the
whole of module 30's accessor block, reached by offset because the class is not modelled. Blocker:
`+0x1130` and `+0x115C` are past 0x1000 bytes into the object, so this cannot be written any other
way until `fn_30_130` - the module's own entity loader - is decompiled and `CIngBoostBallGuardian`
can be given a header.

## `src/MetroidPrime/ScriptObjects/CIngBoostBallGuardianD2xx.cpp` (2 sites)

`+0xAE4` and `+0x11BC`, the switch word and the float `fn_30_D250` tests before it calls
`CPatterned::AddToRenderer`. Free function over a `void*` for the reason in
`CIngBoostBallGuardian194C.cpp` above: the receiver is the module's own entity, which this tree
does not model, and the two offsets are members `CIngBoostBallGuardianPredicates.cpp` reaches the
same way (`+0xAE4` three times there). Blocker: the same one - `fn_30_130`, the module's entity
loader, before the class can have a header. The third function in the unit,
`fn_30_D2C0`, spells no raw offset (it forwards its two arguments unchanged), and `fn_30_D230`,
the fourth-shaped one, spells none either.

## `src/MetroidPrime/ScriptObjects/CIngBoostBallGuardian13B7C.cpp` (1 site)

`+0x4F6`, the halfword `fn_30_13B7C` copies to its destination. Free function over a `const void*`
for the reason above - and with the destination in r3 and the object in r4, the argument order is
the measurement, not a member function's `this`. The other three functions in the unit reach the
byte at `+0x5D8` and the record at `+0x570` through subscripts, which the checker does not key on.
Blocker: the same missing `CIngBoostBallGuardian` class.

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

## `src/MetroidPrime/ScriptObjects/CSandBossRelTail3.cpp` (2 sites)

- `+0x1F8` and `+0x238`, both in `fn_55_10B48` (module 55, `.text 0x10B48`, 0x7C = 124 bytes,
  `Object(Matching, ...)` in `configure.py`; `progress-twin-rel-sandboss`). The class is
  `CGameProjectile` and both are subobjects retail destroys in place: `CProjectileWeapon` at
  +0x238 through the imported `__dt__17CProjectileWeaponFv`, the member at +0x1F8 through
  **this module's own** out-of-line copy at 0x480C (dtk's `fn_55_480C`, 0x54). **Kind B, and the
  member spelling is measured to be worse than the offsets**: compiling the DOL's own
  `CGameProjectile::~CGameProjectile() {}` (`src/MetroidPrime/Weapons/CGameProjectile.cpp:302`)
  for this module emits `__dt__15CGameProjectileFv` (0x7C, right size) *plus* an out-of-line weak
  `__dt__Q24rstl56optional_object<Q218CImpactVisorEffect15SParticleEffect>Fv` (0x5C), a local
  `destroy<...>` (0x48) and an 0x8C-byte local `__vt__15CGameProjectile` in `.data` - measured
  with a scratch compile of that one line under the module's flags and `nm --defined-only`. Every
  one of those extra definitions moves bytes, and the +0x1F8 teardown would resolve to the DOL,
  not to the module's 0x480C that retail's `bl` names. The function must also stay a free
  `extern "C"` definition: retail's symbol is the anonymous `fn_55_10B48`
  (`config/G2ME01/rels/SandBoss/symbols.txt:307`), and a member definition emits
  `__dt__15CGameProjectileFv` and pairs with nothing in objdiff.
  Removed by: nothing available to a carve. It goes if the module's copy at 0x480C is ever named
  by a header (it would have to be the member's own type) or the two claims merge, a change this
  unit does not need and must not make. The other three functions in the claim spell no counted
  raw offset: `fn_55_10AE8` and `fn_55_10C18` store a vtable at +0 and call a destructor on the
  receiver unchanged, and `fn_55_10BC4` reaches `+0xC`, a single-character offset the checker's
  two-character minimum does not count (the `CFlyerSwarmRelTail.cpp` section above).

## `src/MetroidPrime/ScriptObjects/CSandBossRelTail2.cpp` (3 sites)

- `+0x18`, `+0x5C` and `+0xA0`, all three in `fn_55_4D78` (module 55, `.text 0x4D78`, 0x70 = 112
  bytes, `Object(Matching, ...)` in `configure.py`; `progress-twin-rel-sandboss`). These are the
  three `CMayaSpline` members of `CCameraShakerData`
  (`include/MetroidPrime/Cameras/CCameraShakerData.hpp`, `CHECK_SIZEOF(CCameraShakerData, 0xf4)`:
  `mHorizontalMotion` +0x18, `mForwardMotion` +0x5C, `mVerticalMotion` +0xA0), destroyed in reverse
  declaration order - the instruction is `addi r3,r30,0xA0 / li r4,-1 / bl fn_55_4DE8`, three
  times. **Kind B, and the blocker is access, not layout**: all three members are `private` and the
  class has no accessor for them, so a free function cannot name them (`&self->mVerticalMotion`
  fails with `illegal access to protected/private member`, the same wall `CTweakPlayerRes` above
  measures). The function also has to stay a free `extern "C"` definition: retail's symbols are the
  anonymous `fn_55_4D78`/`fn_55_4DE8`
  (`config/G2ME01/rels/SandBoss/symbols.txt:305-308`, the module's own names for those addresses),
  and a member definition would emit `__dt__17CCameraShakerDataFv` and pair with nothing in
  objdiff. The bodies are the DOL's own, matched at 100.00% in `build/report.json`:
  `__dt__17CCameraShakerDataFv` (0x8009D174, `main/MetroidPrime/TypesMatch`),
  `__dt__11CMayaSplineFv` (0x800327FC) and `fn_80032854` (0x80032854), the last two in
  `main/MetroidPrime/Factories/Carve80032774`.
  Removed by: nothing available to a carve. It goes if `CCameraShakerData.hpp` gets public
  accessors for the three splines (or the destructor is claimed under its own mangled name), a
  header change this unit does not need and must not make. The other two functions in the claim
  spell no counted raw offset: `fn_55_4E40` reaches `+0xC` and `fn_55_4DE8` reaches `+8`, both
  single-character offsets the checker's two-character minimum does not count, the rule the
  `CFlyerSwarmRelTail.cpp` section above states.

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

## `src/MetroidPrime/ScriptObjects/CGunTurretBaseTriggers.cpp` (2 sites)

`+0x7E8` (`fn_28_9050` returns) and `+0x808` (`fn_28_9048` returns), the two byte accessors of
module 28's trigger block, claimed with the `Attacked` forwarder next to them
(`fn_28_9028`, `.text 0x9028..0x9058`). **Kind A, opaque receiver**, like `GunTurretAccessors.cpp`
in the same module: each takes a `const void*`, so there is no `this` to write and the offset is
retail's own `lbz r3,0x7e8(r3)` / `lbz r3,0x808(r3)`; what is different is that **these two are not
part of the loader generator's fourteen-accessor block** - that one reads +0x54, +0x44F and the rest
and sits at the head of the module, while these sit at 0x9028 above the class's own code - so they
are this module's own two flag bytes, at offsets no other module's block uses. Blocker: the owner is
`CGunTurretBase`, named by the module's own mangled `"TCastToPtr<14CGunTurretBase>__FP7CEntity"`
(`fn_28_856C`, `fn_28_90C4`) and declared only inside `src/MetroidPrime/TypesMatch.cpp`, so there is
no header to name them through. Modelling it is the same `CActor`/`CPatterned`/`CAi` hierarchy work
that module 28's own entity loader `fn_28_7B98` (0x7B98, 0x420) is waiting on; when
`CGunTurretBase` gets a header these two move into it and the offsets go with the class.

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

## `src/MetroidPrime/ScriptObjects/CCommandoPirateRel.cpp` (1 site)

The one site the checker sees is `+0x54` (`fn_9_AC`'s three-float copy). **It understates the file
in the same way as `CDarkCommandoRel.cpp` and `CGrenchlerRel.cpp` above**: `+0x928` (`fn_9_0`),
`+0x754` (`fn_9_9C`), `+0x34C` (`fn_9_90`'s bit 3), `+0x448` (`fn_9_58`'s `skDamageHitTime`
store) and `+0x44F` (`fn_9_68`'s byte) are reached through a plain `static_cast< char* >` or a
subscript, which the checker does not key on, so the true count is **six sites over six members**.
**Kind A, opaque receiver**: free functions over a `void*`, because `CCommandoPirate` has no header
here, and `fn_9_C8` reaches the vtable through the thirteen-slot stand-in
`CCommandoPirateDispatch` it needs. Its block is `CIngSpaceJumpGuardianRel.cpp`'s order, not the
family's usual one, which is why `fn_9_90` is the +0x34c flag test rather than a
`lbl_8041B758`-style accessor; `+0x928` and `+0x754` are the two member-address accessors. Blocker:
the same CActor/CPatterned/CAi hierarchy module 34's needs, which is what module 9's own entity
loader `fn_9_168` (0x168, 0x76C) must be written before the other 230 class functions can move.
When `CCommandoPirate` gets a header these move into it.

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

## `src/MetroidPrime/ScriptObjects/SporbDtors.cpp` (6 sites)

Offsets 0x584, 0xA0, 0x5C, 0x18, 0x238, 0x1F8. **Kind A**, for the same reason as every other
section on this page and the same situation as `SporbAccessors.cpp` above, only for a different
part of the module: this is Sporb's (module 76) deleting-destructor chain, `.text 0x11B34..0x11D14`,
carved out as its own `Matching` unit. The receivers are bare `void*`, so there is no `this` and no
modelled class to write a field through: `+0x238` and `+0x1F8` are the two members
`~CGameProjectile` tears down, `+0xA0`/`+0x5C`/`+0x18` the three `CMayaSpline` members
`~CCameraShakerData` tears down, and `+0x584` the three floats `fn_76_11CCC` copies. The classes
these belong to have no struct in this tree, and the same blockers as the rest of this table
apply: modelling them is a per-class repair, not a one-line header edit. Rule 1 applies - turning
these into fabricated members would be inventing a class.

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
`CAnimationSet` is repaired. It is 1 of the 168 sites in 72 files measured at the top of this
file; this section read 165 in 69 when it was written (2026-10-01, with
`progress-unit-canimcharacterset`), and that figure is history rather than the current total.

## `src/MetroidPrime/Carve800E9C14.c` (1 site)

- `+0x2C8`, in `fn_800E9C14` (retail 0x800E9C14, `.text 0x800E9C14..0x800E9C1C`, 8 bytes, 1
  function, 100.00% matched, `Object(Matching)` in `configure.py`). The whole carve is the
  two instructions `lwz r3, 0x2c8(r3) / blr` - retail's ordinary out-of-line getter shape, and
  one the asm is full of - returned as a pointer by
  every one of its six `bl fn_800E9C14` sites (two in `MetroidPrime/CGroundMovement.s`, two in
  `auto_03_80218E28_text.s`, one in `Player/CPlayerDynamics.s`, one in `Player/CPlayer.s`), each
  null-testing the result and then handing it to a `CAABox`-shaped call with
  `mskNullBox__6CAABox` as the argument (`mr. r28, r3 / beq` at 0x80186580, `cmplwi r3, 0x0` at
  0x8012A488, `fn_80258A68` / `fn_80258970` at 0x8001391C / 0x8001392C).
  **Kind B, unmodelled member, reached through a free function** - debt, not a pass. The receiver
  is a `CPhysicsActor` and the member is retail's own `CPhysicsActor::unk2`, the lazily
  heap-allocated 60-byte object retail constructs from `mskNullBox` (`__nw__FUl(0x3c, ...)`),
  with its `x254_` flag byte at 0x2C4 and the pointer at 0x2C8; both offsets are measured in
  `docs/goal-notes/progress-prime1-cphysicsactor.md:95-99`. The header has the member as a bare
  `void* unk2` (`include/MetroidPrime/CPhysicsActor.hpp:249`) and no accessor for it, so a
  `CAABox*`-typed accessor would delete this site.
  Blocker, and it is the mild kind: **not** the per-class layout repair the other kind B entries
  need - the member is already declared and `CHECK_SIZEOF(CPhysicsActor, 0x2d0)`
  (`CPhysicsActor.hpp:252`) already holds, so a getter is a header edit with no layout
  consequence. The one thing still unmeasured is the member's offset *in this build*: only the
  class's total size is asserted, never its per-member offsets, so the accessor is worth landing
  with `offsetof(CPhysicsActor, unk2)` checked first. The function is a `.c` carve taking
  `void* self` rather than a C++ member because retail names none of it and the claim is a
  standalone eight-byte range - the receiver is modelled, but nothing in this unit is a
  `CPhysicsActor`, so the offset has to be spelled for now.

## `src/MetroidPrime/Cameras/Carve801E7C14.c` (1 site)

- `+12`, in `fn_801E7C58` (retail 0x801E7C58, `.text 0x801E7C14..0x801E7CB0`, 0x58 = 88 bytes,
  22 instructions, 100.00% matched, `Object(Matching)` in `configure.py`). The instruction is
  `addi r3,r30,12 / li r4,-1 / bl __dt__17CCameraShakerDataFv`: the destructive step of the
  deleting-destructor chain the carve reproduces, and the address of that member is what the call
  needs. **Kind A, opaque receiver** - the class is unnamed and modelled by nothing: it is known
  only from the DOL bytes, which say the member at `+0xC` is a `CCameraShakerData`
  (`__dt__17CCameraShakerDataFv`, 0x8009D174, destroys three `CMayaSpline`s at +0x18/+0x5C/+0xA0,
  which is `include/MetroidPrime/Cameras/CCameraShakerData.hpp`'s layout at
  `CHECK_SIZEOF(..., 0xf4)`), that `fn_801E7D88` walks elements of it in 0x104-byte strides
  (`lwz r0,0x0(r29) / cmpw r30,r0 / blt` with `addi r31,r31,260`), and that `fn_801E7CB0`
  (`0x801E7CB0`, 0x64 = 100 bytes) copies the element with `lbz r0,256(r31) / stb r0,256(r30)` -
  the 0xC + 0xF4 = 0x100 member plus that byte is the stride, and it reads the same `+0xC` through
  `bl __as__17CCameraShakerDataFRC17CCameraShakerData`. The same run's `fn_801E7EC0`
  (`include/MetroidPrime/CCameraShakeManager.hpp:11`) calls the same body on a stack temporary at
  `r1+0xFC`. Retail's twin is
  `CPlayerState::SPersistentState::~SPersistentState()` (0x80009508, `src/MetroidPrime/main.cpp:2208`),
  the same 22 instructions with the same `addi r3,r30,0xC` on a member that class *does* model - so
  the offset is not a guess, it is the one member this function's bytes name. The carve cannot spell
  it as a member because no header owns the class, and inventing one would invent a class for bytes
  nothing else names; the unit that next claims the run around 0x801E782C can, and then this site
  goes with it.

## `src/MetroidPrime/Factories/Carve80032774.cpp` (5 sites)

- `+0x214`, `+0x1D0`, `+0x18C`, `+0x148`, `+0x104`, all in `fn_80032774` (retail 0x80032774,
  `.text 0x80032774..0x80032A98`, 0x88 = 136 bytes, 34 instructions, 100.00% matched,
  `Object(Matching)` in `configure.py`; `carve-80032774`). The five instructions are
  `addi r3,r30,off / li r4,-1 / bl __dt__11CMayaSplineFv` - the destructive step of the
  deleting-destructor chain this carve reproduces - and they are the five `SLdrSpline` members of
  `CTweakPlayerRes` (`typedef CMayaSpline`, stride `CHECK_SIZEOF(CMayaSpline, 0x44)`), destroyed in
  reverse declaration order. **Kind B, unmodelled member in a modelled class** - debt, not a pass.
  The class *is* modelled and its offsets are retail's: `include/MetroidPrime/Tweaks/
  CTweakPlayerRes.hpp:55-59` declares `mBallTransitionSpline1..4` and `mMovementControlSpline` in
  that order with `const SLdrTweakPlayerRes* mData` after them, `CHECK_SIZEOF(CTweakPlayerRes,
  0x25c)`, so 0x258 - 5 x 0x44 = 0x104 fixes the first member at the offset the bytes show, and
  `mData` at +0x258 is measured independently in `docs/research/tweak_globals.md` row 13
  (the 604-byte class, `fn_82_AB4` constructing the same five splines at +0x104 .. +0x214).
  What stops the member spelling is **access, not layout**: the five splines are `private` in that
  header (the first member is private and the class has no `friend` and no accessor for them), so a
  free function cannot name them - `__dt__11CMayaSplineFv(&self->mMovementControlSpline, -1)`
  fails with `illegal access to protected/private member`, measured this run. And the function has
  to stay a free `extern "C"` definition: retail's symbol is the anonymous `fn_80032774`
  (`config/G2ME01/symbols.txt:962`), the deleting destructor `.ctors` 0x80032610 registers for
  `gpTweakPlayerRes`, and a member definition would emit `__dt__18CTweakPlayerResFv` and pair with
  nothing in objdiff.
  Removed by: nothing available to a carve. It goes if `CTweakPlayerRes` gets a public accessor (or
  the class's own destructor is claimed under its own mangled name), which is a header change this
  unit does not need and must not make. The other six functions in the claim spell no raw offset:
  two reach their members through the real class types (`delete self->mPtr` on
  `single_ptr< CTweakPlayerGun >`/`< CTweakParticle >`), and the remaining two use `+ 8` and `+ 0xC`
  on `void*` receivers, which the checker's two-character minimum does not count as field accesses.

## `src/MetroidPrime/ScriptObjects/CFlyerSwarmRelTail.cpp` (1 site)

- `+0x18`, in `fn_21_18D4` (module 21, `.text 0x18D4`, 0x58 = 88 bytes,
  `Object(Matching, ..., mw_version="GC/2.7")` in `configure.py`; `progress-twin-rel-flyerswarm`).
  **Kind A, opaque receiver.** The instruction is `addi r3,r30,0x18 / li r4,-1 / bl fn_21_14E4`,
  the destructive step of a deleting-destructor chain - the shape `CMorphBall.cpp`'s
  `fn_800CD460` carries verbatim with the same `+24`, and its twin
  `src/MetroidPrime/Player/CMorphBall.cpp` is written the same way. The function takes a `void*`
  receiver with no `this` and no class, and the member at +0x18 is torn down by this module's own
  unclaimed `fn_21_14E4` (0x14E4, 0x50), so there is no header to put a field in and inventing one
  would be inventing a class for a record nothing else in this tree names. The other seven
  functions in the claim spell no raw offset: the record and the count+array vector they walk are
  declared in the file (`struct SFlyerElem`, `struct SFlyerVec`), and the class's deleting
  destructor reaches its vtable through the module's own `lbl_21_data_4` data label.

## `src/MetroidPrime/Carve8010EE5C.c` (1 site)

- `+0x54`, in `fn_8010EE5C` (retail `.text:0x8010EE5C..0x8010EEEC`, 0x90 = 144 bytes, 2 functions,
  100.00% matched, `Object(Matching)` in `configure.py`; `carve-8010ee5c`). **Kind A, opaque
  receiver**, so kept rather than fixed. The carve's first function is a vtable entry of the vtable
  `lbl_803B4BE0`, the vtable of an **unnamed** `CActor` subclass - retail names the class nowhere,
  and the only thing that reaches either body is that table, a dtk-only data object with no claimed
  unit - so the file is a `.c` unit taking `void* self` and there is no class to write a member
  through. The offset is retail's own `addi r4,r4,0x54` and nothing else: it points at
  `CActor::mPosition`, measured from the far side as well
  (`include/MetroidPrime/CActor.hpp:290`, `mutable CVector3f mPosition;  // x54`), and the callee
  `__ct__6CAABoxFRC9CVector3fRC9CVector3f` (0x802F8CC4) takes `CVector3f const&` twice, which is why
  the same pointer is passed for both corners. So the member *is* identified - it is
  `CActor::mPosition` - but the *class* is not, and Rule 1 is about the class: naming a
  `CWhatever::mPosition` would be inventing a class for retail bytes, exactly what
  `CSfxManager.cpp`'s `fn_8029FC34` above declines to do. The carve's second function,
  `fn_8010EECC`, spells no offset at all. Blocker, and the mild kind: this goes when the class is
  modelled, which is the same per-class layout repair as Kind C above and not a one-line header
  edit - the vtable has to be read first, to learn the members between +0x00 and +0x54.

  It is the **third** raw offset in a `.c` unit and the third file to carry one, 1 site each -
  `python3 tools/check_raw_offsets.py --list | grep -E "\.c\s+[0-9]+$"` prints exactly those three,
  summing to 3 of the 203 - after `src/MetroidPrime/Carve800E9C14.c` (`+0x2C8`) and
  `src/MetroidPrime/Cameras/Carve801E7C14.c` (`+12`).  Both of those are the same situation in a
  different guise: a carved out-of-line accessor over a receiver whose class *is* modelled but whose
  member is not, so neither is removable without a header change either.
