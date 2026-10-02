# progress-quartic-solver-rmathutils — Kyoto/Math/RMathUtils, 29/39 → 30/39

`kind: progress`. Lane 2, `goal/lane-2`, worktree `../wt-mp2-goal-L2`. One change, one file:
`src/Kyoto/Math/RMathUtils.cpp`. The unit stays `NonMatching`; `flip_test.sh` was not run.

## Result

`./tools/goal_check.sh build/goal/item.json` → **`goal_check: PASS
progress-quartic-solver-rmathutils`**, with every check ok: no judge-owned path touched,
`gate.sh` (DOL sha1 `6ef9b491…`, all 86 RELs, per-function report diff, module wiring, docs
claims, port probe), `check_symbol_names.py`, `All:` line not fallen, target rose, no `asm`
added.

| | before | after |
|---|---|---|
| `main/Kyoto/Math/RMathUtils` matched_functions | 29 / 39 | **30 / 39** |
| tree matched functions | 12293 / 28465 | **12294 / 28465** |
| tree `All:` fuzzy | 34.70% | 34.73% |
| tree `linked` | 5863 | 5863 (unchanged, correct: the unit is `NonMatching`) |

## The item's target: `fn_802CB918` is now byte-exact

The item's `reason` was that `fn_802CB918` (904 B) was unwritten. It is now **100%, 0 of 226
instructions differing**, against the retail object — that single function is the +1.

It is Prime 1's `CSteeringBehaviors::SolveQuartic`
(`/run/media/odran/Leo/projects/Restored-projects/Chatgpt/prime-ref/src/MetroidPrime/CSteeringBehaviors.cpp:200-286`),
which Echoes moved into `Kyoto/Math`. **That donor file *is* usable for these three functions,
unlike for the rest of this unit** — the note on `progress-unit-rmathutils` says Prime 1's
`RMathUtils.cpp` is not a donor (it stops at `FloorPowerOfTwo`), and that is still true, but the
solvers in Prime 1 live in `CSteeringBehaviors.cpp` and read like retail's bytes. Prime 1 spells
them `bool` over `rstl::reserved_vector`; Echoes takes `(const float* coefficients, float* roots)`
and returns the root count, which is what changes the codegen.

### The signature, read off the caller

`CSteeringBehaviors` at `0x800F9E08` does `bl fn_802CB918` with `r3 = sp+0x18` (five floats) and
`r4 = sp+0x8` (a four-float buffer), then `mtctr r3` / `cmplwi r3, 0` / `ble` / `bdnz` over
`0(r4)`. So the return value is **the number of roots**, not a bool, and the second argument is a
plain `float*`, not a vector. Prime 1's `roots.size() > 0` return and its `push_back` capacity
checks are both absent from retail's object. Retail's `li r31, 0x0` before the call and the
`stfsx fN, r30, r3` with `slwi r3, r31, 2` after it are `roots[count++]`, so `count` is
`unsigned` (`cmplwi`, not `cmpwi`).

### What had to be spelled differently from Prime 1

Measured, each change one build, with the differing-instruction count from
`objdiff-cli diff` (targets normalised for relocation labels and branch displacements):

| spelling | fn_802CB918 |
|---|---|
| first try, Prime 1 verbatim with `float cubic[4] = {…}` | 85.67% |
| `const float` → plain `float` on the array (see below) | 85.67% (no change) |
| aggregate init → four separate `cubic[i] = …` statements | **97.65%** |
| `count` → `unsigned int`, `return count` on the empty-resolvent path | 97.37% |
| derivative and numerator operand order flipped to `coefficients[4] * (4.f * root)` | 97.17% |
| `const int cubicRootCount` for the callee's result, separate from `count` | 98.88% |
| the whole Ferrari block wrapped in `if (cubicRootCount != 0) { … }` | **100.00%** |

Four of these are load-bearing and none is obvious:

- **MWCC 2.7 miscompiles a brace-initialised local `float[4]` whose elements are runtime values.**
  It emits `lis r3, @c@ha / lfs f3, 0x0(r31) / lwz r7, 0x0(r3) / stw r7, 0x28(r1) / …` — it
  materialises the initialiser as a *constant* image and then **also** stores the loaded values
  over it, because it treats the brace list as a static initialiser it can fold. Retail's bytes
  are four `lfs` and four `stfs` and nothing else. Writing the four assignments separately is
  what removes the dead `lwz`/`stw` pair and the `lis`/`addi` it needs to address them. This is
  not a style preference: the aggregate form costs 30 instructions.
- **`unsigned int`, not `int`, for `count`.** Retail compares with `cmplwi`/`cmplwi r31, 0x2`,
  i.e. unsigned. `int` gives `cmpwi`.
- **The resolvent's root count must be a separate variable from `count`.** Reusing `count` for it
  makes MWCC fold the `li r31, 0x0` and then materialise `mr. r31, r3` plus a `slwi` off the
  *same* register, and `if (cubicRootCount == 0) return count;` becomes `cmpwi`/`bne`/`li r3, 0`
  where retail has `cmplwi`/`beq` into the shared `mr r3, r31` epilogue.
- **The Ferrari block is one `if`, not an early return.** Retail's `beq .L_802CBC50` jumps to the
  *epilogue*, i.e. `return count` with `count == 0` — there is no separate `li r3, 0` on that
  path. `if (cubicRootCount != 0) { …everything… } return count;` reproduces it exactly.

Inside the block, the Newton iteration's two Horner expressions have to be spelled
`coefficients[4] * (4.f * root)` and `coefficients[4] * root + coefficients[3]`, not
`4.f * root * coefficients[4]` and `coefficients[3] + root * coefficients[4]`. Both spellings are
the same arithmetic and differ only in which operand MWCC puts on the left of the `fmuls`, which
is the whole of the remaining 2.8%.

### Verified exact, not just high-scoring

`build/tools/objdiff-cli diff -1 build/G2ME01/src/Kyoto/Math/RMathUtils.o -2
build/G2ME01/obj/Kyoto/Math/RMathUtils.o` → `fn_802CB918` `match_percent` 100, **0 differing
instructions of 226**, with sda21 relocation labels and branch displacements normalised (they
differ by name and by address only). `objdiff` scores `lfs`/`bl` relocations as equal, so this is
the same standard the earlier notes in this unit used.

## `fn_802CBCA0` (964 B): written, at 93.17%, not counted

The cubic solver `fn_802CB918` delegates to, so it had to exist for the call to resolve. It went
from **0.00% to 93.17%** — 38 of 242 instructions differing. It is *not* a matched function, so
the item's +1 is `fn_802CB918` alone.

It carries the same Prime 1 donor, and most of the same spelling rules apply and were applied:
`unsigned int count`, `coefficients[1] * shift` (not `shift * coefficients[1]`) inside the
`-0.5f *` for `q`, `coefficients[3] * (3.f * root)` in its derivative, and
`coefficients[3] * root + coefficients[2]` in its numerator — those three are the same
operand-order rule as the quartic's. Two more shapes were measured:

- `roots[0] = …; count = 1;` instead of `roots[count++] = …` for the single-root Cardano path
  and the linear path, and `count = 2` for the quadratic, reproduce retail's `li r30, 0x1` /
  `li r30, 0x2` with plain `stfs f2, 0x0(r29)` stores. `roots[count++] = …` keeps the `slwi` pair.
  Worth **+4 points** (86.99 → 93.17) and it is also what makes the trig loop's pointer spelling
  below possible.
- The three-real-roots loop needs `float* out = roots; … *out++ = …;` rather than
  `roots[count++] = …`. Retail walks the pointer (`mr r31, r29` before the loop,
  `addi r31, r31, 0x4` after the store) instead of indexing it, which is why retail increments
  two registers per iteration and the indexed form does not. Worth **+2 points**.

### What still stops `fn_802CBCA0` (not measured to a wall, not claimed)

Two groups remain, and I stopped rather than guess:

1. **Register allocation, ~20 instructions.** `count` is in `r31` in ours and `r30` in retail, and
   the loop pointer follows it, so every `li`/`stfs`/`mtctr` on those differs. Separately `f24` and
   `f25` are swapped for `q * -0.5f`, which then propagates through the Cardano branch
   (`f25`/`f26`/`f27` interchanged at 118-140) because `positive` and `negative` are computed
   from the same temp. This is the `CSteeringBehaviors` note's "register allocation" category:
   it is not a correctness difference and I did not find the source shape that allocates
   `count` to `r30`.
2. **Retail's 14-instruction dead tail** (`.L_802CBFC0`): `cmplwi r30, 0` / `mr r4, r29` /
   `mr r3, r30` / `ble` / `srwi. r0, r30, 3` / `mtctr r0` / `beq` / `addi r4, r4, 0x20` / `bdnz`
   / `andi. r3, r3, 0x7` / `beq` / `mtctr r3` / `addi r4, r4, 0x4` / `bdnz` — an 8x-unrolled
   walk of `r4` over `count` elements that computes nothing the function uses. It is the
   `roots`-vector advance from the Prime 1 spelling surviving as dead code, i.e. retail's source
   still had the `rstl::reserved_vector` here and MWCC did not fold the pointer bump away. I
   could not reproduce it from a `float*` signature and did not try hard: it is 14 instructions
   of provably-inert code, so matching it would not make the function correct, only longer.

I also did not try `fn_802CCF88` (344 B), `fn_802CD2E4` (720 B) or `fn_802CD5B4` (808 B), the other
three still-unwritten functions in this unit. They are the natural next item.

## Placement and shape checks

- `python3 tools/check_decl_order.py --unit RMathUtils` → `ok: 1 unit(s) checked, none emits its
  functions out of retail order`. **This caught a real error**: the two solvers were first written
  at line ~142, between `fn_802CD0E0` and `fn_802CCE4C`, which is *ascending* relative to
  `SolveQuadratic` (0x802CC064) below them. They now sit between `SolveQuadratic` and
  `PhongBlob` — descending, `fn_802CBCA0` (0x802CBCA0) above `fn_802CB918` (0x802CB918), both
  below `SolveQuadratic` (0x802CC064) and above `EaseInOut` (0x802CB330). objdiff was at 100%
  *before* the move, exactly as the brief warns.
- `tools/unit_fit.sh Kyoto/Math/RMathUtils.cpp` → `no extra functions: our object defines only
  what the retail unit object does`. `.text` 8404 of 11356 claimed bytes, `.ctors` still 4 short
  (`fn_802CDF64`'s `__ctors$10` pragma is present but the section is `NonMatching`), so the unit
  is nowhere near flipping and I did not try.
- `sha1sum build/G2ME01/main.dol` → `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, unchanged.

## The `#ifndef TARGET_PC` guard, and why

Both new functions are inside the guard the file already uses for `fn_802CDF50`/`fn_802CDF64`
and the Perlin bodies (see the note on `progress-unit-rmathutils`: unguarding them costs a
`link_check: STRICT FAIL - 326 undefined against a baseline of 324`). It is **not** optional
here, because both functions call `fn_802CDF50` — the five-comparison sort and the three-element
sort both go through it — and `fn_802CDF50` is itself guarded, so compiling them into the port
would put a new undefined symbol into a link that has no caller for them. `goal_check`'s port
probe passed with the guard in place.

## NEW

NEW: progress-unit-rmathutils-cubic-solver | progress | Kyoto/Math/RMathUtils | fn_802CBCA0 (964 B, the cubic solver the quartic delegates to) is at 93.17%; the residue is count allocated to r31 instead of r30 and retail's 14-instruction dead vector-advance tail at 0x802CBFC0, neither of which I reproduced

The MWCC 2.7 brace-init miscompilation is a codegen rule, not work that raises a count, so it is
recorded here and deliberately **not** filed as an `NEW:` item.