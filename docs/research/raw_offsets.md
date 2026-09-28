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
the count here fails the gate. **104 sites in 36 files** (measured 2026-09-28, after the upstream merge; an earlier "46 in 15" predated the accessor carves).

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
