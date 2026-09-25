#include "Kyoto/CDependencyGroup.hpp"

#include "Kyoto/CDependencyGroupToken.hpp"
#include "Kyoto/CFactoryMgr.hpp"
#include "Kyoto/IObjectStore.hpp"
#include "Kyoto/Streams/CInputStream.hpp"

CDependencyGroup::CDependencyGroup(CInputStream& in) { ReadFromStream(in); }

void CDependencyGroup::ReadFromStream(CInputStream& in) {
  int numTags = in.ReadInt32();
  x0_objectTags.reserve(numTags);

  for (int i = 0; i < numTags; ++i) {
    FourCC type = in.ReadInt32();
    CAssetId id = in.ReadInt32();
    x0_objectTags.push_back_unsafe(SObjectTag(type, id));
  }
}

int CDependencyGroup::GetCountForResType(FourCC type) const {
  int ret = 0;
  for (rstl::vector< SObjectTag >::const_iterator it = x0_objectTags.begin();
       it != x0_objectTags.end(); ++it) {
    if (it->type == type) {
      ++ret;
    }
  }

  return ret;
}

CFactoryFnReturn FDependencyGroupFactory(const SObjectTag& tag, CInputStream& in,
                                         const CVParamTransfer& xfer) {
  return rs_new CDependencyGroup(in);
}

CDependencyGroupToken::CDependencyGroupToken(const TToken< CDependencyGroup >& group,
                                             IObjectStore& store)
: x0_group(group), x18_lockCount(0), x1c_loaded(false) {
  const rstl::vector< SObjectTag >& tags = x0_group->GetObjectTagVector();
  x8_dependencies.reserve(tags.size());
  for (int i = 0; i < tags.size(); ++i) {
    x8_dependencies.push_back_unsafe(store.GetObj(tags[i]));
  }
}

void CDependencyGroupToken::Lock() {
  ++x18_lockCount;
  if (x18_lockCount == 1) {
    for (int i = 0; i < x8_dependencies.size(); ++i) {
      x8_dependencies[i].Lock();
    }
    x1c_loaded = false;
  }
}

void CDependencyGroupToken::Unlock() {
  --x18_lockCount;
  if (x18_lockCount == 0) {
    for (int i = 0; i < x8_dependencies.size(); ++i) {
      x8_dependencies[i].Unlock();
    }
    x1c_loaded = false;
  }
}

bool CDependencyGroupToken::IsLocked() const { return x18_lockCount != 0; }

bool CDependencyGroupToken::IsLoaded() {
  if (x18_lockCount == 0) {
    return false;
  }
  if (x1c_loaded) {
    return true;
  }
  bool loaded = true;
  for (int i = 0; i < x8_dependencies.size(); ++i) {
    if (!x8_dependencies[i].IsLoaded()) {
      loaded = false;
    }
  }
  x1c_loaded = loaded;
  return loaded;
}
