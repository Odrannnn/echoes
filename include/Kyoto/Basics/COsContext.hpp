#ifndef _COSCONTEXT
#define _COSCONTEXT

#include <stddef.h>

#include "types.h"

#include <dolphin/gx/GXStruct.h>

enum EConsoleType {
  kCT_Emulator,
  kCT_Development1,
  kCT_Development2Or3,
  kCT_Retail,
};

class COsKeyState {
public:
  COsKeyState(int key, bool down, bool released, bool repeat, bool unk)
  : x0_key(key), x4_down(down), x4_released(released), x4_repeat(repeat), x4_unk(unk) {}

  bool IsPressed() const { return x4_down; }
  bool JustPressed() const { return x4_unk && x4_down; }

private:
  int x0_key;
  short x4_down : 1;
  short x4_released : 1;
  short x4_repeat : 1;
  short x4_unk : 1;
};

class COsContext {
public:
  // TODO: Once main.cpp is matched make this private
  static bool mProgressiveMode;

  COsContext(bool, bool);
  ~COsContext();

  int OpenWindow(const char* title, int x, int y, int w, int h, bool fullscreen);
  bool Update();
  COsKeyState GetOsKeyState(int key) const;

  void* AllocFromArena(size_t sz);

  uint GetBaseFreeRam() const {
    size_t hiAddr = reinterpret_cast< size_t >(x1c_arenaHi);
    size_t loAddr = reinterpret_cast< size_t >(x20_arenaLo2);
    return ((hiAddr & ~31) - ((loAddr + 31) & ~31));
  }

  void* GetFramebuf1() const { return x24_frameBuffer1; }
  void* GetFramebuf2() const { return x28_frameBuffer2; }
  const GXRenderModeObj& GetRenderModeObj() const { return x30_renderMode; }

  static void SetProgressiveMode(bool progressive) { mProgressiveMode = progressive; }
  static bool GetProgressiveMode() { return mProgressiveMode; }

private:
  int x0_right;
  int x4_bottom;
  int x8_left;
  int xc_top;
  // +0x10 is the console type and +0x14 is the language nibble, in that order.
  // This header had them the other way round - `x10_format` and
  // `x14_consoleType` - which reads as a guess, because retail's constructor
  // (0x8028C09C, `COsContextCtor.cpp`) stores `OSGetLanguage() & 0xF` at +0x14
  // *before* CBasics::Init and the EConsoleType value at +0x10 after it. Neither
  // word is read anywhere in retail, so the swap was invisible; it was found by
  // reading the constructor's stores rather than the header.
  int x10_consoleType;
  int x14_language;
  void* x18_arenaLo1;
  void* x1c_arenaHi;
  void* x20_arenaLo2;
  void* x24_frameBuffer1;
  void* x28_frameBuffer2;
  int x2c_frameBufferSize;
  GXRenderModeObj x30_renderMode;
};
CHECK_SIZEOF(COsContext, 0x6c)

#endif // _COSCONTEXT
