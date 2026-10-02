// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses and
// sizes come from `config/G2ME01/symbols.txt:75-76`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_8000447C_text.s`, and the bodies below are the C++ those
// bytes are the compilation of.
//
// .text 0x8000447C..0x800045A0, 0x124 = 292 bytes, 2 functions:
//
//   __dt__11CWorldStateFv  0x8000447C  0x94  37 instructions  CWorldState::~CWorldState()
//   fn_80004510            0x80004510  0x90  36 instructions
//
// **The first is `CWorldState`'s deleting destructor.**  `include/MetroidPrime/Player/CWorldState.hpp:48`
// asserts `CHECK_SIZEOF(CWorldState, 0x24)`, which is exactly this function's stride, and the six
// members are declared at `CWorldState.hpp:41-46` in the order the `CWorldState` constructor's
// init list walks them (`src/MetroidPrime/Player/CGameState.cpp:565-571`).  The three teardowns run
// at **+0x1C, +0x10, +0x08** -
// `mLayerState`, `mMapWorldInfo`, `mRelayTracker` - the three `rstl::ncrc_ptr` members in reverse
// declaration order, which is the order a destructor runs them in and the order the bytes run in:
//
//   mLayerState    +0x1C   bl fn_80009224                            (0x80009224)
//   mMapWorldInfo  +0x10   bl ReleaseData__Q24rstl23rc_ptr<13CMapWorldInfo>Fv (0x80009058)
//   mRelayTracker  +0x08   bl fn_80009008                            (0x80009008)
//
// Each is `addic. r0,r30,off / beq / addic. r0,r30,off / beq / addi r3,r30,off / bl`: **one null
// test per class level**.  `include/rstl/rc_ptr.hpp:126` makes `ncrc_ptr< T >` an empty class
// derived from `rc_ptr< T >`, and each member is an `ncrc_ptr`, so the derived level tests and the
// base level tests and then calls the release - which is why the two local class templates below
// (rather than a hand-written `if (&member != 0)`) are what reproduce the pair.
//
// **The second, `fn_80004510`, is the deleting destructor of the `+0x18` subobject of
// `CGameState`**: its caller is `__dt__10CGameStateFv` at 0x800042F0 with `addi r3,r30,0x18 /
// li r4,-1`, and that member is `rstl::reserved_vector< rstl::rc_ptr< CPlayerState >, 4 >
// mPlayerStates` (`include/MetroidPrime/Player/CGameState.hpp:255`).  It walks the count at +0 and
// releases each 8-byte element at +4.  Measured from the bytes: the bound is a **signed** `int`
// re-read from `self` each iteration (`lwz r0,0(r28)` / `cmpw r31,r0` / `blt`), the cursor is in
// **r30** and the index in **r31** (so the index is declared before the cursor), the cursor is
// stepped before the index (`addi r30,r30,8` then `addi r31,r31,1` - the comma operator is
// evaluated left to right, so the source order *is* the instruction order), and the element
// teardown is `cmplwi r30,0 / beq / beq / mr r3,r30 / bl` - two branches on one comparison, which
// is the same two-class-level null test with the address already in a register.
//
// The capacity of the array is **codegen-neutral**: nothing in the loop can see it, because the
// loop is bounded by the signed count at +0.  `[4]` is what the member actually is.
//
// Both functions are the shape `if (self) { <member teardowns>; if (flag > 0)
// Free__7CMemoryFPCv(self); } return self;` - MWCC's deleting-destructor convention - and the flag
// is a **`short`**: retail's tail is `extsh. r0,r31 / ble`, which `int` would make `cmpwi r31,0`.
// That is count-neutral and byte-count-neutral (73 instructions and 292 bytes either way), so only
// the byte comparison sees it.
//
// **`rstl::rc_ptr` / `rstl::ncrc_ptr` are declared locally, and that is deliberate.**
// `include/rstl/rc_ptr.hpp` defines `ReleaseData()` out of line, so including it emits a 0x4C-byte
// weak copy of `ReleaseData__Q24rstl23rc_ptr<13CMapWorldInfo>Fv` into this object - a function
// retail's range does not contain.  Declared here, the primary template's `ReleaseData()` is
// *declared and never defined*, so the two calls that retail leaves named mangle to retail's own
// MWCC symbols and resolve against `src/MetroidPrime/main.cpp:1883`/`:2205`, where the same
// `template class` instantiations define them.  The two releases retail leaves **unnamed** are
// reached by full specialisations of the class template that call the `extern "C"` names.  Same two
// words, same destructor body as the real header; nothing about `rstl::rc_ptr` changes for any
// other unit, and no `rstl` header is included by this file.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  `tools/check_decl_order.py --unit
// main/MetroidPrime/Carve8000447C` is the cheap check.
//
// `extern "C"` is what keeps `__dt__11CWorldStateFv` unmangled (the name `symbols.txt:75` carries
// after the ninth upstream sync) and `fn_80004510` a plain `fn_` symbol, so objdiff pairs both.
//
// Its own unit because a claim may not span an unclaimed gap: below it,
// 0x8000408C..0x80004438 (`fn_8000408C` .. `__dt__10CGameStateFv`) is unclaimed, and above it,
// 0x800045A0..0x80004744 (`fn_800045A0` .. `fn_800046D0`) is `MetroidPrime/Carve800045A0.c`.
// The directory is retail's own, taken from the nearest claimed ranges.

/** 0x802CE388, `symbols.txt:12992`, size 0x64: `CMemory::Free(void const*)`.  Claimed by
 *  `Kyoto/Alloc/CMemory.cpp` (`.text` 0x802CE224..0x802CE72C), so our own tree supplies it.
 *  Declared, never defined here. */
extern "C" void Free__7CMemoryFPCv(const void* ptr);

/** 0x80009224, `symbols.txt:191`, size 0x50: the release of a `CWorldLayerState` payload, which
 *  retail leaves unnamed.  Declared, never defined here; the port link's stand-in is
 *  `stub_carve8000447c_0` in `src/MetroidPrime/PortLinkStubs.cpp`, an announced empty body.  The
 *  `extern "C"` name is what the member's destructor below calls. */
extern "C" void fn_80009224(void* self);

/** 0x80009008, `symbols.txt:185`, size 0x50: the release of a `CRelayTracker` payload, which
 *  retail leaves unnamed.  Declared, never defined here; the port link's stand-in is
 *  `stub_carve8000447c_1` in `src/MetroidPrime/PortLinkStubs.cpp`, an announced empty body. */
extern "C" void fn_80009008(void* self);

class CMapWorldInfo;
class CPlayerState;
class CRelayTracker;
class CWorldLayerState;

namespace rstl {

/** `rstl::rc_ptr< T >`'s two words and its destructor, declared locally so no out-of-line
 *  `ReleaseData` is emitted (`include/rstl/rc_ptr.hpp:69-124` is the real one).  `ReleaseData` is
 *  deliberately **declared and never defined**: the call from `~rc_ptr` mangles to the MWCC symbol
 *  retail's own `main.cpp` instantiations define, and no copy lands in this object. */
template < class T >
class rc_ptr {
public:
  void ReleaseData();
  ~rc_ptr() { ReleaseData(); }

private:
  const T* mPtr;
  int* mRefCount;
};

/** The empty derived level (`include/rstl/rc_ptr.hpp:126`): its implicit destructor is what emits
 *  the *second* null test, so the pair in retail's bytes is one test per class level. */
template < class T >
class ncrc_ptr : public rc_ptr< T > {};

} // namespace rstl

/** `CWorldLayerState`'s release is the unnamed 0x80009224, so this instantiation defines its own
 *  `ReleaseData` in terms of the `extern "C"` name instead of the primary template's undefined
 *  one.  Nothing is emitted out of line: the call inlines into `~rc_ptr`. */
template <>
class rstl::rc_ptr< CWorldLayerState > {
public:
  void ReleaseData() { fn_80009224(this); }
  ~rc_ptr() { ReleaseData(); }

private:
  const CWorldLayerState* mPtr;
  int* mRefCount;
};

/** `CRelayTracker`'s release is the unnamed 0x80009008; same trade as `CWorldLayerState` above. */
template <>
class rstl::rc_ptr< CRelayTracker > {
public:
  void ReleaseData() { fn_80009008(this); }
  ~rc_ptr() { ReleaseData(); }

private:
  const CRelayTracker* mPtr;
  int* mRefCount;
};

/** `CWorldState`'s layout as far as these bytes read it: the six members of
 *  `include/MetroidPrime/Player/CWorldState.hpp:41-46`, `CHECK_SIZEOF(CWorldState, 0x24)`.  The
 *  three `ncrc_ptr` members are what the destructor below destroys, in reverse declaration order. */
struct SWorldState {
  unsigned int mWorldId;
  int mAreaId;
  rstl::ncrc_ptr< CRelayTracker > mRelayTracker;
  rstl::ncrc_ptr< CMapWorldInfo > mMapWorldInfo;
  unsigned int mDesiredAreaAssetId;
  rstl::ncrc_ptr< CWorldLayerState > mLayerState;
};

/** `CGameState`'s `+0x18` member as far as `fn_80004510` reads it: the signed count at +0 and the
 *  inline element array at +4 (`rstl::reserved_vector< rstl::rc_ptr< CPlayerState >, 4 >`,
 *  `include/MetroidPrime/Player/CGameState.hpp:255`).  The elements are `ncrc_ptr` because that is
 *  what makes retail's two-branch null test; `rc_ptr` elements would be one test, and 4 bytes
 *  short - measured. */
struct SPlayerStates {
  int mCount;
  rstl::ncrc_ptr< CPlayerState > mData[4];
};

extern "C" void* fn_80004510(SPlayerStates* self, short flag);

extern "C" void* __dt__11CWorldStateFv(SWorldState* self, short flag);

/** `fn_80004510` - retail `.text:0x80004510`, 0x90 = 144 bytes.  See the header for the loop's
 *  three measured facts: the index declared outside the `for` and before the cursor, the cursor
 *  stepped first, and the signed bound re-read from `self`. */
extern "C" void* fn_80004510(SPlayerStates* self, short flag) {
  if (self) {
    int i = 0;
    rstl::ncrc_ptr< CPlayerState >* cursor = self->mData;
    for (; i < self->mCount; cursor += 1, ++i) {
      cursor->~ncrc_ptr();
    }
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

/** `__dt__11CWorldStateFv` - retail `.text:0x8000447C`, 0x94 = 148 bytes: `CWorldState`'s deleting
 *  destructor.  The three teardowns are the member expressions themselves, in reverse declaration
 *  order; a `reinterpret_cast`'d pointer variable makes MWCC compute the address once and is 12
 *  bytes short - measured. */
extern "C" void* __dt__11CWorldStateFv(SWorldState* self, short flag) {
  if (self) {
    self->mLayerState.~ncrc_ptr();
    self->mMapWorldInfo.~ncrc_ptr();
    self->mRelayTracker.~ncrc_ptr();
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}
