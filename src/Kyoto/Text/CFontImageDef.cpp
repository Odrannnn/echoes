// Ported from upstream PrimeDecomp/echoes @ d83da79: src/Kyoto/Text/CFontImageDef.cpp
#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/Text/CFontImageDef.hpp"

CFontImageDef::CFontImageDef(const TToken< CTexture >& texture, const CVector2f& cropFactor)
: x0_fps(0.f), x4_textures(1, texture), x8_cropFactor(cropFactor) {
  rstl::vector< TToken< CTexture > >::iterator it = x4_textures.begin();
  for (; it != x4_textures.end(); ++it) {
    it->Lock();
  }
}

CFontImageDef::CFontImageDef(const rstl::vector< TToken< CTexture > >& textures, float fps,
                             const CVector2f& cropFactor)
: x0_fps(fps), x4_textures(textures), x8_cropFactor(cropFactor) {
  rstl::vector< TToken< CTexture > >::iterator it = x4_textures.begin();
  for (; it != x4_textures.end(); ++it) {
    it->Lock();
  }
}

bool CFontImageDef::IsLoaded() const {
  for (int i = 0; i < x4_textures.size(); ++i) {
    if (!x4_textures[i].IsLoaded()) {
      return false;
    }
  }

  return true;
}

int CFontImageDef::CalculateBaseline() const {
  return (2.5f * GetHeight()) / 3.f;
}
