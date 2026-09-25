#ifndef _COBJECTREFERENCE
#define _COBJECTREFERENCE

#include <Kyoto/CVParamTransfer.hpp>
#include <Kyoto/SObjectTag.hpp>
#include <rstl/auto_ptr.hpp>

class IObj;
class IObjectStore;
class CObjectReference {
public:
  CObjectReference(const rstl::auto_ptr< IObj >& obj);
  ~CObjectReference();
  CObjectReference(IObjectStore& store, const rstl::auto_ptr< IObj >& obj, const SObjectTag& tag,
                   CVParamTransfer xfer);

  bool IsLoaded() const { return x18_object != nullptr; }

  void AddReference() { x0_refCount++; }
  int RemoveReference();
  void Lock();
  void Unlock();
  IObj* GetObject();
  void Unload();
  void CancelLoad();
  bool IsLoading() const;
  const SObjectTag& GetTag() const { return xc_objTag; }

private:
  int x0_refCount;
  int x4_lockCount;
  int x8_loading;
  SObjectTag xc_objTag;
  IObjectStore* x14_objectStore;
  IObj* x18_object;
  CVParamTransfer x1c_params;
  // There is **no** member at 0x20, and this class is 0x24 bytes because `CVParamTransfer` is one
  // `rstl::rc_ptr<IVParamObj>` at 8 bytes (0x1c-0x23) - not because of a word missing from here.
  // Measured: `__dt__CObjectReference` (0x803006C8) reads 0x18 (`x18_object`) and, when it is
  // null, 0x08 (`x8_loading`), 0x14 (`x14_objectStore`) and 0x0c (`xc_objTag`, passed as
  // `addi r4,r30,12`), then does the dead `addic. r0,r30,28` allocator test on 0x1c and calls
  // `fn_80031D98(this + 0x1c)` - `~CVParamTransfer`, i.e. `~rc_ptr`. Nothing above 0x23.
  // The header used to end with a `void* x20_refData` standing in for retail's 8-byte rc_ptr and
  // blamed this fork's 4-byte one for the width; see docs/research/rc_ptr.md.
};
CHECK_SIZEOF(CObjectReference, 0x24)
#endif // _COBJECTREFERENCE
