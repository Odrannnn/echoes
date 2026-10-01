#include "Kyoto/CRelFileDebugInfo.hpp"

#include "Kyoto/Alloc/CMemory.hpp"

CRelFileDebugInfo* CRelFileDebugInfo::mspHead = nullptr;

CRelFileDebugInfo::CRelFileDebugInfo()
: mName(nullptr), mStart(0), mSize(0), mNext(nullptr), mPrev(nullptr) {}

CRelFileDebugInfo::~CRelFileDebugInfo() { Unregister(); }

void CRelFileDebugInfo::Register(const char* name, const void* start, uint size) {
  if (mNext != nullptr) {
    return;
  }
  mName = name;
#ifdef TARGET_PC
  // A host pointer is 64 bits and `mStart` is retail's 32-bit field. Only the exception handler's
  // address-to-module report reads it, which the port does not run, so the low word is kept.
  mStart = static_cast< uint >(reinterpret_cast< uintptr_t >(start));
#else
  mStart = reinterpret_cast< uint >(start);
#endif
  mSize = size;
  mNext = mspHead;
  if (mNext != nullptr) {
    mNext->mPrev = this;
  }
  mspHead = this;
}

void CRelFileDebugInfo::Unregister() {
  if (mspHead == this) {
    mspHead = mNext;
  }
  if (mNext != nullptr) {
    mNext->mPrev = mPrev;
  }
  if (mPrev != nullptr) {
    mPrev->mNext = mNext;
  }
  mNext = nullptr;
  mPrev = nullptr;
}

bool CRelFileDebugInfo::Contains(int address) const { return address - mStart < mSize; }

CRelFileDebugInfo* CRelFileDebugInfo::FindByAddress(int address) {
  for (CRelFileDebugInfo* info = mspHead; info != nullptr; info = info->mNext) {
    if (info->Contains(address)) {
      return info;
    }
  }
  return nullptr;
}
