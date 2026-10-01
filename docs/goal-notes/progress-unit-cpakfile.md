# progress-unit-cpakfile — `main/Kyoto/CPakFile` 25/33 → 26/33

## What landed

One function reached 100%: `rstl::vector<CPakFile::SResInfo>::reserve(int)` (232 B, retail
`.text:0x80324AC4`). `EnsureWorldPakReady` also rose, 90.89% → 94.37%.

Two edits, both in `src/Kyoto/CPakFile.cpp`:

1. **The `reserve` instantiation is now written in this translation unit** (lines 22-58), with the
   allocation behind a free inline helper `alloc_resinfo(int)`.
   `include/rstl/vector.hpp:158`'s template hands the buffer to `rstl::rmemory_allocator::allocate`,
   which is out of line, so our object emitted `bl allocate__Q24rstl17rmemory_allocatorFi`; retail's
   object emits the `CCallStack` constructor and `bl Alloc__7CMemoryFUlQ210IAllocator5EHint…`
   inline (`__ct__10CCallStackFUiPCcPCc` and `kUnknownType__10CCallStack` are in retail's
   undefined list and were absent from ours).
   Out-lining an instantiation that mwcceppc would put in a trailing pool is the established fix
   here — see `src/MetroidPrime/Player/CStaticInterference.cpp:12` and
   `docs/RUNNING_THE_DECOMP.md` "An emission-order wall: out-of-line template instantiations".

2. **`kPakVersionText`**. `src/MetroidPrime/PortGlobals.cpp:1203` already documents retail's
   `.rodata:0x803B0098` as *two* strings in one 0x58-byte object: the 75-character pak-version
   message, and at **+76** the six bytes `??(??)`. Read from `build/G2ME01/main.dol`:

   ```
   803b0098  "%s: Incompatible pak file version -- Current version is %x, you're using %x\0"
   803b00e4  "??(??)\0"                      <- what the reserve's CCallStack gets
   ```

   Retail's reserve materialises it with `addi r5,r5,76` on the `lis` that also feeds the sprintf.
   One named constant now serves both, so `.rodata` grows from 76 to 83 bytes and the
   `R_PPC_ADDR16_HA/LO` pair in `reserve` names our object rather than retail's global — objdiff
   pairs them either way.

3. **`EnsureWorldPakReady`'s dep-list loop** now indexes (`resources[i].GetId()`) instead of
   walking a pointer. Retail indexes with a byte offset: `li r7,0 … lwzx r6,r3,r7 …
   addi r7,r7,11`, and compares `cmpw` (signed), ours had `lwz r7,20(r1)` hoisted out of the loop
   plus `cmplw`. 90.89% → 94.37%.

## Measurements (all from `build/report.json`, `./tools/decomp_build.sh main/Kyoto/CPakFile`)

| | before | after |
|---|---|---|
| `main/Kyoto/CPakFile` fuzzy | 95.2652% | 96.43% |
| `main/Kyoto/CPakFile` matched_functions | **25 / 33** | **26 / 33** |
| `main/Kyoto/CPakFile` matched_code | 3896 / 7436 | 4128 / 7436 |
| `main/Kyoto/CPakFile` complete_code | 826268 (global) | 826268 (global, unchanged) |
| global `matched_functions` | 11788 / 28465 | **11789 / 28465** |
| global `matched_code` | 1731256 | 1731488 (+232) |
| global `complete_code` / `complete_units` | 826268 / 755 | 826268 / 755 (unchanged) |

Per function:

| function | before | after |
|---|---|---|
| `reserve__Q24rstl55vector<Q28CPakFile8SResInfo,…>Fi` | 67.03% | **100.00%** |
| `EnsureWorldPakReady__8CPakFileFv` | 90.89% | 94.37% |
| `InitialHeaderLoad__8CPakFileFv` | 99.72% | 99.72% (unchanged) |
| `LoadResourceTable__8CPakFileFR15CMemoryInStream` | 99.42% | 99.42% (unchanged) |
| `GetResInfo__8CPakFileCFUi` | 78.25% | 78.25% |
| `GetResInfoForLoadDirectionless` / `…PreferForward` | 88.59 / 89.41% | unchanged |
| `RebuildResourceLists` | 84.89% | 84.89% |

Judge: `./tools/goal_check.sh build/goal/item.json` → **PASS**
(`matched 11788 -> 11789  linked 5727 -> 5727`, `target rose: 25 -> 26 / 33`, no asm, no
judge-owned path touched; `gate.sh` green, which covers the DOL sha1 and all 86 RELs).

## The `reserve` regswap, and the one thing that fixed it

Written directly in `reserve`'s body the function sat at **99.74%**, and the residue was three
instructions, all one register: the byte count in `r26` where retail has `r27`
(`mulli` / `cmpwi` / `mr r3`). Everything else — frame, `stmw`, the CCallStack setup, the
`li r4,2 / li r5,1 / li r6,0` argument triple, the `__copy(dst+4, src+4, 7)` element loop, the
`CMemory::Free`, both stores — already matched byte for byte.

Measured this run, all on `reserve`, in the order tried:

| spelling | score |
|---|---|
| `if (size == 0) newData = nullptr; else newData = …` (baseline) | 99.74% |
| multiply operand order swapped (`sizeof * newSize`) | 99.74% |
| `newData = nullptr;` declared before `size` | 99.74% |
| `uint size`, `size != 0u` | 99.74% |
| ternary `size == 0 ? nullptr : …` | 99.74% |
| `const CCallStack cs` named, passed by reference | 99.74% |
| `static_cast<size_t>(size)` at the `Alloc` call | 99.74% |
| `size_t size` | 98.79% |
| `size > 0` | 92.67% |
| `if (size) newData = …` | 93.19% |
| `const size_t size` | 98.79% |
| `if (newSize != 0)` instead of testing the product | 89.66% |
| `CCallStack cs` hoisted above `size` | 66.72% |
| `CMemory::Free(oldData)` from a saved `mItems` | 96.78% |
| whole realloc inside the `else` | 86.28% |
| **`CPakFile::SResInfo* newData = alloc_resinfo(size);` (free inline helper) — 100.00%** | **100.00%** |

So: the allocation has to be behind a **separate `static inline` function**, not an `if`/`else` in
the body. That is also the shape `include/rstl/rmemory_allocator.hpp:20`'s `allocate2` has — the
`// TODO: this fixes a regswap in vector::reserve` on that helper is this regswap, and it is fixed
by writing the helper out, because `allocate2` itself cannot be used: it ends in the out-of-line
`allocate(int)`. Worth a lesson in `docs/RUNNING_THE_DECOMP.md`: **a `static inline` free function
is a register-allocation lever that an `if`/`else` cannot reach**, and it cost nothing here.

## What is left, and why

All seven remaining functions are **register allocation only** — every instruction is present, in
order, in the right encoding; only the register differs.

* **`GetResInfo` (78.25%), `GetResInfoForLoadDirectionless` (88.59%),
  `GetResInfoForLoadPreferForward` (89.41%)** are all one difference. Retail keeps `last` in a
  **callee-saved `r30`** across the `SResInfo::SResInfo` call (`stw r30,56(r1)` … `lwz r30,56(r1)`)
  and re-reads `first` from the frame spill into the `lower_bound` argument slot; we keep both in
  the frame and reload both. `first`/`last` are provably the same computation (`mResList.data() +
  11 * mBucketOffsets[b]`), and Prime 1's `src/Kyoto/CPakFile.cpp:181-246` has the same bucket-free
  `lower_bound` over the whole list, so the *logic* is not in question — only whether MWCC spends a
  callee-saved register on the range or rematerialises it from the spill.
* **`EnsureWorldPakReady` (94.37%)** is now down to **five instructions**: retail zeroes the
  vector's three words first and only then loads `mResTableCount` and compares it against the `r0`
  that already holds zero (`li r0,0; stw×3; lwz r30,76(r31); cmpw r30,r0`); we load first and
  compare against an immediate (`cmpwi r30,0`). It is one scheduling decision inside
  `rstl::vector<T>::vector(int)` (`include/rstl/vector.hpp:40`), which is shared — do not touch it
  for this.
* **`InitialHeaderLoad` (99.72%)** is `r28`/`r29` where retail has `r27`/`r28` for the name-loop
  counter and the `ReadInt32` temporary. Spellings tried this run: `uint` counter (99.45%),
  `while` loop (98.71%), `const SObjectTag` local (99.72%), the `push_back_unsafe` argument on one
  line (99.72%).
* **`LoadResourceTable` (99.42%)** is a three-way permutation in the first loop: ours
  temp=`r31`, counter=`r30`, id=`r29`; retail temp=`r30`, counter=`r29`, id=`r31`, and the second
  loop's two counters swap the other way. Spellings tried: `uint` counter (98.99%), a named
  `SResInfo` temporary (99.42%), `while` (99.42%), dep-list push before the resource push (72.84%),
  a hoisted `const int resCount` (96.28%).
* **`RebuildResourceLists` (84.89%)** is the only one with a structural difference, and it is a
  frame-layout one: retail keeps the `rstl::reserved_vector<uint,256>` at `r1+36` with `stmw r27`,
  we keep it at `r1+24` with `stmw r25`, and retail builds the `emptyInfo` at `r1+8` and `__copy`s
  it to `r1+20` before `resize`, while we construct it in place at `r1+8` and pass `r1+8` straight
  to `resize`. 712 bytes; a lane's worth of work on its own.

`GetResInfoForLoad*` and `RebuildResourceLists` are the cheapest real targets left.

## Not done, deliberately

`./tools/unit_fit.sh Kyoto/CPakFile.cpp` reports **18 functions in our object that retail's object
does not define (2128 bytes)** — the implicit instantiations (`reserve__Q24rstl37vector<Uc,…>`,
`__as__…`, the `~vector` family, `uninitialized_copy`/`destroy` template bodies, …). Until those
are out-lined the unit cannot flip, and `python3 tools/check_decl_order.py --unit Kyoto/CPakFile`
already reports "would break on a flip" **on the unmodified tree** (measured, not caused by this
change: the baseline run prints the same). Left alone here: it is 18 more out-lining edits plus the
emission-order work, and the item is a `progress` item.

Also noted and not touched: `docs/HANDOFF.md`'s state block is rewritten by the judge itself
(`goal_check.sh` runs `gate.sh` with `MP_GATE_DOCS_WRITE=1`), so the diff was reverted to keep the
change to `src/` alone.