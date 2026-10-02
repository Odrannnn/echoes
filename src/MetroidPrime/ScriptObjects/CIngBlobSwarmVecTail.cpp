// CIngBlobSwarmVecTail.cpp - IngBlobSwarm's (module 31) out-of-line element-construct chain,
// .text 0x2350..0x23D0: an `rstl::vector`'s `push_back_unsafe`, the out-of-line `rstl::construct`
// it calls, and that one's `rstl::construct_impl`. A third unit in the same module, the same
// arrangement as `CLumiteRelTail.cpp`: the head, `CScriptIngBlobSwarmRel.cpp`, claims
// `.text 0x0..0xD8` and `CIngBlobSwarmFree.cpp` claims 0x1A60..0x1A80, this one claims
// `0x2350..0x23D0`, and everything else stays unclaimed so `dtk` fills it from retail and the
// module's sha1 against `config/G2ME01/config.yml` holds.
//
// Ranges from `config/G2ME01/rels/IngBlobSwarm/symbols.txt`:
//
//   0x2350 fn_31_2350  0x38  `push_back_unsafe` for the module's 0x4C-stride element vector
//   0x2388 fn_31_2388  0x20  the out-of-line `rstl::construct` it calls (a bare forwarder)
//   0x23A8 fn_31_23A8  0x28  that one's `rstl::construct_impl`: the null test, then the copy ctor
//
// **The three are named `fn_31_*`, so they are written as three `extern "C"` functions that call
// each other, not as template instantiations**: a `rstl::vector<T>` spelled with its own types
// emits `__ct__...`/`__as__...` mangled symbols, and `objdiff` pairs functions by name against
// `config/G2ME01/rels/IngBlobSwarm/symbols.txt`, so nothing would match. `rstl::construct.hpp`
// fixes the *shape* of the third function - `new (dest) T(src)` is `cmplwi dest,0 / beq / bl`,
// which is retail's - and `include/rstl/vector.hpp` fixes the first one's:
// `rstl::construct(mItems + mCount++, in)` is `lwz count / lwz items / mulli / addi / stw / add /
// bl`, in that order, with `mCount++` fused into the element address.
//
// **The element type is not modelled.** Retail's stride is 0x4C and nothing else here needs it:
// the layout read is `mCount` at +0x4 and `mItems` at +0xC, which is this tree's
// `rstl::vector`'s layout (`mAllocator` is 4 bytes). The copy constructor the chain ends in,
// `fn_31_23D0` (0x23D0, 0x13C), is **left retail** and is declared only so the call resolves by
// the dtk name, the arrangement `CLumiteRelTail.cpp` uses for `fn_39_7C0`. Its declaration carries
// one destination pointer and one `const void*` source reference and no return value, because that
// is all retail passes down the chain: r3 is the element slot and r4 is the caller's `const T&`.
//
// **fn_31_23A8 is the early-return spelling, not `if (dest != nullptr) { call(); }`.** The two are
// the same program and MWCC schedules them differently: retail's `cmplwi r3,0` sits *above* the
// saved-LR store (`stwu / mflr / cmplwi / stw / beq / bl`), which is what an early return emits.
// Measured on `fn_39_798` in `CLumiteRelTail.cpp`, which is the same eight instructions.
//
// This file is listed in `files.cmake`, and **its host branch defines nothing at all** - the same
// arrangement `CLumiteRelTail.cpp` uses, and the reason is the one
// `tools/check_files_cmake.py` enforces: a `Matching` object has to be either in `files.cmake` or
// in the checker's judge-owned EXCLUDED set, and the only automatic exemption is a unit that
// defines RELMain/RELExit, which this one deliberately does not.
//
// **The range needs no `force_active:` entry, which is not what the ScriptCoin note predicts.**
// All three functions are unreachable from the module's data and from its roots - none of them is
// in the generated `build/G2ME01/IngBlobSwarm/ldscript.lcf` FORCEACTIVE block - and they are the
// shape `AIMannedTurret`'s `fn_1_48C0` is, which is why `config/G2ME01/config.yml` has
// `force_active:` entries at all. Measured: with all three added to a `force_active:` list and
// with the list removed, the module's `.rel` is `cmp`-equal to retail both times (`87 files OK`).
// The difference is that here the three reach each other and `fn_31_23D0`, and mwldeppc keeps
// what a kept section calls; nothing here is a lone orphan.
//
// Definitions are in descending retail text order (mwcceppc emits definitions in reverse source
// order); `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CIngBlobSwarmVecTail.cpp`.

extern "C" {

// .text 0x23D0, 0x13C bytes: the copy constructor this chain ends in. Left retail - see above.
void fn_31_23D0(void* dest, const void* src);

#ifdef __MWERKS__

// .text 0x23A8, 0x28 bytes. `rstl::construct_impl`: `dest` in r3, `src` in r4.
void fn_31_23A8(void* dest, const void* src) {
  if (dest == 0) {
    return;
  }
  fn_31_23D0(dest, src);
}

// .text 0x2388, 0x20 bytes. The out-of-line `rstl::construct` above it: r3 and r4 pass straight
// through, and it keeps retail's own full epilogue rather than tail-calling.
void fn_31_2388(void* dest, const void* src) { fn_31_23A8(dest, src); }

// The element this vector holds is 0x4C bytes of data we do not model; only its size is
// load-bearing, because it is the `mulli` of `mItems + mCount++`.
struct SIngBlobElem {
  unsigned char mBytes[0x4C];
};

// `rstl::vector`'s head, in this tree's layout: `mAllocator` is four bytes, `mCount` at +0x4 and
// `mItems` at +0xC, which is where retail's `lwz 0x4(r3)` / `lwz 0xc(r3)` read them.
struct SIngBlobVec {
  int mAllocator;
  int mCount;
  int mCapacity;
  SIngBlobElem* mItems;
};

// .text 0x2350, 0x38 bytes. `push_back_unsafe`: `self` is the vector in r3, `in` the new element
// in r4. Retail multiplies the *old* count, stores count+1, then forms the slot address - that is
// `rstl::construct(mItems + mCount++, in)` with the increment inside the index expression, which
// is what puts the `stw` between the `mulli` and the `add`.
void fn_31_2350(SIngBlobVec* self, const void* in) {
  SIngBlobElem* const slot = self->mItems + self->mCount++;
  fn_31_2388(slot, in);
}

#endif
}