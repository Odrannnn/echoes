// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:6047-6048`, the instructions are the ones dtk
// emitted into `build/G2ME01/asm/auto_03_8016DF3C_text.s` before the claim existed (they are now
// this unit's own `build/G2ME01/asm/MetroidPrime/Carve8016FD4C.s:9-33`, and the bytes were re-read
// this run from the disc with `tools/dol_read.py 0x8016FD4C 0x48`), and the body below is the C
// those bytes are the compilation of.
//
// .text 0x8016FD4C..0x8016FD94, 0x48 = 72 bytes, 2 functions:
//
//   fn_8016FD4C    0x8016FD4C  0x20     8 instructions   the forwarder
//   fn_8016FD6C    0x8016FD6C  0x28    10 instructions   the null-guarded construct
//
// **What the two are: `rstl::construct< CHealthInfo >`'s two halves.**  Retail calls the pair once
// inside this same run and neither body touches its arguments beyond passing them on, so both are
// read off the twin pair rather than off their own code:
//
//   fn_8016FD4C  is a frame and one unconditional `bl fn_8016FD6C` - no load, no test, no return
//                value - which is the shape `include/rstl/construct.hpp:74-77` gives the in-class
//                `construct` (`construct_impl(dest, src)`).  **The twin is exact**: `fn_80004D3C`
//                (0x80004D3C, 0x20, `src/MetroidPrime/Player/Carve80004C4C.c`, a `Matching` unit) is
//                these eight instructions word for word **including the `bl`** - both branch +0x14
//                to their own null-guarded `construct`, so the two objects' bytes are identical
//                (compared this run with `tools/dol_read.py`) - and `__sys_free` (0x80008A28,
//                `src/MetroidPrime/main.cpp`) is the same shape a third time.
//   fn_8016FD6C  tests its *first* argument for null (`cmplwi r3,0x0`, i.e. the destination, not
//                the source) and only then calls `__ct__11CHealthInfoFRC11CHealthInfo` with both
//                arguments untouched, so it is `rstl::construct`'s null guard.  **The twin is
//                exact too**: `fn_80004D5C` (0x80004D5C, 0x28,
//                `src/MetroidPrime/Player/CGameStateBlockConstruct.cpp`, a `Matching` unit) is
//                these ten instructions word for word with `bl fn_80004AA0` where this `bl` is,
//                including the `cmplwi` sitting between `mflr r0` and the `stw r0,0x14(r1)`.
//
// The callee is the measurement that names the element: the retail `bl` at 0x8016FD80 goes to
// `__ct__11CHealthInfoFRC11CHealthInfo` (0x80070D60, `symbols.txt:2082`, 0x54 bytes), whose own
// bytes copy the source into the destination member by member - three `lfs`/`stfs` pairs for the
// floats at +0x00/+0x04/+0x08, `lwz`/`stw` for +0x0C and +0x14, `lha`/`sth` for +0x10, +0x12,
// +0x18 and +0x1A, and `lbz`/`stb` for +0x1C - which is `CHECK_SIZEOF(CHealthInfo, 0x20)` in
// `include/MetroidPrime/CHealthInfo.hpp` exactly.  The caller confirms it: `fn_8016FCB8`
// (0x8016FCB8, 0x94, unclaimed) tests the byte at +0x20 of its object and, when it is zero, calls
// `bl fn_8016FD4C` on that object and sets the byte to 1 (`0x8016FCD8..0x8016FCE0`); the copy the
// `else` branch then makes inline is the same 0x20 bytes.  So the pair is the "not constructed
// yet" branch of a holder whose `CHealthInfo` sits at +0x00 and whose initialised flag is the byte
// at +0x20, and the whole pair is written here as what its bytes are.
//
// **Why this unit does not define the copy constructor it calls.**  `0x80070D60` is inside
// `MetroidPrime/ScriptObjects/CScriptActor.cpp`'s existing claim (0x8006EB98..0x80070DB4,
// `config/G2ME01/splits.txt:252`), and that unit is `NonMatching`, so dtk's own object for it
// supplies the bytes in the DOL link and the symbol is nothing this carve can or should claim.
// The host link has no such object, so `src/Kyoto/Alloc/PortMwccNew.cpp` defines the name - see
// the note there; the DOL link never reads that file.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  (Written ascending first, this file's object came out
// `fn_8016FD6C` first and `tools/decomp_build.sh` failed on the whole-DOL sha1 immediately.)
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definitions have to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.  The file is compiled as C for the port's host build and, like every other source
// in `files.cmake`, is syntax-checked as C++ by `tools/probe_sources.sh`; both accept this file as
// written.
//
// Its own unit because a claim may not span an unclaimed gap.  Below this range,
// 0x8016DF3C..0x8016FD4C is still dtk's `auto_03_8016DF3C_text` (`fn_8016FCB8` above is the
// function that ends exactly where this claim starts), and above it 0x8016FD94..0x80170DE0 is the
// rest of the same unclaimed run (`fn_8016FD94`, 0xBC, is the function immediately above).
//
// The directory is retail's own, taken from the nearest claimed ranges:
// `MetroidPrime/CRumbleManager.cpp` (0x8016DC58..0x8016DF3C, `splits.txt:873`) is the claim below
// this one and `MetroidPrime/CWorldLayerState.cpp` (0x80171DC4..0x801724CC, `splits.txt:880`) the
// one above, so this address is in the `MetroidPrime/` neighbourhood.

/** 0x80070D60, `symbols.txt:2082`, 0x54 = 84 bytes: `CHealthInfo`'s copy constructor, the callee
 *  of `fn_8016FD6C` below.  Its bytes are inside `ScriptObjects/CScriptActor.cpp`'s existing
 *  claim, so no unit of this carve defines it; it is declared here with retail's own mangled name,
 *  which is what a plain C definition site emits the `bl` against.  Declared, never defined
 *  here. */
extern void __ct__11CHealthInfoFRC11CHealthInfo(void* self, const void* src);

/** `fn_8016FD6C` - retail `.text:0x8016FD6C`, 0x28 = 40 bytes, 10 instructions: `rstl::construct`
 *  for the 0x20-byte `CHealthInfo` above - a null test on the *destination* and one call to its
 *  copy constructor.  Its twin is `fn_80004D5C` (0x80004D5C, 0x28), byte for byte apart from the
 *  `bl` target. */
void fn_8016FD6C(void* self, const void* src);

void fn_8016FD6C(void* self, const void* src) {
  if (self != 0) {
    __ct__11CHealthInfoFRC11CHealthInfo(self, src);
  }
}

/** `fn_8016FD4C` - retail `.text:0x8016FD4C`, 0x20 = 32 bytes, 8 instructions: the in-class
 *  `construct` forwarder, a frame and one call to `fn_8016FD6C` above and nothing else, as
 *  `include/rstl/construct.hpp:74-77` spells it.  Its twin is `fn_80004D3C` (0x80004D3C, 0x20),
 *  byte for byte identical, `bl` included. */
void fn_8016FD4C(void* self, const void* src);

void fn_8016FD4C(void* self, const void* src) { fn_8016FD6C(self, src); }
