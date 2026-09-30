# cmorphball-getballtoworld-inline

`MetroidPrime/Player/CMorphBall` **49 -> 50 matched functions** (158 total), unit fuzzy
**17.646% -> 17.758%**, unit `matched_code` 3800 -> 3944 of 66600. Global
`matched 10010 -> 10011`, `fuzzy 30.828823% -> 30.829964%`. The unit stays `NonMatching`.

One function, one file: `src/MetroidPrime/Player/CMorphBall.cpp` (+9/-1). No header, no class
layout, no `configure.py` / `splits.txt` / `files.cmake`, no `asm`.

## What retail does, and the spelling that reaches it

`GetBallToWorld__10CMorphBallCFv`, target `0x800CB764`, 144 bytes. Retail's object is
`build/G2ME01/obj/MetroidPrime/Player/CMorphBall.o` (objdiff's `target_path`) - the branch that
matters, because `build/G2ME01/src/...` is *ours*. Four calls, in this order:

    GetRotation(out=r1+20,  this=mPlayer+0x24)      # mPlayer.GetTransform().GetRotation()
    Translate  (out=r1+68,  in=r1+8)                 # r1+8 is the ball position, built inline
    __ml__     (out=r1+116, a=r1+68, b=r1+20)        # operator*
    __ct__     (out=sret,   in=r1+116)               # copy the product into the return slot

Two facts fall out of that, and both were needed:

1. **The ball position is inlined, not called.** `lfs f1,12(r4)` is `this+12` = `mRadius`, and
   the three `fadds` are the same 12 instructions as `GetBallPosition` (`0x800CB730`, already
   100%), so `GetBallPosition()`'s body is written out here. Note the `mRadius`, not
   `GetBallRadius()`: retail never calls into `CTweakBall` from here.
2. **RVO did not apply to the return.** The product is materialised at `r1+116` and
   copy-constructed into sret, and the frame is 176 = `0x8` vector + `0x14` rotation +
   `0x44` translate + `0x44` product. A plain `return <class expr>;` **does** get RVO'd into
   sret by mwcceppc, which drops the last copy and shrinks the frame - that is the whole of the
   remaining gap in spelling 2 below. `return CTransform4f(<expr>);` blocks it, because the
   temporary is then a real argument to the copy constructor.

`../prime-ref` (the matched Prime 1 decomp) already spells it that way, and its shape maps onto
Echoes one-for-one (`GetBallRadius()` -> `mRadius`, everything else identical):

    CTransform4f CMorphBall::GetBallToWorld() const {
      const CTransform4f& playerXf = mPlayer.GetTransform();
      const CVector3f ballTranslation(0.f, 0.f, mRadius);
      const CVector3f& translation = mPlayer.GetTranslation();
      const CVector3f ballPos = translation + ballTranslation;

      return CTransform4f(CTransform4f::Translate(ballPos) * playerXf.GetRotation());
    }

`mPlayer+0x54/0x58/0x5c` (84/88/92) is `mPosition`, what `CActor::GetTranslation()` returns -
`mPlayer.GetTransform()` is at `+0x24`, and retail's `addi r0,r5,36; mr r4,r0` agrees.

## Spellings measured, so the next run does not repeat them

All are the same file, same unit, scored by `./tools/decomp_build.sh MetroidPrime/Player/CMorphBall`:

| # | spelling | score | what is left |
|---|---|---|---|
| 0 | baseline: `return CTransform4f::Translate(GetBallPosition()) * mPlayer.GetTransform().GetRotation();` | 48.17% | two `bl`s; retail has none |
| 1 | position expression spelled out, direct `return A * B;` | 85.92% | frame 128 vs 176; `__ml__` RVO'd into sret, retail's final `__ct__` missing |
| 2 | 1 + `CTransform4f ballToWorld = A * B; return ballToWorld;` | 91.47% | frame 224: MWCC NRVO's the local *and* still copies twice |
| 3 | 1 + `const CTransform4f& ballToWorld = A * B; return ballToWorld;` | 94.44% | 36/36 instructions, frame 176 - only `mr r3,r31` / `addi r4,r1,116` swapped in the last `__ct__` |
| 4 | Prime 1's spelling with `return CTransform4f(...)` | **100.00%** | nothing |

`const` on the local makes no difference to 2 (both 91.47%). The instruction-level comparison
was done with `objdump -d --disassemble=<sym>` on both objects, offset-stripped and diffed.

## Verified

- `GetBallToWorld__10CMorphBallCFv` **48.166668% -> 100.0%**; the two disassembly dumps are
  now identical instruction for instruction.
- Per-function diff of the whole report before/after (`build/report.json`, 12234 scored
  functions both sides): **0 worse, 1 better, 0 new, 0 gone**. The one better entry is
  `GetBallToWorld__10CMorphBallCFv`. `GetBallPosition` stays 100%, `GetSwooshToWorld` stays
  89.40% (unchanged, see below).
- `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
- `python3 tools/check_symbol_names.py` -> `checked 503 units; 0 declared names are missing`.
- `python3 tools/check_decl_order.py --unit main/MetroidPrime/Player/CMorphBall` -> "would break
  on a flip", byte-identical before and after (checked by stashing); the unit is 50/158 and is
  not going near a flip.
- `python3 tools/check_docs_claims.py` -> `docs claims agree with the tree`.
- `./tools/unit_fit.sh MetroidPrime/Player/CMorphBall.cpp` -> 50 functions present in ours but
  not in retail's object, pre-existing scaffolds, unchanged by this item.

## Left next to it (measured, not fixed here)

`GetSwooshToWorld__10CMorphBallCFv` is 89.40% for **exactly the same reason** - ours RVO's the
final `operator*` into sret. Retail has 5 temps (frame 272 vs our 224) and the tail
`__ml__(216, 168, 24); __ct__(216, 216); __ct__(sret, 216)`: it wraps the *intermediate*
product in a real `CTransform4f` too, hence the `120 -> 168` copy that we do not have. It was
left alone because it is a different function from the one this item names, and the recipe
("`return CTransform4f(...)`, plus one more explicit wrap on the first product") is a separate
edit to the same file.

NEW: cmorphball-getswooshtoworld-rvo | progress | MetroidPrime/Player/CMorphBall |
`GetSwooshToWorld` is 89.40% only because ours RVO's the last `operator*` into sret; retail
materialises it (`0x800CB7F4`, frame 272, tail `__ml__`+two `__ct__`). Same fix as
`GetBallToWorld`: wrap the products in `CTransform4f(...)` so the temporaries are real, the
first one included. Recipe and exact instruction diff are above.
