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
  short x0_refCount : 16;
  short x2_lockCount : 15;
  short x3_loading : 1;
  int x4_unk;
  int x8_unk;
  SObjectTag xc_objTag;
  IObjectStore* x14_objectStore;
  IObj* x18_object;
  CVParamTransfer x1c_params;
};
#endif // _COBJECTREFERENCE
