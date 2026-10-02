# progress-twin-rel-sandworm - `module:Sandworm`, 11 -> 17 matched functions

## What landed

Two new units in Sandworm (module 56), both `Matching`, both byte-exact, six functions:

| unit | claim | functions |
| --- | --- | --- |
| `MetroidPrime/ScriptObjects/CSandwormRelTail.cpp` | `.text 0x13CBC..0x13D8C` (0xD0) | `fn_56_13D50`, `fn_56_13CF8`, `fn_56_13CBC` |
| `MetroidPrime/ScriptObjects/CSandwormRelTail2.cpp` | `.text 0x13EA4..0x13F74` (0xD0) | `fn_56_13F38`, `fn_56_13EFC`, `fn_56_13EA4` |

All eight are deleting destructors of one shape - receiver guard, sign-extended "deleting" flag
test, the payload teardown the class owns, then `CMemory::Free(self)` - and they come from the twin
run `[.text 0x13CBC..0x13F74, 8 adjacent]` in the item's reason. Two shapes, six functions:

- **0x3C, nothing to tear down** (`13CBC`, `13D50`, `13EFC`, `13F38`). Byte-identical to
  `fn_55_1064C` in `src/MetroidPrime/ScriptObjects/CSandBossRelTail.cpp`, already built inside
  module 55, so its spelling was copied unchanged:
  `if (self) { if (flag > 0) { Free(self); } } return self;`. **The flag is a `short`, not a
  `bool`** - retail's test is `extsh.` - and the flag test must be *inside* `if (self)`, or the
  `beq` lands on the `extsh.` instead of on the epilogue (measured on `fn_80006678`,
  `src/MetroidPrime/main.cpp:1167`).
- **0x58, one `delete`** (`13CF8` over `rstl::single_ptr<CProjectedShadow>`, `13EA4` over
  `rstl::single_ptr<CCollisionActorManager>`). Byte-identical to `__dt__80006678` in
  `src/MetroidPrime/main.cpp:1289` apart from the one `bl`, so it is written the same way:
  `if (self) { delete self->get(); if (flag > 0) { Free(self); } } return self;`. **The `delete` is
  the only spelling that works**: retail's callee symbol is `__dt__16CProjectedShadowFv` /
  `__dt__22CCollisionActorManagerFv`, which no C++ identifier can hold and which therefore cannot
  be declared by hand. It is also what emits the `lwz r3,0(self) / li r4,1 / bl` triple. Both
  classes declare their destructor without defining it in the header, so the `delete` emits a call
  and **no copy of the destructor into our object** (`unit_fit.sh` confirms: "no extra functions").

`Free__7CMemoryFPCv` is declared under retail's own emitted spelling, the way
`CSandBossRelTail.cpp` does it, so the relocation resolves to the same symbol the module's retail
bytes referenced.

## The carve, in four files (as required)

- `config/G2ME01/rels/Sandworm/splits.txt` - the two `.text` claims above.
- `configure.py` - two `Object(Matching, ...)` lines in the `Rel("Sandworm", ...)` block, placed
  next to the module's existing unit rather than appended at the end of the `Rel` list.
- `files.cmake` - both files, each with the "host branch is empty by design" note the neighbouring
  REL tails carry. Bodies are inside `#ifdef __MWERKS__`, so listing them adds no undefined
  reference on the host.
- `config/G2ME01/config.yml` - a `force_active:` list for the module with the six names. **Nothing
  in the module calls any of them**, so without it mwldeppc dead-strips them and the module links
  short; this is the same arrangement, and the same reason, as `CSandBossRelTail.cpp`'s two.
  Config changes, as a list: one new `force_active:` block on module 56, six names, nothing else.

Nothing else in the module is claimed. `fn_56_13D8C`/`fn_56_13E18` sit in the gap between the two
claims and `auto_00_00013D8C_text` (2 functions) plus `auto_00_00013F74_text` (24) plus the
remaining 339 stay retail bytes.

## Measured

    Sandworm matched_functions (summed over Sandworm/* units)   11 -> 17   (of 383)
    global matched_functions                                   13199 -> 13205
    global linked (complete units)                             6247 -> 6253
    both new units                                             100.00% matched, 3/3 each
    unit_fit.sh on both                                       .text claimed 208 ours 208 retail 208, fits;
                                                                no extra functions
    dtk shasum -c config/G2ME01/build.sha1                     87 files OK
    all 86 RELs vs config.yml                                  none differ
    main.dol sha1                                              6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
    check_decl_order.py / check_files_cmake.py / check_symbol_names.py / check_module_wiring.py   ok
    ./tools/goal_check.sh build/goal/item.json                 goal_check: PASS progress-twin-rel-sandworm

`flip_test.sh` is not the test for a REL unit - it looks the source up under
`extern/musyx/src/`, finds nothing and prints FAIL, which proves nothing; the module sha1 is
(`RUNNING_THE_DECOMP.md`, "The check that actually means something").

Per-function, from `build/report.json`:

| function | size | before | after |
| --- | --- | --- | --- |
| `fn_56_13CBC` | 0x3C | unmatched (`auto_00_000000DC_text`) | 100.00% |
| `fn_56_13CF8` | 0x58 | unmatched | 100.00% |
| `fn_56_13D50` | 0x3C | unmatched | 100.00% |
| `fn_56_13EA4` | 0x58 | unmatched | 100.00% |
| `fn_56_13EFC` | 0x3C | unmatched | 100.00% |
| `fn_56_13F38` | 0x3C | unmatched | 100.00% |

The twin's source matched **unchanged**: `fn_55_1064C` in `CSandBossRelTail.cpp` for the 0x3C shape,
and `__dt__80006678` in `main.cpp` for the 0x58 one - neither needed a new spelling, so no
function went backwards.

## What did not land, and why (the two 0x8C functions)

`fn_56_13D8C` and `fn_56_13E18` (0x8C each) are the third shape in that run, the
`~reserved_vector()` instantiation whose element walk has an empty body. Claiming them **breaks the
module sha1**, so they are left unclaimed.

This compiler reproduces them **instruction for instruction** - same opcode, same operands, same
branch structure, 35 instructions, including MW's counted main loop and its self-`bdnz` fill - and
differs only in register assignment and in one instruction *form*:

| | count | induction variable | peeled trip count | the subtraction |
| --- | --- | --- | --- | --- |
| retail (`fn_56_13E18`, 0x13E18) | r6 | **r3** | **r5** | `subi r5, r6, 8` |
| ours | r6 | r5 | r3 | `addi r3, r6, -8` |

Everything else is word-identical. Because the two differ only in allocation, the source has to
change what MW's allocator sees, not what the code does.

Spellings measured this run (all compiled, all scored, the rest of the file unchanged):

| spelling | result |
| --- | --- |
| `for (uchar* p = mData; p != mData + mCount; p += 8) {}` | 0x60, 96 B - strength-reduced to `cmplw p,end; bne` |
| `for (int i = 0; i < mCount; i += 8) {}` | 0x60, 96 B - IV deleted, one fill loop `(n+7)>>3` |
| `for (int i = 0, n = mCount; i < n; ++i) destroy(&ptr[i])` (the header's `destroy_elements` shape, an 8-byte class element) | **0x8C, 140 B, every opcode and operand right, 60.83%** - only the three registers and `subi`/`addi` differ |
| same, with `count` hoisted, with `i` hoisted, with `ptr` dropped and `&mData[i*8]` used, decl orders `(ptr,i,count)`, `(i,count,ptr)`, `(count,i,ptr)`, `(ptr,count,i)` | all 0x8C / 140 B; the *count* register moves (r5 or r6) but the IV is always r5 and the temp always r3 |
| same, but a pointer loop `p != p + mCount` with `destroy(p)` | 0x60, 96 B - `slwi/add/cmplw` |
| same, but `i != count` and an explicit `p[i].~T()` | 0x54, 84 B - `mtctr count; bdnz` |
| same, but `i += sizeof(T)` | 0x5C, 92 B - IV deleted |
| the same best spelling compiled with `mw_version` `GC/1.3.2`, `GC/2.7`, `GC/2.6`, `GC/2.5`, `GC/2.0p1`, `GC/1.3.2r`, `GC/2.0`, `GC/1.3` | byte-identical output on all eight - it is not a compiler-version effect |

Two structural findings worth keeping: `int i = 0;` **before** `const int count = self->mCount;`
gets the count into **r6**, as retail has it (with the count read in the `for` condition it lands
in r5) - so the source order of the two statements is load-bearing for the register allocation; and
`rstl::destroy(&ptr[i])` over an 8-byte class element is what keeps the induction variable alive at
all (the empty `{}` body lets MW delete it).

WALL: fn_56_13D8C 60.83% - instruction-for-instruction reproduction, only the induction-variable and peeled-trip-count registers (r5/r3 vs retail's r3/r5) and `subi` vs `addi -8` differ, and MW's allocator does not move across the eight GC compilers or any of the ten source spellings measured above.

NEW: progress-twin-rel-sandworm-reservedvector | progress | module:Sandworm | the two 0x8C `~reserved_vector()` copies at .text 0x13D8C and 0x13E18 need only the register allocation MW gives the induction variable; the eight-function byte-exact shape is already found (see the spellings table in docs/goal-notes/progress-twin-rel-sandworm.md), and claiming them needs the split range extended to 0x13CBC..0x13F74 as one unit.

## Notes for the next run

- The item's twin list is a map, not a work order, and this run's take was the shortest run of
  adjacent twins in the module: the other 59 twins are single functions in the module's own class
  code and need the CActor/CPatterned/CAi hierarchy the tree does not model.
- `fn_56_13F74` (0x13F74, 0x60) is the next function above the upper claim and is *not* one of the
  run's twins: it is a 0x60 deleting destructor with a `lbz`/`cmplwi` test of the byte at +0x1C and
  a call to the module-local `fn_56_1894` with `li r4,-1`, so it is one class's own flag-guarded
  teardown and would have to be written as that class's destructor rather than as a template
  instantiation.