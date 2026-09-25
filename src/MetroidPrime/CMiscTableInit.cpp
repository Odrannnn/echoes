// Retail 0x8029EFCC, 0x54 bytes. Unnamed in retail, inside dtk's auto_03_8028C9C8_text
// blob. Its only caller is CGameArchitectureSupport::CGameArchitectureSupport at
// 0x80007FA4, between CAudioSys::SetVolumeScale and fn_8033CEE8, which is where
// src/MetroidPrime/main.cpp:237 calls it.
//
// The class is not recovered and the two globals it touches are in no header, so this is
// written at raw offsets and nothing more is claimed:
//
//   0x80411068 (lbl_80411068, .bss, 0x920 bytes)
//       +0x36C  int, a count
//       +0x370  int[364], the table: (0x920 - 0x370) / 4
//   0x804152DC (lbl_804152DC, .bss, 0x1834 bytes) - the receiver of fn_80340F9C, which has
//       a 0x1850-byte frame and pushes three 8-byte entries into a bounded array at its own
//       +0x1814, indexed by the count at +0x1810. (fn_80340F9C is 0x2A0 bytes, so the frame
//       is not a mistake; the array holds four, which is why the callee bounds-checks.)
//
// The body: append a zero at the current end of the table, bump the count, then ask the
// other object to gather.
//
// Taking a *pointer to the count* is what reproduces retail's register allocation, and it
// is not cosmetic. Written as absolute subscripts of lbl_80411068, MWCC keeps the object's
// base in a register and emits 876/880 displacements - four bytes short of retail. Written
// as below, it keeps `base + 0x36C` live for the whole function, so the table store becomes
// `add r4, r6, r0` + `stw r5, 4(r4)` and the count update `lwz r4, 0(r6)` +
// `stw r0, 0(r6)`, which is retail byte for byte.
//
// The `fn_` name is kept: retail names nothing here, and main.cpp already calls it under
// this name.
#ifndef TARGET_PC
extern "C" {
extern int lbl_80411068[];
extern char lbl_804152DC[];
// fn_80340F9C is unwritten and is *not* on the port's link-gap list, so calling it from the
// host build would add one missing symbol and remove none. See the TARGET_PC branch.
extern void fn_80340F9C(void*);
} // extern "C"

enum { kTableCount = 0x36C / 4 };

extern "C" void fn_8029EFCC() {
  int* count = &lbl_80411068[kTableCount];
  count[1 + *count] = 0;
  *count = *count + 1;
  fn_80340F9C(lbl_804152DC);
}
#else
// Neither global exists on the host - two .bss objects at guest addresses with no counterpart
// in the port - so there is nothing to append to and nothing to notify. This is a real
// behaviour gap and it is recorded rather than papered over: whatever retail registers here
// at construction time, the port does not.
extern "C" void fn_8029EFCC() {}
#endif // TARGET_PC
