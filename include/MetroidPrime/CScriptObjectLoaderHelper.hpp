#ifndef _CSCRIPTOBJECTLOADERHELPER
#define _CSCRIPTOBJECTLOADERHELPER

#include "Kyoto/Streams/CInputStream.hpp"
#include "MetroidPrime/TGameTypes.hpp"
#include "rstl/auto_ptr.hpp"
#include "rstl/vector.hpp"

class CEntity;
class CStateManager;

// Partial interface; class name corroborated by Echoes Wii exports.
class CScriptObjectLoaderHelper {
public:
  // Guessed name: result returned when a Generate connection creates an object.
  struct SGeneratedObject {
    TEditorId mEditorId;
    TUniqueId mUniqueId;
    CEntity* mEntity;
  };

  // Guessed name. Shared context for incremental loading of an area's layers.
  struct SLoadContext {
    explicit SLoadContext(TAreaId area);

    TAreaId mAreaId;
    rstl::auto_ptr< CInputStream > mStream;
    int mRemainingObjects;
    int x10_;
    int mLayerIndex;
    rstl::vector< CEntity* > mObjects;
    rstl::vector< TEditorId >* mEditorIds;
  };

  // Guessed method names.
  void LoadScriptObjects(TAreaId aid, CInputStream& in, rstl::vector< TEditorId >& ids,
                         CStateManager& mgr);
  void InitScriptObjects(rstl::vector< TEditorId >& ids, CStateManager& mgr);
  void RemoveLayerObjects(TAreaId area, TLayerId layer, CStateManager& mgr);
  void BeginLayerLoad(SLoadContext& context, rstl::auto_ptr< CInputStream > in,
                      rstl::vector< TEditorId >& ids);
  bool ContinueLayerLoad(SLoadContext& context, uint timeBudget, CStateManager& mgr);
  void LoadGeneratedScriptObjects(TAreaId area, CInputStream& in);
  void RegisterScriptObjects(rstl::vector< CEntity* > objects, CStateManager& mgr);
  SGeneratedObject GenerateObject(const TEditorId& editorId, CStateManager& mgr);

  bool GetUnk14_24() const { return x14_24; }

private:
  // Held inline by CStateManagerContainer at 0x13EC0, whose next member is at 0x13ED8; only the
  // members the decompiled code touches are laid out.
  char x0_pad[0x14];
  bool x14_24 : 1;
};
NESTED_CHECK_SIZEOF(CScriptObjectLoaderHelper, SGeneratedObject, 0xc)
NESTED_CHECK_SIZEOF(CScriptObjectLoaderHelper, SLoadContext, 0x2c)

#endif // _CSCRIPTOBJECTLOADERHELPER
