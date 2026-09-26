// Temporary layout probe for CActor, run with mwcceppc's own flags.
//
// CActor.hpp carries a 32-bit bitfield group at 0x150 and CHECK_SIZEOF(CActor, 0x158), and
// CHECK_SIZEOF cannot decide anything about it: `check_sizeof<T,n>` passes for any n, so a group
// laid out one byte late is still "0x158". What decides `CActor::SetDirtyFlags` (retail 0x8004A0A0)
// is the byte MWCC actually accesses each field through - every retail one-bit access in this group
// is an `lbz`/`stb` on 0x150..0x153 (SetDirtyFlags 0x8004A0A0, SetMuted 0x8004B600, SetCallTouch
// 0x8004C514, SetUseInSortedLists 0x8004C53C), and `offsetof` on a bitfield is illegal in mwcceppc
// anyway.
//
// So the bitfields are measured the way they are compiled: one setter each, shaped like retail's
// `SetMuted(bool)`, and `tools/probe_cactor_offsets.py` reads the `lbz` displacement and the
// `rlwimi` operands straight out of the object. That is a measurement of the encoding, not of a
// layout number, which is the only thing that can tell us which declared field each of retail's
// four `rlwimi` belongs to.
//
// `private` is opened up so offsetof() and the setters can name the members directly; the host
// compiler must not be used for any of this (it is 64-bit, and `rstl::string` is 24 bytes there
// against retail's 0x10).
#define private public
#define protected public
#include "MetroidPrime/CActor.hpp"
#undef private
#undef protected

#include <stddef.h>

#define S(T) ((int)sizeof(T))
#define O(T, m) ((int)offsetof(T, m))

// Sizes and the non-bitfield offsets around the group. The bitfields cannot appear here - mwcceppc
// rejects offsetof on them - so the second half of this file measures those instead.
unsigned int g_probe[] = {
    S(CActor),                          // 0x158
    O(CActor, m_transform),             // 0x024
    O(CActor, m_position),              // 0x054
    O(CActor, m_modelData),             // 0x060
    O(CActor, m_material),              // 0x068
    O(CActor, x70_materialFilter),      // 0x070
    O(CActor, x88_sfxId),               // 0x088
    O(CActor, xbc_actorLights),         // 0x0BC
    O(CActor, otherBounds),             // 0x0CC
    O(CActor, m_renderBounds),          // 0x0E4
    O(CActor, xfc_drawFlags),           // 0x0FC
    O(CActor, xbc_time),                // 0x100
    O(CActor, xc4_fluidId),             // 0x108
    O(CActor, xc6_nextDrawNode),        // 0x10C
    O(CActor, xd4_maxVol),              // 0x11C
    O(CActor, xd8_nonLoopingSfxHandles),// 0x120
    O(CActor, x130_addedToken),         // 0x130
    O(CActor, actor_padding),           // 0x134
};

// mwcceppc puts `g_probe` in .data and the count in .sdata; the probe script reads both, because
// reading only .data and treating its tail as the count silently passes with garbage.
unsigned int g_n = sizeof(g_probe) / sizeof(g_probe[0]);

// One setter per bitfield, each exactly retail's shape (`SetMuted` is `lbz`/`rlwimi`/`stb` on a
// bool parameter), so the object carries MWCC's own choice of byte and merge range for every field.
// The names are what the probe script disassembles, so they must not be inlined away.
#define SETTER(name, member)                          \
  void probe_##name(CActor * p, bool b) { p->member = b; }

SETTER(nextNonLoopingSfxHandle, m_nextNonLoopingSfxHandle)
SETTER(notInSortedLists, m_notInSortedLists)
SETTER(transformDirty, m_transformDirty)
SETTER(actorLightsDirty, m_actorLightsDirty)
SETTER(renderBoundsDirty, m_renderBoundsDirty)
SETTER(outOfFrustum, m_outOfFrustum)
SETTER(calculateLighting, m_calculateLighting)
SETTER(shadowEnabled, m_shadowEnabled)
SETTER(shadowDirty, m_shadowDirty)
SETTER(muted, m_muted)
SETTER(useInSortedLists, m_useInSortedLists)
SETTER(unk, unk)
SETTER(callTouch, m_callTouch)
SETTER(globalTimeProvider, m_globalTimeProvider)
SETTER(renderUnsorted, m_renderUnsorted)
SETTER(pointGeneratorParticles, m_pointGeneratorParticles)
SETTER(renderParticleDBInside, m_renderParticleDBInside)
SETTER(enablePitchBend, m_enablePitchBend)
SETTER(visorFlags, m_targetableVisorFlags)
SETTER(enableRender, m_enableRender)
SETTER(worldLightingDirty, m_worldLightingDirty)
SETTER(drawEnabled, m_drawEnabled)
SETTER(doTargetDistanceTest, m_doTargetDistanceTest)
SETTER(fluidCounter, m_fluidCounter)
SETTER(targetable, m_targetable)
SETTER(sortedDrawCallback, x154_31_sortedDrawCallback)
