# match-cparticleswoosh — 2026-10-01

`kind: match`, target `Kyoto/Particles/CParticleSwoosh`. Result: **PARTIAL**, judged by
`./tools/goal_check.sh build/goal/item.json` — flip fails, target rose 53 -> 54 / 56.
Unit stays `NonMatching`.

## What I did

Matched `fn_802E82C4`, the unit's worst function at 0.00%, by declaring it.

`src/Kyoto/Particles/CParticleSwoosh.cpp` (new, at the end of the file):

```c
extern "C" void fn_802E82C4(int* self) { self[1] = 0; }
```

It is retail `.text` offset 0 of this unit (`config/G2ME01/splits.txt:2238`,
`.text start:0x802E82C4 end:0x802ED1D0`), so mwcceppc's reverse source order puts the
definition **last** in the file. `config/G2ME01/symbols.txt:13323` names it
`fn_802E82C4`, size 0xC — the only unnamed function the unit defines.

Its three instructions:

```
802e82c4: 38 00 00 00  li      r0,0
802e82c8: 90 03 00 04  stw     r0,4(r3)
802e82cc: 4e 80 00 20  blr
```

Verified byte-for-byte against our object:

```
$ build/binutils/powerpc-eabi-objdump -d --start-address=0x60 --stop-address=0x6c \
      build/G2ME01/src/Kyoto/Particles/CParticleSwoosh.o
00000060 <fn_802E82C4>:
      60:	38 00 00 00 	li      r0,0
      64:	90 03 00 04 	stw     r0,4(r3)
      68:	4e 80 00 20 	blr
```

### Why `int*` and not a named class

The function has exactly one caller in the whole DOL, at `0x8032D334` inside
`fn_8032D284` (an unnamed function in the `auto_03_8032C14C_text` unit, next to
`CParticleSpawnSystem`'s `ForceSet*` methods):

```
8032d330: 38 7f 00 2c  addi    r3,r31,44
8032d334: 4b fb af 91  bl      802e82c4 <fn_802E82C4>
8032d338: 48 00 00 18  b       8032d350
```

`r31` is that function's `this`; it passes `this + 0x2C`, and the loop it was just
walking (`lwz r0,48(r31)` / `lwz r3,56(r31)` / `slwi r0,r0,2`) treats `this + 0x30`
as a count, so the store zeroes that count. That offset is inside
`CParticleSpawnSystem`'s opaque `x20_[0x200]` blob
(`include/Kyoto/Particles/CParticleSpawnSystem.hpp:57`), so this port has no class to
name it. `int*` reproduces the store with no invented type; the reason is in a comment
at the definition.

This is a real out-of-line definition doing real work, not a stub: it is what retail
calls and what the linked DOL needs.

## Measured

```
$ ./tools/fast_try.sh Kyoto/Particles/CParticleSwoosh
main/Kyoto/Particles/CParticleSwoosh: 99.93% fuzzy, 87.09% matched code, 54/56 functions
    96.06%     208 B  __ct__...SSwooshDataFRC9CVector3fRC9CVector3fffibRC12CTransform4fRC9CVector3fffRC6CColor
    99.71%    2404 B  Render2SidedNoSplineNoGaps__15CParticleSwooshFv
```

Before: `99.87% fuzzy, 87.03% matched code, 53/56`. `fn_802E82C4` moved 0.00 -> 100.00
and **no function anywhere got worse** (diffed `build/report.base.json` against
`build/report.json` per function: 1 better, 0 worse).

Gates, all clean:

```
$ ./tools/decomp_build.sh | grep '^All:'
All:  32.82% fuzzy, 25.63% matched, 12.06% linked (11431 / 28465 functions)
$ sha1sum build/G2ME01/main.dol
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
$ ./tools/probe_sources.sh | tail -1
probe: 752 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)
$ python3 tools/check_symbol_names.py | tail -1
checked 514 units; 0 declared names are missing from their object
```

Judge:

```
$ ./tools/goal_check.sh build/goal/item.json
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11430 -> 11431   linked 5565 -> 5565
  ok    target rose: main/Kyoto/Particles/CParticleSwoosh: 53 -> 54 / 56 functions
  flip  flip_test Kyoto/Particles/CParticleSwoosh.cpp: FAIL - judged below as partial progress
goal_check: PARTIAL match-cparticleswoosh
```

## What still stops the flip

Two functions, both at 99.9x-99.7x and both **pure register allocation** — the
instruction sequence is identical, only register numbers and one scheduling slot move.

I diffed these with a per-instruction retail-vs-ours comparison (a throwaway script in the
gitignored `.tmp/`, driving `powerpc-eabi-objdump -d -r` over both
`build/G2ME01/obj/<unit>.o` and `build/G2ME01/src/<unit>.o` and pairing by symbol name),
which prints the first differing instruction index for each. Branch targets are normalised
away, since they move whenever an earlier function in the object changes size — which is
why `objdiff`'s own 96%/99.7% figures and the raw instruction diff disagree about which
lines differ.

### `__ct__...SSwooshData...` — 96.06%, 3 instructions, indices 19/20/21

```
retail: lfs f4,0(r5) / stfs f6,12(r3) / addi r3,r28,56
ours:   addi r3,r28,56 / lfs f4,0(r5) / stfs f6,12(r28)
```

Same three instructions, same registers-worth of work; ours hoists the `addi` that
forms `&mOrientation` one slot earlier and then has to store through `r28` instead of
`r3`. It is a scheduler decision, not a source difference.

Spellings tried, all scored by rebuild (`fast_try.sh`):

| spelling | score |
|---|---|
| baseline (as in the tree) | 96.06% |
| `mOrientation` first in the init list | 96.06% (same 3 insns) |
| `mOrientation` last in the init list | 96.06% (same 3 insns) |
| scalars first (`mActive`,`mInitialRot`,`mRotm`,`mStartFrame`,`mLeftRad`,...) | 96.06% (same 3 insns) |
| `mColor` first | 96.06% (same 3 insns) |
| `mUseOffset(mOffset)` instead of `mUseOffset(offset)` | **90.10%** — worse |
| `mFrame(0)` added to the init list | **92.21%** — worse, and mFrame is not in retail's store list |
| `mOrientation(CTransform4f(orient))` | **90.06%** — worse |
| `{}` instead of the inline ctor body | 96.06% |

Init-list order does not move it at all, which is itself the finding: mwcceppc emits the
stores in **declaration** order, so reordering the initialiser list cannot reach a
different schedule. Only the declaration order in
`include/Kyoto/Particles/CParticleSwoosh.hpp:45-59` can, and that order is fixed by
retail's field offsets (0, 4, 8, 12, 24, 36, 48, 52, 56, 108, 112, 116, 120, 124 —
all corroborated by the copy ctor at `0x802E8BFC`, which is at 100%).

### `Render2SidedNoSplineNoGaps` — 99.71%, 17 instructions, register renaming only

Every remaining difference is a float-register number: ours uses `f28` where retail uses
`f27` (indices 297/303/311/319/326), then `f27` where retail uses `f31` (328/329/334),
repeated for the second block at 485-547. Same mnemonics, same operands otherwise, and
`r1`-relative stack slots match — no offset differs any more.

The one change that moved it: hoisting the `camToParticle - swoosh.mTranslation`
sub-expression into a named local took it 99.71% -> **99.78%** and cut 62 differing
instructions to 17 (the rest were `r1+116` vs `r1+128`-style stack-slot offsets, now
gone). That spelling is a real, behaviour-preserving source change, but it did **not**
raise `matched_functions`, so I reverted it rather than carry a diff the item does not
need — see the reviewer note below.

Spelling tried:

| spelling | score |
|---|---|
| baseline | 99.71%, 62 differing insns |
| `const CVector3f toParticle = camToParticle - swoosh.mTranslation;` as a named local | 99.78%, 17 differing insns |

## Caveats for the next run

- **The 96.06% ctor is not a spelling problem.** Six init-list orderings produce byte-identical
  output. The schedule would have to change through something other than initialiser order.
  A different route to the same stores (e.g. assigning in a constructor *body* after the
  mem-init list, so `mOrientation` is written by an assignment rather than a mem-init) is
  the untried lever; it would change the member-init semantics and needs care.
- **The `.text` also carries 1024 bytes of weak/COMDAT code retail does not emit** — 16
  symbols (`__dt__*`, `reserve`/`clear`/`uninitialized_copy` for `vector<CVector3f>`, the five
  inline `CParticleGen` virtuals). `tools/unit_fit.sh Kyoto/Particles/CParticleSwoosh.cpp` flags
  all 16 as `extra`, and by its own guidance these are the harmless kind: inline virtuals and
  template instantiations that both the retail linker and mwldeppc discard (`CAi` carries 224
  bytes of these and still flips). So they are probably not the flip's cause. I ran
  `check_decl_order.py --unit Kyoto/Particles/CParticleSwoosh`: `ok, none emits its functions out
  of retail order`, so the permuted-object trap is also ruled out. That leaves the two
  sub-100% functions as the whole story.
- **`mFrame` is not in the 11-argument ctor's store list.** Retail's ctor writes offsets
  0..52 and 108..124 but never 104 (`mFrame`), i.e. `mFrame` is left uninitialised
  there. Adding `mFrame(0)` scores worse (92.21%) and is not retail's behaviour. Worth
  recording: do not "fix" it.
- **`fn_802E82C4` belongs to no class this port names.** If someone later identifies the
  subobject at `CParticleSpawnSystem` + 0x2C, the `int*` signature should become that
  type and the declaration should move to its own header.

No `NEW:` items filed: the two remaining functions are a measured wall (spellings and
scores above), not new work, and this unit is already queued.