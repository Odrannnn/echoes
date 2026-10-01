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

---

# Second run (2026-09-30, lane 6)

Re-measured on a clean tree: **33/35**, `TuneScreenBrightness` 84.12%, `ResetControllerAssets`
26.80%. The previous run's rise had already landed upstream, so those two are all that is left.
No source change survived this run; the tree is clean and nothing is committed. The value here is
one **refutation** of the previous run's blocker and 17 more measured spellings.

## The previous run's `fn_80161D04` blocker is WRONG — it resolves

The earlier note says the tail call target is "in the next split range, not implemented anywhere in
this tree", which reads as an unclaimed gap. It is unclaimed in `splits.txt` (line 752 ends
`0x80161D04`, line 755 starts `0x80161FBC`) **but configure.py auto-generates a unit for it**:
the build reports

```
main/auto_03_80161D04_text: 0.00% fuzzy, 0.00% matched (0 / 3 functions)
```

Measured end to end, not inferred: I added `void fn_80161D04(void*, void*, const unsigned char*);`
and a call to it inside `ResetControllerAssets`, then ran `./tools/decomp_build.sh`. It compiled, the
link **succeeded** (exit 0) and `sha1sum build/G2ME01/main.dol` stayed
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`. Probe reverted, `git status --short` empty.
**A call to `fn_80161D04` costs nothing: no carve, no link workaround, no stub.** Whoever picks
this up should not spend time on that.

Two more traps worth writing down:

* `build/G2ME01/main.elf` is dtk's **reference** ELF, not our link output. It holds *retail's* bytes
  for `TuneScreenBrightness` and `ResetControllerAssets` even straight after a full relink, so
  `tools/dis.sh <addr> <size>` on `main.elf` will show you retail's schedule and you will conclude
  you matched. Measure with `build/report.json` / objdiff, or disassemble
  `build/G2ME01/src/MetroidPrime/Player/CGameOptions.o`.
* The same applies to `sha1sum build/G2ME01/main.dol`: it is retail-identical for every unit that is
  not `Matching`, so it proves nothing about this unit either way.

## `fn_80161D04` signature, decoded from `0x80161D04` (0x1C4 bytes)

A recursive quicksort over an array of 8-byte elements keyed on their **first word**:

* r3 → r29, used as `lwz rX,0(r29)` ⇒ `T** first`
* r4 → r30, `lwz rX,0(r30)` ⇒ `T** last`
* r5 → r31, `lbz rX,0(r31)` ⇒ `const uint8*` to the key byte. The key is compared with
  `cmplw r0,r7` against `lwz 0(begin)`, and the byte is copied into its own outgoing arg slots.
* `n = (end - begin) >> 3`; `n <= 1` returns; `n <= 20` calls `fn_80161F40` (insertion sort) with
  `(&begin, &end, &key)`; above that it takes the median via `fn_80161EC8`, partitions, and makes
  two recursive calls with the same `(T**, T**, const uint8*)` shape.

The caller's outgoing block holds `20(r1)` and `24(r1)` = `begin`, `28(r1)` and `32(r1)` = `end`,
`8(r1)` and `12(r1)` = one key byte, and **`16(r1)` is read (`lbz r7,16(r1)`) but never written** —
retail genuinely loads it uninitialised. The duplicated (begin,begin)/(end,end)/(byte,byte) pairs
are a struct-by-value argument area, not three independent pointer args. That pattern is the part
still unreproduced.

## What really blocks `ResetControllerAssets` (confirmed, not a guess)

The element type, not the call. Ours emits

```
__as__Q24rstl47vector<10SObjectTag,Q24rstl17rmemory_allocator>FRCQ24rstl47vector<10SObjectTag,...>
__dt__Q24rstl47vector<10SObjectTag,Q24rstl17rmemory_allocator>Fv
```

retail emits `__as__Q24rstl55vector<Q24rstl11pair<Ui,Ui>,...>` / `__dt__...Fv`. The member is
declared `rstl::vector<SObjectTag>`; retail's is `rstl::vector<rstl::pair<uint,uint>>`. Identical
8-byte layout, different template argument, therefore different mangled helper names and different
`bl` targets — 100% is unreachable without changing the member's type in the header, and that would
change what `PutTo`, `CGameOptions(CBitStreamReader&)` and `ResetToDefaults` (all at 100% today)
call. **The wall is in the header, not in the body.**

Re-measured and still true: `sControllerAssets[]` holds **one** entry,
`SObjectTag(0x2A13423E, 0xF13452F8)`; retail loads ten pairs = 20 words, and retail's first word is
`0x2A13C23E` — this tree has `0x2A13423E`, a `4` where retail has a `C`.

## `TuneScreenBrightness` — 17 more spellings, none above 84.12%

The entire difference is **one hoisted load**. Retail:
`lis / lwz / stw / addi / lfd f3 / xoris / lfs f1 / stw / lfs f4 / lfd f2 / lfs f0 / fsubs / fmuls /
fmadds`. Ours: the same 17 instructions with `lfd f3` moved to slot 2 (right after `lis`) and
`lfs f1`/`lfs f4` pulled ahead of `xoris`. Same registers f0..f4, same four constants at the same
`.sdata2` offsets (0x8041C4F8 = double `0x43300000_80000000`, 0x8041C500 = 1.0f,
0x8041C504 = 0.375f, 0x8041C508 = 0.25f), same integer-op order. It is MWCC's scheduler, not the body.

Measured with `tools/fast_try.sh` (baseline 84.12), none of these tried before:

| spelling | % |
|---|---|
| `const float` return — Prime 1's exact declaration | **68.24** |
| `const float` return + `const float` local | 68.24 |
| `float f = screenBrightness; f = f - 4;` | 68.24 |
| compound chain `f -= 4; f /= 4.f; f *= 0.375f; f += 1.f;` | 58.24 |
| `float f = screenBrightness; f -= 4;` (two statements) | 68.24 |
| split `const float g = f / 4.f * 0.375f; return g + 1.f;` | 72.94 |
| three split consts `f`,`g`,`h` | 72.94 |
| fully split `a`,`b`,`c`,`c` | 72.94 |
| `f *= 0.25f; f *= 0.375f; f += 1.f;` after the init | 72.94 |
| `f * 0.375f / 4.f + 1.f` and `1.f + f * 0.375f / 4.f` | 83.24 |
| `g = f * 0.375f; return g / 4.f + 1.f;` | 83.24 |
| `1.f + f / 4.f * 0.375f` (operand swap) | 84.12 |
| `enum { kBias = 4 }; f = screenBrightness - kBias;` | 84.12 |
| `const int n = screenBrightness - 4; const float f = (float)n;` | 84.12 |
| `const int b = screenBrightness; const float f = b - 4;` | 84.12 |
| `screenBrightness + (-4)` and `screenBrightness - (4)` | 84.12 |
| `static_cast<float>(screenBrightness - 4) / 4.f * 0.375f + 1.f` | 84.12 |
| C-style cast, `4.0f`/`1.0f` literals, parens-only, uninitialised decl first, `g` temp for the divide | 84.12 |

**The `const float` result is the one worth keeping.** Prime 1 declares
`const float CGameOptions::TuneScreenBrightness()`, and that spelling is *worse* (68.24%) because
`const` makes MWCC do the subtraction in **float**: it emits
`xoris / stw / lfd f0 / fsubs f4,f0,f2 / lfs f3 / fsubs f4,f4,f3` — two `fsubs` and **no `addi`** —
instead of retail's integer `addi r0,r3,-4` plus one `fsubs`. So retail is definitively not
`const`-qualified and this whole line of attack is closed. (Top-level `const` on a return type does
not change the mangled name, so trying it was safe.)

## State of the tree

Clean, nothing committed, no `asm` added. Full build green on the clean tree:

```
All:  31.34% fuzzy, 23.72% matched, 11.83% linked (10320 / 28465 functions)
main/MetroidPrime/Player/CGameOptions: 90.14% fuzzy, 85.29% matched code (33 / 35 functions)
sha1sum build/G2ME01/main.dol   6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
```

## NEW

(none — both remaining functions are characterised above as walls; the `fn_80161D04` finding
removes a blocker rather than raising a count, so it belongs in this file and not in the queue.)

WALL: TuneScreenBrightness__12CGameOptionsFv 84.12% - MWCC hoists one `lfd` (the int->float bias constant) to slot 2; body, registers and constants already identical to retail; 42 spellings over two runs, and the `const float` route is now closed (it removes retail's integer `addi`).

WALL: ResetControllerAssets__12CGameOptionsFi 26.80% - member is `rstl::vector<SObjectTag>`, retail's is `rstl::vector<rstl::pair<uint,uint>>`, so `__as__`/`__dt__` mangle differently; changing the member type would break `PutTo` / `CGameOptions(CBitStreamReader&)` / `ResetToDefaults`, which are at 100%. The `fn_80161D04` call itself is NOT a blocker (verified: it links).
---

# Third run (2026-10-01, lane 2)

**`ResetControllerAssets` 26.80% -> 100%. `matched_functions` 33 -> 34 of 35.** Unit
`90.14% -> 99.75%` fuzzy, `85.29% -> 98.43%` matched code. The unit stays `NonMatching`.

Diff: `include/MetroidPrime/Player/CGameOptions.hpp` (one member's element type, plus a comment
on why) and `src/MetroidPrime/Player/CGameOptions.cpp` (`ResetControllerAssets` written out,
the one-entry `sControllerAssets` global deleted, `rstl/algorithm.hpp` included).

## Both earlier "walls" on this function were wrong. Both were cheap to test.

The previous two runs wrote off `ResetControllerAssets` without trying the obvious thing. Both
blockers were checked in minutes and neither holds.

### 1. "Changing the member type would break `PutTo` / the ctors / `ResetToDefaults`" — no

That claim was never tested; it is false. Measured:

```
build/binutils/powerpc-eabi-objdump -d -r build/G2ME01/obj/MetroidPrime/Player/CGameOptions.o \
  | grep REL24 | sed 's/.*REL24\t//' | sort | uniq -c
```

In retail's own object `__as__`, `__dt__` and `reserve` for
`rstl::vector<rstl::pair<Ui,Ui>,rstl::memory_allocator>` appear **once each**, and all three are
inside `ResetControllerAssets`. No other function in the unit references any `vector<...>`
helper. `grep` over `src/` and `include/` also finds no user of `CGameOptions::vec` at all.
So retyping the member to `rstl::vector<rstl::pair<uint, uint> >` is safe: after the change all
33 previously matched functions were still at 100% (measured, whole unit). **Do this first in
any unit where a member's element type is in the way — it is a one-line edit.**

### 2. "The `fn_80161D04` tail call cannot be written down" — it already is

`rstl::sort_by_key` has been in `include/rstl/algorithm.hpp` for a while, and one
`rstl::sort_by_key(vec);` reproduces retail's call **exactly**, including the parts the earlier
runs called unreproducible: the `lbz r7,16(r1)` read of a slot retail never writes, `r3 = r1+20`,
`r4 = r1+28`, `r5 = r1+8`, the duplicated `(begin,begin)/(end,end)/(byte,byte)` words at
`8/12/20/24/28/32(r1)`, and the never-written `0/4/16(r1)`. Those three pointer-looking
registers are `rstl::sort`'s own `(It first, It last, Cmp)` argument block; the `pair_sorter_finder`
comparator is the `const uint8*`-shaped value. Nothing had to be invented.

**`sort<...>` mangles to a very long name and retail's target is `fn_80161D04`, and the function
still scores 100%.** So objdiff does **not** compare `R_PPC_REL24` target symbol names when it
scores a function — the earlier inference that the guessed name was the blocker was wrong. The
same holds for the 20 `R_PPC_EMB_SDA21` relocs: retail's `lbl_80418448`..`lbl_80418494` versus
our `@653`..`@672` show up in `lanediff.sh` but are not counted. That is also why "retail's
function name must be recoverable" is the wrong thing to worry about when reading a `fn_` target.

(`rstl::sort`'s third parameter is `pair_sorter_finder<pair<Ui,Ui>, less<Ui>>` by value, which is
a byte wide and is passed through the `r5` pointer; `rstl::pointer_iterator<pair<Ui,Ui>,...>` is
a `T*`, which is the `r3`/`r4` pair. That is where the three "pointers into the outgoing
argument block" come from.)

### 3. The one thing that actually mattered: `push_back_unsafe`, not `push_back`

Retail's append is `construct(mItems + mCount, in); ++mCount` with **no capacity test** — one
`bl reserve(15)` before the loops and then 15 plain stores. This repo's
`rstl::vector::push_back` (`include/rstl/vector.hpp`) starts with
`if (mCount >= mCapacity) reserve(...)`, which emits a compare and a second `bl reserve` per
element. `push_back_unsafe` (same file, one line below) is the check-free form, and the
`reserve(15)` above makes 15 unconditional appends into a 15-slot vector sound.

Prime 1's headers carry the same check (`prime-ref/extern/rstl/include/rstl/vector.hpp`) while
its copy of this function is reported Matching upstream, so MW 1.3.2 must elide the test — I could
not verify that, `prime-ref` is a source-only clone with no `build/`. What is measured here is
only the MW 2.7 half: `push_back` emits a compare plus a second `bl reserve` per element and
`push_back_unsafe` does not. **A `push_back` in a MW 2.7 unit that retail compiled without one
wants `push_back_unsafe`.**

## The loop: a plain pre-test `for` counts; `do`/`while` does not

With `push_back_unsafe` in place, the only remaining difference was retail's
`li r0,5 / mtctr r0 / bdnz` against ours' `li r9,0 / cmpwi r9,5 / bne`. Measured, all with the
final `push_back_unsafe` body, `tools/fast_try.sh`:

| first loop | % |
|---|---|
| `for (int i = 0; i < 5; ++i)` | **100.00** |
| `for (int i = 0; i != 5; ++i)` | 100.00 |
| `while (i < 5) { ...; ++i; }` | 100.00 |
| `for (uint i = 0; i < 5; ++i)` | 100.00 |
| `for (int i = 0; i <= 4; ++i)` | 100.00 |
| `int i = 0; do { ... } while (i != 5);` | 97.01 |
| `int i = 0; while (i < 5) { ...; ++i; }` pre-set form | 97.01 |
| `do { ... } while (i++ != 4);` | 96.94 |
| `do { ... } while (++i != 5);` | 97.01 |
| pointer `do { } while (++p != e)` | 92.04 |
| pointer `for (; p != e; ++p)` | 92.40 |
| `int i = 5; do { ...[--i] } while (i != 0);` | 92.04 |

**The earlier runs' "post-test loop needed" rule is refuted here.** With the branch out of the
body (via `push_back_unsafe`) MWCC 2.7 counts the trip fine and rotates the pre-test `for` to
`mtctr`/`bdnz`. The branch was what stopped it counting, not the loop shape. With `push_back`
still in the body no spelling reached the counted form.

Also needed: **`case 0` written first, `case 1` second, no `default:`** — retail's layout puts
`case 0` as the dispatch fallthrough and `case 1` after it, which is Prime 1's order. The
dispatch instructions were already right either way, so only the block layout distinguishes them.

## `TuneScreenBrightness` — 10 more spellings, still 84.12%, same 2 loads

Unchanged from the baseline; new spellings measured this run (baseline `84.12`):

| spelling | % |
|---|---|
| no local: `return (screenBrightness - 4) / 4.f * 0.375f + 1.f;` | 84.12 |
| `int n = screenBrightness - 4; float f = n;` | 84.12 |
| `const int bias = 4; float f = screenBrightness - bias;` | 84.12 |
| `return 0.375f * (f / 4.f) + 1.f;` (operand order) | 84.12 |
| `double d = screenBrightness - 4; return float(d) / 4.f * 0.375f + 1.f;` | 82.94 |
| `float g = f / 4.f; g = g * 0.375f; return g + 1.f;` | 72.94 |
| `float g = f / 4.f * 0.375f; return 1.f + g;` | 72.94 |
| `return f / 4.f * (0.375f + 1.f);` | 72.94 |
| `... + 1.` (double literal) | 60.29 |

Still MWCC's scheduler: our `lfd f3` (int->double bias constant) is hoisted to slot 2 and
`lfs f0` comes before `lfd f2`, retail has the bias load after `addi` and `lfd f2` before `lfs f0`.
Same 17 instructions, same registers f0..f4, same four constants at the same `.sdata2` offsets.

## Caveat for whoever takes the `match` item on this unit

`tools/unit_fit.sh MetroidPrime/Player/CGameOptions.cpp` reports **8 functions in ours but not in
the retail object, 1288 bytes**: the `rstl::sort` / `__insertion_sort` / `__sort3` template
instantiations plus `__as__` / `__dt__` / `reserve` for the new element type and
`__dt__` for `reserved_vector`. Retail puts the sort's bytes in the *next* split range
(`fn_80161D04` at `0x80161D04`), so per `unit_fit.sh`'s own note this is the "dtk put retail's
bytes in a neighbouring unit" / COMDAT-weak case. It is a real obstacle to flipping the unit and
it is new to this unit's object (the previous three `vector<SObjectTag>` helpers were already
there), so it belongs in a `match` attempt, not in this `progress` one.

## Verification

```
sha1sum build/G2ME01/main.dol        6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
python3 tools/check_symbol_names.py  checked 514 units; 0 declared names are missing from their object
python3 tools/check_decl_order.py --unit MetroidPrime/Player/CGameOptions
                                     ok: 2 unit(s) checked, none emits its functions out of retail order
./tools/decomp_build.sh              All:  32.83% fuzzy, 25.67% matched, 12.17% linked (11440 / 28465)
                                     main/MetroidPrime/Player/CGameOptions: 99.75% fuzzy, 98.43% matched (34 / 35 functions)
tools/goal_check.sh build/goal/item.json
  ok  no judge-owned path touched
  ok  gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok  counts: matched 11439 -> 11440   linked 5587 -> 5587
  ok  check_symbol_names.py
  ok  target rose: main/MetroidPrime/Player/CGameOptions: 33 -> 34 / 35 functions
  ok  no asm added
  PASS progress-prime1-cgameoptions
```

`tools/lanediff.sh` for `ResetControllerAssets__12CGameOptionsFi` now differs only in the
`R_PPC_EMB_SDA21` symbol *names* (`lbl_80418448` vs `@653`) and the `R_PPC_REL24` target name,
neither of which objdiff counts — see point 2 above. Not committed, per the brief.
`docs/HANDOFF.md` is rewritten by `gate.sh` inside `goal_check.sh` and was reverted; the driver
regenerates it.

## NEW

NEW: progress-fn80161d04-sort | progress | main/auto_03_80161D04_text | 3 retail functions at 0% (`fn_80161D04` recursive sort keyed on the first word of each 8-byte element, `fn_80161F40` insertion sort, `fn_80161EC8` median helper) whose bodies this repo already has as `rstl::sort` / `__insertion_sort` / `__sort3` in `include/rstl/algorithm.hpp`; needs a carve of the unclaimed `0x80161D04..0x80161FBC` range, and objdiff does not compare `R_PPC_REL24` target names so the instantiation's mangled name is not an obstacle.

WALL: TuneScreenBrightness__12CGameOptionsFv 84.12% - MWCC hoists one `lfd` (the int->float bias constant) to slot 2 and swaps `lfd f2`/`lfs f0`; body, registers and constants already identical to retail; 52 spellings over three runs, `const float` closed (it removes retail's integer `addi`), and no local/temporary/splitting/operand-order route moves it.
