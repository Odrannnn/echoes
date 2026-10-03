# carve-801c2cbc

**DONE - `MetroidPrime/Carve801C2CBC` is a new `Matching` DOL unit, 1 / 1 function at 100.00%.**
`./tools/goal_check.sh build/goal/item.json` exits 0 with every check `ok`:

```
goal_check: item carve-801c2cbc (match) target=MetroidPrime/Carve801C2CBC
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13559 -> 13560   linked 6607 -> 6608
  ok    check_symbol_names.py
  ok    All:  37.67% fuzzy, 31.10% matched, 13.97% linked (13560 / 28465 functions)
  ok    flip_test MetroidPrime/Carve801C2CBC.cpp: PASS, Object(Matching) in configure.py
goal_check: PASS carve-801c2cbc
```

`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`; all 86 RELs
`cmp`-equal to `orig/G2ME01/files/RelProd/`; `total_functions` still **28465** (this claim moves
one function out of `auto_03_801C2CBC_text` into a unit of its own and adds none);
`python3 tools/check_symbol_names.py` prints `checked 607 units; 0 declared names are missing`;
`tools/link_gap.py --rebuild` reports `ok: 278 MISSING symbol(s), all accounted for`.

## The carve - four files, each in address order

| file | line | content |
| --- | --- | --- |
| `configure.py` | 776 | `Object(Matching, "MetroidPrime/Carve801C2CBC.cpp")`, one line, between `Carve801C2BA8.cpp` and `Carve801C2D74.cpp` |
| `config/G2ME01/splits.txt` | 1247-1248 | `MetroidPrime/Carve801C2CBC.cpp:` / `.text start:0x801C2CBC end:0x801C2D74` |
| `files.cmake` | 782-791 | the entry, with the reason its host branch is empty |
| `src/MetroidPrime/Carve801C2CBC.cpp` | new, 1-180 | the claim, its header comment and the one body |

**Claimed range: `.text 0x801C2CBC..0x801C2D74` = 0xB8 = 184 bytes, 1 function,
`fn_801C2CBC` (0x801C2CBC, 46 instructions), `symbols.txt:7321`, and nothing else.** That range
was the *whole* of `auto_03_801C2CBC_text`, so unlike the previous carve in this hole this claim
**removes** the auto object instead of splitting it - `build/report.json` no longer lists
`main/auto_03_801C2CBC_text` and lists `main/MetroidPrime/Carve801C2CBC` with
`total_functions 1, matched_functions 1, complete_units 1`. Below the claim
`MetroidPrime/Carve801C2BA8.cpp` ends at 0x801C2CBC and above it `MetroidPrime/Carve801C2D74.cpp`
begins at 0x801C2D74, so the claim spans no unclaimed gap. The directory is retail's own, taken
from those two neighbours, as the other two carves in this hole did.
No `PortLinkStubs.cpp` duplicate existed for the symbol
(`grep -rn '801C2CBC' src/ include/` returns nothing but this new file).

## What the function is, and how it was identified

It is a **byte-shape twin of `fn_801C2BA8`**, which is already `Matching` in this tree at
`src/MetroidPrime/Carve801C2BA8.cpp:197-219`, and the comparison was made instruction by
instruction with a script over the two `.fn` blocks in the `.s` files - not assumed, and not
eyeballed. 46 instructions each; **exactly seven words differ**, and all seven are accounted for:

| idx | `fn_801C2BA8` | `fn_801C2CBC` | why |
| --- | --- | --- | --- |
| 11 | `801C2BD4 1C7E0018 mulli r3, r30, 0x18` | `801C2CE8 1C7E0044 mulli r3, r30, 0x44` | element size |
| 12 | `801C2BD8 4813AEE1 bl allocate` | `801C2CEC 4813ADCD bl allocate` | same callee, different displacement |
| 17 | `801C2BEC 1C000018 mulli r0, r0, 0x18` | `801C2D00 1C000044 mulli r0, r0, 0x44` | element size |
| 26 | `801C2C10 48000051 bl fn_801C2C60` | `801C2D24 48000051 bl fn_801C2D74` | the copy callee |
| 29 | `801C2C1C 1C000018 mulli r0, r0, 0x18` | `801C2D30 1C000044 mulli r0, r0, 0x44` | element size |
| 33 | `801C2C2C 38840018 addi r4, r4, 0x18` | `801C2D3C 38840044 addi r4, r4, 0x44` | element size |
| 36 | `801C2C38 4810B751 bl Free__7CMemoryFPCv` | `801C2D4C 4810B63D bl Free__7CMemoryFPCv` | same callee, different displacement |

The `bl fn_801C2D74` has the **same** displacement `0x51` as the twin's `bl fn_801C2C60`: retail
emitted the out-of-line copy 0x50 after the caller in both cases, so the word is byte-identical and
only the symbol differs.

**So it is the same `rstl::vector<T,Alloc>::reserve`** (`include/rstl/vector.hpp:166-179`, read
out of the twin's own unit `src/MetroidPrime/Player/CScanDisplay.cpp`), one element type over:

* the **stride is `0x44`**, and `NESTED_CHECK_SIZEOF(CRagDoll, CRagDollParticle, 0x44)` is
  `include/MetroidPrime/CRagDoll.hpp:197`;
* the **copy callee settles it independently**: `fn_801C2D74` is
  `rstl::uninitialized_copy<pointer_iterator<CRagDoll::CRagDollParticle,...>,
  CRagDoll::CRagDollParticle*>` (`src/MetroidPrime/Carve801C2D74.cpp`, `Matching`), its own loop
  steps `addi ..,0x44`, and it is **retail's only caller on this side** -
  `grep -rn 'bl fn_801C2D74' build/G2ME01/asm/` returns exactly this one instruction;
* the block read is the same three words at the same offsets as the twin's: `+0x4` count,
  `+0x8` capacity (the test at 0x801C2CE0 is **signed** `cmpw` against it), `+0xc` base pointer;
* the `destroy(mItems, mItems + mCount)` walk at 0x801C2D28-0x801C2D48 **has an empty body**,
  which is retail's: `destroy_impl` (`construct.hpp:101-109`) keeps the loop for a class that is
  not trivially destructible, and `CRagDoll::CRagDollParticle` is a struct of scalars with no
  destructor to call. Nothing was dropped to get the bytes.

## Independent verification against the disc

`tools/carve_diff.sh` reads its "retail" side from `build/G2ME01/main.elf`, which **is our own
link output** (recorded in `build/goal/notes/carve-80233a90.md`), so after a flip it proves
nothing on its own. The byte-exactness was therefore measured against **`orig/G2ME01/sys/main.dol`**,
raw bytes with `.text` file offset `vaddr - 0x80003200`:

```
disc 184 B, linked-dol 184 B, object .text 184 B
linked DOL == disc            : True
object .text  == disc         : 43 of 46 words byte-exact
  +0x801c2cec disc 4813adcd ours 48000001     <- bl allocate, relocation not yet filled
  +0x801c2d24 disc 48000051 ours 48000001     <- bl fn_801C2D74
  +0x801c2d4c disc 4810b63d ours 48000001     <- bl Free__7CMemoryFPCv
```

All three differences are unresolved `bl` relocations in the *unlinked* object; mwldeppc fills them
from the real addresses, which is why the 184 bytes of the linked `main.dol` are identical to the
disc's. `tools/flip_test.sh` - which links - agrees, and it is the acceptance test.

```
python3 tools/check_decl_order.py --unit Carve801C2CBC
  ok: 1 unit(s) checked, none emits its functions out of retail order

./tools/unit_fit.sh MetroidPrime/Carve801C2CBC.cpp
   .text  claimed 184  ours 184  retail 184  fits
   no extra functions: our object defines only what the retail unit object does

./tools/flip_test.sh MetroidPrime/Carve801C2CBC.cpp
  PASS  -> kept as Matching     kept: 1/1  failed: 0  skipped: 0
```

## Three decisions, and the measurements behind them

**1. The unit is a `.cpp` with `extern "C"`, not a `.c`.** `extern "C"` keeps the `fn_801C2CBC`
symbol verbatim (a C++ definition without it mangles to `_Z<len>fn_801C2CBCv` and objdiff would
pair nothing). The language could not be C because the two by-value iterator arguments are what
the bytes are made of: measured on the twin (`Carve801C2BA8.cpp:128-137`), mwcceppc's C mode
interleaves the two caller-built argument slots and common-subexpresses the two reads of the base
pointer, losing an instruction against retail. **The converting constructor on the one-word
iterator class is load-bearing and is the reason the object matches**: it is what makes mwcceppc
build the temporary at `r1+0x14` / `r1+0xc` and then copy it into the 8-byte argument slot,
producing the two `stw`s of one pointer retail has per slot. This is the same arrangement
`Carve801C2BA8.cpp` and `Carve801C2D74.cpp` use, and both record it.

**2. The callee is declared `extern "C"` with *our* iterator class, not with
`rstl::pointer_iterator`.** `extern "C"` means no template arguments appear in the symbol, so the
declaration mangles to retail's own `fn_801C2D74` and resolves against
`MetroidPrime/Carve801C2D74.cpp`; what matters at the call site is only that the two iterator
arguments are classes passed by value, which is what produces retail's two contiguous 8-byte
slots at `0x10/0x14` and `0x8/0xc` with `end` written into the low one first. `rstl`'s own header
is not included: `include/rstl/pointer_iterator.hpp` includes `rstl/construct.hpp`, whose inline
`construct_impl` definition would risk being inlined here or outlined as a local weak copy that
retail's range does not define (`tools/unit_fit.sh` would say so).

**3. The element type is a local stand-in and only its size is reproduced.** The source says so in
the header comment, so the next reader does not mistake it for a claim about the layout:
`CRagDoll::CRagDollParticle` is `include/MetroidPrime/CRagDoll.hpp:44-77` with eleven members,
and **nothing in these 184 bytes reads a member** - the walk steps a pointer by `sizeof` and the
element's copy is retail's own `fn_801C2D74` in another unit. What is reproduced is the size
(`0x44`), because the four strides are `NESTED_CHECK_SIZEOF(CRagDoll, CRagDollParticle, 0x44)`
and the callee's mangled symbol spells `Q28CRagDoll16CRagDollParticle`.

**Host branch: empty by design**, inside `#ifdef __MWERKS__`, for the reason
`Carve801C2D74.cpp:74-79` and its `files.cmake` entry give. `fn_801C2D74` - this function's only
other callee besides the allocator and `Free` - is itself defined only in that file's DOL branch,
so a host body here would add one undefined symbol to the port's link for a function no host
source calls. The file is still listed in `files.cmake` because `tools/check_files_cmake.py`
requires every configure.py `Matching` object to be listed or excluded with a reason (it prints
`every configured DOL object is either in files.cmake or excluded with a reason`).

## Nothing left in this hole, and no wall

The other function the previous run's notes flagged, `fn_801C2AA4` (0x801C2AA4, 0xB8,
`symbols.txt:7317`), is still inside `auto_03_801C13F8_text` and is the same 46-instruction
`reserve` a third time - so this run's bodies are the third instance of one spelling that reached
100.00% on the first try. There is no `WALL:` line, nothing was blocked, and no `NEW:` item is
filed: both the spelling and the identification are already recorded above and in the two
neighbouring notes, so a queued item would only re-pay for what this file's header comment already
carries.

The tree is left as the four carve files plus the two doc files the judge's own
`check_docs_claims.py --write` rewrote (derived counts only - 13559 -> 13560, 6607 -> 6608, the
probe count 962 -> 963), which the driver discards.