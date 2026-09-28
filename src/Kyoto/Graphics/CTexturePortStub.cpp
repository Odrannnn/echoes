/**
 * The port's `CTexture` constructor and destructor. **This file is port-only**:
 * `configure.py` does not declare it, so mwcceppc never sees it and it is not a decompilation
 * unit - the same arrangement as `src/Kyoto/CSimplePoolPort.cpp` and
 * `src/Kyoto/CResFactoryPortVirtuals.cpp`.
 *
 * ## Why it exists
 *
 * `src/MetaRender/Carve80271238.cpp` is `CCubeRenderer`'s constructor, and it builds six
 * `CTexture`s. `CTexture.cpp` is in no manifest, so listing that carve asks the host link for
 * `CTexture::CTexture(ETexelFormat, short, short, int)` and `CTexture::~CTexture()`. Those are
 * retail's `__ct__8CTextureF12ETexelFormatssi` at 0x802C6000 and `__dt__8CTextureF12Ev` beside
 * it, and both are still unclaimed.
 *
 * **It also blocks `tools/boot_probe.sh` outright, which is why this file is not optional
 * polish.** The probe's self-heal declares a missing symbol as `extern "C" void <name>(void)`
 * and skips anything that is not a valid C identifier, printing what it skipped. A demangled
 * C++ signature is not one, so the probe reports
 *
 * ```
 * boot_probe: not declarable as C identifiers (left for a human):
 *   CTexture::CTexture(ETexelFormat,
 *   short,
 *   short,
 *   int)
 * ```
 *
 * and then the relink fails: without a binary the probe cannot print a single boot marker, so a
 * carve that *added* two unstubbable symbols would have cost the whole diagnostic. Defining
 * them here is what keeps the probe runnable.
 *
 * ## The bodies are the class's contract, not retail's instructions
 *
 * Retail's constructor and destructor are not written. What is implemented is the state the
 * header declares and that anything reading a `CTexture` afterwards depends on: the texel
 * format, the two dimensions, one mip level, and zero everywhere else. **No claim is made that
 * this is what 0x802C6000 does** - it is what a `CTexture` has to be for
 * `CCubeRenderer`'s constructor to have run to completion. `GetBitMapData(0)` therefore returns
 * null and `Load` is a no-op, and the bitfields `Lock`/`UnLock` maintain are still their own
 * code (`include/Kyoto/Graphics/CTexture.hpp`, `Lock` is inline).
 *
 * A lane that decompiles `CTexture.cpp` should delete this file and list that one; the two
 * definitions are deliberately the *same* symbols, so there is no duplicate if only one of the
 * two files is ever listed.
 */

#include "types.h"

#include "Kyoto/Graphics/CTexture.hpp"

CTexture::CTexture(ETexelFormat fmt, short w, short h, int mips) {
  mTexelFormat = fmt;
  mWidth = w;
  mHeight = h;
  // `mNumMips` and `mBitsPerPixel` were `uchar` in the pre-merge header and are `char` upstream.
  // Same 1-byte store at the same offset, and the only values reaching them are 0 and 1
  // (`PortPoolStandIns.cpp:567`, `CWorldShadow.cpp:16`, `CParticleDataFactory.cpp:1524`), which
  // read back the same through the signed `char` - the probe builds with `-fsigned-char`.
  mNumMips = static_cast< char >(mips);
  mBitsPerPixel = 0;
  mLocked = false;
  // `mCanLoadPalette` is the one member that is **not** renamed: upstream's flag byte has no such
  // bit, and the declaration order there is `mLocked`, `mIsPowerOfTwo`, `mNoSwap`, `mCounted`,
  // `mCanLoadObj` - so the bit that used to be `mCanLoadPalette` is `mIsPowerOfTwo`'s now, and
  // `mCanLoadObj` is `bool : 1` where it was `uchar : 1`. Dropping the store is behaviour-preserving
  // for this constructor specifically because it was clearing the bit: both old and new bodies
  // write every bit they declare, and `CHECK_SIZEOF(CTexture, 0x68)` holds either way - 6 one-bit
  // fields and 5 both fit the same byte, and the two spare bits are untouched padding in both.
  // Nothing in the port reads a palette-load flag; `HasPalette()` is `IsCITextureFormat(mTexelFormat)`.
  mIsPowerOfTwo = false;
  mNoSwap = false;
  mCounted = false;
  mCanLoadObj = false;
  mMemoryAllocated = 0;
  // `mNativeFormat`/`mNativeCIFormat` were `uint` in the pre-merge header and are the GX enums
  // upstream. The stored value is unchanged - still the literal 0 the old body wrote - and the
  // casts are what keep it that way. Note 0 is `GX_TF_I4` in `GXTexFmt` and is **not** a member
  // of `GXCITexFmt` (whose lowest is `GX_TF_C4 = 0x8`), so the enum name cannot be spelled; the
  // cast preserves the bits, which is what the rest of the class compares. `InitTextureObjects`
  // (`DolphinCTexture.cpp:386-416`) is what assigns a real format.
  mNativeFormat = static_cast< GXTexFmt >(0);
  mNativeCIFormat = static_cast< GXCITexFmt >(0);
  mClampMode = kCM_Clamp;
  mFrameAllocated = 0;
}

CTexture::~CTexture() {}
