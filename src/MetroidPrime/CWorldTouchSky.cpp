// Retail `TouchSky__6CWorldCFv` = `_ZNK6CWorld8TouchSkyEv`, .text 0x8004F770..0x8004F7C8,
// 0x58 = 88 bytes.  The body is the one already written in `src/MetroidPrime/CWorld.cpp`,
// which `configure.py` holds as a `NonMatching` unit (line 425) and `files.cmake` does not
// list, so nothing in the port compiled it - and the port reaches it only through
// `CStateManager::TouchSky`, which calls it by its retail `fn_` name
// (src/MetroidPrime/CStateManager.cpp:767), so the link was asking for the `fn_` symbol.
//
// **Why a carve-out and not `CWorld.cpp`.**  That file is a whole `NonMatching` unit: the
// area/layer tables, the layer REL load and unload bookkeeping, the environment-effects
// selection and the sky override resolution, all of which is a net *rise* in the port's
// undefined count.
//
// **Net -1 with no new callees**: both members are `rstl::optional_object< TLockedToken<
// CModel > >` and the body calls `CModel::Touch(int) const`, which
// `src/Kyoto/Graphics/CModelTouch.cpp` already defines for the port (retail
// `Touch__6CModelCFi`, 0x803112DC).
//
// The parameter is a shader index, not a flag: the skybox models are the ones whose
// textures are touched from here, and touching a shader is what keeps a two-frame-locked
// texture from being dropped while the skybox is still on screen.  Both slots are optional,
// so a skybox that is not loaded is simply skipped rather than asserted on.
#include "MetroidPrime/CWorld.hpp"

#include "Kyoto/Graphics/CModel.hpp"

void CWorld::TouchSky() const {
  if (mSkyboxWorldLoaded) {
    (*mSkyboxWorldLoaded)->Touch(0);
  }
  if (mSkyboxOverride) {
    (*mSkyboxOverride)->Touch(0);
  }
}
