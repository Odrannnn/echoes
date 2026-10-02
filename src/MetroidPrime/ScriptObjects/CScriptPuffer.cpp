// Small CPuffer vtable helpers whose types are not yet described by a local header.
// Keep the unidentified entry points under their retail names until their virtual
// signatures can be established from CPatterned's interface.

extern "C" float skDamageHitTime__10CPatterned;

extern "C" bool fn_52_30(void*) { return false; }

extern "C" bool fn_52_28(void*) { return false; }

extern "C" bool fn_52_20(void*) { return false; }

extern "C" bool fn_52_18(void*) { return false; }

extern "C" bool fn_52_10(const void* self) {
  return *reinterpret_cast< const bool* >(static_cast< const char* >(self) + 0x44f);
}

extern "C" void fn_52_0(void* self) {
  *reinterpret_cast< float* >(static_cast< char* >(self) + 0x448) = skDamageHitTime__10CPatterned;
}
