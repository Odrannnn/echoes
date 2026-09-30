# progress-cmetroidareacollider

`kind: match`, `target: WorldFormat/CCollisionPrimitiveData`. The unit did not exist; this item
**carved it, wrote the function, and flipped it**. `flip_test` **PASS**, and the whole gate is green.

## Result, measured

`build/report.json`, unit `main/WorldFormat/CCollisionPrimitiveData` — new, **1 / 1 function at
100.00%**, `fuzzy 100.0`, `matched_code 228 / 228`, `metadata.complete: true` (objdiff's word for
Matching and really in the link). `total_functions` is still **28465** after the `splits.txt` edit.

Whole-build, from `./tools/decomp_build.sh`:

```
before:  All: 30.83% fuzzy, 23.10% matched, 11.74% linked (10011 / 28465 functions)
after:   All: 30.83% fuzzy, 23.11% matched, 11.74% linked (10012 / 28465 functions)
```

`tools/gate.sh` (with `MP_GATE_DOCS_WRITE=1`, the judge's mode) prints **`GATE PASS d0b6921+8
changed`**. Its per-function diff line is worth quoting in full, because it is the check that would
have caught a bad carve:

```
per-function diff  SPLIT   main/auto_03_80255128_text: 23 function(s) moved into
                   main/WorldFormat/CCollisionPrimitiveData, main/auto_03_80257AF8_text
                   (exact count match - a split, not a loss)
```

Gates, all on the final tree:

```
sha1sum build/G2ME01/main.dol   6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
tools/decomp_build.sh           All: line rose, did not fall
tools/flip_test.sh WorldFormat/CCollisionPrimitiveData.cpp   PASS
tools/probe_sources.sh          750 files, 0 failed, 0 errors; LINKED (250 undefined, 0 duplicates)
python3 tools/check_symbol_names.py   checked 504 units; 0 declared names are missing
python3 tools/check_decl_order.py     958 units checked, 31 permuted, all 31 accounted for
python3 tools/check_files_cmake.py    every configured DOL object is in files.cmake or excluded
```

All 86 RELs are `cmp`-equal (gate step `hashes vs config.yml: ok`). No `asm` was added. The diff
touches `src/`, `include/`, `configure.py`, `config/G2ME01/splits.txt` and `files.cmake` — the four
files a carve is, plus the one header and the one sibling source the body needs. `tools/`,
`docs/research/port_link_baseline.txt` and `build/goal/` are untouched. Not committed.

## The carve: what was claimed and why

`fn_80257A14` sits in an **unclaimed gap**, `0x80255128..0x802591B4`, which dtk hands to
`auto_03_80255128_text` (110 functions, 16524 bytes). The claim is a **sub-range of that auto
unit**, not a cut out of a named neighbour:

```
WorldFormat/CCollisionPrimitiveData.cpp:
	.text       start:0x80257A14 end:0x80257AF8
```

0xE4 = 228 bytes, one function, both boundaries retail function boundaries, neither another unit's.
`configure.py` declares it `Object(Matching, ...)` from the start, so `flip_test` verified it in
place. Claiming only this function leaves `fn_80257540` (the `GetTriangleVertexIndices` sibling
right below it) and the two constructors in the auto unit where they were.

## What the function is

`CCollisionPrimitiveData::GetTriangle(ushort)`, retail `fn_80257A14`. It is the helper
`progress-prime1-cmetroidareacollider` identified: every one of `CMetroidAreaCollider`'s twelve
leaf-walking collision queries calls it at the top of its triangle loop, and
`CMetroidAreaCollider.o` carries `U fn_80257A14` and `U fn_80257540` as undefined symbols, so
nothing could be written until it existed.

Prime 1's counterpart is `CAreaOctTree::GetMasterListTriangle`
(`prime-ref/src/WorldFormat/CAreaOctTree.cpp:161`). Echoes folds it into the base class and
**changes the third-vertex rule**: Prime 1 searches `edge1.GetVertIndex1()` against both of
`edge0`'s indices and falls back to `GetVertIndex2()`, whereas retail branches on the material flag
and takes `edge1`'s low half when the flag is set and its high half when it is clear — the same
choice that decides whether `edge0` is read `(2,0)` or `(0,2)`:

```
80257a8c  lhz r6,2(r9)     flag set:  edge0.(idx2, idx1), edge1.idx1
80257abc  lhz r6,0(r9)     flag clear: edge0.(idx1, idx2), edge1.idx2
```

### The winding flag is bit 24 of the material's low word, and the material is a `u64`

`mMaterials` is `const u64*` here, so the test is a 64-bit AND that mwcceppc splits across two
words, and both halves are visible in retail:

```
80257a34  lis    r5,256          0x01000000
80257a60  and    r0,r7,r6        material.lo & 0    <- low half of the 64-bit test, r6 == 0
80257a68  and    r5,r8,r5        material.hi & 0x01000000
80257a78  or.    r0,r5,r0
```

So the mask is `0x1000000` on the **low** word, and the `and r0,r7,r6` with `r6 == 0` is not a
separate expression. **Prime 1's `material & 0x2000000` does not compile to these bytes** — a
`u32` mask in a `u64` expression puts the constant in the wrong half and shifts every subsequent
instruction. Measured: `1ULL << 24` and `0x1000000` both give the right half; `0x2000000` and
`1ULL << 56` do not.

## The three things that cost the time, all measured

**1. `CCollisionSurface` is 48 bytes, so its by-value return is a hidden pointer in r3.** Spelled
as `extern "C" CCollisionSurface fn_800E88A8(...)` and returned by value, the object came out
**240 bytes against a 228-byte claim** — three extra instructions, `stw r31,12(r1)` /
`mr r31,r3` / `lwz r31,12(r1)`, the compiler allocating a home for the returned temporary even
though retail never writes r3 between the prologue and the `bl`. Writing the hidden pointer out as
an explicit first parameter, `fn_800E88A8(CCollisionSurface* out, ...)`, puts it in r3 too — the
same call — and the object is **228 bytes exactly**. Same for `fn_80257A14` itself. **A struct
larger than a register pair is returned through a pointer the caller passes; spelling that pointer
out is the difference between 240 and 228 bytes here.**

**2. `rlwinm rX,rY,2,14,29` is `ushort * 4` and plain `slwi` is not.** The last 8 bytes were two
instructions where ours emitted `slwi r10,r0,2` (`540a103a`) and retail emitted
`rlwinm r10,r0,2,14,29` (`540a13ba`) — the same shift with a 16-bit field mask. These are
**different, not a register-choice wobble**: the mask is the compiler narrowing the index to 16
bits, and it only does that when the source says so. Thirteen spellings were measured, and every
one that loaded the index straight from the array gave `slwi`:

| spelling | instruction |
|---|---|
| `mEdges[indices[start]]`, `const ushort`, `const uint`, `static_cast<uint>`, `* sizeof(CCollisionEdge)`, byte-offset pointer arithmetic, a `ushort*` view of the edge array | `slwi` `10 3a` |
| `ushort o = i * 4;` then `edges + o` | `rlwinm` `14 3a` (MB=16, one step off) |
| `const uint e = idx[i];` then `base[e]` | `slwi` |
| **`const uint e = idx[i];` then `base[static_cast<ushort>(e)]`** | **`rlwinm` `13 ba`** ✅ |

The winner is a `uint` local narrowed back to `ushort` **at the subscript**: the load widens to 32
bits, and the `static_cast` puts the 16-bit knowledge back where the multiply happens. Widening
`ushort -> uint` alone is not enough and narrowing at the load is not enough; both, in that order,
is what the compiler needs. **Isolated by compiling eight two-line functions and reading the
encodings** — `tools/try_batch.py` could not be used (it requires a return type on the definition,
and an `extern "C" void` definition has none), so the sweep was a small local script over
`mwcceppc` + `objdump`.

**3. The two `bl`s are the only remaining bytescmp differences and they are not differences.**
`bytescmp.py` reports `ours 48000001 | retail 4be90df5` at `+A0` and `+D0`: those are the
unresolved relocations in our object against the addresses in the linked ELF. They resolve at link
time and the DOL sha1 proves it. **`bytescmp.py`'s 2 differing instructions on a 57-instruction
function means the object is byte-identical.**

## The host build needed `fn_800E88A8`, and the fix is in `CCollisionSurface.cpp`

`fn_800E88A8` is retail 0x800E88A8, a **strong `T` in `MetroidPrime/CDecalManager.o`**, not a
COMDAT copy — retail's four-argument `CCollisionSurface` constructor is genuinely out of line, so
it has to be called by that name. The DOL has it in the link already; the host did not, because
the header's constructor is inline and mangles to a C++ name nothing references under the retail
one. Listing the new unit in `files.cmake` therefore grew the port's undefined count **250 -> 251**
and `link_check.sh --strict` failed the item on it.

The fix is 15 lines in `src/WorldFormat/CCollisionSurface.cpp` (already in `files.cmake`), under
`#ifdef TARGET_PC`: the same constructor, under the name the caller uses. It is the real object,
not a placeholder, and it is host-only so the DOL build never sees it and this unit's bytes are
untouched. Back to **250 undefined, 0 duplicates**. **The guard direction is `#ifdef TARGET_PC`,
not `#ifndef`**: the port defines `TARGET_PC` (`tools/probe_sources.sh:49`) and the DOL does not, so
the first attempt at `#ifndef TARGET_PC` compiled the definition *out* of the build that needed it
and the gap stayed at 251.

## Two header notes, both load-bearing

`include/WorldFormat/CCollisionPrimitiveData.hpp` gains an `extern "C"` declaration of
`fn_80257A14` and a matching `friend`. The friend is needed because the definition reads
`mEdges` / `mSurfaceIndices` / `mMaterials` / `mSurfaceMaterials` / `mVertices`, which are
`protected`. The friend declaration must be spelled **without a `::`** — mwcceppc rejects
`friend CCollisionSurface ::fn_80257A14(...)` with *"undefined identifier 'fn_80257A14'"* and
aborts every translation unit that includes the header (38 of them, one error each).

`GetTriangle(ushort)` is left declared in the header as the class's own interface; the definition
in this unit is the C-linkage one the linker needs. A C++ member-function spelling would mangle to
`GetTriangle__23CCollisionPrimitiveDataFUs`, which is not the name
`CMetroidAreaCollider.o`'s `U fn_80257A14` resolves against, and nothing else defines it.

## What this unblocks, and what it does not

`CMetroidAreaCollider`'s twelve leaf-walking functions can now be written: they all call
`fn_80257A14`, and it links. That is `docs/goal-notes/progress-prime1-cmetroidareacollider.md`'s
stated blocker, gone.

**Still missing, and not in this item's scope:** `fn_80257540` (0x80257540, 0xA4) is the same
shape writing three `ushort` vertex indices instead of a `CCollisionSurface` — retail's
`GetTriangleVertexIndices`, and `CMetroidAreaCollider.o` lists it as undefined too. Its body is
read straight off the disassembly in this session and the shape is the one probe `w5` above
reproduces, so it is a short follow-up. `CCollisionPrimitiveData`'s own constructors and destructor
(0x80257AF8..0x80257CB8) are also still in the auto unit; the destructor maintains a refcount
table at `0x80410E24` and the two `ct`s allocate cache ids through `fn_80257498`, which is a
separate piece of work.

## New queue items

None filed. The remaining work in this area (`fn_80257540`, the constructors/destructor) is a
second function and a third, not a unit that can reach 100% on its own, and a `NEW:` line costs a
lane about an hour, so the notes above are where the next run should start rather than a queue
entry. The two code-shape rules that would have saved most of this session — the hidden return
pointer and the `uint`-then-`static_cast<ushort>` index — are in the notes and in the source
comments, which is where a codegen rule belongs.

---

# Second run, 2026-09-30: the previous run's work was never committed and was reset away

`git reflog` on `goal/lane-2` shows the previous run ended on `d0b6921` and the driver then did
`branch: Reset to goal/decomp`, and the reflog contains no commit touching
`src/WorldFormat/CCollisionPrimitiveData.cpp` on any branch - the change was uncommitted and went
with `git reset --hard` + `git clean`. Everything above had to be redone from the notes. The notes
were worth their weight: the carve, the function, the three code-shape rules and the
`fn_800E88A8` fix all reproduced on the first or second attempt.

**One correction to the note above, and it cost most of this run.** The out pointer has to be the
*first* parameter of `fn_80257A14` as well as of `fn_800E88A8`, not just of the callee. Spelled
`fn_80257A14(self, out, index)` the object comes out with `self` in r3 and r4 unused for it, and
emits an `mr r3,r4` in each branch: **240 bytes, and only 236 of them the right ones**. Retail's
r4 is `this` and r5 is the index, so `(out, self, index)` is the only spelling consistent with the
disassembly.

## Result, measured

`build/report.json`, unit `main/WorldFormat/CCollisionPrimitiveData` - new, **1 / 1 function at
100.00%**, `matched_code 228 / 228`, `complete_code 228`, `metadata.complete: true`. `total_functions`
is still **28465** after the `splits.txt` edit.

```
before:  All: 30.97% fuzzy, 23.23% matched, 11.76% linked (10041 / 28465 functions)
after:   All: 30.97% fuzzy, 23.23% matched, 11.76% linked (10042 / 28465 functions)
```

Gates, all on the final tree:

```
sha1sum build/G2ME01/main.dol            6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
tools/decomp_build.sh -r WorldFormat/CCollisionPrimitiveData
                                         main/WorldFormat/CCollisionPrimitiveData: 100.00% fuzzy,
                                         100.00% matched (1 / 1 functions)
tools/flip_test.sh WorldFormat/CCollisionPrimitiveData.cpp
                                         PASS  -> kept as Matching; kept: 1 / 1  failed: 0
tools/unit_fit.sh WorldFormat/CCollisionPrimitiveData.cpp
                                         .text claimed 228 ours 228 retail 228 fits
                                         no extra functions
tools/probe_sources.sh                   750 files, 0 failed, 0 errors; LINKED (250 undefined,
                                         0 duplicates)
tools/link_check.sh --strict             STRICT PASS - 250 undefined against a baseline of 250
                                         (no growth), 0 duplicate(s), 0 compile error(s)
python3 tools/check_symbol_names.py      checked 504 units; 0 declared names are missing
python3 tools/check_decl_order.py --unit WorldFormat/CCollisionPrimitiveData
                                         ok: 1 unit checked, none emits its functions out of
                                         retail order
python3 tools/check_files_cmake.py       every configured DOL object is in files.cmake or excluded
all 86 RELs                              86/86 sha1 match against config/G2ME01/config.yml
```

No `asm` added. The diff touches `src/`, `include/`, `configure.py`, `config/G2ME01/splits.txt`
and `files.cmake` - the four files a carve is, plus the one header and the one sibling source the
body needs. `tools/`, `docs/research/port_link_baseline.txt` and `build/goal/` are untouched. Not
committed.

## The fourth rule, and it is the expensive one: `self` is a `const CCollisionPrimitiveData*`

This is new, it is not in the previous run's notes, and it is the whole of the last four
instructions. **`CCollisionPrimitiveData* self` and `const CCollisionPrimitiveData* self` compile the
same reads but differ in 79 bytes of the 228** - same 57 instructions, same registers, same
everything except the *position* of three of them:

| | want (retail) | `CCollisionPrimitiveData*` | `const CCollisionPrimitiveData*` |
|---|---|---|---|
| `lwz r6,0x1c(r4)` (mSurfaceMaterials) | 3rd | 6th | 3rd |
| `lis r5,0x100` (winding mask) | 8th | 5th | 8th |
| `lwz r11,0x20(r4)` (mEdges) | 21st | 13th | 21st |

Everything else lines up instruction for instruction, including `clrlwi r0,r5,16` and
`mulli r8,r0,3`. With the non-const pointer the scheduler hoists those three loads early; with the
const pointer it does not, and the object is byte-exact.

**What this is not:** it is not reachable by writing the body differently. 78 permutations were
built and measured - every ordering of {narrowing declaration, `start`, the two edge-index reads, the
edge references, the material read}, each with the narrowing written three ways (`ushort` param,
`int` param + `static_cast<ushort>`, explicit `const ushort i`), each with the if/else and
early-return forms, with references and with pointers, and with the flag as `0x1000000`,
`1ULL << 24` and a named `bool`. **All 78 land on exactly 79 differing bytes**; three of them (the
`const CCollisionPrimitiveData*` ones) land on 0. The same three spellings with a non-const `self`
land on 79. So the one-word difference in the parameter's const-ness is the lever, and nothing in
the body is.

Two measurement traps on the way there, both worth the note:

- **`tools/carve_diff.sh` compares against `build/G2ME01/main.elf`, which is *our* build.** Once the
  new unit is in `configure.py` and linked, the ELF at 0x80257A14 is our object, so carve_diff
  reports "4 differing instructions" against our own bytes. It read as a near miss. Use
  `tools/bytescmp.py`, which reads `orig/G2ME01/sys/main.dol`, or diff the two `.text` sections.
- **A positional instruction diff saturates.** `bytescmp.py` compares index by index, so one
  misordered load reads as 22 differences out of 57 and every variant looks equally hopeless. The
  metric that ranked the 78 candidates is the edit distance of the two *instruction streams*
  (`diff` on `mnemonic operands`, or `cmp` on the raw `.text`) - that is what showed 4 misplacements
  instead of 22, and it is what found the const pointer.

`build/G2ME01/obj/WorldFormat/CCollisionPrimitiveData.o` is **not** the previous run's object: that
directory is objdiff's *original* objects, extracted from the DOL, so it is retail's bytes and looks
like a recovered answer. It is a dead end; the real previous-run artifact did not survive.

## The carve: what was claimed

Identical to the previous run - a sub-range of dtk's `main/auto_03_80255128_text`:

```
WorldFormat/CCollisionPrimitiveData.cpp:
	.text       start:0x80257A14 end:0x80257AF8
```

0xE4 = 228 bytes, one function, both boundaries retail function boundaries. `fn_80257540` and the
two constructors stay in the auto unit.

## Two things the notes above did not mention, both measured

**`unit_fit.sh` reports an unclaimed 5-byte `.sbss`** (two anonymous `b @84` / `b @86` statics that
mwcceppc emits for this function). It is not a problem and not specific to this unit:
`MetroidPrime/CCredits.o`, also `Matching`, emits an unclaimed 8-byte `.sbss` plus a `.data` full of
`d @NNNN`. `flip_test.sh` passes and the DOL sha1 is retail's, so those bytes land in zeroed sbss
exactly as retail has them.

**The host-side `fn_800E88A8` is written with the class's own constructor**
(`*out = CCollisionSurface(*v0, *v1, *v2, flags);`) rather than by assigning `mVertices` directly,
because those members are private and the port's own ctor is the only public way in. It is
host-only, the DOL never sees it, and the guard direction is `#ifdef TARGET_PC` as the notes above
say - the port defines `TARGET_PC` (`tools/probe_sources.sh:49`) and the DOL does not.

## What is still missing, unchanged from the previous run

`fn_80257540` (0x80257540, 0xA4), retail's `GetTriangleVertexIndices`, is the same shape writing
three `ushort` vertex indices instead of a `CCollisionSurface`, and `CMetroidAreaCollider.o` lists
it as undefined too - so **the twelve leaf-walking collision queries are still not writable**, on
that symbol rather than on this one. Its disassembly and shape are in the notes above. The class's
own constructors and destructor (0x80257AF8..0x80257CB8) are also still in the auto unit.

## New queue items

None filed. `fn_80257A14` is now defined, so `fn_80257540` is the single remaining blocker for
`WorldFormat/CMetroidAreaCollider` and it is one function of the same shape; the notes above hold
its disassembly. That is a follow-up, not a queue entry.

Object bytes, `python3 tools/bytescmp.py build/G2ME01/src/WorldFormat/CCollisionPrimitiveData.o
fn_80257A14 80257A14 0xE4` (reads `orig/G2ME01/sys/main.dol`, not our ELF):

```
  +A0  ours 48000001  | retail 4be90df5  | bl      a0
  +D0  ours 48000001  | retail 4be90dc5  | bl      d0
2 differing instructions of 57 (228 bytes ours vs 228 retail)
```

Those two are the unresolved `R_PPC_REL24 fn_800E88A8` relocations against the address the linked
ELF gives the symbol; they resolve at link time and the DOL sha1 is retail's. 2 differences on a
57-instruction function is byte-identical.
