#include "Kyoto/Math/CVector3f.hpp"

// The module's field schema is not recovered yet. These compact accessors are
// collected in offset order so the portions of the state record they touch are
// explicit without guessing names for the surrounding UI types.
class CFrontEndDataNetworkState {
public:
  void AppendPointer(void* value);
  CVector3f* GetVectorAt20();
  void SetVectorAt20(const CVector3f& value);
  CVector3f* GetVectorAt2C();
  void SetVectorAt2C(const CVector3f& value);
  CVector3f* GetVectorAt50();
  void SetVectorAt50(const CVector3f& value);
  CVector3f* GetVectorAt38();
  void SetVectorAt38(const CVector3f& value);
  CVector3f* GetVectorAt44();
  void SetVectorAt44(const CVector3f& value);
  void SetIntAt8(int value);
  int GetIntAt1C() const;
  void SetIntAt1C(int value);
  void SetFloatAt5C(float value);
  void SetFloatAt60(float value);
  void SetFloatAt64(float value);
};

void CFrontEndDataNetworkState::AppendPointer(void* value) {
  struct PointerArray {
    char x0[0x10];
    unsigned int count;
    unsigned int capacity;
    void** values;
  };
  PointerArray* array = reinterpret_cast<PointerArray*>(this);
  array->values[array->count++] = value;
}

CVector3f* CFrontEndDataNetworkState::GetVectorAt20() {
  return reinterpret_cast<CVector3f*>(reinterpret_cast<char*>(this) + 0x20);
}

void CFrontEndDataNetworkState::SetVectorAt20(const CVector3f& value) {
  const volatile float* source = reinterpret_cast<const volatile float*>(&value);
  float x = source[0];
  float y = source[1];
  *reinterpret_cast<float*>(reinterpret_cast<char*>(this) + 0x20) = x;
  float z = source[2];
  *reinterpret_cast<float*>(reinterpret_cast<char*>(this) + 0x24) = y;
  *reinterpret_cast<float*>(reinterpret_cast<char*>(this) + 0x28) = z;
}

CVector3f* CFrontEndDataNetworkState::GetVectorAt2C() {
  return reinterpret_cast<CVector3f*>(reinterpret_cast<char*>(this) + 0x2C);
}

void CFrontEndDataNetworkState::SetVectorAt2C(const CVector3f& value) {
  const volatile float* source = reinterpret_cast<const volatile float*>(&value);
  float x = source[0];
  float y = source[1];
  *reinterpret_cast<float*>(reinterpret_cast<char*>(this) + 0x2C) = x;
  float z = source[2];
  *reinterpret_cast<float*>(reinterpret_cast<char*>(this) + 0x30) = y;
  *reinterpret_cast<float*>(reinterpret_cast<char*>(this) + 0x34) = z;
}

CVector3f* CFrontEndDataNetworkState::GetVectorAt50() {
  return reinterpret_cast<CVector3f*>(reinterpret_cast<char*>(this) + 0x50);
}

void CFrontEndDataNetworkState::SetVectorAt50(const CVector3f& value) {
  const volatile float* source = reinterpret_cast<const volatile float*>(&value);
  float x = source[0];
  float y = source[1];
  *reinterpret_cast<float*>(reinterpret_cast<char*>(this) + 0x50) = x;
  float z = source[2];
  *reinterpret_cast<float*>(reinterpret_cast<char*>(this) + 0x54) = y;
  *reinterpret_cast<float*>(reinterpret_cast<char*>(this) + 0x58) = z;
}

CVector3f* CFrontEndDataNetworkState::GetVectorAt38() {
  return reinterpret_cast<CVector3f*>(reinterpret_cast<char*>(this) + 0x38);
}

void CFrontEndDataNetworkState::SetVectorAt38(const CVector3f& value) {
  const volatile float* source = reinterpret_cast<const volatile float*>(&value);
  float x = source[0];
  float y = source[1];
  *reinterpret_cast<float*>(reinterpret_cast<char*>(this) + 0x38) = x;
  float z = source[2];
  *reinterpret_cast<float*>(reinterpret_cast<char*>(this) + 0x3C) = y;
  *reinterpret_cast<float*>(reinterpret_cast<char*>(this) + 0x40) = z;
}

CVector3f* CFrontEndDataNetworkState::GetVectorAt44() {
  return reinterpret_cast<CVector3f*>(reinterpret_cast<char*>(this) + 0x44);
}

void CFrontEndDataNetworkState::SetVectorAt44(const CVector3f& value) {
  const volatile float* source = reinterpret_cast<const volatile float*>(&value);
  float x = source[0];
  float y = source[1];
  *reinterpret_cast<float*>(reinterpret_cast<char*>(this) + 0x44) = x;
  float z = source[2];
  *reinterpret_cast<float*>(reinterpret_cast<char*>(this) + 0x48) = y;
  *reinterpret_cast<float*>(reinterpret_cast<char*>(this) + 0x4C) = z;
}

void CFrontEndDataNetworkState::SetIntAt8(int value) {
  *reinterpret_cast<int*>(reinterpret_cast<char*>(this) + 0x08) = value;
}

int CFrontEndDataNetworkState::GetIntAt1C() const {
  return *reinterpret_cast<const int*>(reinterpret_cast<const char*>(this) + 0x1C);
}

void CFrontEndDataNetworkState::SetIntAt1C(int value) {
  *reinterpret_cast<int*>(reinterpret_cast<char*>(this) + 0x1C) = value;
}

void CFrontEndDataNetworkState::SetFloatAt5C(float value) {
  *reinterpret_cast<float*>(reinterpret_cast<char*>(this) + 0x5C) = value;
}

void CFrontEndDataNetworkState::SetFloatAt60(float value) {
  *reinterpret_cast<float*>(reinterpret_cast<char*>(this) + 0x60) = value;
}

void CFrontEndDataNetworkState::SetFloatAt64(float value) {
  *reinterpret_cast<float*>(reinterpret_cast<char*>(this) + 0x64) = value;
}
