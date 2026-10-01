#include "MetroidPrime/CWorldSaveGameInfo.hpp"

#include "Kyoto/CFactoryMgr.hpp"
#include "Kyoto/Streams/CInputStream.hpp"

// Port-only: `files.cmake` lists this file, `configure.py` does not. Retail's constructor is
// `fn_80182EC8` and its factory `fn_80182830`, both inside the unsplit `auto_03_80182830_text`
// (together with the vector helpers `fn_8018340C`/`fn_80183218`/`fn_801834C8`), so there is no
// unit to carve this body into yet.
//
// The SAVW format, read off `fn_80182EC8` (every field big-endian, `u32` unless noted):
//   magic (skipped), version, areaCount,
//   cinematics  : u32 count, then count x TEditorId     (+0x04)
//   relays      : u32 count, then count x TEditorId     (+0x14)
//   layers      : u32 count, then count x (area, layer) (+0x24)
//   doors       : u32 count, then count x TEditorId     (+0x34)
//   scans       : u32 count, then count x (CAssetId, u32) (+0x54)
//   if version > 3: system variables, game variables, each u32 count then count x
//                   (string, min, max, default)         (+0x64, +0x74)
//   if version > 4: unmappable objects, as the doors    (+0x44)
// Prime 1's constructor gates the same lists on lower versions; Echoes reads the first five
// unconditionally.

#ifndef __MWERKS__
TEditorId::TEditorId(CInputStream& in) : value(in.ReadInt32()) {}
#endif

namespace {
rstl::vector< CWorldSaveGameInfo::SLayerState > ReadLayers(CInputStream& in) {
  rstl::vector< CWorldSaveGameInfo::SLayerState > layers;
  const int count = in.ReadInt32();
  layers.reserve(count);
  for (int i = 0; i < count; ++i) {
    CWorldSaveGameInfo::SLayerState state;
    state.mArea = TAreaId(in.ReadInt32());
    state.mLayer = in.ReadInt32();
    layers.push_back_unsafe(state);
  }
  return layers;
}
} // namespace

CWorldSaveGameInfo::CWorldSaveGameInfo(CInputStream& in) : mAreaCount(0) {
  in.ReadInt32();
  const uint version = in.ReadInt32();
  mAreaCount = in.ReadInt32();
  mCinematics = rstl::vector< TEditorId >(in);
  mRelays = rstl::vector< TEditorId >(in);
  mLayers = ReadLayers(in);
  mDoors = rstl::vector< TEditorId >(in);
  mScans = rstl::vector< ScanState >(in);
  if (version > 3) {
    mSystemVariables = rstl::vector< SEnvironmentVariable >(in);
    mGameVariables = rstl::vector< SEnvironmentVariable >(in);
  }
  if (version > 4) {
    mUnmappableObjects = rstl::vector< TEditorId >(in);
  }
}

#ifndef __MWERKS__
// Retail's `fn_80182830`: `operator new(0x84)`, construct, wrap in a `CFactoryFnReturn`.
extern "C" CFactoryFnReturn fn_80182830(const SObjectTag&, CInputStream& in,
                                        const CVParamTransfer&) {
  return rs_new CWorldSaveGameInfo(in);
}
#endif
