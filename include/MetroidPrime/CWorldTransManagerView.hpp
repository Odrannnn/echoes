#ifndef _CWORLDTRANSMANAGERVIEW
#define _CWORLDTRANSMANAGERVIEW

#include "types.h"

#include "rstl/string.hpp"

#include "Kyoto/CRandom16.hpp"
#include "Kyoto/Math/CTransform4f.hpp"

/**
 * The per-world model-data object `CWorldTransManagerView::Update` walks: a scale at +0x08, six
 * `CModelData` at +0x1C on a 0x4C stride, and five 0xC-byte slots from +0x1E4. Not modelled
 * yet - `Update` reads only whether `x4_modelData` is null, and modelling the rest of the class
 * is the same job `docs/research/raw_offsets.md` calls out for `CFrontendDataNetwork`.
 */
struct SWorldModelData;

class CWorldTransManagerView;

// Declared at namespace scope and with C linkage, and *then* friended below, because
// mwcceppc rejects `friend extern "C"` (it reads the `extern` as a storage class) and GCC
// rejects a friend declaration whose linkage does not match the definition. Same arrangement as
// `CResLoader.hpp:48-51`. Without the namespace-scope declaration mwcceppc mangles the definition
// to `fn_8015C34C__FP11CWorldState` and objdiff has nothing in the retail object to pair it with.
extern "C" void fn_8015C34C(CWorldTransManagerView*);

/**
 * The 1200 (0x4B0) byte object `CGameState` owns in an `rstl::rc_ptr` at +0x3C, and whose
 * `Update()` runs every frame from `CGameArchitectureSupport::Update` before the frame-end
 * message is queued.
 *
 * What is established, and how:
 *  - **Size 0x4B0.** `CGameState`'s constructor (retail 0x80144140) does `li r3,1200` /
 *    `__nw__FUlPCcPCc` and stores the result at +0x3C, and 1200 is 0x4B0. The object's own
 *    constructor is retail 0x8015C34C (0x114 bytes), and every member that constructor touches
 *    is named below with the instruction that fixes its offset and width.
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
 *
 * **Superseded 2026-09-28 (upstream sync): this is upstream's `CWorldTransManager`.** Upstream's
 * `CGameState` has `rstl::rc_ptr<CWorldTransManager> mTransManager` at +0x3C, and the
 * constructor call `CGameStateCtor.cpp` declares is `__ct__18CWorldTransManagerFv`. The class
 * was called `CWorldState` until upstream added the real `CWorldState`
 * (`MetroidPrime/Player/CWorldState.hpp`) under the same include guard, which made whichever
 * header came first silently win. It is a port-side *view* of the manager's measured offsets
 * until it is folded into `MetroidPrime/Player/CWorldTransManager.hpp`.
 */
class CWorldTransManagerView {
public:
  void Update();

  // Plain friend declarations, with no `extern "C"` on them: mwcceppc rejects `friend extern "C"`
  // (it reads the `extern` as a storage class) and GCC rejects a friend declaration whose
  // linkage does not match the definition. `CResLoader.hpp:48-49` records the same.
  friend void fn_8015C34C(CWorldTransManagerView*);

private:
  // 0x8015C34C, and the offsets below are read off it instruction by instruction. The padding
  // is retail's too: the constructor stores nothing in it, and the fields it does store are in
  // ascending offset order, which is what the order of the initialisers in
  // `src/MetroidPrime/CWorldStateCtor.cpp` reproduces.
  float x0_scale;              //!< +0x00 `stfs f0,0(r3)`  - retail's `lbl_8041C398`, 1.0f
  SWorldModelData* x4_modelData; //!< +0x04 `stw r0,4(r31)`
  uint x8_unk;                 //!< +0x08 `stw r0,8(r31)`
  uint xc_unk;                 //!< +0x0C `stw r0,12(r31)`
  char x10_pad[0x8];           //!< +0x10
  bool x18_flag;               //!< +0x18 `stb r0,24(r31)`
  char x19_pad[0x73];          //!< +0x19 .. +0x8B
  bool x8c_flag;               //!< +0x8C `stb r0,140(r31)`
  char x8d_pad[0x13];          //!< +0x8D .. +0x9F
  CRandom16 xa0_random;        //!< +0xA0, 0x4, `addi r3,r31,160` -> `__ct__9CRandom16FUi` (99)
  char xa4_pad[0x8];           //!< +0xA4 .. +0xAB
  bool xac_flag;               //!< +0xAC `stb r7,172(r31)`
  char xad_pad[0x3];           //!< +0xAD .. +0xAF
  s16 xb0_seed;                //!< +0xB0 `sth r0,176(r31)`, 9611
  char xb2_pad[0x2];           //!< +0xB2
  uint xb4_unk;                //!< +0xB4 `stw r7,180(r31)`
  u8 xb8_max;                  //!< +0xB8 `stb r6,184(r31)`, 127
  u8 xb9_min;                  //!< +0xB9 `stb r5,185(r31)`, 64
  char xba_pad[0x2];           //!< +0xBA
  uint xbc_unk;                //!< +0xBC `stw r7,188(r31)`
  char xc0_pad[0x4];           //!< +0xC0
  float xc4_scale;             //!< +0xC4 `stfs f0,196(r31)`
  char xc8_pad[0x10];          //!< +0xC8 .. +0xD7
  /**
   * +0xD8, 0x10 bytes: `stw r0,216(r31)` stores `addi r0,r13,-25240` = 0x80419AE8 =
   * `rstl::basic_string<char>::mNull`, and the two `stw r7,220/224(r31)` that follow are that
   * class's `x4_cow = nullptr` and `x8_size = 0` - **retail's `basic_string()` constructor,
   * inlined**, which is why it is written out by hand in the constructor rather than left to a
   * member initialiser. `SetEmpty()` is the reason that is possible; see `rstl/string.hpp`.
   */
  rstl::string xd8_name;
  char xe8_pad[0x4];           //!< +0xE8
  float xec_f0;                //!< +0xEC `stfs f0,236(r31)`
  float xf0_f0;                //!< +0xF0 `stfs f0,240(r31)`
  float xf4_f0;                //!< +0xF4 `stfs f0,244(r31)`
  char xf8_pad[0x1B4];         //!< +0xF8 .. +0x2AB
  bool x2ac_flag;              //!< +0x2AC `stb r7,684(r31)`
  char x2ad_pad[0x1B7];        //!< +0x2AD .. +0x463
  bool x464_flag;              //!< +0x464 `stb r7,1124(r31)`
  char x465_pad[0x3];          //!< +0x465
  CTransform4f x468_transform; //!< +0x468, 0x30 - `addi r3,r31,1128` -> the copy ctor of sIdentity
  void* x498_token;            //!< +0x498
  char x49c_pad[0x8];          //!< +0x49C .. +0x4A3
  bool x4a4_flag;              //!< +0x4A4 `stb r5,1188(r31)`
  char x4a5_pad[0x3];          //!< +0x4A5
  void* x4a8_token;            //!< +0x4A8 `stw r5,1192(r31)`
  //!< +0x4AC, two bytes of `bool : 1`, of which the constructor sets six. Retail emits one
  //!< `lbz`/`rlwimi`/`stb` per field and each mask is a single bit: +0x4AC bits 0,1,2,3,4 and
  //!< +0x4AD bit 0, set in that order except that bit 4 is written before bit 3.
  struct SFlags {
    bool b0 : 1;
    bool b1 : 1;
    bool b2 : 1;
    bool b3 : 1;
    bool b4 : 1;
    bool b5 : 1;
    bool b6 : 1;
    bool b7 : 1;
    bool b8 : 1;
  } x4ac_flags;
};
CHECK_SIZEOF(CWorldTransManagerView, 0x4b0)

#endif // _CWORLDTRANSMANAGERVIEW
