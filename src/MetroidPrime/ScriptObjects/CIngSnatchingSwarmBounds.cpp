// CIngSnatchingSwarmBounds.cpp - IngSnatchingSwarm's (module 33) render/pre-render run, .text
// 0x39B8..0x3AC0. Four functions, contiguous, sitting above the state-table run
// `CIngSnatchingSwarmAi.cpp` claims (0x35F8..0x383C).
//
// **Only the offsets are derived; the bodies are written as ordinary C++ over them.** Every offset
// below is read off `build/G2ME01/IngSnatchingSwarm/asm/auto_00_0000383C_text.s`, and the whole
// range is in the module's FORCEACTIVE list, so nothing here is a dead-stripping hazard.
//
// The range is claimed exactly and nothing else: everything outside it is left unclaimed, so dtk
// fills it from retail and the module's sha1 against `config/G2ME01/config.yml` still holds. The
// neighbours left retail are `fn_33_3970` (0x3970, 0x48) below and `fn_33_3AC0` (0x3AC0, 0x19C)
// above - the latter being the module's own `CScriptMsg` dispatcher, which needs the script-message
// table the tree does not model.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% while the module's hash breaks - only
// `tools/flip_test.sh` catches that.

#include "types.h"

#include "Kyoto/Particles/CParticleGen.hpp"
#include "MetroidPrime/CActor.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetaRender/CCubeRenderer.hpp"

// Byte 0x73C is a block of 1-bit flags, not a mask field: the read is a single `extrwi` extraction
// followed by a `beq`, which is the bitfield form. The names are the bit each field occupies in the
// byte's MSB-first order, so `x80` is the byte's 0x80 and the declaration order below is the byte
// order - the same layout `CIngSnatchingSwarmAi.cpp` writes out for its run below this one.
//
// **The read's encoding and the printed `extrwi` are not the same shift**, so a field's index has
// to be picked from the retail word rather than from the printed line: retail's is
// `54 00 F7 FF` = `rlwinm. r0,r0,30,31,31`, which is index **5**, i.e. the sixth field below -
// `x04`, after the byte's 0x80. See the bit map in
// `docs/goal-notes/progress-rel-body-ingsnatchingswarm-2.md`.
struct CIngSnatchingSwarmBoundsFlags {
  u8 x80 : 1;
  u8 x40 : 1;
  u8 x20 : 1;
  u8 x10 : 1;
  u8 x08 : 1;
  u8 x04 : 1;
  u8 x02 : 1;
  u8 x01 : 1;
};

// The object this run is written over, reduced to the members the four functions touch. Every
// offset is measured; the padding between members is the space the class's real body occupies and
// is deliberately written as `char x_padN[...]`, which `tools/check_raw_offsets.py` does not count
// as a raw offset. The run below (`CIngSnatchingSwarmAi.cpp`) reads the same object, and its
// members sit in the padding this one skips over, so the two files agree on one layout.
class CIngSnatchingSwarmBounds {
private:
  char x_pad0[0x1C0];

public:
  // The sub-object `fn_33_39B8` hands back, which retail's own constructor destroys as a
  // `CDamageVulnerability` (`__dt__20CDamageVulnerabilityFv` at 0x1C0). The function returns its
  // address rather than a value read out of it, so the class carries the range rather than the
  // object: `char` is enough, and nothing here reads through it.
  char x1C0[0x1F0 - 0x1C0];

private:
  char x_pad1[0x3E4 - 0x1F0];

public:
  CParticleGen* x3E4;

private:
  char x_pad2[0x3F4 - 0x3E8];

public:
  CParticleGen* x3F4;

private:
  char x_pad3[0x73C - 0x3F8];

public:
  CIngSnatchingSwarmBoundsFlags x73C;
};

// `tools/check_symbol_names.py` reads these names out of the object, so they have to be exactly
// what `config/G2ME01/rels/IngSnatchingSwarm/symbols.txt` calls them.
extern "C" {
// .text 0x3A60, 0x60 bytes - the 0x73C bit-5 flag, then `CActor::Render`, then
// `CParticleGen::Render()` on 0x3F4 and 0x3E4 in that order.
//
// Both calls are **qualified**: the imported names are `Render__6CActorCFRC13CStateManager` and
// `PreRender__6CActorFR13CStateManager`, neither with the `Fv` suffix a virtual call carries, so
// retail calls the base implementations directly rather than dispatching through a vtable. The two
// particle calls *are* virtual and *are* dispatch (`lwz r12,0(r3) ; lwz r12,0x10(r12)` - vtable
// slot 4, which is `CParticleGen::Render` in `Kyoto/Particles/CParticleGen.hpp`). `self` and `mgr`
// are already in r3 and r4 for all three, which is why retail saves only r31.
void fn_33_3A60(CIngSnatchingSwarmBounds* self, CStateManager& mgr) {
  if (self->x73C.x04) {
    CActor* actor = reinterpret_cast< CActor* >(self);
    actor->CActor::Render(mgr);
    self->x3F4->Render();
    self->x3E4->Render();
  }
}

// .text 0x3A34, 0x2C bytes - the same bit, then the qualified `CActor::PreRender`, with both
// arguments already in place so that retail saves no register at all.
void fn_33_3A34(CIngSnatchingSwarmBounds* self, CStateManager& mgr) {
  if (self->x73C.x04) {
    CActor* actor = reinterpret_cast< CActor* >(self);
    actor->CActor::PreRender(mgr);
  }
}

// .text 0x39C0, 0x74 bytes - `gpRender`'s vtable slot 0x44 on 0x3E4, then the same slot on 0x3F4.
// Slot 0x44 is index 17, which `MetaRender/CCubeRenderer.hpp` and `IRenderer.hpp` put
// `AddParticleGen(const CParticleGen&)` at - the same call `CParticleGenInfoGeneric.cpp:25` spells
// as `gpRender->AddParticleGen(*mSystem.GetPtr())`. Retail loads r4 straight out of the member, so
// the pointer is passed as the reference.
void fn_33_39C0(CIngSnatchingSwarmBounds* self) {
  if (self->x73C.x04) {
    gpRender->AddParticleGen(*self->x3E4);
    gpRender->AddParticleGen(*self->x3F4);
  }
}

// .text 0x39B8, 0x08 bytes - the address of the sub-object at +0x1C0, not a value read out of it,
// so the whole body is the one `addi r3,r3,0x1c0` and the array member decays to its own address.
char* fn_33_39B8(CIngSnatchingSwarmBounds* self) { return self->x1C0; }
}