# progress-example-dtq24rstl32autoptr20cscannab

`module:AtomicAlpha`: **23 -> 24 matched functions**, from a new `Matching` unit
`AtomicAlpha/MetroidPrime/ScriptObjects/CAtomicAlpha7E0` that owns exactly one function,
`fn_2_7E0` (`.text` `0x7E0..0x844`, 0x64 = 100 bytes), carved out of the middle of the module's
unclaimed `auto_00_0000013C_text` run. No `asm`, no header change, no commit.

```
goal_check: item progress-example-dtq24rstl32autoptr20cscannab (progress) target=module:AtomicAlpha
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13165 -> 13166   linked 6245 -> 6246
  ok    check_symbol_names.py
  ok    All:  37.15% fuzzy, 30.58% matched, 13.51% linked (13166 / 28465 functions)
  ok    target rose: module:AtomicAlpha: 23 -> 24 / 71 functions
  ok    no asm added
goal_check: PASS progress-example-dtq24rstl32autoptr20cscannab
```

Measured after the run:

```
cmp build/G2ME01/AtomicAlpha/AtomicAlpha.rel orig/G2ME01/files/RelProd/AtomicAlpha.rel -> identical
sha1sum build/G2ME01/AtomicAlpha/AtomicAlpha.rel -> ade8972eb74648c3c5c99caa022ff457ff2b27fd
                              (== config/G2ME01/config.yml, module ID 2)
sha1sum build/G2ME01/main.dol                   -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/probe_sources.sh  -> probe: 849 files, 0 failed, 0 errors; link: LINKED (286 undefined, 0 duplicates)
python3 tools/check_symbol_names.py  -> checked 585 units; 0 declared names are missing from their object
python3 tools/check_files_cmake.py   -> every configured DOL object is either in files.cmake or excluded
python3 tools/check_decl_order.py    -> ok: 1098 unit(s) checked, 37 permuted, all accounted for
./tools/unit_fit.sh MetroidPrime/ScriptObjects/CAtomicAlpha7E0.cpp
   .text claimed 100  ours 100  retail 100  fits
   no extra functions: our object defines only what the retail unit object does
build/report.json: AtomicAlpha/MetroidPrime/ScriptObjects/CAtomicAlpha7E0 1/1 functions, fn_2_7E0 100.0%
                   total_functions still 28465; the auto run split into auto_00_0000013C_text (7 fns)
                   and auto_00_00000844_text (39), all still unmatched, none worse.
```

## Intended changes (one of them is a `config/` change)

1. `src/MetroidPrime/ScriptObjects/CAtomicAlpha7E0.cpp` (new) - the unit.
2. `config/G2ME01/rels/AtomicAlpha/splits.txt` - `MetroidPrime/ScriptObjects/CAtomicAlpha7E0.cpp:
   .text start:0x7E0 end:0x844`. One contiguous range, no gap spanned.
3. `configure.py` - `Object(Matching, "MetroidPrime/ScriptObjects/CAtomicAlpha7E0.cpp")` added to
   the `Rel("AtomicAlpha", ...)` block (line ~1631); the head unit's entry is unchanged.
4. `files.cmake` - the source listed under the ScriptObjects REL entries, host branch empty.
5. `config/G2ME01/config.yml`, module ID 2 - **`force_active: [fn_2_7E0]`**. This is required, not
   cosmetic: see the dead-strip measurement below. `splits.txt`/`symbols.txt` untouched otherwise
   (no rename).

## The declaration that produced the shape - copy this for the other 62 copies

```cpp
class CAnimData;

extern "C" {
void __dt__9CAnimDataFv(CAnimData* self, int flag);   // the DOL's CAnimData deleting dtor
void Free__7CMemoryFPCv(const void* ptr);            // CMemory::Free, the dtor's delete half

#ifdef __MWERKS__
struct CAtomicAlphaAnimDataPtr {                     // the two words the bytes show
  bool mHas;        // +0: `lbz r0,0(r30) / cmplwi r0,0 / beq` guards the delete
  CAnimData* mItem; // +4: `lwz r3,4(r30) / li r4,1 / bl __dt__9CAnimDataFv`
};

void* fn_2_7E0(CAtomicAlphaAnimDataPtr* self, short flag) {
  if (self != 0) {
    if (self->mHas) { __dt__9CAnimDataFv(self->mItem, 1); }
    if (flag > 0)  { Free__7CMemoryFPCv(self); }
  }
  return self;
}
#endif
}
```

Three measured points about it:

- **The object is byte-exact.** `build/G2ME01/src/MetroidPrime/ScriptObjects/CAtomicAlpha7E0.o`
  and the unit's retail-derived target object
  `build/G2ME01/AtomicAlpha/obj/MetroidPrime/ScriptObjects/CAtomicAlpha7E0.o` are both `.text`
  0x64, and identical over the whole range once the two `R_PPC_REL24` words at +0x34 and +0x44 are
  masked - and those two words name the *same* two symbols (`__dt__9CAnimDataFv`,
  `Free__7CMemoryFPCv`) in both objects.
- **The flag parameter has to be a `short`.** Written `int`, the flag test is `cmpwi r31,0` where
  retail has `extsh. r0,r31` - that one instruction is the whole difference, and everything else
  (self in r30, flag in r31, `mr r31,r4` *before* the `mr. r30,r3`, both exits) is identical.
  This is what the item's "a member function only matches written as a member" is about; it is
  the flag's declared width, not member-ness.
- **The same body as an out-of-line member destructor is byte-identical too**, but it emits the
  *mangled* `__dt__<whatever the class is called>Fv`, and this range's `symbols.txt` entry is the
  dtk placeholder `fn_2_7E0`, so objdiff would pair nothing - the rule
  `src/MetroidPrime/Cameras/Carve801E7C14.c` states for the same situation. Hence the C name.
- **The template's own name is not reachable.** `rstl::auto_ptr<CAnimData>`'s deleting destructor
  is what this is, but MWCC emits it out-of-line only from a `delete` site it does not inline:
  `template class rstl::auto_ptr<CAnimData>;` and `template <> rstl::auto_ptr<CAnimData>::~auto_ptr()
  { ... }` both emit **no** `.text` (measured), and `void use(rstl::auto_ptr<CAnimData>* p) {
  delete p; }` inlines the body and leaves only the two callees undefined. Provoking it would add
  a function the claim does not cover, so the class is declared as the two words the bytes show.
- Do **not** include `MetroidPrime/CAnimData.hpp` to spell `delete self->mItem`: that adds 0x24
  bytes of `.data` and 5 of `.bss` to the object (measured) that the claim does not cover.

## What the copy is (why this is the faithful declaration)

`tools/twin_scan.py --list` pairs it with `__dt__Q24rstl32auto_ptr<20CScannableObjectInfo>Fv`
(`src/MetroidPrime/Factories/CScannableObjectInfo.cpp`, 100 bytes, matched). The shape is a
two-word `rstl::auto_ptr<T>` deleting destructor, so the T is what differs. Before this claim
there were **63 copies of it in REL modules**; 59 of those called `__dt__9CAnimDataFv`, 2
`__dt__12CActorLightsFv`, 1 `__dt__8COBBTreeFv`, 1 an unnamed module function (measured by reading
each copy's own `.rela.text` in its object file). This copy calls `__dt__9CAnimDataFv`, so its T
is `CAnimData` - `rstl::auto_ptr<CAnimData>`, a template instantiation, not the twin's
`auto_ptr<CScannableObjectInfo>`. After the claim, **62 unmatched copies remain, in 59 modules**
(`AtomicBeta, BacteriaSwarm, Blogg, ChozoGhost, CommandoPirate, DarkCommando, DarkSamus,
DarkTrooper, DestructibleBarrier, DigitalGuardian, ElitePirate, EmperorIngStage1,
EmperorIngStage2Tentacle, EmperorIngStage3, EyeBall, FlyingPirate, Glowbug, Grenchler, GunTurret,
Ing, IngBlobSwarm, IngBoostBallGuardian, IngSpaceJumpGuardian, IngSpiderballGuardian, Kralee,
Krocuss, Lumite, MediumIng, Metaree, MetareeSwarm, Metroid, MinorIng, MysteryFlyer,
OctapedeSegment, Parasite, PillBug, PlantScarabSwarm, PuddleSpore, Puffer, Rezbit, Ripper,
SandBoss, Sandworm, ScriptPlayerActor, ScriptPlayerProxy, ScriptRiftPortal, Shredder, Shrieker,
SnakeWeedSwarm, SpacePirate, SpankWeed, Splinter, StoneToad, SwampBossStage1, SwampBossStage2,
SwarmBasics, Tryclops, WallWalker, WispTentacle`), each the same one-function claim with that
module's own `fn_<id>_<addr>` name. The 2 `CActorLights` and 1 `COBBTree` copies take the same
declaration with a different callee.

## The dead-strip: the claim alone breaks the module

First build after wiring (no `force_active`): `dtk shasum -c config/G2ME01/build.sha1` printed
`build/G2ME01/AtomicAlpha/AtomicAlpha.rel: FAILED`, `86 files OK`. Cause, measured on
`build/G2ME01/AtomicAlpha/AtomicAlpha.plf`: `nm` shows `000007e0 T fn_2_844`, i.e. the object was
linked but `fn_2_7E0` was **dead-stripped** - nothing in the module references it and it is not in
dtk's generated `FORCEACTIVE` list, so `.text` came out `0x27BC` (0x64 short) and every function
above 0x7E0 moved down by 0x64. `fn_2_7E0` is also not in the auto run's `FORCEACTIVE` entries,
which is why the retail-derived object never had to face this. The fix is the one
`docs/RUNNING_THE_DECOMP.md`'s ScriptCoin note records as sanctioned since 2026-09-29: a
`force_active:` list on the module in `config/G2ME01/config.yml` (dtk 1.8.4 reads it into the link
script). With it, the module is `cmp`-identical to retail and hashes to `config.yml`.

## Host branch

Empty by design (everything is inside `#ifdef __MWERKS__`), the arrangement `CLumiteRelTail.cpp`
and `CIngBoostBallGuardianBits.cpp` use: `check_files_cmake.py` requires a `Matching` object to be
in `files.cmake`, and an empty host branch keeps the port's undefined count where it is. The port
probe (`849 files, 0 failed; link: LINKED (286 undefined, 0 duplicates)`) covers it.

No `NEW:` items: the 62 remaining copies are already the queue's twin items for their modules, and
this file is the recipe they were waiting for. No `WALL:` - nothing was left sub-100%.
