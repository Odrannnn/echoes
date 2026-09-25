#ifndef _CSCRIPTFORGOTTENOBJECT
#define _CSCRIPTFORGOTTENOBJECT

#include "MetroidPrime/CEntity.hpp"

class CScriptForgottenObject : public CEntity {
public:
  CScriptForgottenObject(TUniqueId uid, const CEntityInfo& info, const rstl::string& name);
  // Declared before TypesMatch on purpose: the first non-inline virtual in the class is its key
  // function, and that is the object the vtable is emitted into. Retail's object carries
  // __vt__22CScriptForgottenObject (0x28 in .data, the only thing in .data) while ours put it in
  // TypesMatch.o, because TypesMatch was declared first. CScriptSequenceTimer already does this.
  ~CScriptForgottenObject() override;
  CEntity* TypesMatch(int typeId) const override;
  void AcceptScriptMsg(CStateManager& mgr, const CScriptMsg&) override;

  virtual void Render1(CStateManager& mgr);
  virtual void Render2(CStateManager& mgr);

private:
  void RenderInternal(CStateManager& mgr, TUniqueId uid, bool b);

  TUniqueId x24_;
  TUniqueId x28_;
};

#endif // _CSCRIPTFORGOTTENOBJECT
