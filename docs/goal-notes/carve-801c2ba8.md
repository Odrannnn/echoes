# carve-801c2ba8

**DONE - `MetroidPrime/Carve801C2BA8` is a new `Matching` DOL unit, 2 / 2 functions at 100.00%.**
`./tools/goal_check.sh build/goal/item.json` exits 0:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13534 -> 13536   linked 6582 -> 6584
  ok    check_symbol_names.py
  ok    All:  37.62% fuzzy, 31.06% matched, 13.93% linked (13536 / 28465 functions)
  ok    flip_test MetroidPrime/Carve801C2BA8.cpp: PASS, Object(Matching) in configure.py
goal_check: PASS carve-801c2ba8
```

`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, all 86 RELs
`cmp`-equal, `total_functions` still 28465 (the splits.txt edit moves two functions between units
and adds none), port link unchanged at **287 undefined, 0 duplicates**.

## The carve - four files, each in address order

| file | line | content |
| --- | --- | --- |
| `configure.py` | 769 | `Object(Matching, "MetroidPrime/Carve801C2BA8.cpp")`, one line, between `Carve801C13F4.c` and `Carve801C2D74.cpp` |
| `config/G2ME01/splits.txt` | 1238-1239 | `MetroidPrime/Carve801C2BA8.cpp:` / `.text start:0x801C2BA8 end:0x801C2CBC` |
| `files.cmake` | 770-778 | the entry, with the host-branch note |
| `src/MetroidPrime/Carve801C2BA8.cpp` | 1-219 | the claim, its header comment and the two bodies |

**Claimed range: `.text 0x801C2BA8..0x801C2CBC` = 0x114 = 276 bytes, 2 functions, and nothing
else** - `fn_801C2BA8` (0x801C2BA8, 0xB8, 46 instructions) and `fn_801C2C60` (0x801C2C60, 0x5C,
23 instructions), `symbols.txt:7319-7320`. Carving it split dtk's `auto_03_801C13F8_text` into
three units, which is what the claim required: `auto_03_801C13F8_text` (0x801C13F8..0x801C2BA8),
this one, `auto_03_801C2CBC_text` (0x801C2CBC..0x801C2D74, now 1 function) and then the
pre-existing `MetroidPrime/Carve801C2D74.cpp`. No `PortLinkStubs.cpp` duplicate existed for either
symbol (`grep -rn '801C2BA8\|801C2C60' src/ include/` returns nothing but this file).
`tools/unit_fit.sh MetroidPrime/Carve801C2BA8.cpp` prints `fits` and `no extra functions`;
`python3 tools/check_decl_order.py --unit Carve801C2BA8` passes.

## What the two functions are

Both are byte-shape twins of `Matching` functions in `src/MetroidPrime/Player/CScanDisplay.cpp`,
which is retail's own source for them (`rstl::vector<SScanHistoryWidgets, rmemory_allocator>`):

| function | twin | what it is |
| --- | --- | --- |
| `fn_801C2BA8` | `reserve__Q24rstl56vector<19SScanHistoryWidgets,Q24rstl17rmemory_allocator>Fi` (0x80116954, 0xB8, `Matching`) | `vector::reserve` - `include/rstl/vector.hpp:166-179` |
| `fn_801C2C60` | `uninitialized_copy<Q24rstl132pointer_iterator<19SScanHistoryWidgets,...>,P19SScanHistoryWidgets>` (0x80116A0C, 0x5C, `Matching`) | the out-of-line `rstl::uninitialized_copy` - `include/rstl/construct.hpp:116-127` |

The twin comparison was **measured**, not assumed, with
`build/binutils/powerpc-eabi-objdump -d --start-address=... --stop-address=...` on the two ranges
in `build/G2ME01/main.elf`: `fn_801C2BA8` and its twin are 46 instructions each and **exactly two
words differ** - the `bl` at 0x801C2BD8 and the `bl` at 0x801C2C38, which reach
`allocate__Q24rstl17rmemory_allocatorFi` and `Free__7CMemoryFPCv` from this copy and the same two
symbols from the twin's address, so only the displacement field differs. (The third `bl`, to the
copy helper, is byte-identical by coincidence: `fn_801C2C60` and the twin's `uninitialized_copy`
are each the *next* function after the caller, 0x50 away.) `fn_801C2C60` and its twin are 23
instructions each and **not one word differs**.

Three readings fall out of the bytes and are all in the file's header comment:
* the element is `SScanHistoryWidgets`, 0x18 = 24 bytes - `CHECK_SIZEOF` at
  `include/MetroidPrime/HUD/CScanHistory.hpp:49`, six pointers, which is the six `lwz`/`stw` pairs;
* the per-element `cmplwi r5,0 / beq` at 0x801C2C6C is the **primary** `rstl::construct_impl`
  (placement new) rather than the trivially-constructible specialisations of `construct.hpp`;
* the `destroy(mItems, mItems + mCount)` walk at 0x801C2C28-0x801C2C34 has **an empty body**, which
  is retail's: `destroy_impl` (`construct.hpp:101-109`) keeps the loop for a class that is not
  trivially destructible, and a struct of six pointers has no destructor to call.

## Independent verification, because `carve_diff.sh` compares the unit with itself

`tools/carve_diff.sh` reads its "retail" side from `build/G2ME01/main.elf`, which **is our own link
output** (this is already recorded in `build/goal/notes/carve-80233a90.md`), so after the flip it
proves nothing.  The byte-exactness below was measured while the range was still dtk's
`auto_03_801C13F8_text`, and then checked a second time against the **disc**:

```
$ python3 - ...   # raw bytes, .text file offset = vaddr - 0x80003200
disc 276 B, linked-dol 276 B, object .text 276 B
linked DOL == disc            : True
object .text  == disc         : False
  +0x801c2bd8 disc 4813aee1 ours 48000001     <- bl, relocation not yet filled
  +0x801c2c10 disc 48000051 ours 48000001     <- bl
  +0x801c2c38 disc 4810b751 ours 48000001     <- bl
```

66 of 69 words byte-exact in the object, the three `bl`s resolved by mwldeppc to retail's own
bytes, and all 276 bytes of the linked DOL identical to `orig/G2ME01/sys/main.dol`.
`tools/flip_test.sh` agrees, and it is the acceptance test.

## The unit is a `.cpp`, not the `.c` the seed asked for - measured

The seed's rule is "plain C so the `fn_` names do not mangle".  `extern "C"` does that
(`powerpc-eabi-nm` on the object: two `T fn_801C2<addr>`, two `U`, nothing mangled), but the
**language** could not be C: with `-lang=c` the same bodies give `fn_801C2BA8` as **45**
instructions against retail's 46, the two by-value iterator arguments land **interleaved**
(`begin` at `r1+0xc`, `end` at `r1+0x8`, the two 8-byte slots overlapping) instead of retail's two
contiguous slots at `0x10/0x14` and `0x8/0xc`, and C mode common-subexpressions the two reads of
`x0c_items` into one `lwz r6` where retail reads it twice (0x801C2BE4 and 0x801C2C00).  A converting
constructor on a one-word class is what produces retail's temporaries.  This is `Carve801FF5A0.cpp`
(46-78) and `Carve801C2D74.cpp` (94-102) - the same arrangement, measured the same way, and the
judge resolves the target either way (`goal_check` tries `.cpp`, `.cp`, `.c` in that order).  One
more measured difference, in the other direction: `*out = *in` in `fn_801C2C60` is byte-exact with
`-lang=c++` and **not** with `-lang=c` (9 of 23 instructions differ there - mwceppc reorders it
into two loads then two stores), so the same spelling that is right in C++ is wrong in C.

## Three small things that cost a minute each

* **`nullptr` does not exist in this compiler.**  mwcceppc 2.7 with `-lang=c++` reports
  `undefined identifier 'nullptr'`; the project gets it from `#define nullptr NULL` in
  `include/dolphin/types.h:69`, and a file that includes nothing (this one, `Carve801C2D74.cpp`)
  must write `0`.  A `nullptr` inside a **template** that is never instantiated compiles fine and
  proves nothing - that is why `Carve801C2D74.cpp:122` still says `nullptr`.
* **MWCC's C mode is C89 for declarations**: a declaration after any statement is
  `expression syntax error`, so every local in a `.c` body must be declared at the top of the
  function.  (In C++ it is fine, which is the only reason `fn_801C2BA8`'s `SCarveElem* const
  buffer` needs no hoisting.)
* **`python3 tools/check_decl_order.py --unit <unit>` matches its argument as a substring of the
  report's unit names**, so `--unit MetroidPrime/Carve801C2BA8.cpp` checks **nothing** and still
  prints `ok: 0 unit(s) checked`.  Use `--unit Carve801C2BA8`.

## What is left in this hole (not this item)

`fn_801C2CBC` (0x801C2CBC, 0xB8, `symbols.txt:7321`) begins exactly where this claim ends and is
**the same 46-instruction `reserve`** with a 0x44 stride - its `mulli` is `0x44`, it calls
`fn_801C2D74` (which `Carve801C2D74.cpp` already claims and which is retail's only caller) instead
of an out-of-line copy, and `build/G2ME01/config.json` now names it as the whole
`auto_03_801C2CBC_text` object.  `fn_801C2AA4` (0x801C2AA4, 0xB8, `symbols.txt:7317`) is the same
shape again, still inside `auto_03_801C13F8_text`.  Both are the same skeleton, so both are the
same work as this item.

NEW: carve-801c2cbc | match | MetroidPrime/Carve801C2CBC | 0x801C2CBC..0x801C2D74 is the whole
`auto_03_801C2CBC_text` unit and the same 46-instruction reserve as Carve801C2BA8.cpp, with a 0x44
stride and the already-`Matching` `fn_801C2D74` as its copy callee, so the same bodies should flip