// Carved out of the unclaimed dtk `auto_*` range that sat between
// `Kyoto/Animation/DolphinCVirtualBone.cpp` (ends 0x80310E8C) and
// `Kyoto/Graphics/DolphinCModel.cpp` (starts 0x80310F38).  Every number here is
// measured: the addresses and sizes come from `symbols.txt`, the instructions are the ones
// dtk emitted into `build/G2ME01/asm/auto_03_80310E8C_text.s`, and the bodies below are the
// C those bytes are the compilation of.
//
// .text 0x80310E8C..0x80310F38, 0xAC = 172 bytes, 3 functions:
//
//   fn_80310E8C  0x80310E8C  0x7C  raise CModel's "textures touched" flag, then
//                                   UnlockTextures on every material set
//   fn_80310F08  0x80310F08  0xC   &CModel::mModelInstance->mBounds
//   fn_80310F14  0x80310F14  0x24  CModel::IsDefinitelyOpaque: no cube model,
//                                   or no alpha surfaces on it
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits
// function definitions in *reverse* source order, so an ascending file puts these three out
// of retail's order in the object.  `tools/check_decl_order.py` sees it immediately - it
// compares our object's symbols against retail's addresses by name - and it is worth running
// before the first build rather than after.
//
// **Measured, and it contradicts the usual warning.** Declared ascending the first time, the
// object came out permuted (`fn_80310F14` at `.text+0x0`) and
// `check_decl_order.py --unit` flagged it, yet `tools/dol_read.py 80310E8C 32` on the relinked
// `main.dol` returned retail's bytes and the sha1 was still
// `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.  So for *this* unit mwldeppc did not emit the
// object's `.text` verbatim.  That is not a licence to declare ascending: objdiff pairs by
// name, so a permutation is invisible to it and to `unit_fit.sh`, and one link is not proof
// about a linker.  Declared descending, the object emits `fn_80310E8C`, `fn_80310F08`,
// `fn_80310F14` at `+0x0`, `+0x7C`, `+0x88` and `check_decl_order.py` is clean.
//
// Retail names none of these three.  `symbols.txt` carries the `fn_<addr>` placeholder and
// this file reproduces that symbol verbatim, so the definitions have to stay C: a C++ one
// would mangle to `fn_80310E8CPv` and objdiff would pair nothing.  That is also why the unit
// is a `.c` rather than a `.cpp`.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section
// (dtk `dol split` fails with "Cyclic dependency ... link order").
//
// The directory is retail own, taken from the nearest claimed range: the gap ends 0xAC bytes
// before `Kyoto/Graphics/DolphinCModel.cpp`, so the code is that unit's neighbourhood.
//
// **Why it exists.** `CModelData::LockTextures` (0x800E4B20) and
// `CModelData::IsDefinitelyOpaque` (0x800E531C) both `bl` into this range, so neither could
// be written until the symbols existed: naming them as undefined externs added undefined
// symbols to the port link, and defining them in `MetroidPrime/CModelData.cpp` would add
// functions the retail object does not have.
//
// **The layout below is retail's, not this tree's.** `CModel`'s private members are at
// 0x1C / 0x24 / 0x28 / 0x32 in the 32-bit DOL layout that `include/Kyoto/Graphics/CModel.hpp`
// already models (`rstl::vector<SShader> mMatSets` at 0x18, so `mCount` 0x1C and `mItems`
// 0x24; `rstl::single_ptr<CCubeModel> mModelInstance` 0x28; the `x30_16_` /
// `mHasSkinMatrices` bitfield byte at 0x32).  The two bitfields are pinned by retail's own
// readers of that byte: `CModel::VerifyCurrentShader` (0x803115F8) tests it with
// `rlwinm. r0,r0,25,31,31` and `CModel::SetupSkinMatrices` (0x80311954) with
// `rlwinm. r0,r0,26,31,31`, i.e. bitfield index 0 and index 1 in that byte, which is
// `x30_16_` and `mHasSkinMatrices` in declaration order.
//
// 0x20 and 0x3C inside `CCubeModel` are `mBounds` and `mFirstSorted.mData`
// (`CHECK_SIZEOF(CCubeModel, 0x50)`, `include/Kyoto/Graphics/CCubeModel.hpp`: the seven
// pointers of `ModelInstance` end at 0x1C, `mTextures` takes 0x1C..0x1F, `mBounds` is a
// 0x18 `CAABox` at 0x20..0x37, `mFirstUnsorted` 0x38, `mFirstSorted` 0x3C).
// `fn_80310F14` asking whether the *sorted* (alpha) surface list is empty is what makes the
// name `IsDefinitelyOpaque` mean what it says.
//
// **Port.** `TARGET_PC` is not defined by `configure.py`, so the bodies below are the DOL's.
// On the 64-bit host none of those offsets hold - `mMatSets`/`mModelInstance` are not at
// 0x18/0x28 - and `CModelData::LockTextures` and `CModelData::IsDefinitelyOpaque` are both
// reachable from port code (`src/MetroidPrime/Player/CGrappleArm.cpp`,
// `src/MetroidPrime/CActor.cpp`), so the host arms below stand in and do nothing.  That is
// what those two `CModelData` methods already did, so the port's behaviour is unchanged;
// the stand-in is named rather than plausible because the real answers need a `CCubeModel`
// on the host, which is upstream work.

/* `rstl::vector<SShader>`'s element: retail's `CModel::SShader` is 0x20 = 32 bytes
 * (`rstl::vector` 0x10 + `mData` + `mOwner` + `mPrev` + `mNext`, four pointers). */
typedef struct {
  char x00[0x20];
} SCarveShader;

/* Retail's layout of the CModel words these functions read; see the note above. */
typedef struct {
  char x00[0x1C];
  int mMatSetCount;      /* 0x1C - rstl::vector<SShader>::mCount */
  char x20[4];
  SCarveShader* mMatSetItems; /* 0x24 - rstl::vector<SShader>::mItems */
  void* mModelInstance;  /* 0x28 - rstl::single_ptr<CCubeModel> */
  char x2C[4];
  char x30[2];
  unsigned char x30_16_ : 1;         /* 0x32, bitfield index 0 */
  unsigned char mHasSkinMatrices : 1; /* 0x32, bitfield index 1 */
  unsigned char x32_2 : 6;
} SCarveModel;

/* The two CCubeModel words fn_80310F08 and fn_80310F14 reach; see the note above. */
typedef struct {
  char x00[0x20];
  char mBounds[0x1C];               /* 0x20 - CAABox */
  void* mSortedSurfaceData;         /* 0x3C - CCubeModel::mFirstSorted.mData */
} SCarveCubeModel;

/* retail's own mangled name, from `nm build/G2ME01/main.elf`; mwcceppc does not mangle C. */
extern void UnlockTextures__Q26CModel7SShaderFv(void* self);

#ifndef TARGET_PC

int fn_80310F14(void* self) {
  SCarveModel* model = (SCarveModel*)self;
  int ret = 0;
  if (model->mModelInstance != 0) {
    if (((SCarveCubeModel*)model->mModelInstance)->mSortedSurfaceData == 0) {
      ret = 1;
    }
  }
  return ret;
}

void* fn_80310F08(void* self) {
  SCarveModel* model = (SCarveModel*)self;
  return ((SCarveCubeModel*)model->mModelInstance)->mBounds;
}

void fn_80310E8C(void* self) {
  SCarveModel* model = (SCarveModel*)self;
  int i;
  if (model->x30_16_) {
    return;
  }
  model->x30_16_ = 1;
  /* The count and the item pointer are reloaded from `self` on every iteration, which is
   * what retail does: both are `mMatSets`'s members and neither is hoisted. */
  for (i = 0; i < model->mMatSetCount; i++) {
    UnlockTextures__Q26CModel7SShaderFv(&model->mMatSetItems[i]);
  }
}

#else

/* Port stand-ins - see "Port" above.  Named, not plausible. */
int fn_80310F14(void* self) {
  (void)self;
  return 0;
}
void* fn_80310F08(void* self) {
  (void)self;
  return 0;
}
void fn_80310E8C(void* self) { (void)self; }

#endif