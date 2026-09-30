# progress-prime1-canimsourcereaderbase

`kind: progress`, `target: Kyoto/Animation/CAnimSourceReaderBase`. Prime 1's
`prime-ref/src/Kyoto/Animation/CAnimSourceReaderBase.cpp` used as the reference, adapted to this
repo's own headers (name hashes instead of name strings, `CAnimPOIData*` instead of
`IAnimSourceInfo` owning the streams, 4-byte time component in `CCharAnimTime`).

## Result

`matched_functions` **3 -> 12 of 28**. Every function in retail's object that has a name in
`config/G2ME01/symbols.txt` is now at 100.00%; the other 16 are unnamed template instantiations
(`fn_802A4860` .. `fn_802A5BAC`) whose DOL symbols dtk never named, so no source can ever be
matched against them and 12 is this unit's ceiling. The unit stays `NonMatching` and no flip was
attempted.

Per function, before -> after, and whether Prime 1's source matched unchanged:

| function | before | after | Prime 1's source |
| --- | --- | --- | --- |
| `GetUniqueBoolPOIs` | 73.17% | 100.00% | **structure only**, needed a fix (below) |
| `GetUniqueInt32POIs` | 73.17% | 100.00% | same fix |
| `GetUniqueParticlePOIs` | 73.17% | 100.00% | same fix |
| `VGetBoolPOIList` | 7.00% | 100.00% | **needs edits** - Echoes has no `HasPOIData()` gate and no `sourceInfo` member, the stream comes from `mPOIData` and the helper takes `sourceInfo` + `passedCount` |
| `VGetInt32POIList` | 7.00% | 100.00% | same |
| `VGetParticlePOIList` | 7.00% | 100.00% | same |
| `VGetSoundPOIList` | 7.00% | 100.00% | same |
| `UpdatePOIStates` | 0.81% | 100.00% | body is Prime 1's; the index guard is `index >= 0 && index < <state vector>.size()`, which Prime 1 spells only as `index >= 0`, and bool/int32 assign the whole `rstl::pair` while particle does too |
| `PostConstruct` | 2.55% | 100.00% | needs edits - no `HasPOIData()` branch, no `string_l("")` (uint hash 0), the advance loop is `else` not `else if` |

`VGetBoolPOIState` / `VGetInt32POIState` / `VGetParticlePOIState` were already 100% and are
untouched.

## What the three codegen lessons were

1. **`int count = nodes.size();` as its own statement, declared before the `set`, is worth 27
   points per `GetUnique*POIs`.** With `for (int i = 0; i < nodes.size(); ++i)` mwceppc reloads
   the count inside the loop condition and spends four registers; retail hoists it into r30 and
   spends five. Byte-for-byte otherwise identical, and 188 bytes either way only after the hoist.
2. **`mBoolStates[index].second = v;` is `stbx`, retail is `add` + `stb 4(r)`.** Writing
   `mBoolStates[index] = rstl::pair<uint,bool>(mBoolStates[index].first, v);` instead makes mwcc
   materialise the element address and it matches. Taking a reference or a pointer
   (`rstl::pair<...>* p = &mBoolStates[index]; p->second = v;`) does **not** - both compile to the
   same `stbx`. The self-assignment of `first` is what forces the address into a register.
3. **`const int count = set.size();` in `PostConstruct` is worth 30 points.** Without `const`,
   mwceppc rematerialises each size from the tree's `mCount` field right before the `bl`, and the
   whole function comes out 16 bytes short (198 instructions against retail's 202). With `const`
   it keeps two of them in callee-saved registers and the function is instruction-for-instruction
   retail's.

## Files

- `src/Kyoto/Animation/CAnimSourceReaderBase.cpp` - the four `VGet*POIList` wrappers, the
  `_getPOIList< T >` template they share, `UpdatePOIStates`, `PostConstruct`, and the
  `CBoolPOINode::CopyNodeMinusStartTime` definition (see the port-link note below).
- `include/Kyoto/Particles/CParticleData.hpp` - one inline getter, `GetParentedMode()`. No layout
  change (`CHECK_SIZEOF(CParticleData, 0x18)` still holds, retail reads the mode at node offset
  0x40 and the build still reproduces the DOL).

## The port link, and why `CopyNodeMinusStartTime` is here

`_getPOIList< CBoolPOINode >` calls `CBoolPOINode::CopyNodeMinusStartTime`, which nothing in the
tree defined. Retail defines it in `Kyoto/Animation/CAnimTreeTweenBase.cpp` (0x802AB968, 0x98
bytes) - a unit the port build does not compile, and one that could not be added without opening
its own 16 undefined symbols. With the reference left dangling the gate failed:

```
link_check: STRICT FAIL - regression gate: 251 undefined against a baseline of 250 (GREW)
link_check: 1 symbol(s) this change ADDED to the gap:
  NEW  CBoolPOINode::CopyNodeMinusStartTime(CBoolPOINode const&, CCharAnimTime const&)
```

So the definition is in `CAnimSourceReaderBase.cpp`, spelled exactly like its three siblings in
`CInt32POINode.cpp` / `CParticlePOINode.cpp` / `CSoundPOINode.cpp`. It is here rather than in
`CParticlePOINode.cpp` or `CInt32POINode.cpp` because a unit that emits a function its retail
object does not define can never be `Matching`, and those two can be: this one cannot anyway (16
unnamed functions), `CParticlePOINode` is at 1/3 and `CInt32POINode` is `Matching` at 2/2.
`CAnimTreeSequence.cpp` and `CAnimTreeLoopIn.cpp` already referenced the same symbol and are also
absent from `files.cmake`, which is why the gap only appeared now.

## Measured

```
sha1sum build/G2ME01/main.dol            6ef9b491d0cc08bc81a124fdedb8bfaec34d0010   (retail)
./tools/decomp_build.sh                  All:  30.64% fuzzy, 22.77% matched, 11.74% linked (9948 / 28465 functions)
./tools/probe_sources.sh                 probe: 749 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)
python3 tools/check_symbol_names.py      checked 503 units; 0 declared names are missing from their object
python3 tools/check_decl_order.py        ok: 957 unit(s) checked, 31 permuted, all 31 accounted for in decl_order.md
python3 tools/check_files_cmake.py       every configured DOL object is either in files.cmake or excluded with a reason
python3 tools/check_raw_offsets.py       ok: 152 raw-offset site(s) in 61 file(s)
```

Per-function diff against `build/report.base.json`: **9 functions improved, 0 got worse, 0 units
added or removed** - exactly the 9 this item is about. Project total 9939 -> 9948 matched.

`python3 tools/check_docs_claims.py` reports the HANDOFF state block stale
(`matched 9948 / 28465`, `DOL units 8537 / 16726`); those are the derived counts the judge
rewrites, and the brief forbids editing `docs/HANDOFF.md`.

`./tools/unit_fit.sh Kyoto/Animation/CAnimSourceReaderBase.cpp` reports 32 functions in ours that
are not in retail's object, 5960 bytes: the 4 `_getPOIList` instantiations, the `resize` /
`reserve` / red-black-tree COMDAT copies retail's linker also emits and discards, and
`CopyNodeMinusStartTime`. This is why the unit cannot flip, and it was already true before this
change for the other 28.

## Notes for the next run

- Retail's `UpdatePOIStates` and `PostConstruct` both bounds-check the POI index against the
  state vector's size (`cmpw index, size; bge skip`). That is *not* in Prime 1, which only tests
  `index >= 0`. It is a real second check, not a compiler artefact: it loads the size from
  `mBoolStates + 4` and skips the store.
- `rstl::vector` in this repo is `{mAllocator, mCount, mCapacity, mItems}` - size at `+4`, data at
  `+12` - and `size_type` is `int`, so retail's `cmpw` (signed) comparisons against `.size()` are
  consistent with no cast in the source.
- `rstl::red_black_tree`'s `header` sits at tree `+8` and `mCount` at `+4`, so a
  `rstl::set< pair< uint, int > >` is 0x14 bytes and `begin()` reads `set + 8`. The `it != end()`
  test compiles to `cmplw <set+8>, <set+8>`, which is how retail's tree walks are recognisable.
- Echoes dropped Prime 1's `HasPOIData()` guard: the four wrappers dereference `mPOIData`
  unconditionally and `PostConstruct` calls the three `GetUnique*POIs` with no test.
- Nothing here needed `docs/RUNNING_THE_DECOMP.md`; no new blocker found.
