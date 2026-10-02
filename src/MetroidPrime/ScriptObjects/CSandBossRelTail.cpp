// CSandBossRelTail.cpp - SandBoss's (module 55) out-of-line template tail, .text
// 0x10548..0x1073C: eight functions the module's own translation units emitted out of line, all
// of them one 0x2C-byte record's container instantiation, its payload copy and two destructors.
//
//   0x10548 fn_55_10548  0x40  the two-member copy: the 2-byte first member by value, the second
//                              member copy-constructed at +0x4 by fn_55_105C4
//   0x10588 fn_55_10588  0x3C  deleting destructor of a memberless object
//   0x105C4 fn_55_105C4  0x40  copy of the class whose only data is the byte at +0x30, the
//                              "engaged" flag; when it is set the payload copy runs
//   0x10604 fn_55_10604  0x20  forwarder to fn_55_10624
//   0x10624 fn_55_10624  0x28  null-guarded forwarder to fn_55_106E0
//   0x1064C fn_55_1064C  0x3C  deleting destructor of a memberless object
//   0x10688 fn_55_10688  0x58  the payload's own constructor: a float at +0x0, three floats at
//                              +0x4, four at +0x10, two words at +0x20, a `1` byte at +0x28
//   0x106E0 fn_55_106E0  0x5C  the payload copy: the same 0x2C bytes copied member for member
//
// The chain is `fn_55_105C4` -> `fn_55_10604` -> `fn_55_10624` -> `fn_55_106E0`; `fn_55_1073C`
// (0x24) is the first function above the claim and stays retail's, as does everything else between
// the module head (0x0..0x178) and here. Every body is read off
// `build/G2ME01/SandBoss/asm/auto_00_00000178_text.s`, and every name is the module's own, from
// `config/G2ME01/rels/SandBoss/symbols.txt`.
//
// **`mw_version="GC/2.7"` is load-bearing and measured.** Under the module's default GC/1.3.2,
// `fn_55_10548` compiles to 62.5% and `fn_55_105C4` to 77.5%: both want the argument setup (a
// `lhz`/`lbz`) scheduled *in front of* the `stw r31,0xc(r1)` prologue save, and 1.3.2 puts the
// save first. 2.7 emits retail's order and both go to 100.00%, so this tail was compiled by the
// later compiler - the same per-object `mw_version` arrangement `CLumiteRelTail.cpp` uses.
//
// **The two destructors need the module's `force_active` list** (`config/G2ME01/config.yml`):
// nothing in the module calls them, so mwldeppc dead-strips them and the module links 0x78 bytes
// short with every function at 100.00%. They are listed there by name.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and only the module's sha1 would catch it.

/** 0x802CE388, `symbols.txt`: `CMemory::Free(void const*)`, claimed by `Kyoto/Alloc/CMemory.cpp`
 *  in the DOL. Declared under retail's own emitted spelling so the call needs no header. */
extern "C" void Free__7CMemoryFPCv(const void* ptr);

/** 0x1064C, 0x3C. The stock deleting destructor of an object with nothing to tear down: the
 *  receiver guard, the sign-extended flag test and `CMemory::Free(self)`. Its twin in
 *  `build/report.json` is `__dt__5CMainFv`, i.e. `CMain::~CMain() {}` in
 *  `src/MetroidPrime/main.cpp`, and the body is `Carve801FD4B0.cpp`'s deleting-destructor shape.
 *
 *  **The bodies are inside `#ifdef __MWERKS__` and the host branch is empty, so listing this file
 *  in `files.cmake` adds no undefined reference** - the arrangement `CLumiteRelTail.cpp` uses.
 *  Every call here is to a module-local symbol (`fn_55_106E0`, `fn_55_10604`) or to the DOL's
 *  `CMemory::Free`, so a host body would make the port link names it does not have. */
#ifdef __MWERKS__

/** 0x106E0, 0x5C: the payload copy - eight floats, two words and a byte, all copied in the
 *  2-deep load/store pipeline retail's own bytes show. `fn_55_10624` is its only caller, and it
 *  is called with the two pointers it was handed, so the signature stays `void*` and the record
 *  is a cast inside - a typed definition would not be callable from there. */
struct SFn55_106E0 {
  float x00;
  float x04[3];
  float x10[4];
  unsigned int x20;
  unsigned int x24;
  unsigned char x28;
};

extern "C" void fn_55_106E0(void* self_, const void* other_) {
  SFn55_106E0* self = static_cast< SFn55_106E0* >(self_);
  const SFn55_106E0* other = static_cast< const SFn55_106E0* >(other_);
  self->x00 = other->x00;
  self->x04[0] = other->x04[0];
  self->x04[1] = other->x04[1];
  self->x04[2] = other->x04[2];
  self->x10[0] = other->x10[0];
  self->x10[1] = other->x10[1];
  self->x10[2] = other->x10[2];
  self->x10[3] = other->x10[3];
  self->x24 = other->x24;
  self->x20 = other->x20;
  self->x28 = other->x28;
}

/** 0x10688, 0x58: the constructor of the same record - the incoming float lands at +0x0, then the
 *  three floats at +0x4, the four at +0x10, two words at +0x20 and a `1` byte at +0x28. */
struct SFn55_10688 {
  float x00;
  float x04[3];
  float x10[4];
  unsigned int x20;
  unsigned int x24;
  unsigned char x28;
};

extern "C" void fn_55_10688(SFn55_10688* self, float value, const float* a, const float* b,
                            const unsigned int* words) {
  self->x00 = value;
  self->x04[0] = a[0];
  self->x04[1] = a[1];
  self->x04[2] = a[2];
  self->x10[0] = b[0];
  self->x10[1] = b[1];
  self->x10[2] = b[2];
  self->x10[3] = b[3];
  self->x24 = words[1];
  self->x20 = words[0];
  self->x28 = 1;
}

extern "C" void* fn_55_1064C(void* self, short flag) {
  if (self) {
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

/** 0x10624, 0x28. `if (self) fn_55_106E0(self, other);` - the null test is on the receiver and
 *  the payload pointer is handed through untouched. */
extern "C" void fn_55_10624(void* self, const void* other) {
  if (self) {
    fn_55_106E0(self, other);
  }
}

/** 0x10604, 0x20. A plain forwarder: retail makes a frame for the call and does nothing else. */
extern "C" void fn_55_10604(void* self, const void* other) { fn_55_10624(self, other); }

/** 0x105C4, 0x40. The class's only data this function reads is the byte at +0x30. Retail stores
 *  the flag unconditionally and branches on the value it loaded, then returns the receiver. */
struct SFn55_105C4 {
  unsigned char x00_pad[0x30];
  unsigned char x30_flag;
};

extern "C" SFn55_105C4* fn_55_105C4(SFn55_105C4* self, const SFn55_105C4* other) {
  self->x30_flag = other->x30_flag;
  if (other->x30_flag) {
    fn_55_10604(self, other);
  }
  return self;
}

/** 0x10588, 0x3C. `fn_55_1064C`'s body under its own symbol - a second deleting destructor of a
 *  memberless object. */
extern "C" void* fn_55_10588(void* self, short flag) {
  if (self) {
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

/** 0x10548, 0x40. The class's two members: a 2-byte first member at +0x0 and the second at +0x4,
 *  whose copy is `fn_55_105C4`. Retail loads the first member, forms both addresses, stores the
 *  first member and then calls, and returns the receiver - so this is a reference-returning copy,
 *  not a constructor. */
struct SFn55_10548 {
  unsigned short x00;
  unsigned char x02_pad[2];
  unsigned char x04_second;
};

extern "C" SFn55_10548* fn_55_10548(SFn55_10548* self, const SFn55_10548* other) {
  self->x00 = other->x00;
  fn_55_105C4(reinterpret_cast< SFn55_105C4* >(&self->x04_second),
              reinterpret_cast< const SFn55_105C4* >(&other->x04_second));
  return self;
}

#endif
