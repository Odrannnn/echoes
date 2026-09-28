#ifndef _CSCRIPTOBJECTLOADERHELPER
#define _CSCRIPTOBJECTLOADERHELPER

#include "types.h"

#include "MetroidPrime/TGameTypes.hpp"

#include "rstl/vector.hpp"

class CInputStream;
class CStateManager;

// Partial interface; class name corroborated by Echoes Wii exports. Held inline by
// CStateManagerContainer at 0x13EC0; only the members the decompiled code touches are laid out.
class CScriptObjectLoaderHelper {
public:
  void LoadScriptObjects(TAreaId aid, CInputStream& in, rstl::vector< TEditorId >& ids,
                         CStateManager& mgr);                                 // Guessed name
  void InitScriptObjects(rstl::vector< TEditorId >& ids, CStateManager& mgr); // Guessed name

  bool GetUnk14_24() const { return x14_24; }

private:
  char x0_pad[0x14];
  bool x14_24 : 1;
};

#endif // _CSCRIPTOBJECTLOADERHELPER
