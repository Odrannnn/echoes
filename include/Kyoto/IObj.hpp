#ifndef _IOBJ
#define _IOBJ

#include "types.h"

#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/SObjectTag.hpp"
#include "rstl/auto_ptr.hpp"

extern const SObjectTag gkInvalidObjectTag;

class IObj {
public:
  virtual ~IObj() {}
};

class CObjOwnerDerivedFromIObjUntyped : public IObj {
public:
  ~CObjOwnerDerivedFromIObjUntyped() {}
  template < typename T >
  CObjOwnerDerivedFromIObjUntyped(T* obj) : m_objPtr(obj) {}
  template < typename T >
  CObjOwnerDerivedFromIObjUntyped(const rstl::auto_ptr< T >& obj) : m_objPtr(obj.release()) {}

  void* GetContents() { return m_objPtr; }

protected:
  void* m_objPtr;
};

template < typename T >
class TObjOwnerDerivedFromIObj : public CObjOwnerDerivedFromIObjUntyped {
public:
  ~TObjOwnerDerivedFromIObj() {
    if (Owned()) {
      delete Owned();
    }
  }
  T* Owned() { return static_cast< T* >(m_objPtr); }

  static rstl::auto_ptr< TObjOwnerDerivedFromIObj< T > > GetNewDerivedObject(T* obj) {
    return rs_new TObjOwnerDerivedFromIObj< T >(obj);
  }
  static rstl::auto_ptr< TObjOwnerDerivedFromIObj< T > >
  GetNewDerivedObject(const rstl::auto_ptr< T >& obj) {
    return rs_new TObjOwnerDerivedFromIObj< T >(obj);
  }

  // Public like `rstl::auto_ptr`'s members, for the reason given there: retail's
  // `GetNewDerivedObject` can only be re-spelled as an `extern "C"` function - no C++
  // declaration can rename a template instantiation, so `fn_8028EC7C` in
  // `src/Kyoto/Animation/CAnimCharacterSet.cpp` has to construct one by hand rather than call
  // this - and a free function cannot reach a private constructor. Only the access specifier is
  // edited, and access specifiers emit nothing: `main.dol` and all 86 RELs still hash as they did.
  TObjOwnerDerivedFromIObj(T* obj) : CObjOwnerDerivedFromIObjUntyped(obj) {}
  TObjOwnerDerivedFromIObj(const rstl::auto_ptr< T >& obj) : CObjOwnerDerivedFromIObjUntyped(obj) {}
};

#endif // _IOBJ
