# progress-unit-cscripttrigger-rest2

`kind: progress`, target `MetroidPrime/ScriptObjects/CScriptTrigger`. The unit stays `NonMatching`;
`flip_test.sh` was not run and is not the acceptance test here. The judge is
`build/report.json`'s per-unit `matched_functions`.

## Result

**18 -> 20 of 44 matched functions.** `./tools/goal_check.sh build/goal/item.json` prints
`PASS progress-unit-cscripttrigger-rest2`; the whole-project matched count went 11933 -> 11935 with
`linked` unchanged at 5727 and the `All:` line unmoved at 33.74% fuzzy / 26.91% matched.

| function | before | after |
| --- | --- | --- |
| `HasInhabitant__14CScriptTriggerCF9TUniqueId` | 39.38% | **100%** |
| `AcceptScriptMsg__14CScriptTriggerFR13CStateManagerRC10CScriptMsg` | 73.25% | **100%** |

The constructor rose 93.78% -> 95.71% but did not reach 100%, so it does not count yet.
Unit fuzzy 32.66% -> 34.41%, matched code 21.34% -> 25.41%.

The previous run's `NEW:` line guessed the cause of `HasInhabitant`'s 60% gap: "retail copies the
tracker's trigger list onto the stack before walking it". **That guess was wrong in its mechanism,
right in its observation.** Retail does not copy anything; `rstl::find`'s by-value iterator
parameters land in the caller's frame, which is where the four stack stores come from. The copy is a
side effect of calling a function whose parameters are class objects.

## The change

### 1. `HasInhabitant`: `rstl::find`, not a hand-written loop

`src/MetroidPrime/ScriptObjects/CScriptTrigger.cpp:127-131`. The hand-written loop produced a
100-byte body walking the live list in registers. Retail's is 148 bytes with a frame and the
iterator at `r1+20`. `rstl::find` (`include/rstl/algorithm.hpp:24`) is

```cpp
while (first != last && !(*first == val))
  ++first;
return first;
```

so `find(begin, end, id) != end` gives retail's exact shape: two spill pairs
(`stw r9,8(r1)` / `stw r9,12(r1)` and `stw r0,16(r1)` / `stw r0,20(r1)`, both halves of the two
by-value iterators), the loop-carried `lwz/stw r1+20`, and the post-loop `cmplw r0,r9` that returns
true only when the found iterator is not `end`. Byte-for-byte, 148/148.

`#include "rstl/algorithm.hpp"` added at line 10 for it.

### 2. `AcceptScriptMsg`: the missing `kSM_XALD` arm

`src/MetroidPrime/ScriptObjects/CScriptTrigger.cpp:63-76`. The previous body had a
`// TODO: resolve the Connect/Attach target on area load` and was missing the whole first branch.
Retail's head of the function (0x8007240c) is `lwz r31,8(r5)` - the message loaded *before* the
`GetActive()` test - then `addis r31,-22593 / cmplwi r0,19524`, which is the shifted-subtract
compare against `0x4C44` in the low half; `0xA7B54C44` decodes to `kSM_XALD`, the same idiom
`CScriptSkyRipple.cpp:130` documents. The call is
`FindConnectedObject(mgr, kSS_Connect, kSM_Attach)` (both immediates are the ASCII `CONN`/`ATCH`)
writing `sth r0,368(r28)`, i.e. `mAttachedTrigger` at +0x170.

The message is read once into a local `message`, not three times off `msg`: retail keeps it in
`r31` across the call. Using `msg.GetMessage()` inline in the second condition reloads it and gives
87.54% instead of 100%.

### 3. Constructor: `CModelData::CModelDataNull()`

`src/MetroidPrime/ScriptObjects/CScriptTrigger.cpp:16`. Retail calls `CModelDataNull__10CModelDataFv`
(0x80036184, a 0x20-byte wrapper that tail-jumps the real `CModelData::CModelData()`), not the ctor
itself. `CModelData::CModelDataNull()` (`include/MetroidPrime/CModelData.hpp:183`) is exactly that
wrapper and its symbol already exists. Call target now matches; 93.78% -> 95.71%.

### 4. Header: the deactivate pair is a byte-wide bitfield, not a word

`include/MetroidPrime/ScriptObjects/CScriptTrigger.hpp:92-96`. Retail's constructor sets the two
flags with `lbz r3,444(r21)` / `rlwimi r3,r30,7,24,24` / `stb r3,444(r21)` - a read-modify-write on
the single byte at +0x1bc, twice. As `uint : 1` the compiler emits `lwz`/`stw` with
`rlwimi r3,r30,31,0,0` instead. Declaring them `uchar : 1` (in a 1-byte unit) produces retail's
byte sequence exactly. `x1bc_2_` was a 30-bit `uint`, which is what held the word together; as
`uchar : 6` it fills the same byte.

The first attempt at this broke `GetPlayerInside` (100% -> 99.67%): the byte-only bitfields pulled
`mPlayerInside` from +0x1c0 back to +0x1bd, and retail reads `lbz r3,448(r3)`. `uchar x1bd_3_[3]`
restores the offset. **The padding is load-bearing** - the two fields are `uchar` bitfields but the
*array* still starts on the +0x1c0 word boundary, so the 3 bytes are real, not alignment.

## Files touched

- `src/MetroidPrime/ScriptObjects/CScriptTrigger.cpp` - lines 10, 16, 63-76, 127-131
- `include/MetroidPrime/ScriptObjects/CScriptTrigger.hpp` - lines 92-96

No `configure.py`, `splits.txt` or `files.cmake` change: nothing was carved and no symbol was
claimed, so the four-file carve rule does not apply. `docs/HANDOFF.md` was **not** edited (the gate
rewrites its counts itself; `check_docs_claims.py` passed with the tree as it stands).
`python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CScriptTrigger` says
`ok: 1 unit(s) checked, none emits its functions out of retail order`.
`python3 tools/check_symbol_names.py` says `checked 515 units; 0 declared names are missing`.

## Things measured, so nobody repeats them

Every number below is `./tools/fast_try.sh MetroidPrime/ScriptObjects/CScriptTrigger` after a
rebuild of just that object.

`HasInhabitant`, one spelling at a time, from the 39.38% starting point:

- hand-written loop over the live list (pre-existing) -> **39.38%**, 96 bytes, register iterator.
- `break` out of a hand-written loop, then `if (it != end) return true;` -> **56.81%**, 100 bytes.
  This gets retail's *loop shape* (the post-loop `cmplw r0,r9` is there) but no frame and no spill.
- `rstl::find(triggers.begin(), triggers.end(), GetUniqueId()) != triggers.end()` -> **100%**.
  The remaining 43% of the first attempt was not the loop at all; it was the missing stack frame.

`AcceptScriptMsg`, three spellings:

- `msg.GetMessage()` in both conditions, no `kSM_XALD` arm -> 73.25% (pre-existing).
- add the `kSM_XALD` arm, still `msg.GetMessage()` inline -> 87.54%.
- same, with `const EScriptObjectMessage message = msg.GetMessage();` hoisted -> **100%**.

Constructor, one change at a time:

- `CModelData()` (pre-existing) -> 93.78%.
- `CModelData::CModelDataNull()` -> **95.71%**.
- Deactivate pair as `uint : 1` (pre-existing) -> constructor emits `lwz`/`stw`/`rlwimi r3,r30,31,0,0`.
- Deactivate pair as `uchar : 1` with no padding -> 95.71% (constructor unaffected) **but
  `GetPlayerInside` drops to 99.67%**, because `mPlayerInside` moves to +0x1bd.
- plus `uchar x1bd_3_[3]` -> both hold, 20/44.

## What is still open in this unit (24 functions unmatched, measured)

`UpdateInhabitants` 1520 B at 0.26%, `Touch` 1044 B at 0.38%, `AddInhabitant` 944 B at 0.42%,
`UpdateCameraInhabitant` 664 B at 0.60%, `ClearInhabitants` 224 B at 1.79%, `SetPlayerInside`
240 B at 1.67%, `ReplaceInhabitant` 276 B at 98.39%, the constructor at 95.71%, and 13 unnamed
`fn_*` at 0.00%.

The constructor's remaining 4.3% is one `lhz r4,-27740(r13)` (a second load of `kInvalidUniqueId`)
plus `li r3,0` / `sth r4,16(r1)` / `li r4,1` sitting inside the `CActor` argument-evaluation
window rather than after it - i.e. where the compiler places the base-class argument temporaries
relative to the two ctor calls (`CModelDataNull` and `CActorParameters`). That is argument
evaluation order in the mem-initializer, not a spelling difference in the body.

The six low-percentage bodies (`UpdateInhabitants`, `Touch`, `AddInhabitant`,
`UpdateCameraInhabitant`, `ClearInhabitants`, `SetPlayerInside`) are all `// TODO:` stubs with no
body. They are real work, not spelling problems, and 1044-1520 bytes each.

The 13 unnamed `fn_*` are the same dead end the previous run measured: three of them
(`fn_8007334C`, `fn_800733E0`, `fn_8007298C`) are byte-identical to COMDAT template functions our
object already emits, and renaming them in `config/G2ME01/symbols.txt` did not pair them.
Unchanged this run, not re-attempted.

NEW: progress-unit-cscripttrigger-csctor | progress | MetroidPrime/ScriptObjects/CScriptTrigger |
the constructor is at 95.71% and its whole remaining gap is where the compiler places the
`CActor` mem-initializer argument temporaries (one extra `kInvalidUniqueId` load and three
instructions moved inside the `CModelDataNull`/`CActorParameters` call window); `ReplaceInhabitant`
is at 98.39%.