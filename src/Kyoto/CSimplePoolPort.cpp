/**
 * The port's `CSimplePool`: its constructor, its destructor and all nine virtuals. **This file is
 * port-only**: `configure.py` does not declare it, so mwcceppc never sees it and it is not a
 * decompilation unit - the same arrangement as `src/Kyoto/CResFactoryPortVirtuals.cpp`.
 *
 * **It exists because of the vtable.** `CGameGlobalObjects`' constructor
 * (`src/MetroidPrime/CGameGlobalObjectsCtor.cpp`) builds a `CSimplePool` at +0xE4 and
 * `CCharacterFactoryBuilder`'s builds one at +0x04, so listing either asks the host link for
 * `CSimplePool::CSimplePool(IFactory&)` and `CSimplePool::~CSimplePool()`. A body for either
 * stores the vptr, which asks for `vtable for CSimplePool`, and GCC emits that only in the unit
 * defining the key function - the first non-inline virtual, `GetObj(const SObjectTag&,
 * CVParamTransfer)`. So the two constructors and the nine virtuals have to be one file.
 *
 * `src/Kyoto/CSimplePoolCtor.cpp` is retail's constructor (`fn_80301008`, 0x150, `NonMatching`
 * 94.32%), and it is **not** what the host runs: it stores the *guest* vtable addresses
 * (`lbl_803BAF90`, ...) as data operands so that mwcceppc reproduces retail's relocations, and on
 * the host those are not vtables. What it established is used here: the member at +0x1C ends up
 * holding an 8-byte object with a vtable and the owning `CSimplePool*` - a
 * `TObjOwnerParam< IObjectStore* >(this)` - and the hash table's four words start zeroed.
 *
 * **The bodies are the pool's behaviour, not retail's instructions.** Retail's `GetObj`, `HasObject`,
 * `ObjectIsLive`, `ObjectUnreferenced` and `Flush` are 0x80300000-0x80301600 and not written; what
 * they do is the class's contract, and it is the same contract `CObjectReference` and `CToken`
 * (both in the port build) already rely on:
 *
 *  * `GetObj(tag, xfer)` returns a `CToken` on the one `CObjectReference` for `tag`, creating it -
 *    unloaded, with `xfer` as its build parameters - if the pool has none. Nothing is loaded here;
 *    `CObjectReference::Lock`/`GetObject` ask `GetFactory()` for the object when a token locks.
 *  * `ObjectUnreferenced(tag)` is the callback `CObjectReference::RemoveReference` makes when the
 *    last token lets go. It drops the entry; the token deletes the reference.
 *  * `HasObject` is "resident, or the factory can build it"; `ObjectIsLive` is "resident and
 *    loaded".
 *
 * `fn_8029c7e8`, which `CMain::FillInAssetIDs` calls through `gpSimplePool`, is **not** a pool
 * method despite the name the map gives it: at 0x8029C7E8 it takes the store as an argument and
 * calls `GetObj(tag)` through vtable slot 0xC, in the audio code. It stays unwritten.
 */
#include "Kyoto/CSimplePool.hpp"

#include "Kyoto/CObjectReference.hpp"
#include "Kyoto/CResFactory.hpp"

// Declared in `Kyoto/CVParamTransfer.hpp` and asked for by `CObjectReference(const auto_ptr<IObj>&)`
// in the port build; retail has no out-of-line copy (its callers construct the null `rc_ptr` in
// place, `fn_80031F68`). A default `CVParamTransfer` is exactly that: a null `rc_ptr` on the
// shared null refcount.
CVParamTransfer CVParamTransfer::Null() { return CVParamTransfer(); }

namespace {

typedef CSimplePool::ResourceMap::Pair SPoolEntry;
typedef CSimplePool::ResourceMap::table_type::bucket_type SPoolBucket;
typedef CSimplePool::ResourceMap::table_type::bucket_vector SPoolBuckets;

// A power of two. The table is sized on the first insert, so a pool nothing asks of - and
// `CCharacterFactoryBuilder`'s dummy store is one - allocates nothing, as retail's does not.
const int kBucketCount = 1024;

bool TagsEqual(const SObjectTag& a, const SObjectTag& b) { return a.id == b.id && a.type == b.type; }

SPoolBucket* FindBucket(SPoolBuckets& buckets, const SObjectTag& tag) {
  if (buckets.size() == 0) {
    return nullptr;
  }
  return &buckets[static_cast< int >(tag.id & (kBucketCount - 1))];
}

CObjectReference* FindRef(SPoolBuckets& buckets, const SObjectTag& tag) {
  SPoolBucket* bucket = FindBucket(buckets, tag);
  if (bucket == nullptr) {
    return nullptr;
  }
  for (SPoolBucket::iterator it = bucket->begin(); it != bucket->end(); ++it) {
    if (TagsEqual(it->first, tag)) {
      return it->second;
    }
  }
  return nullptr;
}

} // namespace

CSimplePool::CSimplePool(IFactory& factory)
: x4_(0), x5_(0), x18_factory(factory), x1c_paramXfr(CVParamTransfer::Null()) {
  x1c_paramXfr = CVParamTransfer(rs_new TObjOwnerParam< IObjectStore* >(this));
}

CSimplePool::~CSimplePool() { Flush(); }

CToken CSimplePool::GetObj(const SObjectTag& tag, CVParamTransfer xfer) {
  SPoolBuckets& buckets = x8_resources.get_table().buckets();
  CObjectReference* ref = FindRef(buckets, tag);
  if (ref != nullptr) {
    return CToken(ref);
  }

  if (buckets.size() == 0) {
    buckets.resize(kBucketCount);
  }
  ref = rs_new CObjectReference(*this, rstl::auto_ptr< IObj >(), tag, xfer);
  FindBucket(buckets, tag)->push_back(SPoolEntry(tag, ref));
  return CToken(ref);
}

CToken CSimplePool::GetObj(const SObjectTag& tag) { return GetObj(tag, x1c_paramXfr); }

CToken CSimplePool::GetObj(const char* name) { return GetObj(name, x1c_paramXfr); }

CToken CSimplePool::GetObj(const char* name, CVParamTransfer xfer) {
  return GetObj(*GetFactory().GetResourceIdByName(name), xfer);
}

bool CSimplePool::HasObject(const SObjectTag& tag) {
  if (FindRef(x8_resources.get_table().buckets(), tag) != nullptr) {
    return true;
  }
  return x18_factory.CanBuild(tag);
}

bool CSimplePool::ObjectIsLive(const SObjectTag& tag) {
  CObjectReference* ref = FindRef(x8_resources.get_table().buckets(), tag);
  return ref != nullptr && ref->IsLoaded();
}

// Retail's pool keeps nothing that is not referenced - an entry leaves in `ObjectUnreferenced` the
// moment its last token does - so there is nothing for a flush to drop.
void CSimplePool::Flush() {}

void CSimplePool::ObjectUnreferenced(const SObjectTag& tag) {
  SPoolBucket* bucket = FindBucket(x8_resources.get_table().buckets(), tag);
  if (bucket == nullptr) {
    return;
  }
  for (SPoolBucket::iterator it = bucket->begin(); it != bucket->end(); ++it) {
    if (TagsEqual(it->first, tag)) {
      bucket->erase(it);
      return;
    }
  }
}
