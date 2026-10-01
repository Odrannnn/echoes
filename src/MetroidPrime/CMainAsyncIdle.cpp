/**
 * `MetroidPrime/main.cpp`'s lower half - retail `.text:0x800053B8-0x80005C64`, 0x8AC = 2,220
 * bytes, eleven functions - split off so that `CMain::AsyncIdle` can be worked as its own
 * range rather than as one function inside a 12,500-byte unit.
 *
 * The cut is at 0x80005C64 because that is where `CMain::AsyncIdle` (0x80005B44, 0x120 = 288
 * bytes) ends and `CMain::SetFrameTimeMinimum` (0x80005C64, 8 bytes) begins; `main.cpp` now
 * starts there and `CMain::RsMain` (0x80005C6C) is its second function. A unit may claim only
 * one range per section (`dtk dol split`: "Cyclic dependency ... link order"), so
 * `CMain::AsyncIdle` could not become a unit of its own without a cut somewhere in this range,
 * and a cut at 0x80005C64 is the narrowest one that reaches it.
 *
 * ## The split moved eleven functions, and it cost exactly what the thirty-eight-function
 * ## split cost - because the cost is not the function count
 *
 * `mainTail.cpp` -> `CMainShutdownSubsystems.cpp` moved **one** function and cost nothing
 * (`CMain::ShutdownSubsystems` went 1.47% -> `Matching` 100.00%). The three-way `main.cpp` cut
 * that would have made `CMain::RsMain` and `CMain::CheckReset` carvable moved **38** and cost
 * `__ct__24CGameArchitectureSupport` 93.10% -> 87.99% and `AddPaksAndFactories`
 * 57.15% -> 57.04%. `docs/HANDOFF.md` attributed that cost to the *number of functions
 * changing units*, and declined the cut on that basis.
 *
 * **This cut moves eleven and pays the identical price, to the identical decimal:**
 *
 *     SPLIT   main/MetroidPrime/main: 11 function(s) moved into
 *             main/MetroidPrime/CMainAsyncIdle (exact count match - a split, not a loss)
 *     WORSE   main/MetroidPrime/main :: AddPaksAndFactories 57.15% -> 57.04%
 *     WORSE   main/MetroidPrime/main :: __ct__24CGameArchitectureSupport 93.10% -> 87.99%
 *
 * So the axis is wrong, and here is the axis. The difference is **where `operator new`'s
 * `"??"` placement string lands in mwcceppc's `@stringBase0` pool**, and the pool is ordered by
 * *first use in the order the functions are emitted*, which is **ascending by address** (mwcceppc
 * emits in reverse source order and the source is descending by address). Both `.rodata`s were
 * dumped with `objdump -s`:
 *
 *   before:  0000 "ShotSmoke"  000a "Power2nd_1"  0015 "??(??)"  001e "%d"  0024 ".pak" ...
 *   after:   0000 "ShotSmoke"  000a "Power2nd_1"  0015 "Strings.pak" ... 008e `"??"(??)`
 *
 * Before the split, `CMain::StreamNewGameState` (0x800053B8) is the lowest-address function in
 * the unit and therefore the first emitted, and its `new CGameState(...)` is the first literal
 * user - so `"??"` is at pool offset 0 and retail's two-instruction
 * `lis r3,0 / addi r4,r3,0` comes out. After the split the lowest-address function is
 * `CMain::SetFrameTimeMinimum` (0x80005C64), the first *literal* user becomes
 * `CGameGlobalObjects::AddPaksAndFactories` (0x80007168) with its thirteen pak names, and `"??"`
 * is last, at offset 0x76 - so each of the constructor's four `new` sites grows a third
 * instruction, `addi r4,r3,118`. Those four instructions are the whole 5.11%.
 *
 * Two consequences worth writing down. First, **no cut that moves
 * `CMain::StreamNewGameState` out of `main.cpp` is free**, whatever its width: the function
 * that owns the first `operator new` is the thing whose unit the pool depends on. Second,
 * **`docs/HANDOFF.md`'s ordering - model the two frame-time histories, write `fn_800069AC`,
 * then split - should be re-read**: the `CMain::RsMain` split is not made expensive by moving
 * 38 functions, and doing the order in the documented order will not avoid the cost.
 *
 * The cost is **matching fidelity, not behaviour**: both units are `NonMatching`, so
 * `dtk dol split` supplies retail's bytes for the whole range either way and the linked DOL is
 * byte-identical (`main.dol` sha1 `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` before and after).
 * What is measured is objdiff's comparison of the two *unlinked* objects, and the four extra
 * `addi`s exist only in an object the link never uses.
 *
 * `.ctors` and `.sbss` stay on `mainTail.cpp`, which is the last unit of `main.cpp`'s original
 * three-way cut; this range has neither, so nothing moved.
 *
 * The globals these four functions read - `gpResourceFactory`, `gpTweakGame`, `gpGameState`,
 * `gpSimplePool` and retail's `.rodata` string pool `lbl_803A56C0` - are **declared, not
 * defined**, for the reason `mainTail.cpp`'s header gives at length: this unit is
 * `NonMatching`, so nothing in it can be what the **port** links, and a definition here would
 * be a duplicate of `main.cpp`'s the moment both files are in the port build - which the gate's
 * `port link dups` step, and only that step, can see.
 *
 * ## What this buys the port, which is nothing yet, and why
 *
 * `linked` did not move and cannot move from this split. `tools/report_diff.py` computes it as
 * the sum of `matched_functions` over units objdiff marks *complete*, so a `NonMatching` unit
 * contributes nothing however good its individual functions are - and this unit is 1 of 11
 * functions, because `CMain::StreamNewGameState` is 25.26% and seven COMDATs are unpaired.
 * `matched` is unchanged at 3973 for the same reason: `CMain::EnsureWorldPaksReady`'s 100%
 * moved with the split, and `CMain::AsyncIdle` is one instruction short of the 100% that would
 * replace it. **Nothing in the port calls `CMain::AsyncIdle`**: `src/MetroidPrime/PortBoot.cpp`
 * gives `CMain::RsMain` a host body that returns immediately, and the frame loop behind
 * `AsyncIdle` is unreachable (boot_path.md's wall, step 17). What the split buys is the ability
 * to work this function against a 2,220-byte object instead of a 12,500-byte one, which is what
 * took it from 85.94% to 99.17%.
 *
 * The eleven functions are the four below plus seven **COMDAT weak copies** that mwcceppc
 * emitted into retail's object for this range and that no source here defines:
 * `__dt__15CMemoryInStreamFv` (0x800055CC, 96 B), `SomethingWorldId_80005698` (0x80005698,
 * 208 B), `__dt__vector_80005768` (0x80005768, 132 B), `__dt__800057EC` (0x800057EC, 96 B),
 * `vector_copy_8000584C` (0x8000584C, 204 B), `__dt__80005918` (0x80005918, 80 B) and
 * `rstl::operator+` (0x80005AE8, 92 B). `tools/unit_fit.sh MetroidPrime/CMainAsyncIdle` prints
 * the residue.
 */

#include "MetroidPrime/CMain.hpp"

#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/Basics/CBasics.hpp"
#include "Kyoto/CPakFile.hpp"
#include "Kyoto/CDvdFile.hpp"
#include "Kyoto/CResFactory.hpp"
#include "Kyoto/CSimplePool.hpp"

#include "MetroidPrime/CGameGlobalObjects.hpp"
#include "MetroidPrime/Player/CGameState.hpp"
#include "MetroidPrime/Tweaks/CTweakGame.hpp"

// Retail's `.rodata` string pool, 0x803A56C0, 0x1C0 bytes. `CMain::AddWorldPaks` reaches its
// pak names and its ".pak" suffix as `lbl_803A56C0 + <offset>` through a single
// `R_PPC_ADDR16_HA`/`R_PPC_ADDR16_LO` pair, which is what a literal in *this* unit would not
// produce - mwcceppc routes a literal through its own `@stringBase0` pool. `main.cpp` declares
// the same symbol for the same reason.
extern "C" const char lbl_803A56C0[];

// Retail 0x80005B44, 0x120 = 288 bytes. The frame loop's per-iteration pump: it averages the
// last ten frame times, writes this one into the ring, clamps it against `mFrameTimeMinimum`,
// and hands it to `CResFactory::AsyncIdle` unless it is zero. **99.17%, up from 85.94%**, and
// one instruction from byte-exact.
//
// **The callee does not exist, and it is not what is left.** `CResFactory::AsyncIdle` is
// retail `.text:0x802FA384`, 0x10C = 268 bytes, and `docs/research/boot_path.md` row 21e
// lists it as this function's blocker. That is a *port reachability* blocker, not a matching
// one: a `Matching` unit needs relocations against retail's own objects, which
// `dtk dol split` supplies for every claimed range, so an unwritten callee costs nothing
// here. What is left is one instruction, and it is measured and not yet explained:
//
//   retail   0x80005C8C  54 60 06 3f  clrlwi r5,r30,24      ; flag & 0xFF
//   ours                 7c a3 1b 78  mr     r5,r30
//
// The whole function is byte-identical otherwise, and the two spellings differ only in the
// conversion mwcceppc applies to the `bool` argument of `CResFactory::AsyncIdle`.
//
// `sizeof(bool)` is 1 under these flags - measured with `tools/probe_cc.sh`, not on the host -
// so this is not a bool-width setting. Twenty spellings were measured with
// `tools/try_batch.py` and all of them move away from retail rather than towards it:
//
//   local/argument type                mwcceppc's conversion of the second argument
//   --------------------------------  ------------------------------------------------
//   `bool` (retail's, and ours)        `mr`                            - retail
//   `uchar`, `char`                    `clrlwi`/`extsb` + `neg or srwi`  (mask then normalise)
//   `uint`                             `neg r0,rX ; or ; srwi rX,r0,31`  (normalise)
//   `(uchar)flag`                      `clrlwi` + normalise
//   `flag & 1`                         `clrlwi r5,r30,31`  - `& 1`, not `& 0xFF`
//   `flag ? true : false`              `clrlwi` + normalise
//   `flag == true`                     `clrlwi` + `subfic`
//
// **The one thing that reproduces retail's byte exactly is declaring the parameter `uchar`**
// instead of `bool` - verified, `clrlwi r5,r30,24` and nothing else differs - but that changes
// the callee's mangled name to `AsyncIdle__11CResFactoryFUiUc`, and retail's object defines
// `AsyncIdle__11CResFactoryFUib`, so the DOL link would have an undefined symbol. The
// declaration in `include/Kyoto/CResFactory.hpp:65` is therefore right and the instruction is
// not reachable from the source. **`(uchar)flag` in the argument list is the spelling to try
// first if this is picked up again**, because it is the only one that produces a `clrlwi` at
// that register at all, and the remaining difference is then the normalisation.
//
// ## The two statement decompositions that were worth the search
//
// Twenty-six variants were measured (`tools/try_batch.py`, ranked by differing instructions).
// Twenty of them held at 25 or worse, and the two that mattered were not spellings of the same
// expression - they were different statements:
//
// 1. `uint t = 5000; if (time <= 5000) { t = time; }` scores where
//    `uint t = (time <= 5000) ? time : 5000;` does not, and where
//    `uint t = time; if (t > 5000) { t = 5000; }` does not either. Retail's four instructions
//    are `cmplwi r4,5000 ; li r31,5000 ; bgt ; mr r31,r4` - the `5000` arm is the *fall-through*
//    and `time` is the branch target, which only the initialiser spelling lays out that way.
//    Measured: 11 differing instructions for the ternary, 5 for `t = time; if (t > 5000)`, **1**
//    for the initialiser. The same fact is why the clamped value is a *separate variable* from
//    the parameter: it is the one that has to survive `GetMaxSpeed()`'s call, and retail keeps
//    the parameter in `r4` and the clamp in `r31` where the one-variable spelling puts the
//    parameter in `r31` and adds an `mr`.
// 2. `bool flag = false; if (GetMaxSpeed()) { flag = true; t = 1000000; }` is what produces
//    retail's `li r30,0` before the `mFrameTimeMinimum = 0` store, `li r30,1` in the branch and
//    the `stw r30,8(r1)` spill. `bool flag = GetMaxSpeed();` scores 18 differing instructions
//    because mwcceppc then keeps the value in a volatile register and never spills, so retail's
//    `r30` disappears from the prologue and the epilogue.
void CMain::AsyncIdle(uint time) {
  if (time < 500) {
    uint total = 0;
    for (int i = 0; i < mFrameTimes.capacity(); ++i) {
      total += mFrameTimes[i];
    }
    if (total < 500 * mFrameTimes.capacity()) {
      time = 500;
    } else {
      time = 0;
    }
  }
  mFrameTimes[mFrameTimeIdx] = time;
  mFrameTimeIdx = mFrameTimeIdx + 1;
  if (mFrameTimeIdx >= mFrameTimes.capacity()) {
    mFrameTimeIdx = 0;
  }

  uint t = 5000;
  if (time <= 5000) {
    t = time;
  }
  if (t < mFrameTimeMinimum) {
    t = mFrameTimeMinimum;
  }
  mFrameTimeMinimum = 0;
  bool flag = false;
  if (GetMaxSpeed()) {
    flag = true;
    t = 1000000;
  }

  if (t != 0) {
    gpResourceFactory->AsyncIdle(t, flag);
  }
}

// Retail 0x80005968, 0x180 = 384 bytes. The sixteen world paks, `<pakfile base>0.pak` through
// `15.pak`, each probed on the DVD before it is handed to `CResLoader::AddPakFileAsync`. The
// base name and the "%d" format are both out of retail's string pool.
void CMain::AddWorldPaks() {
  rstl::string basePath = gpTweakGame->GetPakFile();
  for (int i = 0; i < 16; ++i) {
    rstl::string pak =
        basePath + (i == 0 ? rstl::string_l("") : rstl::string(CBasics::Stringize("%d", i)));
    if (CDvdFile::FileExists((pak + rstl::string_l(".pak")).data())) {
      gpResourceFactory->GetResLoader().AddPakFileAsync(pak, false, true);
    }
  }
}

// Retail 0x8000562C, 0x6C = 108 bytes: spin the world paks to their ready phase once, at the
// point the game asks for them rather than every frame.
void CMain::EnsureWorldPaksReady() {
  CResLoader& resLoader = gpResourceFactory->GetResLoader();
  for (int i = 0; i < resLoader.GetPakCount(); ++i) {
    CPakFile& file = resLoader.GetPakFile(i);
    if (file.IsWorldPak()) {
      file.EnsureWorldPakReady();
    }
  }
}

// Retail 0x800053B8, 0x214 = 532 bytes. This is the function that makes the new game state
// reachable, and it is unreachable without `CGameGlobalObjects::AddPaksAndFactories`, because
// the record it reads is inside a pak. The order is measured, block by block, and the
// dependency is a cycle worth stating plainly:
//
//   AddPaksAndFactories (step 13)  ->  gpResourceFactory, gpSimplePool
//   StreamNewGameState  (this)     ->  a record out of a pak, via the factory above
//   CGameArchitectureSupport ctor  ->  gpGameState, which only this function sets
//
// so step 17 faults on a null `gpGameState` until step 13 exists, and step 13 needs the
// sixteen symbols in correction 3 of docs/research/boot_path.md to be reachable at all. The
// full block map, with every CGameState offset retail reads, is in docs/research/paks.md.
//
// What retail does, in order, and what is written here:
//
//   0x800053D0  construct a local at r1+0xB0 from gpGameState+0x54      fn_80005108    (unwritten)
//   0x800053E0  construct a local at r1+0x7C from gpGameState+0x110     fn_80004C90    (unwritten)
//   0x800053FC  construct a local at r1+0x24 from gpGameState+0x188     fn_80004AA0    (unwritten)
//   0x8000540C  flag = (that local's first word != 0)                  written
//   0x80005428  construct a local at r1+0x48 from gpGameState+0x144     fn_80004C90    (unwritten)
//   0x80005438  construct a local at r1+0x14 from gpGameState+0x178     fn_80004AA0    (unwritten)
//   0x80005448  construct a local at r1+0xDC from gpGameState+0x80      fn_80004E84    (unwritten)
//   0x80005458  release the old CGameState (single_ptr::operator=(0))  written
//   0x80005470  gpGameState = 0                                        written
//   0x80005474  pick the record: the r1+0x24 local if flag, else        written (shape)
//               records[r1+0x7C+0x5C] out of an array at r1+0x80,
//               16 bytes each, data at +0x04 and size at +0x0C
//   0x80005494  CMemoryInStream(data, size)                            written (shape)
//   0x800054A0  CBitStreamReader(that stream)                          written (shape)
//   0x800054B4  ::operator new(752, "??(??)..", 0)                     written (shape)
//   0x800054C4  fn_80144140(bitStreamReader) - the CGameState ctor,    (unwritten, 0x684)
//               1,668 bytes (0x684), and the only writer of the fields below
//   0x800054D4  publish it into mGameGlobalObjects' single_ptr          written
//   0x80005500  gpGameState = the new one                              written
//   0x8000550C  copy-assign the r1+0xB0 local into the new +0x54       fn_80003F08    (unwritten)
//   0x80005518  fn_80142FA4(new, r1+0x7C local)                         (unwritten)
//   0x80005524  fn_80142920(new, r1+0x48 local)                         (unwritten)
//   0x80005530  fn_801427DC(new, r1+0x14 local)                         (unwritten)
//   0x80005538  fn_80003D00(&new->x80, r1+0xDC local)                   (unwritten)
//   0x80005550  CGameOptions::EnsureOptions()                          **written**
//   0x80005558  new->x10C = the old x10C, new->x108 = the old x108      written (shape)
//   0x80005568  fn_80142FEC(new), only when the flag is set             (unwritten)
//   0x80005570  six destructors, in reverse order                       (unwritten)
//
// The seventeen `(unwritten)` callees are all unnamed in `config/G2ME01/symbols.txt` and none
// is defined in the port, so writing them as calls would add seventeen symbols to the
// link-gap ratchet and close none. `CGameOptions::EnsureOptions` is the one real behaviour
// in this function that the port can already do, and it is written.
void CMain::StreamNewGameState(CInputStream& in, int saveIdx) {
  mGameGlobalObjects->GameState() = nullptr;
  gpGameState = nullptr;
  mGameGlobalObjects->GameState() = new CGameState(in, saveIdx);
  gpGameState = mGameGlobalObjects->GameState().get();
  // 0x80005550: `gpGameState + 0x80`, and `CGameState::gameOptions` is at +0x80 in
  // include/MetroidPrime/Player/CGameState.hpp - pad1b ends at 0x80. This is the same call
  // CGameArchitectureSupport's constructor makes at 0x800081AC, so it costs the ratchet
  // nothing, and it is the step that turns a freshly-read options block into a usable one.
  gpGameState->GameOptions().EnsureOptions();
  // gpGameState->HintOptions().SetHintNextTime();
}

