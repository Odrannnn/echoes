# carve-80255a0c — `WorldFormat/Carve80255A0C`, four functions, `Matching`

`kind: match`. Carved 0x80255A0C..0x80255B28 (0x11C = 284 bytes, 4 functions) out of the
unsourced dtk range `auto_03_80255128_text` and landed it as its own `Matching` unit.

## Result, measured

| | before | after |
| --- | --- | --- |
| `matched` | 12475 | **12479** (+4) |
| `linked` (unit `Matching` + has source) | 5863 | **5867** (+4) |
| `All:` | 35.25% fuzzy, 29.04% matched, 12.90% linked (12475 / 28465) | **35.25% fuzzy, 29.05% matched, 12.91% linked** (12479 / 28465) |

- `main/WorldFormat/Carve80255A0C`: **100.00% fuzzy, 100.00% matched, 4 / 4 functions**,
  `total_code 284`, `complete_code 284`.
- `./tools/flip_test.sh WorldFormat/Carve80255A0C.c` → `PASS -> kept as Matching`,
  `kept: 1 / 1   failed: 0   skipped: 0`, and `configure.py:430` reads
  `Object(Matching, "WorldFormat/Carve80255A0C.c",)`.
- `sha1sum build/G2ME01/main.dol` → `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, unchanged.
- `tools/unit_fit.sh WorldFormat/Carve80255A0C.c` → `.text claimed 284 ours 284 retail 284
  fits`, `no extra functions: our object defines only what the retail unit object does`.
- `python3 tools/check_decl_order.py --unit WorldFormat/Carve80255A0C.c` → ok.
- `python3 tools/check_symbol_names.py` → `checked 525 units; 0 declared names are missing`.
- `python3 tools/check_raw_offsets.py` → `ok: 167 raw-offset site(s) in 71 file(s)` — unchanged
  from the head (167 / 71), because the receiver is modelled as a four-word struct rather than
  reached through `this + 0x4`.
- `tools/carve_diff.sh 0x80255A0C 0x11C build/G2ME01/src/WorldFormat/Carve80255A0C.o` →
  `retail: 71 instructions, 284 bytes` / `ours: 71 instructions, 284 bytes`,
  `differing instructions: 6`, and all six are `bl` **relocations**
  (`+3 → fn_80255A2C`, `+12 → fn_80255A50`, `+26 → fn_80255AA4`, `+30/+59/+63 →
  Free__7CMemoryFPCv`).  `carve_diff` compares raw bytes, so it can never report byte-exactness
  for a function with a cross-reference; `flip_test.sh` is the acceptance test and it passes.
- `main/auto_03_80255128_text` went 87 → **23** functions (still 0 matched), and dtk split the
  tail into a new `main/auto_03_80255B28_text` of **60** (still 0 matched): 23 + 4 + 60 = 87, so
  no function was lost.  The gate says the same in its own words:
  `per-function diff  SPLIT   main/auto_03_80255128_text: 64 function(s) moved into
  main/WorldFormat/Carve80255A0C, main/auto_03_80255B28_text (exact count match - a split, not a
  loss)`.
- The range starts at 0x80255A0C, **not** at a unit boundary, and the nearest claimed range below
  it (`WorldFormat/CAreaRenderOctTree.cpp`, ending 0x80255128) is `NonMatching`, so the
  link-order cycle `RUNNING_THE_DECOMP.md` warns about does not arise — `dtk dol split` was
  never unhappy.
- `./tools/goal_check.sh build/goal/item.json` → see "The judge's two runs" below.

## The four files (a carve is four, and all four are here)

| file | change |
| --- | --- |
| `src/WorldFormat/Carve80255A0C.c` | new, 145 lines, plain C |
| `configure.py:430` | `Object(Matching, "WorldFormat/Carve80255A0C.c"),` between `CAreaRenderOctTree.cpp` and `CCollisionPrimitiveData.cpp`, in address order |
| `config/G2ME01/splits.txt:1579-1580` | `WorldFormat/Carve80255A0C.c:` / `.text start:0x80255A0C end:0x80255B28`, between the same two neighbours |
| `files.cmake:557` | `src/WorldFormat/Carve80255A0C.c`, between `Carve802476D8.c` (0x802476D8) and `Carve8026040C.c` (0x8026040C), i.e. in the one address-ordered carve run |

One **fifth** file, and it is not optional: `src/Kyoto/Alloc/PortMwccNew.cpp` gains

```cpp
extern "C" void Free__7CMemoryFPCv(const void* ptr) { CMemory::Free(ptr); }
```

See "check_files_cmake.py is what forces this" below.

## What the four functions are

They are **one free chain**, each step calling the next, and that is what makes the unit worth
having — the whole chain becomes `Matching` in one carve:

```
fn_80255A0C  r3 -> fn_80255A2C                              8 insns   __sys_free-shaped
fn_80255A2C  r3, li r4,-1 -> fn_80255A50                    9 insns   rstl::destroy_impl-shaped
fn_80255A50  delete-by-destructor: teardown(self,-1), then
             CMemory::Free(self) when (short)flag > 0       21 insns
fn_80255AA4  +0x4 count, +0xC block, stride 0x14, walk with
             an EMPTY body, Free(block), Free(self) if >0   33 insns
```

`+0x4` / `+0xC` / stride `0x14` is `rstl::vector<T>`'s own layout once the empty allocator
takes the word at `+0` (`include/rstl/vector.hpp`: `mAllocator`, `mCount`, `mCapacity`,
`mItems`), so `fn_80255AA4` is a destructor whose only member teardown is that vector, and
`fn_80255A50` is the `operator delete` that calls it.  The class is not named in retail and the
source does not guess: `SCarve80255A0COwner` is only the four words the bytes read.

The `bl Free__7CMemoryFPCv` target is real: `symbols.txt:12992` puts
`Free__7CMemoryFPCv = .text:0x802CE388, size:0x64`, and **no unit claims 0x802CE388**, so dtk's
own `auto_*` object supplies those bytes in the DOL link and the relocation resolves to retail's
address.  Verified by the unchanged DOL sha1 above.

## The twins, and how close they are

`item.json` named four byte-shape twins.  Measured by disassembling both sides of `main.elf`
(`powerpc-eabi-objdump -d`) and comparing raw instruction bytes:

| this copy | twin | twin's unit | identical bytes |
| --- | --- | --- | --- |
| `fn_80255A0C` (0x20) | `__sys_free` (0x80008A28) | `MetroidPrime/main.cpp` | 7 / 8 (the `bl`) |
| `fn_80255A2C` (0x24) | `destroy_impl<11CTweakValue>__4rstlFP11CTweakValue` (0x80006850) | `MetroidPrime/main.cpp` | 9 / 9 |
| `fn_80255A50` (0x54) | `__dt__19CStaticInterferenceFv` (0x80009460) | `MetroidPrime/main.cpp` | 20 / 21 (the `bl`) |
| `fn_80255AA4` (0x84) | `__dt__Q24rstl70vector<Q219CPFPointSearchState10SPointData,…>Fv` (0x8014009C) | `MetroidPrime/PathFinding/CPathFindArea.cpp` | 31 / 33 (the two `bl`) |

Every difference is an `R_PPC_REL24` displacement.  `MetroidPrime/main.cpp` claims
0x800053B8..0x80009880 (`splits.txt:35`), which covers three of the four.  The fourth is
`CPathFindArea.cpp`'s own weak `rstl::vector<CPFPointSearchState::SPointData>` instantiation —
and `CPathFindArea.cpp` also contains the twin of the whole 33-instruction shape, which is where
the spelling below was read off.

## The one hard part: four dead stores

`fn_80255AA4` writes each of its two pointer values **twice** and reads neither back:

```
mulli r0,r0,0x14 ; stw r3,0x14(r1) ; mr r4,r3 ; add r0,r3,r0 ;
stw r3,0x8(r1) ; stw r0,0x10(r1) ; stw r0,0xc(r1)
```

Four `stw`s, no `lwz`.  In C++ those are the copies `rstl::destroy(begin(), end())` leaves
behind.  Reproducing them took nine measured spellings; the scores are instruction-level diffs
against retail's 33, so a next run can skip all of them:

| spelling | insns | differing |
| --- | --- | --- |
| two `volatile` locals for the copies, non-volatile loop bounds | 31 | 23 |
| four separate `volatile unsigned char*` locals, declared 3,0,2,1 | 37 | 26 |
| `volatile unsigned char* q[4]`, assigned `q[3],q[0],q[2],q[1]` | 29 | 27 |
| four addresses taken through an empty `static void touch(void*,void*)` | 46 | 45 |
| the same, one pointer per `touch1()` call | 48 | 47 |
| four addresses taken inside a never-taken `if (count == 0x7fffffff)` | 49 | 48 |
| a plain (non-`volatile`) four-pointer struct | — | all four stores deleted |
| a **counted** loop `for (i = 0; i != count; ++i)` | — | the loop is deleted outright |
| two `volatile struct SPair` written `s1.a, s0.a, s1.b, s0.b` | 33 | 6 |
| **one `volatile struct SQuad`, written `copies.d, copies.a, copies.c, copies.b`** | **33** | **2** (the `bl`s) |

A second spelling also reaches 2 differing: two `volatile struct SPair` written
`s0.b, s1.a, s0.a, s1.b`.  The one that landed is recorded because a single struct reads more
like what retail's source did.

Three things that cost the time, all now in the file's header comment:

- **`volatile` is what keeps the store; the *order* of the writes is what places it.**  mwcc
  keeps volatile-struct field stores in source order, so retail's `0x14, 0x8, 0x10, 0xc` is
  obtained by writing the fields as `d, a, c, b`.  Nothing about the values is non-obvious.
- **Four separate `volatile` pointers are not equivalent to one `volatile` struct.**  They are
  re-read at every use, so the loop grows four `lwz`s and mwcc picks r5 as the accumulator: 37
  instructions against retail's 33.  This is the one that looks like the answer and is not.
- **mwccceppc 2.7 does not inline an empty `static` helper away here**, so "make it
  address-taken by passing it to a call" is not a way to keep a dead store: the call survives and
  the frame doubles.

Also: the walk must be an **inequality between two pointers**, not a count.  Written as a counted
loop the compiler removes it, because nothing in the body can have an effect; retail keeps it.

## check_files_cmake.py is what forces the fifth file

`tools/check_files_cmake.py` requires every configured, on-disk unit to be either listed in
`files.cmake` or carry an entry in its own `EXCLUDED` table — and that table is in `tools/`,
which an agent may not edit.  So this carve **must** be in `files.cmake`, which means it must
compile and resolve in the **host** link too, and `tools/link_gap.py` fails on any newly
undefined symbol that is not already in `docs/research/port_link_gap_list.md`.

The carve's only external callee is `Free__7CMemoryFPCv`, which nothing in the port defined.
`src/Kyoto/Alloc/PortMwccNew.cpp` is the file whose stated purpose is exactly this — it already
defines `__nw__FUlPCcPCc` "because units written against retail's bytes call it by that mangled
name through `extern "C`"… on a host link that name binds to nothing, so this defines it" — so
the definition went there, and **no `docs/research/port_link_gap_list.md` edit was needed**.
Verified on the host: `/usr/bin/c++ … -c src/Kyoto/Alloc/PortMwccNew.cpp` is clean and `nm`
reports `T Free__7CMemoryFPCv`.

Generalisable: **a carve is not four files if it calls a retail-named symbol.**  The DOL half
resolves against dtk's own unclaimed objects; the port half does not, and the port half is what
fails the gate.

## A trap the matching build cannot see

`struct SCarve80255A0COwner` was first named in the **parameter list** of the prototype
(`void* fn_80255AA4(struct SCarve80255A0COwner*, short flag);`).  A `struct` named there is
scoped to that list, so the definition 20 lines later is a different type.  **mwcceppc only
warns**: `decomp_build.sh` was green, objdiff was 100.00% on all four, `flip_test.sh` was PASS
and the DOL sha1 held.  The host build is the only thing that rejects it:

```
src/WorldFormat/Carve80255A0C.c:104:7: error: conflicting types for 'fn_80255AA4'
```

Fixed by moving both struct definitions **above** the prototypes.  Worth remembering because the
carve's own acceptance test passes with the defect in place.

**A second one, in this file's own header.**  It cites
`build/G2ME01/asm/auto_03_80255128_text.s` lines 700-789 for the instructions - which is where
dtk put them *before* the claim, and which the claim itself then emptied (that file now holds
only the 23 functions below 0x80255A0C).  A comment pointing at a file the change itself moves
is stale the moment it lands, so the header now also gives the command that reads retail's bytes
out of `build/G2ME01/main.elf`, which nothing in this change can invalidate.

## The judge's two runs

1. **FAIL** on `gate.sh` only — `GATE FAIL: probe link-gap link-dups`, from
   `link_check: 1 compile error(s), linker_ran=0`.  Everything else was already `ok`:
   `counts: matched 12475 -> 12479   linked 5863 -> 5867`, `check_symbol_names.py`,
   `All: 35.25% fuzzy, 29.05% matched, 12.91% linked`, and
   `flip_test WorldFormat/Carve80255A0C.c: PASS, Object(Matching) in configure.py`.
   The one compile error was the struct-scoping trap above.
2. **PASS** after the fix — `goal_check: PASS carve-80255a0c`, exit 0, and the whole gate green
   (`GATE PASS  e8672095+7 changed`), including:

   - `hashes vs config.yml  ok`, and measured independently: **86/86 modules match
     `config/G2ME01/config.yml`**
   - `port probe  ok` → `probe: 757 files, 0 failed, 0 errors; link: LINKED (290 undefined,
     0 duplicates)` — the port's undefined count is **unchanged at 290**, and `Free__7CMemoryFPCv`
     is not among them
   - `port link gap  ok` → `ok: 285 MISSING symbol(s), all accounted for in
     port_link_gap_list.md` (measured over 750 objects, one more than before — this unit's)
   - `docs claims agree with the tree`, `raw offsets ok`, `decl order ok`, `files.cmake ok`

That failure is the useful half of this item's record: the unit's own acceptance test passed
while the change was broken, and only `tools/gate.sh`'s port half could see it.

## Left as a `NEW:` line

None.  Nothing found here is a new blocker with a target whose success would raise a count: the
port-side lesson above is a rule, and `check_files_cmake.py`'s behaviour is documented in this
file rather than queued.