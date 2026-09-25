#ifndef _CSAVEGAMESCREEN
#define _CSAVEGAMESCREEN

#include "types.h"

class CSaveGameScreen {
public:
  CSaveGameScreen(int saveContext, u64 cardSerial);
  ~CSaveGameScreen();

  int GetUnk80() const { return x80_unk; }

private:
  char pad[0x80];
  int x80_unk;
  char pad84[0x14];
};
CHECK_SIZEOF(CSaveGameScreen, 0x98)

#endif // _CSAVEGAMESCREEN
