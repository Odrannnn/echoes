# progress-fn-names-canimationset

`kind: progress`, `target: Kyoto/Animation/CAnimationSet`. Unit stays `NonMatching`; the flip was
not attempted (the item's kind does not ask for it, and the unit is 16 functions short of it).

## Result, measured

`build/goal/judge/report.base.json` against the regenerated `build/report.json`, unit
`main/Kyoto/Animation/CAnimationSet`:

| | before | after |
|---|---|---|
| `matched_functions` | **13 / 67** | **51 / 67** |
| `matched_functions_percent` | 19.402985 | 76.119403 |
| `matched_code` | 1844 | 4944 |
| `matched_code_percent` | 25.540167 | 68.476454 |
| `fuzzy_match_percent` | 25.540167 | 76.865672 |
| `total_code` | 7220 | **7220** (unchanged) |
| `total_functions` | 67 | **67** (unchanged) |

Whole build, from the judge: `All: 32.90% fuzzy, 25.72% matched, 12.17% linked (11490 / 28465
functions)` against `11452` in the baseline - **+38 matched functions**, and the judge's own
per-function report diff (inside `gate.sh`) is what established that **no function anywhere got
worse**.

`./tools/goal_check.sh build/goal/item.json`, verbatim:

```
goal_check: item progress-fn-names-canimationset (progress) target=Kyoto/Animation/CAnimationSet
goal_check: baseline .../wt-mp2-goal-L1/build/goal/judge/report.base.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11452 -> 11490   linked 5587 -> 5587
  ok    check_symbol_names.py
  ok    All:  32.90% fuzzy, 25.72% matched, 12.17% linked (11490 / 28465 functions)
  ok    target rose: main/Kyoto/Animation/CAnimationSet: 13 -> 51 / 67 functions
  ok    no asm added
goal_check: PASS progress-fn-names-canimationset
```

and separately `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.

The diff is `src/Kyoto/Animation/CAnimationSet.cpp` (+700 lines),
`include/Kyoto/Animation/CAnimPOIData.hpp` (+16) and `docs/research/decl_order.md` (+8).
`configure.py`, `config/`, `files.cmake`, `build/goal/` and the three big docs are untouched. No
`asm` was added.

## The item's claim, re-measured

The item says "48 of the unit's 54 unnamed retail functions (5.4 kB total) are already
byte-identical to code this object emits under a mangled template name". **That is exact.** The
measurement is `build/G2ME01/obj/Kyoto/Animation/CAnimationSet.o` (retail's own object, with its
relocations resolved) against `build/G2ME01/src/Kyoto/Animation/CAnimationSet.o`, comparing every
`T`/`W` symbol's `.text` bytes:

```
54 fn_ functions in the unit's retail object
48 of them byte-identical to a function our object already contained
 6 not: fn_8028DB8C 132  fn_8028DC10 80  fn_8028DD1C 96
       fn_8028DE38 96   fn_8028DE98 168 fn_8028DF40 88
```

The six misses are the `vector< pair<uint, CAdditiveAnimationInfo> >` and
`vector< CHalfTransition >` copy constructors, their `uninitialized_copy` halves, and the two
`destroy_impl` range walks for `CTransition` - they differ from our instantiations in the
element-size immediates and are not in this change.

So the item's arithmetic holds. What it did not say is that **"byte-identical" is necessary but
not sufficient**: 48 candidates, 38 matched. The other ten are characterised below, and none of
them is a spelling I did not try.

## What was written, and the technique

mwceppc emits an out-of-line copy of a template under its *mangled* name, dtk gave retail's own
copy of the same code the name of the address it sits at, and objdiff pairs functions **by name** -
so retail's function scored 0.00% however right the bytes were. No C++ declaration can rename a
template instantiation, so the only way to give objdiff a symbol to pair is to write the function
out under an `extern "C"` name. This is the repo's existing practice three times over
(`fn_80143CD4` in `src/MetroidPrime/Player/CGameState.cpp`, `fn_800F4FB4` in
`src/MetroidPrime/BodyState/CBSLocomotion.cpp`, and the thirteen in
`src/WorldFormat/CMetroidAreaCollider.cpp` from `progress-prime1-cmetroidareacollider`), and
`include/rstl/reserved_vector.hpp` documents it in the source.

**52 functions were written out, 38 of them to 100.00%.** Each body is the body
`include/rstl/construct.hpp` / `include/rstl/vector.hpp` / `include/rstl/pointer_iterator.hpp`
already spell, and each was read out of retail's object before it was written. Nothing is a stub.
The comments above each one quote the disassembly it was written from, and say which measurement
decided the spelling.

**Declarations are in descending retail offset**, interleaved with the five `CAnimationSet`
methods at their own addresses (0x8028D81C, 0x8028D68C, 0x8028D624, 0x8028D4DC, 0x8028CB88), which
is what mwcceppc needs to emit them ascending.

### The 38 that reached 100.00%, by group

* **12 `rstl::construct<T>` / `construct_impl<T>` / `destroy<T>` / `destroy_impl<T>` wrappers** -
  `fn_8028CCA8`, `CCC8`, `CEF8`, `D0B0`, `D268`, `D420`, `D4BC`, `D604`, `D78C`, `DB48`, `DB68`,
  `E1D0`, `E6E8`, `DF98`, `E708`. Each is a frame and one `bl`, and each is written as a call to
  its neighbour *by retail's name*, because a `bl` is a `bl` in both objects and objdiff compares
  the instruction, not the symbol it relocates to. That is measured, not assumed:
  `CAreaCollisionCache::AddOctreeLeafCache` scores 100.00% with retail's `bl fn_80248D74` against
  our `bl push_back__Q24rstl61...`.
* **5 `uninitialized_copy_n< T*, T* >`** - `fn_8028CE90`, `D048`, `D200`, `D3B8`, `D9D4`.
* **3 `uninitialized_copy< pointer_iterator< T, ... >, T* >`** - `fn_8028E534`, `E410`, `E2C0`
  is 95.42% (below), so this group is 2.
* **`fn_8028CC70`** - `vector<CAnimPOIData>::push_back_unsafe`.
* **`fn_8028CCF0`** - `CAnimPOIData`'s copy constructor, as a free function over the same
  statements (a constructor's symbol name is fixed by the mangler, so it cannot be given the name
  `fn_` as a member).
* **`fn_8028D440`** - `CAnimPOIData`'s deleting destructor, same reason.
* **`fn_8028D950`** - `vector<CAnimPOIData>`'s copy constructor, as a free function.
* **`fn_8028D7AC`** - `rstl::pair<uint, CAdditiveAnimationInfo>::pair( CInputStream& )`.
* **`fn_8028DA3C`, `DC60`, `DD7C`, `DFB8`** - the four `vector<T>::~vector()` *deleting*
  destructors, and **`fn_8028DAC0`, `DE00`, `E03C`** - three `destroy< pointer_iterator< T, ... > >`.
* **`fn_8028DAF8`, `E074`** - two `destroy_impl< pointer_iterator< T, ... > >` range walks.
* **`fn_8028DE38`** - `destroy_impl< pointer_iterator< CTransition, ... > >`.

## Measurements that decided the spellings (each one cost a build)

* **`new (dest) T(in)` is 40 bytes, retail's `rstl::construct<T>` stream wrapper is 32.** mwceppc
  expands the placement form into "call `operator new`, test the result against null, then
  construct", and that `cmplwi r3,0` / `beq` pair is retail's *other* `construct` function -
  `fn_8028CCC8` is `construct_impl` and carries exactly that pair. Written inline, the four stream
  wrappers (`fn_8028E1D0`, `DF98`, `D4BC`, `D604`) all sat at **75.00%**. Routing the construction
  through a file-local `Place*` function leaves the wrapper as a frame and one `bl`: **100.00%**,
  four functions. Same shape `fn_80248F0C` is written in
  `src/WorldFormat/CMetroidAreaCollider.cpp`.
* **The same applies to `fn_8028CCF0`'s four member copy-constructors.** Written as placement
  news, each cost an `addic.`/`beq`: 144 bytes against retail's 112, **60.96%**. Four file-local
  `Copy*PoiVec` helpers: **100.00%**.
* **A `uninitialized_copy`-style loop must return its end cursor.** Without `return cur;` the
  epilogue's `mr r3,r30` is missing and the whole function shifts: `fn_8028D9D4` and its four
  siblings went 93.85% -> **100.00%**, `fn_8028D950` 96.97% -> **100.00%**, `fn_8028D7AC` 89.29%
  -> **100.00%** (a `pair`'s constructor returns `this`).
* **The loop body is `rstl::construct`, not `operator=`.** `*cur = *it` copy-*assigns* and pulls in
  `rc_ptr<IMetaTrans>::ReleaseData`, which retail's `construct` of a trivially-destructible
  `CHalfTransition`/`CTransition` does not: `fn_8028E534` 0.00% -> **97.62%** and `fn_8028E410`
  0.00% -> **97.20%** with `rstl::construct(cur, *it)`, both then 100.00% after the return value.
* **`destroy`/`destroy_impl` over `pointer_iterator` take the iterators *by value*.** As two
  `T* const*` arguments the wrapper came to 12 instructions against retail's 14;
  `fn_8028DAC0`/`fn_8028E03C` were **66.29%** and went to **100.00%** with
  `void fn(PoiIter begin, PoiIter end)`. `rstl::pointer_iterator` is a class, so mwceppc passes
  it by hidden pointer and the callee copies it to the outgoing slot - which is retail's
  `lwz r5,0(r4)` / `stw r5,8(r1)` pair.
* **`vector<CAnimPOIData>::push_back_unsafe` stores `mCount` *before* the construct.**
  `fn_8028CCA8(dest, in); ++self->mCount;` is **40.64%**; `rstl::vector.hpp`'s own
  `rstl::construct(mItems + mCount++, in)` - the post-increment in the index expression - is
  **96.79%** and 100.00% once the destination is a named local rather than a temporary.

## What is left, and why (measured; not a `WALL:`, see below)

Sixteen functions are still short. None is a spelling I did not try.

### 1. Six functions blocked on a one-line shared header: `rstl::pointer_iterator` is one word, retail's is two

`fn_8028E63C` (82.70%), `fn_8028E588` (73.89%), `fn_8028E474` (63.69%), `fn_8028E350` (63.69%),
`fn_8028E1F0` (65.38%), and `fn_8028E754` (92.69%).

**The measurement.** Retail's five `vector<T>::reserve` bodies each build **four** words on the
stack - `stw` to 8(r1), 12(r1), 16(r1) and 20(r1) - before the `bl` to `uninitialized_copy`, and
`fn_8028E754` reloads its bound with `lwz r0,0(r29)` from an address held in a register. That is
**two** `pointer_iterator`s of **two** words each. `include/rstl/pointer_iterator.hpp` in this repo
has one member, `T* current` - the owner is taken by the constructor and discarded - so ours builds
two words and the two objects are one instruction pair short *and* one register short.

`fn_8028E754` is 92.69% rather than lower because it only *dereferences* the iterators, so the
second word is dead there; the missing instructions are the two `mr`s that pin the two argument
addresses instead of reloading them.

`include/rstl/pointer_iterator.hpp` is included by every `rstl::vector` in the tree, and
`include/rstl/reserved_vector.hpp` records what the last shared-header change of this kind cost:
eight `Matching` units, one function each, and `main.dol` off its sha1. **Not attempted here.**
This is a codegen/layout rule, not a blocker, so it is not filed as `NEW:`.

### 2. Four functions blocked on `CInputStream::Get<T>()`'s hidden `TType<T>` argument

`fn_8028CBD0` (160 B), `fn_8028D524` (224 B), `fn_8028D6D4` (184 B), `fn_8028E0E4` (236 B) - all
0.00%, all byte-identical, none written. Characterised at the end of
`src/Kyoto/Animation/CAnimationSet.cpp`: each loop opens with `lbz r0,lbl_804198xx` / `mr r5,r31` /
`stb r0,8(r1)` - a byte out of `.sbss`, its address passed as a third argument, and the argument
stored to a stack slot first. The third argument is the empty `TType<T>` that
`CInputStream::Get<T>( const TType<T>& type = TType<T>() )` takes by hidden pointer, and the four
labels (`lbl_80419838`, `lbl_80419830`, `lbl_80419834`, `lbl_80419828` in
`config/G2ME01/symbols.txt`) are four distinct one-byte `.sbss` objects, one per element type. So
retail's source is `in.Get<T>()` and the 32-byte `fn_` wrappers *are* retail's out-of-line copies
of that `Get<T>` - which is also why they carry no placement-new null test. Reproducing it needs a
`static` byte mwceppc reloads per iteration, which this tree's one-line `Get<T>` does not produce,
and then four 160-236-byte loop bodies to allocate registers identically on top.

### 3. Four functions at 92-95% - register allocation, two spellings tried each

`fn_8028E754` 92.69% (see 1), `fn_8028E2C0` 95.42%, and the two `vector<T>::~vector()`-adjacent
ones. `fn_8028E2C0` with the end iterator's *address* held in a register scores 95.42%; with the
end *value* hoisted it scores 93.19% (measured, both). `fn_8028E754` with the end value hoisted
scores 89.81%, with the address held 92.69%. The remaining diff in each is `mr` versus `lwz` of the
same two registers - the rest of the function, including the loop, the `bl` and the epilogue, is
byte-identical.

### 4. `fn_8028DC10`, `fn_8028DB8C`, `fn_8028DF40` - the six original misses

`fn_8028DB8C` (132 B, `vector< AdditivePair >` copy constructor), `fn_8028DC10` (80 B, its
`uninitialized_copy`), `fn_8028DF40` (88 B, `push_back_unsafe` for that vector) and the three
others. These are the six of the original 48 that were **not** byte-identical to anything our
object emits, so the technique's premise does not hold for them at all and they are real
decompilation work.

**No `WALL:` line.** The rule for one is "the same sub-100% score across several different
spellings you tried *in this run*, with the remaining diff only register allocation". The three
in group 3 are at 92-95% with a known single cause each and two spellings measured; the rest are
not register allocation at all. This is a description, not a park request.

## The one shared-header change, and why it is in this diff

`include/Kyoto/Animation/CAnimPOIData.hpp` gains two `friend` declarations for `fn_8028D440` and
`fn_8028CCF0`. `CAnimPOIData`'s four `rstl::vector` members are private and its copy constructor
and deleting destructor are two of the functions this item writes out; a mangler fixes a
constructor's symbol name, so neither can be given the name `fn_` as a member, and a free function
cannot reach private members. The `friend` shape is the one `include/Kyoto/Graphics/CGX.hpp`
already uses for `fn_802BCC74` and `fn_802BCC80`. It adds no member, moves nothing, and emits no
code; `CHECK_SIZEOF(CAnimPOIData, 0x44)` still holds and the DOL sha1 is unmoved.

## `docs/research/decl_order.md`

`python3 tools/check_decl_order.py` reports this unit permuted, and it is recorded there with the
measurement. The 52 written-out functions and the five `CAnimationSet` methods **are** declared
descending by retail offset, and mwcceppc emits 51 of the 52 in that order. **`fn_8028D950` alone
is not**: it lands between `fn_8028CCA8` and `fn_8028CCC8` whatever its source position - measured
by declaring it before `fn_8028D9D4` and after it, same result both times, so it is not a sort
mistake on my side. One function out of 52. The unit is not a flip candidate in any case (ten
functions are below 100% and four are unwritten), so this is recorded rather than chased.

## New queue items

None filed. The two groups above are a shared-header layout change and a codegen rule, and the
brief says not to file those: "a lesson or a codegen rule ... put it in your notes file". Both are
worth roughly 10 functions in this one unit and several more in every other unit with
`vector<T>::reserve`/`~vector()` in it, but they belong to whoever owns
`include/rstl/pointer_iterator.hpp` and `include/Kyoto/Streams/CInputStream.hpp`, not to a
`progress` item on one unit.

---

# Attempt 3 (lane 7) — 55/67 -> 66/67, +11 functions at 100%, `goal_check: PASS`

## Read this first

**The `NEW:` line queued by the previous attempt of this item is built on two claims that this run
disproved by measurement.** Do not work it as written:

```
NEW: match-unit-canimationset | match | Kyoto/Animation/CAnimationSet.cpp | 55/67 and 12 functions short;
the 5 `vector(CInputStream&)` ctors need a `.sbss` byte in CInputStream::Get, and the 5
`reserve`/`uninitialized_copy` partials all turn on rstl::pointer_iterator being 1 word where
retail's is 2
```

Both halves are wrong. The `.sbss` byte **is** produced by this tree's `CInputStream::Get` — four of
the five stream constructors are now at 100% because of it. And retail's `rstl::pointer_iterator` is
**one** word, like this tree's; nothing in `include/rstl/pointer_iterator.hpp` needs to change and
nothing in it was changed. Nine of the eleven functions this run matched turn on those two
corrections. The unit is now 66/67 and 99.83% fuzzy.

## Result, measured

`build/goal/judge/report.base.json` vs the regenerated `build/report.json`, unit
`main/Kyoto/Animation/CAnimationSet`:

| | before | after |
|---|---|---|
| `matched_functions` | **55 / 67** | **66 / 67** |
| `fuzzy_match_percent` | 82.34903 | **99.83379** |
| `matched_code` | 5056 | **6996** |
| `matched_code_percent` | 70.0277 | **96.89751** |
| `total_code` / `total_functions` | 7220 / 67 | **7220 / 67** (unchanged) |

Whole DOL: matched **11887 -> 11898**, linked **5727 -> 5727** (unchanged, as it must be: the unit
stays `NonMatching`). `python3 tools/report_diff.py build/goal/judge/report.base.json
build/report.json` prints `+100%` for exactly these eleven and then `no regression`:

```
fn_8028CBD0  fn_8028D6D4  fn_8028DE98  fn_8028E0E4  fn_8028E1F0  fn_8028E2C0
fn_8028E350  fn_8028E474  fn_8028E588  fn_8028E63C  fn_8028E754
```

`./tools/goal_check.sh build/goal/item.json`, verbatim:

```
goal_check: item progress-fn-names-canimationset (progress) target=Kyoto/Animation/CAnimationSet
goal_check: baseline .../wt-mp2-goal-L7/build/goal/judge/report.base.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11887 -> 11898   linked 5727 -> 5727
  ok    check_symbol_names.py
  ok    All:  33.66% fuzzy, 26.81% matched, 12.64% linked (11898 / 28465 functions)
  ok    target rose: main/Kyoto/Animation/CAnimationSet: 55 -> 66 / 67 functions
  ok    no asm added
goal_check: PASS progress-fn-names-canimationset
```

The diff is `src/Kyoto/Animation/CAnimationSet.cpp` only, +236/-106. `configure.py`, `config/`,
`files.cmake`, `include/`, `tools/` and `build/goal/` are untouched. No `asm` added. `docs/HANDOFF.md`
is rewritten by `gate.sh` itself (derived state block) and was reverted so the diff is one file.

## The one measurement that settles both of the previous run's walls

`build/G2ME01/obj/Kyoto/Animation/CAnimationSet.o` (retail, relocations resolved) against
`build/G2ME01/src/Kyoto/Animation/CAnimationSet.o` (ours), dumping every `T`/`W` symbol's `.text`
bytes and searching for each retail function's bytes as a subsequence of ours:

```
fn_8028CBD0 160 -> __ct__Q24rstl49vector<12CAnimPOIData,...>FR12CInputStreamRCQ24rstl17rmemory_allocator  (0xa0 = 160)
fn_8028D524 224 -> __ct__Q24rstl52vector<15CHalfTransition,...>FR12CInputStream...                        (0xe0 = 224)
fn_8028D6D4 184 -> __ct__Q24rstl77vector<pair<Ui,22CAdditiveAnimationInfo>,...>FR12CInputStream...       (0xb8 = 184)
fn_8028E0E4 236 -> __ct__Q24rstl47vector<10CAnimation,...>FR12CInputStream...                             (0xec = 236)
fn_8028DE98 168 -> (no match; ours is 0xf0 = 240, retail's variant inlines its reserve)
```

**Four of the five stream constructors were already in our object, byte-identical, under their
mangled template names.** They scored 0.00% for the reason every other function in this file scored
0.00% — objdiff pairs by name and a mangler fixes a template's — and for no other reason. The
previous note's "reproducing it needs a `static` byte mwceppc reloads per iteration, which this
tree's `CInputStream::Get` does not do" is not a codegen fact; `in.Get< T >()` in the loop produces
`lbz r0,lbl_804198xx` / `mr r5,rN` / `stb r0,8(r1)` exactly, because that byte is `Get<T>`'s
**default argument** `TType<T>()` materialised as a function-static. No header change was needed or
made.

## What each function needed (all measured; spellings the notes did not list)

Nine functions turn on **one** change: the hand-written `uninitialized_copy` / `destroy` wrappers
took their iterators as `T* const*`, retail's take them **by value**.

| change | functions | effect |
|---|---|---|
| `PoiIter begin, PoiIter end` instead of `CAnimPOIData* const*` | `fn_8028E754` | 92.69% -> **100.00%** |
| caller passes `self->begin(), self->end()` | `fn_8028E63C` | 82.70% -> **100.00%** |
| same, `HalfIter`/`TransIter`/`AnimIter` | `fn_8028E588`, `fn_8028E474`, `fn_8028E350`, `fn_8028E1F0` | 73.89/63.69/63.69/65.38% -> **100.00%** |
| same, callees re-signed too | `fn_8028E2C0` | 95.42% -> **100.00%** |
| `fn_8028E534`, `fn_8028E410` re-signed the same way | — | stayed 100.00% |

**Why.** mwceppc lowers a by-value class argument to a hidden pointer to the caller's *outgoing
copy*, and the callee reads the member through it — retail's `lwz r31,0(r3)` / `mr r29,r4` in
`fn_8028E754`. So (a) the callee **cannot hoist** the loop bound across the `bl` in its body: retail
re-issues `lwz r0,0(r29)` every iteration and so must we, and (b) the caller emits **two** stores
per iterator — the temporary, then the copy into the outgoing argument slot. That second point is
what the previous run read as "retail's iterator is two words". Retail's
`rstl::pointer_iterator` has one member; the two words on the stack are two *stores of the same
iterator*, not two members. It follows that the fix needs no change to
`include/rstl/pointer_iterator.hpp` — good, because that header is shared by every vector in the
tree.

Four more:

* **`fn_8028E754` / `fn_8028E2C0`: declare the source cursor before the destination cursor.**
  `CAnimPOIData* it = begin.get_pointer(); CAnimPOIData* cur = out;` scores 100.00%; the reverse
  order puts `out` in `r31` and `begin` in `r30` where retail has them the other way round
  (measured 82.88% on `fn_8028E754` after the by-value change alone, with the order still reversed).
* **`fn_8028E474`, `fn_8028E350`, `fn_8028E1F0`: the destroy half of a `reserve` is
  `rstl::destroy( mItems, mItems + mCount )`, not a loop.** A hand-rolled index loop reloads
  `mItems` every iteration; a hand-rolled cursor walk is closer but still not retail's walk. The
  template inlines to exactly retail's `cmplwi r30,0 / beq / addic. r0,r30,4 / beq / addi r3,r30,4 /
  bl ReleaseData`: **81.46% -> 100.00%** on the first two and 81.88% -> 100.00% on the third.
* **`fn_8028E588`: the copy half of a `reserve` is `rstl::uninitialized_copy( self->begin(),
  self->end(), newData )`, not a loop either.** `AdditivePair` is trivial so the template folds to
  three `lwz`/`stw` pairs and retail makes no call — but it is still called *with* `begin()` and
  `end()`, and their temporaries are retail's four dead stack words: **73.89% -> 100.00%**.
* **`fn_8028DE98`: `push_back_unsafe` must be the out-of-line `bl`.** Written
  `self->push_back_unsafe(in.Get< CTransition >())` mwceppc inlines it (20 instructions) where
  retail `bl`s `fn_8028DF40` at 0x8028DF50: 54.02%. Calling retail's `fn_8028DF40` by name:
  **100.00%**. The opposite of `fn_8028CBD0`, where `CAnimPOIData`'s user-declared copy constructor
  keeps the `push_back_unsafe` out of line and `self->push_back_unsafe(...)` is right first try.

## `fn_8028D524` — 0.00% -> 94.64%, and why it is not 100%

`rstl::vector< CHalfTransition >::vector( CInputStream& )`. The obvious spelling,
`self->push_back_unsafe(in.Get< CHalfTransition >())`, is **byte-identical to retail and scores
100.00%** — but it instantiates `CInputStream::Get< CHalfTransition >`, whose COMDAT references
`CHalfTransition::CHalfTransition(CInputStream&)`, and **no object in this tree defines that
symbol**. `src/Kyoto/Animation/CHalfTransition.cpp` does not exist and `Kyoto/Animation/CHalfTransition`
is not a unit; retail's constructor survives only as the address name `fn_8032250C`, defined by the
generated reach stub `build/G2ME01/obj/auto_03_8032250C_text.o`.

`main.dol` still links and still hashes to retail — a `NonMatching` unit is not in the DOL link at
all, so the undefined symbol is inert — but `main.elf` does not link, and a unit that cannot link
can never be flipped. So the element is built through retail's own `fn_8028D604`, which gets the
`lbz`/`stb`/`mr r5` triple back from a **defaulted** third parameter
`const TType< CHalfTransition >& type = TType< CHalfTransition >()` on it (unused by its body, so
its code is unchanged and it stays at 100.00%), and the temporary is a raw 12-byte slot.

What is left is three instructions in that temporary's destructor. Retail emits
`cmplwi r30,0 / beq / addi r3,r30,4 / bl ReleaseData`; reaching it through a `reinterpret_cast`
slot costs mwceppc an extra `addic. r0,r1,12 / beq` null test on the object address, which retail
does not have. Spellings tried and measured: `CHalfTransition tmp;` (won't compile, no default
ctor), `union { CHalfTransition obj; uint words[3]; } tmp;` (won't compile, "cannot construct
HalfSlot's direct member 'obj'"), `uint tmp[3]` + a named `CHalfTransition* slot` reused by all
three statements (92.84% — mwceppc hoists the address into a callee-saved register and emits
`mr r3,r28` where retail has `addi r3,r1,12`), `uint tmp[3]` with the `reinterpret_cast` written out
three times (94.64%, the spelling in the tree). Getting the last 3 instructions needs a real
`CHalfTransition` object in that slot, which needs its stream constructor by its C++ name, which
needs the `Kyoto/Animation/CHalfTransition` unit.

## The flip is blocked, and it was blocked before this change

`./tools/flip_test.sh Kyoto/Animation/CAnimationSet.cpp` **FAILS**, and it is a pre-existing
blocker, not something this diff introduced:

```
### mwldeppc.exe Linker Error:
#   undefined: 'CHalfTransition::CHalfTransition(CInputStream&)'
```

Verified by running the same command on the clean tree (`git checkout -- src/Kyoto/Animation/
CAnimationSet.cpp`, then the identical script): **the identical error, with this unit at 55/67**.
`build/G2ME01/src/Kyoto/Animation/CAnimationSet.o` on the clean tree already lists
`U __ct__15CHalfTransitionFR12CInputStream`; it has been inert only because `fn_8028D604` and
`PlaceCHalfTransition` are unreferenced and mwldeppc drops them, and the moment the unit is marked
`Matching` they become live.

**No `WALL:` line.** `fn_8028D524` is not register allocation or instruction scheduling — the
remaining difference is an extra null test mwceppc emits for a `reinterpret_cast` slot — and it has
one known external cause rather than several exhausted spellings.

## No `NEW:` filed, deliberately

The queued `NEW: match-unit-canimationset` covers this unit and this run has answered its reason;
a second item for the same unit would cost a lane an hour to re-derive what is above. The only
genuinely new blocker is the missing `Kyoto/Animation/CHalfTransition` unit, and its target would
be a unit that does not exist in `configure.py`, which the brief rules out as a placeholder.

## Other gates

```
./tools/decomp_build.sh Kyoto/Animation/CAnimationSet   # All: 11898/28465, unit 66/67
./tools/unit_fit.sh Kyoto/Animation/CAnimationSet.cpp   # 83 extras, 7892 B, all pre-existing
                                                         # COMDAT weak template copies
python3 tools/check_symbol_names.py                      # checked 515 units; 0 missing names
python3 tools/check_decl_order.py                        # 978 units, 31 permuted, all in decl_order.md
sha1sum build/G2ME01/main.dol                            # 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
```

The five new constructors are declared in descending retail offset (0xE0E4 after 0xE1D0, 0xDE98
after 0xDF40, 0xD6D4 after 0xD78C, 0xD524 after 0xD4BC, 0xCBD0 after 0xCC70 and before
`StreamEventSetList`), so the unit is no further from flippable than it was. Not committed.
