#ifndef _CSIMPLEPOOL
#define _CSIMPLEPOOL

#include "types.h"

#include "rstl/hash_map.hpp"
#include "rstl/rc_ptr.hpp"

#include "Kyoto/CToken.hpp"
#include "Kyoto/IObjectStore.hpp"

class IFactory;

// Retail's `CSimplePool` constructor is 0x80301008, `size:0x150`, and it is called exactly once
// in the DOL: `addi r3,r31,228 ; addi r4,r31,4 ; bl 80301008` at 0x800084AC-0x800084B4, i.e. on
// `CGameGlobalObjects`+0xE4 with `&resFactory` at +0x04. Two `stw r0,0(r31)` at the top of
// 0x80301008 are the two vptrs, so **this constructor has to stay declared-only for the same
// reason `CResFactory`'s does** - an inline body puts `__vt__12IObjectStore` and the whole
// `rstl::hash_map` default construction in the caller, 15 instructions where retail has two.
//
// `fn_80301008` is **renamed in `config/G2ME01/symbols.txt`** to that constructor's mangled
// name, so the call mwcceppc emits resolves against the DOL.
class CSimplePool : public IObjectStore {
public:
  CSimplePool(IFactory& factory);
  ~CSimplePool();

  virtual CToken GetObj(const SObjectTag& tag, CVParamTransfer xfer);
  virtual CToken GetObj(const SObjectTag& tag);
  virtual CToken GetObj(const char* name);
  virtual CToken GetObj(const char* name, CVParamTransfer xfer);
  virtual bool HasObject(const SObjectTag& tag);
  virtual bool ObjectIsLive(const SObjectTag& tag);
  virtual IFactory& GetFactory() { return x18_factory; }
  virtual void Flush();
  virtual void ObjectUnreferenced(const SObjectTag& tag);

  void fn_8029c7e8(const SObjectTag& tag);

private:
  uchar x4_;
  uchar x5_;
  rstl::hash_map< unkptr, unkptr, void, void > x8_resources;
  IFactory& x18_factory;
  rstl::rc_ptr< CVParamTransfer > x1c_paramXfr;
};
// 0x24, not 0x20: `x1c_paramXfr` is retail's 8-byte `rstl::rc_ptr`. Measured with mwcceppc's own
// flags into `.data` and read with `objdump -s` - the host compiler's 64-bit `rstl` gives a
// different answer. See docs/research/rc_ptr.md.
CHECK_SIZEOF(CSimplePool, 0x24)

extern CSimplePool* gpSimplePool;

#endif // _CSIMPLEPOOL
