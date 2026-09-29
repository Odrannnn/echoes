// `CGraphics::GetPerspectiveProjectionMatrix` - the static retail reaches at 0x802C229C.
//
// `CPlayerGun::ConvertToScreenSpace` (src/MetroidPrime/Player/CPlayerGun.cpp) calls it, and
// `files.cmake` does not list `src/Kyoto/Graphics/DolphinCGraphics.cpp`, so the port's link
// had no definition and the symbol went undefined when that body was decompiled.
//
// **Why a carve-out and not `DolphinCGraphics.cpp`.** That file is retail's Echoes `CGraphics`
// whole; `PORT_NOTES.md` records why it cannot be listed (four compile errors that are not
// local to it, and it pulls in `CCubeModel`, `CCubeMaterial` and `CFrameDelayedKiller`). The
// same one-function-per-file arrangement the other single-body port files use applies here,
// and the body is retail's verbatim from `DolphinCGraphics.cpp:664`.
//
// `mProj` is guest storage another port file already owns (`CGraphicsHostGlobals.cpp` defines it
// as `lbl_80416F28`), so it is reached by that `extern "C"` name rather than defined twice -
// the rule `CGraphicsHostStartup.cpp`'s header tabulates.
#include "Kyoto/Graphics/CGraphics.hpp"

extern "C" {
extern CGraphics::CProjectionState lbl_80416F28; // mProj
}

CMatrix4f CGraphics::GetPerspectiveProjectionMatrix() {
  const CGraphics::CProjectionState& mProj = lbl_80416F28;
  return CMatrix4f(
      // clang-format off
    (mProj.GetNear() * 2.f) / (mProj.GetRight() - mProj.GetLeft()),
    -(mProj.GetRight() + mProj.GetLeft()) / (mProj.GetRight() - mProj.GetLeft()),
    0.f,
    0.f,
    0.f,
    -(mProj.GetTop() + mProj.GetBottom()) / (mProj.GetTop() - mProj.GetBottom()),
    (mProj.GetNear() * 2.f) / (mProj.GetTop() - mProj.GetBottom()),
    0.f,
    0.f,
    (mProj.GetFar() + mProj.GetNear()) / (mProj.GetFar() - mProj.GetNear()),
    0.f,
    -(mProj.GetFar() * 2.f * mProj.GetNear()) / (mProj.GetFar() - mProj.GetNear()),
    0.f,
    1.f,
    0.f,
    0.f
      // clang-format on
  );
}
