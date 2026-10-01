# progress-unit-cenvfxmanager (kind: progress, target `main/MetroidPrime/CEnvFxManager`)

Unit stayed `NonMatching`. **Matched functions 12 -> 17 / 59** (measured, `build/report.json`
against `build/goal/judge/report.base.json`). Unit fuzzy 11.4584% -> 13.3753%, matched code
4.3897% -> 7.1429%. Project matched 11545 -> 11550 / 28465; DOL sha1 unchanged
(`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`); `probe_sources.sh` 754 files 0 failed;
`check_symbol_names.py` 0 missing. `tools/goal_check.sh build/goal/item.json` -> **PASS**.
One file changed: `src/MetroidPrime/CEnvFxManager.cpp` (no `asm`, no config, no carve).

## Per function

| function | before | after | what it took |
|---|---|---|---|
| `SetupDefaultTevSwapMode` (44 B) | 9.09% | **100%** | body from Prime 1 donor: `GXSetTevSwapMode(GX_TEVSTAGE1, GX_TEV_SWAP0, GX_TEV_SWAP0)`; added `#include "dolphin/gx/GXTev.h"`. Retail is `li r3,1 / li r4,0 / li r5,0 / bl GXSetTevSwapMode`. |
| `CalcRainVolume` (72 B) | 82.83% | **100%** | the else-branch shared the tail via a `result` local. Retail emits `b` to one `fctiwz/stfd/lwz` epilogue; two `return`s duplicated it. |
| `GetParticleBoundsToWorldScale` (68 B) | 80.00% | **100%** | retail `lfs f4,-24032(r2)` = **0.007874015718698502 = 1/127** and `fmuls` three times, i.e. a multiply by a pooled reciprocal, not `fdivs` by 127. `* (1.f / 127.f)` reproduces it. |
| `UpdateUnderwaterParticles` (352 B) | 49.64% | **100%** | **the only change that mattered was `const short zVal = zVec.mZ;`** hoisted out of the loops, plus writing `mGrids[i].mParticles[j]` at both use sites. Measured: `short zVal = zVec.GetZ()` -> 42 diff insns; `int` -> 42; `const short zVal = zVec.mZ` -> **0**. Same body otherwise. The *non*-`const` and the accessor spelling both allocate the callee-saved register differently. |
| `BuildBlockObjectList` (184 B) | 2.17% | **100%** | Prime 1's donor carries a `CScriptWater` fallback; **retail does not** - `tools/dis.sh 0x80161FF0 0xB8` goes straight from the failed `kTFL_BlockEnvironmentalEffects` test (`rlwinm. r0,r0,0,11,11` = bit 11 of `CScriptTrigger::mFlags` at `+0x1a0`) to the loop increment. Dropped the water branch, used `mgr.GetObjectListById(kOL_All)` (`m_objectLists[0]` at `+0x810`), `GetFirstObjectIndex`/`GetNextObjectIndex` (`+0x2008` / `mObjects[idx].mNext`), `TCastToConstPtr<CScriptTrigger>`, and `push_back`. |
| `UpdateSnowParticles` (392 B) | 48.24% | **68.16%** | partial - see below. |

## UpdateSnowParticles: what retail actually does (measured, not inferred)

`tools/dis.sh 0x80164708 0x188`. `UpdateDriftingParticles` at `0x80164580`, 392 B, is
**byte-for-byte identical** to it (0 differing bytes over 98 instructions) - so it is the same
body, and both stay unmatched.

Retail's inner loop, with `k` = `clrlwi` force index and `r12 = r4 + k*6`:

```
lha r6,4(r12)   p.mX += f[k].mZ
lha r9,6(r12)   p.mY += f[k+1].mX
lha r8,8(r12)   p.mZ  = (f[k+1].mY + p.mZ) & 0x3fff
```

so the index is advanced **between** the `mX` update and the `mY`/`mZ` updates - a real
retail quirk (each particle mixes force `k` and force `k+1`). The body I wrote encodes exactly
that and reaches 68.16%. The outer frame is also `stwu r1,-48(r1)` + `stmw r27,28(r1)`
(five callee-saved registers: force index, grid pointer, grid stride index, byte offset), where
mine still uses two - that is the whole of the remaining 32%.

**WALL: UpdateSnowParticles 68.16% - instruction order and schedule match retail's, the five-register frame does not; every spelling tried in this run produces a 32-byte frame and the same 104 differing instructions.**

Spellings tried, all compiled with the exact `build.ninja` cflags line and diffed
instruction-by-instruction against retail:

| spelling | diff insns |
|---|---|
| baseline (`CEnvFxManagerGrid& grid`, `uint force`, `particle += snowForces[force]`) | ~200 |
| `int forceIdx`, `GetVisibility().first`, `p.mX += f[k].mZ; p.mY += f[k].mX; p.mZ = (f[k].mY + p.mZ) & 0x3fff;` | **104** (kept) |
| `uint forceIdx` | 108 |
| `short forceIdx` | 105 |
| `const int forceIdx` | compile error (`const` local in the C++/EC++ dialect) |
| increment placed **between** the `mX` and `mY` updates | 104, but 62.04% fuzzy - worse than kept |
| `snowForces[k+1].mX / .mY` spelled with an explicit `+1` | 107 |
| `CVectorFixed8_8* f = &snowForces[k]; f[0].mZ / f[1].mX / f[1].mY` | 100 |
| `const CVectorFixed8_8& f = snowForces[k]` | 100 |
| `CVectorFixed8_8 f = snowForces[k]` (by value) | 103 |
| `p += snowForces[k]; p.mZ &= 0x3fff;` via `operator+=` | 113 |
| `rstl::vector<CVectorFixed8_8>& parts = mGrids[i].mParticles` hoisted | 100 |
| fully inline `mGrids[i].mParticles[j].mX += ...` at every use | 110 |
| `CEnvFxManagerGrid& grid = mGrids[i]` | 100 |

Two codegen rules this unit pays for (general, not GameCube-specific):

- **A hoisted `const short` from a member of a reference parameter is what mwcceppc wants.**
  `UpdateUnderwaterParticles` is a pure-register-allocation miss (42/88 instructions, every
  one of them the same add/load with a different register) until the local is `const` and read
  as `.mZ` rather than through `GetZ()`. Same statement, same semantics, byte-exact.
- **`operator+=` is not free.** The `CVectorFixed8_8::operator+=` spelling emits 113
  instructions where the three explicit component updates emit 104.

## Considered and not attempted

- `SetSplashEffectRate` (184 B, 2.17%): needs `CHUDBillboardEffect`, which this repo does not
  have (`include/` has no match). Retail reads `mEnvRainSplashIds` at `+4972` (count) and
  `+4976` (data, `lhz`), casts the `ObjectById` result with
  `TCastToPtr<CHUDBillboardEffect>` (type id 28 = `li r4,0x1c`), tests a bool at `+0x178`
  (`rlwinm. r0,r0,27,31,31`), then virtual slot `+0x34` of the object at `+0x158`.
  Declaring a whole new actor class to match this is a different item.
- `SetupRender` (464 B, 7.46%): fully disassembled to `0x801672C8` and needs
  `CVector2i` -> float via the 8.8 `xoris 32768` trick plus a 10-word `.rodata` constant pair
  loaded into `GXLoadTexMtxImm`'s matrix - `lbl_803B3A40`-ish, not ours to guess.
- `fn_80168210` / `fn_80168258` / `fn_80168278` / `fn_801682A0` / `fn_80168334` (72/32/40/148/184 B,
  all 0%): these are `CEnvFxManagerGrid`'s destructor chain
  (`rstl::vector<CVectorFixed8_8>::operator=`, grid copy-assign, guarded grid destroy). They
  are retail `fn_` placeholders in `config/G2ME01/symbols.txt`, so a C++ definition mangles and
  objdiff pairs nothing - matching them needs the real class members named first, which changes
  a shared header.
- `__ct__13CEnvFxManagerFv` (1236 B, 96.93%): 1236 vs 1228 bytes, and the divergence starts at
  `+0x18` where retail hoists the `gpSimplePool` pointer into a callee-saved register and loads
  three floats from one base (`lis r4,17200 / addi r7,r4,29872 / lfs f0,0(r7) / lfs f2,4(r7) /
  lfs f2,8(r7)`) where ours reloads each literal separately. Register-hoisting shape in the
  initialiser list; not a spelling I could pin down.