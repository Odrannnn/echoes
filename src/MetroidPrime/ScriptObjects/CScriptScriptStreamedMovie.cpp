// Functions for the ScriptStreamedMovie REL.

#include "MetroidPrime/CModelData.hpp"

extern "C" void fn_800E6AD0(CModelData* modelData);

// The REL keeps a local CModelData default-constructor wrapper. Its shared
// initialization routine lives in the main DOL.
extern "C" void __ct__10CModelDataFv(CModelData* modelData) {
  fn_800E6AD0(modelData);
}
