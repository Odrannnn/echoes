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
 * calls `GetObj(tag)` through vtable slot 0xC, in the audio code. It is retail's
 * `CSfxManager::LoadTranslationTable`, identified by its own instructions
 * (`./tools/dis.sh 0x8029C7E8 0x150`) and written at the bottom of this file.
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

// ---------------------------------------------------------------------------
// `CSimplePool::fn_8029c7e8(const SObjectTag&)` - retail 0x8029C7E8, 0x150 = 336 bytes
// ---------------------------------------------------------------------------
//
// **The name is this tree's, the function is audio's.** `config/G2ME01/symbols.txt:11842` names
// the address `fn_8029c7e8__11CSimplePoolFRC10SObjectTag`, and `src/MetroidPrime/
// CMainFillInAssetIDs.cpp:60` calls it as a pool member - that is the symbol the port's link asks
// for, so the definition has to be a `CSimplePool` member and it has to be here. The machine code
// disagrees with the name, and `./tools/dis.sh 0x8029C7E8 0x150` says exactly how: `r3` is used
// **only** as the object of one virtual call (`lwz r12,0(r29)` / `lwz r12,12(r12)` / `bctrl`, with
// `r4=r29, r5=r30` - `GetObj(tag)` through vtable slot 0xC), `r4` is tested as a **null pointer**,
// an `.sbss` vector at `r13-25852` is deleted and zeroed, a fresh `CToken` is stored into an
// `auto_ptr`-shaped static at `r13-25844` (`x0_has` byte then `x4_item` word, exactly `rstl::
// auto_ptr`'s layout), that token is `Lock()`ed, and the function returns 1. That is
// `CSfxManager::LoadTranslationTable(CSimplePool* pool, const SObjectTag* tag)` statement for
// statement - `../MetroidPrimePort/src/Kyoto/Audio/CSfxManager.cpp:703`, which `main.cpp:558` there
// calls the same way, with `gpSimplePool` and
// `gpResourceFactory->GetResourceIdByName("sound_lookup")`.
//
// Retail's five statements, and what the port does with each:
//
//  * `if (!tag) return false;` - **not expressible, and not needed here.** Retail's parameter is a
//    pointer and MP1's caller passes one through; this tree's is a `const SObjectTag&`, which is
//    what `_ZN11CSimplePool11fn_8029c7e8ERK10SObjectTag` encodes, and the only caller dereferences
//    `GetResourceIdByName`'s answer before the call, so a null never reaches this function.
//  * `if (mTranslationTable) delete mTranslationTable; mTranslationTable = nullptr;` - retail's
//    parsed `rstl::vector< short >*` at `lbl_80419884` (`.sbss`, `r13-25852`, resolved with
//    `tools/sda.py`), freed through `fn_80255C00`, a deleting destructor that frees the buffer at
//    `+12` and then the object. **There is nothing to drop on a PC and nothing to build one
//    from.** Its only reader is `CSfxManager::TranslateSFXID`, which is still one of the port's
//    undefined symbols (`docs/research/port_link_baseline.txt`), so the port has no table and does
//    not invent one.
//  * `mTranslationTableTok = rs_new CToken(pool->GetObj(*tag));` - **the part that matters, and
//    the port does it.** The pool gets the reference for the tag and *keeps* it. Discarding the
//    token instead would run `CObjectReference::RemoveReference` -> `CSimplePool::
//    ObjectUnreferenced` on the spot and delete the entry it had just made, and `FillInAssetIDs`
//    would be a no-op. Retail's holder is `lbl_8041988C` (`.sbss`, 8 bytes, resolved with
//    `tools/sda.py`); MP2's `include/Kyoto/Audio/CSfxManager.hpp` declares no such member, and
//    adding one would mean editing a header that `Kyoto/CSimplePoolCtor.cpp` - a `Matching` unit
//    - includes, so the holder lives beside this function instead and is named as retail's.
//  * `mTranslationTableTok->Lock();` - done, with retail's effect: `CObjectReference::Lock` asks
//    the factory to `BuildAsync` because this tag's registry kind is `kSIK_None`
//    (`src/MetroidPrime/PortPoolStandIns.cpp:326`) and there is no object behind it. The port's
//    `CResFactory::BuildAsync` writes null and returns, so the reference is left marked loading
//    over a null object rather than faulting, and nothing on today's boot ladder reads it.
//  * `return true;` - the caller discards it. The declaration in `include/Kyoto/CSimplePool.hpp`
//    is `void`; a `bool` would change no mangled name, but it would edit the header that same
//    `Matching` unit includes, so it stays `void` and this sentence is the record of why.
//
// What is missing is stated rather than hidden: **no `sound_lookup` table is built**, because its
// bytes are in `Strings.pak` and the tag has no object behind it. What the call gets on a PC is
// what `PortPoolStandIns.cpp`'s `sound_lookup_ATBL` entry was written to give - the pool knows
// the tag and cannot build it - and the token above is what keeps that answer from evaporating.
namespace {
rstl::auto_ptr< CToken > s_translationTableTok; // retail's `CSfxManager::mTranslationTableTok`
} // namespace

void CSimplePool::fn_8029c7e8(const SObjectTag& tag) {
  s_translationTableTok = rstl::auto_ptr< CToken >(rs_new CToken(GetObj(tag)));
  s_translationTableTok->Lock();
}
