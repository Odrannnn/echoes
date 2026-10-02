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

// Retail offset 0x1000: below __unregister_fragment (0x13c4) and above ~bad_exception
// (0x850), so it is declared here.
namespace std {
void terminate();
}

// The PowerPC EABI unwind runtime that retail's compiler emitted into this unit.
class MWExceptionInfo {
public:
  char* exception_record;
  char* current_exception;
  void (*cleanup)(void*, int);
};

void ExPPC_FindExceptionRecord(char* pc, MWExceptionInfo* info);

// One entry of the per-fragment action table ExPPC_NextAction walks.
class ActionRecord {
public:
  unsigned code_start;
  unsigned next;
};

// The header ExPPC_NextAction indexes through to find the enclosing fragment.
class ActionTable {
public:
  unsigned short flags;
};

// The unwind cursor. Retail's field order comes from the loads in
// ExPPC_NextAction: table (0x00), cur (0x08), node (0x18), the pending action
// (0x1C) and the cached code pointer (0x20).
class ActionIterator {
public:
  ActionTable* table;
  unsigned fragment_id;
  unsigned char* cur;
  unsigned scope;
  unsigned handler;
  unsigned region;
  unsigned** node;
  unsigned action;
  unsigned code;
};

int ExPPC_NextAction(ActionIterator* it) {
  for (;;) {
    if (it->cur != 0 && !(it->cur[0] & 0x80)) {
      break;
    }

    {
      unsigned* node = *(unsigned**)it->node;
      int index = it->table->flags;

      if ((index >> 11) != 0) {
        // The stack slot the unwind step reads is indexed by a five-bit field at
        // bits 3-7 of the fragment's table header; retail spells the extraction as
        // a shift-then-mask, and mwcceppc only emits retail's `rlwinm` (with the
        // rotate) for that spelling - `index & 0x1F` masks the same register bits
        // but is emitted as `clrlwi`, which is 4 bytes off retail.
        it->code = *((unsigned*)((char*)node - ((index >> 3) & 0xF8)) - 1);
      }
      ExPPC_FindExceptionRecord((char*)node[1], (MWExceptionInfo*)it);
      if (it->table == 0) {
        std::terminate();
      }
      it->node = (unsigned**)node;
      it->action = (it->table->flags >> 4) & 1 ? it->code : (unsigned)it->node;
      if (it->cur == 0) {
        continue;
      }
      goto found;
    }
  }

  switch (it->cur[0]) {
  case 2:
    it->cur += 8;
    break;
  case 3:
    it->cur += 12;
    break;
  case 4:
    it->cur += 8;
    break;
  case 5:
    it->cur += 12;
    break;
  case 6:
  case 7:
    it->cur += 12;
    break;
  case 8:
    it->cur += 16;
    break;
  case 9:
    it->cur += 20;
    break;
  case 10:
    it->cur += 8;
    break;
  case 11:
    it->cur += 12;
    break;
  case 12:
    it->cur += 12;
    break;
  case 16:
    it->cur += 16;
    break;
  case 13:
    it->cur += 4;
    break;
  case 15:
    it->cur += *(unsigned short*)(it->cur + 2) * 4 + 12;
    break;
  case 0:
  case 1:
  case 14:
  default:
    std::terminate();
  }

found: {
    unsigned action = it->cur[0] & 0x7F;
    if (action == 1) {
      it->cur = (unsigned char*)it->table + *(unsigned short*)(it->cur + 2);
      action = it->cur[0] & 0x7F;
    }
    return action;
  }
}

// Retail offset 0x850: below every function above, so it is declared last of the
// C++ definitions.
namespace std {
bad_exception::~bad_exception() throw() {}
} // namespace std

extern "C" void __end__catch(MWExceptionInfo* info) {
  if (info->exception_record != 0 && info->cleanup != 0) {
    info->cleanup(info->exception_record, -1);
  }
}
