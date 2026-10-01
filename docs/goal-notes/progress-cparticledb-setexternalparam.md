# progress-cparticledb-setexternalparam

`kind: progress`, target `main/MetroidPrime/CParticleDatabase`. The unit stays `NonMatching`;
`flip_test.sh` was not run (76 of its 100 functions are still below 100% - the two
`AddParticleEffect` bodies alone are 4252 bytes of TODO).

## Result

All three functions the item names are at **100.00%**, and the unit rose **23 -> 26 / 100**,
fuzzy **21.33% -> 22.39%**, `matched_code` 3528 -> 3776. Tree-wide `build/report.json` matched
**11514 -> 11517**, linked 5590 -> 5590. `./tools/goal_check.sh build/goal/item.json` -> **PASS**
(gate.sh ok; 6 checks).

```
$ python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json
matched  11514 -> 11517   linked 5590 -> 5590   (+3 functions at 100%, 0 units newly linked)
  +100%    main/MetroidPrime/CParticleDatabase :: SetParticleExternalParam__17CParticleDatabaseFUiif
  +100%    main/MetroidPrime/CParticleDatabase :: fn_800A6444
  +100%    main/MetroidPrime/CParticleDatabase :: fn_800A7A7C
no regression
```

Changed: `src/MetroidPrime/CParticleDatabase.cpp` (+80/-3) and `src/MetroidPrime/PortGlobals.cpp`
(+22). No header, no `configure.py`, no `splits.txt`, no `files.cmake`, no `build/goal/` file, no
`asm`. `docs/HANDOFF.md`'s state block was rewritten by `gate.sh` itself, which `goal_check.sh`
runs with `MP_GATE_DOCS_WRITE=1`; the driver discards it.

## The item's `reason` is wrong about the signature, and the re-measurement is the result

`reason` says retail's `SetParticleExternalParam` is a **four**-parameter `(uint, int, int, float)`
and that the tree's three-parameter declaration is a bug. **It is not.** Two independent
measurements:

```
$ grep -n "SetParticleExternalParam" config/G2ME01/symbols.txt
3256:SetParticleExternalParam__17CParticleDatabaseFUiif = .text:0x800A7AA0; // type:function size:0x70
```

`FUiif` is `(unsigned int, int, float)` - three parameters, MWCC's own mangling. And the only
caller in the whole DOL, `CAnimData::SetEffectComponentExternalParam` (0x800271C4, 0xBC bytes),
sets exactly three arguments before the call:

```
80027240:	fmr      f1,f31            ; value
80027244:	lwz      r4,0(r3)          ; components[0]'s name hash
80027248:	mr       r5,r31            ; index
8002724c:	addi     r3,r29,376        ; this->mParticleDB
80027250:	bl       800a7aa0 <SetParticleExternalParam__17CParticleDatabaseFUiif>
```

(`grep -n "800a7aa0" ` over `objdump -d build/G2ME01/main.elf` finds **one** call site, at
0x80027250, so "CAnimData.cpp:676 is the only caller" holds.) There is no fourth argument, and
`index` *is* r5 - the second parameter - which is what the three-parameter declaration already
puts there. So no signature change, and no change to `CAnimData.cpp:676`.

The body is retail's, from `./tools/dis.sh 0x800a7a7c 0x94`:

```
800a7a7c <fn_800A7A7C>:                       36 bytes, 9 instructions
800a7aa0 <SetParticleExternalParam>:          112 bytes, 28 instructions
```

```cpp
void CParticleDatabase::SetParticleExternalParam(uint name, int index, float value) {
  CParticleGenInfo* effect = GetParticleEffect(name);
  if (effect == nullptr) {
    return;
  }
  rstl::CRcPtrData system;
  fn_800A7A7C(&system, effect);
  CElementGen* gen = static_cast< CElementGen* >(system.x0_ptr);
  fn_800A6444(&system);
  gen->SetExternalParam(index, value);
}
```

## The two helpers, and what they are

`fn_800A6444` (0x800A6444, 0x64 = 100 bytes) is **`rstl::rc_ptr<CParticleGen>::ReleaseData()`** -
`if (--*mRefCount <= 0) { delete GetPtr(); delete mRefCount; }`, the body
`include/rstl/rc_ptr.hpp` already carries. Three measurements pin the type and the spelling:

- **`__dt__23CParticleGenInfoGenericFv` (0x800A6038) calls it on `this+0x78`**:
  `addic. r0,r30,120 / bl 800a6444`, preceded by `stw r0,0(r30)` writing a vtable at `this+0`
  and followed by a vtable write at the base class's own slot. So `+0x78` is a member whose
  destruction releases through this function.
- **`IsSystemDeletable__23CParticleGenInfoGenericCFv` (0x800A5A34) is `lwz r3,120(r3)`** followed
  by `lwz r12,108(r12) / mtctr r12 / bctrl` - a **virtual** call through the word at `+0x78`. The
  pointee is polymorphic, which is exactly the 0x64-vs-0x50 difference the map shows between
  `ReleaseData__Q24rstl15rc_ptr<6CIOWin>Fv` (0x64, virtual dtor) and the 0x50-byte
  instantiations. `GetLightId` (`lhz r0,128(r4)`) fixes the rest of the class:
  `CParticleGenInfoGeneric` is 0x84 with `mSystem` (a `rstl::ncrc_ptr<CParticleGen>`) at 0x78 and
  `TUniqueId mLightId` at 0x80 - which is what `include/MetroidPrime/CParticleGenInfoGeneric.hpp`
  already declares, so **no header or layout change was needed or made**.
- So `fn_800A6444` is reached from `CParticleGenInfoGeneric`'s destructor and from every `rc_ptr`
  temporary in this unit's `AddParticleEffect` pair (eleven `bl 800a6444` sites, `grep
  "800a6444"` over the DOL), and the map gives it no mangled name - so the object has to define
  retail's own placeholder for objdiff to pair it. Same reasoning, same convention, as the three
  releases `src/MetroidPrime/main.cpp` spells `fn_80009224` / `fn_80009008` / `fn_800095E4`.

`fn_800A7A7C` (0x800A7A7C, 0x24 = 36 bytes) is the matching out-of-line **`rstl::rc_ptr` copy
constructor**: the same nine instructions as `rstl::CRcPtrData::CopyInto` (0x80049010,
`src/rstl/rc_ptr_copy.cpp`, `Matching`), a distinct symbol, called once - from
`SetParticleExternalParam`. `docs/research/rc_ptr.md`'s measurement applies directly: as a free
function it gets r4/r3 for its two temporaries, which is retail's allocation here.

**The `+0x78` belongs to the callee, not the caller.** Two spellings were measured, and the
distinction is worth keeping because objdiff scores it:

| spelling | `fn_800A7A7C` | `SetParticleExternalParam` |
|---|---|---|
| `fn_800A7A7C(rstl::CRcPtrData* dest, const rstl::CRcPtrData& src)`, caller passes `&effect->mSystem` (offset computed in the caller) | 76.56% - reads `0x0(r4)`/`0x4(r4)` | 90.71% - the caller's `addi r4,r4,0x78` is missing |
| `fn_800A7A7C(rstl::CRcPtrData* dest, CParticleGenInfo* src)`, body reads `src->mSystem` at `0x78`, caller passes `effect` | **100.00%** - reads `0x78(r4)`/`0x7c(r4)` | **100.00%** - `mr. r4,r3` and the `cmplwi` fold into the pointer copy |

A third intermediate spelling, the *same* second parameter as the first row but reached through a
same-layout view struct, also gave the callee 100% and the caller 100% **only** when the view
was indexed as a member (`info.mSystem.x0_ptr`) rather than returned by a helper
(`GenSystem(effect)`); the helper form is what put `addi r4,r4,120` back in the caller. The
difference is register allocation around the call, not logic.

`static_cast<CElementGen*>` is free and is not a guess about the layout: `SetExternalParam` is
`CElementGen`'s, is **not virtual** (0x10 bytes, `slwi r0,r4,2 / add r3,r3,r0 / stfs f1,148(r3) /
blr` - 148 = 0x94 is `mExternalVars`), and a non-virtual call can only be emitted from a
`CElementGen*`, so retail's static type here was `CElementGen*`. The pointer it holds is a
`CParticleGen*` in general, and the effect a character animation drives is the element-gen one -
which is what `mParticleDescs` (a `CGenDescription` map) caches.

## The port link: one new undefined, closed

Writing the real body put `CElementGen::SetExternalParam(unsigned int, float)` in the port's
undefined set, measured **250 -> 251** with `./tools/link_check.sh --strict`, which `gate.sh`
fails. `src/Kyoto/Particles/CElementGen.cpp:3026` already has the body but is out of the port
build (`tools/check_files_cmake.py`'s EXCLUDED list: listing it takes the count 318 -> 370 and
makes `PortLinkStubs.cpp`'s `stub_16`/`stub_17` duplicates), so the definition went into
`src/MetroidPrime/PortGlobals.cpp` - the same place, and the same copy-not-a-stub shape, as the
two `CAreaOctTree::Node` accessors and the eight `TypesMatch` bodies already there, with the
duplicate-on-later-listing caveat written down. `link_check.sh` after: **250 undefined, 0
duplicates, "unchanged from baseline"**. This is a copy of a decompiled body, not a stand-in: it
stores what retail stores.

## Other gates, measured

- `python3 tools/check_symbol_names.py` -> `checked 515 units; 0 declared names are missing from
  their object` (it skips `fn_`/`lbl_` names, which is what the two new symbols are).
- `python3 tools/check_decl_order.py --unit MetroidPrime/CParticleDatabase` -> `ok: 1 unit(s)
  checked, none emits its functions out of retail order`. This needed attention: mwcceppc emits
  definitions in reverse source order, so `SetParticleExternalParam` (0x800A7AA0) is defined
  **before** `fn_800A7A7C` (0x800A7A7C) in the file and `fn_800A6444` (0x800A6444, retail's lowest
  function in the unit) **last**, after `GetTotalBounds`. Getting that backwards permutes the
  unit's bytes without changing a single objdiff percentage - only `flip_test.sh` would catch it,
  and only at flip time.
- `tools/unit_fit.sh MetroidPrime/CParticleDatabase.cpp` -> the unit emits 33 functions retail's
  object does not, 4120 bytes, all COMDAT/template copies (`insert_into__...red_black_tree...`,
  five `free_node_and_sub_nodes__...`, `__dt__Q24rstl24optional_object<6CAABox>Fv`, ...). This is
  pre-existing and unrelated to the change: none of the three functions I added is in the list,
  and the two new symbols are the same size retail has. The oct-tree note in `PortGlobals.cpp`
  records the same shape for a unit that does flip.
- `sha1sum build/G2ME01/main.dol` and all 86 REL sha1s are checked by `gate.sh`, which passed
  (`ok gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)`).

## What is still in this unit, for whoever picks it up

- **`CacheParticleDesc(SObjectTag)` 1.89% and `CacheParticleDesc(CParticleResData)` 3.57%** - the
  previous lane's note characterises these precisely and nothing has changed: five (resp. ten)
  missing `fn_*` list-walkers, and `CParticleDescriptionSPSC`/`CParticleDescriptionSRSC` are still
  only forward declarations, so a `TLockedToken<X>` cannot be constructed yet. The map offsets it
  measured (0/20/40/60/80 and 0/16/32/48/64) are still right.
- **`AddParticleEffect(CParticleData)` 0.12% (3220 B), `AddParticleEffect(CPositionalParticleData)`
  0.39% (1032 B), `UpdateParticleGenDB` 0.21% (1900 B)** - 6152 bytes of TODO, and the reason
  this unit cannot flip. The eleven `bl 800a6444` sites in the two `AddParticleEffect` bodies
  (0x800A8164, 0x800A8850, 0x800A8858, ...) are now pairable, which they were not before this
  item.
- `GetParticleEffect` is 99.01% and `AccumulateBounds` 86.94%; the previous lane measured the
  spellings that do not help, and I did not retry them.
- `CAnimData::SetEffectComponentExternalParam` (`CAnimData.cpp` is `NonMatching`) does not need
  changing for this item - see the signature measurement above.

---

# Run 2 (lane 7, 2026-10-01) - the two `CacheParticleDesc` overloads

## Result

Unit **26 -> 28 / 100** functions at 100%, fuzzy **22.39% -> 23.76%**, `matched_code`
**3776 -> 4100** (+324). Tree-wide `build/report.json` matched **11953 -> 11955**, linked
5728 -> 5728. `./tools/goal_check.sh build/goal/item.json` -> **PASS** (all 6 checks).

```
$ python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json
matched  11953 -> 11955   linked 5728 -> 5728   (+2 functions at 100%, 0 units newly linked)
  +100%    main/MetroidPrime/CParticleDatabase :: CacheParticleDesc__17CParticleDatabaseFRC10SObjectTag
  +100%    main/MetroidPrime/CParticleDatabase :: CacheParticleDesc__17CParticleDatabaseFRCQ214CCharacterInfo16CParticleResData
no regression
```

Changed: `src/MetroidPrime/CParticleDatabase.cpp` (+75/-2) and
`include/Kyoto/Animation/CCharacterInfo.hpp` (+9, five inline accessors on
`CParticleResData`). No `configure.py`, no `splits.txt`, no `files.cmake`, no `build/goal/`
file, no `asm`, no change to any `rstl` header. `docs/HANDOFF.md`'s state block was rewritten by
`gate.sh` itself (the judge runs it with `MP_GATE_DOCS_WRITE=1`); the driver discards it.

The item's own `reason` (`SetParticleExternalParam` and its two helpers) was **already done** in
`42ca6510` and re-measures at 100.00% on this tree, so this run raised the unit's count instead of
re-doing it. This is also the direction the run-11 reviewer asked for: the two `CacheParticleDesc`
overloads are now implemented in this repo's own headers and member names, with the ten callees
they dispatch to written as real, called, generic code.

## THE finding that matters: an unpaired `bl` target name costs objdiff nothing

The previous two notes both assume the ten `fn_*` callees had to be **renamed** for the callers to
match. **They did not.** Measured directly: I gave `GetParticleEffect`'s six lookups an out-of-line
`extern "C"` `fn_800A7DC8` (retail's placeholder for this instantiation's `red_black_tree::find`)
and called it instead of the header's inline `find`:

| `GetParticleEffect` | `fn_800A7DC8` | unit matched |
|---|---|---|
| `map.find(name)` (the tree's own spelling) | 0.00% | 26/100 |
| out-of-line `fn_800A7DC8` called from `GetParticleEffect` | **57.00%** | 26/100 |

`GetParticleEffect` stayed at **99.01%** in both cases, and the unit's count did not move: the six
`bl` instructions score the same whether the relocation target pairs or not. (Both objects are
relocatable, so the `bl` bytes are identical and objdiff does not compare the relocation.) The
reverted state is in this diff - nothing about `fn_*` naming is needed.

So the real blockers were never the names:

- **`GetParticleEffect` 99.01%** - blocks 2-6 put the found node in `r4` where we put it in `r5`
  (both then `cmplw rX,r3` against the 0 in `r3`). Register allocation only. Three spellings tried
  this run, all 99.01%: `const DrawMap::const_iterator it`, `return (*it).second.get()`, and
  `if (!(it == map.end()))` (that last one is **worse**, 95.16%). Retail itself uses r5/r4 in block 1
  and r4/r5 in blocks 2-6, so this is a coin flip inside the allocator, not a source difference.
- **`AccumulateBounds` 86.94%** - ours is 272 B against retail's 280: one extra `b` after the
  `partBounds` test, two extra instructions around the `if (!bounds)` arm, and `addi r4,r1,0x24` /
  `addi r4,r1,0x30` where retail keeps `r1+0x24` in `r31` across both calls. Its `bl` pairs
  nothing it needs: **`fn_800A6670` (156 B) is `rstl::optional_object<CAABox>::operator=`** -
  `cmplw r3,r4; beqlr` (self-assign), `other.mIsValid` at +24, then the 6-word copy in the
  `!m_valid` and the `m_valid` arms and a bare `stb` in the `!other.m_valid` arm - which is exactly
  `include/rstl/optional_object.hpp:31-45`, already in the tree. Retail's copy of the `GetBounds()`
  result into a second stack slot is the **copy constructor**, not the assignment: `operator=` would
  bring the self-compare with it. Three spellings tried this run, all worse: a second named
  `optional_object` copy (63.64%), direct-init (86.94%, unchanged), declare-then-assign (59.23%).

## What the two overloads needed, measured

Both are pure dispatch, and the tree's layout was already right - the `// TODO: correct
CParticleResData's five resource lists` comment on the old body was stale, as the previous note
said. Confirmed three ways, and worth restating because the numbers are easy to get wrong:

```
$ ./tools/dis.sh 0x800a64a8 0xB0        # GetTotalBounds, 100% matched
  addi r4,r31,120 / 140 / 180 / 200     # mFirstDrawLoop, mLastDrawLoop, mFirstDraw, mLastDraw
$ ./tools/dis.sh 0x800A947C 0x70        # CacheParticleDesc(const CParticleResData&)
  mr r3,r31 / mr r4,r30                 # &data.mPart , this->mParticleDescs
  addi r3,r31,16 / addi r4,r30,20       # mSwhc  / mSwooshDescs
  addi r3,r31,32 / addi r4,r30,40       # mElscA / mElectricDescs
  addi r3,r31,48 / addi r4,r30,60       # mSpsc  / mSpscDescs
  addi r3,r31,64 / addi r4,r30,80       # mSrsc  / mSrscDescs
```

`rstl::map` is **20 bytes** (`mHeader` at +8, which `addi rX, map, 8` in every loop confirms), so
the five desc maps sit at 0/20/40/60/80 and the six `DrawMap`s at 100..220 - `CHECK_SIZEOF(
CParticleDatabase, 0xe0)` already said so. `rstl::vector` is 16, so `CParticleResData`'s lists are
at 0/16/32/48/64 and the sixth (`mElscB`) is never touched. **The helper's argument order is
(vector, map)** - `fn_800A9C50` reads its first argument at +4 (`mCount`) and +12 (`mItems`) and
its second at +8 (`mHeader`); getting it backwards costs the caller its `mr`s.

**The one line that took `CacheParticleDesc(SObjectTag)` from 91.13% to 100.00%** is hoisting the
id out of the tag before the switch:

```cpp
const CAssetId id = tag.GetId();
switch (tag.GetType()) { ... }
```

Retail's `lwz r5,4(r4)` (the id) and `mr r4,r3` (`this`) sit between the first `cmpw` and the
first `beq`, so the five cases are only ever `mr r3,r5` + `addi r4,r4,<map offset>` + `bl` + `b`.
Written as `CacheParticleDescOne<...>(tag.GetId(), mParticleDescs)` the compiler re-loads the id in
every arm and keeps `this` in r5 - same instructions, different registers, 91.13%. A five-case
`switch` on the four-character code already produces retail's binary search
(`cmpw`/`bge` on SPSC, then PART, then ELSC/SWHC, then SRSC) with no extra work.

The ten helpers themselves are `static` templates in this file, so they are linker-local COMDATs -
which is why retail's copies have no mangled name either, and why `unit_fit.sh` now reports them
as "extra" at 412 B each against retail's 408. That is the pre-existing shape of this unit
(`insert_into` x5, the COMDAT copies), not a new problem.

`TDesc` only ever appears as a pointer type - `TLockedToken<T>` holds a `CToken` and a `T*`, and
nothing constructs a `T` - so `CParticleDescriptionSPSC` and `CParticleDescriptionSRSC` stay
forward declarations. **The two missing types were never the blocker the previous note recorded.**

## The four `ForParticleDB` loops: a materialisation, not a spelling

`DestroyParticlesForParticleDB` 99.72%, `DeleteAllLightsForParticleDB` 99.75%,
`SuspendAllActiveEffectsForParticleDB` 99.74%, `SetModulationColorAllActiveEffectsForParticleDB`
99.74% - all four are one instruction from 100%, and it is not reachable from the source.
Everything else in all four is byte-identical to retail. The whole difference is retail's loop
test:

```
retail  cmplw r29,r31 ; stw r31,8(r1) ; li r0,0 ; stw r30,12(r1) ; bne ; cmplw r30,r30 ; beq
it != map.end()      cmplwi r30,0  ; li r0,0 ;                     bne ; cmplw r31,r31 ; beq
map.end() != it      cmplw r31,r29 ; stw r31,8(r1) ; li r0,0 ; stw r30,12(r1) ; bne ; cmplw r30,r30 ; beq
```

The two things retail has and we cannot both have are coupled in this compiler: the temporary
`map.end()` is **materialised into the parameter save area** only when it is the *implicit object*
of the member `operator!=` (which always has an address), and the `cmplw` operand order follows
that same implicit object. `it != map.end()` folds the null into `cmplwi` and drops the stores;
`map.end() != it` keeps the stores and reverses the operands. Nothing in between exists. This is
also not a header problem: `red_black_tree::const_iterator::operator!=` is
`mNode != other.mNode || mHeader != other.mHeader` in Prime 1's tree too
(`prime-ref/extern/rstl/include/rstl/red_black_tree.hpp:81`), and the five loops in *this* file
that already match use the same `it != map.end()`. So the four-versus-five split inside one file
is an MWCC register-allocation artifact, not a source difference.

Measured this run on `DestroyParticlesForParticleDB` (all worse than the kept 99.72% unless noted):
`it != map.end()` 81.58% / `while (it != map.end())` 81.58% / `it++` instead of `++it` 81.58% /
`while (true) { if (it == map.end()) break; }` 47.14% / `do {...} while (it != map.end())` 77.56% /
`!(it == map.end())` 81.17% / `!(map.end() == it)` 99.31% (right size, wrong branch polarity) /
`const DrawMap::const_iterator e = map.end(); it != e` 88.89% / `const CParticleDatabase::DrawMap&
m = map; it != m.end()` 81.58% / `DrawMap::const_iterator* p = &it; *p != map.end()` 67.03% /
`it.operator!=(map.end())` 81.58% / `it(it.begin())` 81.58% / hoisted `const auto& e = map.end()`
88.89% / body perturbed three ways (hoisted `gen` local, `(*it).second`, `it->second.get()->`) all
81.58%. Two header experiments, both reverted: dropping the `const_iterator` ctor's unused third
`bool` (Prime 1 has no such parameter) changed nothing, and `operator!=(const_iterator other)` by
value - which should force the materialisation - dropped all nine loops to 58-89% and the unit to
20.54%.

WALL: DestroyParticlesForParticleDB 99.72% - retail materialises the `map.end()` temporary (0 and `&mHeader` into the parameter save area) *and* compares the node first, and MWCC couples the two to the operand of the member `operator!=`; 20 spellings and two header variants measured, none reach 100%

## Other gates, measured

- `python3 tools/check_symbol_names.py` -> `checked 516 units; 0 declared names are missing`.
- `python3 tools/check_decl_order.py --unit MetroidPrime/CParticleDatabase` -> `ok: 1 unit(s)
  checked, none emits its functions out of retail order`. No function was added to or moved in the
  unit: the ten helpers are `static`, so they are local COMDATs, not unit functions.
- `tools/unit_fit.sh MetroidPrime/CParticleDatabase.cpp` -> 78 functions ours does not have,
  13080 bytes, all COMDAT/template copies (five `insert_into`, the ten new
  `CacheParticleDescList`/`One` at 412 B each, `__dt__Q24rstl24optional_object<6CAABox>`, ...).
  Pre-existing shape; retail has the same ten bodies as unnamed locals.
- The port build compiles the new code for the 64-bit host: the first attempt failed the probe with
  `dependent-name 'DescMap::value_type' is parsed as a non-type` from the host compiler, fixed by
  naming `rstl::pair<CAssetId, rstl::rc_ptr<TLockedToken<TDesc> > >` directly instead of the map's
  `value_type`. No 4-byte-pointer arithmetic and no retail offsets appear in the new code; the map
  and vector are reached only through their own member functions.
- `sha1sum build/G2ME01/main.dol` and all 86 REL sha1s: `gate.sh` -> `ninja + build.sha1 ok`,
  `hashes vs config.yml ok`.

## What is still in this unit

- The four `ForParticleDB` loops, one instruction each (above).
- `GetParticleEffect` 99.01% - r4/r5 allocation in blocks 2-6 only.
- `AccumulateBounds` 86.94% - 8 bytes and one extra register; its `bl` is
  `optional_object<CAABox>::operator=`, which the tree already has.
- `AddParticleEffect(CParticleData)` 0.12% (3220 B), `AddParticleEffect(CPositionalParticleData)`
  0.39% (1032 B), `UpdateParticleGenDB` 0.21% (1900 B) - 6152 bytes of TODO, and the reason this
  unit cannot flip. The eleven `bl 800a6444` sites in the two `AddParticleEffect` bodies are still
  pairable now that `fn_800A6444` is a real symbol.
- 60 unnamed `fn_*` at 0.00% - the template COMDATs. Per the finding above they are worth nothing
  to chase by name; they would have to be *written* as out-of-line bodies, and ten of them now are.
