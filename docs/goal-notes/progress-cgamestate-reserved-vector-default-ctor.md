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

---

# Run 2 (lane 7, 2026-10-02)

## Why this item came back: the eighth sync deleted the previous run's fix

`git log -- include/rstl/reserved_vector.hpp` on this tree: `fc61ccbc` (the previous run) added
`preserved_t`, and **`9f2d849f` "sync: merge upstream PrimeDecomp/echoes 8bb7bd0f (eighth sync)"
removed it** - the merge took upstream's `reserved_vector.hpp` wholesale. So `include/rstl/
reserved_vector.hpp:37` is `reserved_vector() : mCount(0) {}` again and `preserved_t` appears
nowhere in the tree. The header is upstream's now (smaller: `typedef T* iterator`, a template
`operator=`), so re-applying the previous diff is not a rebase.

The item's actual target also stopped existing under its name: **`CGMFrontEnd` is `CFrontEndGameMode`
upstream**, so `__ct__11CGMFrontEndFRC11CGMFrontEnd` is now
`__ct__17CFrontEndGameModeFRC17CFrontEndGameMode` (`src/MetroidPrime/Player/CGameState.cpp:961`),
and it already measures **100.00%** - the plain `mPlayers(other.mPlayers)` spelling works now
because upstream's `reserved_vector` copy ctor is out of line at 0x80143CD4 and stores the count in
the *callee*. The mem-init store the previous run deleted is no longer emitted at all. So no
`STALE:` for the unit (the count is not maxed), but **do not re-land `preserved_t`; the problem it
solved is gone.**

## What this run did: the eighth sync renamed addresses that pre-sync carves still own

That sync filled in `config/G2ME01/symbols.txt` for addresses that used to be unnamed, and this
tree had claimed several of them under `extern "C"` carve names. objdiff pairs **by symbol name**,
so a byte-exact carve under the old name now pairs with nothing and scores 0. Measured on this
unit (`nm --defined-only` on both objects, `LC_ALL=C comm`):

```
OUR fn_ carves with NO retail symbol of that name:
fn_801422D4  fn_801426E0  fn_80142800  fn_80142914  fn_80142944  fn_801466F4
retail fn_ symbols absent from our object:  (none)
```

Of those six, `objdump -r` says only **`fn_80142914`** is still referenced; the other five are dead
weight that pairs with nothing and would each block a future flip (`unit_fit.sh`). Not touched
here - out of scope for this item, but it is what is left between this unit and `unit_fit.sh`.

**The one that was byte-exact: `fn_80142288`.** Retail's 0x80142288 is 76 bytes and is now
`erase__Q24rstl63vector<Q24rstl19pair<Ui,9TEditorId>,...>FQ24rstl146pointer_iterator<...>` - the
one-argument `rstl::vector::erase(iterator)`. The carve was a faithful copy of
`rstl/vector.hpp`'s one-argument `erase`, and the file already said so. Fix, one file:

- `CGameState.cpp:1543` - the call in `CPersistentOptions::SetCinematicState` is now
  `mCinematicStates.erase(it)` instead of `fn_80142288(&mCinematicStates, it)`, so the template
  instantiation is emitted **under retail's own mangled name** and objdiff pairs it;
- `CGameState.cpp:1572` - the `fn_80142288` definition is deleted.

Result: `erase(iterator)` 0.00% -> **100.00%**, and `SetCinematicState` stays at 100.00%. Our
symbol at `.text+0x65fc` is byte-identical to retail's `.text+0x100` (76 B, 19 instructions,
compared as raw `.text` bytes, not through objdiff).

**Measured, and worth knowing: objdiff does not compare a `bl`'s relocation by symbol name.** In
`objdiff-cli diff`, our `bl fn_80142288` and retail's `bl erase__...(1 arg)` carry no `arg_diff`
and `SetCinematicState` is 100.00% either way; the same is true of the `bl` in retail's
`push_back`. So renaming a *callee* a function calls costs nothing - only the callee itself needed
the right name. (Relocations do get compared on some instructions, so this is not a licence to
assume it everywhere.)

## WALL: push_back__rstl::vector<CWorldState>::push_back 0.00%

Retail's 0x801426E0 (56 B) is the **unsafe** append - no capacity test, no temporary:
`mCount` up by one, then an in-place construct at `mItems + count*36`. The same pattern applies as
for `erase`, but the name is the problem:

- `extern "C" fn_801426E0` carries **byte-identical** 56 bytes (verified) and pairs with nothing.
- The name does exist in our object: `rstl::vector<CWorldState>::push_back` is instantiated by
  `StateForWorld` and `InitializeMemoryWorlds`, but `include/rstl/vector.hpp`'s inline `push_back`
  is the growth-checking one, so it emits **144 bytes against retail's 56 and objdiff scores it
  37.25%** (`objdiff-cli diff`, `match_percent: 37.25`).
- The right name under the right body needs an explicit specialization, and **mwcceppc cannot
  express one**. Both spellings measured, both rejected at compile time with
  `object 'rstl::vector<CWorldState, rstl::rmemory_allocator>::push_back(const CWorldState &)'
  redefined`:
  1. `template<> void rstl::vector<CWorldState, rstl::rmemory_allocator>::push_back(const
     CWorldState& in) { ... }` with `push_back` defined in the class body (as it is now);
  2. the same, after moving `push_back` out of the class body in `include/rstl/vector.hpp` to an
     `inline` out-of-line template definition (the only conforming way to make a member
     specializable). `include/rstl/vector.hpp` was reverted.

  A conforming compiler accepts (2); mwcceppc does not. Do not retry the specialization.

## For the next run on this unit

**107/116**, all measured from `build/report.json` after this run:

| function | % | B | note |
|---|---|---|---|
| `LoadGameFileState__10CGameStateFPCv` | 0.00 | 488 | absent from our object; no source at all |
| `push_back__...vector<CWorldState>...` | 0.00 | 56 | the WALL above |
| `StartGameFromFrontEnd__Fv` | 60.11 | 784 | still the guessed-name stub-ish switch |
| `reserve__...vector<CWorldState>...::reserve(int)` | 71.37 | 172 | pairs with the real template; `fn_801466F4` is a dead duplicate |
| `__ct__10CGameStateFR16CBitStreamReader` | 84.14 | 1668 | the two unexpressible blocks stand |
| `PutTo__18CPersistentOptionsCFR16CBitStreamWriter` | 94.35 | 600 | |
| `__ct__18CPersistentOptionsFR16CBitStreamReader` | 95.52 | 776 | |
| `PutTo__10CGameStateFR16CBitStreamWriter` | 96.47 | 876 | |
| `__ct__11CWorldStateFR16CBitStreamReaderUiRC18CWorldSaveGameInfo` | 97.17 | 568 | closest; see below |

`__ct__CWorldState(Reader, uint, const CWorldSaveGameInfo&)` is the closest to 100% and its diff is
small and readable (`objdiff-cli diff`, `match_percent 98.05`): `li r7,0x0` where retail has
`li r7,-0x1` with the two following `stw`s swapped (`+0x04` wants `kInvalidAreaId`, we store 0), plus
three `bl`s whose *names* differ (retail `fn_800B8CF4` / `fn_80009008` / `fn_80009224`, we call
`__ct__13CRelayTrackerFR16CBitStreamReaderRC18CWorldSaveGameInfo` and
`ReleaseData__Q24rstl23rc_ptr<13CRelayTracker>Fv` etc.). Fixing the constant alone will not reach
100% - the three call names would have to match too - so budget for both.

## Verification

```
$ ./tools/goal_check.sh build/goal/item.json
goal_check: item progress-cgamestate-reserved-vector-default-ctor (progress) target=MetroidPrime/Player/CGameState
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13045 -> 13046   linked 6148 -> 6148
  ok    check_symbol_names.py
  ok    All:  36.98% fuzzy, 30.42% matched, 13.41% linked (13046 / 28465 functions)
  ok    target rose: main/MetroidPrime/Player/CGameState: 106 -> 107 / 116 functions
  ok    no asm added
goal_check: PASS progress-cgamestate-reserved-vector-default-ctor
```

One file changed, `src/MetroidPrime/Player/CGameState.cpp` (+27/-11, of which 20 lines are the two
rewritten comment blocks). `include/rstl/vector.hpp` is untouched in the final state.
