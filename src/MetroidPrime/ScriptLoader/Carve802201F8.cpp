// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the address and
// size come from `config/G2ME01/symbols.txt:9623`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_8021FABC_text.s:521-563`, and the body below is the C++
// those bytes are the compilation of.
//
//   fn_802201F8    0x802201F8  0x9C    39 instructions
//
// It is the class deleting destructor of an unnamed class, and the shape is the one the whole port
// uses (`mr. r30,r3 / beq` guards the receiver, `li r4,-1` is the "do not free me afterwards"
// flag, `extsh. r0,r31 / ble` reaches `Free__7CMemoryFPCv` only for a positive flag).  What is
// inside is what the member list says: the constructor `fn_802203F8` (0x802203F8, `bl`'d at
// 0x80220070 from `fn_802216C8`, the class constructor `fn_8021FBCC` calls after
// `__nw__FUlPCcPCc(0x448, "??(??)")` at 0x80220000) news the six subobjects this tears down, and
// this destructor destroys them in reverse declaration order:
//
//   +0x180  the holder whose only non-trivial member is the `rstl::string mNull` initialised by
//           `fn_802204E8` (0x802204E8) - `stw r4, 0x18(r3)` where r4 is the address of retail's
//           `"mNull__Q24rstl66basic_string<c,...>"` static,
//   +0x130  the holder `fn_80220394` (0x802203F8 calls `fn_80241CCC` at `addi r3,r31,0x134` and
//           `__ct__11CMayaSplineFv` at `addi r3,r31,0x138`),
//   +0xE4   the holder `fn_80220340` (`__ct__11CMayaSplineFv` at `addi r3,r31,0xe4`),
//   +0x94   the holder `fn_802202E8` (`__ct__11CMayaSplineFv` at `addi r3,r31,0x98`),
//   +0x48   the holder `fn_80220294` (`__ct__11CMayaSplineFv` at `addi r3,r31,0x48`),
//   +0x00   the `SLdrEditorProperties` base (`__ct__20SLdrEditorPropertiesFv` is the first call in
//           the constructor at 0x80220410), by its own out-of-line destructor.
//
// **The `+0x180` teardown is the only part that is not a plain call, and the doubled `addic.` is
// what makes it that.**  `self->mHolder.~SHolder()` expands to "test the address of the member,
// then run the holder's destructor", and the holder's destructor expands to "test the address of
// `mName`, then `~rstl::string`", and `~rstl::string` is `{ internal_dereference(); }` - the one
// `bl` at 0x80220228 to retail's out-of-line
// `internal_dereference__Q24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>Fv`
// (0x802FE9B8).  So retail's two tests are `addic. r0,r30,0x180` (the member) and
// `addic. r3,r30,0x198` (the string 0x18 into it) and the difference of the two is the layout,
// measured from the constructor: 0x180..0x197 is `sZeroVector` copied twice, 0x198 is `mName`.
// This is the same mechanism `src/MetroidPrime/main.cpp:288-293` documents for `SNodeKey` and
// `src/MetroidPrime/CIOWinDtor.cpp` reproduces for `CIOWin`; naming `mName`'s destructor directly
// gives one test (measured) and leaving it to scope exit gives none.
//
// Only the six offsets above are known.  Nothing between them is read here and the class is
// unnamed in retail (`__nw__FUlPCcPCc(0x448, "??(??)")` at 0x80220000..0x80220014, an
// MSVC-style decorated name for an unnamed type), so the gaps below are opaque padding of the
// measured size rather than invented members.
//
// Retail names this function nothing: `symbols.txt:9623` carries the `fn_<addr>` placeholder and
// this file reproduces it verbatim, so the definition keeps that name via `extern "C"`.
//
// The reverse-declaration-order rule mwcceppc imposes (definitions are emitted in reverse source
// order and mwldeppc keeps `.text` verbatim) has nothing to bite on with one function, and
// `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptLoader/Carve802201F8` confirms it
// emits nothing out of retail order.
//
// **It is a `.cpp` rather than a `.c`, and that is measured, not tidiness.**  `mName`'s destructor
// is retail's out-of-line `internal_dereference__...<c,...>Fv`, and that name is not made of
// identifier characters, so a `.c` cannot spell it (see
// `src/MetroidPrime/Carve800E10EC.cpp:22-31` for the same measurement).  `extern "C"` keeps the
// one symbol unmangled, so objdiff pairs it exactly as it would for a `.c`.
//
// **This file needs no `TARGET_PC` block at all, and that is measured.**  Every symbol it names is
// defined by a unit of ours in *both* builds, so the host's flat link loses nothing without a
// stand-in: `fn_80220294`/`fn_802202E8`/`fn_80220340` by
// `MetroidPrime/ScriptLoader/Carve80220294.c`, `fn_80220394` by
// `MetroidPrime/ScriptLoader/Carve80220394.c` (0x80220394..0x802203F8, `Matching`),
// `Free__7CMemoryFPCv` by `Kyoto/Alloc/CMemory.cpp`, `__dt__20SLdrEditorPropertiesFv` by
// `src/MetroidPrime/ScriptLoader/SLdrStructMembers.cpp:167`, and
// `internal_dereference__...<c,...>Fv` by `src/rstl/rstl_strings.cpp:150`.  The first version of
// this file did carry a `TARGET_PC` stand-in for `fn_80220394`, back when that range was still
// unclaimed; once `carve-80220394` landed the second definition was a duplicate symbol and
// `./tools/goal_check.sh` failed on `link_check: FAIL duplicates went 0 -> 1`.
//
// Its own unit because the claim above it is `MetroidPrime/ScriptLoader/Carve80220294.c`
// (0x80220294..0x80220394) and the claim below is
// `MetroidPrime/ScriptLoader/CannonBallLoaderSet.cpp` (0x8021FAB4..0x8021FABC).  The 0x2D8-byte
// gap between them is unclaimed; this claim takes its last 0x9C bytes, which `symbols.txt:9623`
// and `:9624` show is exactly `fn_802201F8` and nothing else, and the 0x23C bytes above it stay
// unclaimed.
//
// The directory is the nearest claimed range's: the claim immediately below is
// `MetroidPrime/ScriptLoader/CannonBallLoaderSet.cpp` and the one above is
// `MetroidPrime/ScriptLoader/Carve80220294.c`, so this address is in the ScriptLoader
// neighbourhood.

#include "rstl/string.hpp"

#include "Kyoto/Math/CVector3f.hpp"

/** 0x802CE388, `symbols.txt:12992`, size 0x64: `CMemory::Free(void const*)`, claimed by
 *  `Kyoto/Alloc/CMemory.cpp` in both builds.  Declared here under retail's own emitted spelling so
 *  the call needs no header; `grep -rn Free__7CMemoryFPCv src/ include/` finds the C++ name
 *  `CMemory::Free` behind it. */
extern "C" void Free__7CMemoryFPCv(const void* ptr);

/** 0x8023F07C, `symbols.txt:10163`, size 0x68: `SLdrEditorProperties::~SLdrEditorProperties(int)`,
 *  the base subobject at +0.  Not claimed by a unit of ours, so the matching build takes it from
 *  dtk's `auto_03_80220394_text.o`, exactly as `Carve80220294.c` does for
 *  `__dt__11CMayaSplineFv`.  `extern "C"` because the identifier *is* the mangled name mwcceppc
 *  derives for the C++ destructor; a C++-linkage declaration would make the call mangle a second
 *  time and link against nothing.  It needs no port stand-in either:
 *  `src/MetroidPrime/ScriptLoader/SLdrStructMembers.cpp:167` defines the destructor and is in
 *  `files.cmake`, so both builds already have the symbol. */
extern "C" void __dt__20SLdrEditorPropertiesFv(void* self, int flag);

/** 0x80220394, `symbols.txt:9627`, size 0x64 - the +0x130 holder, now claimed and `Matching` by
 *  our own `MetroidPrime/ScriptLoader/Carve80220394.c` (0x80220394..0x802203F8).  Declared here,
 *  never defined here: a second definition in this file would be a duplicate in the host's flat
 *  link. */
extern "C" void* fn_80220394(void* self, short flag);

/** 0x80220340, 0x802202E8, 0x80220294 - the three `CMayaSpline` wrappers this calls, defined by
 *  our own `MetroidPrime/ScriptLoader/Carve80220294.c` (0x80220294..0x80220394, `Matching`).  Each
 *  tears down a `CMayaSpline` at +0, +0 and +4 of the pointer it is given. */
extern "C" void* fn_80220340(void* self, short flag);
extern "C" void* fn_802202E8(void* self, short flag);
extern "C" void* fn_80220294(void* self, short flag);

/** The class `fn_802201F8` destroys.  Retail names it nothing (`"??(??"` at 0x803ACF30) and news
 *  it 0x448 bytes (`li r3,0x448` at 0x80220004), so only the six subobjects below are named -
 *  each at the offset this destructor passes it and the constructor `fn_802203F8` constructs it
 *  at.  The gaps are the measured sizes of the subobjects in between, which nothing in this range
 *  reads. */
struct SHolderAt180 {
  CVector3f mVecA;     // +0x00, `stfs` from `sZeroVector__9CVector3f` at 0x802204FC
  CVector3f mVecB;     // +0x0C, ditto at 0x80220514
  rstl::string mName;  // +0x18, `stw r4, 0x18(r3)` where r4 is `"mNull__Q24rstl66basic_string..."`
  uchar mUnknown24[5]; // +0x24, the byte `fn_802204E8` zeroes at +0x28 (0x80220534)
};

struct SUnknown448 {
  uchar mUnknown00[0x48];
  uchar mSpline48[0x4C];  // `__ct__11CMayaSplineFv` at `addi r3,r31,0x48` (0x80220428)
  uchar mSpline94[0x50];  // `__ct__11CMayaSplineFv` at `addi r3,r31,0x98` (0x80220440)
  uchar mSplineE4[0x4C];  // `__ct__11CMayaSplineFv` at `addi r3,r31,0xe4` (0x80220460)
  uchar mSpline130[0x50]; // `fn_80241CCC` at +0x4, `__ct__11CMayaSplineFv` at +0x8 (0x8022046C)
  SHolderAt180 mHolder;   // +0x180, `fn_802204E8` at `addi r3,r31,0x180` (0x80220498)
};

extern "C" void* fn_802201F8(SUnknown448* self, short flag) {
  if (self) {
    self->mHolder.~SHolderAt180();
    fn_80220394(&self->mSpline130, -1);
    fn_80220340(&self->mSplineE4, -1);
    fn_802202E8(&self->mSpline94, -1);
    fn_80220294(&self->mSpline48, -1);
    __dt__20SLdrEditorPropertiesFv(self, -1);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}
