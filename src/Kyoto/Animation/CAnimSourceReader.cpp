#include "Kyoto/Animation/CAnimSourceReader.hpp"

#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/Animation/CCharAnimTime.hpp"
#include "Kyoto/Animation/CSegId.hpp"
#include "Kyoto/Particles/CParticleData.hpp"
#include "Kyoto/Math/CMatrix3f.hpp"
#include "Kyoto/Math/CQuaternion.hpp"
#include "Kyoto/Math/CVector3f.hpp"

CAnimSourceReader::CAnimSourceReader(const TSubAnimTypeToken< CAnimSource >& source,
                                     const CCharAnimTime& time, const CAnimPOIData* poiData)
: CAnimSourceReaderBase(rs_new CAnimSourceInfo(source), poiData)
, mSource(source)
, mSteadyStateInfo(mSource->GetSteadyStateAnimInfo(time)) {
  PostConstruct(time);
}

SAdvancementResults CAnimSourceReader::VAdvanceView(const CCharAnimTime& time) {
  const CCharAnimTime previousTime = mCurTime;
  const CCharAnimTime& duration = mSource->GetAnimationDuration();
  if (previousTime == duration) {
    mCurTime = CCharAnimTime::ZeroFlat();
    mPassedBoolCount = 0;
    mPassedIntCount = 0;
    mPassedParticleCount = 0;
    mPassedSoundCount = 0;
    return SAdvancementResults(
        time, SAdvancementDeltas(CVector3f::Zero(), CQuaternion::NoRotation()));
  }
  if (time.EqualsZero()) {
    return SAdvancementResults(
        CCharAnimTime::ZeroFlat(), SAdvancementDeltas(CVector3f::Zero(), CQuaternion::NoRotation()));
  }

  mCurTime += time;
  CCharAnimTime remainingTime = CCharAnimTime::ZeroFlat();
  if (mCurTime > duration) {
    remainingTime = mCurTime - duration;
    mCurTime = duration;
  }
  UpdatePOIStates();

  const CSegId root(0);
  const CQuaternion oldRotation = mSource->GetRotation(root, previousTime);
  const CQuaternion newRotation = mSource->GetRotation(root, mCurTime);
  const CQuaternion inverseOldRotation = oldRotation.BuildInverted();
  CVector3f offset(0.f, 0.f, 0.f);
  if (mSource->HasOffset(root)) {
    const CVector3f oldOffset = mSource->GetOffset(root, previousTime);
    const CVector3f newOffset = mSource->GetOffset(root, mCurTime);
    offset = newOffset - oldOffset;
    const CQuaternion inverseNewRotation = newRotation.BuildInverted();
    const CMatrix3f inverseRotation = inverseNewRotation.BuildTransform();
    offset = inverseRotation * offset;
  }
  return SAdvancementResults(remainingTime,
                             SAdvancementDeltas(offset, newRotation * inverseOldRotation));
}

CCharAnimTime CAnimSourceReader::VGetTimeRemaining() const {
  return mSource->GetAnimationDuration() - mCurTime;
}

CSteadyStateAnimInfo CAnimSourceReader::VGetSteadyStateAnimInfo() const { return mSteadyStateInfo; }

bool CAnimSourceReader::VHasOffset(const CSegId& seg) const { return mSource->HasOffset(seg); }

CVector3f CAnimSourceReader::VGetOffset(const CSegId& seg) const {
  return mSource->GetOffset(seg, mCurTime);
}

CVector3f CAnimSourceReader::VGetOffset(const CSegId& seg, const CCharAnimTime& time) const {
  return mSource->GetOffset(seg, time);
}

CQuaternion CAnimSourceReader::VGetRotation(const CSegId& seg) const {
  return mSource->GetRotation(seg, mCurTime);
}

void CAnimSourceReader::VGetSegStatementSet(const CSegIdList& list, CSegStatementSet& set) const {
  mSource->GetSegStatementSet(list, set, mCurTime);
}

void CAnimSourceReader::VGetSegStatementSet(const CSegIdList& list, CSegStatementSet& set,
                                            const CCharAnimTime& time) const {
  mSource->GetSegStatementSet(list, set, time);
}

void CAnimSourceReader::VGetSegData(const CCharLayoutInfo& layout, CJointData_LinearStorage& data,
                                    const CCharAnimTime& time) const {
  mSource->GetSegData(layout, data, time);
}

void CAnimSourceReader::VGetSegData(const CCharLayoutInfo& layout,
                                    CJointData_LinearStorage& data) const {
  mSource->GetSegData(layout, data, mCurTime);
}

/**
 * The four `rstl::vector` special members retail's symbol table could not name, in **descending
 * retail offset** - mwcceppc emits definitions in reverse source order, so this is the order the
 * unit's `.text` has them in (0x802A3B80, 0x802A3AD0, 0x802A2E68, 0x802A2E14), and
 * `tools/check_decl_order.py` is what enforces it. They are all `rstl::vector` code for the three
 * members `CAnimSourceReaderBase` holds, and all four are `extern "C"` under retail's name for
 * the reason `fn_802A2E14` below gives.
 */

/**
 * `fn_802A3B80` - retail `.text:0x802A3B80`, 0x148 = 328 bytes, 0x802A3B80..0x802A3CC8 (the next
 * symbol). The copy constructor of `CAnimSourceReaderBase::mBoolStates`, called from
 * `CAnimSourceReaderBase`'s cloning constructor at 0x802A3A18 as
 * `addi r3,r29,40 ; mr r4,r11 ; bl 0x802A3B80`. Same body as `fn_802A3AD0` below and for the same
 * reason, with one difference: the element copy is unrolled eight times, so this one is 0x148
 * bytes where `fn_802A3AD0` is 0xB0.
 *
 *     802a3bec  srwi. r0,r3,3      ; mCount / 8, the unrolled count
 *     802a3bf0  mtctr r0 / beq 0x802a3c8c
 *     802a3bf8  lwz/stw 0 / lbz/stb 4  x 8 ; then r5 += 64, r4 += 64, bdnz
 *     802a3c84  andi. r3,r3,7 / beq 0x802a3cac  ; mCount % 8
 *     802a3c90  lwz/stw 0 / lbz/stb 4  ; the remainder, one element at a time
 *
 * An 8-byte `pair<uint, bool>` copied by assignment (`lwz`/`stw` for the `uint`, `lbz`/`stb` for
 * the `bool`), which is what the `construct_impl` added for this pair in
 * `include/rstl/pair.hpp` buys; a placement new of `pair`'s copy constructor emits a call and gets
 * neither the byte stores nor the unroll.
 */
extern "C" rstl::vector< rstl::pair< uint, bool > >*
    fn_802A3B80(rstl::vector< rstl::pair< uint, bool > >* self,
                const rstl::vector< rstl::pair< uint, bool > >& other) {
  self->mAllocator = other.mAllocator;
  self->mCount = other.mCount;
  self->mCapacity = other.mCapacity;
  if (other.mCount == 0 && other.mCapacity == 0) {
    self->mItems = nullptr;
  } else {
    self->mAllocator.allocate(self->mItems, self->mCapacity);
    rstl::uninitialized_copy_n(other.mItems, self->mCount, self->mItems);
  }
  return self;
}

/**
 * `fn_802A3AD0` - retail `.text:0x802A3AD0`, 0xB0 = 176 bytes, 0x802A3AD0..0x802A3B80 (the next
 * symbol). The copy constructor of the same `mParticleStates` vector, called from
 * `CAnimSourceReaderBase`'s cloning constructor at 0x802A3A18 as
 * `addi r3,r29,72 ; mr r4,r31 ; bl 0x802A3AD0`. It is `rstl::vector`'s copy constructor body:
 *
 *     802a3adc  lwz  r0,8(r4)      ; other.mCapacity
 *     802a3af0  lwz  r3,4(r4)      ; other.mCount
 *     802a3af4  stw  r3,4(r30)     ; mCount   = other.mCount
 *     802a3afc  stw  r0,8(r30)     ; mCapacity= other.mCapacity
 *     802a3af8  cmpwi r3,0 / 802a3b04 cmpwi r0,0 ; both zero?
 *     802a3b10  stw  r0,12(r30)     ; mItems = 0
 *     802a3b1c  slwi r3,r0,3       ; mCapacity * 8
 *     802a3b20  bl   0x802fdab8    ; rmemory_allocator::allocate
 *     802a3b34  mtctr r0           ; mCount
 *     802a3b40  cmplwi r3,0 / beq  ; skip the store when mItems is null
 *     802a3b48  lwz/stw 0,0 / lwz/stw 4,4 ; one 8-byte element
 *     802a3b60  bdnz
 *
 * `mAllocator` is an empty class, so its copy costs nothing and retail stores nothing for it. The
 * `cmplwi r3,0` guard inside the loop is `rstl::construct`'s null check on the destination.
 */
extern "C" rstl::vector< rstl::pair< uint, CParticleData::EParentedMode > >* fn_802A3AD0(
    rstl::vector< rstl::pair< uint, CParticleData::EParentedMode > >* self,
    const rstl::vector< rstl::pair< uint, CParticleData::EParentedMode > >& other) {
  self->mAllocator = other.mAllocator;
  self->mCount = other.mCount;
  self->mCapacity = other.mCapacity;
  if (other.mCount == 0 && other.mCapacity == 0) {
    self->mItems = nullptr;
  } else {
    self->mAllocator.allocate(self->mItems, self->mCapacity);
    rstl::uninitialized_copy_n(other.mItems, self->mCount, self->mItems);
  }
  return self;
}

rstl::ownership_transfer< IAnimReader > CAnimSourceReader::VClone() const {
  return rstl::ownership_transfer< IAnimReader >(
      rs_new CAnimSourceReader(mSource, mPOIData, mCurTime, mSteadyStateInfo, mPassedBoolCount,
                               mPassedIntCount, mPassedParticleCount, mPassedSoundCount,
                               mBoolStates, mInt32States, mParticleStates));
}

SAdvancementResults CAnimSourceReader::VReverseView(const CCharAnimTime& time) {
  const CCharAnimTime previousTime = mCurTime;
  const CCharAnimTime& duration = mSource->GetAnimationDuration();
  if (previousTime.EqualsZero()) {
    mCurTime = duration;
    return SAdvancementResults(
        time, SAdvancementDeltas(CVector3f::Zero(), CQuaternion::NoRotation()));
  }
  if (time.EqualsZero()) {
    return SAdvancementResults(
        CCharAnimTime::ZeroFlat(), SAdvancementDeltas(CVector3f::Zero(), CQuaternion::NoRotation()));
  }

  mCurTime -= time;
  CCharAnimTime remainingTime = CCharAnimTime::ZeroFlat();
  if (mCurTime < CCharAnimTime()) {
    remainingTime = CCharAnimTime() - mCurTime;
    mCurTime = CCharAnimTime();
  }

  const CSegId root(0);
  const CQuaternion oldRotation = mSource->GetRotation(root, previousTime);
  const CQuaternion newRotation = mSource->GetRotation(root, mCurTime);
  const CQuaternion inverseOldRotation = oldRotation.BuildInverted();
  CVector3f offset(0.f, 0.f, 0.f);
  if (mSource->HasOffset(root)) {
    const CVector3f oldOffset = mSource->GetOffset(root, previousTime);
    const CVector3f newOffset = mSource->GetOffset(root, mCurTime);
    offset = newOffset - oldOffset;
    const CQuaternion inverseNewRotation = newRotation.BuildInverted();
    const CMatrix3f inverseRotation = inverseNewRotation.BuildTransform();
    offset = inverseRotation * offset;
  }
  return SAdvancementResults(remainingTime,
                             SAdvancementDeltas(offset, newRotation * inverseOldRotation));
}

void CAnimSourceReader::VSetPhase(float phase) {
  mCurTime =
      CCharAnimTime(phase * mSource->GetSteadyStateAnimInfo(mCurTime).GetDuration().GetSeconds());
  UpdatePOIStates();
  if (!mCurTime.GreaterThanZero()) {
    mPassedBoolCount = 0;
    mPassedIntCount = 0;
    mPassedParticleCount = 0;
    mPassedSoundCount = 0;
  }
}

bool CAnimSourceReader::VSupportsReverseView() const { return true; }

SAdvancementResults CAnimSourceReader::VGetAdvancementResults(const CCharAnimTime& time,
                                                              const CCharAnimTime& startOffset) const {
  const CCharAnimTime previousTime = mCurTime + startOffset;
  CCharAnimTime currentTime = mCurTime + startOffset;
  const CCharAnimTime& duration = mSource->GetAnimationDuration();
  if (previousTime >= duration) {
    return SAdvancementResults(
        time, SAdvancementDeltas(CVector3f::Zero(), CQuaternion::NoRotation()));
  }
  if (time.EqualsZero()) {
    return SAdvancementResults(
        CCharAnimTime::ZeroFlat(), SAdvancementDeltas(CVector3f::Zero(), CQuaternion::NoRotation()));
  }

  currentTime += time;
  CCharAnimTime remainingTime = CCharAnimTime::ZeroFlat();
  if (currentTime > duration) {
    remainingTime = currentTime - duration;
    currentTime = duration;
  }

  const CSegId root(0);
  CVector3f offset(0.f, 0.f, 0.f);
  const CQuaternion oldRotation = mSource->GetRotation(root, previousTime);
  const CQuaternion newRotation = mSource->GetRotation(root, currentTime);
  const CQuaternion inverseOldRotation = oldRotation.BuildInverted();
  if (mSource->HasOffset(root)) {
    const CVector3f oldOffset = mSource->GetOffset(root, previousTime);
    const CVector3f newOffset = mSource->GetOffset(root, currentTime);
    offset = newOffset - oldOffset;
    const CQuaternion inverseNewRotation = newRotation.BuildInverted();
    const CMatrix3f inverseRotation = inverseNewRotation.BuildTransform();
    offset = inverseRotation * offset;
  }
  return SAdvancementResults(remainingTime,
                             SAdvancementDeltas(offset, newRotation * inverseOldRotation));
}

/**
 * `fn_802A2E68` - retail `.text:0x802A2E68`, 0x84 = 132 bytes, 0x802A2E68..0x802A2EEC (the next
 * symbol). It is the same deleting destructor as `fn_802A2E14` below, for
 * `CAnimSourceReaderBase::mParticleStates` - the `rstl::vector<rstl::pair<uint,
 * CParticleData::EParentedMode>>` at +72, called from the same destructor as
 * `addi r3,r30,72 ; li r4,-1 ; bl 0x802A2E68` - with one difference that is 0x30 bytes long:
 *
 *     802a2e88  lwz  r0,4(r30)     ; mCount
 *     802a2e8c  lwz  r3,12(r30)    ; mItems
 *     802a2e90  slwi r0,r0,3       ; mCount * 8, the element size
 *     802a2e94  add  r0,r3,r0      ; end
 *     802a2e98  stw  r3,20(r1)     ; both iterators, owner and pointer
 *     802a2ea0  stw  r0,16(r1)
 *     802a2ea4  stw  r0,12(r1)
 *     802a2ea8  stw  r3,8(r1)
 *     802a2eb0  addi r4,r4,8 / cmplw r4,r0 / bne -1 ; the element-destroy loop
 *     802a2ebc  bl   0x802ce388     ; CMemory::Free
 *     802a2ec0  extsh. r0,r31 / ble 0x802a2ed0
 *     802a2ec8  mr   r3,r30 / bl 0x802ce388
 *
 * The loop body is empty: the element is `rstl::pair<uint, CParticleData::EParentedMode>`, which
 * `include/rstl/pair.hpp` does not mark trivially destructible, so `rstl::destroy` keeps the loop
 * and only the element calls fold away. The four stack words are the `rstl::pointer_iterator`
 * pair (owner, pointer) of `begin()` and `end()`, spilled twice each; the loop itself compares
 * the pointers only. That is `rstl::destroy(begin(), end())` verbatim, which is what
 * `rstl::vector::~vector()` calls - and the 0x54-byte `fn_802A2E14` below is the same code once
 * `rstl::destroy` returns at once for a trivially destructible element.
 */
extern "C" rstl::vector< rstl::pair< uint, CParticleData::EParentedMode > >*
    fn_802A2E68(rstl::vector< rstl::pair< uint, CParticleData::EParentedMode > >* self, int flag) {
  if (self != nullptr) {
    rstl::destroy(self->begin(), self->end());
    self->mAllocator.deallocate(self->mItems);
    if (static_cast< short >(flag) > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

/**
 * `fn_802A2E14` - retail `.text:0x802A2E14`, 0x54 = 84 bytes, 0x802A2E14..0x802A2E68 (the next
 * symbol). It is the deleting destructor of the `rstl::vector<rstl::pair<uint, bool>>` that
 * `CAnimSourceReaderBase::mBoolStates` is: `CAnimSourceReaderBase::~CAnimSourceReaderBase()` at
 * 0x802A2D1C calls it as `addi r3,r30,40 ; li r4,-1 ; bl 0x802A2E14`, and +40 is `mBoolStates`.
 *
 *     802a2e14  stwu r1,-16(r1) / mflr r0 / stw r0,20(r1) / stw r31,12(r1) / stw r30,8(r1)
 *     802a2e24  mr   r31,r4         ; the flag, live from entry to its only test
 *     802a2e2c  mr.  r30,r3         ; `this`
 *     802a2e30  beq  0x802a2e4c     ; if (this == 0) return
 *     802a2e34  lwz  r3,12(r30)     ; mItems, the element storage
 *     802a2e38  bl   0x802ce388     ; CMemory::Free
 *     802a2e3c  extsh. r0,r31       ; the flag, sign-extended to 16
 *     802a2e40  ble  0x802a2e4c     ; if (flag <= 0) return
 *     802a2e44  mr   r3,r30
 *     802a2e48  bl   0x802ce388     ; CMemory::Free(this)
 *     802a2e4c  epilogue, `mr r3,r30`; returns `this`
 *
 * Three of the details are measurements, and the shape is the one `fn_80004A4C`
 * (`src/MetroidPrime/Player/CGameStateBlockDtor.cpp`) already reproduces at 100%:
 *
 * - **No element loop.** The other two `rstl::vector` destructors in this unit differ on
 *   exactly this: `fn_802A2E68` (`mParticleStates`, +72) is 0x84 bytes and runs an element-destroy
 *   loop first, because `rstl::pair<uint, CParticleData::EParentedMode>` is not marked trivially
 *   destructible in `include/rstl/pair.hpp`, and the int one is the 0x54-byte `fn_801ED894` in
 *   another unit. Marking `pair<uint, bool>` and `pair<uint, int>` trivially destructible is what
 *   makes this one 0x54 as well.
 * - **The flag is compared as a `short`** (`extsh.`, not `extsw.`) and stays in `r4` - only
 *   `CMemory::Free` is called, and it takes its argument in `r3`, so the frame spills `r30`/`r31`
 *   and nothing else.
 * - **`this` is tested once** and is the return value; `CMemory::Free` is null-safe
 *   (`cmplwi r31,0` at 0x802CE3A4), so a null `mItems` needs no test of its own.
 *
 * All 22 callers in the DOL pass -1, so the free-through-to-self is dead on this path; it is not
 * dead in the source.
 *
 * **Why the `extern "C"` name and not the destructor.** Retail's own symbol for this is the
 * implicit special member of `rstl::vector`, which mangles to
 * `__dt__Q24rstl54vector<Q24rstl10pair<Ui,b>,Q24rstl17rmemory_allocator>Fv`; dtk could not recover
 * a name for it and `config/G2ME01/symbols.txt` calls it `fn_802A2E14`, and objdiff pairs
 * functions by name. mwcceppc names the special member itself and will not take an `asm` name on
 * it, so - as in `CGameStateBlockDtor.cpp` - the body is written out here under retail's name. The
 * compiler still emits its own weak copy of the identical 0x54 bytes, because
 * `~CAnimSourceReaderBase` calls the mangled symbol; nothing in the DOL calls `fn_802A2E14`, so
 * this definition is only there to be compared.
 */
extern "C" rstl::vector< rstl::pair< uint, bool > >*
    fn_802A2E14(rstl::vector< rstl::pair< uint, bool > >* self, int flag) {
  if (self != nullptr) {
    CMemory::Free(self->mItems);
    if (static_cast< short >(flag) > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

CAnimSourceReader::~CAnimSourceReader() {}
