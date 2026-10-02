# progress-twin-rel-bacteriaswarm - `module:BacteriaSwarm`, 9 -> 19 matched functions

## What landed

Three new units in BacteriaSwarm (module 6), all `Matching`, all byte-exact, ten functions. All
three carve the module's out-of-line template tail, i.e. the shapes retail's own translation units
emitted for the machinery around its element arrays:

| unit | claim | functions |
| --- | --- | --- |
| `MetroidPrime/ScriptObjects/CBacteriaSwarmRelTail.cpp` | `.text 0x4150..0x42E8` (0x198) | `fn_6_4290`, `fn_6_4244`, `fn_6_421C`, `fn_6_41FC`, `fn_6_4194`, `fn_6_4150` |
| `MetroidPrime/ScriptObjects/CBacteriaSwarmRelTail2.cpp` | `.text 0x4020..0x4068` (0x48) | `fn_6_4040`, `fn_6_4020` |
| `MetroidPrime/ScriptObjects/CBacteriaSwarmRelTail3.cpp` | `.text 0x5C00..0x5C48` (0x48) | `fn_6_5C20`, `fn_6_5C00` |

Per function, from `build/report.json`:

| function | size | before | after |
| --- | --- | --- | --- |
| `fn_6_4020` | 0x20 | unmatched (`auto_00_000000A0_text`) | 100.00% |
| `fn_6_4040` | 0x28 | unmatched | 100.00% |
| `fn_6_4150` | 0x44 | unmatched | 100.00% (76.47% under GC/1.3.2) |
| `fn_6_4194` | 0x68 | unmatched | 100.00% |
| `fn_6_41FC` | 0x20 | unmatched | 100.00% |
| `fn_6_421C` | 0x28 | unmatched | 100.00% |
| `fn_6_4244` | 0x4C | unmatched | 100.00% (84.16% under GC/1.3.2) |
| `fn_6_4290` | 0x58 | unmatched | 100.00% |
| `fn_6_5C00` | 0x20 | unmatched | 100.00% |
| `fn_6_5C20` | 0x28 | unmatched | 100.00% |

## The spellings, per function, and whether the twin's source matched unchanged

Every twin's source matched **unchanged** - no function needed a new body, so none went backwards.
The one thing that had to change is the compiler version of `CBacteriaSwarmRelTail.cpp` (below).

- **0x20, the forwarder** (`fn_6_4020`, `fn_6_41FC`, `fn_6_5C00`): a frame and one unconditional
  `bl`, no load and no test. Twin `__sys_free` (`src/MetroidPrime/main.cpp:1167`'s neighbourhood)
  and `fn_80004C4C` in `src/MetroidPrime/Player/Carve80004C4C.c`, the same eight instructions
  apart from the `bl`. Body: `void f(void* d, const void* s) { callee(d, s); }`.
- **0x28, `construct_impl`** (`fn_6_4040`, `fn_6_421C`, `fn_6_5C20`): `cmplwi r3,0x0 / stw r0,0x14(r1)
  / beq / bl`, i.e. the null test is on the **destination**. Twin
  `construct_impl<CPASAnimState>`, i.e. `rstl::construct_impl< CPASAnimState >(dest, src)` folded
  into `fn_8002E4B8` at `src/MetroidPrime/CAnimData.cpp:145`; `fn_80248DDC` in
  `src/WorldFormat/CMetroidAreaCollider.cpp` is the same nine instructions written as
  `if (dest) { ... }`, which is the spelling used here.
- **0x68, `uninitialized_copy_n`** (`fn_6_4194`): `(first, n, result)` in r3/r4/r5, cursors in
  r31/r30/r29, the loop entered at its **bottom** test, both cursors advanced after the construct,
  and the **end cursor returned**; a zero count copies nothing and still returns `result`. Twin
  `fn_80248EA4` (`src/WorldFormat/CMetroidAreaCollider.cpp:915`) and `fn_80004CD4`
  (`src/MetroidPrime/Player/Carve80004C4C.c`) - the same 26 instructions, and the same 0x24 stride,
  so the twin's body is this body. The callee is passed `*reinterpret_cast<const SFn6_4244*>(it)`,
  a reference, because retail's `mr r3,r30 / mr r4,r31` is the *address* of the element.
- **0x44, `reserved_vector`'s copy constructor** (`fn_6_4150`): the "store the count, then read it
  back" shape. **The bound is `self->mCount`, not `other->mCount`** - retail loads the source's word
  once, stores it, and then *re-loads it from the destination* (`lwz r4,0(r31)`) to pass as the
  count, and that reload is what puts `self` in r31 and the two cursors in r3/r5. Twins
  `fn_80248E60` (`src/WorldFormat/CMetroidAreaCollider.cpp:950`) and `fn_80004C90`
  (`src/MetroidPrime/Player/Carve80004C4C.c`); copied unchanged, stride aside.
- **0x4C, the 0x24-byte element's copy constructor** (`fn_6_4244`): six floats at +0x0..+0x14 and
  three words at +0x18, +0x1C, +0x20 - `CAreaOctTree::Node`, `NESTED_CHECK_SIZEOF(CAreaOctTree,
  Node, 0x24)`. Its twin is that class's **implicit** copy constructor, emitted weakly as
  `__ct__Q212CAreaOctTree4NodeFRCQ212CAreaOctTree4Node` in `src/WorldFormat/CAreaOctTree_Tests.cpp`
  (there is no hand-written source for it), so the statement order comes from `fn_55_106E0` in
  `src/MetroidPrime/ScriptObjects/CSandBossRelTail.cpp`, the same member-for-member copy over
  another 0x2C record and at 100.00% already. Named by offset rather than by retail's member names
  because spelling `CAABox mAabb; const void* mPtr; const CAreaOctTree& mOwner; ETreeType
  mNodeType;` out would drag a `CAreaOctTree` in for a function that only copies them.
- **0x58, the owner's deleting destructor** (`fn_6_4290`): receiver guard, the member at +0x18 torn
  down with the literal -1, the sign-extended flag test, `CMemory::Free(self)`, and the receiver
  returned. Twin `fn_800CD460` (`src/MetroidPrime/Player/CMorphBall.cpp:501`), word for word with
  `fn_6_3EA0` there for `fn_800CD4B8`. Copied unchanged - **the flag is a `short`** (retail's test
  is `extsh.`) and the flag test sits **inside** `if (self)`, or the `beq` lands on the `extsh.`
  instead of on the epilogue.

## `mw_version="GC/2.7"` on `CBacteriaSwarmRelTail.cpp` is load-bearing and measured

Under the module's default `GC/1.3.2` the unit is **64.71%, 4/6**. The four that match are
`fn_6_4194`, `fn_6_41FC`, `fn_6_421C` and `fn_6_4290`; two do not, and both are pure scheduling:

| function | under GC/1.3.2 | what differs |
| --- | --- | --- |
| `fn_6_4150` | 76.47% | retail hoists `lwz r0,0(r4)` **in front of** the `stw r31,0xc(r1)` prologue save; 1.3.2 keeps it next to its `stw r0,0(r31)` |
| `fn_6_4244` | 84.16% | retail uses the two-deep `lfs f1`/`lfs f0` load/store pipeline; 1.3.2 copies each float straight through `f0` (`lfs f0,0(r4); stfs f0,0(r3); lfs f0,4(r4); ...`) and reuses `r0` for all three words |

`GC/2.7` emits retail's order and all six go to 100.00%. This is the same **per-object**
`mw_version=` arrangement `CSandBossRelTail.cpp` uses (`configure.py:1960`), not a change to the
module's `Rel(...)` block, so `CBacteriaSwarmRel.cpp` and `REL/REL_Setup.cpp` are untouched. The two
short-shape units (`Tail2`, `Tail3`) are at 100.00% under the module default and keep it.

## Why the claim starts at 0x4150 and not at 0x40C4

The seeder's longest run is `[.text 0x40C4..0x42E8, 7 adjacent]`, and its first function
`fn_6_40C4` (0x8C = 140 B) is a `~reserved_vector()` instantiation - byte-identical to
`__dt__Q24rstl50reserved_vector<Q211CSfxManager14SLowPassFilter,8>Fv`
(`src/Kyoto/Audio/CSfxManager.cpp`, a matched twin). It is a **measured wall**, and not one this run
spent time on: `docs/goal-notes/progress-twin-rel-sandworm.md` already established that this
compiler reproduces the shape instruction for instruction (same opcodes, same operands, same branch
structure, 35 instructions, MW's counted main loop and its self-`bdnz` fill) and differs **only** in
register assignment - induction variable `r5` and peeled trip count `r3` against retail's `r3`/`r5`,
plus `addi r3,r6,-8` against `subi r5,r6,8` - and that MW's allocator does not move between the eight
`GC/*` compilers or across ten source spellings. The twin scan's identical spelling would have to be
a *template instantiation*, which cannot be used here anyway: this module's `symbols.txt` carries
`fn_<addr>` placeholders, so a mangled `_Z...fn_6_40C4...` would put a name where retail has none.

Claiming a range the object does not reproduce takes those bytes out of the module and breaks its
sha1, so `fn_6_40C4` and `fn_6_3570`'s 0x8AC body stay retail's. Same reason `fn_6_4068` (0x5C) and
`fn_6_5C48` (0x90) stop `Tail2` and `Tail3`: neither is a twin (one copies a `reserved_vector`
member by calling `fn_6_4150`, the other builds a `CTransform4f` by calling
`__ct__12CTransform4fFRC12CTransform4f`), so each claim stops one function short of it and the
element constructor is declared and never defined here.

## The carve, in four files (as required)

- `config/G2ME01/rels/BacteriaSwarm/splits.txt` - the three `.text` claims, in address order.
- `configure.py` - three `Object(Matching, ...)` lines added to the existing
  `Rel("BacteriaSwarm", ...)` block (not a second `Rel` for the same module, which dtk would read
  as a duplicate module definition), and `mw_version="GC/2.7"` on `CBacteriaSwarmRelTail.cpp` only.
- `files.cmake` - all three files, each with the "host branch is empty by design" note the
  neighbouring REL tails carry. The bodies are inside `#ifdef __MWERKS__`, so listing them adds no
  undefined reference on the host.
- `config/G2ME01/config.yml` - one `force_active:` list on module 6, ten names
  (`fn_6_4020`, `fn_6_4040`, `fn_6_4150`, `fn_6_4194`, `fn_6_41FC`, `fn_6_421C`, `fn_6_4244`,
  `fn_6_4290`, `fn_6_5C00`, `fn_6_5C20`), nothing else. **Config changes, as a list:** one new
  `force_active:` block on module 6 with those ten names; no hash, split or symbol was touched.
  `fn_6_4194`, `fn_6_41FC`, `fn_6_421C` and `fn_6_4244` are reachable only from inside
  `CBacteriaSwarmRelTail.o`, so without the list mwldeppc dead-strips them and the module links
  short - the arrangement, and the reason, `CSandBossRelTail.cpp`'s two carry.

Two consequences of the claim worth recording:

- **Names must be reproduced verbatim, so every definition has to stay C.** Retail names none of
  these, and the linked module's symbol table is part of the file the sha1 covers.
- **`fn_6_4150` and `fn_6_4290` are called from the module's own retail bytes** (0x409C calls
  `fn_6_4150`; 0x38E0, 0x39A8, 0x3AE8, 0x51B4 and 0x51C4 call `fn_6_4290`; 0x3FE8 calls
  `fn_6_4020`; 0x5BEC and 0x7210 call `fn_6_5C00`), so those references only resolve because the
  definitions carry dtk's own names. `fn_6_4040` and `fn_6_5C20` are called only from inside their own
  objects. `fn_6_4290` calls `fn_6_3EA0` and `Free__7CMemoryFPCv`, both of which stay outside this
  change.

## Measured

    BacteriaSwarm matched_functions (summed over BacteriaSwarm/* units)   9 -> 19   (of 114)
    global matched_functions                                   13325 -> 13335
    global linked (complete units)                              6373 -> 6383
    all three new units                                             100.00% matched, 2/2, 6/6 and 2/2
    tools/audit_rel_claim.py BacteriaSwarm         0 claim(s) with a problem; every range 2/2, 6/6, 2/2,
                                                            4/4 and 5/5 - no filled gap, no invented symbol
    all 86 RELs vs config.yml                                  none differ
    main.dol sha1                            6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
    probe_sources.sh                                          861 files, 0 failures, link LINKED (286 undefined)
    check_decl_order.py / check_files_cmake.py / check_symbol_names.py / check_module_wiring.py   ok
    ./tools/goal_check.sh build/goal/item.json               goal_check: PASS progress-twin-rel-bacteriaswarm

`tools/unit_fit.sh` is **not** the test for a REL unit - it reports
"src/.../CBacteriaSwarmRelTail*.cpp: not declared in any splits.txt", because it reads the DOL's
`config/G2ME01/splits.txt` and a module's ranges live in
`config/G2ME01/rels/<Module>/splits.txt`. `tools/audit_rel_claim.py <Module>` is the REL equivalent
and it is clean, which is the check that matters for "the unit claims only what its own object
reproduces". For the same reason `tools/flip_test.sh` proves nothing here (it looks the source up
under `extern/musyx/src/` and prints FAIL); the module's sha1 against
`config/G2ME01/config.yml` is the result, `RUNNING_THE_DECOMP.md`, "The check that actually means
something".

## Negative results, for the next run

- **`fn_6_40C4` (0x40C4, 0x8C) is not writable here**, and the reason is structural rather than a
  spelling problem: the only spelling that produces the shape is the `rstl::reserved_vector<...>`
  destructor instantiation, which mangles to a name the module's `symbols.txt` does not have.
  `docs/goal-notes/progress-twin-rel-sandworm.md` holds the ten spellings and eight compiler
  versions measured against that shape; all of them give the induction variable `r5` and the peeled
  trip count `r3` where retail has `r3`/`r5`. It is **not** a `WALL:` line - this run measured no
  spelling of it, and the point of a `WALL:` is that the *current* run tried several.
- **`fn_6_6A8C` (0x6A8C, 0x6C) is an `optional_object` destructor whose element this tree does not
  declare.** It reads the engaged flag at **+0xC**, which means `sizeof(element) == 0xC`, and calls
  the mangled `__dt__6CTokenFv` with `r3` still holding the receiver (i.e.
  `rstl::optional_object<T>::~optional_object()`'s `rstl::destroy(get_ptr())` with the data at
  offset 0). `include/Kyoto/CToken.hpp` declares `~CToken()` without defining it - good, that part
  works - but its `CToken` is **8 bytes** (`CObjectReference* mObjRef; bool mLockHeld;`), so a real
  `optional_object<CToken>` puts the flag at +0x8, not +0xC. Getting the bytes needs a 12-byte class
  named `CToken`, which would mean a local declaration that contradicts a real header - not worth
  it for one function.
- **`fn_6_6D30` (0x6D30, 0x64) is writable in principle but the mangled callee is the obstacle.**
  It is `rstl::auto_ptr<T>`'s teardown plus a deleting-destructor tail: `lbz r0,0(self)` (the
  `mHas` byte) / `cmplwi` / `beq` / `lwz r3,4(self)` / `li r4,1` / `bl __dt__9CAnimDataFv` /
  `extsh.` / `ble` / `mr r3,self` / `bl Free__7CMemoryFPCv`, i.e.
  `if (self) { if (self->mHas) { delete self->mItem; } if (deleting > 0) Free(self); }`. The
  `li r4,1` triple is what `delete p` emits (`CSandwormRelTail.cpp` measured it on
  `fn_56_13CF8`), and `rstl/auto_ptr.hpp`'s `mHas`/`mItem` layout is already +0/+4. It needs
  `#include "MetroidPrime/CAnimData.hpp"` so that `~CAnimData()` is declared and not defined in this
  translation unit; `CAnimData` **is** declared with its destructor out of line, so that should
  work, but it was not tried here.
- **`fn_6_6AF8` (0x6AF8, 0x3C) is the 0x3C deleting-destructor shape** - receiver guard,
  sign-extended flag test, `CMemory::Free(self)`, receiver returned - which
  `fn_56_13D50` in `src/MetroidPrime/ScriptObjects/CSandwormRelTail.cpp` already reproduces at
  100.00% inside module 56, and `fn_55_1064C` in module 55. It is referenced by nothing in the
  module, so a claim of `0x6AF8..0x6B34` would need `fn_6_6AF8` in `force_active:`. It was left
  alone here to keep this change to what the item's own runs describe.

NEW: progress-twin-rel-bacteriaswarm-autoptr | progress | module:BacteriaSwarm | `.text 0x6D30..0x6DB4` is two functions whose twin (`__dt__auto_ptr<CScannableObjectInfo>`, `src/MetroidPrime/Factories/CScannableObjectInfo.cpp`) is already matched: `fn_6_6D30` (0x64) is `rstl::auto_ptr<T>`'s teardown plus a deleting-destructor tail and needs `#include "MetroidPrime/CAnimData.hpp"` so the `delete mItem` emits `bl __dt__9CAnimDataFv` with `li r4,1`, and `fn_6_6D94` (0x20) is the same eight-instruction forwarder `fn_6_41FC` already is. Both are unreferenced, so the claim needs two `force_active:` names.

NEW: progress-twin-rel-bacteriaswarm-flagged-dtor | progress | module:BacteriaSwarm | `fn_6_6AF8` (0x3C) is the stock deleting destructor `fn_56_13D50`/`fn_55_1064C` already reproduce at 100.00% in modules 56 and 55, and is claimed by nothing - a `0x6AF8..0x6B34` unit plus one `force_active:` name is one function; `fn_6_6A8C` above it stays retail's unless a 12-byte `CToken` is declared (see this note's negative results).

## Notes for the next run

- The item's twin list is a map, not a work order, and the run that pays is the one that stops at a
  function the claim does not have to span. Three of this module's five all-twin runs are now
  claimed (0x4020..0x4068, 0x4150..0x42E8 minus the wall at 0x40C4, 0x5C00..0x5C48). The remaining
  twenty-three twins are single functions in the module's own class code and need the
  CActor/CPatterned/CAi hierarchy the tree does not model.
- **`mw_version="GC/2.7"` is worth trying on a REL unit before anything else.** Under the module
  default `GC/1.3.2` this unit was 4/6 with the two misses being pure scheduling, and the switch
  cost one configure.py line. The other REL tails in the repo
  (`CSandBossRelTail.cpp`, `CSandwormRelTail.cpp`) do not need it, so this is not a general rule -
  but the 0x1C24/0x5E8C/0x6A8C/0x6C70 `optional_object`/`vector` destructor shapes above may be the
  same kind of miss.
- The claimable ranges here are all **inside** dtk's auto units, so each one cuts `auto_*_text` in
  two (`auto_00_000000A0_text` is now `0xA0..0x4020`, `0x4068..0x4150`, `0x42E8..0x5C00`,
  `0x5C48..0x734C`). That is the `CRipperForwarders.cpp` and `ScriptCoin` arrangement and it needs
  no extra care beyond the four files.