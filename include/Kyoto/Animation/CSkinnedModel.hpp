#ifndef _CSKINNEDMODEL
#define _CSKINNEDMODEL

#include "types.h"

#include "Kyoto/TToken.hpp"

#include "rstl/auto_ptr.hpp"

class CModel;
class CSkinRules;
class CCharLayoutInfo;
// SetPointGeneratorFunc's second parameter is a function pointer, so mwcceppc has to parse the
// pointee type. A bare `CVector3f` with no declaration in scope is an undeclared identifier there,
// which GC/2.7 reports as "implicit 'int' is no longer supported in C++" and then dies with
// "')' expected" - the whole class becomes uncompilable. Forward-declared like every other
// header in the tree that only needs the type; no definition is needed, so no layout is touched.
class CVector3f;

class CSkinnedModel {
public:
  virtual ~CSkinnedModel();

  static void ClearPointGeneratorFunc();

  TLockedToken< CModel >& Model() { return x4_model; }
  const TLockedToken< CModel >& GetModel() const { return x4_model; }

  static void SetPointGeneratorFunc(void*, void (*)(void*, const CVector3f*, const CVector3f*, int));

private:
  TLockedToken< CModel > x4_model;
  TLockedToken< CSkinRules > x10_skinRules;
  TLockedToken< CCharLayoutInfo > x1c_layoutInfo;
  // `rstl::auto_ptr< float[] >` cannot be written in CodeWarrior either, and it is what stopped
  // mwcceppc from *using* this class: the moment anything needs `~CSkinnedModel` the implicit
  // `~auto_ptr` is instantiated, and `delete x4_item` on `float(*)[]` is
  //   Error: illegal type
  //   (instantiating: 'rstl::auto_ptr<float[]>::~auto_ptr()')
  // `float` gives the identical layout (bool at +0, `float*` at +4, size 8, align 4) - measured
  // with mwcceppc, `offsetof` on a struct holding `auto_ptr<float[]>` and on one holding
  // `auto_ptr<float>` both give 0 / 8 / 0x10 / 0x11 and sizeof 0x14 - so nothing downstream of
  // `x1c_layoutInfo` moves. MWCC's rstl::auto_ptr is a single-object smart pointer with no array
  // specialisation, so the workspaces are `new float[n]` blocks owned as one object and
  // `auto_ptr<float>` is also the only declaration retail can actually have had.
  rstl::auto_ptr< float > x24_vertWorkspace;
  rstl::auto_ptr< float > x2c_normalWorkspace;
  bool x34_owned;
  bool x35_disableWorkspaces;

  // OPEN: the four names above are 4 bytes below what mwcceppc lays out. Measured, they are
  // 0x28 / 0x30 / 0x38 / 0x39 with sizeof 0x3c, because `TLockedToken<T>` is 0xC bytes here
  // (`CToken` is a pointer plus a trailing `bool x4_lockHeld`), so `x1c_layoutInfo` ends at
  // 0x28, not 0x24. `x4_model` -> `x10_skinRules` -> `x1c_layoutInfo` is a uniform 0xC step, and
  // CAnimData's retail-measured `x0_charFactory` 0x000 / `xc_charInfo` 0x00C / `xcc_layoutData`
  // 0x0CC / `xd8_modelData` 0x0D8 says the same 0xC, so the trailing four names - not the token
  // size - are the ones that are wrong. `config/R3ME01/symbols.txt` has the constructor
  // `__ct__13CSkinnedModelFRC21TLockedToken<6CModel>RC26TLockedToken<10CSkinRules>RC31TLockedToken<15CCharLayoutInfo>`,
  // i.e. three TLockedTokens, which is consistent with the 0xC step and with a 0x3c class.
  // Nothing in G2ME01 reads these members (CSkinnedModel has exactly one retail function here,
  // `ClearPointGeneratorFunc`), so no code is wrong today; renaming them to 0x28/0x30/0x38/0x39
  // is a separate, layout-only change and is NOT made here.

  // Retail `ClearPointGeneratorFunc` (0x8030F048) is `li r0,0 / stw r0,spointGeneratorFunc(r13) / blr`;
  // the `r13` displacement resolves through `_SDA_BASE_` 0x8041FD80 (tools/sda.py) to 0x80419BB8,
  // a 4-byte uninitialised word in `.sbss` that `config/G2ME01/symbols.txt` calls `lbl_80419BB8`.
  // Two *unclaimed* functions (`fn_8030F054`, `fn_8030F3A4`) also store to it, so that symbol had
  // to keep existing in the link: it is renamed in `config/G2ME01/symbols.txt` to the name below
  // and CSkinnedModel.cpp claims the 4 `.sbss` bytes at 0x80419BB8, which is the same treatment
  // `gCurrentTimeProvider__13CTimeProvider` gets. A static data member adds no storage to the
  // instance, so no offset moves.
  static void* spointGeneratorFunc;
};

#endif // _CSKINNEDMODEL
