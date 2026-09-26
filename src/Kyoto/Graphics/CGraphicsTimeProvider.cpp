// CGraphics' external-time-provider pair: `SetExternalTimeProvider` and
// `GetSecondsMod900`.
//
// Retail's two functions are adjacent and tiny, and they are adjacent because they
// share the same pair of globals - which is the whole body:
//
//   802bf618 <SetExternalTimeProvider__9CGraphicsFP13CTimeProvider>:   size:0x8
//   802bf618:  90 6d 9c 5c   stw   r3,-25508(r13)   ; field 0x9C5C = -25508, so 0x804199DC
//   802bf61c:  4e 80 00 20   blr
//
//   802bf620 <GetSecondsMod900__9CGraphicsFv>:                        size:0x20
//   802bf620:  80 6d 9c 5c   lwz   r3,-25508(r13)   ; 0x804199DC
//   802bf624:  28 03 00 00   cmplwi r3,0
//   802bf628:  41 82 00 10   beq   802bf638
//   802bf62c:  80 63 00 00   lwz   r3,0(r3)        ; CTimeProvider::x0_currentTime
//   802bf630:  c0 23 00 00   lfs   f1,0(r3)        ;   the float it references
//   802bf634:  4e 80 00 20   blr
//   802bf638:  c0 2d 9c 58   lfs   f1,-25512(r13)  ; field 0x9C58 = -25512, so 0x804199D8
//   802bf63c:  4e 80 00 20   blr
//
// The `lwz`+`lfs` pair is the whole of `CTimeProvider::GetSecondsMod900()`:
// `include/Kyoto/CTimeProvider.hpp:20` declares `x0_currentTime` as a
// **`const float&`**, so the member *is* a pointer and one dependent load is
// exactly what the header's inline accessor costs. Retail is not doing anything
// clever here and neither is this.
//
// The two globals are named `lbl_804199DC` and `lbl_804199D8` in
// `config/G2ME01/symbols.txt`; retail leaves them unnamed, and
// `include/Kyoto/Graphics/CGraphics.hpp:428-429` calls them `mpExternalTimeProvider`
// and `mSecondsMod900`. **Those two spellings are not interchangeable and this unit
// uses the retail ones deliberately.** A `Matching` object is linked into the DOL,
// where the definition of `lbl_804199DC` is a retail object; a reference to
// `CGraphics::mpExternalTimeProvider` mangles to
// `_ZN9CGraphics21mpExternalTimeProviderE`, which nothing defines in the DOL and
// which would fail to link. `CGraphics` has no `.cpp` at all, so there is no
// TU that could define it. The retail `lbl_` names are therefore the only
// spelling that links, and the C++-named members must stay **undefined** -
// `src/MetroidPrime/PortGlobals.cpp` defines the `lbl_` objects for the port build
// and deliberately does not define the two members. Nothing else in the tree
// names them (measured: `grep -rn mSecondsMod900|mpExternalTimeProvider src/
// include/ platform/` finds only the header's own declarations), so there is no
// second object and no way for the two spellings to drift apart in practice.
//
// `lbl_804199D8` is `.sbss`, so it is **zero** in a retail binary and zero in the
// port: `GetSecondsMod900` with no provider returns 0.0f. That is a real answer,
// not a placeholder, and it is what retail returns before any `CTimeProvider` is
// constructed.
//
// Declared descending by retail offset: mwcceppc emits in reverse source order.

#include "Kyoto/CTimeProvider.hpp"
#include "Kyoto/Graphics/CGraphics.hpp"

extern "C" {
extern CTimeProvider* lbl_804199DC;
extern float lbl_804199D8;
}

float CGraphics::GetSecondsMod900() {
  if (lbl_804199DC != nullptr) {
    return lbl_804199DC->GetSecondsMod900();
  }
  return lbl_804199D8;
}

void CGraphics::SetExternalTimeProvider(CTimeProvider* provider) { lbl_804199DC = provider; }
