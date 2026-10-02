#include "MetroidPrime/CParticleDatabase.hpp"

#include "Kyoto/Math/CFrustumPlanes.hpp"
#include "Kyoto/Particles/CElementGen.hpp"
#include "Kyoto/Particles/CParticleGen.hpp"
#include "MetroidPrime/CParticleGenInfo.hpp"

#include "rstl/rc_ptr.hpp"
#include "rstl/vector.hpp"

#include "Kyoto/CSimplePool.hpp"
#include "Kyoto/SObjectTag.hpp"

/**
 * A same-layout view of the one derived member `SetParticleExternalParam` needs.
 *
 * `GetParticleEffect` hands this unit a `CParticleGenInfo*`, but retail reads the generation
 * system's two words at **+0x78**: `fn_800A7A7C` copies `0x78`/`0x7C`, and
 * `CParticleGenInfoGeneric::~CParticleGenInfoGeneric` (0x800A6038) releases `this+0x78` through
 * `fn_800A6444` and reads its first word as a vtable pointer
 * (`CParticleGenInfoGeneric::IsSystemDeletable`, 0x800A5A34, calls through it). That is
 * `CParticleGenInfoGeneric::mSystem`, a private `rstl::ncrc_ptr<CParticleGen>` which the class's
 * own `GetParticleSystem()` returns by value, so there is no reference to take here. `rstl::rc_ptr`
 * keeps `mPtr` at +0 and `mRefCount` at +4, which is what `rstl::CRcPtrData` is a view of; the
 * view is eight bytes and changes no layout. See `include/rstl/rc_ptr.hpp`.
 */
struct SGenericParticleGenInfo {
  char mBase[0x78];
  rstl::CRcPtrData mSystem;
};

/**
 * A same-layout view of `rstl::optional_object< CAABox >`.
 *
 * `include/rstl/optional_object.hpp` keeps `m_data` (24 bytes, one `CAABox`) at +0 and `m_valid`
 * at +24, which is what retail's out-of-line assignment reads (`lbz r0,24(r4)`, then the six-word
 * `CAABox` copy at +0..+20). The view exists only so that `fn_800A6670` can hold the body without
 * calling the template's own `operator=`: mwcceppc emits that as a separate COMDAT
 * (`__as__Q24rstl24optional_object<6CAABox>F`, byte-identical to retail's function) instead of
 * inlining it, and the object has to *define* retail's name to pair. `#pragma inline_max_size`
 * forces the inline but is file-scoped and drops `GetParticleEffect` 99.01% -> 0.00% by inlining
 * `red_black_tree::find` into it as well.
 */
struct SOptionalAABox {
  CAABox mData;
  bool mValid;
};

/**
 * Same-layout views of `rstl::red_black_tree<T, pair<T,V>, 0, ...>`, its `node` and its `header`.
 *
 * `rstl::map` is twenty bytes: `mSelector`, `mCmp` and `mAllocator` are empty and MWCC gives the
 * three of them four bytes, `mCount` is at +4 and `mHeader` at +8 - which every `addi rX, map, 8`
 * in retail's own `CacheParticleDesc` (0x800A947C) and `GetTotalBounds` (0x800A64A8) confirms.
 * A node is twenty-eight bytes: three pointers and a four-byte `node_color`, then the
 * `pair<CAssetId, rc_ptr<TLockedToken<TDesc>>>` at +16, whose `CAssetId` is the tree's selector
 * key at +0, `rc_ptr::mPtr` at +4 and `rc_ptr::mRefCount` at +8.
 *
 * The views exist so the out-of-line copies of `insert_into`, `create_node` and `node`'s
 * constructor can be *written* here under retail's linker-local names. `red_black_tree` keeps
 * `node` and `mHeader` private, and mwcceppc will not accept a `void*` for a private pointer type
 * ("illegal implicit conversion"), so without a view these bodies cannot be written at all outside
 * the template - and the template's own copies only get mangled COMDAT names, which objdiff does
 * not pair with retail's `fn_800A****` placeholders.
 */
struct SRbValue {
  uint mKey;
  void* mPtr;
  int* mRefCount;
};

struct SRbNode {
  SRbNode* mLeft;
  SRbNode* mRight;
  SRbNode* mParent;
  uint mColor;
  SRbValue mValue;
};

struct SRbHeader {
  SRbNode* mLeftmost;
  SRbNode* mRightmost;
  SRbNode* mRoot;
};

struct SRbTree {
  char mEmptyMembers[4];
  int mCount;
  SRbHeader mHeader;
};

/** `pair<iterator, bool>`, the three words `insert_into` returns through the SRET pointer. */
struct SInsertResult {
  SRbNode* mNode;
  SRbHeader* mHeader;
  bool mInserted;
  SInsertResult() : mNode(nullptr), mHeader(nullptr), mInserted(false) {}
};

/**
 * The two `bool` payloads `insert_into` returns, as objects rather than literals.
 *
 * Retail stores each one with `lbz r0,0(<rodata>)` and never with `li r0,1` - three separate
 * relocation sites in `fn_800AAE34` (`lbl_80418018`, `lbl_80418019`, `lbl_8041801A` in dtk's object),
 * one per return statement, so the compiler materialised a temporary per site. Passing the value
 * as an argument is what reproduces the load; `li r0,1` instead costs two instructions' worth of
 * register pressure and puts the `&mHeader` address in r3 where retail has r0.
 */
static bool sbTrue = true;
static bool sbFalse = false;

extern "C" SRbNode* fn_800AB204(SRbTree* self, SRbNode* left, SRbNode* right, SRbNode* parent,
                                uint color, const SRbValue* value);
extern "C" void fn_800AB268(SRbNode* n, SRbNode* left, SRbNode* right, SRbNode* parent, uint color,
                            const SRbValue* value);
extern "C" SInsertResult fn_800AB05C(SRbTree* self, SRbNode* start, const SRbValue* value);

/**
 * Which insertion a description map's `CacheParticleDesc` walker performs.
 *
 * Retail emits one out-of-line `insert_into` per map and the linker kept them local, so the map
 * names them `fn_800AB05C` (PART), `fn_800AB2AC` (SWHC), `fn_800AB4FC` (ELSC), `fn_800AB74C` (SPSC)
 * and `fn_800AB99C` (SRSC); the two `CacheParticleDesc` overloads' ten callees are their only
 * callers, and the two `CacheParticleDesc` bodies only `bl` those. The primary template is
 * `rstl::map::insert`, which is what every map but `mParticleDescs` still uses here.
 */
template < typename TDesc >
struct DescMapOps {
  typedef rstl::map< CAssetId, rstl::rc_ptr< TLockedToken< TDesc > > > DescMap;
  typedef rstl::pair< CAssetId, rstl::rc_ptr< TLockedToken< TDesc > > > DescValue;
  static void InsertInto(DescMap& descs, const DescValue& value) { descs.insert(value); }
};

template <>
struct DescMapOps< CGenDescription > {
  typedef rstl::map< CAssetId, rstl::rc_ptr< TLockedToken< CGenDescription > > > DescMap;
  typedef rstl::pair< CAssetId, rstl::rc_ptr< TLockedToken< CGenDescription > > > DescValue;
  static void InsertInto(DescMap& descs, const DescValue& value) {
    SRbTree& tree = reinterpret_cast< SRbTree& >(descs);
    fn_800AB05C(&tree, tree.mHeader.mRoot, reinterpret_cast< const SRbValue* >(&value));
  }
};

#define DEFINE_DESC_MAP_OPS(TDESC, INSERT)                                                 \
  template <>                                                                              \
  struct DescMapOps< TDESC > {                                                              \
    typedef rstl::map< CAssetId, rstl::rc_ptr< TLockedToken< TDESC > > > DescMap;            \
    typedef rstl::pair< CAssetId, rstl::rc_ptr< TLockedToken< TDESC > > > DescValue;          \
    static void InsertInto(DescMap& descs, const DescValue& value) {                        \
      SRbTree& tree = reinterpret_cast< SRbTree& >(descs);                                  \
      INSERT(&tree, tree.mHeader.mRoot, reinterpret_cast< const SRbValue* >(&value));       \
    }                                                                                       \
  }

extern "C" SInsertResult fn_800AB2AC(SRbTree* self, SRbNode* start, const SRbValue* value);
extern "C" SInsertResult fn_800AB4FC(SRbTree* self, SRbNode* start, const SRbValue* value);
extern "C" SInsertResult fn_800AB74C(SRbTree* self, SRbNode* start, const SRbValue* value);
extern "C" SInsertResult fn_800AB99C(SRbTree* self, SRbNode* start, const SRbValue* value);

DEFINE_DESC_MAP_OPS(CSwooshDescription, fn_800AB2AC);
DEFINE_DESC_MAP_OPS(CElectricDescription, fn_800AB4FC);
DEFINE_DESC_MAP_OPS(CParticleDescriptionSPSC, fn_800AB74C);
DEFINE_DESC_MAP_OPS(CParticleDescriptionSRSC, fn_800AB99C);

/** 0x800A6444, retail's lowest function in the unit, so defined at the end of this file. */
extern "C" void fn_800A6444(rstl::CRcPtrData* self);
/** 0x800A7A7C, immediately below `SetParticleExternalParam` in retail, so defined after it. */
extern "C" void fn_800A7A7C(rstl::CRcPtrData* dest, CParticleGenInfo* src);
/**
 * 0x800A6670, `optional_object<CAABox>::operator=`, below `AccumulateBounds` in retail, so
 * defined after it. See the definition for why the name is retail's.
 */
extern "C" void fn_800A6670(rstl::optional_object< CAABox >* dest,
                            const rstl::optional_object< CAABox >& src);

/**
 * Retail's three out-of-line tree members, written once per description map under retail's
 * linker-local names: fifteen functions, five identical bodies.
 *
 * The map names `insert_into`, `create_node` and `node`'s constructor for every instantiation
 * separately (`fn_800AB05C`/`fn_800AB204`/`fn_800AB268` for PART, then `AB2AC`/`AB454`/`AB4B8` for
 * SWHC, `AB4FC`/`AB6A4`/`AB708` for ELSC, `AB74C`/`AB8F4`/`AB958` for SPSC and
 * `AB99C`/`ABB44`/`ABBA8` for SRSC) because the linker kept every out-of-line template member
 * local, and objdiff will not pair a mangled COMDAT with a `fn_800A****` placeholder. Only the two
 * `CacheParticleDesc` overloads' ten walkers reach them, through `DescMapOps`.
 *
 * `insert_into` is `include/rstl/red_black_tree.hpp:357-399` with `node` and `header` reached
 * through the `SRbTree` view, because both are private and mwcceppc refuses a `void*` for a private
 * pointer type. `U` is 0 for every `rstl::map`, so the `!U &&` half of the duplicate-key test is
 * gone and the comparison collapses to `item->mKey < n->mValue.mKey` and its reverse - retail's
 * `xor/cntlzw/slw/srwi.` pair. `create_node` is lines 298-303, and `node`'s constructor is its four
 * link stores plus the value copy (the `CAssetId` key and both `rc_ptr` words, then
 * `++*mRefCount`) behind the `addic.`/`beqlr` null test mwcceppc puts on `&mValue` - the same shape
 * retail's `free_node_and_sub_nodes` has on the way out (`addic. r0,r31,16` / `beq`).
 *
 * Within one map's group the source order is ctor, `create_node`, `insert_into`, because mwcceppc
 * emits in reverse source order and retail's .text runs `insert_into`, `create_node`, ctor.
 */

/** SRSC (`CParticleDescriptionSRSC`), retail's highest group in .text. */
extern "C" void fn_800ABBA8(SRbNode* node, SRbNode* left, SRbNode* right, SRbNode* parent, uint color,
                         const SRbValue* value) {
  node->mLeft = left;
  node->mRight = right;
  node->mParent = parent;
  node->mColor = color;
  SRbValue* dest = &node->mValue;
  if (dest != nullptr) {
    dest->mKey = value->mKey;
    dest->mPtr = value->mPtr;
    dest->mRefCount = value->mRefCount;
    ++(*dest->mRefCount);
  }
}

extern "C" SRbNode* fn_800ABB44(SRbTree* self, SRbNode* left, SRbNode* right, SRbNode* parent,
                             uint color, const SRbValue* value) {
  SRbNode* node;
  rstl::rmemory_allocator::allocate(node, 1);
  if (node != nullptr) {
    fn_800ABBA8(node, left, right, parent, color, value);
  }
  return node;
}

extern "C" SInsertResult fn_800AB99C(SRbTree* self, SRbNode* start, const SRbValue* item) {
  SInsertResult res;
  if (start == nullptr) {
    self->mHeader.mRoot = fn_800ABB44(self, nullptr, nullptr, nullptr, 0, item);
    self->mCount += 1;
    self->mHeader.mLeftmost = self->mHeader.mRoot;
    self->mHeader.mRightmost = self->mHeader.mRoot;
    res.mNode = self->mHeader.mRoot;
    res.mHeader = &self->mHeader;
    res.mInserted = sbTrue;
    return res;
  }
  SRbNode* n = start;
  SRbNode* newNode = nullptr;
  while (newNode == nullptr) {
    const bool firstComp = item->mKey < n->mValue.mKey;
    if (!firstComp && !(n->mValue.mKey < item->mKey)) {
      res.mNode = n;
      res.mHeader = &self->mHeader;
      res.mInserted = sbFalse;
      return res;
    }
    if (firstComp) {
      if (n->mLeft == nullptr) {
        newNode = fn_800ABB44(self, nullptr, nullptr, n, 1, item);
        n->mLeft = newNode;
        if (n == self->mHeader.mLeftmost) {
          self->mHeader.mLeftmost = newNode;
        }
      } else {
        n = n->mLeft;
      }
    } else {
      if (n->mRight == nullptr) {
        newNode = fn_800ABB44(self, nullptr, nullptr, n, 1, item);
        n->mRight = newNode;
        if (n == self->mHeader.mRightmost) {
          self->mHeader.mRightmost = newNode;
        }
      } else {
        n = n->mRight;
      }
    }
  }
  self->mCount += 1;
  rstl::rbtree_rebalance(&self->mHeader, newNode);
  res.mNode = newNode;
  res.mHeader = &self->mHeader;
  res.mInserted = sbTrue;
  return res;
}

/** SPSC (`CParticleDescriptionSPSC`). */
extern "C" void fn_800AB958(SRbNode* node, SRbNode* left, SRbNode* right, SRbNode* parent, uint color,
                         const SRbValue* value) {
  node->mLeft = left;
  node->mRight = right;
  node->mParent = parent;
  node->mColor = color;
  SRbValue* dest = &node->mValue;
  if (dest != nullptr) {
    dest->mKey = value->mKey;
    dest->mPtr = value->mPtr;
    dest->mRefCount = value->mRefCount;
    ++(*dest->mRefCount);
  }
}

extern "C" SRbNode* fn_800AB8F4(SRbTree* self, SRbNode* left, SRbNode* right, SRbNode* parent,
                             uint color, const SRbValue* value) {
  SRbNode* node;
  rstl::rmemory_allocator::allocate(node, 1);
  if (node != nullptr) {
    fn_800AB958(node, left, right, parent, color, value);
  }
  return node;
}

extern "C" SInsertResult fn_800AB74C(SRbTree* self, SRbNode* start, const SRbValue* item) {
  SInsertResult res;
  if (start == nullptr) {
    self->mHeader.mRoot = fn_800AB8F4(self, nullptr, nullptr, nullptr, 0, item);
    self->mCount += 1;
    self->mHeader.mLeftmost = self->mHeader.mRoot;
    self->mHeader.mRightmost = self->mHeader.mRoot;
    res.mNode = self->mHeader.mRoot;
    res.mHeader = &self->mHeader;
    res.mInserted = sbTrue;
    return res;
  }
  SRbNode* n = start;
  SRbNode* newNode = nullptr;
  while (newNode == nullptr) {
    const bool firstComp = item->mKey < n->mValue.mKey;
    if (!firstComp && !(n->mValue.mKey < item->mKey)) {
      res.mNode = n;
      res.mHeader = &self->mHeader;
      res.mInserted = sbFalse;
      return res;
    }
    if (firstComp) {
      if (n->mLeft == nullptr) {
        newNode = fn_800AB8F4(self, nullptr, nullptr, n, 1, item);
        n->mLeft = newNode;
        if (n == self->mHeader.mLeftmost) {
          self->mHeader.mLeftmost = newNode;
        }
      } else {
        n = n->mLeft;
      }
    } else {
      if (n->mRight == nullptr) {
        newNode = fn_800AB8F4(self, nullptr, nullptr, n, 1, item);
        n->mRight = newNode;
        if (n == self->mHeader.mRightmost) {
          self->mHeader.mRightmost = newNode;
        }
      } else {
        n = n->mRight;
      }
    }
  }
  self->mCount += 1;
  rstl::rbtree_rebalance(&self->mHeader, newNode);
  res.mNode = newNode;
  res.mHeader = &self->mHeader;
  res.mInserted = sbTrue;
  return res;
}

/** ELSC (`CElectricDescription`). */
extern "C" void fn_800AB708(SRbNode* node, SRbNode* left, SRbNode* right, SRbNode* parent, uint color,
                         const SRbValue* value) {
  node->mLeft = left;
  node->mRight = right;
  node->mParent = parent;
  node->mColor = color;
  SRbValue* dest = &node->mValue;
  if (dest != nullptr) {
    dest->mKey = value->mKey;
    dest->mPtr = value->mPtr;
    dest->mRefCount = value->mRefCount;
    ++(*dest->mRefCount);
  }
}

extern "C" SRbNode* fn_800AB6A4(SRbTree* self, SRbNode* left, SRbNode* right, SRbNode* parent,
                             uint color, const SRbValue* value) {
  SRbNode* node;
  rstl::rmemory_allocator::allocate(node, 1);
  if (node != nullptr) {
    fn_800AB708(node, left, right, parent, color, value);
  }
  return node;
}

extern "C" SInsertResult fn_800AB4FC(SRbTree* self, SRbNode* start, const SRbValue* item) {
  SInsertResult res;
  if (start == nullptr) {
    self->mHeader.mRoot = fn_800AB6A4(self, nullptr, nullptr, nullptr, 0, item);
    self->mCount += 1;
    self->mHeader.mLeftmost = self->mHeader.mRoot;
    self->mHeader.mRightmost = self->mHeader.mRoot;
    res.mNode = self->mHeader.mRoot;
    res.mHeader = &self->mHeader;
    res.mInserted = sbTrue;
    return res;
  }
  SRbNode* n = start;
  SRbNode* newNode = nullptr;
  while (newNode == nullptr) {
    const bool firstComp = item->mKey < n->mValue.mKey;
    if (!firstComp && !(n->mValue.mKey < item->mKey)) {
      res.mNode = n;
      res.mHeader = &self->mHeader;
      res.mInserted = sbFalse;
      return res;
    }
    if (firstComp) {
      if (n->mLeft == nullptr) {
        newNode = fn_800AB6A4(self, nullptr, nullptr, n, 1, item);
        n->mLeft = newNode;
        if (n == self->mHeader.mLeftmost) {
          self->mHeader.mLeftmost = newNode;
        }
      } else {
        n = n->mLeft;
      }
    } else {
      if (n->mRight == nullptr) {
        newNode = fn_800AB6A4(self, nullptr, nullptr, n, 1, item);
        n->mRight = newNode;
        if (n == self->mHeader.mRightmost) {
          self->mHeader.mRightmost = newNode;
        }
      } else {
        n = n->mRight;
      }
    }
  }
  self->mCount += 1;
  rstl::rbtree_rebalance(&self->mHeader, newNode);
  res.mNode = newNode;
  res.mHeader = &self->mHeader;
  res.mInserted = sbTrue;
  return res;
}

/** SWHC (`CSwooshDescription`). */
extern "C" void fn_800AB4B8(SRbNode* node, SRbNode* left, SRbNode* right, SRbNode* parent, uint color,
                         const SRbValue* value) {
  node->mLeft = left;
  node->mRight = right;
  node->mParent = parent;
  node->mColor = color;
  SRbValue* dest = &node->mValue;
  if (dest != nullptr) {
    dest->mKey = value->mKey;
    dest->mPtr = value->mPtr;
    dest->mRefCount = value->mRefCount;
    ++(*dest->mRefCount);
  }
}

extern "C" SRbNode* fn_800AB454(SRbTree* self, SRbNode* left, SRbNode* right, SRbNode* parent,
                             uint color, const SRbValue* value) {
  SRbNode* node;
  rstl::rmemory_allocator::allocate(node, 1);
  if (node != nullptr) {
    fn_800AB4B8(node, left, right, parent, color, value);
  }
  return node;
}

extern "C" SInsertResult fn_800AB2AC(SRbTree* self, SRbNode* start, const SRbValue* item) {
  SInsertResult res;
  if (start == nullptr) {
    self->mHeader.mRoot = fn_800AB454(self, nullptr, nullptr, nullptr, 0, item);
    self->mCount += 1;
    self->mHeader.mLeftmost = self->mHeader.mRoot;
    self->mHeader.mRightmost = self->mHeader.mRoot;
    res.mNode = self->mHeader.mRoot;
    res.mHeader = &self->mHeader;
    res.mInserted = sbTrue;
    return res;
  }
  SRbNode* n = start;
  SRbNode* newNode = nullptr;
  while (newNode == nullptr) {
    const bool firstComp = item->mKey < n->mValue.mKey;
    if (!firstComp && !(n->mValue.mKey < item->mKey)) {
      res.mNode = n;
      res.mHeader = &self->mHeader;
      res.mInserted = sbFalse;
      return res;
    }
    if (firstComp) {
      if (n->mLeft == nullptr) {
        newNode = fn_800AB454(self, nullptr, nullptr, n, 1, item);
        n->mLeft = newNode;
        if (n == self->mHeader.mLeftmost) {
          self->mHeader.mLeftmost = newNode;
        }
      } else {
        n = n->mLeft;
      }
    } else {
      if (n->mRight == nullptr) {
        newNode = fn_800AB454(self, nullptr, nullptr, n, 1, item);
        n->mRight = newNode;
        if (n == self->mHeader.mRightmost) {
          self->mHeader.mRightmost = newNode;
        }
      } else {
        n = n->mRight;
      }
    }
  }
  self->mCount += 1;
  rstl::rbtree_rebalance(&self->mHeader, newNode);
  res.mNode = newNode;
  res.mHeader = &self->mHeader;
  res.mInserted = sbTrue;
  return res;
}

/** PART (`CGenDescription`), `mParticleDescs`. */
extern "C" void fn_800AB268(SRbNode* node, SRbNode* left, SRbNode* right, SRbNode* parent, uint color,
                         const SRbValue* value) {
  node->mLeft = left;
  node->mRight = right;
  node->mParent = parent;
  node->mColor = color;
  SRbValue* dest = &node->mValue;
  if (dest != nullptr) {
    dest->mKey = value->mKey;
    dest->mPtr = value->mPtr;
    dest->mRefCount = value->mRefCount;
    ++(*dest->mRefCount);
  }
}

extern "C" SRbNode* fn_800AB204(SRbTree* self, SRbNode* left, SRbNode* right, SRbNode* parent,
                             uint color, const SRbValue* value) {
  SRbNode* node;
  rstl::rmemory_allocator::allocate(node, 1);
  if (node != nullptr) {
    fn_800AB268(node, left, right, parent, color, value);
  }
  return node;
}

extern "C" SInsertResult fn_800AB05C(SRbTree* self, SRbNode* start, const SRbValue* item) {
  SInsertResult res;
  if (start == nullptr) {
    self->mHeader.mRoot = fn_800AB204(self, nullptr, nullptr, nullptr, 0, item);
    self->mCount += 1;
    self->mHeader.mLeftmost = self->mHeader.mRoot;
    self->mHeader.mRightmost = self->mHeader.mRoot;
    res.mNode = self->mHeader.mRoot;
    res.mHeader = &self->mHeader;
    res.mInserted = sbTrue;
    return res;
  }
  SRbNode* n = start;
  SRbNode* newNode = nullptr;
  while (newNode == nullptr) {
    const bool firstComp = item->mKey < n->mValue.mKey;
    if (!firstComp && !(n->mValue.mKey < item->mKey)) {
      res.mNode = n;
      res.mHeader = &self->mHeader;
      res.mInserted = sbFalse;
      return res;
    }
    if (firstComp) {
      if (n->mLeft == nullptr) {
        newNode = fn_800AB204(self, nullptr, nullptr, n, 1, item);
        n->mLeft = newNode;
        if (n == self->mHeader.mLeftmost) {
          self->mHeader.mLeftmost = newNode;
        }
      } else {
        n = n->mLeft;
      }
    } else {
      if (n->mRight == nullptr) {
        newNode = fn_800AB204(self, nullptr, nullptr, n, 1, item);
        n->mRight = newNode;
        if (n == self->mHeader.mRightmost) {
          self->mHeader.mRightmost = newNode;
        }
      } else {
        n = n->mRight;
      }
    }
  }
  self->mCount += 1;
  rstl::rbtree_rebalance(&self->mHeader, newNode);
  res.mNode = newNode;
  res.mHeader = &self->mHeader;
  res.mInserted = sbTrue;
  return res;
}


CParticleDatabase::CParticleDatabase() : mUpdatesEnabled(true), mAnySystemsDrawnWithModel(false) {}

CParticleDatabase::~CParticleDatabase() {}

/**
 * The ten list-walkers the two `CacheParticleDesc` overloads dispatch to.
 *
 * Retail's `CacheParticleDesc(const CParticleResData&)` (0x800A947C, 112 B) is five tail calls -
 * nothing else - and `CacheParticleDesc(const SObjectTag&)` (0x800A93A8, 212 B) is a binary search
 * on the tag's four-character code followed by one of five more. The ten callees are
 * 0x800A9C50/0x800A9DE8/0x800A9F80/0x800AA118/0x800AA2B0 (408 B each, the list form, one per
 * description type) and 0x800AA448/0x800AA5C8/0x800AA748/0x800AA8C8/0x800AAA48 (384 B each, the
 * single-id form). Ten near-identical bodies exist because each is a separate copy of one
 * template, and the linker kept them local, so the map gives none of them a mangled name.
 *
 * The bodies are this: for every id in the list, if the description map has no entry for it, make
 * one out of `gpSimplePool->GetObj(SObjectTag(<type>, id))`. `TDesc` is only ever used as a
 * pointer type - `TLockedToken<T>` holds a `CToken` and a `T*` - so the two guessed description
 * classes (`CParticleDescriptionSPSC`, `CParticleDescriptionSRSC`) stay forward declarations and
 * no layout is invented for them.
 */
/**
 * Which insertion a description map's `CacheParticleDesc` walker performs: see `DescMapOps` above.
 */
template < typename TDesc, uint Type >
static void CacheParticleDescList(
    const rstl::vector< CAssetId >& ids,
    rstl::map< CAssetId, rstl::rc_ptr< TLockedToken< TDesc > > >& descs) {
  // `rstl::map::value_type` is this pair; naming it directly keeps the dependent type out of the
  // expression, which MWCC and the host compiler spell differently.
  typedef rstl::pair< CAssetId, rstl::rc_ptr< TLockedToken< TDesc > > > DescValue;
  for (rstl::vector< CAssetId >::const_iterator it = ids.begin(); it != ids.end(); ++it) {
    if (descs.find(*it) == descs.end()) {
      const CToken token = gpSimplePool->GetObj(SObjectTag(Type, *it));
      DescMapOps< TDesc >::InsertInto(
          descs, DescValue(*it, rstl::rc_ptr< TLockedToken< TDesc > >(
                              rs_new TLockedToken< TDesc >(token))));
    }
  }
}

/** The single-id form, `CacheParticleDesc(const SObjectTag&)`'s five callees. */
template < typename TDesc, uint Type >
static void CacheParticleDescOne(
    CAssetId id, rstl::map< CAssetId, rstl::rc_ptr< TLockedToken< TDesc > > >& descs) {
  typedef rstl::pair< CAssetId, rstl::rc_ptr< TLockedToken< TDesc > > > DescValue;
  if (descs.find(id) == descs.end()) {
    const CToken token = gpSimplePool->GetObj(SObjectTag(Type, id));
    DescMapOps< TDesc >::InsertInto(
        descs, DescValue(id, rstl::rc_ptr< TLockedToken< TDesc > >(
                             rs_new TLockedToken< TDesc >(token))));
  }
}

void CParticleDatabase::CacheParticleDesc(const CCharacterInfo::CParticleResData& data) {
  CacheParticleDescList< CGenDescription, 'PART' >(data.GetPartIds(), mParticleDescs);
  CacheParticleDescList< CSwooshDescription, 'SWHC' >(data.GetSwhcIds(), mSwooshDescs);
  CacheParticleDescList< CElectricDescription, 'ELSC' >(data.GetElscAIds(), mElectricDescs);
  CacheParticleDescList< CParticleDescriptionSPSC, 'SPSC' >(data.GetSpscIds(), mSpscDescs);
  CacheParticleDescList< CParticleDescriptionSRSC, 'SRSC' >(data.GetSrscIds(), mSrscDescs);
}

void CParticleDatabase::CacheParticleDesc(const SObjectTag& tag) {
  // Retail reads both halves of the tag before the dispatch: `lwz r5,4(r4)` (the id) and
  // `mr r4,r3` (this) sit between the first `cmpw` and the first `beq`, so the five cases only
  // ever `mr r3,r5` and `addi r4,r4,<map offset>`.
  const CAssetId id = tag.GetId();
  switch (tag.GetType()) {
  case 'PART':
    CacheParticleDescOne< CGenDescription, 'PART' >(id, mParticleDescs);
    break;
  case 'SWHC':
    CacheParticleDescOne< CSwooshDescription, 'SWHC' >(id, mSwooshDescs);
    break;
  case 'ELSC':
    CacheParticleDescOne< CElectricDescription, 'ELSC' >(id, mElectricDescs);
    break;
  case 'SPSC':
    CacheParticleDescOne< CParticleDescriptionSPSC, 'SPSC' >(id, mSpscDescs);
    break;
  case 'SRSC':
    CacheParticleDescOne< CParticleDescriptionSRSC, 'SRSC' >(id, mSrscDescs);
    break;
  }
}

void CParticleDatabase::InsertParticleGen(bool oneShot, int flags, uint name,
                                          const rstl::auto_ptr< CParticleGenInfo >& gen) {
  DrawMap* map;
  if (oneShot) {
    switch (flags & 0x60) {
    case 0x20:
      map = &mFirstDraw;
      break;
    case 0x40:
      map = &mLastDraw;
      break;
    default:
      map = &mRendererDraw;
      break;
    }
  } else {
    switch (flags & 0x60) {
    case 0x20:
      map = &mFirstDrawLoop;
      break;
    case 0x40:
      map = &mLastDrawLoop;
      break;
    default:
      map = &mRendererDrawLoop;
      break;
    }
  }
  map->insert(DrawMap::value_type(name, gen));
  if (flags & 0x60)
    mAnySystemsDrawnWithModel = true;
}

void CParticleDatabase::AddParticleEffect(uint name, int flags, const CParticleData& data,
                                          const CVector3f& scale, CStateManager* mgr,
                                          TAreaId areaId, bool oneShot, uint lightId) {
  // TODO: cached PART/SWHC/ELSC/SPSC/SRSC construction and effect initialization.
}

void CParticleDatabase::AddParticleEffect(uint name, int flags, const CPositionalParticleData& data,
                                          const CVector3f& scale, CStateManager* mgr,
                                          TAreaId areaId, uint lightId) {
  // TODO: instantiate a positional PART effect from the cached description.
}

CParticleGenInfo* CParticleDatabase::GetParticleEffect(uint name) {
  {
    DrawMap::iterator it = mRendererDrawLoop.find(name);
    if (it != mRendererDrawLoop.end())
      return it->second.get();
  }
  {
    DrawMap::iterator it = mFirstDrawLoop.find(name);
    if (it != mFirstDrawLoop.end())
      return it->second.get();
  }
  {
    DrawMap::iterator it = mLastDrawLoop.find(name);
    if (it != mLastDrawLoop.end())
      return it->second.get();
  }
  {
    DrawMap::iterator it = mRendererDraw.find(name);
    if (it != mRendererDraw.end())
      return it->second.get();
  }
  {
    DrawMap::iterator it = mFirstDraw.find(name);
    if (it != mFirstDraw.end())
      return it->second.get();
  }
  {
    DrawMap::iterator it = mLastDraw.find(name);
    if (it != mLastDraw.end())
      return it->second.get();
  }
  return nullptr;
}

void CParticleDatabase::SetParticleEffectState(CParticleGenInfo* effect, bool active,
                                               CStateManager* mgr) {
  if (effect == nullptr)
    return;
  effect->SetParticleEmission(active, mgr);
  effect->SetIsActive(active);
  if (!active && (effect->GetFlags() & 1))
    effect->DestroyParticles();
  effect->SetIsGrabInitialData(true);
}

void CParticleDatabase::SetParticleEffectState(uint name, bool active, CStateManager* mgr) {
  SetParticleEffectState(GetParticleEffect(name), active, mgr);
}

void CParticleDatabase::SetParticleExternalParam(uint name, int index, float value) {
  CParticleGenInfo* effect = GetParticleEffect(name);
  if (effect == nullptr) {
    return;
  }
  // Retail's 0x800A7AA0: the effect's `mSystem` pair is copied out by an out-of-line call, its first
  // word is kept, the pair is released, and only then is the parameter set. `fn_800A7A7C` is the
  // out-of-line `rstl::rc_ptr` copy constructor (`rstl::CRcPtrData::CopyInto` is the same nine
  // instructions at 0x80049010, but a distinct symbol here) and `fn_800A6444` is its release.
  rstl::CRcPtrData system;
  fn_800A7A7C(&system, effect);
  CElementGen* gen = static_cast< CElementGen* >(system.x0_ptr);
  fn_800A6444(&system);
  gen->SetExternalParam(index, value);
}

/**
 * 0x800A7A7C, 36 bytes - the out-of-line `rstl::rc_ptr` copy constructor this unit calls.
 *
 * `rstl::CRcPtrData::CopyInto` (0x80049010, `src/rstl/rc_ptr_copy.cpp`) is retail's shared copy,
 * one for every `T`; this is the second, distinct symbol with the same nine instructions, reached
 * only from `SetParticleExternalParam` above. Same body, same ABI (r3 = destination, r4 = source),
 * for the reason in `docs/research/rc_ptr.md`: the words are not inside a template here.
 *
 * The `+0x78` is the source's own member offset, not the caller's: retail's caller passes the
 * effect pointer unchanged, so the two words are read as `0x78(src)`/`0x7C(src)` here.
 */
extern "C" void fn_800A7A7C(rstl::CRcPtrData* dest, CParticleGenInfo* src) {
  const SGenericParticleGenInfo& info = *reinterpret_cast< const SGenericParticleGenInfo* >(src);
  dest->x0_ptr = info.mSystem.x0_ptr;
  dest->x4_refCount = info.mSystem.x4_refCount;
  ++(*dest->x4_refCount);
}

void CParticleDatabase::Update(float dt, CAnimData& animData, const CCharLayoutInfo& layout,
                               const CTransform4f& xf, const CVector3f& scale, CStateManager* mgr) {
  if (!mUpdatesEnabled)
    return;
  UpdateParticleGenDB(dt, animData, layout, xf, scale, mgr, mRendererDrawLoop, true);
  UpdateParticleGenDB(dt, animData, layout, xf, scale, mgr, mFirstDrawLoop, true);
  UpdateParticleGenDB(dt, animData, layout, xf, scale, mgr, mLastDrawLoop, true);
  UpdateParticleGenDB(dt, animData, layout, xf, scale, mgr, mRendererDraw, false);
  UpdateParticleGenDB(dt, animData, layout, xf, scale, mgr, mFirstDraw, false);
  UpdateParticleGenDB(dt, animData, layout, xf, scale, mgr, mLastDraw, false);
  mAnySystemsDrawnWithModel =
      mFirstDrawLoop.size() || mLastDrawLoop.size() || mFirstDraw.size() || mLastDraw.size();
}

void CParticleDatabase::UpdateParticleGenDB(float dt, CAnimData& animData,
                                            const CCharLayoutInfo& layout, const CTransform4f& xf,
                                            const CVector3f& scale, CStateManager* mgr,
                                            DrawMap& map, bool deleteIfDone) {
  // TODO: segment transforms, parenting, lifetime and deletion.
}

void CParticleDatabase::AddToRendererClipped(const CFrustumPlanes& frustum) const {
  AddToRendererClippedParticleGenMap(mRendererDraw, frustum);
  AddToRendererClippedParticleGenMap(mRendererDrawLoop, frustum);
}

void CParticleDatabase::RenderSystemsNormallyAddedToRenderer() const {
  RenderParticleGenMap(mRendererDraw);
  RenderParticleGenMap(mRendererDrawLoop);
}

void CParticleDatabase::AddToRendererClippedMasked(const CFrustumPlanes& frustum, uint mask,
                                                   uint target) const {
  AddToRendererClippedParticleGenMapMasked(mRendererDraw, frustum, mask, target);
  AddToRendererClippedParticleGenMapMasked(mRendererDrawLoop, frustum, mask, target);
}

void CParticleDatabase::AddToRendererClippedParticleGenMap(const DrawMap& map,
                                                           const CFrustumPlanes& frustum) const {
  for (DrawMap::const_iterator it = map.begin(); it != map.end(); ++it) {
    CParticleGenInfo* const gen = it->second.get();
    if (frustum.BoxInFrustumPlanes(gen->GetBounds()) == true)
      gen->AddToRenderer();
  }
}

void CParticleDatabase::AddToRendererClippedParticleGenMapMasked(const DrawMap& map,
                                                                 const CFrustumPlanes& frustum,
                                                                 uint mask, uint target) const {
  for (DrawMap::const_iterator it = map.begin(); it != map.end(); ++it) {
    CParticleGenInfo* const gen = it->second.get();
    if ((gen->GetFlags() & mask) == target &&
        frustum.BoxInFrustumPlanes(gen->GetBounds()) == true)
      gen->AddToRenderer();
  }
}

void CParticleDatabase::RenderSystemsToBeDrawnFirst() const {
  RenderParticleGenMap(mFirstDraw);
  RenderParticleGenMap(mFirstDrawLoop);
}

void CParticleDatabase::RenderSystemsToBeDrawnFirstPOICheck(uint mask, uint target) const {
  RenderParticleGenMapMasked(mFirstDraw, mask, target);
  RenderParticleGenMapMasked(mFirstDrawLoop, mask, target);
}

void CParticleDatabase::RenderSystemsToBeDrawnLast() const {
  RenderParticleGenMap(mLastDraw);
  RenderParticleGenMap(mLastDrawLoop);
}

void CParticleDatabase::RenderSystemsToBeDrawnLastPOICheck(uint mask, uint target) const {
  RenderParticleGenMapMasked(mLastDraw, mask, target);
  RenderParticleGenMapMasked(mLastDrawLoop, mask, target);
}

void CParticleDatabase::RenderParticleGenMap(const DrawMap& map) {
  for (DrawMap::const_iterator it = map.begin(); it != map.end(); ++it) {
    it->second->Render();
  }
}

void CParticleDatabase::RenderParticleGenMapMasked(const DrawMap& map, uint mask, uint target) {
  for (DrawMap::const_iterator it = map.begin(); it != map.end(); ++it) {
    if ((it->second->GetFlags() & mask) == target)
      it->second->Render();
  }
}

void CParticleDatabase::DeleteAllLights(CStateManager* mgr) {
  DeleteAllLightsForParticleDB(mgr, mRendererDrawLoop);
  DeleteAllLightsForParticleDB(mgr, mFirstDrawLoop);
  DeleteAllLightsForParticleDB(mgr, mLastDrawLoop);
  DeleteAllLightsForParticleDB(mgr, mRendererDraw);
  DeleteAllLightsForParticleDB(mgr, mFirstDraw);
  DeleteAllLightsForParticleDB(mgr, mLastDraw);
}

void CParticleDatabase::DeleteAllLightsForParticleDB(CStateManager* mgr, const DrawMap& map) {
  for (DrawMap::const_iterator it = map.begin(); map.end() != it; ++it) {
    it->second->DeleteLight(mgr);
  }
}

void CParticleDatabase::SuspendAllActiveEffects(CStateManager* mgr) {
  SuspendAllActiveEffectsForParticleDB(mgr, mRendererDrawLoop);
  SuspendAllActiveEffectsForParticleDB(mgr, mFirstDrawLoop);
  SuspendAllActiveEffectsForParticleDB(mgr, mLastDrawLoop);
}

void CParticleDatabase::SuspendAllActiveEffectsForParticleDB(CStateManager* mgr,
                                                             const DrawMap& map) {
  for (DrawMap::const_iterator it = map.begin(); map.end() != it; ++it) {
    SetParticleEffectState(it->second.get(), false, mgr);
  }
}

void CParticleDatabase::SetModulationColorAllActiveEffects(const CColor& color) {
  SetModulationColorAllActiveEffectsForParticleDB(color, mRendererDrawLoop);
  SetModulationColorAllActiveEffectsForParticleDB(color, mFirstDrawLoop);
  SetModulationColorAllActiveEffectsForParticleDB(color, mLastDrawLoop);
  SetModulationColorAllActiveEffectsForParticleDB(color, mRendererDraw);
  SetModulationColorAllActiveEffectsForParticleDB(color, mFirstDraw);
  SetModulationColorAllActiveEffectsForParticleDB(color, mLastDraw);
}

void CParticleDatabase::SetModulationColorAllActiveEffectsForParticleDB(const CColor& color,
                                                                        const DrawMap& map) {
  for (DrawMap::const_iterator it = map.begin(); map.end() != it; ++it) {
    if (it->second.get())
      it->second->SetModulationColor(color);
  }
}

void CParticleDatabase::DestroyAllActiveParticles() {
  DestroyParticlesForParticleDB(mRendererDrawLoop);
  DestroyParticlesForParticleDB(mFirstDrawLoop);
  DestroyParticlesForParticleDB(mLastDrawLoop);
  DestroyParticlesForParticleDB(mRendererDraw);
  DestroyParticlesForParticleDB(mFirstDraw);
  DestroyParticlesForParticleDB(mLastDraw);
}

void CParticleDatabase::DestroyParticlesForParticleDB(const DrawMap& map) {
  for (DrawMap::const_iterator it = map.begin(); map.end() != it; ++it) {
    it->second->DestroyParticles();
  }
}

void CParticleDatabase::ClearAllNonPersistentEffects(CStateManager* mgr) {
  DeleteAllLightsForParticleDB(mgr, mRendererDrawLoop);
  DeleteAllLightsForParticleDB(mgr, mFirstDrawLoop);
  DeleteAllLightsForParticleDB(mgr, mLastDrawLoop);
  mRendererDrawLoop.clear();
  mFirstDrawLoop.clear();
  mLastDrawLoop.clear();
}

/**
 * 0x800A6670, 156 bytes - `rstl::optional_object< CAABox >::operator=(const optional_object&)`.
 *
 * The body is `include/rstl/optional_object.hpp:30-45`, reached through the `SOptionalAABox` view
 * because mwcceppc otherwise emits it as a separate COMDAT (`__as__Q24rstl24optional_object<6CAABox>F`,
 * byte-identical to retail's function) instead of inlining it. Retail keeps it out of line and
 * `AccumulateBounds` (0x800A65B0) is its only caller in the DOL, so retail's copy is a linker-local
 * symbol the map names `fn_800A6670`; the object has to define that name for the pair to match, the
 * same reason `main.cpp` spells its three releases `fn_80009224`, `fn_80009008` and `fn_800095E4`.
 * Calling it instead of writing `bounds = partBounds;` keeps the caller's `bl` byte-identical.
 */
extern "C" void fn_800A6670(rstl::optional_object< CAABox >* dest,
                            const rstl::optional_object< CAABox >& src) {
  SOptionalAABox& out = *reinterpret_cast< SOptionalAABox* >(dest);
  const SOptionalAABox& in = *reinterpret_cast< const SOptionalAABox* >(&src);
  if (&out == &in) {
    return;
  }
  if (in.mValid) {
    if (!out.mValid) {
      rstl::construct< CAABox >(&out.mData, in.mData);
      out.mValid = true;
    } else {
      out.mData = in.mData;
    }
  } else {
    out.mValid = false;
  }
}

void CParticleDatabase::AccumulateBounds(rstl::optional_object< CAABox >& bounds,
                                         const DrawMap& map) {
  for (DrawMap::const_iterator it = map.begin(); it != map.end(); ++it) {
    rstl::optional_object< CAABox > partBounds = it->second->GetBounds();
    if (!partBounds)
      continue;
    if (!bounds) {
      // Retail calls its own out-of-line `optional_object<CAABox>::operator=` here (0x800A6670,
      // 156 bytes) rather than inlining it; the object has to define that symbol for the pair to
      // match, and the `bl` itself is free.
      fn_800A6670(&bounds, partBounds);
    } else {
      bounds->AccumulateBounds(partBounds->GetMinPoint());
      bounds->AccumulateBounds(partBounds->GetMaxPoint());
    }
  }
}

rstl::optional_object< CAABox > CParticleDatabase::GetTotalBounds() const {
  rstl::optional_object< CAABox > bounds;
  AccumulateBounds(bounds, mFirstDrawLoop);
  AccumulateBounds(bounds, mLastDrawLoop);
  AccumulateBounds(bounds, mFirstDraw);
  AccumulateBounds(bounds, mLastDraw);
  return bounds;
}


/**
 * 0x800A6444, 100 bytes - `rstl::rc_ptr<CParticleGen>::ReleaseData()`.
 *
 * `if (--*mRefCount <= 0) { delete GetPtr(); delete mRefCount; }`, which is retail's own body in
 * `include/rstl/rc_ptr.hpp`. The 0x64 rather than the 0x50 of the 0x50-byte instantiations is
 * `CParticleGen`'s **virtual** destructor, so `delete` goes through the vtable
 * (`lwz r12,0(r3) / li r4,1 / lwz r12,8(r12) / mtctr r12 / bctrl`) behind a null check; a class
 * with a plain destructor gets the direct `bl ~D0` instead and is ten bytes shorter. The map gives
 * this one no mangled name, so the object has to define retail's own placeholder for objdiff to
 * pair it - the same reason `main.cpp` spells its three releases `fn_80009224`, `fn_80009008` and
 * `fn_800095E4`. It is reached from `CParticleGenInfoGeneric::~CParticleGenInfoGeneric` and from
 * every `rc_ptr` temporary in this unit's `AddParticleEffect` pair, one symbol for all of them.
 */
extern "C" void fn_800A6444(rstl::CRcPtrData* self) {
  if (--*self->x4_refCount <= 0) {
    delete static_cast< CParticleGen* >(self->x0_ptr);
    delete self->x4_refCount;
  }
}

