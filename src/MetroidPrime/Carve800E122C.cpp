// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:3909-3910`, the instructions are the ones dtk
// itself emitted into `build/G2ME01/asm/auto_03_800E122C_text.s:8-78`, and the bodies below are
// the C those bytes are the compilation of.
//
// .text 0x800E122C..0x800E131C, 0xF0 = 240 bytes, 2 functions:
//
//   fn_800E12D4    0x800E12D4  0x48    18 instructions   the twin, one `bl` target different
//   fn_800E122C    0x800E122C  0xA8    42 instructions   the twin
//
// **Both are byte-shape twins of one already-`Matching` function, read out of its own source
// rather than guessed.**  The seed named them and the twin was confirmed by disassembling our own
// objects, which is the only instrument that shows the bytes:
//
//   fn_800E122C  <- `rstl::vector<TToken<CTexture>, rstl::rmemory_allocator>::~vector()` -
//     `__dt__Q24rstl54vector<17TToken<8CTexture>,Q24rstl17rmemory_allocator>Fv`, emitted into
//     `src/MetroidPrime/CSplashScreen.cpp` by the `CFontImageDef` instantiation (its
//     `CFontImageDef::~CFontImageDef()` is the one retail-named member that reaches it), and at
//     100.00% there.  `powerpc-eabi-objdump -d build/G2ME01/obj/MetroidPrime/CSplashScreen.o`
//     gives `dc4`-`e68`, and **every one of those 42 instructions is the same word as retail's
//     0x800E122C-0x800E12D3** - the `bl` at `e20` being the only field the linker fills in.
//   fn_800E12D4  <- `single_ptr_assign_800064D0` in `src/MetroidPrime/main.cpp:1213-1218`, retail
//     `single_ptr<CGameGlobalObjects>::operator=`, at 100.00% there.  `main.o` gives `1118`-`115c`
//     against retail's 0x800E12D4-0x800E131B, word for word.
//
// **It is a `.cpp` rather than a `.c`, and that is measured, not tidiness.**  Retail names neither
// of these, so `symbols.txt` carries the `fn_<addr>` placeholder and the definitions have to keep
// that name verbatim - which `extern "C"` does on every compiler here, the same trick
// `src/MetroidPrime/Carve800E10EC.cpp` uses 0x140 bytes below.  What forces the `.cpp` is the four
// `stw`s at 0x800E1264-0x800E1270: `fn_800E122C`'s loop is `rstl::destroy(begin(), end())` over
// `rstl::vector<TToken<CTexture>>`, and those stores are `include/rstl/construct.hpp:112`'s
// `destroy(It begin, It end)` taking the two iterators by value (hoisted in reverse call order -
// the lesson `docs/RUNNING_THE_DECOMP.md` records for `mwcceppc`).  A raw-pointer C loop with a
// placeholder callee cannot spell them at all, and `include/rstl/vector.hpp` is not includable
// from a `.c` (`namespace rstl {` is a C++ construct) - the same wall
// `src/MetroidPrime/Carve800E10EC.cpp:22-31` measured for its own `~vector()`.
//
// **Using the real header is also what makes both `bl` targets resolve in the *port's* link**,
// which a hand-declared `__dt__6CTokenFv` would not: the element teardown is
// `TToken<CTexture>`'s implicit destructor calling its base `CToken::~CToken()`, which MWCC spells
// `__dt__6CTokenFv` (0x8030154C, `symbols.txt:13922`, 0x68, global - `Kyoto/CToken.cpp` claims
// 0x803013D0..0x80301710 and is `MatchingFor("G2ME01")`, so the DOL link resolves it) and a host
// compiler spells `_ZN6CTokenD1Ev` from the very same `src/Kyoto/CToken.cpp:21`.  Spelling the
// call by hand would put a name on the port's undefined list that nothing removes.
//
// The vector's own layout is the header's (`include/rstl/vector.hpp:19-22`): `mAllocator` is the
// empty `rstl::rmemory_allocator`, so it takes a byte and `mCount`/`mCapacity`/`mItems` land at
// +0x04/+0x08/+0x0C - which is exactly what retail reads at 0x800E1254 (`lwz r0,4(r28)`) and
// 0x800E1258 (`lwz r30,0xc(r28)`), and the element stride is `slwi r0,r0,3` at 0x800E125C, so
// `TToken<CTexture>` is 8 bytes.
//
// **The `li r4,0` at 0x800E1284 is why the element teardown is `__dt__6CTokenFv` and not a
// `delete`.**  `rstl::destroy` passes the deleting flag 0 - it releases the reference, it does
// not free the element - and `include/rstl/construct.hpp:85-95`'s `destroy_impl(T*)` is what
// puts the `cmplwi r30,0 / beq` null guard in front of it.  Both instructions come from the
// header; neither is written here.
//
// **`fn_800E12D4` calls `fn_800E131C` by name and does not say `delete`.**  `delete self->mPtr`
// is what the matched twin in `main.cpp` says, and it is why that object's own bytes are right -
// but the pointee here is anonymous in retail and the name MWCC would emit for it,
// `__dt__18CInGameGuiManagerFv`, **does not exist anywhere in the DOL** (`grep __dt__18CInGameGuiManager
// config/G2ME01/symbols.txt` is empty, and `nm build/G2ME01/main.elf` has only
// `800e131c T fn_800E131C` in this range).  Naming the callee the way retail names it reproduces
// `lwz r3,0(r3) / li r4,1 / bl` at retail's own displacement, and objdiff does not compare
// `R_PPC_REL24` targets anyway (`src/MetroidPrime/main.cpp:1186`).  The **1** is the deleting
// flag, and it has to be a literal: retail materialises `li r4,1` *before* it reloads `r3` from
// `self` (0x800E12E8 vs 0x800E12F4), which is the schedule a known constant argument gives.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// Its own unit because a claim may not span an unclaimed gap, and because the neighbours are not
// trivial.  The claim is exactly 0x800E122C..0x800E131C; the run below it is
// `MetroidPrime/Carve800E10EC.cpp` (0x800E10EC..0x800E122C, `splits.txt`) and the run above it
// is `fn_800E131C` (0x800E131C, 0x8C = 140 bytes), which `fn_800E12D4` calls and which is
// therefore a member of the deleting chain and not of this claim.  The whole claim sits inside
// `auto_03_800E122C_text` (0x800E122C..0x800E1548), and `Carve800E1548.c` already owns
// 0x800E1548..0x800E163C, which is what keeps `dtk dol split` from reporting a link-order cycle
// against a neighbouring unit boundary.
//
// The directory is retail's own, taken from the nearest claimed ranges: below is
// `MetroidPrime/Carve800E10EC.cpp` and above is `MetroidPrime/Carve800E1548.c`
// (`config/G2ME01/splits.txt:538,544`).  For an anonymous function that is the only evidence there
// is, and it beats a lane picking the directory it happened to own.

#include <Kyoto/Graphics/CTexture.hpp>
#include <Kyoto/TToken.hpp>
#include <rstl/vector.hpp>

/** 0x800E131C, `symbols.txt:3911`, 0x8C = 140 bytes: retail's unclaimed deleting destructor that
 *  `fn_800E12D4` calls for the object its `single_ptr` holds.  **It is the byte immediately above
 *  this claim and stays in dtk's `auto_03_800E122C_text.o`**, so the DOL link resolves the `bl`
 *  from there and nothing is duplicated.  Declared, never defined here; the port's flat link gets
 *  the announced empty stand-in `stub_800e122c_0` in `src/MetroidPrime/PortLinkStubs.cpp`, and
 *  **nothing here claims `fn_800E131C` is decompiled**.  Measured from
 *  `build/G2ME01/asm/auto_03_800E122C_text.s:80-120` it releases an optional `CGuiFrame` at +0x08/
 *  +0x0C behind its own `lbz` test, then an optional `CGuiFrameLoader` at +0x00/+0x04 behind
 *  another, and frees the holder when the flag casts to a positive short.  The second parameter is
 *  retail's deleting flag - the lesson `docs/goal-notes/carve-8001fedc.md` paid for: a
 *  one-parameter declaration drops the `li`. */
extern "C" void fn_800E131C(void* self, int deleting);

/** `rstl::single_ptr<T>`'s one word, retail's layout (`include/rstl/single_ptr.hpp:18`): the
 *  pointee at +0x00.  `fn_800E12D4` neither null-checks it nor touches the object behind it, so it
 *  is modelled as the `void*` retail's `delete mPtr` sees - the same choice
 *  `src/MetroidPrime/Carve800E0EFC.c:130` makes for the same layout. */
struct SCarve800E122CSinglePtr {
  void* x0_ptr;
};

typedef rstl::vector< TToken< CTexture >, rstl::rmemory_allocator > CCarve800E122CTokenVector;

extern "C" void* fn_800E12D4(SCarve800E122CSinglePtr* self, void* ptr);
extern "C" void* fn_800E122C(CCarve800E122CTokenVector* self, int flag);

// 0x800E12D4, 0x48: retail's `single_ptr<T>::operator=(T*)` for a T this range never names.
// The `mr r3,r30` in the epilogue is that `return *this`, and without it the frame is 4 bytes
// short.  The `li r4,1` is the *deleting* flag, so the old pointee is destroyed **and** released -
// this is `delete self->mPtr`, spelled through retail's own name for the destructor.
extern "C" void* fn_800E12D4(SCarve800E122CSinglePtr* self, void* ptr) {
  fn_800E131C(self->x0_ptr, 1);
  self->x0_ptr = ptr;
  return self;
}

// 0x800E122C, 0xA8: retail's `rstl::vector<TToken<CTexture>, rstl::rmemory_allocator>::~vector()`
// in the deleting-destructor calling convention.  The `destroy(begin(), end())` loop and the
// `deallocate` are the header's own `~vector()` body (`include/rstl/vector.hpp:139-142`); the
// `if (self)` guard, the `extsh.`-tested flag and the `CMemory::Free(self)` are MWCC's deleting
// wrapper, which the compiler emits for a destructor and which an ordinary `extern "C"` function
// has to be given by hand - `src/MetroidPrime/Carve800E10EC.cpp:58-68` is the same shape and that
// unit is 100.00%.
extern "C" void* fn_800E122C(CCarve800E122CTokenVector* self, int flag) {
  if (self != nullptr) {
    rstl::destroy(self->begin(), self->end());
    self->mAllocator.deallocate(self->mItems);
    if (static_cast< short >(flag) > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}
