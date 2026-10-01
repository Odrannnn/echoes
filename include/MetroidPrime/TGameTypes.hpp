#ifndef _TGAMETYPES
#define _TGAMETYPES

#include "types.h"
#include "rstl/construct.hpp"
#include "rstl/pair.hpp"

class CInputStream;
class COutputStream;

struct TAreaId;
struct TEditorId;
struct TUniqueId;

extern const TAreaId kInvalidAreaId;
extern const TEditorId kInvalidEditorId;
extern const TUniqueId kInvalidUniqueId;
// The "no player" player-index sentinel. Unlike the three above, which are `.sbss` words written
// by the one static initialiser at 0x800E9BF4, this is a plain `const int` in `.sdata2` - the map's
// `lbl_8041B750`, four bytes, value -1 - with **no writer anywhere in the DOL** (all eleven
// references in the image are `lwz` reads). It is seeded as the initial value of a player-index
// search and compared against the result to mean "not found"; see `CScriptTrigger::ClearInhabitants`
// and, for the same meaning, `CScriptTrigger::Touch`'s calls to `AddInhabitant`.
extern const int kInvalidPlayerIndex;

struct TAreaId {
  int value;

  TAreaId() : value(-1) {}
  TAreaId(int value) : value(value) {}
  int Value() const { return value; }

  bool operator==(const TAreaId& other) const { return value == other.value; }
  bool operator!=(const TAreaId& other) const { return value != other.value; }
};
CHECK_SIZEOF(TAreaId, 0x4)

struct TEditorId {
  uint value;

  TEditorId(uint value) : value(value) {}
  TEditorId(CInputStream& in);
  // TODO
  uint Value() const { return value & 0x3FFFFFF; }
  uint Id() const { return value & 0xffff; }
  int AreaNum() const { return (value >> 16) & 0x3ff; }

  void PutTo(COutputStream&) const;

  bool operator==(const TEditorId& other) const { return Value() == other.Value(); }
  bool operator!=(const TEditorId& other) const { return Value() != other.Value(); }
  bool operator<(const TEditorId& other) const { return Value() < other.Value(); }
};
CHECK_SIZEOF(TEditorId, 0x4)

struct TUniqueId {
  ushort value;

  explicit TUniqueId(ushort packed) : value(packed) {}
  TUniqueId(ushort version, ushort id) : value(((version & 0x3F) << 10) | (id & 0x3FF)) {}

  ushort Value() const { return value & 0x3FF; }
  ushort Version() const { return (value >> 10) & 0x3F; }

  bool operator==(const TUniqueId& other) const { return value == other.value; }
  bool operator!=(const TUniqueId& other) const { return value != other.value; }
  bool operator<(const TUniqueId& other) const { return value < other.value; }

private:
};
CHECK_SIZEOF(TUniqueId, 0x2)

namespace rstl {
RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE(TUniqueId)
RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE(TEditorId)
// `CMorphBallShadow`'s `do_insert_before<list<TAreaId>>` (retail 0x8018AB00, 144 B) stores the
// node's value with a plain `stw` and no placement-new null check, which is what the trivial
// `construct_impl` gives; the generic `new (dest) T(src)` path emits the check and 8 bytes more.
RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE(TAreaId)

template <>
struct is_trivially_destructible< pair< TEditorId, bool > > {
  enum { value = true };
};

template <>
inline void construct< pair< TEditorId, bool > >(void* dest, const pair< TEditorId, bool >& src) {
  *static_cast< pair< TEditorId, bool >* >(dest) = src;
}

// `rstl::pair<uint, TEditorId>` is the same shape - two four-byte words - and retail treats it the
// same way: `rstl::vector<rstl::pair<Ui,9TEditorId>, rmemory_allocator>::reserve` at retail
// 0x80008E94 (172 bytes, inlined copy loop) is the copy of the `vector<pair<Ui,Ui>>` one at
// 0x80008DE8, and without this specialization the instantiation in
// `src/MetroidPrime/main.cpp` outlines `uninitialized_copy` instead and comes out 184 bytes.
template <>
struct is_trivially_destructible< pair< uint, TEditorId > > {
  enum { value = true };
};

template <>
inline void construct< pair< uint, TEditorId > >(void* dest, const pair< uint, TEditorId >& src) {
  *static_cast< pair< uint, TEditorId >* >(dest) = src;
}
} // namespace rstl

// struct TGameScriptId {
//   TEditorId editorId;
//   bool b;
// };
// CHECK_SIZEOF(TGameScriptId, 0x8)

typedef ushort TSfxId;
struct TLayerId {
  explicit TLayerId(int value) : mValue(value) {}
  int Value() const { return mValue; }

private:
  int mValue;
};
CHECK_SIZEOF(TLayerId, 0x4)

const TSfxId InvalidSfxId = 0xFFFFu;

#define ALIGN_UP(x, a) (((x) + (a - 1)) & ~(a - 1))

#endif // _TGAMETYPES
