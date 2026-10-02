# progress-twin-rel-destructiblebarrier

`module:DestructibleBarrier`: **9 -> 11 matched functions**, from a new `Matching` unit
`DestructibleBarrier/MetroidPrime/ScriptObjects/CDestructibleBarrier48AC` that owns the two
functions `fn_13_4910` (`.text` `0x4910..0x49BC`, 0xAC = 172 bytes) and `fn_13_48AC`
(`.text` `0x48AC..0x4910`, 0x64 = 100 bytes) - one contiguous claim `0x48AC..0x49BC`
(0x110 = 272 bytes), carved out of the middle of the module's unclaimed run. No `asm`, no
header change, no commit.

```
goal_check: item progress-twin-rel-destructiblebarrier (progress) target=module:DestructibleBarrier
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13308 -> 13310   linked 6356 -> 6358
  ok    check_symbol_names.py
  ok    All:  37.35% fuzzy, 30.79% matched, 13.65% linked (13310 / 28465 functions)
  ok    target rose: module:DestructibleBarrier: 9 -> 11 / 117 functions
  ok    no asm added
goal_check: PASS progress-twin-rel-destructiblebarrier
```

Measured after the run:

```
cmp build/G2ME01/DestructibleBarrier/DestructibleBarrier.rel orig/G2ME01/files/RelProd/ -> identical
sha1sum build/G2ME01/DestructibleBarrier/DestructibleBarrier.rel -> e1744b2c85f29f2821175c332d2399ac8ebf1fac
                              (== config/G2ME01/config.yml, module ID 13)
sha1sum build/G2ME01/main.dol                   -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
python3 tools/check_symbol_names.py  -> checked 585 units; 0 declared names are missing from their object
python3 tools/check_files_cmake.py   -> every configured DOL object is either in files.cmake or excluded
total_functions still 28465
./tools/unit_fit.sh MetroidPrime/ScriptObjects/CDestructibleBarrier48AC.cpp
   .text claimed 272  ours 272  retail 272  fits
   no extra functions: our object defines only what the retail unit object does
build/report.json: DestructibleBarrier/MetroidPrime/ScriptObjects/CDestructibleBarrier48AC
                   .text 272 at 0x48AC, 100.0%; the old auto run split into auto_00_000000A0_text
                   (0xA0..0x48AC) and auto_00_000049BC_text (0x49BC..0x7358), both still unmatched.
```

## Per function

| function | before | after | twin's source matched unchanged? |
| --- | --- | --- | --- |
| `fn_13_48AC` | 0% (unmatched, `auto_00_000000A0_text`) | 100.0% | no - same shape, this copy's own class (`COBBTree`, not `CScannableObjectInfo`) |
| `fn_13_4910` | 0% (unmatched, `auto_00_000000A0_text`) | 100.0% | no - same shape, this copy's own layout and callees |

Both are byte-exact against the unit's retail-derived target object: both objects are `.text`
0x110 and **0 of 272 bytes differ** (byte-compare over the two `objdump -d .text` dumps, all four
`R_PPC_REL24` words included), and the ten relocations are the same ten symbols in both -
`__dt__8COBBTreeFv`, `Free__7CMemoryFPCv` and, for `fn_13_4910`, `fn_13_4B0C`, `fn_13_4AB8` x2,
`fn_13_4A64`, `fn_13_4A10` x3, `fn_13_49BC`, `Free__7CMemoryFPCv`.

## Intended changes (one of them is a `config/` change)

1. `src/MetroidPrime/ScriptObjects/CDestructibleBarrier48AC.cpp` (new) - the unit.
2. `config/G2ME01/rels/DestructibleBarrier/splits.txt` - `MetroidPrime/ScriptObjects/
   CDestructibleBarrier48AC.cpp: .text start:0x48AC end:0x49BC`. One contiguous range, no gap
   spanned, the module's own name; the head's and `REL_Setup`'s entries are untouched.
3. `configure.py` - `Object(Matching, "MetroidPrime/ScriptObjects/CDestructibleBarrier48AC.cpp")`
   added to the `Rel("DestructibleBarrier", ...)` block (line ~2132).
4. `files.cmake` - the source listed under the ScriptObjects REL entries, host branch empty.
5. `config/G2ME01/config.yml`, module ID 13 - **`force_active: [fn_13_48AC]`**. Required, not
   cosmetic; see the dead-strip measurement below. No `symbols.txt` rename (the range's entry is
   already the placeholder `fn_13_48AC`).

## The declaration, for the other copies of each shape

`fn_13_48AC` is the twin the item leads with - `tools/twin_scan.py` pairs it with
`__dt__Q24rstl32auto_ptr<20CScannableObjectInfo>Fv` (`src/MetroidPrime/Factories/
CScannableObjectInfo.cpp`, 100 B, matched). It is the same template with **this module's**
argument: its own `.rela.text` names `__dt__8COBBTreeFv`, so its T is `COBBTree`. The identical
declaration is already built in another module - `src/MetroidPrime/ScriptObjects/CAtomicAlpha7E0.cpp`
(module 2, T = `CAnimData`), landed by `progress-example-dtq24rstl32autoptr20cscannab` - and it
reproduced these bytes unchanged, which is the strongest evidence this run has:

```cpp
class COBBTree;
extern "C" {
void __dt__8COBBTreeFv(COBBTree* self, int flag);   // the delete mItem half
void Free__7CMemoryFPCv(const void* ptr);          // the delete this half (CMemory::Free)
#ifdef __MWERKS__
struct CDestructibleBarrierCOBBTreePtr {            // the two words the bytes show
  bool mHas;        // +0: lbz/cmplwi/beq guards the delete
  COBBTree* mItem;  // +4: lwz/li 1/bl __dt__8COBBTreeFv
};
void* fn_13_48AC(CDestructibleBarrierCOBBTreePtr* self, short flag) {
  if (self != 0) {
    if (self->mHas) { __dt__8COBBTreeFv(self->mItem, 1); }
    if (flag > 0)  { Free__7CMemoryFPCv(self); }
  }
  return self;
}
#endif
}
```

- **The flag parameter has to be a `short`.** Written `int` the test compiles to `cmpwi r31,0`
  where retail has `extsh. r0,r31`; that one instruction is the whole difference.
- **The definition keeps the C name** the module's `symbols.txt` carries (`fn_13_48AC`), because
  an out-of-line member destructor emits the mangled `__dt__<class>Fv` and this range's entry is
  the dtk placeholder - the rule `src/MetroidPrime/Cameras/Carve801E7C14.c` states.
- `COBBTree` stays forward-declared: including `WorldFormat/COBBTree.hpp` would add bytes the
  claim does not cover (the AtomicAlpha note measures that on the same body).

`fn_13_4910` is the run's other function, the twin of `__dt__Q28COBBTree10SIndexDataFv`
(`src/WorldFormat/COBBTree.cpp`). Same 0x10 frame, same `mr r31,r4` before `mr. r30,r3`, same
`extsh. r0,r31`; what it adds is MWCC's member-destruction walk, highest member first. The bytes
give the layout, so the body is written against a declared layout rather than the real member
classes (`include/WorldFormat/COBBTree.hpp:116` sizes `COBBTree` at 0x80, which agrees):

```cpp
struct CDBBTreeMember { unsigned char mBytes[0x10]; };
struct CDestructibleBarrierCOBBTree {   // +0x00, +0x10, ... +0x70, in walk order reversed
  CDBBTreeMember mAt00, mAt10, mAt20, mAt30, mAt40, mAt50, mAt60, mAt70;
};
void* fn_13_4910(CDestructibleBarrierCOBBTree* self, short flag) {
  if (self != 0) {
    fn_13_4B0C(&self->mAt70, -1);   // the module's own five member destructors, by symbols.txt name
    fn_13_4AB8(&self->mAt60, -1);   //   (declared, never defined here: their ranges stay retail)
    fn_13_4AB8(&self->mAt50, -1);
    fn_13_4A64(&self->mAt40, -1);
    fn_13_4A10(&self->mAt30, -1);
    fn_13_4A10(&self->mAt20, -1);
    fn_13_4A10(&self->mAt10, -1);
    fn_13_49BC(&self->mAt00, -1);
    if (flag > 0) { Free__7CMemoryFPCv(self); }
  }
  return self;
}
```

**Each member destructor must be named as the module names it** (`fn_13_4B0C` and friends), not
mangled: naming it `__dt__8COBBTreeFv` or a template name would leave a relocation to a symbol the
module does not define and mwldeppc would report it `undefined:`. Definitions are in descending
retail text order (`nm -n` on our object: `fn_13_48AC` at 0, `fn_13_4910` at 0x64, matching retail
0x48AC < 0x4910). `tools/check_decl_order.py --unit` reports "0 unit(s) checked" for this name -
it reads function names out of `build/report.json`, which does not carry REL-unit functions the
way it carries DOL ones - so the order is verified by the 0-of-272 byte compare above, which is
strictly stronger.

## The dead-strip: the claim alone breaks the module

First build after wiring, before `force_active`: `dtk shasum -c config/G2ME01/build.sha1` printed
`build/G2ME01/DestructibleBarrier/DestructibleBarrier.rel: FAILED`, `86 files OK`. Cause, measured
on `DestructibleBarrier.plf`: `nm` shows `000048ac T fn_13_4910`, i.e. the object was linked but
`fn_13_48AC` was **dead-stripped** - nothing in the module references it and it is not in dtk's
generated `FORCEACTIVE` list, so `.text` came out 0x64 short and every function above 0x48AC moved
down. `fn_13_4910` needed nothing: it is called by `bl fn_13_4910` at 0x488C, inside the module's
own unclaimed bytes, which stays in the link. The fix is the sanctioned one: a `force_active:`
list on the module in `config/G2ME01/config.yml` (dtk 1.8.4 reads it into the link script). With
it the module is `cmp`-identical to retail and hashes to `config.yml`.

## Host branch

Empty by design (everything is inside `#ifdef __MWERKS__`), the arrangement
`CAtomicAlpha7E0.cpp` and `CLumiteRelTail.cpp` use: `check_files_cmake.py` requires a `Matching`
object to be in `files.cmake`, and an empty host branch keeps the port's undefined count where it
is (the port probe is inside `gate.sh`, which passed).

## Side findings (not fixed here)

- The item's reason says the `__dt__Q24rstl52vector<15CMayaSplineKnot,...>` twin was "already
  landed in a module as ... in `src/MetroidPrime/Tweaks/Tweaks.cpp`". It is not in that file any
  more - `twin_scan.py --list` now attributes it to `src/MetroidPrime/ScriptObjects/
  CScriptEffect.cpp` (main, not a module). The item's annotation is stale, not wrong about the
  shape.
- `fn_13_4B60` (0x4B60, 0x68) is adjacent to the run this item listed but is a **different**
  shape, so it was left out of the claim; `fn_13_4910`'s neighbour below it, `fn_13_49BC`, is the
  `rstl::vector<T>` deleting destructor.

NEW: progress-rel-destructiblebarrier-vectors | progress | module:DestructibleBarrier | the run `.text 0x49BC..0x4B0C` (fn_13_49BC, 4A10, 4A64, 4AB8, 4B0C - five adjacent `rstl::vector<int>` deleting destructors, `lwz r3,0xc(r30)/bl Free__7CMemoryFPCv`, `extsh. r0,r31/ble`, `Free__7CMemoryFPCv(self)`) is one contiguous claimable range whose twin is already matched in `src/MetroidPrime/ScriptObjects/CScriptEffect.cpp`; fn_13_4910 above shows the shape needs the C-name/short-flag declaration, and a copy nothing references would need `force_active`.
