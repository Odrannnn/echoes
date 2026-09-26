/**
 * `fn_80142914` - retail `.text:0x80142914`, `size:0xC` = 12 bytes: `li r0,0 ; stw r0,4(r3) ;
 * blr`. `rstl::vector<unsigned char>::clear` on the 16-byte `SGameStateBlock` - the count goes to
 * zero, the buffer and its capacity stay. `fn_80142BA4` (`CGameStateBlockFill.cpp`) calls it first.
 */

#include "MetroidPrime/Player/CGameStateBlocks.hpp"

extern "C" void fn_80142914(SGameStateBlock* self) { self->x04_count = 0; }
