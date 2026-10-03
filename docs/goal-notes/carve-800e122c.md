# carve-800e122c — `MetroidPrime/Carve800E122C`, PASS

## What I did

Carved the two functions of dtk's unclaimed `main/auto_03_800E122C_text` range at
`.text 0x800E122C..0x800E131C` (0xF0 = 240 bytes) into a new `Matching` unit, and gave the
port's flat link the one announced stand-in that carve's callee needs.

Claim, exactly the seeded range and nothing else:

    fn_800E12D4    0x800E12D4  0x48    18 instructions
    fn_800E122C    0x800E122C  0xA8    42 instructions

Four files, plus the stand-in:

| file | change |
| --- | --- |
| `src/MetroidPrime/Carve800E122C.cpp` | new, definitions in **descending** address order |
| `configure.py:731` | `Object(Matching, "MetroidPrime/Carve800E122C.cpp")`, one line |
| `config/G2ME01/splits.txt:541-542` | `MetroidPrime/Carve800E122C.cpp:` → `.text start:0x800E122C end:0x800E131C`, between `Carve800E10EC.cpp` (ends 0x800E122C) and `Carve800E1548.c` (starts 0x800E1548) |
| `files.cmake:690` | `src/MetroidPrime/Carve800E122C.cpp`, same neighbours |
| `src/MetroidPrime/PortLinkStubs.cpp:1854-1879` | `stub_800e122c_0() asm("fn_800E131C")` + its announced paragraph |

`total_functions` is still **28465** (`All: ... 13528 / 28465 functions`).

## Measured

    ./tools/decomp_build.sh MetroidPrime/Carve800E122C.cpp
      main/MetroidPrime/Carve800E122C  total_functions 2, matched_functions 2,
        fuzzy 100.0, matched_code 240/240, complete_units 1, metadata.complete true
      fn_800E12D4 72 100.0      fn_800E122C 168 100.0
    sha1sum build/G2ME01/main.dol
      6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
    ./tools/unit_fit.sh MetroidPrime/Carve800E122C.cpp
      .text claimed 240  ours 240  retail 240  fits
      no extra functions: our object defines only what the retail unit object does
    ./tools/flip_test.sh MetroidPrime/Carve800E122C.cpp
      PASS  -> kept as Matching      kept: 1 / 1  failed: 0  skipped: 0
    ./tools/carve_diff.sh 800E122C F0 build/G2ME01/obj/MetroidPrime/Carve800E122C.o
      retail 60 instructions / 240 bytes, ours 60 / 240, "differing instructions: 4"
      - and the 4 are 0x800E1288, 0x800E129C, 0x800E12AC, 0x800E12F8, i.e. the four `bl`
        displacements the linker fills in.  objdump resolves them to exactly retail's
        targets (`8030154c`, `802ce388`, `802ce388`, `800e131c`), which is why main.dol
        still hashes to retail.
    ./tools/goal_check.sh build/goal/item.json
      goal_check: PASS carve-800e122c
      gate.sh ok (report diff: "main/auto_03_800E122C_text: 5 function(s) accounted for
        across 2 new unit(s) in main (exact count match - a split, not a loss)")
      counts: matched 13526 -> 13528, linked 6574 -> 6576
      probe: 945 files, 0 failed, 0 errors; link: LINKED (287 undefined, 0 duplicates)
      port undefined 287 == judge baseline 287

## Why the bodies are what they are

Both functions are byte-shape twins, confirmed by disassembling **our own already-`Matching`
objects**, which is the only instrument that shows the bytes rather than a percentage:

* `fn_800E122C` is retail's `rstl::vector<TToken<CTexture>, rstl::rmemory_allocator>::~vector()`,
  emitted into `src/MetroidPrime/CSplashScreen.cpp` at 100.00% as
  `__dt__Q24rstl54vector<17TToken<8CTexture>,Q24rstl17rmemory_allocator>Fv`.  `CSplashScreen.o`
  `dc4`-`e68` is the same word for word as retail's 0x800E122C-0x800E12D3.
* `fn_800E12D4` is retail's `single_ptr<T>::operator=(T*)`, twin of
  `single_ptr_assign_800064D0` (`src/MetroidPrime/main.cpp:1213-1218`, 100.00%).
  `main.o` `1118`-`115c` is the same word for word as 0x800E12D4-0x800E131B.

**Lesson worth keeping: this one had to be a `.cpp`, not the `.c` the seed asks for.**  The four
`stw`s at 0x800E1264-0x800E1270 are `include/rstl/construct.hpp:110`'s
`destroy(It begin, It end)` taking `rstl::vector`'s two iterators **by value** (hoisted in reverse
call order).  A raw-pointer C loop cannot spell them, and `include/rstl/vector.hpp` is not
includable from a `.c` (`namespace rstl {` is a C++ construct) — the same wall
`src/MetroidPrime/Carve800E10EC.cpp:22-31` measured for its own `~vector()` 0x140 bytes below.
`extern "C"` keeps both `fn_` symbols unmangled, which is the only reason the `.c` exists there.

**Using the real header is also what made the port's link close with no second stand-in.**  The
element teardown is `TToken<CTexture>`'s implicit destructor calling its base
`CToken::~CToken()`: MWCC spells it `__dt__6CTokenFv` (0x8030154C, `symbols.txt:13922`,
`Kyoto/CToken.cpp` is `MatchingFor("G2ME01")` and claims 0x803013D0..0x80301710) and a host
compiler spells `_ZN6CTokenD1Ev` from the very same `src/Kyoto/CToken.cpp:21`.  Spelling that
call by hand would have put a name on the port's undefined list that nothing removes.

`fn_800E12D4` calls `fn_800E131C` by name and does **not** say `delete`.  The matched twin in
`main.cpp` does say `delete self->mPtr`, but the pointee here is anonymous in retail and the name
MWCC would emit for it, `__dt__18CInGameGuiManagerFv`, **does not exist anywhere in the DOL**
(`grep __dt__18CInGameGuiManager config/G2ME01/symbols.txt` is empty; `nm build/G2ME01/main.elf`
has only `800e131c T fn_800E131C` in this range).  Naming the callee the way retail names it
reproduces `lwz r3,0(r3) / li r4,1 / bl` at retail's own displacement.  The `1` must be a
literal: retail materialises `li r4,1` *before* reloading `r3` from `self` (0x800E12E8 vs
0x800E12F4), which is the schedule a known constant argument gives.

## The port stand-in, and why it is a stub and not a body

    python3 tools/link_gap.py --rebuild     # with the carve, before the stub
      gap grew: fn_800E131C is not in port_link_gap_list.md      (the ONLY line printed)
    python3 tools/link_gap.py --rebuild     # with the stub
      279 MISSING   ok: 279 MISSING symbol(s), all accounted for in port_link_gap_list.md

`fn_800E131C` (0x8C = 140 bytes) is the byte immediately **above** this claim and stays in dtk's
`auto_03_800E122C_text.o`, so the DOL link resolves the `bl` from there and nothing is
duplicated.  For the port it gets `stub_800e122c_0`, following `stub_carve800e0efc_0` one carve
below (named after the unit that asks for it, not `stub_NNN`, and the header paragraph at the top
of `PortLinkStubs.cpp` deliberately left untouched for that reason).

**An empty body does none of what retail's `fn_800E131C` does** — measured from
`build/G2ME01/asm/auto_03_800E122C_text.s:80-120` it releases an optional `CGuiFrame` at +0x08/
+0x0C through `__dt__9CGuiFrameFv` and an optional `CGuiFrameLoader` at +0x00/+0x04 through
`__dt__15CGuiFrameLoaderFv`, both with the deleting flag 1, then frees the holder on a positive
short — so an object destroyed through this name keeps both of its members.  That trade is
announced in the stub's own paragraph, exactly as `stub_carve800e0efc_0` and `stub_8001fedc_0`
do for theirs.

## Blockers

None.  No `NEW:` and no `WALL:` line for this item.
