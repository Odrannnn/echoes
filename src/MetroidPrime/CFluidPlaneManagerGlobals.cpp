/**
 * The water feature switches, in the translation unit that defines them.
 *
 * **Not a `configure.py` unit, on purpose.** Retail defines these in `CFluidPlaneManager.cpp`,
 * and `config/G2ME01/splits.txt` has no `.text` or `.sdata2` block for that file - its
 * `.text` has no symbol dtk could name (so there is no function to carve) and its `.sdata2`
 * lands in the `auto_11_8041B148_sdata2.o` pool. A `Matching` unit here would have to be linked
 * to be believed, and this file is not one; it exists so that the *readers* of these flags -
 * `CFluidPlaneCPU`, and any port source that wants them - have a definition to link against.
 *
 * `gkWaterEnable` is the byte at `.sdata2:0x8041B7D0`, and it holds 1. Measured, not recalled:
 *
 *     $ build/binutils/powerpc-eabi-objdump -s -j .sdata2 \
 *           --start-address=0x8041B7D0 --stop-address=0x8041B7D4 build/G2ME01/main.elf
 *       8041b7d0 01010000 00000000 40800000 3ecccccd
 *
 * so `.sdata2:0x8041B7D0 = 01` and `0x8041B7D1 = 01` (`gkWaterTurbulence`, which also holds 1);
 * the neighbouring words are `0.0f`, `4.0f`, `0.4f` and `9999.0f` at `0x8041B7D4`..`0x8041B7E0`.
 *
 * It has to live outside `CFluidPlaneCPU.cpp`: mwcceppc constant-folds a `const` whose
 * definition it can see, and with the definition in the same file `CFluidPlaneCPU::RenderCleanup`
 * compiles to no `lbz`/`cmplwi`/`beq` guard at all. Retail's guard is
 * `lbz r0,-27632(r2); cmplwi r0,0; beq <end>` at `0x80132004`, i.e. an `r2`-relative read of the
 * byte - the object carries an `R_PPC_EMB_SDA21` reloc and the linker picks the base register.
 */
extern const bool gkWaterEnable;

const bool gkWaterEnable = true;