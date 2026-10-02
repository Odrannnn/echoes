// CScriptSafeZonePrefix.cpp - ScriptSafeZone's (module 66) head, .text 0x0..0x2C: the one
// function above the module's class code. The range comes from
// `config/G2ME01/rels/ScriptSafeZone/splits.txt`, and the function is the only symbol in it
// (`build/G2ME01/ScriptSafeZone/asm/MetroidPrime/ScriptObjects/CScriptSafeZonePrefix.s`,
// `fn_66_0`, `size:0x2C`). This unit exists in the split and had no source until now; it is the
// cheapest thing this module can decompile, because the whole range is one function and the
// module's other two ranges are claimed by units of their own:
//
//   0x00  fn_66_0  0x2C  this file: CActor's `GetHealthInfo` slot, dispatching vtable slot 0x38
//   0x2C  RELExit 0x24  li r3,0 / bl SetSSafeZone_FuncPtrs   (CScriptSafeZone.cpp, Matching)
//   0x50  RELMain 0x20  bl fn_66_70                           (CScriptSafeZone.cpp, Matching)
//   0x70  fn_66_70 ..0x8C60                                  (CScriptSafeZoneTail.cpp, 120
//                                                             functions, unclaimed)
//
// **fn_66_0 is a vtable entry, not a free function, and its bytes are identical to
// `CSnakeWeedSwarmRel.cpp`'s `fn_71_0`, `CSandwormRel.cpp`'s `fn_56_0` and
// `CFishCloudRel.cpp`'s `fn_20_0`** - all four are `.text:0x0`, `size:0x2C`, eleven instructions,
// word for word. That is CActor's `GetHealthInfo`, returning the result of the `HealthInfo` slot
// above it.
//
// It is stored in *two* of this module's vtables, so nothing here is a dead-stripping hazard and no
// `force_active:` entry is needed. `build/G2ME01/ScriptSafeZone/asm/auto_04_00000000_data.s` shows
// `lbl_66_data_10` (`.data:0x10`, 0xA0 bytes = 40 words) and `lbl_66_data_B8` (`.data:0xB8`,
// 0x7C bytes = 31 words), each holding two leading zero words (offset-to-top, then the RTTI
// pointer, both zero in this REL) and then one word per virtual, so 38 virtuals in the first and 29
// in the second - `tools/rel_class_map.py` reads the two tables' base classes off the inherited
// slots, `23CScriptTriggerEllipsoid` (22 agreeing) and `6CActor` (19 agreeing). In both, word 14 -
// vtable offset 0x38, the 13th virtual, the one the call below dispatches on - is a `HealthInfo`
// slot (`HealthInfo__6CActorFv` in the first, the module's own `fn_66_7748` in the second) and word
// 15, offset 0x3C, is `fn_66_0` in both; each table is its class's with that one slot replaced.
//
// So the call is a member call, and that spelling is measured rather than guessed: loading the
// vtable by hand - `void* const* vt = *(void* const* const*)self;` and calling `vt[14]` -
// compiles to `lwz r3,0(r3)` where retail has `lwz r12,0(r3)`, and that one register is 99.09%
// on the function (measured 2026-09-29 on CIngPuddleRel.cpp, the same call). `mwcceppc` only
// reaches for r12 on its own virtual-dispatch path, and it lays a class's virtuals out the way
// retail's vtable is laid out, so the 13th virtual is at 0x38. The slots are named by position
// because no header here models a CActor virtual; none of them is defined or called from this
// file, because the only objects that carry these vtables are the module's own retail bytes. The
// class is sized to the smaller of the two tables, which is a choice, not a measurement - only
// `Slot12`'s position is load-bearing, and it is the same in both.
class CHealthInfo;

class CScriptSafeZoneVTable {
public:
  virtual void Slot0();
  virtual void Slot1();
  virtual void Slot2();
  virtual void Slot3();
  virtual void Slot4();
  virtual void Slot5();
  virtual void Slot6();
  virtual void Slot7();
  virtual void Slot8();
  virtual void Slot9();
  virtual void Slot10();
  virtual void Slot11();
  virtual CHealthInfo* Slot12();
  virtual void Slot13();
  virtual void Slot14();
  virtual void Slot15();
  virtual void Slot16();
  virtual void Slot17();
  virtual void Slot18();
  virtual void Slot19();
  virtual void Slot20();
  virtual void Slot21();
  virtual void Slot22();
  virtual void Slot23();
  virtual void Slot24();
  virtual void Slot25();
  virtual void Slot26();
  virtual void Slot27();
  virtual void Slot28();
};

extern "C" {
// .text 0x0, 0x2C bytes. Vtable entry 0x3C of both of the module's vtables: CActor's
// `GetHealthInfo` slot, returning the result of the `HealthInfo` slot above it at 0x38. Retail's
// CActor spelling of the same function is `return const_cast<CActor*>(this)->HealthInfo();`; the
// const_cast is invisible in the bytes - r3 is already the address that gets passed on - so the
// record's pointer is passed straight through. These eleven instructions are `fn_71_0`'s byte for
// byte, so this line is the same one `CSnakeWeedSwarmRel.cpp` already measures.
CHealthInfo* fn_66_0(CScriptSafeZoneVTable* self) { return self->Slot12(); }
}