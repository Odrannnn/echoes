#include "NMWException.h"
#include "__ppc_eabi_linker.h"

#if __MWERKS__
#pragma exceptions on
#endif

namespace std {
class exception {
public:
  exception() throw() {}
  virtual ~exception() throw() {}
  virtual const char* what() const throw();
};

// Declared first so it is emitted last, at retail's offset (0x142C), the
// highest function in this unit.
class bad_exception : public exception {
public:
  virtual ~bad_exception() throw();
  virtual const char* what() const throw();
};

const char* bad_exception::what() const throw() { return "bad_exception"; }
} // namespace std

typedef struct ProcessInfo {
  __eti_init_info* exception_info;
  char* TOC;
  int active;
} ProcessInfo;

static ProcessInfo fragmentinfo[1];

int __register_fragment(struct __eti_init_info* info, char* TOC) {
  ProcessInfo* f;
  int i;

  for (i = 0, f = fragmentinfo; i < 1; ++i, ++f) {
    if (f->active == 0) {
      f->exception_info = info;
      f->TOC = TOC;
      f->active = 1;
      return i;
    }
  }

  return -1;
}

void __unregister_fragment(int fragmentId) {
  ProcessInfo* f;
  if (fragmentId >= 0 && fragmentId < 1) {
    f = &fragmentinfo[fragmentId];
    f->exception_info = 0;
    f->TOC = 0;
    f->active = 0;
  }
}

// Retail offset 0x850: below every function above, so it is declared last of the
// C++ definitions.
namespace std {
bad_exception::~bad_exception() throw() {}
} // namespace std

// The PowerPC EABI unwind runtime that retail's compiler emitted into this unit.
// Retail offset 0x248, below everything above.
typedef struct MWExceptionInfo {
  char* exception_record;
  char* current_exception;
  void (*cleanup)(void*, int);
} MWExceptionInfo;

extern "C" void __end__catch(MWExceptionInfo* info) {
  if (info->exception_record != 0 && info->cleanup != 0) {
    info->cleanup(info->exception_record, -1);
  }
}
