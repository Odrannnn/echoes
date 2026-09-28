// `CScriptActor::CheckActorRenderOnly() const`.  The body is the one already written in
// `src/MetroidPrime/ScriptObjects/CScriptActor.cpp` (line 284), which `configure.py` holds as
// a `NonMatching` unit (line 466) and `files.cmake` does not list.
//
// **Why a carve-out and not `CScriptActor.cpp`.**  That file is a whole `NonMatching` unit - the
// script object's load and property cases, the actor overrides, the spline and layer plumbing
// and the dtor - and listing it would add all of it, a net *rise* in the port's undefined
// count.
//
// **Net -1 with no new callees**: `GetMaterialList()` and `CMaterialList::HasMaterial` are
// both inline, and the three material ids are enum values.
//
// The predicate is the render-only test the level designer sets up with materials rather than
// with a flag: an actor the camera may pass through but must not collide with, and which must
// not be treated as an occluder.  All three clauses are needed - `kMT_Immovable` and
// `kMT_CameraPassthrough` say what it is, and the explicit `kMT_Unknown59` exclusion says the
// default material a world-format area carries is *not* one of them, so an actor that only
// looks render-only is not render-only.
#include "MetroidPrime/ScriptObjects/CScriptActor.hpp"

bool CScriptActor::CheckActorRenderOnly() const {
  return GetMaterialList().HasMaterial(kMT_Immovable) &&
         GetMaterialList().HasMaterial(kMT_CameraPassthrough) &&
         !GetMaterialList().HasMaterial(kMT_Unknown59);
}
