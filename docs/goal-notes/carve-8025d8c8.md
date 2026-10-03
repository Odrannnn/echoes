# carve-8025d8c8

`match` on `Weapons/Carve8025D8C8` - **PASS**. New `Matching` unit, 2/2 functions, 100.00%.

## What was carved

`.text 0x8025D8C8..0x8025D9D0`, 0x108 = 264 bytes, 2 functions, out of dtk's unclaimed
`main/auto_03_8025CA80_text` (which spans 0x8025CA80..0x8025DD1C, 0x129C):

```
   fn_8025D94C    0x8025D94C  0x84    33 instructions
   fn_8025D8C8    0x8025D8C8  0x84    33 instructions
```

`symbols.txt:10608-10609` carries the `fn_<addr>` placeholders; the bytes are
`build/G2ME01/asm/auto_03_8025CA80_text.s:1031` and `:1071`.

## What they are

`rstl::optional_object<TLockedToken<CStringTable>>::operator=(const TLockedToken<CStringTable>&)`,
emitted twice. Confirmed against the seeded twin
`__as__Q24rstl47optional_object<28TLockedToken<12CStringTable>>FRC28TLockedToken<12CStringTable>`
at `0x8000835C` (`symbols.txt:157`, weak, 0x84), which `src/MetroidPrime/main.cpp` emits through
`include/rstl/optional_object.hpp:46` -> `assign()` (`:82-89`): **all 33 instructions are the
twin's 33 with only the three `bl` targets different** - same 0x20 frame, same three saved
registers in the same order, same branch targets at the same offsets. The twin's live asm is
`build/G2ME01/asm/MetroidPrime/main.s:3459-3498`.

Instruction map (offsets for `fn_8025D8C8`; `fn_8025D94C` is identical):

| offset | instruction | what |
| --- | --- | --- |
| +0x20 | `lbz r0,0xc(r3)` | `m_valid` (`optional_object.hpp:80`) |
| +0x28 | `bne +0x58` | the already-valid arm, laid out after the construct arm |
| +0x2C/+0x30 | `mr. r0,r30` / `beq +0x4c` | the placement-new **destination guard**, from `construct_impl`'s `new (dest) T(src)` (`rstl/construct.hpp:54-56`). This is why the frame is 0x20, not 0x10 |
| +0x34 | `mr r29,r0` | the destination in its own callee-saved register, live across both calls while `r30` keeps `this` for the `m_valid` store |
| +0x38 | `bl __ct__6CTokenFRC6CToken` (0x803015B4) | `mToken(token)`, `TToken.hpp:76` |
| +0x3C..0x44 | `lwz r0,0x8(r31)` / `mr r3,r29` / `stw r0,0x8(r29)` | `mItem(*token)`, `T*` at +0x08 |
| +0x48 | `bl Lock__6CTokenFv` (0x80301490) | `mToken.Lock()`, third statement of the same ctor |
| +0x4C..0x54 | `li r0,1` / `stb r0,0xc(r30)` / `b +0x64` | `m_valid = true; return;` |
| +0x58 | `bl __as__6CTokenFRC6CToken` (0x803013D0) | `mToken = token.mToken` |
| +0x5C | `lwz r0,0x8(r31)` (+ `stw r0,0x8(r30)`) | `mItem = token.mItem`, so `TLockedToken::operator=` (`TToken.hpp:78-82`) is exactly those two statements |
| +0x64..0x80 | epilogue, `mr r3,r30` | the `optional_object&` this overload returns |

Layout fixed twice: `TLockedToken<CStringTable>` = `TToken<CStringTable>` (a `CToken`, no extra
members, 8 bytes: `CToken.hpp:34-35`) + `T* mItem` at +0x08, and `m_valid` at +0x0C - so
`sizeof(TLockedToken<CStringTable>) == 0xC`. Same shape
`src/MetroidPrime/CModelDataCopyCtor.cpp:18-29` measures on `optional_object<TLockedToken<CModel>>`.

## The one real decision: this unit is `.cpp`, not `.c`

The carve vein wants plain C so `fn_` names do not mangle. `extern "C"` gives that identically
(precedent: `src/MetroidPrime/Carve8024492C.cpp`, listed in `files.cmake` with that reason), and the
reason to need C++ at all is measured.

**Written as plain `.c` with the three callees declared `extern`, both bodies compile to 30
instructions, not 33.** Spellings tried, all measured against `build/G2ME01/main.elf`:

| spelling | instructions | what was missing |
| --- | --- | --- |
| `if (self->mValid) { assign } else { if (self) { ctor; mItem; Lock } valid=1 }` | 30 | `cmplwi r30,0` for `mr. r0,r30`; no `mr r29,r0`; frame -0x10; `stw 0x8(r30)` for `stw 0x8(r29)` |
| same, `if (!self->mValid) { ... } else { assign }` | 30 | identical - **block layout then matches retail** (`bne` to the out-of-line assign arm at +0x58), only the register is wrong |
| `unsigned char* dest = self->mData; if (dest) { ... }` | 30 | `dest` is copy-propagated into `r30`; check becomes `cmplwi r30,0` |
| nested `struct SToken8025 {u32,u32}` / `SLocked8025` / `SOpt8025`, `dest` typed `SLocked8025*`, ctor arg `&dest->mToken` | 30 | same |
| `void* dest = self->mData`, `if (dest != 0)` | 30 | same |
| whole construct arm in a `static inline` helper taking `dest` | 24 | helper folded differently, worse |

All three missing instructions are one mechanism: MWCC's **C++ placement-new front end keeps
`dest` a value distinct from `this`**, so the null test is `mr. r0,r30` instead of `cmplwi r30,0`
and the destination gets its own callee-saved register. The C front end copy-propagates the two
together, the register disappears, and the frame drops from 0x20 to 0x10.

Writing the construct as `new (dest) TLocked8025(item)` - the same expression `rstl::construct`
makes - restores all three on the first try. Confirmed independently by compiling the real
template (`vt.cpp` probe: `optional_object<TLockedToken<CStringTable>>::operator=` from the tree's
own headers) and getting retail's bytes exactly.

## Two smaller findings

- **Naming the real `TLockedToken<CStringTable>` emits a fourth function.** Measured: the
  instantiation drags in `__dt__22TToken<12CStringTable>Fv` (0x54 = 84 bytes of weak `.text`),
  because `TLockedToken` has no destructor of its own and `TToken`'s is then needed. A two-function
  claim cannot hold it. So `TLocked8025` in the file is that class with its three members spelled
  out, `CToken` taken from the real `include/Kyoto/CToken.hpp`, and no destructor.
- **The three callees are real, not stubs.** `nm` on the compiled object: two `T` symbols
  (`fn_8025D8C8`, `fn_8025D94C`), three `U` (`__ct__6CTokenFRC6CToken`, `Lock__6CTokenFv`,
  `__as__6CTokenFRC6CToken`), all defined by `Kyoto/CToken.cpp`. `PortLinkStubs.cpp` needed no
  entry; `grep` for `fn_8025D8C8`/`fn_8025D94C` across `src/`, `include/`, `configure.py`,
  `config/G2ME01/splits.txt` and `files.cmake` finds nothing but this claim.

## Placement

Address order in all three manifests. `configure.py` between `Weapons/CProjectileWeapon.cpp`
(0x802591B4..0x8025CA80) and `Weapons/CCollisionResponseData.cpp` (0x8025DD1C..); `splits.txt`
`.text start:0x8025D8C8 end:0x8025D9D0` between those two blocks; `files.cmake` between
`src/WorldFormat/Carve80256D1C.c` and `src/MetroidPrime/Carve8026040C.c` (the carve block there is
address-ordered). Definitions **descending** by address - `fn_8025D94C` then `fn_8025D8C8` -
because mwcceppc emits in reverse source order.

**The carve-vein's link-order cycle did not fire, measured.** The hazard is a claim whose
`auto_*` range starts exactly where another unit's `.text` ends, and that is the case here:
`Weapons/CProjectileWeapon.cpp` ends at 0x8025CA80 and dtk's `auto_03_8025CA80_text` starts there.
It links anyway: the claim is strictly inside the gap, so dtk's remaining ranges are
0x8025CA80..0x8025D8C8 and 0x8025D9D0..0x8025DD1C with this unit between them - a chain, not a
cycle. `fn_8025D9D0` (0x8C = 140 bytes, `symbols.txt:10610`) starts 4 bytes above the claim and
stays dtk's.

## Verification

```
./tools/decomp_build.sh -r                 # clean, DOL sha1 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/decomp_build.sh Weapons/Carve8025D8C8.cpp
  All: 37.62% fuzzy, 31.05% matched, 13.92% linked (973 / 2368 files)
  Code: 2029652 / 6535816 bytes (13534 / 28465 functions)      <- was (972 / 2368), (13532 / 28465)
  DOL: 59.24% fuzzy, 48.09% matched, 21.98% linked (685 / 1566 files)   <- was (684 / 1566), (11594 / 16726)
sha1sum build/G2ME01/main.dol               -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
build/report.json  main/Weapons/Carve8025D8C8
  fuzzy_match_percent 100.0, matched_code 264/264, matched_functions 2/2,
  complete_code 264/264, complete_units 1/1, total_functions 2
  fn_8025D94C 132 B 100.0      fn_8025D8C8 132 B 100.0
total_functions 28465 (unchanged)
./tools/unit_fit.sh Weapons/Carve8025D8C8.cpp
  .text claimed 264  ours 264  retail 264  fits;  no extra functions
python3 tools/check_decl_order.py --unit Weapons/Carve8025D8C8   -> ok, 1 unit checked, in retail order
python3 tools/check_symbol_names.py                              -> checked 596 units, 0 missing names
python3 tools/check_files_cmake.py                               -> every configured DOL object listed or excluded
./tools/flip_test.sh Weapons/Carve8025D8C8.cpp                   -> PASS, kept as Matching
./tools/goal_check.sh build/goal/item.json                       -> goal_check: PASS carve-8025d8c8
  ok  no judge-owned path touched
  ok  gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok  counts: matched 13532 -> 13534   linked 6580 -> 6582
```

Byte-level, independent of objdiff: comparing the compiled object against `build/G2ME01/main.elf`
for both functions, **30 of 33 words are identical and the 3 that differ are exactly the three
`bl` immediates** (`48 00 00 01` vs retail's resolved `48 0a 3c b5` / `48 0a 3b 81` / `48 0a 3a b1`),
which the linker fills from the three `R_PPC_REL24` relocations to 0x803015B4 / 0x80301490 /
0x803013D0. Every opcode, register, displacement and branch offset already matches.

`g++ -std=c++20 -fsigned-char -DTARGET_PC -DAURORA ... -include platform/compat.h -fsyntax-only`
on the new file exits 0, so `files.cmake` listing it is safe for the port build.

## Files

- `src/Weapons/Carve8025D8C8.cpp` (new, 141 lines including the header)
- `configure.py`: one line, `Object(Matching, "Weapons/Carve8025D8C8.cpp")` after
  `Weapons/CProjectileWeapon.cpp`
- `config/G2ME01/splits.txt`: two lines after the `Weapons/CProjectileWeapon.cpp` block
- `files.cmake`: one line after `src/WorldFormat/Carve80256D1C.c`