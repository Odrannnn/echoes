/**
 * `CGameState::CGameState()` - retail `fn_801449C8`, .text:0x801449C8, `size:0x2E4` = 740 bytes.
 *
 * The default constructor. `CGameGlobalObjects::CGameGlobalObjects` allocates 752 bytes at
 * 0x800084D0 and calls this at 0x800084DC, and the result is what 0x80008548 stores into
 * `gpGameState` - the global `CGameArchitectureSupport`'s constructor dereferences at
 * 0x800081A4 with no null test (see `src/MetroidPrime/PortBoot.cpp`).
 *
 * Everything up to the +0x204 member is the same straight-line code as the stream constructor
 * `fn_80144140` (`CGameStateStreamCtor.cpp`, whose header explains the three things that are not
 * logic: the merged `.rodata` operand `lbl_803A9208`, the two named `.sdata2` constants and the
 * C-linkage callees). After it this one differs:
 *
 *   - 0x80144B70-0x80144BA8: three one-bit fields of the byte at +0x2EC, each its own
 *     `lbz`/`rlwimi`/`stb` - bit 7 cleared, bit 6 set, bit 5 cleared.
 *   - 0x80144BAC-0x80144C4C: four `rc_ptr<CPlayerState>(new CPlayerState(i, nullptr))`
 *     temporaries, each `push_back`-ed into the reserved vector whose count is +0x18 and whose
 *     8-byte slots start at +0x1C (the `addic.` is placement `new`'s null test), and then
 *     released through `fn_8000934C`.
 *   - 0x80144C50-0x80144C84: `if (gpMemoryCard) fn_801440C0(this)`, three calls of
 *     `fn_80142DD4(this, i)`, and `fn_80142CF8(this)`.
 *
 * **Not in `files.cmake`, measured.** Listing it takes `tools/link_check.sh`'s undefined count from
 * 326 to 337 and closes nothing, because no port code calls it yet (`CGameGlobalObjects`'
 * constructor in `src/MetroidPrime/main.cpp` is a stub). The eleven are in
 * `tools/check_files_cmake.py`'s entry for this file. When it is listed, `SGameStateFlags` below
 * needs a host spelling too: mwcceppc allocates bitfields from the most significant bit, a
 * little-endian host from the least, so `b7` is 0x80 here and 0x01 there.
 */

// `#define _CMEMORY` keeps `Kyoto/Alloc/CMemory.hpp`'s own throwing `operator new` out, so the
// one below is the only `new` in this translation unit - see `CGameStateStreamCtor.cpp`.
#define _CMEMORY

#include "types.h"

#include "MetroidPrime/Player/CGameState.hpp"

#include "MetroidPrime/CMemoryCard.hpp"
#include "MetroidPrime/CWorldState.hpp"
#include "MetroidPrime/Player/CGameOptions.hpp"
#include "MetroidPrime/Player/CHintOptions.hpp"
#include "MetroidPrime/Player/CPersistentOptions.hpp"
#include "MetroidPrime/Player/CPlayerState.hpp"

#include "rstl/reserved_vector.hpp"

#if !defined(__MWERKS__)
#include <new>
#endif

// The merged `.rodata` pool object every `new` in this function passes as its file operand.
extern "C" const char lbl_803A9208[];

// `lbl_8041C1A8` = 359999.0 and `lbl_8041C1B8` = 100.0f, retail's named `.sdata2` objects.
extern "C" const double lbl_8041C1A8;
extern "C" const float lbl_8041C1B8;

#if defined(__MWERKS__) || defined(CLANGD)
void* operator new(size_t sz, const char*, const char*);
inline void* operator new(size_t sz) { return operator new(sz, lbl_803A9208, nullptr); }
inline void* operator new(size_t n, void* ptr) { return ptr; }
#endif

// `CGameOptions()` is called by retail's name under MWERKS, because placement `new` costs a null
// test retail does not have. The host mangles the member to `_ZN12CGameOptionsC1Ev`, so there the
// call is the ordinary C++ one - the same split as `src/MetroidPrime/CWorldStateCtor.cpp`.
#if defined(__MWERKS__)
extern "C" void __ct__12CGameOptionsFv(CGameOptions* self);
#define CTOR_GAMEOPTIONS( obj ) __ct__12CGameOptionsFv(obj)
#else
#define CTOR_GAMEOPTIONS( obj ) new (obj) CGameOptions()
#endif

extern "C" {
// CHintOptions' default constructor (`CHintOptionsCtor.cpp`, Matching).
void fn_80180738(CHintOptions* self);
// The +0x54 block's constructor.
void fn_80145950(SGameStateCardOpts* self);
// CPersistentOptions' constructor - see `CGameStateStreamCtor.cpp` for why it has no source.
void fn_80146154(CPersistentOptions* self, int flag);
// The two 0x34 blocks' fill constructor (`CGameStateSlotsCtor.cpp`, Matching).
void fn_80144924(SGameStateSlots* self, int n, const SGameStateBlock* src);
// The 16-byte block element's destructor, `(self, -1)`.
void fn_80004A4C(void* self, int flag);
// `new(12)`'s constructor; returns `this`, which retail stores.
void* fn_80193E08(void* self);
// The +0x1A0 block's constructor.
void fn_80007040(SGameStateWorlds* self);
// The +0x204 block's constructor.
void fn_80009DBC(SGameStateMemcard* self, int flag);
// `rc_ptr<CPlayerState>::ReleaseData`, on the rc_ptr itself.
void fn_8000934C(void* self);
// The three calls after the player loop, all on `this`.
void fn_801440C0(CGameState* self);
void fn_80142DD4(CGameState* self, int idx);
void fn_80142CF8(CGameState* self);
} // extern "C"

// The +0x19C member's 12-byte object, only for its size.
class SGameStateCtorMarker {
public:
  u32 x0, x4, x8;
};
CHECK_SIZEOF(SGameStateCtorMarker, 0xc)

// The byte at +0x2EC is three one-bit fields and five more bits: retail's three separate
// `rlwimi` + `stb` are what mwcceppc emits for bitfield stores, not for `|=` on a `u8`.
struct SGameStateFlags {
  u8 b7 : 1;
  u8 b6 : 1;
  u8 b5 : 1;
  u8 rest : 5;
};

// `rstl::rc_ptr<CPlayerState>`, as the two words retail keeps on the stack at 8(r1)/12(r1).
// The copy constructor is the `push_back`'s: both words, then `++*refCount` through the copy.
// There is no destructor: a declared one is emitted as a weak out-of-line
// `__dt__15SPlayerStateRefFv` (with a `__dl__FPv` reference) that retail's object does not
// have, so the release is the explicit `fn_8000934C` call at the end of the loop body.
struct SPlayerStateRef {
  CPlayerState* x0_ptr;
  uint* x4_refCount;

  SPlayerStateRef(const SPlayerStateRef& other)
  : x0_ptr(other.x0_ptr), x4_refCount(other.x4_refCount) {
    ++*x4_refCount;
  }
  SPlayerStateRef(CPlayerState* ptr) : x0_ptr(ptr) {
    uint* refCount = static_cast< uint* >(::operator new(4));
    if (refCount) {
      *refCount = 1;
    }
    x4_refCount = refCount;
  }
};

// The `x18_playerStates` count and `x01c_players` slots are a
// `rstl::reserved_vector< rc_ptr< CPlayerState >, 4 >`; the `addic.` is `construct`'s null test.
// The header keeps them as a plain count and array, so the vector is an overlay here.
typedef rstl::reserved_vector< SPlayerStateRef, 4 > SPlayerStateVector;

// Named for the address and given C linkage because retail's symbol table has no name for it;
// a C++ `CGameState::CGameState()` would mangle to `__ct__9CGameStateFv` and objdiff would have
// nothing to pair it with. Returns `this`, as retail does (`mr r3,r29` before the epilogue).
extern "C" CGameState* fn_801449C8(CGameState* self) {
  self->x00_unk = -1;
  self->x04_unk = -1;
  self->x08_reserve.x04_count = 0;
  self->x08_reserve.x08_cap = 0;
  self->x08_reserve.x0c_data = nullptr;
  self->x18_playerStates = 0;

  CWorldState* worldState = static_cast< CWorldState* >(::operator new(sizeof(CWorldState)));
  // `new T()`'s result is the constructor's return value (`mr r0,r3` after the call), and the
  // header declares `fn_8015C34C` as returning void, hence the cast.
  if (worldState) {
    worldState = reinterpret_cast< CWorldState* (*)(CWorldState*) >(fn_8015C34C)(worldState);
  }
  self->x3c_worldState = worldState;

  uint* refCount = static_cast< uint* >(::operator new(4));
  if (refCount) {
    *refCount = 1;
  }
  self->x40_refCount = refCount;

  self->x48_time = lbl_8041C1A8;
  self->x50_unk = lbl_8041C1B8;
  fn_80145950(&self->x54);

  CTOR_GAMEOPTIONS(&self->gameOptions);
  fn_80180738(&self->hintOptions);
  fn_80146154(&self->persistentOptions, 1);

  self->persistentOptions.x1c = 0;
  self->persistentOptions.x20 = 0;
  self->persistentOptions.x24 = 0;
  self->cardSerial = 0;
  SGameStateBlock block110;
  block110.x04_count = 0;
  block110.x08_cap = 0;
  block110.x0c_data = nullptr;
  fn_80144924(&self->x110, 3, &block110);
  fn_80004A4C(&block110, -1);

  SGameStateBlock block144;
  block144.x04_count = 0;
  block144.x08_cap = 0;
  block144.x0c_data = nullptr;
  fn_80144924(&self->x144, 3, &block144);
  fn_80004A4C(&block144, -1);

  self->x178.x04_count = 0;
  self->x178.x08_cap = 0;
  self->x178.x0c_data = nullptr;
  self->x188.x04_count = 0;
  self->x188.x08_cap = 0;
  self->x188.x0c_data = nullptr;
  // Written as a conditional, not an `if`: the `if` spelling (and the stream constructor's
  // void-returning one) routes the pointer through r0 or r28 where retail keeps it in r4.
  void* marker = ::operator new(sizeof(SGameStateCtorMarker));
  marker = marker ? fn_80193E08(marker) : marker;
  self->x198_ptrSet = marker != nullptr;
  self->x19c_ptr = marker;

  fn_80007040(&self->x1a0);

  self->x1f4.x04_count = 0;
  self->x1f4.x08_cap = 0;
  self->x1f4.x0c_data = nullptr;
  fn_80009DBC(&self->x204, 0);

  SGameStateFlags& flags = reinterpret_cast< SGameStateFlags& >(self->x2ec_flags);
  flags.b7 = false;
  flags.b6 = true;
  flags.b5 = false;

  SPlayerStateVector& players = reinterpret_cast< SPlayerStateVector& >(self->x18_playerStates);
  // `push_back` written out: retail forms the slot address from the vector (r31 = this+0x18)
  // but increments the count through `this` (`lwz r4,24(r29)`), which `push_back` itself -
  // both through r31 - does not reproduce.
  for (int i = 0; i < 4; ++i) {
    SPlayerStateRef ref(new CPlayerState(i, nullptr));
    rstl::construct(players.data() + players.size(), ref);
    ++self->x18_playerStates;
    fn_8000934C(&ref);
  }

  if (gpMemoryCard) {
    fn_801440C0(self);
  }
  for (int i = 0; i < 3; ++i) {
    fn_80142DD4(self, i);
  }
  fn_80142CF8(self);
  return self;
}
