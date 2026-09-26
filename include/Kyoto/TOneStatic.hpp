#ifndef _TONESTATIC
#define _TONESTATIC

#include "types.h"
#include "stdio.h"

template < typename T >
class TOneStatic {
public:
  void* operator new(size_t sz, const char*, const char*); /* {
   ReferenceCount()++;
   return GetAllocSpace();
 }*/
  void* operator new(size_t sz) { return operator new(sz, "??(??)", nullptr); }
  void operator delete(void* ptr);

private:
  static void* GetAllocSpace() {
    static uchar sAllocSpace[sizeof(T)];
    return &sAllocSpace;
  }
  static uint& ReferenceCount();
};

template < typename T >
uint& TOneStatic< T >::ReferenceCount() {
    static uint sReferenceCount = 0;
    return sReferenceCount;
}

template < typename T >
void TOneStatic< T >::operator delete(void* ptr) {
  ReferenceCount()--;
}

// The other half of the pair, which was declared at line 10 and never defined. `operator delete`
// above has always been here, so the type was half-formed: a `new` on any `TOneStatic` subclass
// resolved the 1-arg form (line 14) to the 3-arg form, and the 3-arg form had no definition
// anywhere in the tree - retail's lives in its CRT. `CGameArchitectureSupport` is the one class
// here that derives from it, so `new CGameArchitectureSupport(*osContext)` failed to link with
// `undefined reference to TOneStatic<CGameArchitectureSupport>::operator new(unsigned long,
// char const*, char const*)`. Its sibling `CGameGlobalObjects` is a plain class and never
// noticed.
//
// This is retail's definition, recovered from the hint the class itself carries in the
// `operator new` declaration - `ReferenceCount()++; return GetAllocSpace();` - not an invention.
// `GetAllocSpace()` returns a per-class `static uchar[sizeof(T)]`, which is what the name means:
// a `TOneStatic` is one static instance, and the count tracks it. Defining it is a no-op for the
// DOL because the template is only instantiated where a unit actually writes a `new`, and
// verified by the hash check.
template < typename T >
void* TOneStatic< T >::operator new(size_t sz, const char*, const char*) {
  ReferenceCount()++;
  return GetAllocSpace();
}

#endif // _TONESTATIC
