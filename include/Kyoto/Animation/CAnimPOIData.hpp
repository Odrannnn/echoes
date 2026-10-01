#ifndef _CANIMPOIDATA
#define _CANIMPOIDATA

#include "Kyoto/Animation/CBoolPOINode.hpp"
#include "Kyoto/Animation/CInt32POINode.hpp"
#include "Kyoto/Animation/CParticlePOINode.hpp"
#include "Kyoto/Animation/CSoundPOINode.hpp"

#include "rstl/vector.hpp"

class CAnimPOIData;

// See the note on the two `friend` declarations at the bottom of the class.
extern "C" void* fn_8028D440(void* self, int flag);
extern "C" CAnimPOIData* fn_8028CCF0(CAnimPOIData* self, const CAnimPOIData* other);

class CAnimPOIData {
public:
  explicit CAnimPOIData(CInputStream& in);

  const rstl::vector< CBoolPOINode >& GetBoolPOIStream() const { return mBoolNodes; }
  const rstl::vector< CInt32POINode >& GetInt32POIStream() const { return mInt32Nodes; }
  const rstl::vector< CParticlePOINode >& GetParticlePOIStream() const { return mParticleNodes; }
  const rstl::vector< CSoundPOINode >& GetSoundPOIStream() const { return mSoundNodes; }

private:
  uint mVersion;
  rstl::vector< CBoolPOINode > mBoolNodes;
  rstl::vector< CInt32POINode > mInt32Nodes;
  rstl::vector< CParticlePOINode > mParticleNodes;
  rstl::vector< CSoundPOINode > mSoundNodes;

  // `CAnimPOIData`'s copy constructor and its *deleting* destructor are retail's unnamed
  // functions `fn_8028CCF0` and `fn_8028D440` in `src/Kyoto/Animation/CAnimationSet.cpp`, and both
  // are written out there by hand because mwceppc emits retail's copies under mangled names and
  // objdiff pairs functions by name (see the note at the top of that file). A mangler fixes a
  // constructor's symbol name, so neither can be given the name `fn_` as a member; they are free
  // functions over the same statements instead, and free functions cannot reach private members.
  // The same `friend` shape `include/Kyoto/Graphics/CGX.hpp` uses for `fn_802BCC74`.
  friend void* fn_8028D440(void* self, int flag);
  friend CAnimPOIData* fn_8028CCF0(CAnimPOIData* self, const CAnimPOIData* other);
};
CHECK_SIZEOF(CAnimPOIData, 0x44)

#endif // _CANIMPOIDATA
