// Temporary layout probe for CCharacterInfo / CAnimData. Sizes and member offsets are written
// into .data so they can be read back with `objdump -s`; the host compiler cannot be used for
// this because rstl::string and friends are 64-bit there and 32-bit under mwcceppc.
// `#define private public` around the includes is what lets offsetof reach a private member, the
// same trick tools/probe_offsets.cpp uses.
//
//   tools/probe_cc.sh tools/cchar_probe.cpp /tmp/cchar_probe.o
//   build/binutils/powerpc-eabi-objdump -s -j .data /tmp/cchar_probe.o
//
// The expected values are duplicated in the RETAIL table at the bottom on purpose: a header and a
// probe edited together cannot catch anything.
#define private public
#define protected public
#include "MetroidPrime/CAnimData.hpp"
#include "Kyoto/Animation/CCharacterInfo.hpp"
#undef protected
#undef private

typedef CCharacterInfo T;
typedef CCharacterInfo::CParticleResData P;

#define O(cls, m) (unsigned int) offsetof(cls, m)
#define POFF(m) O(P, m)

unsigned int cchar_offsets[] = {
  O(T, x0_tableCount),  O(T, x4_name),      O(T, x14_cmdl),         O(T, x18_cksr),
  O(T, x1c_cinf),       O(T, x20_animInfo), O(T, x30_pasDatabase),  O(T, x44_partRes),
  O(T, xa4_unk),        O(T, xa8_aabbs),    O(T, xb8_effects),      O(T, xc8_cmdlOverlay),
  O(T, xcc_cksrOverlay), O(T, xd0_animIdxs), O(T, xe0_unk),         O(T, xe4_unk),
  O(T, xe5_unk),        O(T, xe8_unk),      sizeof(T),
};

unsigned int cchar_part_offsets[] = {
  POFF(x0_part), POFF(x10_swhc), POFF(x20_elsc),
  POFF(x30_unk), POFF(x40_unk), POFF(x50_unk),
  sizeof(P),
};

unsigned int cchar_element_sizes[] = {
  sizeof(rstl::pair< int, rstl::string >),
  sizeof(rstl::pair< rstl::string, CAABox >),
  sizeof(rstl::pair< rstl::string, rstl::vector< CEffectComponent > >),
  sizeof(CAABox),
  sizeof(CPASDatabase),
  sizeof(CAssetId),
  sizeof(rstl::vector< CAssetId >),
  sizeof(CAnimIdEntry),
  sizeof(CAnimData),
  sizeof(CParticleDatabase),
};

// ## RETAIL (G2ME01). Every line is the instruction in the table in
// ## include/Kyoto/Animation/CCharacterInfo.hpp that fixes it.
unsigned int cchar_retail_offsets[] = {
  0x000, 0x004, 0x014, 0x018, 0x01c, 0x020, 0x030, 0x044, 0x0a4, 0x0a8,
  0x0b8, 0x0c8, 0x0cc, 0x0d0, 0x0e0, 0x0e4, 0x0e5, 0x0e8, 0x0f8,
};
unsigned int cchar_retail_part_offsets[] = { 0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 };
unsigned int cchar_retail_sizes[] = {
  0x14,            // pair<int, string>
  0x28,            // pair<string, CAABox>
  0x20,            // pair<string, vector<CEffectComponent>>
  0x18,            // CAABox
  0x14,            // CPASDatabase
  0x04,            // CAssetId
  0x10,            // vector<CAssetId>
  0x1c,            // CAnimIdEntry
  0x5b8,           // CAnimData  (li r3,1464 @0x80030184; stfs f0,1460(r25) @0x8002D598)
  0xe0,            // CParticleDatabase
};
