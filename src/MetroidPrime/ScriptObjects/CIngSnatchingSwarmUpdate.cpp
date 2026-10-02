// CIngSnatchingSwarmUpdate.cpp - IngSnatchingSwarm's (module 33) state entry point,
// .text 0x348C..0x35F8. One function, `fn_33_348C` (0x16C), sitting directly **below** the
// state-table run `CIngSnatchingSwarmAi.cpp` claims (0x35F8..0x383C).
//
// **The module's own `.data` names it.** `build/G2ME01/IngSnatchingSwarm/asm/auto_04_00000000_data.s`
// holds a run of 12-byte records `.4byte 0, 0xFFFFFFFF, <fn>`, and the one at `.data:0x10C` holds
// `fn_33_348C` - the record right after the one at `0x100` that holds `fn_33_35F8`, the first
// function `CIngSnatchingSwarmAi.cpp` claims. So this is a table entry of the same dispatch as
// that run, which is why its third argument is a small integer state and it ends by sending
// `kSS_Dead`.
//
// The claim is the whole run and nothing else: everything outside 0x348C..0x35F8 is left
// unclaimed, so dtk fills it from retail and the module's sha1 against `config/G2ME01/config.yml`
// still holds. The neighbours left retail are the rest of `auto_00_000000A8_text` below (the class
// body, which needs the `CActor` constructor chain at `fn_33_41CC`) and `auto_00_0000383C_text`
// above. Every offset below is read off that run's disassembly, which dtk renames to
// `auto_00_00000000_text.s` once this range becomes a unit of its own.
//
// Like the other three class-body units in this module, the object is a stand-in class carrying
// the measured offsets and the bodies are ordinary C++ over them - **only the offsets are
// derived.**
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% while the module's hash breaks - only
// `tools/flip_test.sh` catches that.

#include "types.h"

#include "Kyoto/Particles/CParticleGen.hpp"
#include "MetroidPrime/CActor.hpp"
#include "MetroidPrime/CEntity.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/TGameTypes.hpp"

// `.rodata:0x34` is `.float 0` (`build/G2ME01/IngSnatchingSwarm/asm/auto_03_00000000_rodata.s`)
// and is the argument of both `CParticleGen` rate calls. This unit claims `.text` only, so the
// reference has to be to the symbol the unclaimed `.rodata` object defines and not to a literal of
// our own - a literal would put a `.rodata` section in this object and move the module.
//
// **The host branch only declares it.** `CIngSnatchingSwarmAi.cpp` already defines
// `lbl_33_rodata_34` for the host and both files are in `files.cmake`, so defining it a second
// time here is a duplicate symbol in the port's flat link - measured: `link_check.sh` reported
// `DUP lbl_33_rodata_34` until this became a declaration.
#ifdef __MWERKS__
extern "C" {
extern const float lbl_33_rodata_34;

// `fn_33_92C` (0x492C, 0x4C) is this module's own function and is **not** claimed here - it is
// inside the unclaimed 0x3AC0..0x4F28 run, so dtk links it from retail and the call below becomes
// an import.
void fn_33_92C(void* self);
}
#else
extern "C" const float lbl_33_rodata_34;
extern "C" void fn_33_92C(void* self) { (void)self; }
#endif

// Byte 0x73C is a block of 1-bit flags, not a mask field: the stores are `lbz`+`rlwimi`+`stb`,
// which is the bitfield form. The names are the bit each field occupies in the byte's MSB-first
// order, so `x80` is the byte's 0x80 and the declaration order below is the byte order - the same
// layout `CIngSnatchingSwarmAi.cpp` and `CIngSnatchingSwarmBounds.cpp` write out for their runs.
// **The store's encoding and the read's are not the same shift**, so the two fields cleared here
// are read off the retail words (`rlwimi ...,7,24,24` and `rlwimi ...,6,25,25`), not off the names.
struct CIngSnatchingSwarmUpdateFlags {
  u8 x80 : 1;
  u8 x40 : 1;
  u8 x20 : 1;
  u8 x10 : 1;
  u8 x08 : 1;
  u8 x04 : 1;
  u8 x02 : 1;
  u8 x01 : 1;
};

// The two `CParticleGen`s this function drives, as the **vtable** rather than as the class.
// Retail's call is `lwz r12,0(r3)` / `lwz r12,0x34(r12)` on 0x3E4 and 0x3F4, and that reproduces
// byte for byte with `CParticleGen.hpp`'s own declaration order read at **+8**: slot 0x34 is index
// 11, `SetGeneratorRate(float)` - which is why the rate arrives in f1 - and slot 0x74 is index 27,
// `GetParticleCount()`, whose result is only compared against zero. Those two slots are the only
// ones this function names, but the table has to be written out to index 27 to put them there.
//
// **Every entry is pure, and that is load-bearing.** `CParticleGen.hpp` gives `SetGeneratorRate` a
// body (`{}`), and mwcceppc emits an out-of-line **weak** copy of an inline virtual into every
// object that mentions it: with the real class in scope this unit's object grew a four-byte
// `SetGeneratorRate__12CParticleGenFf` that the 0x348C..0x35F8 claim has no room for, and
// `tools/unit_fit.sh` reported it as `extra: +4`. Making the real header's entry pure would fix
// that but is not this item's to do - `CScriptEffect.cpp` and `CHUDBillboardEffect.cpp` call it
// non-virtually, so it would turn into a link gap. The slot numbers are unchanged by writing the
// table out here, and both were measured: the real header's spelling emits exactly retail's 0x34
// and 0x74, and this table emits exactly the same two displacements.
class CIngSnatchingSwarmParticleGen {
public:
  virtual ~CIngSnatchingSwarmParticleGen() = 0;
  virtual const bool Update(double) = 0;
  virtual void Render() = 0;
  virtual void SetOrientation(const CTransform4f&) = 0;
  virtual void SetTranslation(const CVector3f&) = 0;
  virtual void SetGlobalOrientation(const CTransform4f&) = 0;
  virtual void SetGlobalTranslation(const CVector3f&) = 0;
  virtual void SetGlobalScale(const CVector3f&) = 0;
  virtual void SetLocalScale(const CVector3f&) = 0;
  virtual void SetParticleEmission(bool) = 0;
  virtual void SetModulationColor(const CColor&) = 0;
  // vtable slot 0x34, index 11.
  virtual void SetGeneratorRate(float) = 0;
  virtual void SetDrawFlags(uint) = 0;
  virtual const CTransform4f& GetOrientation() const = 0;
  virtual const CVector3f& GetTranslation() const = 0;
  virtual const CTransform4f& GetGlobalOrientation() const = 0;
  virtual const CVector3f& GetGlobalTranslation() const = 0;
  virtual const CVector3f& GetGlobalScale() const = 0;
  virtual bool GetParticleEmission() const = 0;
  virtual const CColor& GetModulationColor() const = 0;
  virtual float GetGeneratorRate() const = 0;
  virtual int GetEmitterTime() const = 0;
  virtual uint GetDrawFlags() const = 0;
  virtual bool ShouldDraw() const = 0;
  virtual int GetSystemCount() = 0;
  virtual bool IsSystemDeletable() = 0;
  virtual int GetBounds() = 0;
  // vtable slot 0x74, index 27.
  virtual int GetParticleCount() const = 0;
};

// The object, reduced to the members this function touches: the 2-byte id at 0x8, the two
// particle generators at 0x3E4 and 0x3F4 (the same slots `CIngSnatchingSwarmBounds.cpp` drives
// `Render()` on) and the flag byte at 0x73C.
class CIngSnatchingSwarmUpdate {
private:
  char x_pad0[8];

public:
  TUniqueId x8;

private:
  char x_pad1[0x3E4 - 0xA];

public:
  CIngSnatchingSwarmParticleGen* x3E4;

private:
  char x_pad2[0x3F4 - 0x3E8];

public:
  CIngSnatchingSwarmParticleGen* x3F4;

private:
  char x_pad3[0x73C - 0x3F8];

public:
  CIngSnatchingSwarmUpdateFlags x73C;
};

// `tools/check_symbol_names.py` reads these names out of the object, so they have to be exactly
// what `config/G2ME01/rels/IngSnatchingSwarm/symbols.txt` calls them.
extern "C" {
// .text 0x348C, 0x16C bytes - the module's own state-table entry, dispatching on a small integer
// state. `0` clears the two top flag bits, clears the module's private state, drops the object's
// target material and stops both particle generators; `1` stops both generators and, once neither
// has any particles left, deletes the object and tells everyone it died. Anything else does
// nothing.
//
// The comparison tree is a `switch`: `cmpwi r5,1` / `beq` / `bge` then `cmpwi r5,0` / `bge` / `b`
// is the two-case lowering, and the `case 0` block is laid out before the `case 1` block.
//
// `0x28` is `kMT_Target` (the 41st entry of `Collision/CMaterialList.hpp`'s `EMaterialTypes`, value
// 40) and `0x44454144` is `kSS_Dead`; `kSM_None` is `0xFFFFFFFF`, the `-1` in r7.
void fn_33_348C(CIngSnatchingSwarmUpdate* self, CStateManager& mgr, int state) {
  switch (state) {
  case 0:
    self->x73C.x80 = 0;
    self->x73C.x40 = 0;
    fn_33_92C(self);
    reinterpret_cast< CActor* >(self)->RemoveMaterial(kMT_Target, mgr);
    self->x3E4->SetGeneratorRate(lbl_33_rodata_34);
    self->x3F4->SetGeneratorRate(lbl_33_rodata_34);
    break;
  case 1:
    self->x3E4->SetGeneratorRate(lbl_33_rodata_34);
    self->x3F4->SetGeneratorRate(lbl_33_rodata_34);
    if (self->x3E4->GetParticleCount() == 0 && self->x3F4->GetParticleCount() == 0) {
      mgr.DeleteObjectRequest(TUniqueId(self->x8));
      reinterpret_cast< CEntity* >(self)->SendScriptMsgs(
          kSS_Dead, mgr, kInvalidUniqueId, kSM_None);
    }
    break;
  }
}
}