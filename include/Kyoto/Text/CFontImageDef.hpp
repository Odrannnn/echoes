#ifndef _CFONTIMAGEDEF
#define _CFONTIMAGEDEF

#include <Kyoto/Graphics/CTexture.hpp>
#include <Kyoto/Math/CVector2f.hpp>
#include <Kyoto/TToken.hpp>
#include <rstl/vector.hpp>

class CTexture;
class CVector2f;
class CFontImageDef {
public:
  CFontImageDef(const TToken< CTexture >& texture, const CVector2f& cropFactor);
  CFontImageDef(const rstl::vector< TToken< CTexture > >& texture, float fps,
                const CVector2f& cropFactor);

  bool IsLoaded() const;
  const rstl::vector< TToken< CTexture > >& GetImages() const { return x4_textures; }
  float GetFps() const { return x0_fps; }
  const CVector2f& GetScale() const { return x8_cropFactor; }
  int GetMonoWidth() const {
    TToken< CTexture > tex = x4_textures[0];
    return tex->GetWidth() * x8_cropFactor.GetX();
  }
  int GetMonoHeight() const {
    TToken< CTexture > tex = x4_textures[0];
    return tex->GetHeight() * x8_cropFactor.GetY();
  }
  // Out of line in Echoes (0x802B8920 / 0x802B889C, defined in another TU).
  int GetWidth() const;
  int GetHeight() const;
  int CalculateBaseline() const;
  int CalculateHeight() const;

private:
  float x0_fps;
  rstl::vector< TToken< CTexture > > x4_textures;
  CVector2f x8_cropFactor;
};

CHECK_SIZEOF(CFontImageDef, 0x1c)

#endif // _CFONTIMAGEDEF
