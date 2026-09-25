#ifndef _CWORLDSTATE
#define _CWORLDSTATE

#include "types.h"

/**
 * The per-world model-data object `CWorldState::Update` walks: a scale at +0x08, six
 * `CModelData` at +0x1C on a 0x4C stride, and five 0xC-byte slots from +0x1E4. Not modelled
 * yet - `Update` reads only whether `x4_modelData` is null, and modelling the rest of the class
 * is the same job `docs/research/raw_offsets.md` calls out for `CFrontendDataNetwork`.
 */
struct SWorldModelData;

/**
 * The 1200 (0x4B0) byte object `CGameState` owns in an `rstl::rc_ptr` at +0x3C, and whose
 * `Update()` runs every frame from `CGameArchitectureSupport::Update` before the frame-end
 * message is queued.
 *
 * What is established, and how:
 *  - **Size 0x4B0.** `CGameState`'s constructor (retail 0x80144140) does `li r3,1200` /
 *    `__nw__FUlPCcPCc` and stores the result at +0x3C, and 1200 is 0x4B0. The object's own
 *    constructor is retail 0x8015C34C (0x114 bytes), which writes +0x00, +0x04, +0x08, +0x0C,
 *    +0x18, +0x8C, constructs a `CRandom16` (seed 99) at +0xA0, sets +0xAC, +0xB0 = 9611,
 *    +0xB4, +0xB8 = 127, +0xB9 = 64, +0xBC, +0xC4, +0xD8, +0xDC, +0xE0, +0xEC, +0xF0, +0xF4,
 *    +0x2AC, +0x464, copy-constructs a `CTransform4f` at +0x468 and clears +0x4A4, +0x4A8,
 *    +0x4AC, +0x4AD.
 *  - **Ownership.** The constructor allocates the object and then separately allocates a 4-byte
 *    word initialised to 1 and stores it at +0x40 - retail's `rstl::rc_ptr`, whose data pointer
 *    is at +0x3C. `fn_80142520` (retail 0x80142520, 8 bytes: `addi r3,r3,60; blr`) is
 *    `CGameState::GetWorldState()` returning the address of that rc_ptr, and every caller
 *    dereferences it once (`lwz r3,0(r3)`) before calling a method.
 *  - **The two members `Update()` reads** are modelled below: the per-world model-data object at
 *    +0x04, and the object +0x4A8 re-releases each frame. The rest is padding, because nothing
 *    written here reaches it.
 *
 * **The name is inferred, not read out of the binary** - retail ships no string for the class,
 * and 0x80142188-0x8015CD4C is an unclaimed `.text` gap in `config/G2ME01/splits.txt` holding
 * several translation units, so even the constructor's own file cannot be established from the
 * tree. What the object does is per-world render state: a scale, a set of `CModelData`, a
 * `CTransform4f`, a seeded `CRandom16` ranged 64..127, and a token it re-releases each frame.
 * Renaming it is a one-line change once someone can show which file owns 0x8015C34C.
 */
class CWorldState {
public:
  void Update();

private:
  float x0_unk;
  SWorldModelData* x4_modelData;
  char x8_pad[0x490];
  void* x498_token;
  char x49c_pad[0xc];
  void* x4a8_token;
  char x4ac_pad[0x4];
};
CHECK_SIZEOF(CWorldState, 0x4b0)

#endif // _CWORLDSTATE
