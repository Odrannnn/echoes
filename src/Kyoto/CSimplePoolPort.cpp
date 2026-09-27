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
 *
 * ---------------------------------------------------------------------------
 * What the two name-taking overloads do on a PC, and why it is a stand-in
 * ---------------------------------------------------------------------------
 *
 * `GetObj(const char* name)` and `GetObj(const char* name, CVParamTransfer)` exist because retail
 * resolves the name itself and hands the pool a tag. **On a PC nothing can do that**: the
 * resolver is `CResFactory::GetResourceIdByName` (0x80006B80) -> `CResLoader::GetResIdByName`
 * (0x802FCC44) -> `CPakFile::GetResIdByName` (0x803236CC), all three written and all three
 * walking a `CPakFile` that comes out of a pak on the disc.
 *
 * So the two overloads consult `port::pool::FindStandInTag` first
 * (`include/Kyoto/CSimplePool.hpp` declares it; `src/MetroidPrime/PortPoolStandIns.cpp` holds the
 * entries and says what each one is standing in for) and fall through to the factory otherwise.
 * The fallback still dereferences the factory's answer with no test, and that is the point: the
 * fault this file used to raise was exactly that dereference, and a null test would have turned
 * a pool with no such resource into a pool with a null reference in its map.
 *
 * `GetObj(const SObjectTag&, CVParamTransfer)` then asks the same registry for an object and
 * hands it to the `CObjectReference` it builds, so a registered tag arrives **already loaded** -
 * `CObjectReference::Lock` sees a non-null `x18_object` and never calls `BuildAsync`, and
 * `GetObject` never calls `Build`. This is the same arrangement
 * `src/MetroidPrime/PortTweakGlobals.cpp` uses for `gpTweakPlayerA`, and for the same reason:
 * a real object at a real address, named in the source as a stand-in, with the missing data
 * written down next to it.
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
  // **The port's stand-in registry gets first refusal, and it is the only thing on a PC that can
  // hand back a non-null object for a tag** - see `include/Kyoto/CSimplePool.hpp`'s block above
  // and `src/MetroidPrime/PortPoolStandIns.cpp` for the ten entries and for what is missing
  // behind each object. A registered stand-in arrives *already built*, so
  // `CObjectReference::x18_object` is non-null from the constructor and neither `Lock()` nor
  // `GetObject()` asks the factory for anything: no `BuildAsync`, no `Build`, no walk of a pak
  // list that is empty. Everything else about the entry is unchanged, and it is still one
  // `CObjectReference` per live tag, which is the contract the header states and which
  // `CObjectReference::RemoveReference` relies on. **An entry whose kind is `kSIK_None` gets a
  // null object here and the next `CToken::GetObj()` faults**, which is the same place retail
  // stands with no pak behind the name - see the registry's comment on that case.
  ref = rs_new CObjectReference(*this, rstl::auto_ptr< IObj >(port::pool::CreateStandInObject(tag)),
                                tag, xfer);
  FindBucket(buckets, tag)->push_back(SPoolEntry(tag, ref));
  return CToken(ref);
}

CToken CSimplePool::GetObj(const SObjectTag& tag) { return GetObj(tag, x1c_paramXfr); }

CToken CSimplePool::GetObj(const char* name) { return GetObj(name, x1c_paramXfr); }

// **The name is resolved against the port's stand-in registry first and the factory second**,
// and the factory's answer is dereferenced with no test, exactly as before. That is deliberate
// and it is the whole point: the fault this function used to raise was
// `*GetFactory().GetResourceIdByName(name)` on a null tag, and a null check here would convert a
// pool that has no such resource into a token that silently refers to nothing. A boot that stops
// with the next unanswerable *name* in the backtrace is worth more than a boot that limps on with
// a null `CObjectReference` in its map.
CToken CSimplePool::GetObj(const char* name, CVParamTransfer xfer) {
  const SObjectTag* const tag = port::pool::FindStandInTag(name);
  if (tag != nullptr) {
    return GetObj(*tag, xfer);
  }
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
