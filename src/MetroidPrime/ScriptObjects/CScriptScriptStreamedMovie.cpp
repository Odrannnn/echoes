// Functions for the ScriptStreamedMovie REL.

#include "MetroidPrime/CModelData.hpp"

// The REL keeps a local 0x20-byte forwarder to CModelData's default constructor in the
// main DOL (0x800E6AD0, which upstream's symbols.txt names `__ct__10CModelDataFv`). The
// forwarder takes the REL-local name so the call binds to the DOL copy. The host keeps
// the port's names: `fn_800E6AD0` is CModelDataDefaultCtor.cpp.
#ifdef __MWERKS__
extern "C" void __ct__10CModelDataFv(CModelData* modelData);

extern "C" void fn_67_1054(CModelData* modelData) { __ct__10CModelDataFv(modelData); }
#else
extern "C" void fn_800E6AD0(CModelData* modelData);

extern "C" void __ct__10CModelDataFv(CModelData* modelData) {
  fn_800E6AD0(modelData);
}
#endif
