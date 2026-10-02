// CSplitterRelTwins.cpp - Splitter's (module 75) block of shared instantiations, .text
// 0x3678..0x3A8C: ten functions retail emitted out of `rstl::string` and
// `rstl::vector<CJointCollisionDescription>`. A third unit in the same module, the same
// arrangement as `CSplitterRel.cpp` / `CSplitterRelMain.cpp`: the head claims `.text 0x0..0xFC`
// and the entry pair claims 0x81F8..0x82B0, so everything between the three ranges stays
// unclaimed, dtk fills it from retail, and the module's sha1 against
// `config/G2ME01/config.yml` still holds (`27f0d2cd85fc1ad0ed0682f6bdd5356693090863`, measured with
// this object in the link).
//
// Ranges from `config/G2ME01/rels/Splitter/symbols.txt`:
//
//   0x3678 fn_75_3678  0x28  `rstl::string::operator==`'s shape - `bl compare; cntlzw; srwi r3,r0,5`
//   0x36A0 fn_75_36A0  0x7C  `rstl::string::compare`: four iterators into the frame, then 0x371C
//   0x371C fn_75_371C  0x114 `internal_compare`: the sign-extended byte loop, ends -1/0/1
//   0x3830 fn_75_3830  0x38  `vector<CJointCollisionDescription>::push_back_unsafe`
//   0x3868 fn_75_3868  0x20  `rstl::construct<T>` - the forwarder
//   0x3888 fn_75_3888  0x28  `rstl::construct_impl<T>` - the null-guarded placement copy
//   0x38B0 `__ct__22CJointCollisionOverlayFRC22CJointCollisionOverlay`  0xC0  the class's copy
//                                constructor; the address is renamed in `symbols.txt` (below)
//   0x3970 fn_75_3970  0x84  `vector<CJointCollisionDescription>`'s deleting destructor
//   0x39F4 fn_75_39F4  0x38  `rstl::destroy<It>` - the forwarder that re-copies both iterators
//   0x3A2C fn_75_3A2C  0x60  `rstl::destroy_impl<It>` - the loop, re-reading the end every pass
//
// Nine of the ten are compiler emissions rather than written functions: `construct`/
// `construct_impl` are the `inline` templates of `rstl/construct.hpp`, the two `destroy`s are
// `include/rstl/construct.hpp`'s pair, and the three string functions are the `inline` members of
// `rstl/string.hpp`. Retail outlined each of them, so this file writes each one out as a named
// `extern "C"` function holding the statements the template/member body has - that is the only
// way to get them out of line under the names `symbols.txt` gives them, which is what the rest of
// the module's (retail) code calls them by. The tenth, the copy constructor, is a real
// constructor spelled through an overlay (see below).
//
// The ten are not one translation unit's emission in retail either: the three string functions
// are the shape `src/MetroidPrime/CIOWinManager.cpp` and `src/MetroidPrime/CCredits.cpp` emit,
// and the seven `CJointCollisionDescription` ones are `src/MetroidPrime/CCollisionActorManager.cpp`'s
// - the same instructions, with this module's own call and data addresses (`tools/twin_scan.py`
// lists them as twins). The two `destroy` functions are hand-written here for the reason
// `CCollisionActorManager.cpp`'s header comment records: the header's `rstl::destroy` outlines a
// single 100-byte copy, while retail has the forwarder and the loop as two functions.
//
// **`mw_version="GC/2.7"` is load-bearing and measured** (the `Object(...)` line in
// `configure.py`). Under the module's default GC/1.3.2 the same source reproduces only five of
// the ten (`fn_75_3678`, `fn_75_3830`, `fn_75_3868`, `fn_75_3888`, `fn_75_3970`);
// `fn_75_36A0`, `fn_75_371C`, the copy constructor, `fn_75_39F4` and `fn_75_3A2C` come out
// scheduled differently, and 2.7 emits retail's order for all ten - the same finding as
// `CLumiteRelTail.cpp` and `CSandBossRelTail.cpp`.
//
// **`fn_75_3A2C` re-reads the end iterator every pass** (`lwz r0,0x0(r30)` above the loop test,
// not hoisted): the loop body calls `internal_dereference`, and mwcceppc will not keep a load
// out of a loop whose body can write through the non-const `It&` the callee receives. That is
// why both `destroy` parameters are by value - a class type is passed by address either way, and
// by value is the spelling that reproduces both functions (measured in
// `CCollisionActorManager.cpp`).
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100%, and only the module's sha1 would catch it.
//
// This file is in `files.cmake`, and its host branch defines nothing - the same arrangement
// `CLumiteRelTail.cpp` and `CSandBossRelTail.cpp` use: the port reads `Splitter.rel` off the disc
// and never calls into the module, so defining these symbols for the host link would only add
// undefined references to `fn_75_*` neighbours.

#include "MetroidPrime/Collision/CJointCollisionDescription.hpp"
#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/Animation/CSegId.hpp"
#include "Kyoto/Math/CMatrix3f.hpp"
#include "Kyoto/Math/CVector3f.hpp"
#include "rstl/construct.hpp"
#include "rstl/string.hpp"
#include "rstl/vector.hpp"

typedef rstl::vector< CJointCollisionDescription >::iterator CJointDescriptionIterator;

// A bit-exact overlay of `CJointCollisionDescription`, used only to spell its copy constructor
// out of line - the same device `include/Kyoto/Math/CMatrix3f.hpp` uses for `CMatrix3fBlock`
// ("a bit-exact overlay of CMatrix3f, used only to spell the element copy the way retail's bytes
// do"). The class's own copy constructor is implicitly declared, and mwcceppc rejects an
// out-of-line definition of it ("object ... redefined"), so an overlay is the only way to write
// that body as a constructor - and a constructor is what retail has: its register allocation
// (`this` in r30, the argument in r31, `mr r3,r30` returning it) and its member-by-member schedule
// come from a member initialisation list, and no plain function spelling reproduced them
// (measured: `s.mX = o.mX` assignments, with placement new for the two class members, come out
// with two `addic.` guards and a three-deep `f0`/`f1`/`f2` pipeline).
//
// **The member types are load-bearing too.** `mPivotId`/`mNextId` have to be the class's own
// `CSegId`, not `char`: with `char` the bytes are read before the first float and the float copy
// runs three registers deep, where retail reads the bytes after the first float store and
// alternates `f1`/`f0` two deep.
class CJointCollisionOverlay {
public:
  CJointCollisionOverlay(const CJointCollisionOverlay& other);
  int mType;
  int mOrientationType;
  CSegId mPivotId;
  CSegId mNextId;
  CVector3f mBounds;
  CVector3f mPivotPoint;
  float mRadius;
  float mMaxSeparation;
  rstl::string mName;
  TUniqueId mActorId;
  float mMass;
  CMatrix3f mOrientation;
};

extern "C" {
#ifdef __MWERKS__

void fn_75_3A2C(CJointDescriptionIterator b, CJointDescriptionIterator e);
void fn_75_39F4(CJointDescriptionIterator b, CJointDescriptionIterator e);
void* fn_75_3970(rstl::vector< CJointCollisionDescription >* self, short flag);

void fn_75_3888(void* dest, const CJointCollisionDescription& src);
void fn_75_3868(CJointCollisionDescription* dest, const CJointCollisionDescription& src);
void fn_75_3830(rstl::vector< CJointCollisionDescription >* self,
                const CJointCollisionDescription& in);
int fn_75_371C(rstl::string::const_iterator first, rstl::string::const_iterator last,
               rstl::string::const_iterator otherFirst, rstl::string::const_iterator otherLast);
int fn_75_36A0(const rstl::string& self, const rstl::string& other);
bool fn_75_3678(const rstl::string& self, const rstl::string& other);

// .text 0x3A2C, 0x60 bytes. `destroy_impl<It>(It begin, It end)`: the end is re-read from its
// frame slot every pass, because `destroy` calls out of line.
void fn_75_3A2C(CJointDescriptionIterator b, CJointDescriptionIterator e) {
  CJointDescriptionIterator cur = b;
  for (; cur != e; ++cur) {
    rstl::destroy(&*cur);
  }
}

// .text 0x39F4, 0x38 bytes. `destroy<It>(It begin, It end)`: both parameters by value, so
// mwcceppc materialises them in argument order - end at +0x8, begin at +0xC - and forwards their
// addresses.
void fn_75_39F4(CJointDescriptionIterator b, CJointDescriptionIterator e) { fn_75_3A2C(b, e); }

// .text 0x3970, 0x84 bytes. `vector<CJointCollisionDescription>`'s deleting destructor: the
// elements, then the block, then `this` when the flag says so. `flag` is a `short` in retail's
// frame (`extsh. r0,r31`), and the return in r3 is the destructor's `this`.
void* fn_75_3970(rstl::vector< CJointCollisionDescription >* self, short flag) {
  if (self != 0) {
    fn_75_39F4(self->begin(), self->end());
    CMemory::Free(self->mItems);
    if (flag > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

} // extern "C"

// .text 0x38B0, 0xC0 bytes. The copy constructor - `CJointCollisionDescription`'s, spelled
// through the overlay above so that it is a constructor at all; see the overlay's comment.
// `symbols.txt` renames this module address after the overlay (`fn_75_38B0` was the dtk name),
// so the module's retail code that calls 0x38B0 resolves to this definition while the body stays
// the class's own - the same device `tools/wire_rel_setup.py` uses when it names a module's
// entry points RELMain/RELExit after they are written.
//
// Its position in the source is what puts it at 0x38B0: mwcceppc emits definitions in reverse
// source order, so it sits between `fn_75_3970` and `fn_75_3888` below. It also makes the
// compiler emit `rstl::string`'s destructor as a weak `__dt__...Fv` copy after it (a constructor
// with a class-typed member does that); nothing references it, mwldeppc drops it, and the
// module's `.text` still comes out 0x11C64 against retail's 0x11C64 - measured, not assumed.
CJointCollisionOverlay::CJointCollisionOverlay(const CJointCollisionOverlay& other)
: mType(other.mType)
, mOrientationType(other.mOrientationType)
, mPivotId(other.mPivotId)
, mNextId(other.mNextId)
, mBounds(other.mBounds)
, mPivotPoint(other.mPivotPoint)
, mRadius(other.mRadius)
, mMaxSeparation(other.mMaxSeparation)
, mName(other.mName)
, mActorId(other.mActorId)
, mMass(other.mMass)
, mOrientation(other.mOrientation) {}

extern "C" {

// .text 0x3888, 0x28 bytes. `rstl::construct_impl<T>`: the placement copy, whose destination
// guard is the template's own.
void fn_75_3888(void* dest, const CJointCollisionDescription& src) {
  new (dest) CJointCollisionOverlay(
      *static_cast< const CJointCollisionOverlay* >(static_cast< const void* >(&src)));
}

// .text 0x3868, 0x20 bytes. The forwarder; `construct<T>` calls `construct_impl<T>`.
void fn_75_3868(CJointCollisionDescription* dest, const CJointCollisionDescription& src) {
  fn_75_3888(dest, src);
}

// .text 0x3830, 0x38 bytes. `push_back_unsafe`: the count is bumped into its own slot before the
// element address is formed, and the element is constructed at `mItems + oldCount`.
void fn_75_3830(rstl::vector< CJointCollisionDescription >* self,
                const CJointCollisionDescription& in) {
  fn_75_3868(self->mItems + self->mCount++, in);
}

// .text 0x371C, 0x114 bytes. `internal_compare<It>`: the two ranges interleaved, the first
// differing byte sign-extended and subtracted, and the -1/0/1 tail.
int fn_75_371C(rstl::string::const_iterator first, rstl::string::const_iterator last,
               rstl::string::const_iterator otherFirst, rstl::string::const_iterator otherLast) {
  rstl::string::const_iterator it = first;
  rstl::string::const_iterator other = otherFirst;
  for (; it != last && other != otherLast; ++it, ++other) {
    const int cmp = rstl::char_traits< char >::compare(*it, *other);
    if (cmp != 0) {
      return cmp;
    }
  }
  if (it == last && other != otherLast) {
    return -1;
  }
  if (it == last) {
    return 0;
  }
  return 1;
}

// .text 0x36A0, 0x7C bytes. `compare(const basic_string&) const`: one range from the receiver,
// one from the argument, both into 0x371C.
int fn_75_36A0(const rstl::string& self, const rstl::string& other) {
  return fn_75_371C(self.begin(), self.end(), other.begin(), other.end());
}

// .text 0x3678, 0x28 bytes. `operator==`: the comparison's result inverted by `cntlzw`/`srwi`.
bool fn_75_3678(const rstl::string& self, const rstl::string& other) {
  return fn_75_36A0(self, other) == 0;
}

#endif
}
