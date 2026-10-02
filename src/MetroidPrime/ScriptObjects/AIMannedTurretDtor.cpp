// AIMannedTurretDtor.cpp - `fn_1_48C0`, retail `.text 0x48C0..0x491C`, 0x5C = 92 bytes, the
// deleting destructor of one of the module's unnamed classes, carved out of dtk's
// `auto_00_00000018_text` range. This is the worked example for the REL twin family
// `tools/twin_scan.py` reports as a byte-shape twin of `__dt__21CArchMsgParmUserInputFv`
// (`src/MetroidPrime/CArchMsgParmUserInput.cpp`, DOL 0x8001D7E0, also 0x5C): `twin_scan.py
// --list` on this tree pairs **221 unmatched REL functions in 50 modules** whose matched twin's
// source is that file (measured 2026-10-02), and this is the first of them landed inside a
// module.
//
// **What the bytes are** (read from `build/G2ME01/AIMannedTurret/asm/auto_00_00000018_text.s`,
// lines 5184-5211, and from the relocations dtk emitted into the retail-derived object):
//
//   mr.  r31, r3 / beq <end>            the null-`this` guard MWCC emits for `__dt`
//   lis/addi lbl_1_data_1EC / stw r0,0(r31)
//   beq <skips the second store>        the base subobject's own guard, reusing cr0
//   lis/addi lbl_1_data_1E0 / stw r0,0(r31)
//   extsh. r0,r4 / ble <end>            `dispose > 0` - a **short** parameter
//   mr   r3, r31 / bl Free__7CMemoryFPCv
//   lwz r0,0x14(r1) / mr r3,r31 / ...   epilogue, returns `this`
//
// That is MWCC's deleting-destructor convention, and `fn_1_4830` (the constructor that allocates
// the 8-byte object this tears down) stores the same two labels - `lbl_1_data_1EC` then
// `lbl_1_data_1E0` - so the class sits two levels below the module's most-derived class, whose
// vtable is `lbl_1_data_1F8` (see `fn_1_4728`).
//
// **Why it is written as an `extern "C"` function and not as a class.** A `class` with a virtual
// destructor reproduces the shape (measured: `class B { public: virtual ~B() {} }; class C :
// public B { public: ~C(); }; C::~C() {}` compiles to these same 23 instructions) but
// mwcceppc *emits the two vtables into the object's `.data`* - 0x18 bytes carrying two
// `R_PPC_ADDR32` relocations to the destructor (measured on that same test translation unit).
// This module's `lbl_1_data_1E0`/`lbl_1_data_1EC` are 12 zero bytes each with **no relocation at
// all** (measured with `powerpc-eabi-objdump -s -j .data
// build/G2ME01/AIMannedTurret/obj/auto_04_00000000_data.o` and `-r` on the same object; the
// module's own relocation table has no entry for either), so a compiler-emitted vtable cannot be
// what these labels hold, and a `.text`-only split does not claim the object's `.data`. `extern
// "C"` with the vptr stores written out is the spelling that leaves the object with exactly one
// `.text` section of 0x5C bytes and the module's `.data` untouched - the same arrangement
// `src/MetroidPrime/Carve8000447C.cpp` uses for the same convention in the DOL.
//
// The two stores are `mvptr = <label>` on a one-member struct rather than a raw offset, so
// nothing here is a raw-offset access. The nested `if (self)` is load-bearing: it is what makes
// MWCC reuse the cr0 set by `mr. r31,r3` for the second `beq`. Written flat - the two stores in a
// row - MWCC instead hoists both address computations into r5/r6, drops the second guard and the
// `mr r3,r31` before the call, and the body comes out **0x54 bytes, not 0x5C** (measured).
//
// Source order is descending by retail offset; this unit has one function, so it is trivially
// so. The module's sha1 against `config/G2ME01/config.yml` is the acceptance test, not objdiff.
//
// `config/G2ME01/config.yml` lists `fn_1_48C0` under this module's `force_active:` because
// nothing references it: without that entry mwldeppc drops the object's `.text` - measured, the
// module came out with `.text` 0x52EC against retail's 0x5348 and a file 128 bytes short (the
// dead-strip hazard `docs/RUNNING_THE_DECOMP.md` records as structural fact 3). With the entry
// the module's sha1 is `949b8c21caf1112b10d07748dbe8c32d3bd7efac`, which is what
// `config/G2ME01/config.yml` and `config/G2ME01/build.sha1` both record.
//
// Only the MWCC branch is real code. The port neither links this module's objects nor has the
// two `.data` labels this one relocates against, so the host translation unit is deliberately
// empty: an unguarded body would add `lbl_1_data_1E0`/`lbl_1_data_1EC` to the port's link gap.

#ifdef __MWERKS__

extern "C" char lbl_1_data_1E0[];
extern "C" char lbl_1_data_1EC[];

/** 0x802CE388, `CMemory::Free(void const*)`. Claimed by `Kyoto/Alloc/CMemory.cpp`, and the port
 *  has the same `extern "C"` name in `Kyoto/Alloc/PortMwccNew.cpp`. Declared, never defined. */
extern "C" void Free__7CMemoryFPCv(const void* ptr);

/** The object as far as this destructor reads it: a vptr at +0, which is all the class's public
 *  surface this function touches. */
struct SAIMannedTurretVptr {
  void* mVptr;
};

/** `fn_1_48C0` - retail `.text:0x48C0`, 0x5C = 92 bytes. The five relocations dtk's object carries
 *  for it are exactly the five below: `lbl_1_data_1EC` at +0x2/+0x6, `lbl_1_data_1E0` at
 *  +0x12/+0x16 and `Free__7CMemoryFPCv` at +0x28, all `R_PPC_ADDR16_HA/LO` in that order
 *  followed by `R_PPC_REL24`. */
extern "C" void* fn_1_48C0(SAIMannedTurretVptr* self, short flag) {
  if (self) {
    self->mVptr = lbl_1_data_1EC;
    if (self) {
      self->mVptr = lbl_1_data_1E0;
    }
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

#endif // __MWERKS__
