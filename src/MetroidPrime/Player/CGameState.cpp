#include "MetroidPrime/Player/CGameState.hpp"

#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/Basics/CBasics.hpp"
#include "Kyoto/CResFactory.hpp"
#include "Kyoto/Math/CMath.hpp"
#include "Kyoto/Streams/CBitStreamReader.hpp"
#include "Kyoto/Streams/CBitStreamWriter.hpp"
#include "Kyoto/Streams/CMemoryInStream.hpp"
#include "Kyoto/Streams/CMemoryStreamOut.hpp"
#include "MetroidPrime/CMain.hpp"
#include "MetroidPrime/CMemoryCard.hpp"
#include "MetroidPrime/CRelayTracker.hpp"
#include "MetroidPrime/CWorldLayerState.hpp"
#include "MetroidPrime/Player/CGMCoin.hpp"
#include "MetroidPrime/Player/CGMDeathMatch.hpp"
#include "MetroidPrime/Player/CFrontEndGameMode.hpp"
#include "MetroidPrime/Player/CGMSinglePlayer.hpp"
#include "MetroidPrime/Player/CGameStateBlocks.hpp"
#include "MetroidPrime/Player/CPersistentOptionsMap.hpp"
#include "MetroidPrime/Player/CGameMode.hpp"
#include "MetroidPrime/Player/CPlayerState.hpp"
#include "MetroidPrime/Player/CWorldTransManager.hpp"
#include "MetroidPrime/Player/SPersistentOptionsValue.hpp"
#include "MetroidPrime/Tweaks/CTweakGame.hpp"

#include "dolphin/os.h"
#include "rstl/algorithm.hpp"
#include "rstl/math.hpp"
#include "rstl/red_black_tree.hpp"

#include <stdio.h>
#include <string.h>

// The 16-byte SGameStateBlock helpers (see CGameStateBlocks.hpp). Defined below in retail order,
// ported from the pre-sync carves (63aba15); fn_801465EC (reserve) and fn_80004D5C (copy) live
// elsewhere.
extern "C" void fn_80004BEC(SGameStateSlots* self);
extern "C" void fn_80004D5C(SGameStateBlock* self, const SGameStateBlock* src);
extern "C" void fn_80142914(SGameStateBlock* self);
extern "C" void* fn_801429AC(void* begin, void* end, void* dst);
extern "C" void fn_80142A10(SGameStateBlock* self, const SGameStateBlock* src);
extern "C" void fn_801465EC(SGameStateBlock* self, int size);

// Guessed name. Layer-name prefixes select which game mode owns each layer.
//
// **Declared here, immediately before its only user, and not at the top of the file.** The three
// literals land in this unit's own `.rodata` pool, and a literal's offset is part of the
// instruction that loads it, so the pool order is part of the match. Retail's pool puts
// `"InitialWorld"` at +0x07 and `"Samus01"`/`"Coins"` at +0x1B8/+0x1C0, so `fn_80143E88` is
// retail's *first* user of a string literal and this table is nearly its last. Moving the table
// down the file is what reproduces that: retail's `__sinit_CGameState_cpp` (0x80146874) writes
// the table at runtime out of `lbl_803A9208 + 42 / +440 / +448` - the same three offsets, and
// +42 is shared with `fn_80143E88`'s own `"Deathmatch"`.

// The 36-byte-element block helpers, and the two loops that walk one. `SGameStateBlock`'s
// `x0c_data` is the base pointer and `x04_count` the count for this instance, because
// `fn_801426E0` (0x801426E0) indexes it as `data + count * 36`.
//
// `fn_80004458` (0x80004458) is retail code no port unit claims, so it is called through a
// declaration rather than an inlined copy. `fn_80142760` (0x80142760), the 36-byte element's own
// copy, is declared and defined below, in retail offset order.
extern "C" void fn_80004458(void* elem);
extern "C" void fn_80142718(void* elem, const void* src);

extern "C" void fn_801435D4(void* elem);
extern "C" void fn_801467C0(uchar* begin, uchar* end);

// The move half of `fn_801466F4`'s grow: it copy-constructs each 36-byte element from the old
// range into the new buffer and returns the new end. `begin` and `end` arrive by address -
// `fn_801466F4` materialises both as a pair of two-word argument temporaries on its own stack
// (8(r1)/12(r1) for `end`, 16(r1)/20(r1) for `begin`, 0x80146734-0x80146758).
//
// **The parameters are a one-word class, and that is what makes the temporaries and the order
// the two argument addresses are formed in come out as retail's.** `SStateIter` stands in for
// the vector iterator these two arguments really are: this tree's own
// `rstl::vector<T>::reserve` (`include/rstl/vector.hpp`, `uninitialized_copy(begin(), end(),
// newData)`, both iterators passed by value) builds the same 8(r1)/12(r1) and 16(r1)/20(r1)
// pairs - measured on the 100% `reserve__...vector<pair<TEditorId,bool>>` instantiation in
// `main/MetroidPrime/CMapWorldInfo`. mwcceppc evaluates the arguments right to left and gives
// each one a pair of words, and the two `addi`s land in retail's order (0x80146734
// `addi r3,r1,20` for `begin` first, 0x80146740 `addi r4,r1,12` for `end` last). With the
// previous spelling, `void* range[4]` and `fn_8014680C(&range[3], &range[1], buffer)`, the
// same two addresses are formed in the opposite order and the function measures 90.56% at the
// same 172 bytes.
//
// **The loop bound is re-read from `end` on every iteration:** retail's guard is `lwz r0,0(r29)`
// / `cmplw r31,r0` (0x80146848-0x8014684C) with `r29` the address of the `end` parameter kept
// live across the loop. This spelling reloads it (104 bytes, byte-identical to retail); the
// earlier `void* const* begin, void* const* end` spelling hoisted the load above the loop and
// emitted `cmplw r29,r31` against a register, 100 bytes and 92.69%.
//
// The two cursors are declared input-first for the same reason: `in` is defined before `out`, and
// mwcceppc then reserves r31 for `in` and r30 for `out` and can place the `lwz r31,0(r3)` right
// after the `stw r31,28(r1)` prologue spill, which is retail's order. `out` first gives the
// same 104 bytes with the five prologue instructions rotated (88.46%), and a `while` loop or an
// index-based range loses the reload entirely.
struct SStateIter {
  void* mCur;
  SStateIter(void* cur) : mCur(cur) {}
};

extern "C" void* fn_8014680C(SStateIter begin, SStateIter end, void* dst) {
  uchar* in = static_cast< uchar* >(begin.mCur);
  uchar* out = static_cast< uchar* >(dst);
  for (; in != static_cast< uchar* >(end.mCur); in += 36, out += 36) {
    fn_80142718(out, in);
  }
  return out;
}

extern "C" void fn_801467C0(uchar* begin, uchar* end) {
  for (uchar* p = begin; p != end; p += 36) {
    fn_801435D4(p);
  }
}

extern "C" void fn_801467A0(uchar* begin, uchar* end) { fn_801467C0(begin, end); }

// The 36-byte block's `reserve` (retail 0x801466F4). The new buffer is filled by the
// `fn_8014680C` above from the two `SStateIter` arguments this function materialises on its own
// stack (0x80146734-0x80146758: the old end twice at 8(r1)/12(r1), the old begin twice at
// 16(r1)/20(r1)), the old elements are then destroyed with `fn_801467A0` and the old block
// handed back to `CMemory::Free`. The guard is a **signed** `cmpw` - `n <= x08_cap` skips the
// grow entirely, with no allocation.
//
// Both ends are re-read from `self` after the copy and again for the destroy rather than kept in
// locals across the `fn_8014680C` call: holding them costs a fifth live register, `stmw r27,28(r1)`
// and a 144-byte frame (66.63%).
extern "C" void fn_801466F4(SGameStateBlock* self, int capacity) {
  if (capacity <= static_cast< int >(self->x08_cap)) {
    return;
  }

  uchar* const buffer =
      static_cast< uchar* >(rstl::rmemory_allocator::allocate(capacity * 36));
  fn_8014680C(SStateIter(self->x0c_data),
              SStateIter(static_cast< uchar* >(self->x0c_data) + self->x04_count * 36), buffer);
  fn_801467A0(static_cast< uchar* >(self->x0c_data),
              static_cast< uchar* >(self->x0c_data) + self->x04_count * 36);
  CMemory::Free(self->x0c_data);
  self->x0c_data = buffer;
  self->x08_cap = static_cast< u32 >(capacity);
}

// The 12-byte block's element copy (retail 0x801465A8). One word and two floats per element.
// `begin` and `end` are loaded once, before the loop (0x801465A8 / 0x801465AC), and `dst` is
// tested inside the loop body (0x801465B4), so a null destination still walks the range and
// returns the new end.
extern "C" void* fn_801465A8(void* const* begin, void* const* end, void* dst) {
  uchar* out = static_cast< uchar* >(dst);
  for (uchar* in = static_cast< uchar* >( *begin ); in != static_cast< uchar* >( *end );
       in += 12, out += 12) {
    if (out != nullptr) {
      u32* to = reinterpret_cast< u32* >(out);
      const u32* from = reinterpret_cast< const u32* >(in);
      to[0] = from[0];
      reinterpret_cast< float* >(to)[1] = reinterpret_cast< const float* >(from)[1];
      reinterpret_cast< float* >(to)[2] = reinterpret_cast< const float* >(from)[2];
    }
  }
  return out;
}

// Retail 0x80146338, 0x1B8 = 440 bytes: the option map's rbtree node insert (see
// `CPersistentOptionsMapInsert.cpp`, its only caller). `fn_80008CE0` is the node constructor.
//
// The three results are file statics, not literals: retail loads `true`, `false`, `true` from
// three consecutive `.sdata` bytes (`-31135/-31134/-31133(r13)`), which a literal `true` would
// turn into `li r0,1` (93.82% measured). Guest layout throughout (`SMap` is the DOL's tree), so
// the port, which keeps the option map in a host `rstl::map` (`PortCPersistentOptionsMap.cpp`),
// does not compile it and does not need `fn_80008CE0` / `fn_800273B4` defined.
#ifndef TARGET_PC
extern "C" SMapNode* fn_80008CE0(SMap* tree, SMapNode* left, SMapNode* right, SMapNode* parent,
                                 u32 colour, const SMapEntry* entry);

static bool sInsertedFirst = true;
static bool sFound = false;
static bool sInserted = true;

extern "C" void fn_80146338(SMapInsert* out, SMap* tree, SMapNode* root, const SMapEntry* entry) {
  if (root == nullptr) {
    tree->x10_root = fn_80008CE0(tree, nullptr, nullptr, nullptr, 0, entry);
    ++tree->x04_count;
    tree->x08_header = tree->x10_root;
    tree->x0c_rightmost = tree->x10_root;
    out->x0_iter.x00_node = tree->x10_root;
    out->x0_iter.x04_end = reinterpret_cast< SMapNode* >(&tree->x08_header);
    out->x8_inserted = sInsertedFirst;
    return;
  }
  SMapNode* cur = root;
  SMapNode* created = nullptr;
  while (created == nullptr) {
    bool less = fn_800273B4(&tree->x01_compare_this, entry->key, cur->x10_key);
    if (!less && !fn_800273B4(&tree->x01_compare_this, cur->x10_key, entry->key)) {
      out->x0_iter.x00_node = cur;
      out->x0_iter.x04_end = reinterpret_cast< SMapNode* >(&tree->x08_header);
      out->x8_inserted = sFound;
      return;
    }
    if (less) {
      if (cur->x00_left == nullptr) {
        created = fn_80008CE0(tree, nullptr, nullptr, cur, 1, entry);
        cur->x00_left = created;
        if (cur == tree->x08_header) {
          tree->x08_header = created;
        }
      } else {
        cur = cur->x00_left;
      }
    } else {
      if (cur->x04_right == nullptr) {
        created = fn_80008CE0(tree, nullptr, nullptr, cur, 1, entry);
        cur->x04_right = created;
        if (cur == tree->x0c_rightmost) {
          tree->x0c_rightmost = created;
        }
      } else {
        cur = cur->x04_right;
      }
    }
  }
  ++tree->x04_count;
  rstl::rbtree_rebalance(&tree->x08_header, created);
  out->x0_iter.x00_node = created;
  out->x0_iter.x04_end = reinterpret_cast< SMapNode* >(&tree->x08_header);
  out->x8_inserted = sInserted;
}
#endif // TARGET_PC

uint CEnvironmentVariable::GetBitCount(uint value) {
  uint count = 0;
  for (; value != 0; value >>= 1) {
    ++count;
  }
  return count;
}

CEnvironmentVariable::CEnvironmentVariable(int minimum, int maximum, int value)
: mMin(minimum), mMax(maximum), mValue(value) {
  ClampToMinMax();
}

// Retail reads back the members the initialiser list has just stored, not the parameters, which
// keeps the frame at retail's 16 bytes (0x801462E4).
CEnvironmentVariable::CEnvironmentVariable(int minimum, int maximum, CBitStreamReader& in)
: mMin(minimum), mMax(maximum), mValue(mMin + in.ReadBits(GetBitCount(mMax - mMin))) {
  ClampToMinMax();
}

// Retail forms the difference into a local before calling GetBitCount, holding it in r31 across
// the call (0x80146118).
void CEnvironmentVariable::PutTo(CBitStreamWriter& out) const {
  const int value = mValue - mMin;
  out.WriteBits(value, GetBitCount(mMax - mMin));
}

void CEnvironmentVariable::Set(int value) {
  mValue = value;
  ClampToMinMax();
}

void CEnvironmentVariable::ClampToMinMax() {
  if (mValue < mMin || mValue > mMax) {
    mValue = CMath::Clamp(mMin, mValue, mMax);
  }
}

CGameStateEnvVarManager::CGameStateEnvVarManager(EVariableScope scope) : mScope(scope) {
  LoadFields();
}

CGameStateEnvVarManager::CGameStateEnvVarManager(EVariableScope scope, CBitStreamReader& in)
: mScope(scope) {
  LoadFields();
  InitializeMemoryState();
  for (rstl::map< rstl::string, CEnvironmentVariable >::iterator it = mVariables.begin();
       it != mVariables.end(); ++it) {
    it->second = CEnvironmentVariable(it->second.GetMinimum(), it->second.GetMaximum(), in);
  }
}

// `fn_80145BDC` (0x80145BDC) and the two wrappers around it are one out-of-line copy of
// `red_black_tree<rstl::string, rstl::pair<rstl::string, CEnvironmentVariable>, 0,
// select1st<...>, rstl::less<rstl::string>, rmemory_allocator>::find_node` and the `find` that
// wraps it, for the `mVariables` map of `CGameStateEnvVarManager`
// (`include/MetroidPrime/Player/CGameStateEnvVarManager.hpp:25`). Retail keeps the walk out of
// line; our build inlines the map's own copy of the template into its callers, so both shapes
// are spelled out here instead.
//
// `mVariables` is `this + 0`, so the members retail reads sit where `red_black_tree` puts them:
// the comparator `mCmp` at `this + 1` (`addi r3,r28,1`, 0x80145C10), the header at `this + 8`
// (the `addi r0,r31,8` the wrappers pair the iterator with, 0x80145BBC) and the root
// `mHeader.mRootNode` at `this + 0x10` (`lwz r31,16(r3)`, 0x80145BEC). A node is
// `mLeft`/`mRight`/`mParent`/`mColor` then the `rstl::pair`, so its key is `node + 0x10`.
namespace {

// The node `red_black_tree` allocates: `mLeft`/`mRight`/`mParent`/`mColor`, then the
// `rstl::pair` the tree stores, whose `first` is the `rstl::string` key at `node + 0x10`.
struct SGameStateVarNode {
  SGameStateVarNode* mLeft;
  SGameStateVarNode* mRight;
  SGameStateVarNode* mParent;
  u32 mColor;
  rstl::pair< rstl::string, CEnvironmentVariable > mValue;

  const rstl::string& key() const { return mValue.first; }
};

// The three words of `red_black_tree::header`. Spelled out because `red_black_tree::mHeader` is
// private and the wrappers need the header's *address*, not the value stored there.
struct SGameStateVarHeader {
  SGameStateVarNode* mLeftmost;
  SGameStateVarNode* mRightmost;
  SGameStateVarNode* mRootNode;
};

// The map laid out as `red_black_tree` lays it out: `mSelector` and `mAllocator` are empty
// classes, so `mCmp` follows `mSelector` at `this + 1`, `mCount` is the first word-aligned
// member and the header lands after it at `this + 8`.
struct SGameStateVarTree {
  u8 mSelector;
  rstl::less< rstl::string > mCmp;
  u8 mAllocator;
  int mCount;
  SGameStateVarHeader mHeader;
};

// The two words of `red_black_tree::const_iterator`: the node, and the header it walks from.
// Constructed in the return statement rather than filled in field by field - building a local
// first drops the constructor call, and with it retail's `addi`/`stw` pair.
struct SGameStateVarIter {
  SGameStateVarNode* mNode;
  const SGameStateVarHeader* mHeader;
  SGameStateVarIter(SGameStateVarNode* node, const SGameStateVarHeader* header)
  : mNode(node), mHeader(header) {}
};

} // namespace

// The walk itself: unnamed in the symbol table and claimed by no unit, so the two wrappers call
// it through this declaration. It is `find_node`, not `find_lower_bound` - it re-tests the
// needle against the found node's key with a *second* comparator call (0x80145C54) and returns
// null unless the two are equal, which `find_lower_bound` would not do.
extern "C" SGameStateVarNode* fn_80145BDC(const SGameStateVarTree& self, const rstl::string& key);

// `fn_8014601C` (0x8014601C) - that walk reached through a `map::find`, which returns the
// eight-byte `const_iterator` **by value**: `r3` is the hidden return pointer, `r4` the tree and
// `r5` the key. Its body is byte-for-byte the one at `fn_80145B90` (0x80145B90), the other copy
// of the same wrapper - the two differ only in the `bl` (0x80146040 is a relocated long branch
// where 0x80145BB4 is a short one into the same callee).
extern "C" SGameStateVarIter fn_8014601C(void* tree, const rstl::string& key) {
  const SGameStateVarTree& self = *static_cast< const SGameStateVarTree* >(tree);
  return SGameStateVarIter(fn_80145BDC(self, key), &self.mHeader);
}

CEnvironmentVariable* CGameStateEnvVarManager::FindEnvironmentVariable(const char* name) {
  rstl::map< rstl::string, CEnvironmentVariable >::iterator it =
      mVariables.find(rstl::string_l(name));
  // Retail tests the end iterator with `!=` and selects the second through a ternary
  // (0x80145C74). `end` is bound to a local declared *after* the find: binding it before puts
  // `addi rX,this,12` in the prologue and costs r31.
  rstl::map< rstl::string, CEnvironmentVariable >::iterator end = mVariables.end();
  return it != end ? &it->second : nullptr;
}

// **Declared between `FindEnvironmentVariable` (0x80145E24) and `AddVariable` (0x801442CC) because
// that is where retail's offset order puts it** - `check_decl_order.py` pairs by name, and the
// block landed after `PutTo` at first and put the whole tail of the unit 7 slots out of place.
extern "C" void fn_80145ACC(CPersistentOptions* self, const rstl::string& name,
                            const SPersistentOptionsValue& value);

// `LoadFields` - retail `.text:0x80145C98`, `size:0x2F4` = 756 bytes, 0x80145C98..0x80145F8C,
// named `fn_80145C98` until 2026-10-01. Retail's symbol table has no name for it; this is the
// member function the header declares and that **both** constructors call, and its body is what
// `0x80146154` (the constructor that stores the scope word this function branches on) branches
// into. Written under its own class's name so the port's `_ZN23CGameStateEnvVarManager10LoadFieldsEv`
// resolves here: with the body under the C name, every caller of the constructor went to
// `PortReachStubs.cpp`'s print-a-line stand-in and the eleven rows never ran
// (`build-boot-probe/run.log`, `[reach-stub 0006]`, twice per boot). `config/G2ME01/symbols.txt`
// carries the same name for 0x80145C98, so objdiff still pairs it and the unit's matched count
// does not move.
//
// Eleven straight-line statements, not a loop over a table: a loop gives mwcceppc a `ctr` and a
// body to unroll, and retail has neither.
//
// **The eleven names are literals here and were `lbl_803A9208 + K` in the carve.** This unit owns
// the pool (`splits.txt` claims `.rodata 0x803A9208..0x803A93D0`), so the names have to be its
// own literals for `lis/addi/addi K` to reproduce; that is also what puts them at +213..+438,
// which is where retail keeps them and where `__sinit_CGameState_cpp`'s `+440`/`+448` ("Samus01",
// "Coins") then land.
//
// The three numbers are the value's `{lo, hi, default}`; every row has `lo == 0`, so
// `SPersistentOptionsValue`'s clamp is a no-op for all of them, but retail passes them.
void CGameStateEnvVarManager::LoadFields() {
  // `fn_80145ACC` is retail's out-of-line map insert, spelled with `CPersistentOptions*` because
  // that is the class whose source defines it (`CPersistentOptionsMapInsert.cpp`). It touches only
  // the base's `mVariables`, which starts at the same offset either way, and this is the base's own
  // member, so the downcast is the same pointer. Retail passes nothing: `self` is r31 from the
  // prologue and each call is `mr r3,r31` (0x80145CC0). **Bind it before the branch** - after it,
  // mwcceppc hoists `mr r31,r3` to its first use and the two instructions swap.
  CPersistentOptions* self = reinterpret_cast< CPersistentOptions* >(this);

  // +0x00 is the constructor's scope word: the system-wide object gets the table, the per-game
  // one (built from the bit stream) does not. Read through `int*` because it is a private member.
  if (reinterpret_cast< const int* >(self)[0] != 0) {
    return;
  }

  fn_80145ACC(self, rstl::string_l("FreezeInstructionsFirstPerson"), SPersistentOptionsValue(0, 3, 0));
  fn_80145ACC(self, rstl::string_l("FreezeInstructionsMorphBall"), SPersistentOptionsValue(0, 3, 0));
  fn_80145ACC(self, rstl::string_l("PowerbombPickupMessages"), SPersistentOptionsValue(0, 1, 0));
  fn_80145ACC(self, rstl::string_l("PercentScans"), SPersistentOptionsValue(0, 100, 0));
  fn_80145ACC(self, rstl::string_l("NormalModeCompleted"), SPersistentOptionsValue(0, 1, 0));
  fn_80145ACC(self, rstl::string_l("HardModeCompleted"), SPersistentOptionsValue(0, 1, 0));
  fn_80145ACC(self, rstl::string_l("AllPickupsFound"), SPersistentOptionsValue(0, 1, 0));
  fn_80145ACC(self, rstl::string_l("AutoMapperPaneMode"), SPersistentOptionsValue(0, 2, 1));
  fn_80145ACC(self, rstl::string_l("LogbookLegendVisible"), SPersistentOptionsValue(0, 1, 1));
  fn_80145ACC(self, rstl::string_l("IngAttachedWarningCount"), SPersistentOptionsValue(0, 3, 0));
  fn_80145ACC(self, rstl::string_l("SeenIntroText"), SPersistentOptionsValue(0, 1, 0));
}

// `fn_80145BDC` (0x80145BDC) and `fn_80145B90` (0x80145B90) - the walk and the `find` that wraps
// it. They are defined here, between `LoadFields` (0x80145C98) and `AddVariable` (0x80145B0C),
// so that this unit's definitions run **descending by retail offset**: 0x8014601C, 0x80145F8C,
// 0x80145C98, 0x80145BDC, 0x80145B90, 0x80145B0C, ... Ascending would leave the module's bytes
// permuted with objdiff still at 100%, and only `flip_test.sh` sees that. The `fn_80145BDC`
// declaration near the top of this group emits no code and so does not affect the order.
extern "C" SGameStateVarNode* fn_80145BDC(const SGameStateVarTree& self, const rstl::string& key) {
  // The tree arrives **by reference**. With a pointer parameter mwcceppc sinks the
  // `lwz r31,16(r3)` root load to the end of the prologue; by reference it emits it at +0x10,
  // straight after `stw r31,12(r1)` and before any other register is set up, which is where
  // retail has it (0x80145BEC).
  SGameStateVarNode* n = self.mHeader.mRootNode;
  SGameStateVarNode* needle = nullptr;
  while (n != nullptr) {
    if (!self.mCmp(n->key(), key)) {
      needle = n;
      n = n->mLeft;
    } else {
      n = n->mRight;
    }
  }
  return (needle == nullptr || self.mCmp(key, needle->key())) ? nullptr : needle;
}

extern "C" SGameStateVarIter fn_80145B90(void* tree, const rstl::string& key) {
  const SGameStateVarTree& self = *static_cast< const SGameStateVarTree* >(tree);
  return SGameStateVarIter(fn_80145BDC(self, key), &self.mHeader);
}

void CGameStateEnvVarManager::AddVariable(const rstl::string& name,
                                          const CEnvironmentVariable& variable) {
  // Same hoist as `FindEnvironmentVariable` (the end iterator is materialised into a local
  // declared after the find, which is what puts `addi r0,r29,12` where retail has it at
  // 0x80145B08) but the opposite branch polarity: retail tests `it == end` and only reaches
  // the insert when they are equal (0x80145B10/0x80145B20).
  // Both values must be captured as `const_iterator` to reproduce retail's instruction sequence.
  rstl::map< rstl::string, CEnvironmentVariable >::const_iterator it = mVariables.find(name);
  rstl::map< rstl::string, CEnvironmentVariable >::const_iterator end = mVariables.end();
  if (it == end) {
    mVariables.insert(rstl::pair< rstl::string, CEnvironmentVariable >(name, variable));
  }
}

void CGameStateEnvVarManager::InitializeMemoryState() {
  const rstl::vector< CMemoryCard::EnvironmentVariable >& variables =
      mScope == kVS_System ? gpMemoryCard->GetSystemVariables() : gpMemoryCard->GetGameVariables();
  for (rstl::vector< CMemoryCard::EnvironmentVariable >::const_iterator it = variables.begin();
       it != variables.end(); ++it) {
    AddVariable(it->mName, CEnvironmentVariable(it->mMinimum, it->mMaximum, it->mDefaultValue));
  }
}

void CGameStateEnvVarManager::PutTo(CBitStreamWriter& out) const {
  for (rstl::map< rstl::string, CEnvironmentVariable >::const_iterator it = mVariables.begin();
       it != mVariables.end(); ++it) {
    it->second.PutTo(out);
  }
}

CPersistentOptions::CPersistentOptions() : CGameStateEnvVarManager(kVS_System), mSaveIdx(0) {
  if (gpMemoryCard != nullptr) {
    InitializeMemoryState();
  }
}

CPersistentOptions::CPersistentOptions(CBitStreamReader& in)
: CGameStateEnvVarManager(kVS_Game), mSaveIdx(0) {
  in.ReadBits(32); // SYST
  mSaveIdx = in.ReadBits(2);

  const rstl::vector< CMemoryCard::MemoryWorld >& worlds = gpMemoryCard->GetMemoryWorlds();
  int cinematicCount = 0;
  for (rstl::vector< CMemoryCard::MemoryWorld >::const_iterator it = worlds.begin();
       it != worlds.end(); ++it) {
    TLockedToken< CWorldSaveGameInfo > saveWorld =
        gpSimplePool->GetObj(SObjectTag('SAVW', it->second.GetSaveWorldAssetId()));
    cinematicCount += saveWorld->GetCinematicCount();
  }

  rstl::vector< bool > cinematicStates(cinematicCount, false);
  for (int i = 0; i < cinematicCount; ++i) {
    cinematicStates[i] = in.ReadPackedBool();
  }

  int stateIdx = 0;
  for (rstl::vector< CMemoryCard::MemoryWorld >::const_iterator it = worlds.begin();
       it != worlds.end(); ++it) {
    TLockedToken< CWorldSaveGameInfo > saveWorld =
        gpSimplePool->GetObj(SObjectTag('SAVW', it->second.GetSaveWorldAssetId()));
    const rstl::vector< TEditorId >& cinematics = saveWorld->GetCinematics();
    for (int i = 0; i < cinematics.size(); ++i) {
      if (cinematicStates[stateIdx]) {
        SetCinematicState(rstl::pair< CAssetId, TEditorId >(it->first, cinematics[i]), true);
      }
      ++stateIdx;
    }
  }

  InitializeMemoryState();
  CGameStateEnvVarManager::operator=(CGameStateEnvVarManager(kVS_System, in));
  in.ReadBits(32); // SYND
}

void CPersistentOptions::InitializeMemoryState() {
  CGameStateEnvVarManager::InitializeMemoryState();
}

void CPersistentOptions::PutTo(CBitStreamWriter& out) const {
  out.WriteBits('SYST', 32);
  out.WriteBits(mSaveIdx, 2);

  const rstl::vector< CMemoryCard::MemoryWorld >& worlds = gpMemoryCard->GetMemoryWorlds();
  int cinematicCount = 0;
  for (rstl::vector< CMemoryCard::MemoryWorld >::const_iterator it = worlds.begin();
       it != worlds.end(); ++it) {
    TLockedToken< CWorldSaveGameInfo > saveWorld =
        gpSimplePool->GetObj(SObjectTag('SAVW', it->second.GetSaveWorldAssetId()));
    cinematicCount += saveWorld->GetCinematicCount();
  }

  rstl::vector< bool > cinematicStates;
  cinematicStates.reserve(cinematicCount);
  for (rstl::vector< CMemoryCard::MemoryWorld >::const_iterator it = worlds.begin();
       it != worlds.end(); ++it) {
    TLockedToken< CWorldSaveGameInfo > saveWorld =
        gpSimplePool->GetObj(SObjectTag('SAVW', it->second.GetSaveWorldAssetId()));
    for (int i = 0; i < saveWorld->GetCinematicCount(); ++i) {
      // `stbx` right after `mCount++` (0x80145564..0x8014557C): the reserved vector's append is
      // unchecked, like retail's.
      cinematicStates.push_back_unsafe(GetCinematicState(
          rstl::pair< CAssetId, TEditorId >(it->first, saveWorld->GetCinematics()[i])));
    }
  }
  for (int i = 0; i < cinematicCount; ++i) {
    out.WriteBits(cinematicStates[i] ? 1 : 0, 1);
  }

  CGameStateEnvVarManager::PutTo(out);
  out.WriteBits('SYND', 32);
}

CWorldState::CWorldState(CAssetId worldId)
: mWorldId(worldId)
, mAreaId(0)
, mRelayTracker(rs_new CRelayTracker)
, mMapWorldInfo(rs_new CMapWorldInfo)
, mDesiredAreaAssetId(kInvalidAssetId)
, mLayerState(rs_new CWorldLayerState) {}

CWorldState::CWorldState(CBitStreamReader& in, CAssetId worldId,
                         const CWorldSaveGameInfo& saveWorld)
: mWorldId(worldId)
, mAreaId(kInvalidAreaId)
, mRelayTracker(nullptr)
, mMapWorldInfo(nullptr)
, mDesiredAreaAssetId(kInvalidAssetId)
, mLayerState(nullptr) {
  mAreaId = TAreaId(in.ReadBits(32));
  mDesiredAreaAssetId = in.ReadBits(32);
  mRelayTracker = rs_new CRelayTracker(in, saveWorld);
  mMapWorldInfo = rs_new CMapWorldInfo(in, saveWorld, mWorldId);
  mLayerState = rs_new CWorldLayerState(in, saveWorld);
}

void CWorldState::PutTo(CBitStreamWriter& out, const CWorldSaveGameInfo& saveWorld) const {
  out.WriteBits(mAreaId.Value(), 32);
  out.WriteBits(mDesiredAreaAssetId, 32);
  mRelayTracker->PutTo(out, saveWorld);
  mMapWorldInfo->PutTo(out, saveWorld, mWorldId);
  mLayerState->PutTo(out, saveWorld);
}

CAssetId CWorldState::GetWorldAssetId() const { return mWorldId; }

rstl::ncrc_ptr< CRelayTracker >& CWorldState::RelayTracker() { return mRelayTracker; }

rstl::ncrc_ptr< CMapWorldInfo >& CWorldState::MapWorldInfo() { return mMapWorldInfo; }

rstl::rc_ptr< CMapWorldInfo > CWorldState::GetMapWorldInfo() const { return mMapWorldInfo; }

TAreaId CWorldState::GetCurrentArea() const { return mAreaId; }

void CWorldState::SetAreaId(TAreaId areaId) { mAreaId = areaId; }

CAssetId CWorldState::GetDesiredAreaAssetId() const { return mDesiredAreaAssetId; }

void CWorldState::SetDesiredAreaAssetId(CAssetId areaId) { mDesiredAreaAssetId = areaId; }

rstl::ncrc_ptr< CWorldLayerState >& CWorldState::GetLayerState() { return mLayerState; }

CGameState::SPlayerResult::SPlayerResult(CBitStreamReader& in)
: mPlayerSelection(in.ReadBits(2))
, mScore(int(in.ReadBits(16)) - 0x8000)
, mDeaths(int(in.ReadBits(16)) - 0x8000) {
  // The controller options are not serialized or initialized by this constructor.
}

void CGameState::SPlayerResult::PutTo(CBitStreamWriter& out) const {
  out.WriteBits(mPlayerSelection, 2);
  out.WriteBits(mScore + 0x8000, 16);
  out.WriteBits(mDeaths + 0x8000, 16);
}

CGameState::SPreviousGameResults::SPreviousGameResults(CBitStreamReader& in) {
  in.ReadBits(32); // PREV
  mGameMode = in.ReadBits(32);
  mShowResults = in.ReadPackedBool();
  x8_ = int(in.ReadBits(8)) - 0x80;
  mPlayerCount = in.ReadBits(3);
  for (int i = 0; i < 4; ++i) {
    mPlayers.push_back(SPlayerResult(in));
  }
}

void CGameState::SPreviousGameResults::PutTo(CBitStreamWriter& out) const {
  out.WriteBits('PREV', 32);
  out.WriteBits(mGameMode, 32);
  out.WriteBits(mShowResults ? 1 : 0, 1);
  out.WriteBits(x8_ + 0x80, 8);
  out.WriteBits(mPlayerCount, 3);
  for (int i = 0; i < 4; ++i) {
    mPlayers[i].PutTo(out);
  }
}

CGameState::CGameState()
: mWorldId(kInvalidAssetId)
, mDesiredWorldId(kInvalidAssetId)
, mTransManager(rs_new CWorldTransManager)
, mTotalPlayTime(0.0)
, mEscapeTime(0.f)
, mPersistentOptions(CGameStateEnvVarManager::kVS_Game)
, mCardSerial(0)
, mCompressedGameStates(3, rstl::vector< uchar >())
, mCompressedGameOptions(3, rstl::vector< uchar >())
, mGameMode(rs_new CGMSinglePlayer)
, mControlMapper(0)
, mHardMode(false)
, mInitPowerupsAtFirstSpawn(true)
, mIsDarkWorld(false) {
  for (int player = 0; player < 4; ++player) {
    mPlayerStates.push_back(rstl::rc_ptr< CPlayerState >(rs_new CPlayerState(player, nullptr)));
  }
  if (gpMemoryCard != nullptr) {
    InitializeMemoryStates();
  }
  for (int slot = 0; slot < 3; ++slot) {
    RecordCompressedGameOptions(slot);
  }
  RecordCompressedMultiplayerOptions();
}

extern "C" void fn_8014495C(SGameStateBlock* elems, int n, const SGameStateBlock* src) {
  SGameStateBlock* p = elems;
  for (int i = 0; i < n; i++, p++) {
    fn_80142A10(p, src);
  }
}

extern "C" SGameStateSlots* fn_80144924(SGameStateSlots* self, int n, const SGameStateBlock* src) {
  self->x00_count = n;
  fn_8014495C(self->x04_blk, n, src);
  return self;
}

// `rstl::vector< CHintOptions::SHintState >::operator=` - retail 0x80144818, 200 bytes, unnamed
// in the symbol table, and so claimable only under an `extern "C"` name (see CHintOptions.hpp
// for why the friend declaration has to come first). **This is the body of
// `rstl::vector`'s copy assignment, written out rather than reached through
// `to.mHintStates = from.mHintStates`**, and the reason is the score, not the shape: a template
// instantiation is emitted under its *mangled* name, objdiff pairs functions by name, and
// `__as__Q24rstl63vector<Q212CHintOptions10SHintState,...>` therefore never pairs with retail's
// `fn_80144818` - the unit reported it as having no body at all while the object held all 50
// correct instructions. The statements below are `rstl/vector.hpp`'s `operator=` verbatim, so
// the code is the same code; only the symbol is retail's.
extern "C" {
void* fn_80144818(rstl::vector< CHintOptions::SHintState >* self,
                  const rstl::vector< CHintOptions::SHintState >* src) {
  if (self == src) {
    return self;
  }
  self->clear();
  if (src->size() == 0) {
    self->mAllocator.deallocate(self->mItems);
    self->mCount = 0;
    self->mCapacity = 0;
    self->mItems = nullptr;
  } else {
    self->reserve(src->size());
    rstl::uninitialized_copy(src->mItems, src->mItems + src->mCount, self->data());
    self->mCount = src->mCount;
  }
  return self;
}
} // extern "C"

// `CHintOptions`'s copy assignment (retail 0x801447C4, unnamed in the symbol table, and so
// claimable only under an `extern "C"` name - see CHintOptions.hpp). The
// `rstl::vector< SHintState >::operator=` it calls is out of line and lands at 0x80144818,
// immediately before this, and is called **by retail's name** for the reason spelled out there.
extern "C" void* fn_801447C4(void* self, const void* src) {
  CHintOptions& to = *static_cast< CHintOptions* >(self);
  const CHintOptions& from = *static_cast< const CHintOptions* >(src);
  fn_80144818(&to.mHintStates, &from.mHintStates);
  to.mNextHintIdx = from.mNextHintIdx;
  to.mInRezbitState = from.mInRezbitState;
  to.mScanDisplayActive = from.mScanDisplayActive;
  return self;
}

CGameState::CGameState(CBitStreamReader& in)
: mWorldId(kInvalidAssetId)
, mDesiredWorldId(kInvalidAssetId)
, mTransManager(rs_new CWorldTransManager)
, mTotalPlayTime(0.0)
, mEscapeTime(0.f)
, mPersistentOptions(CGameStateEnvVarManager::kVS_Game)
, mCardSerial(0)
, mCompressedGameStates(3, rstl::vector< uchar >())
, mCompressedGameOptions(3, rstl::vector< uchar >())
, mGameMode(rs_new CGMSinglePlayer)
, mControlMapper(0)
, mHardMode(false)
, mInitPowerupsAtFirstSpawn(true)
, mIsDarkWorld(false) {
  in.ReadBits(32); // GMST
  in.ReadBits(32); // Timestamp
  mHardMode = in.ReadPackedBool();
  mInitPowerupsAtFirstSpawn = in.ReadPackedBool();
  mIsDarkWorld = in.ReadPackedBool();
  mWorldId = in.ReadBits(32);
  mDesiredWorldId = mWorldId;
  CMain::EnsureWorldPakReady(mWorldId);

  union {
    double value;
    u64 bits;
  } playTime;
  playTime.bits = u64(in.ReadBits(32)) << 32;
  playTime.bits |= in.ReadBits(32);
  mTotalPlayTime = playTime.value;
  for (int player = 0; player < 4; ++player) {
    mPlayerStates.push_back(rstl::rc_ptr< CPlayerState >(rs_new CPlayerState(player, in)));
  }
  mHintOptions = CHintOptions(in);
  mPreviousGameResults = SPreviousGameResults(in);
  mEscapeTime = in.GetInputStream().ReadFloat();
  mPersistentOptions = CGameStateEnvVarManager(CGameStateEnvVarManager::kVS_Game, in);

  const rstl::vector< CMemoryCard::MemoryWorld >& worlds = gpMemoryCard->GetMemoryWorlds();
  mWorldStates.reserve(worlds.size());
  const uchar worldCount = in.GetInputStream().ReadUint8();
  for (uchar i = 0; i < worldCount; ++i) {
    const CAssetId worldId = in.GetInputStream().ReadInt32();
    int bitCount = in.GetInputStream().ReadUint16();
    if (!gpMemoryCard->HasSaveWorldMemory(worldId)) {
      // **The string is built and thrown away, and retail builds it.** `Stringize` into an
      // `rstl::string`, skip the save data's bits, then let the string die (0x80144684-0x801446b0,
      // `~basic_string` at 0x801446d8). The format literal is not decoration either: it is
      // `lbl_803A9208 + 60`, the first of the two long messages this unit's pool keeps at +60
      // and +134, and the instruction pair that loads it is part of the match.
      rstl::string missing(CBasics::Stringize(
          "Cannot find World Asset(%x) to load save data.  Skipping save game info.\n", worldId));
      while (bitCount > 0) {
        in.ReadBits(rstl::min_val(bitCount, 32));
        bitCount -= 32;
      }
    } else {
      TLockedToken< CWorldSaveGameInfo > saveWorld = gpSimplePool->GetObj(
          SObjectTag('SAVW', gpMemoryCard->GetSaveWorldMemory(worldId).GetSaveWorldAssetId()));
      mWorldStates.push_back(CWorldState(in, worldId, **saveWorld));
    }
  }
  in.GetInputStream().ReadInt32(); // GMND

  for (rstl::vector< CMemoryCard::MemoryWorld >::const_iterator it = worlds.begin();
       it != worlds.end(); ++it) {
    // StateForWorld creates defaults for worlds absent from the save.
    const uchar knownWorlds = mWorldStates.size();
    StateForWorld(it->first);
    if (mWorldStates.size() != knownWorlds) {
      // The second unused diagnostic string, `lbl_803A9208 + 134`: only when StateForWorld had
      // to invent a world, i.e. the save did not carry one (0x8014470c-0x80144758).
      rstl::string defaulted(CBasics::Stringize(
          "Save game did not contain World Asset(%x).  Creating default world save info.\n",
          it->first));
    }
  }
  InitializeMemoryWorlds();
  WriteBackupBuf();
  for (int slot = 0; slot < 3; ++slot) {
    RecordCompressedGameOptions(slot);
  }
  RecordCompressedMultiplayerOptions();
}

void CGameState::InitializeMemoryStates() {
  for (int i = 0; i < mPlayerStates.size(); ++i) {
    mPlayerStates[i]->InitializeScanTimes();
  }
  mHintOptions.InitializeMemoryState();
  mPersistentOptions.InitializeMemoryState();
  InitializeMemoryWorlds();
  WriteBackupBuf();
}

// The 64 zero bytes `fn_80143E88` copy-constructs its local out of: `.rodata:0x803A91C8`, the
// 0x40 bytes immediately below this unit's own pool at `lbl_803A9208` (0x803A9208). It is retail
// data in a retail object, not something this unit may claim - the claim starts at 0x803A9208
// (`config/G2ME01/splits.txt`) - so it is referenced by name, the way `lbl_803A9208` is.
extern "C" const char lbl_803A91C8[];

// `fn_800068F4` walks its argument as a twelve-byte-element container: `+0x04` the element count,
// `+0x0C` the base pointer, `count * 12` the end (0x800068F4, 0x80146900-0x80146920). Retail
// code the port does not have, so it is called through an untyped pointer.
extern "C" void fn_800068F4(void* self);

struct SGameStateName {
  char x00_name[0x40];
};

// Called from `CMainFlow::AdvanceGameState` (0x8001DE34) when the restart mode is neither
// `kRM_None` nor `kRM_StateSetter`, i.e. when the game is resuming into the world rather than
// resetting through the front end. `gpResourceFactory->GetResourceIdByName("InitialWorld")` is
// the probe: a non-null answer means the world is loaded, and the game resumes as a single-player
// game; a null answer means it is not, and the game resumes *in* the front end. The name it
// builds in the second case is the results-screen layer name for the mode that was played.
void fn_80143E88() {
  CMain::EnsureWorldPaksReady();
  fn_800068F4(gpGameState->AudioGroups());

  const SObjectTag* const world = gpResourceFactory->GetResourceIdByName("InitialWorld");
  if (world != nullptr) {
    gpGameState->SetCurrentWorldId(world->GetId());
    gpGameState->SetGameMode(rs_new CGMSinglePlayer());
  } else {
    gpGameState->SetCurrentWorldId(gpResourceFactory->GetResourceIdByName("FrontEnd")->GetId());
    gpGameState->SetGameMode(rs_new CFrontEndGameMode());

    rstl::rc_ptr< CWorldLayerState > layers = gpGameState->CurrentWorldState().GetLayerState();
    layers->GetAreaLayerCount(TAreaId(0));

    // **Pool order, not order of use.** Retail loads the three addresses in one hoisted block as
    // `+29`, `+37`, `+42` - `Results`, `Coin`, `Deathmatch` - and the `"%s%s%d"` it passes to
    // both `sprintf`s is created last, by the first one, and lands at +53. Written inline at the
    // call sites the pool would order them by first *use* and every immediate would move. They
    // are declared before the two member reads for a second reason: the pool base they share with
    // the `new` operands has to land in `r4`, and the member read has to be pushed off it.
    const char* const kResults = "Results";
    const char* const kCoin = "Coin";
    const char* const kDeathmatch = "Deathmatch";

    // **These two are read before the 64-byte copy, and that is load-bearing.** Retail loads
    // them at 0x80143FB8/0x80143FC0 and `mShowResults` only at 0x80144050, so two values have to
    // survive sixteen stores. Reading all three before the copy is *also* wrong - it costs
    // `mShowResults` its live range and the frame comes out -128 bytes.
    const CGameState::SPreviousGameResults& results = gpGameState->PreviousGameResults();
    const int gameMode = results.mGameMode;
    const int playerCount = results.mPlayerCount;
    SGameStateName name = *reinterpret_cast< const SGameStateName* >(lbl_803A91C8);

    if (results.mShowResults && playerCount > 1) {
      // The two-sided tests are `== 'DTHM'` and `== 'COIN'`, spelled as `addis` against the
      // high half and `cmplwi` against the low (0x80144064, 0x80144084) - which is what mwcceppc
      // emits for a full-word compare against a constant that will not fit in one immediate.
      if (gameMode == 'DTHM') {
        sprintf(name.x00_name, "%s%s%d", kResults, kDeathmatch, playerCount);
      } else if (gameMode == 'COIN') {
        sprintf(name.x00_name, "%s%s%d", kResults, kCoin, playerCount);
      }
    }
  }
}

// Guessed name. Layer-name prefixes select which game mode owns each layer.
//
// The element type is a plain struct with a user constructor, not `rstl::pair`, and both halves of
// that are load-bearing for `__sinit_CGameState_cpp` (0x80146874, 80 bytes, 20 instructions). Every
// claim below was measured on this compile, not inferred:
//
//   * `rstl::pair(const L& first, const R& second)` takes its arguments **by const reference**, so
//     each `'DTHM'` is an object that needs an address: mwcceppc puts the three constants in a
//     `.sdata` literal pool and loads them with `lwz ...,0(0)` + `R_PPC_EMB_SDA21`, at +0x0C, +0x14
//     and +0x18 of the function. Retail has no pool loads - it materialises each constant with
//     `lis`+`addi`, `lis r5,17492` at +0x0C and `addi r7,r5,18509` at +0x20 for `'DTHM'` - because
//     the constant is an immediate in the constructor's call, not an address to copy. Taking the
//     `uint` **by value** is what gets that, and it is worth 63.35% -> 80.80% on its own.
//   * the `const` on the *by-value pointer* parameter is load-bearing too, and is the remaining
//     80.80% -> 100.00%. Without it the compile is one instruction short of retail (19 instructions
//     / 76 bytes): the register holding `R_PPC_ADDR16_HA sGameModeLayers` (+0x04) dies at its
//     `ADDR16_LO` addi, so the store-with-update pass folds the pair into `stwu r8,0(r6)` and
//     stores the other five words off the write-back register. Retail keeps that register live - it
//     is reused at +0x24 for `addi r6,r10,440` - so the low half stays a separate
//     `addi r8,r6,0` (+0x1C) and all six stores are plain `stw`s off r8. The `const` is what changes
//     the allocator's decision; nothing about the code's meaning differs.
struct SGameModeLayer {
  const char* first;
  uint second;
  SGameModeLayer(const char* const a, uint b) : first(a), second(b) {}
};
static SGameModeLayer sGameModeLayers[] = {
    SGameModeLayer("Deathmatch", 'DTHM'),
    SGameModeLayer("Samus01", 'SNGL'),
    SGameModeLayer("Coins", 'COIN'),
};

void ConfigureGameModeLayers() {
  for (int area = 0;
       area < gpMemoryCard->GetSaveWorldMemory(gpGameState->CurrentWorldAssetId()).GetAreaCount();
       ++area) {
    rstl::rc_ptr< CWorldLayerState > layersRc = gpGameState->CurrentWorldState().GetLayerState();
    CWorldLayerState& layers = *layersRc;
    int layerCount = layers.GetAreaLayerCount(TAreaId(area));
    for (int layer = 0; layer < layerCount; ++layer) {
      for (int i = 0; i < 3; ++i) {
        // Spelled as a difference compared to zero, not as `==`, and with the layer table's
        // value on the left. mwcceppc lowers `a == b` to `subf r0,r0,r3` (b - a) whatever the
        // source operand order, but lowers `a - b == 0` to `subf r0,r3,r0` (a - b) - which is
        // what retail emits at 0x80143DC8. `type - second == 0` is the one order that does NOT
        // work; only `second - type == 0` does. Unsigned subtraction, so it is exactly the
        // equality it replaces.
        bool active =
            (sGameModeLayers[i].second - gpGameState->GetGameMode().GetGameModeType()) == 0;
        const char* prefix = sGameModeLayers[i].first;
        const rstl::string& name = layers.GetLayerName(TAreaId(area), TLayerId(layer));
        if (strncmp(prefix, name.data(), strlen(prefix)) == 0) {
          layers.SetLayerActive(TAreaId(area), TLayerId(layer), active);
        }
      }
    }
  }
}

// `mPlayers(other.mPlayers)` instantiates `rstl::reserved_vector< CFrontEndPlayerData, 4 >`'s copy
// constructor out of line at retail 0x80143CD4 (80 bytes), under the name upstream's symbols.txt
// gives it. Before the eighth upstream sync that address was unnamed and was claimed here by an
// `extern "C" fn_80143CD4` carve called from this constructor's body.
CFrontEndGameMode::CFrontEndGameMode(const CFrontEndGameMode& other)
: CGameMode(other)
, mGameOver(other.mGameOver)
, mResultIndex(other.mResultIndex)
, mSelectedGameMode(other.mSelectedGameMode)
, mFragLimit(other.mFragLimit)
, mCoinLimit(other.mCoinLimit)
, mTimeLimit(other.mTimeLimit)
, mMusicIndex(other.mMusicIndex)
, mPlayers(other.mPlayers) {}

// `CFrontEndGameMode`'s deleting destructor (retail 0x80143B94, 180 bytes). It is declared in
// CFrontEndGameMode.hpp and defined **here** because retail defines it in this unit and nothing else
// the port builds needs it: with no definition here the object holds `U __dt__17CFrontEndGameModeFv`
// and the function scores 0.00%.
//
// The body is empty on purpose - retail's is the member teardown with nothing added to it, and
// the compiler writes all of it: the null guard, the `__vt__17CFrontEndGameMode` store (0x803B8470),
// `rstl::reserved_vector<SPlayerConfig, 4>`'s destructor **inlined** - its count read at
// `this+0x20` (0x80143BC0), then the `li r3,0` / `cmpwi` / `addi r5,r6,-8` / `mtctr`+`bdnz`
// 8-byte chunk loop and the `subf`/`mtctr`/`cmpw`/`bdnz` byte loop that the standalone
// `__dt__Q24rstl49reserved_vector<...>Fv` in this same object already emits identically - the
// `__vt__9CGameMode` store (0x803B0D68), and the D0 test with `operator delete`.
//
// **Both of those need the destructor of the thing they inline to be visible here.**
// `~reserved_vector` is `inline` in reserved_vector.hpp:53; `~CGameMode` only became
// inline-visible when CGameMode.hpp:13 gave it an empty body. Left declared-only, the base
// teardown is an out-of-line `bl __dt__9CGameModeFv` that parks the D0 flag in r31 and `this`
// in r30, and the function is 184 bytes at 83.00%.

// Guessed name
void StartGameFromFrontEnd() {
  const CFrontEndGameMode config = static_cast< const CFrontEndGameMode& >(gpGameState->GetGameMode());

  for (int i = 0; i < config.GetPlayerCount(); ++i) {
    config.GetPlayer(i);
  }

  CGameMode* mode = nullptr;

  switch (config.GetSelectedGameMode()) {
  case CFrontEndGameMode::kSGM_SinglePlayer:
    mode = rs_new CGMSinglePlayer;
    break;
    
  case CFrontEndGameMode::kSGM_DeathMatch: {
    CGMDeathMatch* deathMatch = rs_new CGMDeathMatch(config.GetPlayerCount(), config.GetFragLimit(),
                                                     config.GetTimeLimit(), true, false);
    deathMatch->SetMusicIndex(config.GetMusicIndex());
    mode = deathMatch;
    break;
  }
  case CFrontEndGameMode::kSGM_Coin: {
    CGMCoin* coin =
        rs_new CGMCoin(config.GetPlayerCount(), config.GetCoinLimit(), config.GetTimeLimit(), true);
    coin->SetMusicIndex(config.GetMusicIndex());
    mode = coin;
    break;
  }
  case CFrontEndGameMode::kSGM_FrontEnd:
    mode = rs_new CFrontEndGameMode;
    break;
  }

  const CGameState::SPreviousGameResults results = gpGameState->PreviousGameResults();
  gpMain->StreamNewGameState(false);
  if (config.GetSelectedGameMode() == CFrontEndGameMode::kSGM_Coin ||
      config.GetSelectedGameMode() == CFrontEndGameMode::kSGM_DeathMatch) {
    gpGameState->LoadCompressedMultiplayerOptions();
  } else if (config.GetSelectedGameMode() == CFrontEndGameMode::kSGM_SinglePlayer) {
    gpGameState->LoadCompressedGameOptions(gpGameState->SystemOptions().GetSaveIdx());
  }
  gpGameState->GameOptions().EnsureOptions();
  gpGameState->SetGameMode(mode);
  gpGameState->PreviousGameResults() = results;

  for (int i = 0; i < config.GetPlayerCount(); ++i) {
    const CFrontEndPlayerData& player = config.GetPlayer(i);
    gpGameState->PlayerState(i)->FUN_80085c18(player.mPlayerSelection);
    gpGameState->GameOptions().PlayerOptions(i) = player.mOptions;
  }
  ConfigureGameModeLayers();
  gpGameState->WriteBackupBuf();
}

void CGameState::InitializeMemoryWorlds() {
  const rstl::vector< CMemoryCard::MemoryWorld >& worlds = gpMemoryCard->GetMemoryWorlds();
  for (rstl::vector< CMemoryCard::MemoryWorld >::const_iterator it = worlds.begin();
       it != worlds.end(); ++it) {
    rstl::rc_ptr< CWorldLayerState > layers = StateForWorld(it->first).GetLayerState();
    // Retail materialises the three arguments in r4, r5, r6 in that order (0x80143818..0x80143820);
    // named locals make the compiler emit them in declaration order rather than last-first.
    const rstl::vector< CWorldLayers::Area >& defaultStates = it->second.GetDefaultLayerStates();
    const rstl::rc_ptr< rstl::vector< rstl::string > >& layerNames = it->second.GetLayerNames();
    const rstl::rc_ptr< rstl::vector< int > >& layerNameOffsets = it->second.GetLayerNameOffsets();
    layers->InitializeWorldLayers(defaultStates, layerNames, layerNameOffsets);
  }
}

// The per-element destructor `fn_801467C0` loops over.
extern "C" void fn_801435D4(void* elem) { fn_80004458(elem); }

void CGameState::SerializeNewForCleanSlot(CBitStreamWriter& out, bool hardMode) {
  CGameState state;
  state.SetHardMode(hardMode);
  state.SetDesiredWorldId(0x3bfa3eff);
  state.StateForWorld(0x3bfa3eff).SetDesiredAreaAssetId(0x62b0d67d);
  state.PutTo(out);
}

void CGameState::PutTo(CBitStreamWriter& out) {
  out.WriteBits('GMST', 32);
  out.WriteBits(OSTicksToSeconds(OSGetTime()), 32);
  out.WriteBits(mHardMode ? 1 : 0, 1);
  out.WriteBits(mInitPowerupsAtFirstSpawn ? 1 : 0, 1);
  out.WriteBits(mIsDarkWorld ? 1 : 0, 1);
  out.WriteBits(mDesiredWorldId, 32);

  u64 time = *reinterpret_cast< const u64* >(&mTotalPlayTime);
  out.WriteBits(time >> 32, 32);
  out.WriteBits(time & 0xffffffff, 32);

  for (int i = 0; i < mPlayerStates.size(); ++i) {
    mPlayerStates[i]->PutTo(out);
  }
  mHintOptions.PutTo(out);
  mPreviousGameResults.PutTo(out);
  const rstl::vector< CMemoryCard::MemoryWorld >& worlds = gpMemoryCard->GetMemoryWorlds();
  out.GetOutputStream().WriteReal32(mEscapeTime);
  mPersistentOptions.PutTo(out);

  out.GetOutputStream().WriteUint8(worlds.size());
  rstl::auto_ptr< uchar > buffer(rs_new uchar[0x400]);
  for (AUTO(it, worlds.begin()); it != worlds.end(); ++it) {
    TLockedToken< CWorldSaveGameInfo > saveWorld =
        gpSimplePool->GetObj(SObjectTag('SAVW', it->second.GetSaveWorldAssetId()));
    CWorldState& state = StateForWorld(it->first);
    uint bitCount;
    {
      CMemoryStreamOut stream(buffer.get(), 0x400);
      CBitStreamWriter writer(stream);
      state.PutTo(writer, **saveWorld);
      stream.Flush();
      bitCount = writer.GetWrittenBits();
    }
    out.GetOutputStream().WriteUint32(it->first);
    out.GetOutputStream().WriteUint16(bitCount);
    state.PutTo(out, **saveWorld);
  }
  out.GetOutputStream().WriteUint32('GMND');
}

void CGameState::ReadSystemOptions(CInputStream& in) {
  CBitStreamReader reader(in);
  mSystemOptions = CPersistentOptions(reader);
}

void CGameState::WriteSystemOptions(COutputStream& out) {
  CBitStreamWriter writer(out);
  mSystemOptions.PutTo(writer);
}

void CGameState::SetSystemOptions(const CPersistentOptions& options) { mSystemOptions = options; }

void CGameState::ExportPersistentOptions(CPersistentOptions& options) {
  options.SetSaveIdx(mSystemOptions.GetSaveIdx());
}

void CGameState::WriteBackupBuf() {
  rstl::vector< uchar >& buffer = mCompressedGameStates[mSystemOptions.GetSaveIdx()];
  buffer.resize(0xa38);
  CMemoryStreamOut stream(buffer.data(), 0xa38);
  CBitStreamWriter out(stream);
  PutTo(out);
}

void CGameState::RecordCheckpoint() {
  mCheckpointGameState.resize(0xa38);
  CMemoryStreamOut stream(mCheckpointGameState.data(), 0xa38);
  CBitStreamWriter out(stream);
  PutTo(out);
}

void CGameState::ClearCheckpoint() { mCheckpointGameState.clear(); }

void CGameState::SetCompressedGameStates(
    const rstl::reserved_vector< rstl::vector< uchar >, 3 >& states) {
  mCompressedGameStates = states;
}

void CGameState::CopyCompressedGameState(int slot, const void* data) {
  mCompressedGameStates[slot].resize(0xa38);
  memcpy(mCompressedGameStates[slot].data(), data, 0xa38);
}

void CGameState::RecordCompressedGameState(int slot, CGameState& state) {
  mCompressedGameStates[slot].resize(0xa38);
  CMemoryStreamOut stream(mCompressedGameStates[slot].data(), 0xa38);
  CBitStreamWriter out(stream);
  state.PutTo(out);
}

void CGameState::ClearCompressedGameState(int slot) {
  mCompressedGameStates[slot] = rstl::vector< uchar >();
}

void CGameState::RecordCompressedGameOptions(int slot) {
  mCompressedGameOptions[slot].resize(0x20);
  CMemoryStreamOut stream(mCompressedGameOptions[slot].data(), 0x20);
  CBitStreamWriter out(stream);
  mGameOptions.PutTo(out);
}

void CGameState::CopyCompressedGameOptions(int slot, const void* data) {
  mCompressedGameOptions[slot].resize(0x20);
  memcpy(mCompressedGameOptions[slot].data(), data, 0x20);
}

void CGameState::RecordCompressedMultiplayerOptions() {
  mCompressedMultiplayerOptions.resize(0x20);
  CMemoryStreamOut stream(mCompressedMultiplayerOptions.data(), 0x20);
  CBitStreamWriter out(stream);
  mGameOptions.PutTo(out);
}

// clear, reserve(count), then count unchecked appends of *src.
extern "C" void fn_80142BA4(SGameStateBlock* self, int count, const unsigned char* src) {
  fn_80142914(self);
  fn_801465EC(self, count);
  for (int i = 0; i < count; ++i) {
    unsigned char* p = static_cast< unsigned char* >(self->x0c_data) + self->x04_count++;
    *p = *src;
  }
}

void CGameState::CopyCompressedMultiplayerOptions(const void* data) {
  mCompressedMultiplayerOptions.resize(0x20);
  memcpy(mCompressedMultiplayerOptions.data(), data, 0x20);
}

// The two `LoadCompressed*Options` readers (retail 0x80142A30, 0x8C bytes, and 0x80142ABC, 0x94)
// are one body differing by two instructions in how the block's address is formed: the
// multiplayer block at `+0x178` is read directly (`lwz` at +0x0C and +0x08 off `this`), while
// `mCompressedGameOptions` is a three-element array at `+0x148` of 16-byte elements, so the slot
// is scaled first (`slwi r0,r4,4 ; add r5,r31,r0` at 0x80142AC8/0x80142AD4). Everything after
// that is identical: `CMemoryInStream(data, len)`, `CBitStreamReader` over it, a `CGameOptions`
// built from the reader, that temporary copied into the `+0x80` member by `fn_80003D00` and
// destroyed by `fn_80004D84`, then the reader and then the stream.
//
// **The length argument is the block's `capacity()`, not its `size()`.** Both are `lwz` from
// `+0x08` of the 16-byte block (`0x80142A48`/`0x80142A4C` and `0x80142ADC`/`0x80142AE0`), and
// `rstl::vector`'s `+0x08` is `mCapacity` while `+0x04` is `mCount` - the same two words
// `SGameStateBlock` names `x08_cap` and `x04_count`. `CMemoryInStream`'s second parameter is
// `unsigned long`, so `.size()` (an `int` off `+0x04`) is the wrong word and the call would read
// a different offset than retail's.
//
// The `CGameOptions` temporary is a POD mirror of the right size rather than a `CGameOptions`,
// for the reason `CMainResetGameState.cpp` gives at its own `SGameOptionsCopy`:
// `include/MetroidPrime/Player/CGameOptions.hpp:21` **declares** a destructor, so a local of that
// type would have mwcceppc run `~CGameOptions()` at scope exit and call it through the C++ name
// `__dt__12CGameOptionsFv` - a symbol `config/G2ME01/symbols.txt` does not carry. Retail's is
// the same function at the same address, under the unnamed `fn_80004D84`, so the destructor and
// the copy-assignment are called by hand. The **constructor** needs no such treatment:
// `__ct__12CGameOptionsFR16CBitStreamReader` (0x80161828, 0x320) is named in the symbol table
// and is already 100% matched in `src/MetroidPrime/Player/CGameOptions.cpp`, so it is called
// through its own MWCC name, the same arrangement `CGameStateCtor.cpp:98` uses for
// `__ct__12CGameOptionsFv`.
//
// The reader and the stream are ordinary locals of their own types and are left to the
// compiler. `CMemoryInStream` is a `virtual` class whose destructor is declared `{}`, so the
// six instructions at 0x80142A90-0x80142AA4 - `lis`/`addi` of the `CInputStream` vtable, the
// `addi` of the object, `li r4,0`, `stw` of the vtable at the object and
// `bl __dt__12CInputStreamFv` - are the compiler's own base-class teardown and are not written
// here.
struct SGameOptionsLoad {
  u8 x00[sizeof(CGameOptions)];
};
CHECK_SIZEOF(SGameOptionsLoad, 0x44)

extern "C" void __ct__12CGameOptionsFR16CBitStreamReader(CGameOptions* self, CBitStreamReader& in);
extern "C" void fn_80003D00(CGameOptions* self, const CGameOptions* src);
extern "C" void fn_80004D84(CGameOptions* self, int flag);

void CGameState::LoadCompressedGameOptions(int slot) {
  CMemoryInStream stream(mCompressedGameOptions[slot].data(),
                         mCompressedGameOptions[slot].capacity());
  CBitStreamReader reader(stream);
  SGameOptionsLoad tmp;
  __ct__12CGameOptionsFR16CBitStreamReader(reinterpret_cast< CGameOptions* >(&tmp), reader);
  fn_80003D00(&mGameOptions, reinterpret_cast< const CGameOptions* >(&tmp));
  fn_80004D84(reinterpret_cast< CGameOptions* >(&tmp), -1);
}

void CGameState::LoadCompressedMultiplayerOptions() {
  CMemoryInStream stream(mCompressedMultiplayerOptions.data(),
                         mCompressedMultiplayerOptions.capacity());
  CBitStreamReader reader(stream);
  SGameOptionsLoad tmp;
  __ct__12CGameOptionsFR16CBitStreamReader(reinterpret_cast< CGameOptions* >(&tmp), reader);
  fn_80003D00(&mGameOptions, reinterpret_cast< const CGameOptions* >(&tmp));
  fn_80004D84(reinterpret_cast< CGameOptions* >(&tmp), -1);
}

extern "C" void fn_80142A10(SGameStateBlock* self, const SGameStateBlock* src) {
  fn_80004D5C(self, src);
}

// The 16-byte block's element copy (retail 0x801429AC), the same shape as the 12-byte block's
// `fn_801465A8` above: `begin` and `end` are the source range, `dst` the destination, the stride
// is the element size and the **return value is the advanced destination**, not `dst` itself
// (0x801429F4 is `mr r3,r31`, with `r31` the destination walked forward in the loop). Writing
// `return dst` costs a register - the original destination has to stay live across the loop, so
// the compiler adds `r28` and the function is 112 bytes against retail's 100. Spelling the
// element as `SGameStateBlock` is what gives the 16-byte stride.
extern "C" void* fn_801429AC(void* begin, void* end, void* dst) {
  SGameStateBlock* out = static_cast< SGameStateBlock* >(dst);
  for (SGameStateBlock* in = static_cast< SGameStateBlock* >(begin);
       in != static_cast< SGameStateBlock* >(end); ++in, ++out) {
    fn_80142A10(out, in);
  }
  return out;
}

// The 0x34 `SGameStateSlots` assignment (retail 0x80142944, reached from `fn_80142920`
// (0x80142920, `addi r3,r3,324`) and `fn_80142FA4` (0x80142FA4, `addi r3,r3,272`)). A
// self-assignment returns at once (0x80142960 `cmplw` / 0x80142964 `beq` straight to the
// epilogue), the destination's elements are released by `fn_80004BEC`, the range of `x00_count`
// 16-byte elements is copy-assigned by `fn_801429AC`, and the count is written **last**, out of a
// *second* read of the source (0x80142988 / 0x8014298C) - so `src->x00_count` is spelled twice and
// not through a local.
//
// **The range end is counted in bytes from `x04_blk`, and that is the whole match.** Retail forms
// it as `src + count * 16` and only *then* adds `x04_blk`'s `+0x04` displacement (0x8014297C
// `add r4,r31,r0` / 0x80142980 `addi r4,r4,4`), with the product in a temporary. Every spelling
// that folds the `+4` into the index first puts the product in the destination register instead
// (`slwi r4,r0,4` / `addi r4,r4,4` / `add r4,r31,r4`) and the function sits at 91.92% - including
// the word-index trick that fixed `fn_801426E0` above, because that `+4` is a member displacement
// and not a byte offset. Adding to a `unsigned char*` is what keeps it a separate `addi`.
//
// The first argument must stay spelled `s->x04_blk`, not a local bound to it: with the local, the
// compiler reuses that register as the base of the index add (`add r4,r3,r0` at 0x8014297C's place)
// and the function falls to 95.96%. `const_cast` is only for the element pointers - `fn_801429AC`
// takes `void*` and is spelled that way at 100%.
//
// `fn_80004BEC` (0x80004BEC, 0x60) is `__dt__80004B9C`'s callee and is claimed by no unit, so it is
// called through this declaration rather than inlined; `PortStreamNewGameState.cpp:158` spells the
// same loop for the port as `ReleaseSlots`.
extern "C" SGameStateSlots* fn_80142944(SGameStateSlots* self, const SGameStateSlots* src) {
  if (self != src) {
    fn_80004BEC(self);
    SGameStateSlots* const s = const_cast< SGameStateSlots* >(src);
    const int count = s->x00_count;
    unsigned char* const last = reinterpret_cast< unsigned char* >(s->x04_blk) + count * 16;
    fn_801429AC(s->x04_blk, last, self->x04_blk);
    self->x00_count = src->x00_count;
  }
  return self;
}

void CGameState::SetCompressedGameOptions(
    const rstl::reserved_vector< rstl::vector< uchar >, 3 >& options) {
  mCompressedGameOptions = options;
}

extern "C" void fn_80142914(SGameStateBlock* self) { self->x04_count = 0; }

// The byte-buffer block's assignment (retail 0x80142800), reached from
// `SetCompressedMultiplayerOptions` (0x801427DC, `addi r3,r3,376` then a tail call). A
// self-assignment returns at once (0x8014281C `cmplw`/`bne`), an empty source frees the buffer
// and zeroes the three words (0x80142838-0x8014284C), and anything else reserves with
// `fn_801465EC` (0x80142858) and then copies `src->x04_count` **bytes** - `x04_count` is the byte
// size in this instance, not the element count it is in the 36-byte block.
//
// Three spellings in this body are the match, and each is load-bearing:
//
// * **The emptiness test is signed.** Retail's is `cmpwi r4,0` (0x80142830) where a `u32` test
//   emits `cmplwi`, and the cast is what puts the `cmpwi` back. The count is a byte size, so the
//   two agree on every value the block can hold.
// * **The copy is a pointer-range loop, not an int-count loop and not `memcpy`.** `memcpy` is an
//   out-of-line `bl` here and an int-count loop is 40 instructions from retail, because retail
//   forms `end = from + count` and compares *pointers* (0x80142868 `add` / 0x8014286C `cmplw` /
//   0x80142870 `subf`); that is the `subf` trip count, and with it mwcceppc emits the same
//   8-way-unrolled byte copy retail has (0x80142884-0x801428CC) and the `andi. r3,r3,7` tail.
// * **The destination pointer is declared first.** The load order is retail's either way
//   (source, count, destination - 0x8014285C-0x80142864), but the unrolled loop's register
//   assignment is not: with `from` declared first the source lands in `r4` and the destination in
//   `r5`, and every `lbz`/`stb` of the loop is then swapped against retail.
extern "C" SGameStateBlock* fn_80142800(SGameStateBlock* self, const SGameStateBlock* src) {
  if (self == src) {
    return self;
  }
  fn_80142914(self);
  if (static_cast< int >(src->x04_count) == 0) {
    CMemory::Free(self->x0c_data);
    self->x04_count = 0;
    self->x08_cap = 0;
    self->x0c_data = nullptr;
  } else {
    fn_801465EC(self, src->x04_count);
    uchar* to = static_cast< uchar* >(self->x0c_data);
    const uchar* from = static_cast< const uchar* >(src->x0c_data);
    const uchar* const end = from + src->x04_count;
    while (from != end) {
      *to++ = *from++;
    }
    self->x04_count = src->x04_count;
  }
  return self;
}

void CGameState::SetCompressedMultiplayerOptions(const rstl::vector< uchar >& options) {
  mCompressedMultiplayerOptions = options;
}

// The 36-byte element's copy, and the element type itself: `CWorldState`
// (`include/MetroidPrime/Player/CWorldState.hpp`, `CHECK_SIZEOF(..., 0x24)`), which is what
// `CGameState`'s `mWorldStates` holds - `CGameState::StateForWorld` (0x8014260C) walks it at
// `mulli r0,r5,36` and constructs one on its own stack with `__ct__11CWorldStateFUi`. Retail walks
// the nine words of the destination one member at a time - `CAssetId mWorldId` (+0x00), `TAreaId
// mAreaId` (+0x04), `ncrc_ptr< CRelayTracker > mRelayTracker` (+0x08), `ncrc_ptr< CMapWorldInfo >
// mMapWorldInfo` (+0x10), `CAssetId mDesiredAreaAssetId` (+0x18), `ncrc_ptr< CWorldLayerState >
// mLayerState` (+0x1C) - and each of the three pointers' two words is stored before its refcount
// word is reloaded out of the **destination** and incremented (0x80142788, 0x801427AC,
// 0x801427C8).
//
// **It is the copy constructor's body and not the assignment's**, and that is the whole difference:
// retail has no `mPtr != other.mPtr` test and never calls `ReleaseData`, which is exactly
// `rc_ptr(const rc_ptr&)`'s `mPtr(other.mPtr), mRefCount(other.mRefCount) { ++*mRefCount; }` and
// not `rc_ptr::operator=` (`include/rstl/rc_ptr.hpp`). `fn_8000447C` is the matching destructor -
// `fn_80004458` and `fn_801435D4` forward to it with -1 - and `StateForWorld` builds a
// `CWorldState` on its own stack, hands it to `fn_801426E0` and then destroys it: push_back's
// copy, not a reuse of a live element. So `to = from` and `new (to) CWorldState(from)` are both
// the wrong spelling, and `rstl::construct` / `uninitialized_copy` (which reach the same
// constructor) come out as an out-of-line **call** to the weak
// `__ct__11CWorldStateFRC11CWorldState` that `rstl::vector< CWorldState >::push_back` also wants.
//
// The three `ncrc_ptr` members are copied through `rstl::CRcPtrData`, the same-layout view of an
// `rc_ptr`'s two words the rest of the port uses for this
// (`src/MetroidPrime/CIOWinManagerRemoveIOWin.cpp:74-79`), and the refcount word is incremented
// directly - which is what `rc_ptr`'s copy constructor does, spelled without a temporary.
//
// **Returning the destination is load-bearing, not decoration.** The last refcount reload is the
// one register choice that differs: the identical body declared `void` scores **99.19%** (four
// instructions), because the allocator has `r3` free by then and reloads through `r4`/`r3`; with
// `r3` live to the end - which is what returning it forces, exactly as a copy constructor returns
// `this` - the reload is `r5`/`r4` and the body is byte-for-byte retail's. Declared in the header
// with C linkage for the reason `CHintOptions.hpp:15-20` records.
extern "C" CWorldState* fn_80142760(void* elem, const void* src) {
  CWorldState& to = *static_cast< CWorldState* >(elem);
  const CWorldState& from = *static_cast< const CWorldState* >(src);
  to.mWorldId = from.mWorldId;
  to.mAreaId = from.mAreaId;
  *reinterpret_cast< rstl::CRcPtrData* >( &to.mRelayTracker ) =
      *reinterpret_cast< const rstl::CRcPtrData* >( &from.mRelayTracker );
  ++*reinterpret_cast< rstl::CRcPtrData* >( &to.mRelayTracker )->x4_refCount;
  *reinterpret_cast< rstl::CRcPtrData* >( &to.mMapWorldInfo ) =
      *reinterpret_cast< const rstl::CRcPtrData* >( &from.mMapWorldInfo );
  ++*reinterpret_cast< rstl::CRcPtrData* >( &to.mMapWorldInfo )->x4_refCount;
  to.mDesiredAreaAssetId = from.mDesiredAreaAssetId;
  *reinterpret_cast< rstl::CRcPtrData* >( &to.mLayerState ) =
      *reinterpret_cast< const rstl::CRcPtrData* >( &from.mLayerState );
  ++*reinterpret_cast< rstl::CRcPtrData* >( &to.mLayerState )->x4_refCount;
  return &to;
}

// The 36-byte element's "construct in place" pair. `fn_80142738` is the null test, `fn_80142718`
// the forwarder `fn_801426E0` and `fn_8014680C` both call.
extern "C" void fn_80142738(void* elem, const void* src) {
  if (elem != nullptr) {
    fn_80142760(elem, src);
  }
}

extern "C" void fn_80142718(void* elem, const void* src) { fn_80142738(elem, src); }

// The block's append: the element slot is `data + count * 36` and the count goes up before the
// element is built, not after (0x801426EC-0x80142704). The index is counted in **words**, not
// bytes: the element is 36 bytes, which is 9 `u32`s, and retail's `mulli r0,r5,36` at 0x801426F4
// is `mwcceppc`'s strength reduction of `words + n * 9`. Spelled as `n * 36` on a `uchar*` the
// multiply lands on the count's own register instead of a temporary and the function sits at
// 97.50%.
extern "C" void fn_801426E0(SGameStateBlock* self, const void* src) {
  u32* const words = static_cast< u32* >(self->x0c_data);
  const u32 n = self->x04_count;
  self->x04_count = n + 1;
  fn_80142718(words + n * 9, src);
}

CWorldState& CGameState::StateForWorld(CAssetId worldId) {
  // Both exits route through one end-test at +0x60, so the search *breaks* rather than
  // returning from inside the loop. `it` is hoisted because it is needed after the loop, but
  // `end` is not: retail re-reads mCount/mItems from the member at each test (0x8014260C).
  rstl::vector< CWorldState >::iterator it = mWorldStates.begin();
  while (it != mWorldStates.end()) {
    if (it->GetWorldAssetId() == worldId) {
      break;
    }
    ++it;
  }

  if (it != mWorldStates.end()) {
    return *it;
  }

  mWorldStates.reserve(mWorldStates.size() + 1);
  mWorldStates.push_back(CWorldState(worldId));
  return mWorldStates.back();
}

CAssetId CGameState::CurrentWorldAssetId() const { return mWorldId; }

CWorldState& CGameState::CurrentWorldState() { return StateForWorld(mWorldId); }

void CGameState::SetCurrentWorldId(CAssetId worldId) {
  StateForWorld(worldId);
  mWorldId = worldId;
  CMain::EnsureWorldPakReady(worldId);
  SetDesiredWorldId(worldId);
}

void CGameState::SetDesiredWorldId(CAssetId worldId) { mDesiredWorldId = worldId; }

rstl::rc_ptr< CPlayerState > CGameState::GetPlayerState() const { return mPlayerStates[0]; }

rstl::rc_ptr< CPlayerState >& CGameState::PlayerState(int player) { return mPlayerStates[player]; }

rstl::rc_ptr< CPlayerState > CGameState::GetPlayerState(int player) const {
  return mPlayerStates[player];
}

rstl::rc_ptr< CWorldTransManager >& CGameState::WorldTransitionManager() { return mTransManager; }

void CGameState::SetTotalPlayTime(double time) {
  mTotalPlayTime = CMath::Clamp(0.0, time, 359999.0);
}

void CGameState::SetEscapeTime(float time) { mEscapeTime = time; }

void CGameState::SetHardMode(bool hardMode) { mHardMode = hardMode; }

void CGameState::SetDeferPowerupInit(bool defer) { mInitPowerupsAtFirstSpawn = defer; }

void CGameState::SetIsDarkWorld(bool darkWorld) { mIsDarkWorld = darkWorld; }

float CGameState::GetHardModeDamageMultiplier() const {
  return gpTweakGame->GetHardModeDamageMultiplier();
}

float CGameState::GetHardModeWeaponMultiplier() const {
  return gpTweakGame->GetHardModeWeaponMultiplier();
}

const CGameMode& CGameState::GetGameMode() const { return *mGameMode; }

CGameMode& CGameState::GetGameMode() { return *mGameMode; }

void CGameState::SetGameMode(CGameMode* mode) { mGameMode = rstl::auto_ptr< CGameMode >(mode); }

bool CPersistentOptions::GetCinematicState(rstl::pair< CAssetId, TEditorId > cinematicId) const {
  for (AUTO(it, mCinematicStates.begin()); it != mCinematicStates.end(); ++it) {
    if (*it == cinematicId) {
      return true;
    }
  }
  return false;
}

// `rstl::vector< rstl::pair< CAssetId, TEditorId > >::erase( iterator )` - retail 0x80142288,
// 76 bytes, unnamed in the symbol table, and so claimable only under an `extern "C"` name.
// **Written out here for the same reason as `fn_80144818`**: a template instantiation is emitted
// mangled, so the instructions were already in the object and already correct (0 differing of 19)
// while objdiff had nothing to pair `fn_80142288` with. The body is `rstl/vector.hpp`'s one-argument
// `erase` - `erase(it, it + 1)`. The two-argument form still emits as a template for this call;
// its operation is also reproduced directly under retail's `fn_801422D4` name in this block.
extern "C" {
typedef rstl::vector< rstl::pair< CAssetId, TEditorId > > SCinematicStates;

SCinematicStates::iterator fn_801422D4(
    SCinematicStates* self, const SCinematicStates::iterator* first,
    const SCinematicStates::iterator* last) {
  SCinematicStates::iterator src = *last;
  const int start = first->get_pointer() - self->mItems;
  int newCount = start;
  SCinematicStates::iterator dst(self->mItems + start);
  for (; src.get_pointer() != self->mItems + self->mCount; ++dst, ++newCount, ++src) {
    dst->first = src->first;
    dst->second = src->second;
  }
  self->mCount = newCount;
  return *first;
}

SCinematicStates::iterator fn_80142288(SCinematicStates* self, SCinematicStates::iterator it) {
  return self->erase(it, it + 1);
}
} // extern "C"

void CPersistentOptions::SetCinematicState(rstl::pair< CAssetId, TEditorId > cinematicId,
                                           bool state) {
  for (SCinematicStates::iterator it = mCinematicStates.begin(); it != mCinematicStates.end();
       ++it) {
    if (*it == cinematicId) {
      if (!state) {
        fn_80142288(&mCinematicStates, it);
      }
      return;
    }
  }
  if (state) {
    mCinematicStates.reserve(mCinematicStates.size() + 1);
    // Retail's `reserve(count+1)` is followed by an inline store of the new last element
    // (0x80142244, then 0x80142248..0x8014226C) with no capacity test, so this is
    // `push_back_unsafe`, not `push_back`.
    mCinematicStates.push_back_unsafe(cinematicId);
  }
}
