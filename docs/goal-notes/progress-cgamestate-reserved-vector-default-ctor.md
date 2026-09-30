# progress-cgamestate-reserved-vector-default-ctor

`__ct__11CGMFrontEndFRC11CGMFrontEnd` is now byte-exact. The unit's matched count rose
**102 -> 103 / 116** and nothing anywhere got worse.

## What the item's `reason` asked for, and what I measured instead

The reason was right about the mechanism and wrong about the fix being out of reach. It said:
deleting `mCount(0)` from `rstl::reserved_vector`'s default constructor makes
`__ct__11CGMFrontEndFRC11CGMFrontEnd` byte-exact, but "the header is shared by 20+ class headers
and in this unit it costs `SPreviousGameResults(Reader)` 100->94.20 and `__ct__10CGameStateFv`
100->99.46 ... so it needs an audit of every reserved_vector user, not a one-line edit".

**The audit is the wrong shape.** I made the global edit and measured the whole tree. The
per-unit cost the reason predicted is real but it is not confined to this unit - it is 8
`Matching` units and a REL module:

```
$ python3 - <<'PY'   # reserved_vector() : mCount(0)  ->  reserved_vector() {}
PY
$ python3 tools/report_diff.py build/goal/judge/report.base.json <new report>
matched  10065 -> 10027   linked 4918 -> 4910   (+1 functions at 100%, 0 units newly linked)
  +100%    main/MetroidPrime/Player/CGameState :: __ct__11CGMFrontEndFRC11CGMFrontEnd
  WORSE    main/Collision/CCollisionPrimitive :: InternalCollideBoolean__19CCollisionPrimitiveFRC27CInternalCollisionStructure 100.00% -> 98.52%
  WORSE    main/Kyoto/Animation/CPASAnimInfo :: __ct__12CPASAnimInfoFi 100.00% -> 50.00%
  WORSE    main/Kyoto/Audio/CMidiManager :: __sinit_CMidiManager_cpp 100.00% -> 75.33%
  WORSE    main/Kyoto/Audio/CStaticAudioPlayer :: __sinit_CStaticAudioPlayer_cpp 100.00% -> 75.33%
  WORSE    main/Kyoto/DolphinCDvdFile :: TryARAMFile__8CDvdFileFv 100.00% -> 98.04%
  WORSE    main/Kyoto/Text/CTextRenderBuffer :: __ct__17CTextRenderBufferFQ217CTextRenderBuffer5EMode 100.00% -> 98.06%
  WORSE    main/MetroidPrime/CControlMapper :: __ct__14CControlMapperFi 100.00% -> 94.35%
  WORSE    main/MetroidPrime/CCredits :: __ct__10CPlayMovieFi 100.00% -> 99.75%
  WORSE    Tweaks/MetroidPrime/Tweaks/Tweaks :: __ct__15CTweakPlayerResFRC18SLdrTweakPlayerRes 100.00% -> 0.00%
  ...
  LINKED TOTAL FELL 4918 -> 4910
REGRESSION
```

and `ninja build/G2ME01/ok` fails outright - `dtk shasum -c config/G2ME01/build.sha1` reports
`build/G2ME01/main.dol: FAILED` plus all 86 RELs, so **the DOL stops reproducing retail
altogether**. Net -38 matched and -8 linked to buy +1. An audit of every `reserved_vector` user
would have to restore 8 of those 8 `Matching` functions and the Tweaks REL, which is a much
larger job than this item, and it is not the shape the fix wants.

## The actual difference, measured

Retail's 0x80143C48 (`symbols.txt:5392`, `size:0x8C` = 140 bytes, 35 instructions):

```
80143cb4: 90 1f 00 1c   stw  r0,28(r31)     <- last member written
80143cb8: 48 00 00 1d   bl   80143cd4 <fn_80143CD4>
80143cbc: 80 01 00 14   lwz  r0,20(r1)
...
```

Ours before the fix had **two extra instructions** and a different register allocation:

```
    298c: d0 1f 00 18  stfs f0,24(r31)
    2990: 90 1f 00 1c  stw  r0,28(r31)
    2994: 90 1f 00 20  stw  r0,32(r31)   <- li r0,0 + stw: the mCount(0), which retail has NO store for
    2998: 48 00 00 01  bl                 <- fn_80143CD4
```

`mPlayers` is at `+0x20`. `rstl::reserved_vector`'s default constructor writes `mCount(0)` there
because `mPlayers` is a member and the mem-init list runs before the body. The body's very first
statement is `fn_80143CD4(&mPlayers, &other.mPlayers)`, and `fn_80143CD4` **is** the
`operator=` (it is `CGameState.cpp:849`, written out under an `extern "C"` name precisely because
retal's is unnamed), and it stores the count itself at its own `+0x00` before copying. So the
mem-init store is dead - retail has none - and mwcceppc keeps ours. The dead store also costs a
register (`r0` gets held live from `li r0,0` through the stores), which is why the whole body
came out 11.14 points off rather than 3 instructions.

## The fix

An **additive** tag-selected constructor, following the pattern `rstl/string.hpp`'s `literal_t` /
`SetEmpty()` and `rstl/optional_object.hpp`'s `optional_object_null` already use for the same
"construct without constructing" problem:

- `include/rstl/reserved_vector.hpp` - `struct preserved_t {}` and
  `reserved_vector(preserved_t) {}`, which does not write `mCount`. **The default constructor is
  untouched**, so no other unit's bytes move - which is exactly what the global edit could not say.
- `src/MetroidPrime/Player/CGameState.cpp` - `CGMFrontEnd`'s copy ctor gains
  `mPlayers(rstl::preserved_t())` in its mem-init list.

The comment on the new constructor records the full 8-unit measurement above, so the next session
does not retry the one-line edit.

## Verification

```
$ ./tools/goal_check.sh build/goal/item.json
goal_check: item progress-cgamestate-reserved-vector-default-ctor (progress) target=MetroidPrime/Player/CGameState
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 10065 -> 10066   linked 4918 -> 4918
  ok    check_symbol_names.py
  ok    All:  31.00% fuzzy, 23.30% matched, 11.78% linked (10066 / 28465 functions)
  ok    target rose: main/MetroidPrime/Player/CGameState: 102 -> 103 / 116 functions
  ok    no asm added
goal_check: PASS progress-cgamestate-reserved-vector-default-ctor
```

The object is now identical to retail instruction for instruction - same 140 bytes, same 35
instructions, same registers (`r7` for `other`, `r31` for `self`, the two `__vt__` relocations and
nothing else):

```
$ build/binutils/powerpc-eabi-objdump -d --section=.text build/G2ME01/src/MetroidPrime/Player/CGameState.o
00002924 <__ct__11CGMFrontEndFRC11CGMFrontEnd>:      # retail: 80143c48
    2924: 94 21 ff f0  stwu r1,-16(r1)              # 80143c48  stwu r1,-16(r1)
    ...                                               # 35 instructions, byte-identical
    2994: 48 00 00 01  bl                           # 80143cb8  bl 80143cd4 <fn_80143CD4>
    ...
    29ac: 4e 80 00 20  blr                          # 80143cd0  blr
```

`main.dol` still hashes to `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` and all 86 RELs still
`cmp`-equal to `orig/G2ME01/files/RelProd/` - both are inside `gate.sh`, which passed.

## For the next run on this unit

The unit is 103/116. The 13 still unmatched, all measured from `build/report.json` (not recalled):

| function | % | note |
|---|---|---|
| `StartGameFromFrontEnd__Fv` | 59.35 | the front-end switch; a guess at one callee's shape |
| `__sinit_CGameState_cpp` | 63.35 | static-init pool, shared string pool |
| `fn_801466F4` | 66.63 | `SGameStateBlock::reserve`-shaped, called from the stream ctor |
| `__ct__10CGameStateFR16CBitStreamReader` | 84.14 | the 0x684-byte stream ctor, `CGameStateStreamCtor.cpp` documents the two unexpressible blocks |
| `PutTo__10CGameStateFR16CBitStreamWriter` | 91.90 | |
| `PutTo__18CPersistentOptionsCFR16CBitStreamWriter` | 94.35 | |
| `__ct__18CPersistentOptionsFR16CBitStreamReader` | 95.52 | |
| `PutTo__11CWorldStateCFR16CBitStreamWriterRC18CWorldSaveGameInfo` | 97.30 | |
| `__ct__11CWorldStateFR16CBitStreamReaderUiRC18CWorldSaveGameInfo` | 97.38 | |
| `fn_801465EC`, `fn_80146338`, `__dt__11CGMFrontEndFv`, `LoadGameFileState__10CGameStateFPCv` | unpaired | no score at all: objdiff pairs by name and these are unnamed in retail's symbol table |

The four unpaired ones are worth a look first - they read as "absent", not "wrong", and
`__dt__11CGMFrontEndFv` is 180 bytes sitting right next to a constructor that is now perfect.

NEW: progress | MetroidPrime/Player/CGameState | __ct__10CGameStateFR16CBitStreamReader is 84.14% and `CGameStateStreamCtor.cpp` names two blocks as not expressible in C++ (a memset-shaped fill whose length comes from an uninitialised stack word, and a `gpSimplePool` vtable slot-3 dispatch whose target cannot be identified) - the other 15.9% is not accounted for by those two and may be reachable.
