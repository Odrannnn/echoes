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
