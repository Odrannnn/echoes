/**
 * The port's `CModel` destructor. **Port-only**: `configure.py` does not declare it, so mwcceppc
 * never sees it - the same arrangement as `src/Kyoto/Graphics/CTexturePortStub.cpp`.
 *
 * Retail's `~CModel` is upstream's `src/Kyoto/Graphics/DolphinCModel.cpp`, which files.cmake does
 * not list yet (it pulls in `CCubeModel`, `CCubeMaterial` and `CFrameDelayedKiller`). The only
 * `CModel`s the port builds are the empty stand-ins from `port::pool::CreateStandInObject`, which
 * own no buffer, so an empty body releases everything they hold. **Delete this file when
 * `DolphinCModel.cpp` is listed**, or the link sees two definitions.
 */

#include "Kyoto/Graphics/CModel.hpp"

CModel::~CModel() {}

/**
 * Retail `FrameDone__6CModelFv` (0x803111A4, 0xAC bytes) is the per-frame end-of-texture-frame
 * step: bump `sFrameCounter`, and if the texture timeout is enabled walk the three shader lists
 * (`sThisFrame`, `sOneFrame`, `sTwoFrame`) unlocking the two-frame shaders' textures and rotating
 * the lists. The port reaches it through `CStateManager::fn_8003C3A8`-neighbourhood code that
 * used the `fn_803111A4` placeholder name; with no `CModel` body in the host build there was
 * nothing to bind that to.
 *
 * **It is empty here for the same reason `~CModel` above is, and the reason is the same
 * structural fact rather than a convenience.** Every word it touches is static state of
 * `src/Kyoto/Graphics/DolphinCModel.cpp` - `sFrameCounter`, `sIsTextureTimeoutEnabled`,
 * `sThisFrame`, `sOneFrame`, `sTwoFrame` and the `SShader` chain - and that file is
 * `configure.py` `NonMatching` and unlisted in `files.cmake` (it pulls in `CCubeModel`,
 * `CCubeMaterial` and `CFrameDelayedKiller`). None of those lists has a writer on the host: the
 * only code that enqueues a shader is `CCubeMaterial`'s two-frame-lock path, which is in the same
 * unlisted file. The `CModel`s the port does build are the empty stand-ins from
 * `port::pool::CreateStandInObject`, so there is no texture to unlock and nothing to rotate.
 *
 * **This is the port-accurate answer, not a stub standing in for a written one**, and it is
 * visible in the same way `~CModel`'s is: nothing observable changes. It is *not* a claim that
 * the function is empty in retail - it is 0xAC bytes there and it is not here.
 *
 * **Delete this when `DolphinCModel.cpp` is listed**, or the link sees two definitions.
 */
void CModel::FrameDone() {}

/**
 * The port's three draw entry points, reached for the first time from
 * `src/MetroidPrime/CModelData.cpp` (`CModelData::RenderUnsortedParts` calls `DrawUnsortedParts`,
 * `CModelData::Render` calls `DrawSortedParts` and `Draw`). **Port-only**, exactly like
 * `~CModel` and `FrameDone` above: `configure.py` does not declare this file, so mwcceppc never
 * sees it and the matching build is untouched.
 *
 * Retail's bodies are upstream's `src/Kyoto/Graphics/DolphinCModel.cpp` (`Draw__6CModelCFRC11CModelFlags`
 * at 0x803118C8, `DrawSortedParts__6CModelCFRC11CModelFlags` at 0x80311700, and
 * `DrawUnsortedParts__6CModelCFRC11CModelFlags` at 0x8031178C - the last two are the call targets
 * in `CModelData::RenderUnsortedParts` at 0x800E658C and in `CModelData::Render` at 0x800E67BC /
 * 0x800E67C8). That file is `configure.py` `NonMatching` and unlisted in `files.cmake`, so the host
 * has no `CModel` body to call. The only `CModel`s the port builds are the empty stand-ins from
 * `port::pool::CreateStandInObject`, which own no surface list and no buffer, so there is nothing
 * for a draw to submit.
 *
 * **This is the port-accurate answer, not a stub standing in for a written one**: nothing
 * observable changes. It is *not* a claim that these are empty in retail - they are 0x48, 0x8C and
 * 0xAC bytes there.
 *
 * **Delete these when `DolphinCModel.cpp` is listed**, or the link sees two definitions.
 */
void CModel::Draw(const CModelFlags&) const {}
void CModel::DrawSortedParts(const CModelFlags&) const {}
void CModel::DrawUnsortedParts(const CModelFlags&) const {}

