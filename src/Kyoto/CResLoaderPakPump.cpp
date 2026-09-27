/**
 * `CResLoader::AreAllPaksLoaded` - retail `fn_802FCCE4`, `.text:0x802FCCE4`, `size:0x10`.
 *
 * `lwz r0,92(r3)` / `cntlzw r0,r0` / `srwi r3,r0,5` / `blr`. 92 = 0x5C, which is the
 * `x14_count` of the `rstl::list< SPakLoadEntry >` at `CResLoader`+0x48 - the list
 * `AddPakFileAsync` pushes every new pak into and the only list the pump removes from.
 * `cntlzw`/`srwi 5` is MWCC's rendering of `count == 0`, i.e. of `rstl::list::empty()`.
 *
 * `CResLoader::AsyncIdlePakLoading` - retail `fn_802FCCF4`, `.text:0x802FCCF4`,
 * `size:0x9C` = 156 bytes.
 *
 * The loop `CGameGlobalObjects::AddPaksAndFactories` block 7 drives
 * (`docs/research/paks.md`), and the one that makes a pak exist:
 *
 *   0x802FCD0C  r29 = this->x48.x4_start                ; begin()
 *   0x802FCD14  r30 = r29->x0xC                         ; the node's item's x4_pak
 *   0x802FCD18  r31 = CPakFile::x28_aramFile             ; flag field 25, `rlwinm ...,26,...`
 *   0x802FCD1C  if (r31 || r28 == 0)  ->  0x802FCD2C
 *   0x802FCD2C  pak->AsyncIdle()
 *   0x802FCD34  if (pak->x2c_asyncLoadPhase == kAP_Loaded) {
 *   0x802FCD40    fn_802FCFF4(this, &r29->x8_item)     ; move it to a finished list
 *   0x802FCD4C    r29 = fn_802FD174(this->x48, r29)   ; unlink; returns the next node
 *   0x802FCD58    goto the test - the "not ARAM file" latch below is NOT set
 *   0x802FCD60  } else if (!r31) { r28 = 1; }          ; a plain pak is still loading
 *   0x802FCD6C  r29 = r29->x4_next
 *   0x802FCD70  } while (r29 != this->x48.x8_end)
 *
 * Three things in that are worth stating because they are not what the names suggest:
 *
 *  * **The flag is the ARAM-file bit, not the world-pak bit.** `rlwinm. r31,r0,26,31,31` at
 *    0x802fcd1c extracts flag field 25, and in `CPakFile`'s constructor (0x803245b8 onwards)
 *    field 25 is the one filled from `CDvdFile::IsARAMFile()` (`rlwimi r0,r4,6,25,25` over
 *    `lbz r4,8(this)`) - field 24 is `buildDepList`, 26 is `worldPak` and 27 is
 *    `stashedInARAM`. `EnsureWorldPakReady` reads `worldPak` with `rlwinm ...,27,31,31`
 *    (0x8032309c), which is the *other* bit, and writing `IsWorldPak()` here compiles to
 *    exactly that wrong instruction. It matters: the condition below is "keep pumping the
 *    ARAM copies", and a world pak is not the same thing as an ARAM-file pak.
 *  * `r28` is a latch, and it means "some plain pak has been idled and is still loading".
 *    Once it is set, `r31 || r28` only idles the **ARAM-file** paks for the rest of the
 *    call, so a long tail of plain paks is pumped one per call.
 *  * `0x802FCD18` reads `r30->x28` once and reuses it across both tests, and
 *    `0x802FCD70` re-reads `this->x8_end` on every iteration instead of keeping the end
 *    iterator in a register. The source has to be written the way retail wrote it - one read
 *    of the flag into a local, and the end test written inside the loop condition - or the
 *    register allocator produces seven saved registers where retail has five, and the frame
 *    comes out 48 bytes deep instead of 32.
 *
 * The polarity at the call site, which `docs/research/paks.md` currently has backwards:
 * `AddPaksAndFactories` 0x80007484-0x8000748C is `AreAllPaksLoaded(); clrlwi. r0,r3,24;
 * beq 0x80007430`, and 0x80007430 is the body. So the body runs while the result is **zero**,
 * i.e. while the loading list is non-empty, and the caller's loop is
 * `while (!resLoader.AreAllPaksLoaded()) { resLoader.AsyncIdlePakLoading(); ... }`.
 */

#include "types.h"

#include "Kyoto/CPakFile.hpp"
#include "Kyoto/CResLoader.hpp"

// Both are also declared, with the same C linkage, just above `class CResLoader` in
// `Kyoto/CResLoader.hpp`: that is where the `friend` declarations that let the `TARGET_PC`
// definitions at the bottom of this file reach the lists live, and mwcceppc rejects a
// `friend extern "C"` outright ("illegal storage class"). They are repeated here because the
// retail addresses belong next to the bodies that call them.
extern "C" {
// `fn_802FCFF4` - `.text:0x802FCFF4`, `size:0x30`. Unnamed in retail, and it is nothing but a
// list choice: `lwz r5,4(r4)` / `lbz r0,40(r5)` / `rlwinm. r0,r0,26,31,31` on the entry's
// `CPakFile*` - flag field **25**, `x28_aramFile` - then `addi r3,r3,24` for an ARAM-file pak or
// `addi r3,r3,48` for a plain one, each followed by `bl fn_802FC350`. So +0x18 is
// `x18_aramFileList` and +0x30 is `x30_pakList`.
void* fn_802FCFF4(void* resLoader, void* entry);
// `fn_802FD174` - `.text:0x802FD174`, `size:0x9C`. Unnamed in retail; it is
// `rstl::list< SPakLoadEntry >::do_erase`, called out of line. It relinks the neighbours
// (`stw r0,4(r3)` / `stw r0,0(r3)` at 0x802fd1b4/0x802fd1c0), runs the item's destructor -
// `lbz` the flag at 0x802fd1c8 and `bl __dt__8CPakFileFv` on `*(item+4)` at 0x802fd1dc - frees
// the node, decrements `x14_count`, and returns the node's `x4_next` captured at 0x802fd198
// *before* the unlink, which is why the caller must use the return value rather than
// `node->x4_next`. That `~CPakFile` spin is a no-op in practice: the only caller reaches this for
// an entry whose pak has just tested `IsCompletelyLoaded()`.
void* fn_802FD174(void* list, void* node);
} // extern "C"

void CResLoader::AsyncIdlePakLoading() {
  // The declaration order of these three is load-bearing and it is the reverse of what it
  // looks like it should be. mwcceppc hands out r30, r29, r28 to the first, second and third
  // local, so declaring them pak / node / latch - with `pak` declared *uninitialised*, before
  // the cursor it is derived from - is what puts them in r30 / r29 / r28. Declaring
  // latch / node / pak instead, which is the order the code reads in, emits an identical
  // instruction-for-instruction body with r28 and r30 the other way round and a 0x9C function
  // that is not retail's 0x9C function. Verified both ways against 0x802FCCF4.
  CPakFile* pak;
  rstl::list< SPakLoadEntry >::node* node = x48_pakLoadingList.begin().get_node();
  // "some plain pak is idled above and is still loading": see the header comment.
  bool sawLoadingPak = false;
  while (node != x48_pakLoadingList.end().get_node()) {
    pak = node->get_value()->x4_pak;
    // The cached bit field, not `DvdFile().IsARAMFile()`: see the comment at the definition.
    const bool isARAMFile = pak->IsARAMFile();
    if (isARAMFile || !sawLoadingPak) {
      pak->AsyncIdle();
    }
    if (pak->IsCompletelyLoaded()) {
      fn_802FCFF4(this, node->get_value());
      // The node is gone after the erase, so the next cursor has to be the one the erase
      // returned - retail's `mr r29,r3` at 0x802fcd58, not `r29->x4_next`.
      node = static_cast< rstl::list< SPakLoadEntry >::node* >(
          fn_802FD174(&x48_pakLoadingList, node));
      continue;
    }
    if (!isARAMFile) {
      sawLoadingPak = true;
    }
    node = node->get_next();
  }
}

// Declared second on purpose. mwcceppc emits function bodies in **reverse** source order and
// mwldeppc keeps the object's `.text` order verbatim, so a unit's functions have to be
// declared descending by retail offset. `AreAllPaksLoaded` is at 0x802FCCE4 and
// `AsyncIdlePakLoading` at 0x802FCCF4, so the *lower* one is declared first and lands second.
// With the two the other way round the object is a clean 0xAC bytes and still permuted, which
// `unit_fit.sh` reports as "fits" and only the DOL sha1 catches.
bool CResLoader::AreAllPaksLoaded() const { return x48_pakLoadingList.empty(); }

#ifdef TARGET_PC
// The port's own copies of the two retail helpers, which the matching build leaves undefined
// on purpose: both are unnamed in `config/G2ME01/symbols.txt`, so their bytes belong to
// whichever `auto_*_text` object dtk carves them into, and naming them in a `Matching` unit
// would delete those bytes. mwcceppc does not define TARGET_PC, so the matching build never
// sees this and the calls above bind to retail's own `fn_802FCFF4` / `fn_802FD174`.
extern "C" void* fn_802FC350(void* pakList, void* entry);

extern "C" void* fn_802FCFF4(void* resLoader, void* entry) {
  CResLoader* const self = static_cast< CResLoader* >(resLoader);
  SPakLoadEntry* const e = static_cast< SPakLoadEntry* >(entry);
  if (e->x4_pak->IsARAMFile()) {
    fn_802FC350(&self->x18_aramFileList, e);
  } else {
    fn_802FC350(&self->x30_pakList, e);
  }
  return nullptr;
}

extern "C" void* fn_802FD174(void* list, void* node) {
  // **The argument is the list, not the loader.** The only caller hands it
  // `&x48_pakLoadingList`: retail's `AsyncIdlePakLoading` does `mr r4,r29` /
  // `addi r3,r27,72` / `bl fn_802FD174` at 0x802fcd4c-0x802fcd54, where r27 is `this` and 72 is
  // 0x48. Retail's `do_erase` then works on whatever it is given: it reads the count at `+0x14`
  // of `r3` (0x802fd1e8) and writes it back at 0x802fd1f4 (`Kyoto/CResLoader.hpp`). Casting it to
  // `CResLoader*` and erasing from `self->x48_pakLoadingList` instead walked `list + 0x48` -
  // 0x90 past the start of a 0x70 object - so `x48`'s count never moved,
  // `AreAllPaksLoaded()` stayed false forever, so the pump loop above kept re-moving the same
  // already-loaded entry (`x18+x30` climbing to a four-digit count while the loop never exited)
  // until the boot probe's timeout. Measured before the fix: `verify: all 7 paks loaded but the
  // boot never left the pump (no "Initializing renderer..." after it) - PAK_PUMP FAIL`.
  rstl::list< SPakLoadEntry >* const self = static_cast< rstl::list< SPakLoadEntry >* >(list);
  rstl::list< SPakLoadEntry >::iterator it(
      static_cast< rstl::list< SPakLoadEntry >::node* >(node));
  return self->erase(it).get_node();
}
#endif // TARGET_PC
