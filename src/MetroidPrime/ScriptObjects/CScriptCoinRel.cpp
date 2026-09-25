// CScriptCoinRel.cpp - module ScriptCoin (REL id 58)'s loader registration and REL entry points.
//
// This is the same shape as CScriptPufferRel.cpp, which reproduces the Puffer module byte for byte:
// a 4-byte .bss slot holding the module's own FScriptLoader, a Register function that fills it and
// hands it to the DOL's SetLoader, then RELMain/RELExit as bare callers. Retail's `_prolog` calls
// fn_58_50 and `_epilog` calls fn_58_2C, which is how dtk knows which of the two is which.
//
// SetLoader_Coin is unnamed in retail, so the DOL symbol table carries the address-derived name.

#include "MetroidPrime/ScriptLoader.hpp"

extern "C" void fn_8021FA80(FScriptLoader* loader);
extern "C" CEntity* fn_58_A0(CStateManager& mgr, CInputStream& input, const CEntityInfo& info);

// fn_58_0 is CScriptCoin's own virtual at vtable slot 13; its body is a dispatch to slot 14
// (offset 0x38), which the retail vtable lbl_58_data_40 fills with
// GetDamageVulnerability__6CActorCFv. The base class is not declared here, so model the dispatch
// with a slot-only vtable, as the Metaree unit does. MWCC puts two words ahead of the first slot,
// so a 13-slot vtable puts Slot12 at 0x38.
class CCoinVtable {
public:
  virtual void Slot0() = 0;
  virtual void Slot1() = 0;
  virtual void Slot2() = 0;
  virtual void Slot3() = 0;
  virtual void Slot4() = 0;
  virtual void Slot5() = 0;
  virtual void Slot6() = 0;
  virtual void Slot7() = 0;
  virtual void Slot8() = 0;
  virtual void Slot9() = 0;
  virtual void Slot10() = 0;
  virtual void Slot11() = 0;
  virtual void Slot12() = 0;
};

extern "C" {
FScriptLoader lbl_58_bss_0;

// MWCC emits definitions in reverse source order, so this file lists its functions in descending
// retail text-address order. objdiff pairs by name and so scores a mis-ordered object 100% while
// the module's bytes are laid out wrong; the module hash is what catches it.
void RegisterCoinLoader() {
  lbl_58_bss_0 = fn_58_A0;
  fn_8021FA80(&lbl_58_bss_0);
}

void RELMain() { RegisterCoinLoader(); }

void RELExit() { fn_8021FA80(0); }

void fn_58_0(void* self) {
  reinterpret_cast< CCoinVtable* >(self)->Slot12();
}
}
