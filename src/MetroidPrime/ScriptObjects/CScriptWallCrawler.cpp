#include "REL/REL_Setup.h"

extern "C" const float lbl_8041AAB8;
extern "C" const float lbl_8041B758;
extern "C" const unsigned short kInvalidUniqueId;

struct SCrawlerDispatch {
  virtual void Slot00();
  virtual void Slot01();
  virtual void Slot02();
  virtual void Slot03();
  virtual void Slot04();
  virtual void Slot05();
  virtual void Slot06();
  virtual void Slot07();
  virtual void Slot08();
  virtual void Slot09();
  virtual void Slot10();
  virtual void Slot11();
  virtual void* Slot12();
};

extern "C" {

// The lower-level names describe the observed member behavior until their original
// CWallCrawler API names can be confirmed from the class declaration.
void* CWallCrawler_GetMember838(void* self) { return static_cast<char*>(self) + 0x838; }
void RELExit(void) {}
void RELMain(void) {}
void* CWallCrawler_ForwardVirtual14(void* self) {
  return reinterpret_cast<SCrawlerDispatch*>(self)->Slot12();
}
void CWallCrawler_CopyVectorFromArgument(void* out, const void* source) {
  const float* values = reinterpret_cast<const float*>(static_cast<const char*>(source) + 0x54);
  float* result = static_cast<float*>(out);
  result[0] = values[0];
  result[1] = values[1];
  result[2] = values[2];
}
bool fn_83_78(void*) { return false; }
bool fn_83_70(void*) { return false; }
bool fn_83_68(void*) { return true; }
void* CWallCrawler_GetMember754(void* self) { return static_cast<char*>(self) + 0x754; }
float CWallCrawler_GetFloatParameter(void*) { return lbl_8041B758; }
bool CWallCrawler_HasFlag34CBit3(const void* self) {
  return (static_cast<const unsigned char*>(self)[0x34C] & 8) != 0;
}
void CWallCrawler_InitInvalidUniqueId(void* self) {
  *static_cast<unsigned short*>(self) = kInvalidUniqueId;
}
bool fn_83_30(void*) { return false; }
bool fn_83_28(void*) { return false; }
bool fn_83_20(void*) { return false; }
bool fn_83_18(void*) { return false; }
unsigned char CWallCrawler_GetByte44F(const void* self) {
  return *reinterpret_cast<const unsigned char*>(static_cast<const char*>(self) + 0x44F);
}
void CWallCrawler_SetDefaultFloat448(void* self) {
  *reinterpret_cast<float*>(static_cast<char*>(self) + 0x448) = lbl_8041AAB8;
}

} // extern "C"
