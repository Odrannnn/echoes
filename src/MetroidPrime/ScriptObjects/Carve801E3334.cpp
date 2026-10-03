// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:7796-7797`, the instructions are the ones dtk
// itself emitted into `build/G2ME01/asm/auto_03_801E2E30_text.s:367-430`, and the bodies below
// are the C those bytes are the compilation of.
//
// .text 0x801E3334..0x801E3414, 0xE0 = 224 bytes, 2 functions:
//
//   fn_801E3334    0x801E3334  0x40    16 instructions
//   fn_801E3374    0x801E3374  0xA0    40 instructions
//
// **What they are: `rstl::list<rstl::pair<TUniqueId, float>>::insert` and
// `::do_insert_before`, retail's `symbols.txt` placeholders for a pair retail's own linker kept
// under the template's mangled name.**  Both bodies are byte-shape twins of functions this tree
// already matches at 100.00%, which is where the source below is read from rather than guessed:
//
//   * `fn_801E3374` (0x801E3374, 0xA0) is
//     `do_insert_before__Q24rstl60list<Q24rstl18pair<9TUniqueId,f>,Q24rstl17rmemory_allocator>FPQ34rstl60list<Q24rstl18pair<9TUniqueId,f>,Q24rstl17rmemory_allocator>4nodeRCQ24rstl18pair<9TUniqueId,f>`
//     (0x8016B0F8, 0xA0, `symbols.txt:5982`) - the same forty instructions, word for word, with
//     `48 19 29 91` where this copy has `48 11 A7 15` for the one `bl`.  `include/rstl/list.hpp:94`
//     is the body, written out below with `create_node` (`:85`) spelled in rather than called.
//   * `fn_801E3334` (0x801E3334, 0x40) is
//     `insert__Q24rstl60list<Q24rstl18pair<9TUniqueId,f>,Q24rstl17rmemory_allocator>FRCQ34rstl60list<Q24rstl18pair<9TUniqueId,f>,Q24rstl17rmemory_allocator>8iteratorRCQ24rstl18pair<9TUniqueId,f>`
//     (0x8016B0B8, 0x40, `symbols.txt:5981`) - the same sixteen, with this copy's `bl
//     fn_801E3374` for that copy's `bl do_insert_before...`.  `include/rstl/list.hpp:106` is the
//     body.  The seed named `fn_8014C0AC` (0x8014C0AC, 0x40,
//     `src/MetroidPrime/CActorModelParticles.cpp:790`, also 100.00%) as the twin, and it compiles
//     to these sixteen instructions too - both are the same wrapper written for a different
//     element type.
//
// **The `addic. r4,r3,8 / beq` around the value copy is retail's, and it is what decides how
// `rstl::construct(nn->get_value(), val)` is spelled in `fn_801E3374`.**  The copy moves as two
// plain loads and stores (`lhz r0,0(r30) / lfs f0,4(r30) / sth r0,0(r4) / stfs f0,4(r4)`), so it
// is not a call, but it is still guarded: `rstl::pair<TUniqueId, float>` has no
// `construct_impl` specialization in `include/rstl/pair.hpp`, so `rstl::construct` takes the
// generic `new (dest) T(src)` path in `include/rstl/construct.hpp:54-56`, and *that* is what
// emits the placement-new null check.  The same note already stands for `pair<int, auto_ptr<T>>`
// and `rstl::map<TUniqueId, CGameModeListener*>` at `include/rstl/pair.hpp:133`.  Writing the
// copy as a bare assignment would drop the branch and the function to 0x98.
//
// **`prev` on its own line is load-bearing too, and it is what puts the function in four
// callee-saved registers instead of three.**  Retail loads `prev` (`lwz r31,0(r4)`) *before* the
// `bl allocate`, into r31, so that r28..r31 all hold live values across the call; the epilogue's
// four restores (`stw r28,0x10(r1)` .. `lwz r28,0x10(r1)`) follow from that.
// `include/rstl/list.hpp:95` gets it because `prev` is `create_node`'s own **parameter** there,
// evaluated before its body runs, so this file spells the same temporary out rather than
// assigning `n->mPrev` after the call: written that way the load lands *after* the `bl`, in r0,
// and the function comes out at 86.80% with three callee-saved registers and the same 40
// instructions.
//
// Source order is **descending by address** - `fn_801E3374` above `fn_801E3334` - and that is
// load-bearing: mwcceppc emits function definitions in *reverse* source order and mwldeppc keeps
// the object `.text` verbatim, so an ascending file is a permuted `.text`.  Measured here rather
// than assumed: declared the other way round, `nm` put `fn_801E3374` at offset 0 and
// `fn_801E3334` at 0x98, and the linked DOL then carried each body at the other's address -
// 166 bytes wrong across the carve and the one `bl` at 0x801E3210 that points into it - while
// objdiff still scored both functions 100.00%.  Only `tools/flip_test.sh` catches that, and this
// carve's flip did.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces those symbols verbatim, so the definitions stay `extern "C"`; the bodies need
// `rstl`'s own types because retail's are template instantiations that only the mangled names can
// reach, which is the arrangement `src/MetroidPrime/CActorModelParticles.cpp:764-795` already uses
// for the same list members.  `fn_801E3374`'s body is written out flat rather than delegating to
// `self->do_insert_before(...)` / `self->create_node(...)` because a call to either would emit a
// weak mangled symbol the retail object does not define, and an object that defines a function
// retail does not can never be `Matching` (`tools/unit_fit.sh`).
//
// Its own unit because the range sits inside one dtk `auto_*` object, and it is also what split
// it: before this claim `build/G2ME01/asm/auto_03_801E2E30_text.s` was one object of
// 0x801E2E30..0x801E3864 (`# 0x801E2E30..0x801E3864 | size: 0xA34`), and `build/report.json` now
// has three where it had one - `auto_03_801E2E30_text` (0x801E2E30..0x801E3334, 0x504 = 1284 B,
// 3 functions), this unit (224 B, 2 functions) and `auto_03_801E3414_text` (0x801E3414..0x801E3864,
// 0x450 = 1104 B, 4 functions).  A unit may not claim part of another's object, and the
// functions on either side of this run are not trivial anyway: below, `fn_801E2E30` (0x801E2E30,
// 0x504) ends the run; above, `fn_801E3414` (0x801E3414, 0x270) starts the next one.
//
// **Who calls them**, measured with `grep -rn 'bl fn_801E3334\|bl fn_801E3374' build/G2ME01/asm/` -
// two call sites, both in unclaimed dtk `auto_*` text: `fn_801E3334` at 0x801E3210, and
// `fn_801E3374` at 0x801E3358, which is inside `fn_801E3334` itself.  So this claim is what the
// port link gets these two symbols from; nothing claimed depends on them.
//
// The directory is retail's own, taken from the nearest claimed range: the claim below is
// `MetroidPrime/ScriptObjects/CScriptTriggerEllipsoid.cpp` (ends 0x801E2E30) and the one above
// `MetroidPrime/ScriptObjects/Carve801E3864.c` (starts 0x801E3864), so this address sits in that
// unit's neighbourhood.  For an anonymous function that is the only evidence there is, and it
// beats a lane picking the directory it happened to own.

#include "MetroidPrime/TGameTypes.hpp"

#include "rstl/construct.hpp"
#include "rstl/list.hpp"
#include "rstl/pair.hpp"

/** `rstl::list<rstl::pair<TUniqueId, float>, rstl::rmemory_allocator>`, spelled out because
 *  retail's link kept it under the mangled template name and the two functions below are retail's
 *  own placeholders for it.  `include/rstl/list.hpp` makes the members public for exactly this. */
typedef rstl::list< rstl::pair< TUniqueId, float >, rstl::rmemory_allocator > SCarve801E3334List;

extern "C" SCarve801E3334List::iterator
fn_801E3334(SCarve801E3334List* self, const SCarve801E3334List::iterator& pos,
            const rstl::pair< TUniqueId, float >& val);

extern "C" SCarve801E3334List::node* fn_801E3374(SCarve801E3334List* self,
                                                SCarve801E3334List::node* n,
                                                const rstl::pair< TUniqueId, float >& val) {
  SCarve801E3334List::node* const prev = n->mPrev;
  SCarve801E3334List::node* nn;
  self->mAllocator.allocate(nn, 1);
  nn->mPrev = prev;
  nn->mNext = n;
  rstl::construct(nn->get_value(), val);
  if (n == self->mStart) {
    self->mStart = nn;
  }
  nn->get_prev()->set_next(nn);
  nn->get_next()->set_prev(nn);
  ++self->mCount;
  return nn;
}

extern "C" SCarve801E3334List::iterator
fn_801E3334(SCarve801E3334List* self, const SCarve801E3334List::iterator& pos,
            const rstl::pair< TUniqueId, float >& val) {
  return SCarve801E3334List::iterator(fn_801E3374(self, pos.get_node(), val));
}