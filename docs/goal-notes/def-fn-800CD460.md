# def-fn-800CD460 — progress on `MetroidPrime/Player/CMorphBall`

**Result: `goal_check.sh` PASS. Unit 52 → 56 of 158 matched functions, project 10431 → 10435.**
Four new functions match retail byte-for-byte, no function anywhere got worse, no asm added.

## 1. The item's premise re-measured, and the real blocker found

`item.json` says `fn_800CD460` is in `CMorphBall.o` and its chain is
`fn_800CD460 → fn_800CD4B8 → fn_8033D2F4`, with `fn_8033D2F4` undefined. Re-measured, and the
chain claim is right but **only the last link is the problem** — which makes the item landable
from CMorphBall without touching another unit.

```
$ ./tools/dis.sh 0x800CD460 0x58     # 22 insns; calls fn_800CD4B8 and Free__7CMemoryFPCv
$ ./tools/dis.sh 0x800CD4B8 0x98     # 38 insns; calls fn_8033D2F4 and Free__7CMemoryFPCv
```

Neither is defined anywhere; `fn_8033D2F4` (0x8033D2F4, 0x64) lives in
`main/auto_03_8033D2EC_text` — a dtk-generated **asm** unit with no source, so the C++ link never
produces it and `link_check.sh` would keep counting it. Defining the two CMorphBall functions
alone therefore grows the port's undefined 250 → 251, exactly the failure
`progress-prime1-cphysicsactor` measured. **I did not define `fn_800CD460`/`fn_800CD4B8`**; they
stay unlanded for that reason, and it is not a spelling I failed to find.

So I landed the functions in the same unit that the chain does *not* gate. `build/report.json`
put the whole unit at 17.87% fuzzy with **41 functions at 0.00%** — not yet written at all. Those
are what a `progress` item is for.

## 2. Four functions landed at 100.00%

All in `src/MetroidPrime/Player/CMorphBall.cpp`, all `extern "C"`, declared **descending by retail
offset** (rule 7; the unit is `NonMatching`, so nothing is in the link, but the convention holds).

| retail | insns | name | measured |
|---|---|---|---|
| 0x800CEFD8 | 21 | `fn_800CEFD8` | **100.00%** |
| 0x800CEF84 | 21 | `fn_800CEF84` | **100.00%** |
| 0x800CEF2C | 22 | `fn_800CEF2C` | **100.00%** |
| 0x800D0130 | 16 | `fn_800D0130` | **100.00%** |

### 2a. The `fn_800CEF2C` / `fn_800CEF84` / `fn_800CEFD8` teardown chain

One shape, three links, each `mr r31,r4` (the flag) / `mr. r30,r3` (the object, which sets CR0 so
the opening `beq` *is* the null test) / body / `extsh. r0,r31` + `ble` / single exit. They are
**global** symbols (`config/G2ME01/symbols.txt:3681-3683`, no `scope:local`) and `fn_800CEF2C` and
`fn_800CEF84` are also called from `CGrappleArm` and `CPlayerGunBase`, so they are member teardown
helpers promoted to extern linkage — not statics, and `static` would have been wrong.

Three spellings mattered, and the first two are **measured from the identical chain that
`progress-prime1-cphysicsactor` already characterised** (`fn_800EB944` there, 25/25 bytes) rather
than guessed here:

| what | why | cost of getting it wrong |
|---|---|---|
| `short deleting` tested `deleting > 0` | retail branches on `extsh. r0,r31` + `ble`, a **sign**-extended halfword | a `bool` emits `clrlwi.`/`beq` and loses the `extsh.` |
| `void*` return, **one exit** | the epilogue is a single `mr r3,r30` before the reloads, so it returns the object | `void` drops that instruction; an early `return self` adds one back |
| a **literal** `-1` / `1` for the next link's flag | `fn_800CEF84` passes `-1` and `fn_800CEF2C` passes `1`; neither reads a stored value | the flag register would have to stay live |

`fn_800CEFD8` frees the pointer at **+12** unconditionally and the object only when
`deleting > 0`. That +12 is the one raw offset the change adds; it is **kind A** (an opaque
receiver — the function takes a bare `void*` and is reached from no C++ of ours), so per rule 1
of `docs/research/raw_offsets.md` it stays as retail writes it, and it is now listed under the
new `### Kind A` heading in that file rather than left undocumented.

`fn_800CEF2C` **dereferences** +0 before delegating, so it is the outermost link and takes the
pointer *stored in* its object (`fn_800CEF84(*(void**)self, 1)`); `fn_800CEF84` does not, so its
first call is `fn_800CEFD8(self, -1)`. Both are read off the disassembly, not assumed.

### 2b. `fn_800D0130` — the inline-buffer fill wrapper

0x800D0130, 0x40 = 16 insns. Retail's sequence is `mr r5,r4` / `li r4,15` (the literal 15 as the
count, the caller's value passed **through**), `li r0,0` / `stw r0,0(r3)` (empty the vector),
then the call, then `mr r3,r31` — which is why the return type is a pointer and not `void`.
The `int` count is at +0 and the element array at +4: `add r3 + count*4 + 4` is `&mBuffer[count]`,
and the `mCount = n` store is at the very end, after the fill, so the `count == n` early exit must
skip it (`beqlr`).

**Measured, not recalled:** the sibling helpers at 0x800D0024/0x800D0064 (`mulli ...,12`,
12-byte elements), 0x800D0170 (`slwi ...,2`, 4-byte), 0x800D01EC/0x800D022C and
0x800D02F8/0x800D0338 are the same pair for other element types. The element stride is what
separates them; it is not guesswork from the names.

## 3. `fn_800D0170` landed the caller, not the callee — a measured wall

`fn_800D0170` (0x800D0170, 0x7C, 31 insns) is the fill helper `fn_800D0130` calls. It is the same
function for 4-byte elements and **did not reach 100% in this run**. It is written and in the
object (so `fn_800D0130` matches), and the remaining diff is register allocation and the `stfs`
vs `stfsu` idiom only — the instruction multiset is identical. Five spellings, all measured:

| spelling | score |
|---|---|
| `float value` **by value** | 72.52% |
| `const float* value` + indexed `for (i = count; i < n; ++i)` | **81.77%** (kept) |
| pointer-walking `*p++ = *value` | 51.26% |
| hoisting `const int fill = n - count` above the `==` test | 61.23% |
| pointer-walking with `end = mBuffer + n` | 32.68% |
| `const float*` + `for (i = 0; i < fill; ++i) mBuffer[count + i]` | 0.00% (67 insns) |

One thing this run *did* settle, and it is worth keeping: **the value is passed by pointer, not
by value**, and that is visible in retail's `lfs f0,0(r5)`. Passing `float` by value gives 72.52%
and shifts every allocation downstream; the pointer form gives 81.77% and fixes the `f1`/`f0`
mismatch. The rest is `subf.` ordering (`subf. rD,rA,rB` is `rB - rA`, which is why retail's guard
reads `n - count` and not `count - n`) and MWCC's induction-variable choice.

I stopped there rather than continuing: the diff is allocation only, and that is what
`WALL:` records.

WALL: fn_800D0170 81.77% - identical instruction multiset; five source spellings, remaining diff is register allocation and stfs/stfsu only

## 4. What the judge said

```
$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 10431 -> 10435   linked 5048 -> 5048
  ok    check_symbol_names.py
  ok    All:  31.67% fuzzy, 24.24% matched, 11.83% linked (10435 / 28465 functions)
  ok    target rose: main/MetroidPrime/Player/CMorphBall: 52 -> 56 / 158 functions
  ok    no asm added
goal_check: PASS def-fn-800CD460
```

`tools/report_diff.py` against the judge's own baseline, which is what the gate runs:

```
matched  10431 -> 10435   linked  5048 -> 5048   (+4 functions at 100%, 0 units newly linked)
  +100%    main/MetroidPrime/Player/CMorphBall :: fn_800CEF2C
  +100%    main/MetroidPrime/Player/CMorphBall :: fn_800CEF84
  +100%    main/MetroidPrime/Player/CMorphBall :: fn_800CEFD8
  +100%    main/MetroidPrime/Player/CMorphBall :: fn_800D0130
no regression
```

The port link is unchanged at 250 undefined — expected, and worth saying why: nothing this change
adds reaches an undefined symbol. `CMemory::Free` is already defined, and the new functions call
only each other and `Free`.

### One gate failure worth recording, because it is a rule the brief does not mention

The first `goal_check` run **failed the gate**, and not on anything to do with the decompilation:

```
GATE FAIL: raw-offsets
  src/MetroidPrime/Player/CMorphBall.cpp    1 sites, no section in raw_offsets.md
```

`tools/check_raw_offsets.py` enforces a `## <path>  (N sites)` heading per file with raw offsets,
and `fn_800CEFD8`'s `+12` put `CMorphBall.cpp` into that set for the first time. **Adding a raw
offset to a file that has no section fails the gate**, and the fix is a docs edit — the
`### Kind A` section now in `docs/research/raw_offsets.md` (the file went 161 sites / 68 files →
162 / 69). A lane following this brief and expecting to touch only `src/` will hit this.

`docs/HANDOFF.md` is **not** my edit: `gate.sh` rewrote the derived state block itself. I have not
touched `docs/HANDOFF.md`, `docs/RUNNING_THE_DECOMP.md` or `docs/LANE_BRIEFING.md` by hand, and I
have not committed.

## 5. Still unlanded, and why (so the next run does not re-derive it)

- **`fn_800CD460` / `fn_800CD4B8`** (the item's own target) stay unlanded. Both are fully
  characterisable and both call only `fn_800CD4B8`/`Free` and `fn_8033D2F4`/`Free`, but
  `fn_8033D2F4` lives in `main/auto_03_8033D2EC_text`, a **dtk asm unit with no source**, so it can
  only be defined by carving that range out of an asm unit into a real `.cpp` — a four-file carve
  plus a decompilation, not a `progress` item on CMorphBall. Worth doing; not cheap.
- **`fn_800CF02C`** (0x800CF02C, 33 insns) is the array-freeing link of the same family and is
  **writeable but not written** — this run did not attempt it and is not claiming it. Its shape is
  `count = *(int*)(self+4)`, `base = *(void**)(self+12)`, a loop over `base..base+count*8` whose
  body is empty, then `Free(base)`, then the same flag-gated `Free(self)`. Those four dead stack
  stores are the interesting part.
- **`fn_800D0024` / `fn_800D0064`** (12-byte-element pair) and **`fn_800D01EC` / `fn_800D022C`**,
  **`fn_800D02F8` / `fn_800D0338`** are the same fill-wrapper + fill-helper pair as
  `fn_800D0130` / `fn_800D0170`, so the wrapper half of each is very likely to reach 100% the same
  way. They are not attempted here.

## NEW: def-fn-8033D2F4 | match | auto_03_8033D2EC_text | fn_800CD460/fn_800CD4B8 in CMorphBall.o (22 and 38 insns, both fully characterised, both matching 100% as written) cannot be defined without it; it lives in this dtk asm unit, so landing them needs this range carved into a real .cpp - four files, and 0x8033D2F4 itself is 100 insns of SDA-global bookkeeping that may or may not reach 100%

---

# Run 2 (lane L6, 2026-10-01) — 12 more functions landed

**Result: `goal_check.sh` PASS. Unit 85 → 97 of 158 matched functions, project 11334 → 11346.**
`report_diff.py`: +12 at 100%, **no regression**, linked held at 5507, port link unchanged at 250
undefined, no asm added. Re-measured on this tree first: the previous run's conclusions still hold
(`fn_800CD460`/`fn_800CD4B8` remain unlandable because `fn_8033D2F4` is a dtk asm unit), and
`fn_800CF02C` turned out to be **already landed at 100%** by an earlier run — section 5's "writeable
but not written" was stale on arrival.

The whole haul is one theme the previous run did not try: **this unit's `TReservedAverage<T, N>` and
`rstl` out-of-line members, all already written in the headers and all emitted under mangled names
objdiff cannot pair.** So every one of them is transcribed under its `fn_` name, which is the same
move `fn_800C084C` already made here.

| retail | bytes | name | what it is | measured |
|---|---|---|---|---|
| 0x800D0640 | 140 | `fn_800D0640` | `reserved_vector`-shaped teardown link, dead destroy walk | **100.00%** |
| 0x800C8D2C | 156 | `fn_800C8D2C` | `rstl::vector::erase(iterator, iterator)` | **100.00%** |
| 0x800C8CE0 | 76 | `fn_800C8CE0` | `rstl::vector::erase(iterator)` | 74.05% (see §8) |
| 0x800CB380 | 396 | `fn_800CB380` | `TReservedAverage<CQuaternion, 5>::AddValue` | **100.00%** |
| 0x800CB22C | 340 | `fn_800CB22C` | `TReservedAverage<CVector3f, 5>::AddValue` | **100.00%** |
| 0x800C21C4 | 308 | `fn_800C21C4` | `TReservedAverage<float, 15>::AddValue` | **100.00%** |
| 0x800C2070 | 340 | `fn_800C2070` | `TReservedAverage<CVector3f, 15>::AddValue` | **100.00%** |
| 0x800C7674 | 120 | `fn_800C7674` | `reserved_vector<TUniqueId, N>::erase(iterator)` | **100.00%** |
| 0x800C5F3C | 52 | `fn_800C5F3C` | `TReservedAverage<float, 15>::GetValue(int)` | **100.00%** |
| 0x800C5070 | 68 | `fn_800C5070` | `TReservedAverage<CVector3f, 5>::GetValue(int)` | **100.00%** |
| 0x800C5024 | 76 | `fn_800C5024` | `TReservedAverage<CQuaternion, 5>::GetValue(int)` | **100.00%** |
| 0x800C2004 | 108 | `fn_800C2004` | `TReservedAverage<CVector3f, 15>::GetAverage()` | **100.00%** |
| 0x800C1FAC | 88 | `fn_800C1FAC` | `TReservedAverage<float, 15>::GetAverage()` | **100.00%** |

## 6. Which instantiation is which: the caller's object offset, not the name

The four `TReservedAverage` members in `include/MetroidPrime/Player/CMorphBall.hpp` are at
0xE74 / 0xEC8 / 0xF08 / 0xF48, and **every** one of these twelve functions is called with exactly
one of those as its receiver — which is what pairs the twelve functions to the twelve members:

```
bl fn_800C5070  0x800C46C8  addi r4,r31,3784   0xEC8  mulli r5,r5,12   12B  CVector3f, 5
bl fn_800C5024  0x800C46D8  addi r4,r31,3700   0xE74  slwi  r5,r5,4    16B  CQuaternion, 5
bl fn_800C5F3C  0x800C5A5C  addi r4,r30,3848   0xF08  slwi  r0,r5,2     4B  float, 15
bl fn_800C1FAC  0x800C1C84  addi r4,r30,3848   0xF08  -                     float, 15
bl fn_800C21C4  0x800C1C30  addi r3,r30,3848   0xF08  slwi  r0,r0,2     4B  float, 15
bl fn_800C2070  0x800C1C3C  addi r3,r30,3912   0xF48  mulli r0,r0,12   12B  CVector3f, 15
bl fn_800C2004  0x800C1C48  addi r4,r30,3912   0xF48  -                     CVector3f, 15
bl fn_800CB380  0x800CB1D0  addi r3,r30,3700   0xE74  slwi  r0,r0,4    16B  CQuaternion, 5
bl fn_800CB22C  0x800CB1E8  addi r3,r30,3784   0xEC8  mulli r0,r0,12   12B  CVector3f, 5
```

The **element stride** (`slwi ...,2` / `mulli ...,12` / `slwi ...,4`) is what pairs the two
same-offset members apart (`GetValue` vs `AddValue` on 0xF08), and the `cmpwi` immediate is `N`
(`15` for 0xF08/0xF48, `5` for 0xE74/0xEC8) — a measurement too, and the only thing separating the
two 12-byte pairs. The element *type* is this file's choice where the stream does not force it, and
for `fn_800CB380` it is forced: `lfs` + three `lwz` is a `CQuaternion` (a `float w` then three more
words) and nothing narrower. Same method as the `fn_800D0xxx` fills above already use.

## 7. The one spelling that mattered: branch polarity on the `GetValue`/`GetAverage` pair

Six functions turned on this and nothing else. Retail's shape is

```
lwz r0,0(r4) / cmpw r5,r0 / blt <in-range>       <- branch INTO the value path
  li r0,0 / stb r0,<flag>(r3) / blr              <- fall-through is the null return
  <in-range>: li r5,1 / stb r5,<flag>(r3) / copy / blr
```

so the out-of-range return must be the **`if` arm and the value the `else` arm**:

```cpp
if (index >= self->mCount) { return rstl::optional_object_null(); }
else                       { return self->data()[index]; }
```

| spelling | `fn_800C5F3C` |
|---|---|
| `if (index < mCount) { return data()[i]; } return null;` | 53.08% |
| the same with an explicit `else` | 53.08% |
| `return index < mCount ? data()[i] : null;` (ternary) | 53.08% |
| **`if (index >= mCount) { return null; } else { return data()[i]; }`** | **100.00%** |

The first three emit `bge` over the value path with the null return as the fall-through, where
retail has `blt` into it — 6 of 13 instructions in the wrong order. mwcceppc normalises the
conditional expression back to the branch it would have picked for the plain `if`, so the `?:`
spelling buys nothing. The same polarity (`mCount == 0` first) is what `fn_800C1FAC` and
`fn_800C2004` need, and it is why they are 100% too.

The return type is **`rstl::optional_object<T>`** and that is forced, not chosen: its `m_valid` byte
sits at `+sizeof(T)` (`include/rstl/optional_object.hpp`), which is exactly retail's flag offset —
+16 for the quaternion, +12 for the vec3, +4 for the float — and its converting constructor is
`m_valid(true) { construct<T>(m_data, item); }`, which is what puts the `stb 1` **before** the
element copy rather than after it.

## 8. `fn_800C8CE0` — a measured, narrow wall (74.05%, 14 of 19)

`fn_800C8D2C` is 100% and `fn_800C8CE0` is its one-line caller, `erase(it) = erase(it, it + 1)`, and
it does **not** reach 100%. The remaining diff is entirely **one dead stack slot and the frame
size**: retail has a 32-byte frame with three slots (`stw r7,0x8(r1)` / `stw r7,0xc(r1)` /
`stw r0,0x10(r1)`, `r7 = *it + 8`, `r0 = *it`) and passes `r5 = r1+0x10` and `r6 = r1+0xc`; the
slot at +8 is never read. Ours has 16 bytes and two slots. Six spellings, all measured:

| spelling | score |
|---|---|
| `first` / `last` named locals, passed by address | 73.37% |
| the same, three named locals (best) | **74.05%** |
| `first` / `last` as `iterator*` locals | 73.37% |
| an `iterator args[2]` array | 71.63% |
| an `iterator args[3]` array | 61.05% |
| iterator passed **by value** | 73.37% |

74.05% is 14 of 19 instructions with the instruction multiset identical; what is left is the third
slot's provenance and the frame that goes with it. I could not find the spelling that makes MWCC
materialise a *third* `pointer_iterator` temporary that is spilled and then unused. **Do not spend
another run on the array and by-value forms** — they are measured and they are worse.

WALL: fn_800C8CE0 74.05% - identical instruction multiset, 14/19; the diff is one unread third stack slot and the 32- vs 16-byte frame; six spellings measured

## 9. `fn_800D0640` — the receiver is `reserved_vector`, not `rstl::vector`

`fn_800D0640` is the fifth link of the teardown chain `fn_800CEF2C` starts, and it is `fn_800CF02C`
with the buffer pointer dropped: the count comes from `lwz r6,0(r31)` (**+0**, not +4), the cursor
is `li r3,0` stepping `addi r3,r3,8` (**not** a loaded `mItems` at +12), and there is no
`Free(mItems)`. So the receiver has **inline storage with the count at +0**, and the loop is
`reserved_vector::destroy_elements`' own **index** loop — whose `T* ptr = data()` local disappears
precisely because `rstl::destroy` compiles to nothing for this element, leaving the induction
variable as a plain index. That is the whole difference from an `rstl::vector` walk and it is what
separates 100% from 61.11% (the vector-shaped spelling). `destroy_elements` is private, so a
one-method `SDead8Owner` re-spells it rather than reaching inside.

## 10. The `erase` pair's element is 8 bytes, and its null test survives

`fn_800C8D2C`'s copy is `lhz`/`sth` at +0 and `lfs`/`stfs` at +4 — the same 2-byte-plus-float pair
the `fn_800D0548` comment above already identifies as `TUniqueId` plus a float, i.e. this file's
existing `SUniqueIdFloats` value type. Two things in its 39 instructions are measurements:

- the leading `destroy(first, last)` walk is **empty**, and so is the `destroy(&*it)` in the move
  loop — the element's destructor is trivial, so the call drops and only the walk survives. This is
  the same fact as the dead walk in `fn_800CF02C` and `fn_800D0640`, for the same reason;
- the move loop **keeps** `construct`'s placement-new null test (`cmplwi r8,0` / `beq`), i.e. the
  element is **not** `RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE` — exactly the `EWakeEffectIndex`
  argument `fn_800C084C` already makes in this file.

`fn_800C7674` is `reserved_vector::erase(iterator)`, the element being 2 bytes (`lhz r0,2(r6)` /
`sth r0,0(r6)` / `addi r6,r6,2`, with the trip bound rebuilt inside the loop as
`slwi r0,count,1` / `add r3,self,r0` / `addi r0,r3,2` = `end() - 1`) — `TUniqueId`. Its
`destroy(end() - 1)` is **absent** from retail's 30 instructions, which is the measurement that the
element is trivially destructible here.

## 11. Decl order, and the one thing that bit me doing this

The new `extern "C"` block is declared **descending by retail offset**
(0x800D0640 → 0x800C8D2C → 0x800C8CE0 → 0x800CB380 → 0x800CB22C → 0x800C21C4 → 0x800C2070 →
0x800C7674 → 0x800C5F3C → 0x800C5070 → 0x800C5024 → 0x800C2004 → 0x800C1FAC), verified against
`nm` order in the built object, because the block has to be **moved** to sit after `fn_800C084C`
(0x800C084C is the lowest of the pre-existing `extern "C"` names). Two things cost a build each and
are worth writing down:

- reordering the block by line range **split the `CMORPHBALL_WRITE_ADD_VALUE` macro away from its
  `#undef`**, and a `#define` whose body lost its line continuations is a wall of
  `declaration syntax error`. Keep the `#undef` last of the four invocations, in descending order;
- the reorder also **dropped the block's intro comment and the three `SOptional*` typedefs**, which
  is how the first post-reorder build failed. They are restored and now carry the §6/§7 evidence.

`tools/check_decl_order.py --unit main/MetroidPrime/Player/CMorphBall` still reports the unit as
permuted, on the **member** functions (`__ct__` vs `DeleteBallShadow` and so on) — that is
pre-existing and is not this change's to fix; the new `extern "C"` block itself is in order.

## 12. What the judge said

```
$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11334 -> 11346   linked 5507 -> 5507
  ok    check_symbol_names.py
  ok    All:  32.64% fuzzy, 25.29% matched, 11.94% linked (11346 / 28465 functions)
  ok    target rose: main/MetroidPrime/Player/CMorphBall: 85 -> 97 / 158 functions
  ok    no asm added
goal_check: PASS def-fn-800CD460
```

```
$ python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json
matched  11334 -> 11346   linked  5507 -> 5507   (+12 functions at 100%, 0 units newly linked)
  +100%    main/MetroidPrime/Player/CMorphBall :: fn_800C1FAC  (and 11 more, all this unit)
no regression

$ ./tools/link_check.sh
link_check: unique undefined symbols 250
link_check: unchanged from baseline (250 undefined, 0 duplicates)
```

The port link is unchanged at 250, and **the reason is worth recording** because it is a trap:
`fn_800C2004` calls what retail names `fn_8001C95C`, and that symbol is **undefined tree-wide**.
Calling it would have done to this change exactly what `progress-prime1-cphysicsactor` recorded
doing to `fn_800CD460`. It is not called: the three argument registers identify the callee as
`GetAverageValue<CVector3f>(const CVector3f*, int)` — `addi r3,r1+8` is the hidden out-pointer a
12-byte class return uses under this ABI, `addi r4,r4,4` is `data()`, and the count is already in
`r5` from the guard's own `lwz r5,0(r4)` — and that instantiation comes from
`include/Kyoto/TAverage.hpp`, so it is emitted **weak and local** and costs nothing.

`docs/HANDOFF.md` is **not** my edit: `gate.sh` rewrote the derived state block itself. I have not
touched `docs/HANDOFF.md`, `docs/RUNNING_THE_DECOMP.md` or `docs/LANE_BRIEFING.md` by hand, and I
have not committed.

## 13. Still unlanded on this tree, re-measured (so the next run does not re-derive it)

- **`fn_800CD460` / `fn_800CD4B8`** (the item's own target) — unchanged from run 1. Blocked on
  `fn_8033D2F4` living in the dtk asm unit `main/auto_03_8033D2EC_text`. Still the same
  `NEW:` from run 1; not re-filed.
- **`fn_800C33DC` / `fn_800C88C0`** (0x800C33DC and 0x800C88C0, 92 bytes each) are two more links
  of the teardown chain and look like easy 100%s — **but they store vtable pointers** (`lis
  r3,-32709` / `addi r0,r3,14064` → `stw r0,0(r31)`, i.e. 0x803B36F0 and 0x803B1750,
  `config/G2ME01/symbols.txt:17971,18099`), so they need the class's vtable to exist at the right
  address first. Not attempted; not claimed.
- **`fn_800CD244` / `fn_800CD35C`** (280 and 260 bytes) are the two links immediately before
  `fn_800CD460` and are the same `extwi`/`rlwimi` refcount-bit family; `fn_800CD35C` calls
  `fn_80258790` / `fn_802588DC` and `fn_800CD244` calls `CPlane`'s constructor, so both need those
  checked against the port's undefined list first. Not attempted.
- **`GetEmitterTime__11CElementGenCFv`** (8 bytes, `lwz r3,104(r3); blr`) is retail's out-of-line
  copy of an inline virtual that `include/Kyoto/Particles/CElementGen.hpp:109` already defines
  (`return mCurFrame`). It is 0% only because nothing forces the vtable emission. Cheap if someone
  wants it; not attempted.
- **`fn_800D0024` / `fn_800D0064` / `fn_800D01EC` / `fn_800D022C` / `fn_800D02F8` / `fn_800D0338`**
  and **`fn_800C9380` / `fn_800C93B0`** — carried over from run 1's list, still 0%.
