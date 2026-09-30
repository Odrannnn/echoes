# progress-prime1-cgameoptions

`MetroidPrime/Player/CGameOptions` — unit stays `NonMatching`, **matched_functions 31 → 33 of 35**,
fuzzy 89.68% → 90.14%, matched code 81.68% → 85.29%. Two functions reached 100%.

Diff: `src/MetroidPrime/Player/CGameOptions.cpp` only (5 insertions, 2 deletions).

## What landed

| function | before | after | Prime 1's source |
|---|---|---|---|
| `ToggleControls(bool)` | 87.06% | **100%** | matched unchanged in intent, needed `const bool` |
| `InitSoundMode()` | 87.73% | **100%** | needed an `int` temporary |
| `TuneScreenBrightness()` | 84.12% | 84.12% | identical source, unreachable |
| `ResetControllerAssets(int)` | 26.80% | 26.80% | far more than a source port; see below |

### `ToggleControls` — `const bool` on the parameter

The only difference was register allocation: retail `clrlwi. r4,r4,24 / lbz r0,36(r3) /
rlwimi r0,r4,4,27,27 / stb r0,36(r3)`, ours `clrlwi. r0,r4,24 / lbz r5,36(r3) /
rlwimi r5,r4,4,27,27 / stb r5,36(r3)`. Adding `const` to the by-value parameter makes MWCC
convert the bool **into the parameter register itself** and then reuse it for the `rlwimi`,
byte for byte. Prime 1's `void CGameOptions::ToggleControls(const bool flag)` is already right
and the fork kept a `bool flag`. Also measured: a named `const bool` local before the assignment
reaches 100% as well, so it is the `const`, not the name. (Same lever as "A technique that works:
`const` on by-value parameters" in `docs/RUNNING_THE_DECOMP.md`.)

### `InitSoundMode` — an `int` temporary

Retail's second branch is `li r0,1 / lwz r3,0(r31) / cmpwi r3,0 / beq / mr r0,r3 / stw r0,0(r31)`:
the default is materialised **before** the load and only conditionally overwritten. The ternary
written in place gives the other order (`lwz r0,0(r31) / cmpwi r0,0 / beq / b / li r0,1 / stw`).
This is what reaches 100%:

```cpp
const int mode = soundMode != CAudioSys::kSM_Mono ? int(soundMode) : int(CAudioSys::kSM_Stereo);
soundMode = CAudioSys::ESurroundModes(mode);
```

`int(...)` on both operands is load-bearing — without the casts the same temporary is 87.73%.
Comparing against `0` instead of `CAudioSys::kSM_Mono` makes no difference. Measured, all on this
unit with `tools/fast_try.sh` (0.5 s a turn):

```
temp var, default assigned first, enum type      98.64
int temp + int() casts                          100.00  <- landed
int temp + int() casts, compare against 0       100.00
int temp + int() casts, explicit cast on assign 100.00
int temp, no casts, implicit enum assignment     build fails (enum <- int)
const ESurroundModes temp                      87.73
ESurroundModes temp (non-const)                87.73
same ternary, cast to ESurroundModes            87.73
reversed condition (== kSM_Mono ? kSM_Stereo : x) 92.05
`if (soundMode == kSM_Mono) soundMode = kSM_Stereo;` 91.82
```

## What did not move, and why it is not worth another lane the same way

### `TuneScreenBrightness` — 84.12%, two instructions of scheduler order

Same 15 instructions in both, same four constants (int-bias double, 0.25f, 0.375f, 1.0f, at the
same `.sdata2` offsets), same integer-op order. The only difference is where the scheduler puts
the int-bias `lfd`:

```
retail  lis  lwz  stw  addi  lfd f3  xoris  lfs f1  stw  lfs f4  lfd f2  lfs f0  fsubs ...
ours    lis  lfd  lwz  stw  addi  lfs f1  xoris  lfs f4  stw   lfs f0  lfd f2  fsubs ...
```

i.e. our constant load is hoisted above the member load, and `lfd f2,8(r1)` / `lfs f0` come out
swapped. 25 spellings measured, all 84.12% unless they broke the instruction sequence
(worse): no local / `const float` local / member read into a local / `static_cast<float>` /
`4.0f` / `4.0f` and `1.0f` / separate statements / nested temps / `f -= 4` (68.24) /
`f * 0.25f` (82.65) / `f * 0.375f / 4.f` (83.24, also swaps f1/f4) / `screenBrightness - 4.f`
(5.88, double arithmetic) / `double` intermediate (72.94). This is MWCC's scheduler, not the
body — the form is the wall `RUNNING_THE_DECOMP.md` rule 5 describes, and it is only 2 of 17
instructions. **Not filed as `NEW:`**: the target unit cannot rise from this alone.

### `ResetControllerAssets` — 26.80%, needs work no source port supplies

Prime 1's version is a starting point but retail Echoes differs, and the function cannot reach
100% without an ABI that is not in the tree. What is measured:

* **The constant table is 10 `SObjectTag`s = 20 words at `0x80418448..0x80418497`**, and Prime
  1's values are exactly right — including the first word, `0x2a13c23e`, which this repo's
  `sControllerAssets[0]` has as `0x2A13423E` (a `4` for a `C`). Pairs, in order:
  `(2a13c23e,f13452f8) (a91a7703,c042ec91) (12a12131,5f556002) (a9798329,b306e26f)
  (cd7b1aca,8ada8184) (1a29c0e6,f13452f8) (5d9f9796,c042ec91) (951546a8,5f556002)
  (7946c4c5,b306e26f) (409aa72e,8ada8184)`.
* Retail loads all 20 words as 20 individual `lwz ...(r13)` with 20 `R_PPC_EMB_SDA21` relocs
  (each 4 bytes gets its own dtk symbol), copies words 0..9 to a 10-pair stack array at `52(r1)`
  and holds words 10..19 in `r31,r30,r29,r28,r27,r26,r25,r24,r23,r22`.
* The dispatch is `cmpwi r4,1 / beq` → `bge` done → `cmpwi r4,0 / bge` → default, so the `switch`
  order matches ours; `case 0` is `vec = rstl::vector<...>()` (temporary built at `40/44/48(r1)`,
  `__as__` then `__dt__`), which is what this repo already writes.
* `case 1` is guarded by `lwz r0,44(r21); cmpwi r0,0; bne done` — i.e. **`if (vec.empty())`**,
  which this repo does not have; then `reserve(15)`.
* **Loop 1 is a counted loop that MWCC did not unroll**: `li r0,5 / addi r8,r1,52 / mtctr r0 /
  <two inlined push_backs> / bdnz`. Per `RUNNING_THE_DECOMP.md` ("MWCC rotates a loop only when
  it cannot count it") that shape needs a **post-test** loop — `do { … } while (++i != 5);` — not
  the pre-test `for` Prime 1 writes, which is why Prime 1's source emits something else here.
* **Loop 2 is unrolled 5x** and pushes from the registers, exactly Prime 1's second loop over
  `CStickOutlineToDPadRemap`.
* **The tail is a call to `fn_80161D04` (`0x80161D04`, 0x1C4 bytes, in the next split range, not
  implemented anywhere in this tree) with an argument list that cannot be written down from one
  disassembly.** `r3 = r1+20`, `r4 = r1+28`, `r5 = r1+8` — three *pointers into the outgoing
  argument block* — then nine stack words: `8(r1)` and `12(r1)` are the same byte, `20/24(r1)`
  are `vec.begin()` and `28/32(r1)` are `vec.end()`, and `0(r1)`, `4(r1)`, `16(r1)` are never
  written. The byte at `16(r1)` is *read* (`lbz r7,16(r1)`) and copied into `8/12(r1)`, so it is
  a value the caller had in a scratch outgoing-arg slot. This is `rstl::sort_by_key`, but the
  12-word shape it is called with does not correspond to any signature in
  `include/rstl/algorithm.hpp`, and a declaration invented to match it would be a guess.
* Also: retail's member is `rstl::vector<rstl::pair<uint,uint>, rstl::memory_allocator>` — the
  `case 0` path calls `__as__Q24rstl55vector<Q24rstl11pair<Ui,Ui>,…>` and
  `__dt__Q24rstl55vector<Q24rstl11pair<Ui,Ui>,…>`. This repo's `rstl::vector<SObjectTag> vec` is
  layout-identical (8 bytes) but mangles the helper names differently, so even a perfect body
  would differ in its `bl` targets.

**Not fixed in this change**, deliberately: writing the correct 20-word table in without the
`if (vec.empty())`, the post-test loop and the sort call does not move `matched_functions`
(a function counts only at 100%), adds 80 bytes of unreproduced data to the unit's object, and
grows the diff for no measured gain. Kept for a lane that can settle `fn_80161D04`'s signature.

## Verification

```
sha1sum build/G2ME01/main.dol   6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
python3 tools/check_symbol_names.py    checked 503 units; 0 declared names are missing
./tools/decomp_build.sh          All:  30.22% fuzzy, 22.08% matched, 11.74% linked (9809 / 28465)
tools/goal_check.sh build/goal/item.json
  ok  no judge-owned path touched
  ok  gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok  counts: matched 9807 -> 9809   linked 4895 -> 4895
  ok  check_symbol_names.py
  ok  All:  30.22% fuzzy, 22.08% matched, 11.74% linked (9809 / 28465 functions)
  ok  target rose: main/MetroidPrime/Player/CGameOptions: 31 -> 33 / 35 functions
  ok  no asm added
  PASS progress-prime1-cgameoptions
```

`tools/lanediff.sh` is empty for both `InitSoundMode__12CGameOptionsFv` and
`ToggleControls__12CGameOptionsFb`, and objdiff reports both at 100%.
Not committed, per the brief. `docs/HANDOFF.md` was rewritten by `gate.sh` running inside
`goal_check.sh` and has been reverted; the driver regenerates it.

## NEW

(none — the two functions above are the item's rise, and `TuneScreenBrightness` /
`ResetControllerAssets` are characterised above rather than re-queued.)