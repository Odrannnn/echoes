# match-cmorphball-fn800d042c

`kind: match`, `target: MetroidPrime/Player/CMorphBall`.
**Result: PARTIAL.** `fn_800D042C` is now 100.00% and the unit's matched count rose **56 -> 57
of 158**; the unit still cannot flip and the reason is not this function (measured below).

## What the function is

`fn_800D042C` is retail's out-of-line copy of one `CCollisionInfo` (0x60 = 96 bytes), and
`config/G2ME01/splits.txt` claims 0x800D042C inside `MetroidPrime/Player/CMorphBall.cpp`
(`.text start:0x800C02A4 end:0x800D06CC`), so that is where the definition belongs.

It was already *defined* — but in the wrong unit and not byte-exact. `progress-prime1-ccollidablesphere`
had put a memberwise `*self = other` in `src/Collision/CCollidableSphere.cpp`; that file's own
comment already said the function "belongs" in `CMorphBall.cpp`. So the item was really two
things: put the definition in the unit whose split owns the address, and reproduce retail's bytes.

## Why the memberwise spelling cannot be made to match

Retail's 0x800D042C..0x800D0490 is 25 instructions, 0x64 bytes, and is **12 uniform `lfd`/`stfd`
8-byte moves** over the whole object. `*self = other` on this layout is a different shape
entirely: 24 `lwz`/`stw` moves plus `lhz`/`lbz` for the `TUniqueId` and the two bit-fields
(0xB0 bytes). Measured with scratch files compiled under the unit's own rule flags from
`build.ninja` (GC/2.7 mwcceppc, `-O4,p -inline deferred,noauto`):

| spelling of a 0x60-byte copy | emitted |
|---|---|
| `*self = other`, members are `CVector3f`/`CMaterialList`/bit-fields | 0xB0, `lwz`/`stw` + `lhz`/`lbz` |
| implicit copy ctor of the same class | 0xC4, `lfs`/`stfs` + `lwz` |
| `memcpy(self, &other, 0x60)` | 0x24, a `bl` to `memcpy` |
| `u64[12]` class assignment | 0x64 but `lwz` pairs — **wrong instructions** |
| `double[12]` class assignment | **0x64, exactly retail's 12 `lfd`/`stfd` pairs** |
| `double[2]` x 6, or 12 `double` members | 0x64, same bytes |
| loop `for (i<12) d[i] = s[i]` over a `double[12]` view | **0x64, byte-identical** |

So the only thing that reaches retail's bytes is a **`double`-typed** 8-byte view. `u64[12]` is
the same size and the same 0x64 length but comes out as `lwz` pairs, and retail's 0x64 bytes
contain `lfd`/`stfd` and no `lfdu`/`stfdu` — so the source loads through a `double` lvalue, not
an integer one. That is the whole finding, and it is a codegen fact about MWCC's 8-byte-float
block copy, not a guess.

## The change

- `src/MetroidPrime/Player/CMorphBall.cpp` — new `union SCCollisionInfoBlock { CCollisionInfo
  mInfo; double mQuads[sizeof(CCollisionInfo)/sizeof(double)]; }` plus the `extern "C"`
  `fn_800D042C` that copies the 12 quads. The array length is derived from `sizeof` rather
  than written as `12`, so a layout change cannot silently desynchronise the view; the
  `CHECK_SIZEOF(CCollisionInfo, 0x60)` in `include/Collision/CCollisionInfo.hpp` is what ties
  the view's size to the object's.
- `src/Collision/CCollidableSphere.cpp` — the duplicate definition is removed. It is not moved
  to `src/Collision/CCollisionInfo.cpp` because that unit is `MatchingFor` and byte-exact.
  The call stays, and it is a `bl` to an undefined symbol in the retail object dump too
  (`build/G2ME01/obj/Collision/CCollidableSphere.o` lists `U fn_800D042C`), so the two now
  agree — verified: our object also lists `U fn_800D042C`, and `CCollidableSphere` holds
  **17/17 functions at 100.00%** across the change (it was already `NonMatching`, over its
  claimed `.text` by 268 bytes of COMDAT weak copies, so it does not flip either).

This is retail's own lowering, not a shortcut round the work: all 96 bytes are copied, and the
only thing that depends on the spelling is this function's own 0x64 bytes.

## Measured

```
$ python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json
matched  10443 -> 10444   linked 5048 -> 5048   (+1 functions at 100%, 0 units newly linked)
  +100%    main/MetroidPrime/Player/CMorphBall :: fn_800D042C
no regression
```

`build/report.json` for `main/MetroidPrime/Player/CMorphBall`:
`fn_800D042C` -> `{'size': '100', 'fuzzy_match_percent': 100.0}`; unit `matched_functions`
56 -> 57, `fuzzy_match_percent` 18.51 -> 18.66. `main/Collision/CCollisionInfo` 6/6 and
`main/Collision/CCollidableSphere` 17/17, both unchanged at 100.00%.

`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (retail; both units
touched are `NonMatching`, so neither object is in the link and the DOL is untouched).
`decomp_build.sh` `All:` 31.68% fuzzy, 24.26% matched, 11.83% linked (10444 / 28465) — the
count rose by exactly this function.

Host check (the reviewer looks for "wrong on the host"): the file compiles clean under the
port's own g++ command from `tools/probe_sources.sh` (`-DTARGET_PC -DAURORA -std=c++20
-include platform/compat.h`, exit 0). The view is not endianness- or width-dependent — it is a
byte-for-byte copy of a 96-byte object — and `CCollisionInfo` contains a `u64`, so the object
and the union are 8-byte aligned and every `double` access is aligned.

`python3 tools/check_decl_order.py --unit MetroidPrime/Player/CMorphBall` still says **"would
break on a flip"** — the unit was already permuted before this change and is 57/158; the new
function is declared descending by retail offset relative to its actual neighbours
(`fn_800D0130`, `fn_800D0170`), which is the rule, but the unit as a whole is not ordered.

## What still stops the flip (measured, not guessed)

`tools/flip_test.sh MetroidPrime/Player/CMorphBall.cpp` -> `FAIL`, mwldeppc `undefined:` for
three symbols **that retail's own `CMorphBall.o` defines**:

```
undefined: 'CElementGen::GetEmitterTime() const'
undefined: 'fn_800CD4B8'
undefined: 'fn_800CD460'
```

Verified in `build/G2ME01/obj/MetroidPrime/Player/CMorphBall.o`:

```
0000a2b4 T GetEmitterTime__11CElementGenCFv
0000d1bc T fn_800CD460
0000d214 T fn_800CD4B8
```

So those three are functions of *other* classes that retail emitted into this TU, and our
scaffold does not define them. Running the same flip on the **stashed, clean** tree fails with
the same three **plus `fn_800D042C` twice** — so this item removed one of the four link errors
and introduced none.

`tools/unit_fit.sh MetroidPrime/Player/CMorphBall.cpp` gives the rest:

```
.text  claimed 66600   ours 17924   retail 66600   SHORT by 48676
50 function(s) present in ours but not in the retail unit object, 4800 bytes total
```

Most of the "extra" are COMDAT weak template instantiations (`rstl::reserved_vector::resize`,
`vector` destructors, `basic_string::compare`, ...) emitted in a trailing pool — the
emission-order wall in `docs/RUNNING_THE_DECOMP.md`. The flip needs the other 101 functions
written and the three undefined symbols defined; that is the same work the queued item already
describes, so no `NEW:` line is filed for it.

## Files

- `src/MetroidPrime/Player/CMorphBall.cpp` — added the union (lines 53-56) and
  `fn_800D042C` (lines 58-65), with the measurement note above them.
- `src/Collision/CCollidableSphere.cpp` — removed the duplicate `fn_800D042C` definition,
  replaced by a comment saying where it lives and why.

`docs/HANDOFF.md` shows a two-line diff in `git status`; that is `MP_GATE_DOCS_WRITE=1` inside
`tools/gate.sh` rewriting the derived counts, not an edit of mine.

---

# Run 2 (2026-09-30, lane 1)

**Result: PARTIAL, and larger than run 1.** The unit's matched count rose **57 -> 67 of 158**
(+10 functions), the judge is `PARTIAL ... commit it and keep the item`, and the flip still fails
for the reason run 1 measured (the 91 unwritten functions), not for anything in this diff.

Run 1's finding about `fn_800D042C` still holds and is untouched. What is new is that the
**whole `rstl` block at the tail of this TU is decompilable**, and that the repo already contains
the templates that produce retail's bytes - the previous run wrote a placeholder struct by hand
where the real `rstl` type was the answer.

## The ten new matches

| retail | bytes | what it is | spelling that reaches 100% |
|---|---|---|---|
| `fn_800D0490` | 184 | `rstl::vector<pair<TUniqueId,float>>::reserve(int)` | body written out, not `self->reserve()` |
| `fn_800D0548` | 60 | the `rstl::uninitialized_copy` helper it calls | body written out in `pointer_iterator` terms |
| `fn_800D0024` | 64 | `reserved_vector<CVector3f,15>` "fill 15 from empty" | `mCount = 0; fn_800D0064(self, 15, v); return self;` |
| `fn_800D0064` | 204 | `reserved_vector<CVector3f,15>::resize` | `rstl::uninitialized_fill_n(data()+count, n-count, *value)` |
| `fn_800D0170` | 124 | `reserved_vector<float,15>::resize` (was 81.77%) | same, with `T = float` |
| `fn_800D0130` | 64 | the `N = 15` wrapper over `fn_800D0170` (was 100%, kept) | unchanged |
| `fn_800D01EC` | 64 | `reserved_vector<CVector3f,5>` "fill 5 from empty" | same shape as `fn_800D0024` |
| `fn_800D022C` | 204 | `reserved_vector<CVector3f,5>::resize` | same as `fn_800D0064` |
| `fn_800D02F8` | 64 | `reserved_vector<CQuaternion,5>` "fill 5 from empty" | same wrapper shape |
| `fn_800D0338` | 244 | `reserved_vector<CQuaternion,5>::resize` | same fill, 16-byte element |
| `fn_800CF02C` | 132 | 4th link of the teardown chain; takes a `rstl::vector` | `rstl::destroy(begin(), end())` + `Free(mItems)` |

## The three findings that got them there (all measured, all on this tree)

**1. The placeholder struct was the wrong abstraction - `rstl::reserved_vector<T, N>` is the real
one, and its `resize` is byte-identical to retail's four fills.** Run 1 measured `fn_800D0170` at
81.77% with a hand-written `struct { int mCount; float mBuffer[15]; }` and a memberwise
`mBuffer[i] = *value`, and attributed the gap to register allocation. It is not allocation: the
hand-written loop folds the `+4` of `&mBuffer[i]` into the store offset and emits **`stfsu`**,
where retail's fill base is `slwi r0,count,2` / `add r5,r3,r0` / `addi r5,r5,4` - that is
`(self + count*stride) + 4`, which is what `data() + mCount` on `reserved_vector`'s
`uchar mData[]` (starting at +4) lowers to - and its body is `stfs` + `addi` on a **walked
pointer**, which is `rstl::uninitialized_fill_n`'s `for (i = 0, cur = dest; ... ++cur)`. Swapping
the placeholder for the real typedef and the loop for `uninitialized_fill_n` takes
`fn_800D0170` from 81.77% to **100.00%** and the three siblings from `---` to 100.00%.

**2. `-inline deferred,noauto` means the templates must be *spelled out*, not called.** This unit's
own rule flags (from `build.ninja`) make `rstl::reserved_vector::resize`,
`rstl::vector::reserve` and `rstl::uninitialized_copy` all emit as **out-of-line weak
instantiations**, and the `extern "C"` function becomes a forwarder:

| spelling | result |
|---|---|
| `self->resize(n, *value)` | weak `resize__Q24rstl21reserved_vector<f,15>FiRCf` emitted; the `extern "C"` symbol is a 0x20-byte forwarder, objdiff `---` |
| `self->reserve(size)` | 17.09%, a forwarder to the weak `reserve__Q24rstl62vector<...>Fi` |
| `rstl::uninitialized_copy(begin, end, out)` with `vector::iterator` | 12-instruction stub (iterator conversion is outlined) |
| all three written out | 100.00% each |

This is the same situation `include/rstl/reserved_vector.hpp` already documents for
`operator=` ("has to be written out by hand in a .cpp under an `extern "C"` name").

**3. `uninitialized_copy` has two overloads and only one of them is retail's.** Retail's
`fn_800D0548` opens `lwz r6,0(r3)` / `lwz r0,0(r4)` - it reads each argument's `current` field -
so the loop is `uninitialized_copy`'s `It`/`It` form over `pointer_iterator`, not the `S*`/`S*`
overload. The same loop written over raw pointers parks the bound in `r3` instead of `r0` and
scores **98.67%** - one instruction, the whole gap.

Two further shapes that had to be found by measurement, not guessed:
- `fn_800CF02C`'s four `stw r3,20(r1)`..`stw r3,8(r1)` are `rstl::destroy`'s two `pointer_iterator`
  arguments. **Calling** `rstl::destroy(v->begin(), v->end())` reproduces them; an explicit
  `for (it = begin(); it != end(); ++it) rstl::destroy(&*it);` emits the same walk with no stack
  temporaries and a 16-byte frame instead of 32 (81.58%). Adding the `mItems = nullptr` /
  `mCount = 0` / `mCapacity = 0` stores retail does not have costs 87.88%.
- `fn_800D0338`'s 16-byte element is one `lfs` plus three `lwz` per element. `CQuaternion` and a
  placeholder `struct { float; uint; uint; uint; }` both score 100%; **four `float`s would not**,
  because that mixes the two load sizes. The type is this file's choice, not a measurement -
  the retail object does not resolve the symbol.

## What I did NOT do, and why (both measured on this tree)

**`GetBallTouchRadius` (0x800CE9A4, 36 bytes) measures 100.00% as
`return gpTweakBall->GetBallTouchRadius();` - and it fails the gate.** `tools/sda.py -28244`
resolves retail's `lwz r3,-28244(r13)` to `gpTweakBall`, and retail's 9 instructions are exactly
that call and nothing else. Writing it makes `CTweakBall::GetBallTouchRadius() const` a **new
undefined symbol in the port's link** (`build/probe-logs/link_check.log`: `NEW CTweakBall::
GetBallTouchRadius() const`) and `tools/link_check.sh --strict` fails at **251 undefined against a
baseline of 250 (GREW)**, which is `goal_check.sh`'s `FAIL gate.sh`. The cause is that
`src/MetroidPrime/Tweaks/CTweakBall.cpp` is **not in the port's `files.cmake`**
(`grep CTweakBall files.cmake` returns nothing). So the body stays a scaffold with the correct
line in a TODO comment, and the fix needs that unit added to `files.cmake` - not this item's to
make. This is the only 100%-in-reach function I left on the table.

NEW: cmorphball-touchradius | match | MetroidPrime/Player/CMorphBall | GetBallTouchRadius measures 100.00% as `return gpTweakBall->GetBallTouchRadius();` but src/MetroidPrime/Tweaks/CTweakBall.cpp is not in the port's files.cmake, so the call adds a new undefined symbol and link_check --strict fails at 251 vs 250

**`fn_800D0640` (0x800D0640, 140 bytes) reaches 97.14%, not 100%.** It is the same
`reserved_vector`-shaped element walk plus the `deleting > 0` free, and
`static_cast< rstl::reserved_vector< T, N >* >(self)->~reserved_vector()` reaches 97.14% with
`struct { ~T() {} }`, `pair<TUniqueId,float>`, or `reserved_vector<CDeferredParticleEffect*, 8>`
- all identical. The **one** extra instruction is a second `beq` right after the null check that
retail does not have; every other instruction matches. Not filed as `WALL`: 97.14% is one
instruction from a wall, not a wall, and the spellings tried are recorded above.

**`fn_800D0584` (0x800D0584, 188 bytes)** is a `__sinit`-shaped static initialiser writing
byte-pairs into four `r13` globals plus `__register_global_object`. Not attempted - it needs the
identity of four `Tweaks`-REL statics, not a spelling.

## Measured

```
$ python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json
matched  10465 -> 10475   linked 5051 -> 5051   (+10 functions at 100%, 0 units newly linked)
  +100%    main/MetroidPrime/Player/CMorphBall :: fn_800CF02C
  +100%    main/MetroidPrime/Player/CMorphBall :: fn_800D0024
  +100%    main/MetroidPrime/Player/CMorphBall :: fn_800D0064
  +100%    main/MetroidPrime/Player/CMorphBall :: fn_800D0170
  +100%    main/MetroidPrime/Player/CMorphBall :: fn_800D01EC
  +100%    main/MetroidPrime/Player/CMorphBall :: fn_800D022C
  +100%    main/MetroidPrime/Player/CMorphBall :: fn_800D02F8
  +100%    main/MetroidPrime/Player/CMorphBall :: fn_800D0338
  +100%    main/MetroidPrime/Player/CMorphBall :: fn_800D0490
  +100%    main/MetroidPrime/Player/CMorphBall :: fn_800D0548
no regression
```

`build/report.json` for `main/MetroidPrime/Player/CMorphBall`: unit `matched_functions` **57 -> 67**,
`fuzzy_match_percent` 18.66 -> 20.52. `fn_800D0170` `100` (100.0), was `100` (81.77).

```
$ ./tools/goal_check.sh build/goal/item.json
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 10465 -> 10475   linked 5051 -> 5051
  ok    check_symbol_names.py
  ok    All:  31.74% fuzzy, 24.32% matched, 11.84% linked (10475 / 28465 functions)
  ok    target rose: main/MetroidPrime/Player/CMorphBall: 57 -> 67 / 158 functions
  ok    no asm added
goal_check: PARTIAL match-cmorphball-fn800d042c - flip_test ... FAIL, but the target rose; commit it and keep the item
```

`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (retail; the unit is
`NonMatching`, so its object is not in the link). `python3 tools/check_symbol_names.py`:
`checked 505 units; 0 declared names are missing from their object`.

## The flip, still measured not guessed

`tools/flip_test.sh MetroidPrime/Player/CMorphBall.cpp` -> `FAIL`, mwldeppc `undefined:` for
**two** symbols (down from run 1's three, which listed `fn_800CD460` as well - that one is not in
this run's error list, so something in the link order moved; the other two are unchanged):

```
undefined: 'CElementGen::GetEmitterTime() const'
undefined: 'fn_800CD4B8'
```

`fn_800D0490`/`fn_800D0548` are **no longer** on that list: this diff defines the symbols that were
undefined in it. Run 1's `unit_fit.sh` figure stands for the rest - `.text claimed 66600 ours
17924`, 50 functions present in ours but not in retail, mostly COMDAT weak template
instantiations in a trailing pool (the emission-order wall in `docs/RUNNING_THE_DECOMP.md`).
Closing the flip still needs the other 91 functions and the two symbols above.

`python3 tools/check_decl_order.py --unit MetroidPrime/Player/CMorphBall` still says **"would
break on a flip"** - the unit was already permuted before this change (run 1 measured the same).
The new functions are declared **descending by retail offset** among themselves and relative to
their actual neighbours (`fn_800D0548` 0x800D0548, `fn_800D0490` 0x800D0490, `fn_800D042C`
0x800D042C, then `fn_800D0338` .. `fn_800D0024` descending), which is the rule; the unit as a
whole is not ordered, and it was not before.

## Files

- `src/MetroidPrime/Player/CMorphBall.cpp` only. Declarations reordered so the tail block reads
  0x800D0548, 0x800D0490, 0x800D042C, 0x800D0338, 0x800D02F8, 0x800D022C, 0x800D01EC, 0x800D0170,
  0x800D0130, 0x800D0064, 0x800D0024, then `fn_800CF02C` (0x800CF02C) before the existing
  0x800CEFD8/0x800CEF84/0x800CEF2C chain. `fn_800CEFD8` and `fn_800CEF84` were disturbed and
  restored byte-for-byte from `HEAD` (a reordering script clobbered them mid-run); both are
  still 100.00% and the unit count is the +10 above.
- No include change, no `configure.py`, no `files.cmake`, no `splits.txt`, no `.s`.

`docs/HANDOFF.md` shows a two-line diff in `git status`; that is `MP_GATE_DOCS_WRITE=1` inside
`tools/gate.sh` rewriting the derived counts, not an edit of mine.
