# progress-unit-cmemorycard — `MetroidPrime/CMemoryCard`, 10 -> 53 of 56 functions

`goal_check.sh`: **PASS**. `main/MetroidPrime/CMemoryCard` **10 -> 53 / 56 functions**,
47.81% -> 98.26% fuzzy, 43.88% -> 94.26% matched code. Project `All:`
33.13% -> 33.21% fuzzy, **11561 -> 11604 / 28465** functions, `linked` held at 5625.
The unit stays `NonMatching`; no `flip_test.sh` was run to decide anything.

## What the item actually was

The queue text ("take the cheapest unmatched first", 10/56 matching, several 32-byte
`fn_8017*` at 0.0%) pointed at the wrong thing. **44 of the 46 unmatched functions were
not missing source at all** - our object already declares every one of them, as this
translation unit's own weak `rstl` template instantiations and free helper templates.
They scored 0.00% because objdiff pairs functions by symbol name and `dtk` cannot name a
TU-local weak instantiation, so `config/G2ME01/symbols.txt` carries only `fn_8017*` for
them. The 32-byte ones the queue listed are `destroy<T>` / `destroy_impl<T>` / `construct<T>`
thunks, not work.

So the fix is the one documented in `docs/RUNNING_THE_DECOMP.md`, "Pairing a function the
retail symbol table has no name for": **rename the retail symbol, not the code.**

## Change 1 - 43 renames in `config/G2ME01/symbols.txt`

Each retail `fn_8017*` renamed to the mangled name mwcceppc emits, read out of
`build/G2ME01/src/MetroidPrime/CMemoryCard.o` with `powerpc-eabi-nm` (never guessed), after
identifying the instantiation from **size + the call graph in
`build/G2ME01/asm/MetroidPrime/CMemoryCard.s`** (retail's `.text` order lines up with ours
for everything up to `fn_80178270`; past the ctor the trailing template block is emitted in
a different order, so the callees decided it).

Addresses renamed (all `// type:function`, sizes unchanged):
`80176A88 80177220 80177268 801772EC 80177324 80177374 80177394 801773B8 80177C10 80177C94
80177CCC 80177D1C 80177D3C 80177D60 80177DB8 80177EB4 80177F0C 80177F90 80178270 801782A8
801782C8 801782F0 801783CC 801784FC 8017859C 801787CC 801787EC 80178838 80178858 80178880
801788C0 80178A00 80178A40 80178B4C 80178BE0 80178C74 80178D20 80178D88 80178E44 80178ECC
80178F78 80178F98 80178FE4`

Every one of them went **0.00% -> 100.00%**. Verified with `sha1sum build/G2ME01/main.dol`
still `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` and `tools/check_symbol_names.py`
`checked 515 units; 0 declared names are missing from their object` - the rename is exactly
what dtk renames the split object to, so the name it checks is the name the object has.

Two pairs are only distinguishable by their callee, and the symbols.txt entry says nothing
about which is which, so they are worth recording:

- `80178B4C` = `lower_bound<pointer_iterator<pair<Ui,CSaveWorldMemory>>>` (called from
  `__ct__11CMemoryCardFv`, the non-const `lower_bound` in the ctor);
  `80178BE0` = the `const_pointer_iterator` one (called from `find_by_key`, which takes
  `const vector&`). Same size (0x94), same shape, different instantiation.
- `80177F0C` = `__dt__vector<SEnvironmentVariable>` (it calls `80177F90`, which calls
  `internal_dereference<basic_string>`), **not** `__dt__vector<pair<Ui,Ui>>`, which is the
  same 0x84 bytes and is called by `__dt__11CMemoryCardFv` for `mScanStates`.
- `801783CC` (0x130) = `__ct__vector<CWorldLayers::Area>`; `__ct__vector<Ui>` (0x100) is
  the external `fn_8002E0EC` in another unit.

## Change 2 - `src/MetroidPrime/CMemoryCard.cpp`

**(a) The ctor's `push_back` -> `push_back_unsafe`, +1 function.** Retail's 56-byte
`fn_80178270` is `vector<CSaveWorldIntermediate>::push_back_unsafe` verbatim - no capacity
check, `construct(mItems + mCount++, x)`, `blr`. Spelled `push_back`, this file emitted a
124-byte out-of-line `push_back` that calls `reserve`, which is 124 bytes retail's unit
object does not have. `worlds.reserve(40)` above already guarantees the room, and retail's
function is the unchecked one, so this is faithful, not a shortcut. `__ct__11CMemoryCardFv`
stays 100.00% (measured) - the call shape in the ctor is one call either way.

**(b) `GetAreaAndWorldIdForSaveId`, 87.08% -> 90.45%,** and it becomes 196 bytes, retail's
size. Hoisting `areas.end()` into a local **before** the `rstl::find` call is what does it.
The three spellings tried, all measured:

| spelling | result |
|---|---|
| `rstl::find(areas.begin(), areas.end(), saveId)` + `!= areas.end()` (baseline) | 87.08%, 176 B |
| `areasEnd` local declared **before** `rstl::find`, subtraction still `areas.begin()` | **90.45%, 196 B** |
| `areasBegin` + `areasEnd` locals passed to `find`, subtraction `area - areasBegin` | 87.65%, 196 B |
| same search hand-rolled as a `for` loop over `*area == saveId` | 61.37%, 196 B |
| `areasEnd` local declared **after** the `find` call | 87.08%, 196 B |

`MergeEnvironmentVariables` was left at 86.20% (not measured against other spellings in
this run). Its whole diff is that retail re-derives `destination.end()` from `mCount`/`mItems`
after the search while ours reuses the cached `find`'s end, plus `add.` versus a separate
`add` in the inlined `push_back_unsafe`. Same class of thing; no spelling tried.

## Still unmatched (3)

- **`fn_80178AD0` (0x7C)** - the only genuinely missing body. It builds a `rstl::string` from
  a `CInputStream&` into a **frame temporary at r1+8** (the result is then dropped - a MWCC
  artifact) and reads three `uint` from the stream into +0x10/+0x14/+0x18 of `this`, with no
  other call. Nothing in this file needs such a function and its class is unknown, so it was
  not guessed at.
- `GetAreaAndWorldIdForSaveId` 90.45%, `MergeEnvironmentVariables` 86.20%.

## The unit is still not a `Matching` candidate

`tools/unit_fit.sh MetroidPrime/CMemoryCard.cpp`: `.text` claimed 10384 / ours 14800
(was 14864 before the `push_back_unsafe` change, which removed the 124-byte `push_back`)
/ over by 4416, `.rodata` **18 against retail's 24**, `.sdata2` **0 against 8**,
and `.sdata` 4 bytes unclaimed. The over is 37 weak template instantiations retail resolves
from other units (the tool's own "harmless causes" case), but the `.rodata`/`.sdata2`
shortfalls are real and are the more likely flip blocker once those are settled: retail
materialises the `?()HINT_Hints` literal as 0x18 bytes against our `@stringBase0` 0x12, and
the `lbl_8041C840 = 0xFFFFFFFF` `kInvalidAreaId` word that `GetAreaAndWorldIdForSaveId`
reads via SDA21 instead of defining.

## Gates

`sha1sum build/G2ME01/main.dol` `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (unchanged);
`tools/check_symbol_names.py` 0 missing; `tools/check_decl_order.py` ok, 978 units;
`tools/check_raw_offsets.py` ok, 165 sites; `./tools/goal_check.sh build/goal/item.json`
PASS (gate.sh green, DOL + 86 RELs, report diff, wiring, docs claims, port probe).

WALL: GetAreaAndWorldIdForSaveId 90.45% - four spellings tried, the rest is frame-slot
assignment (retail keeps `end` in a register and spills 8 iterator copies where ours keeps
4 and reloads), nothing structural left.