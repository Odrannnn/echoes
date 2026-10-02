#include "types.h"

class CEntity;
class CStateManager;
class CInputStream;
class CEntityInfo;

typedef CEntity* (*FScriptLoader)(CStateManager&, CInputStream&, const CEntityInfo&);

struct TUniqueId {
  ushort value;
};

// Only the slot used by this dispatch has been identified; the full base class is not declared yet.
class CMetareeVtable {
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

extern const TUniqueId kInvalidUniqueId;
extern float skDamageHitTime__10CPatterned;
extern float lbl_8041B758;
extern "C" CEntity* REL_LoadMetaree(CStateManager&, CInputStream&, const CEntityInfo&);
void SetLoader_Metaree(FScriptLoader* loader);

extern "C" {
FScriptLoader REL_loader_Metaree = nullptr;
}

// Keep definitions in descending retail text-address order; MWCC emits them in reverse source
// order.
static void SetRelLoaderFunctionToLoader() {
  REL_loader_Metaree = REL_LoadMetaree;
  SetLoader_Metaree(&REL_loader_Metaree);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the
// host these take distinct names that platform/compiled_modules.cpp calls. The MWCC branch
// is the retail source token for token, so the matching build cannot see this change.
#ifdef __MWERKS__
extern "C" void RELMain() { SetRelLoaderFunctionToLoader(); }
extern "C" void RELExit() { SetLoader_Metaree(nullptr); }
#else
extern "C" void mp_relmain_metaree() { SetRelLoaderFunctionToLoader(); }
extern "C" void mp_relexit_metaree() { SetLoader_Metaree(nullptr); }
#endif

extern "C" {
void fn_42_3C0(void* self) {
  reinterpret_cast<CMetareeVtable*>(self)->Slot12();
}

void fn_42_3A4(float* out, const void* self) {
  const float* src = reinterpret_cast<const float*>(static_cast<const char*>(self) + 0x54);
  out[0] = src[0];
  out[1] = src[1];
  out[2] = src[2];
}

int fn_42_39C(void*) { return 0; }
int fn_42_394(void*) { return 0; }
int fn_42_38C(void*) { return 1; }
void* fn_42_384(void* self) { return static_cast<char*>(self) + 0x754; }
float fn_42_378() { return lbl_8041B758; }

int fn_42_36C(const void* self) {
  return (*reinterpret_cast<const uchar*>(static_cast<const char*>(self) + 0x34c) >> 3) & 1;
}

void fn_42_35C(TUniqueId* self) { *self = kInvalidUniqueId; }

int fn_42_354(void*) { return 0; }
int fn_42_34C(void*) { return 0; }
int fn_42_344(void*) { return 0; }
int fn_42_33C(void*) { return 0; }

uchar fn_42_334(const void* self) {
  return *reinterpret_cast<const uchar*>(static_cast<const char*>(self) + 0x44f);
}

void fn_42_324(void* self) {
  *reinterpret_cast<float*>(static_cast<char*>(self) + 0x448) = skDamageHitTime__10CPatterned;
}
}
