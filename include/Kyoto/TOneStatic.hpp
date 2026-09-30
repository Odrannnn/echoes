#ifndef _TONESTATIC
#define _TONESTATIC

#include "types.h"
#include "stdio.h"

// **`operator new` and `GetAllocSpace` are out of line because retail calls both of them, and
// written out of line our object reproduces all four of the bodies retail has in these two
// classes.** Retail: `CMain::RsMain` calls `0x80008AD4` (48 bytes) with `li r3,356` - this
// class's `sizeof` - and `0x80008AD4` ends in a call to `0x80008B30` (12 bytes, `lis`/`addi`/`blr`
// returning the address of the static storage), so the two-level shape is real, and so are both
// of the 12-byte bodies: `0x80008AA4` for `CGameArchitectureSupport`, `0x80008B30` for
// `CGameGlobalObjects`. Written inside the class body they are implicitly inline, and whether
// mwcceppc then folds them into their only caller or still emits them was **not** measured - what
// was measured is that with them out of line `main.o` emits `__nw__`(48) + `GetAllocSpace`(12) +
// `ReferenceCount`(36) for each class and all six are byte-identical to retail. This is the same
// arrangement `operator delete` and `ReferenceCount` already used, and the reason their sizes were
// already retail's before this run.
//
// The one-argument overload is retail-unused in this range: retail's two call sites
// (`0x80005CD0` and `0x80005E14`) pass three arguments, so nothing in the DOL selects it. It
// stays because `PortBoot.cpp`'s `new CGameArchitectureSupport` and `new CGameGlobalObjects` are
// ordinary `new` expressions, which can only reach the one-argument form.
template < typename T >
class TOneStatic {
public:
  void* operator new(size_t sz, const char*, const char*);
  void* operator new(size_t sz);
  void operator delete(void* ptr);

  // **Public only so that retail's `fn_80008B04` can be written at all.** That address
  // (0x80008B04, 0x2C) is `TOneStatic<CGameGlobalObjects>::operator delete` and
  // `config/G2ME01/symbols.txt` holds dtk's `fn_80008B04` placeholder for it, not a mangled name,
  // so objdiff pairs it by that placeholder and the body has to be emitted under it -
  // `src/MetroidPrime/main.cpp` has the definition. `operator delete` is defined out of line
  // (above), so calling it emits a `bl` to the weak `__dl__...Fv` copy instead of retail's
  // `bl ReferenceCount__32TOneStatic<18CGameGlobalObjects>Fv`; the body is one statement and
  // `ReferenceCount()` is the only name in it. Access control does not affect code generation,
  // and every other member keeps its linkage.
  static uint& ReferenceCount();

private:
  static void* GetAllocSpace();
};

template < typename T >
void* TOneStatic< T >::operator new(size_t sz, const char*, const char*) {
  ReferenceCount()++;
  return GetAllocSpace();
}

template < typename T >
void* TOneStatic< T >::operator new(size_t sz) { return operator new(sz, "??(??)", nullptr); }

template < typename T >
void* TOneStatic< T >::GetAllocSpace() {
  static uchar sAllocSpace[sizeof(T)];
  return &sAllocSpace;
}

template < typename T >
uint& TOneStatic< T >::ReferenceCount() {
    static uint sReferenceCount = 0;
    return sReferenceCount;
}

template < typename T >
void TOneStatic< T >::operator delete(void* ptr) {
  ReferenceCount()--;
}

#endif // _TONESTATIC
