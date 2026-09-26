// CPlayer::GetPlayerIndex - retail 0x8000D084, 0x8 bytes.
//
//   8000d084: 80 63 13 b8   lwz    r3,5048(r3)   ; m_x13b8_playerIndex
//   8000d088: 4e 80 00 20   blr
//
// A whole-word load and a return; `lwz` not `lbz`/`lhz`, so the field is a 32-bit
// int and not a flag. Its neighbours in retail are the same shape at other offsets
// (fn_8000D08C reads +0xEBC, fn_8000D094 is 8 bytes), i.e. this is a run of
// accessors and the header models only this one of them.

#include "MetroidPrime/Player/CPlayer.hpp"

int CPlayer::GetPlayerIndex() const {
  return m_x13b8_playerIndex;
}
