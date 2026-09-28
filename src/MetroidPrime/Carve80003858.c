/* Carved out of an unclaimed dtk `auto_*` range by lane `cmain2`.  Every number here is
 * measured: the addresses and sizes come from `config/G2ME01/symbols.txt`, the instructions
 * are the ones dtk emitted into `build/G2ME01/asm/auto_*.s` and the ones
 * `build/binutils/powerpc-eabi-objdump -d build/G2ME01/main.elf` shows, and the body below
 * is the C those bytes are the compilation of.
 *
 * .text 0x80003858..0x8000387C, 0x24 = 36 bytes, 1 function:
 *
 *   fn_80003858    0x80003858  0x24
 *     stwu r1,-16(r1) ; mflr r0 ; lfs f1,-32768(r2) ; stw r0,20(r1) ; bl fn_8032194C
 *     lwz r0,20(r1) ; mtlr r0 ; addi r1,r1,16 ; blr
 *
 * **This is `docs/research/boot_path.md` step 21g** - "`fn_80003858`, 0x80003858, 0x24,
 * missing, unidentified".  It is not unidentified any more, and the one caller settles which
 * step it is: `grep 'bl +80003858' build/G2ME01/main.elf` returns exactly one hit, at
 * **0x80006368**, which is inside the frame loop `boot_path.md` puts at 0x80006034-0x80006354
 * and immediately after it - i.e. `CMain::RsMain`'s own code, three instructions past where
 * the loop's documented range ends.  So it is part of the retail entry point, not of a
 * subsystem it happens to sit next to.
 *
 * What the 36 bytes are:
 *
 *   - `lfs f1,-32768(r2)` with r2 = **0x804223C0** (`_SDA2_BASE_`, `tools/sda.py`) lands on
 *     **0x8041A3C0**, which `symbols.txt:21431` names `lbl_8041A3C0` and types `data:float`.
 *     Its bytes are `3c888889` = **0.016666668f**, i.e. 1/60 - the frame tick, the same
 *     constant `CGameArchitectureSupport::UpdateTicks` hard-codes as `0.016666668f`.
 *   - the callee `fn_8032194C` (0x8032194C) opens with `fmr f31,f1` and then
 *     `lwz r4,-29016(r13) ; lwz r3,-29012(r13) ; cmpw r4,r3 ; ble`, so it takes the float and
 *     compares two globals - a tick-driven streaming step.
 *   - the incoming argument is **never read**.  `r3` is not touched between the prologue and
 *     the call and `f1` is overwritten before the call, so the parameter exists in retail's
 *     signature and is unused here.  The `stwu`/`mflr`/`stw` frame is the ordinary
 *     non-leaf prologue; a call to a function that does not call back would not need one.
 *
 * Why it is a `.c`: retail names this symbol nothing (`symbols.txt:51` carries the
 * `fn_80003858` placeholder) and objdiff pairs by name, so a C++ definition would mangle to
 * `_Z13fn_80003858f` and pair nothing - the unit would silently score 0/0.
 *
 * Its own unit because a unit may not claim two discontiguous ranges in one section
 * (`dtk dol split` fails with "Cyclic dependency ... link order").  **The range starts at
 * 0x80003858, which is exactly where `Runtime/MetroTRKConsoleStubs.cpp`'s `.text` ends**
 * (0x80003840-0x80003858) - the adjacency `FACTS.md`'s third carve trap describes.  It
 * builds and `flip_test`s green, so the trap is about *ordering*: the entry has to go in
 * `splits.txt` and `configure.py` between that unit and `MetroidPrime/CMainResetGameState.cpp`
 * (0x80003A48), in address order, which is where it is.
 */

/* 0x8041A3C0, .sdata2, 4 bytes: `3c888889` = 0.016666668f (1/60).  `data:float` in
 * `symbols.txt`; declared, not defined, because retail's own object supplies the bytes in the
 * DOL link and the port cannot have retail's .sdata2 pool. */
extern const float lbl_8041A3C0;

/* 0x8032194C.  Unnamed in retail and unwritten; the `Matching` unit needs the relocation,
 * the port's link grows by this one symbol. */
#ifdef __MWERKS__
/* Upstream: `CStreamAudioManager::Update(float)`. The host keeps the address name. */
#define fn_8032194C Update__19CStreamAudioManagerFf
#endif
extern void fn_8032194C(float f);

void fn_80003858(float f) { fn_8032194C(lbl_8041A3C0); }
