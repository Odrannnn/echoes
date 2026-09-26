/**
 * `fn_802FC350` and `fn_802FC378` - the pak-list insert every pak load in the boot path
 * goes through. Retail `.text:0x802FC350..0x802FC420`, 0xD0 = 208 bytes, two functions:
 *
 * | function | address | size | what |
 * | --- | --- | --- | --- |
 * | `fn_802FC350` | 0x802FC350 | 0x28 | `node* insert_at_end(void* item)` - forwards to `fn_802FC378` with the list's own `x8_end` |
 * | `fn_802FC378` | 0x802FC378 | 0xA8 | `node* do_insert_before(void* pos, void* item)` - the insert itself |
 *
 * Both are unnamed in retail's map, so they are emitted under their dtk names as `extern "C"`
 * and no rename in `config/G2ME01/symbols.txt` is needed. `fn_802FC378` is referenced from no
 * object outside this one (only `fn_802FC350` calls it), while `fn_802FC350` is called from
 * `fn_802FCFF4`'s retail object and from the `Matching` unit
 * `src/Kyoto/CResLoaderAddPakFileAsync.cpp`, which is why it keeps its name.
 *
 * The bodies are **not** transcriptions. `fn_802FC378` is
 * `rstl::list< SPakLoadEntry >::do_insert_before`, called through the public `node*` and
 * `end()` members, and `fn_802FC350` is the one-line wrapper. The evidence that this is the
 * right reading rather than a convenient one:
 *
 *  * **`fn_802FC378`'s 0xA8 bytes are identical to retail's own named
 *    `do_insert_before<list<auto_ptr<CFilePreloadData>>>` at 0x803445DC** - the same
 *    prologue, `li r3,16`, the same `r28`/`r29`/`r30`/`r31` assignment order, the same
 *    `addic. r5,r3,8` / `beq` over the 8-byte copy, the same head fixup, the same two link
 *    stores, the same `++x14_count`, the same 32-byte frame. That instantiaton is a
 *    `Matching` unit (`src/Kyoto/Streams/CFilePreload.cpp`, 100.00%), so the spelling below is
 *    the one this tree already reproduces for another element type.
 *  * `fn_802FC350`'s 10 instructions are what a non-inlined one-line call compiles to; the
 *    `mr r5,r4` sits above the `stw r0,20(r1)` link-register save, which is where MWCC puts a
 *    parameter shuffle when the callee is a real `bl`.
 *  * `rstl::list`'s own `create_node` / `do_insert_before` reach `allocate` out of line
 *    (`bl allocate__Q24rstl17rmemory_allocatorFi` at 0x802fc3a8), which is this tree's
 *    default `rstl/rmemory_allocator.hpp` revision, so the allocator needs no macro here.
 *
 * **The `addic. r5,r3,8` / `beq` guard is `rstl::construct`, and it is correct.** It is the
 * carry out of a 16-bit add of 8 to a 16-byte-aligned pointer, so it can never be set and the
 * copy always runs; retail has the same dead branch (`docs/RUNNING_THE_DECOMP.md` records that
 * replacing this `new (dest) T(src)` with a plain assignment was measured and breaks five
 * `Matching` units and the DOL sha1, while gaining four functions at 100%). The copy
 * constructor it expands is the one in `Kyoto/CResLoader.hpp`, and *that* is what clears the
 * caller's flag byte at 0x802fc3d0.
 *
 * **`#pragma inline_max_size` has to be *large* here, and both directions of the mistake are
 * silent.** `do_insert_before` is a template member, so mwcceppc emits it as a separate COMDAT
 * and calls it unless the threshold is above its own size estimate. Measured on this file with
 * the current `Kyoto/CResLoader.hpp`:
 *
 * | value | what comes out |
 * | --- | --- |
 * | unset (default) | `fn_802FC378` is a 0x20-byte forwarder and `do_insert_before` is emitted beside it - 0x58 of the 0xA8 missing, plus a symbol retail does not have |
 * | `0` | every inline is suppressed, so `rstl::construct` stops being a placement `new` and becomes `__nw__FUlPv` + a null test + a call to the copy constructor |
 * | 125 / 150 / 160 / 170 / 180 | `.text` is **0xF0**, not 0xD0: `do_insert_before` is emitted beside the two functions and is a third `T` symbol |
 * | **200** | **byte-identical to retail over 0xD0, two `T` symbols** - this is the value in use; 190, 250, 400, 1000 and 100000 are identical to it |
 *
 * **The threshold is not stable across header edits, and that is worth knowing.** 125 was
 * measured working earlier in the same session and stopped working when
 * `Kyoto/CResLoader.hpp` began including `Kyoto/CPakFile.hpp` for `x68_curRes`'s type - the
 * same source, the same compiler, 55 bytes of threshold apart, and the only symptom is
 * `unit_fit.sh` reporting a third function and "over by 32". If this unit ever stops flipping,
 * raise the pragma before looking at the body. (125 is what
 * `src/Kyoto/Streams/CFilePreload.cpp` uses, for the same job and the same fragility: retail
 * keeps `rstl::list`'s inline members out of line there, and here it does not.)
 */
#pragma inline_max_size(200)

#include "types.h"

#include "rstl/list.hpp"

#include "Kyoto/CResLoader.hpp"

// `fn_802FC378` - `.text:0x802FC378`, `size:0xA8`. Declared before it is defined, and
// *before* `fn_802FC350` below, because mwcceppc emits definitions in reverse source order
// and mwldeppc keeps the object's `.text` order: `fn_802FC378` is at the *higher* retail
// offset, so it is the one declared first. The reverse of this is the defect
// `tools/check_decl_order.py` exists to catch - the object comes out exactly 0xD0 bytes and
// every function still scores 100%, and only the DOL sha1 sees the permutation.
extern "C" void* fn_802FC378(void* pakList, void* pos, void* item);

extern "C" void* fn_802FC378(void* pakList, void* pos, void* item) {
  typedef rstl::list< SPakLoadEntry > list_t;
  return static_cast< list_t* >(pakList)->do_insert_before(
      static_cast< list_t::node* >(pos), *static_cast< SPakLoadEntry* >(item));
}

// `fn_802FC350` - `.text:0x802FC350`, `size:0x28`. Its whole job is to reach through the list
// for its own `x8_end` (retail's `lwz r4,8(r3)` at 0x802fc360 - the *tail*, not the head at
// +4) and hand that to the insert; `end()` is the public spelling of that read. It is
// `AddPakFileAsync`'s and `fn_802FCFF4`'s entry point, and it returns the new node, which
// neither caller uses.
extern "C" void* fn_802FC350(void* pakList, void* item) {
  typedef rstl::list< SPakLoadEntry > list_t;
  list_t* const self = static_cast< list_t* >(pakList);
  return fn_802FC378(pakList, self->end().get_node(), item);
}
