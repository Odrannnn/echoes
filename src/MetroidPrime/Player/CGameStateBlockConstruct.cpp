/**
 * `fn_80004D5C` - retail `.text:0x80004D5C`, `size:0x28` = 40 bytes: `rstl::construct` for the
 * 16-byte `SGameStateBlock` - a null test on the destination and a call to its copy constructor,
 * `fn_80004AA0` (`CGameStateBlockCopyCtor.cpp`). `fn_80142A10` (`CGameStateBlockCopy.cpp`) tail
 * calls it, once per element of `CGameState`'s three-slot array.
 */

#include "MetroidPrime/Player/CGameStateBlocks.hpp"

extern "C" SGameStateBlock* fn_80004AA0(SGameStateBlock* self, const SGameStateBlock* src);

extern "C" void fn_80004D5C(SGameStateBlock* self, const SGameStateBlock* src) {
  if (self != 0) {
    fn_80004AA0(self, src);
  }
}
