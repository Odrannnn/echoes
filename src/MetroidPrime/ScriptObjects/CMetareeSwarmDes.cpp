// CMetareeSwarmDes.cpp - experiment

#include "types.h"

extern "C" {
// This unit is listed in `files.cmake` beside the other module-internal carves because it defines
// no RELMain/RELExit: its only relocation outside itself is the call below, to `fn_43_1FB8`, which
// sits at .text 0x1FB8 - the address the split ends on - and is 0x13C bytes of the module's own
// code, not in any port binary. On the host that call is one more undefined symbol, and
// `tools/link_check.sh --strict` fails on a growing count, so it is behind the guard
// `CEmperorIngStage3Rel.cpp` uses for `fn_18_DAEC`. Everything else here is raw offsets into
// `self` and calls into this same unit, so the port gains nothing it cannot link. The MWCC branch
// is the retail source token for token, so the matching build cannot see the guard.
#ifdef __MWERKS__
void fn_43_1FB8(void* self);
#endif

void fn_43_1F90(void* self) {
  if (self) {
#ifdef __MWERKS__
    fn_43_1FB8(self);
#endif
  }
}

void fn_43_1F70(void* self) { fn_43_1F90(self); }

void fn_43_1F38(void* self) {
  char* self_bytes = static_cast<char*>(self);
  int* count = reinterpret_cast<int*>(self_bytes + 4);
  char* array = *reinterpret_cast<char**>(self_bytes + 0xC);
  char* element = array + *count * 0x4C;
  *count = *count + 1;
  fn_43_1F70(element);
}
}
