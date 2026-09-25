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
the count here fails the gate. **45 sites in 14 files.**

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

## `src/MetroidPrime/CActor.cpp` (4 sites)

`+0x110` twice, `+0x11c`, `+0x128`, all in `CActor`. `UnkVtable20__6CActorFv` clears the two
reserved-vector counts and bit 7 of the flags byte, and `CActor`'s `+0x110` accessor returns a
pointer to a vector whose member type is not modelled. Blocker: the same per-class offset
repair as the rest of `CActor` - the class is large and several members are carried as filler.

## `src/MetroidPrime/CStateManager.cpp` (1 site)

`+0x9c`, the list link written as a `uintptr_t` because retail's is a host pointer. Blocker:
`CStateManager` is 63 of 239 functions and its header is still being repaired; the fix is a
member of the right type, not a cast.

## `src/MetroidPrime/Enemies/CPatterned.cpp` (1 site)

`+0x754`, `VSlot73()`, which returns `CPatternedAnimEvent&`. Retail's slot 73 is
`addi r3,r3,1876; blr` - an address of a member, so the offset is right and the *member* is
missing. Blocker: `CPatterned`'s constructor is unwritten, so the layout past the accessors is
not established; this cannot be fixed before that.

## `src/MetroidPrime/Enemies/CSwarmBasicsHealthInfo.cpp` (1 site)

`+0x428`, `CSwarmBasics::HealthInfo(CStateManager&)`. Blocker: `CSwarmBasics`' own layout, which
depends on `CPatterned` and `CAi`.

## `src/MetroidPrime/Enemies/CSwarmBasicsOrbitPosition.cpp` (1 site)

`+0x194`, a `CVector3f` member reached by const pointer. Blocker: as above.

## `src/MetroidPrime/ScriptObjects/CFlyerSwarm.cpp` (1 site)

`+0x184`, a `u8*` to the boids array. Blocker: the `CFlyerSwarm` layout, which is the module's
own class and is 7 functions in.

## `src/MetroidPrime/Enemies/CPatternedCtor.cpp` (1 site)

`+0x10`, on the way to the anim token `CPatterned`'s constructor locks: it reads
`CActor::m_modelData` at `this+0x60`, then a member of *that* at `+0x10`, then `+0x110` inside
it. Both indirections are retail's; the class of the intermediate is not identified, so it has no
name to write and the offsets stand in for it. Blocker: identifying it. It is the one member of
`CActor::m_modelData`'s type that the layout work has not reached.

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

## `src/MetroidPrime/ScriptObjects/CScriptWallCrawler.cpp` (2 sites)

`+0x54` (three floats) and `+0x44f` (a byte), same shape as `CScriptMetaree` - these are the
generated-loader helpers a REL module's setup code calls.

## `src/MetroidPrime/ScriptObjects/CScriptPuffer.cpp` (1 site)

`+0x44f`, a `bool`. The same generated-loader helper as `WallCrawler` and `Metaree`, which is
why the same offset appears in all three: it is one shared function, emitted per module.

## `src/MetroidPrime/ScriptObjects/CScriptPlayerProxyAccessors.cpp` (1 site)

`+0x15c`, a `void*` member. One accessor, over an unmodelled `CScriptPlayerProxy`.

## `src/MetroidPrime/ScriptObjects/CScriptForgottenObject.cpp` (1 site)

`+0x396`, the actor's flags struct, reached from a `CActor*` rather than from the actor class.
Blocker: the flags member of `CActor`, which is the kind B problem above.
