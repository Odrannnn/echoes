# unit-MetroidPrime-CStateManager

`kind: progress`, target `MetroidPrime/CStateManager` (`main/MetroidPrime/CStateManager`,
`NonMatching`).

## Result

**92 -> 99 of 239 matched functions, +7.** Measured from `build/report.json` at the start of the
run and after it, not recalled. The unit's fuzzy went 14.4457% -> 15.6553%. The unit stays
`NonMatching` and is nowhere near flipping: 99/239, and the five `fn_800379xx` alone are still
96.06% each.

```
./tools/goal_check.sh build/goal/item.json
  goal_check: PASS unit-MetroidPrime-CStateManager
    ok  no judge-owned path touched
    ok  gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
    ok  counts: matched 11918 -> 11925   linked 5727 -> 5727
    ok  check_symbol_names.py
    ok  All:  33.70% fuzzy, 26.85% matched, 12.64% linked (11925 / 28465 functions)
    ok  target rose: main/MetroidPrime/CStateManager: 92 -> 99 / 239 functions
    ok  no asm added
```

`linked 5727 -> 5727` is the right shape for this item: a `progress` item adds functions to a
`NonMatching` unit, so nothing becomes link-complete and the number must not move. It did not.

## What landed

The three destructor families a previous run landed (`fn_800431C4`, `fn_80043510`, `fn_800436CC`
and their forwarders) each sit under a **second, unwritten layer** in retail: a counted array
whose elements are that family's object, and a deleting destructor over the array. All six of
those were unwritten, and they are written here. A fourth family, `fn_800437BC`/`fn_8004380C`, is
new.

| retail | offset | bytes | score |
| --- | --- | --- | --- |
| `fn_800430D0` | 0xCED0 | 80 | **100.00%** |
| `fn_80043120` | 0xCF20 | 96 | **100.00%** |
| `fn_8004341C` | 0xD21C | 80 | **100.00%** |
| `fn_8004346C` | 0xD26C | 96 | **100.00%** |
| `fn_800435D8` | 0xD3D8 | 80 | **100.00%** |
| `fn_80043628` | 0xD428 | 96 | **100.00%** |
| `fn_800437BC` | 0xD5BC | 80 | **100.00%** |
| `fn_8004380C` | 0xD60C | 124 | 95.65% (not counted) |

Seven counted, `fn_8004380C` written but not at 100% so not counted.

Both halves of each pair land together for the reason the earlier note
(`progress-cstatemanager-free-forwarders`) records: `fn_800430D0` calls `fn_80043120`, and a
call to a merely-*declared* function is one more undefined symbol in the host link, which
`probe_sources.sh` gates. Measured, not assumed: **324 undefined before, 324 after**, against
`docs/research/port_link_baseline.txt` (324) and `build/goal/judge/undef.base.count` (324).

## The three spellings that decided the array walks

All three walks (`fn_80043120`, `fn_8004346C`, `fn_80043628`) are the same code with a different
element size, and all three first measured **82.92%**. One change took all three to 100.00%.

**Retail hoists the element pointer out of the loop; the indexed `&m_items[i]` does not.**

```
retail  mr r29,r3 ; addi r31,r29,4 ; b test
        body:  mr r3,r31 ; bl leaf
        next:  addi r31,r31,76 ; addi r30,r30,1
        test:  lwz r0,0(r29) ; cmpw r30,r0 ; blt body

indexed ours (82.92%)
        mr r29,r3 ; mr r31,r29 ; b test
        body:  addi r3,r31,4 ; bl leaf          <- the +4 is INSIDE the loop
```

The element pointer is `self->m_items` (offset +4), and with `&self->m_items[i]` the compiler
hoists `self` into r31 and re-adds 4 on every trip. Spelling it as a walking pointer -

```cpp
SVectorOwner3* p = self->m_items;
for (int i = 0; i < self->m_count; ++i) {
  fn_80043180(p);
  p = reinterpret_cast<SVectorOwner3*>(reinterpret_cast<uchar*>(p) + sizeof(SVectorOwner3));
}
```

- moves the `addi` above the loop and the body becomes retail's `mr r3,r31`. 82.92% -> 100.00%,
  measured once on `fn_80043120` and then applied unchanged to the other two, where it also
  reached 100.00%.

Note the *test* block sits **below** the body in retail (`b` to the test, test branches back), so
these are written as indexed `for` loops and not as pointer-bounded `while`s. The test is
`cmpw r30,r0` on a register pair against the count re-read from `self+0` each trip.

The same three details from the earlier note still hold and are what make the forwarders land:
the flag parameter is a **`short`** (`extsh. r0,r31`, not `cmpwi`), the leaf **returns a
pointer** (`mr r3,r30`), and the forwarder passes **`-1`**.

## `fn_8004380C`: the element array is inline, and the inner loop stores nothing

`fn_800437BC` is the fourth family's forwarder and lands first try at 100.00% - it is
byte-for-byte the same shape as `fn_800430D0`. Its callee `fn_8004380C` (124 B, 0xD60C) is at
**95.65%**, with every instruction accounted for and the residue being register allocation
only. What took it there, in order, all measured:

| spelling | score |
| --- | --- |
| inline `[1]` array of `SByteBuf356`, `*p++ = 0` inner loop | 40.23% |
| ... `memset(buf->m_data, 0, buf->m_length)` | 4.58% |
| ... *descending* dead-counter inner loop | 49.10% |
| ... *ascending* dead-counter inner loop | 90.42% |
| ... pointer stepped at the **top** of the body | 82.71% |
| ... pointer stepped at the **bottom** of the body | 95.65% |
| ... `uchar*` base rather than a typed pointer | 95.65% (same) |
| ... naming the length / `(void)written` / dead pointer increment | 95.65% (same) |

Two things this establishes, both of which cost the earlier attempts:

1. **The elements are inline, not behind a pointer.** Retail seeds the element pointer once with
   `addi r5,r3,4` and then only ever steps it by 356. A `SByteBuf356*` member instead makes the
   compiler emit `lwz r0,4(r3)` and an `add.` to re-derive it, which is 86.26% against 95.65%
   for the inline array. The list is `{ int m_count; SByteBuf356 m_items[1]; }`.

2. **The inner loop stores nothing.** It is MWCC's memset expansion with the stores optimised
   away: `srwi r0,r4,3` for the 8-bytes-per-trip count, `subf r0,r3,r7` + `mtctr` for the
   one-byte remainder, `li r3,0` seeding the byte offset, and **no store instruction in the
   function**. So it cannot be written as `*p++ = 0` (that is 40.23% and gets a plain
   1-byte-per-trip loop) and it cannot be handed to `memset` (4.58%, real stores). It is a loop
   whose body is a **dead counter**, and the counter must be **ascending** for the unroll to
   fire - a descending one is 49.10% and does not unroll at all.

The residue at 95.65% is that the registers are rotated: retail holds count in r6, the element
pointer in r5, the length in r7, the index in r8 and the byte offset in r3; the same code here
holds them in r4, r6, r5, r7, r8. Every opcode, immediate, branch target and displacement
matches. Four further spellings aimed at the allocation (naming the count, naming the length,
`(void)written`, a dead pointer increment instead of an int) all left it at 95.65%. **Not
attempted:** forcing the offset register with a `volatile` or an explicit register variable -
that would be a codegen hack rather than a spelling, and this repo does not use them.

## Everything tried that did not land

- **`fn_8003ABF0` (0x8003ABF0, 248 B) - not written in the end.** It is a second block copy over
  the same 16-byte float elements as `fn_800391E4`, and unlike that one it is index-counted:
  retail unrolls the body 4x per trip (64 bytes) and has a `andi. r4,r4,3` remainder loop. Four
  spellings measured: indexed `for` 21.74%, pointer-bounded `for` 20.60%, explicit 4-way unroll
  2.82%. None is close, and the earlier note's WALL for the sibling `fn_800391E4` (14 spellings)
  suggests the same wall. **Reverted** rather than left in at a score that earns nothing and
  adds a function the reviewer would have to read.
- The `SendScriptMsg` pair (99.58 / 99.52%), the five `fn_800379xx` (96.06%), `fn_800391E4`
  (94.38%), `AllocateUniqueId` (83.31%) and `fn_80037784` (96.67%) were **not touched**. The
  first three are walls an earlier run characterised with exact bytes
  (`docs/history/decomp-findings.md`, and the comment block at `CStateManager.cpp:380`), and the
  last two are the `@stringBase0` wall, which is a section-claim property of the merged rodata
  and not a source problem. Re-running their spellings would have measured nothing new.

## Gates, all re-measured after the change

```
sha1sum build/G2ME01/main.dol   6ef9b491d0cc08bc81a124fdedb8bfaec34d0010   (unchanged)
./tools/probe_sources.sh        probe: 745 files, 0 failed, 0 errors;
                                link: LINKED (324 undefined, 0 duplicates)   (324 = baseline)
python3 tools/check_symbol_names.py   0 declared names are missing from their object
python3 tools/check_decl_order.py     ok: 978 unit(s) checked, 31 permuted, all 31 accounted for
./tools/decomp_build.sh          All:  33.70% fuzzy, 26.85% matched, 12.64% linked
                                  (11925 / 28465 functions)
```

`check_decl_order.py` reports CStateManager as permuted; that is **pre-existing** (it was in
`docs/research/decl_order.md` before this change) and the rule only bites on a flip, which this
unit is not close to. The new functions are declared descending by retail offset within their
block (0xD60C, 0xD5BC, 0xD428, 0xD3D8, 0xD26C, 0xD21C, 0xCF20, 0xCED0).

The one compiler warning in this unit (`CStateManager.cpp:1345`, "return value expected", in
`GetEditorIdForUniqueId`) is pre-existing and not in the diff.

## Ranges claimed

None. This is a DOL unit: no `splits.txt` change, no `configure.py` change, no carve, no `asm`.

## Files

- `src/MetroidPrime/CStateManager.cpp` - one new block, lines 283-438, sitting between the
  existing family-1/2/3 forwarders and the `fn_800391B4` block. It adds eight functions and the
  local types they walk (`SVectorOwner3Array`, `STokenVectorOwner24`, `SRecordArray488`,
  `SRecordArray488List`, `SByteBuf356`, `SByteBuf356List`), each with a `CHECK_SIZEOF` tying the
  stride to the element size rather than a literal. No header, no `configure.py`, no `splits.txt`.

`docs/HANDOFF.md` is also dirty in this worktree, rewritten by `goal_check.sh`'s own
`gate.sh` run (the judge runs it with `MP_GATE_DOCS_WRITE=1` and derives the state block from
the tree). Not hand-edited; the driver discards it.

## No NEW:

Nothing new is blocked. `fn_8004380C` at 95.65% and `fn_8003ABF0` are spelling questions inside the
unit this item already targets, not separate units, so neither warrants a `NEW:` line. What the
unit needs next is `fn_80042C80` and `fn_80042C28` (the `__dt__13CStateManager` sub-tree, which
calls five out-of-unit symbols - `fn_80234474`, `__dt__14CRumbleManagerFv`, `fn_80042FAC`,
`fn_8004330C`, `fn_80045D6C` - none of which any source file defines, so it is a link-closure
job before it is a decompilation one).

---

# Second run (lane 3, 2026-10-02) - 101 -> 108 of 239, +7

`kind: progress`, target `MetroidPrime/CStateManager` (`main/MetroidPrime/CStateManager`,
`NonMatching`). Re-measured on this tree before touching anything: the previous run's work had
landed and two functions more had come from upstream, so the start line was **101**, not the 99
the note above ends on.

## Result

**101 -> 108 of 239 matched functions, +7.** From `build/report.json` at the start of the run and
after it. The unit's fuzzy went 17.419659% -> 18.770926% and its matched code 12.845442% ->
13.830991%. The unit stays `NonMatching` and is nowhere near flipping.

```
./tools/goal_check.sh build/goal/item.json
  goal_check: item unit-MetroidPrime-CStateManager (progress) target=MetroidPrime/CStateManager
    ok    no judge-owned path touched
    ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
    ok    counts: matched 12305 -> 12312   linked 5863 -> 5863
    ok    check_symbol_names.py
    ok    All:  34.79% fuzzy, 28.34% matched, 12.90% linked (12312 / 28465 functions)
    ok    target rose: main/MetroidPrime/CStateManager: 101 -> 108 / 239 functions
    ok    no asm added
  goal_check: PASS unit-MetroidPrime-CStateManager
```

`linked 5863 -> 5863` is the right shape: a `progress` item adds functions to a `NonMatching`
unit, so nothing becomes link-complete and the number must not move. It did not.

## What landed

| retail | offset | bytes | first try? |
| --- | --- | --- | --- |
| `fn_8003AD5C` | 0xAD5C | 24 | yes, 100.00% |
| `fn_8003AD28` | 0xAD28 | 52 | yes, 100.00% |
| `fn_80038E0C` | 0x8E0C | 84 | yes, 100.00% |
| `fn_80043888` | 0x3888 | 80 | yes, 100.00% |
| `fn_800416DC` | 0x16DC | 76 | no - 4 spellings, see below |
| `fn_8003EE44` | 0xEE44 | 116 | yes, 100.00% |
| `fn_80039A7C` | 0x9A7C | 160 | no - 4 spellings, see below |

Four of the seven landed first try, which is worth saying plainly: **the deleting-destructor
shape is exhausted, not walled.** `fn_80038E0C` is `fn_800437BC` with a `CMemory::Free` of a
member at +0x0C in place of the leaf, and `fn_80043888` is `fn_800437BC` with `fn_8003AD5C` as
the leaf. Both are the four-layer shape the previous run pinned, written blind. The next run
should spend its budget on the *walks* and the *copies*, not on more destructors.

`fn_8003AD5C` is the smallest thing in the unit: 24 bytes, six instructions, and **no store and
no call** - `lwz r0,0(r3) ; mtctr r0 ; cmpwi r0,0 ; blelr ; bdnz ; blr`. It is MWCC's downward
counted loop over an *empty* body, and the `blelr` (not `beq` to a separate `blr`) is the leaf
shape. So:

```cpp
for (int i = self->m_count; i > 0; --i) {
}
```

`fn_8003AD28` is that call plus `self->m_count = 0`.

## Three findings that are worth more than the seven functions

### 1. `&p->field == nullptr` is the spelling for MWCC's dead address test

`fn_80039A7C` and `fn_8004371C` both contain `addic. rX,rBase,K ; beq` immediately before
`lbz r0,K(rBase)`. That is a null test on the **address** of a member, not on its value - dead
code, because the address is `p + K`. Written as a null test against the member's address,
MWCC emits it and shares the register with the `lbz`:

| spelling of the flag test | fn_80039A7C |
| --- | --- |
| `uchar*` member, `p->m_flag != nullptr && p->m_flag[0] != 0` | does not occur in retail at all |
| `uchar` member, `p->m_flag != 0` only (no address test) | 40.23% on `fn_8004380C`; here the whole shape is lost |
| `uchar` member, `&p->m_flag == nullptr && p->m_flag != 0` | **98.25%** |

The earlier attempt in this file spelled the member as `uchar*` and dereferenced it, which makes
MWCC emit a real `lwz r3,K(rBase)` and costs the entire body. So the reading in the previous
note ("a pointer at +36 whose first byte is a flag") was **wrong**: it is a plain flag byte at
+0x24, and the `lwz` that a pointer would need is not in retail.

### 2. `continue` breaks the latch; retail's outer branches land on the element pointer's step

Retail's three (four, in `fn_8004371C`) `beq`s in these walks all target the latch's
`addi rPtr,rPtr,stride`, so the element pointer advances on **every** trip. Written as three
`continue`s in a `for`, MWCC jumps *past* the pointer step and the branches come out 4 bytes
further on - a silent infinite loop, and 99.62% instead of 100.00%:

| spelling | fn_80039A7C |
| --- | --- |
| three `continue`s, pointer stepped in the body | 99.62% (branches 4 bytes past the step) |
| one `if (a && b && c) { leaf(); }` with the step after it | **100.00%** |

The `&&` chain and the `if` are not stylistic: the branches have to target the step.

### 3. `clrlwi` keeps the LOW bits, and the tree key is masked to 26 of them

`fn_800416DC` / `fn_80041690` mask both keys with `clrlwi .,6`. Which C++ that is took three
attempts, and the mnemonic points the wrong way:

| spelling | emitted | score |
| --- | --- | --- |
| `k = key->m_key & ~static_cast<uint>(0x3F)` | `clrrwi r6,r0,6` (different opcode *and* register) | 76.21% |
| `k = key->m_key & 0x3F` | `clrlwi r5,r0,26` | 76.58% |
| `k = key->m_key & 0x3FFFFFF` | `clrlwi r5,r0,6` | **100.00%** |

So `clrlwi rA,rS,6` is `& 0x3FFFFFF` here, not `& 0x3F`, and `& 0x3F` is the *other* `clrlwi`
with the immediate complemented. **Read the immediate as `32 - log2(mask)`, not as the mask.**

The other two things that took `fn_800416DC` from 76% to 100%, both about which arm MWCC puts
out of line:

* The source's `if` condition names the operands the other way round from the branch. Retail
  0x800416DC tests `key < node_key` and emits `cmplw r5,r0 ; bge`; retail 0x80041690 tests the
  equivalent `node_key >= key` and emits `cmplw r5,r0 ; blt`. Writing 0x800416DC as
  `if (k >= n->m_key) { right } else { best; left }` puts the *then* arm in line; writing it as
  `if (k < n->m_key) { best; left } else { right }` puts the short arm (the single `lwz` to the
  right child) out of line, which is what retail has.
* The search key is read **first**, in its own statement, before `best` is initialised. That is
  what keeps the masked copy in r5 - the register the key pointer arrived in - instead of letting
  `best = nullptr` claim it. Same source, `best` first, is 76.58%.

`fn_80041618` (the caller of both) needed only the slot order: MWCC hands out the two 8-byte
result slots in the **reverse** of declaration order, so the 0x800416DC result is declared second
to land at r1+8.

## Everything tried that did not land - all measured this run

- **`fn_8004371C` (0x8004371C, 160 B): 77.67% -> 93.83%.** The pre-existing partially-matched
  walk. Finding 1 and finding 2 above plus two more took it from 77.67% to 93.83% and the
  instruction *sequence* is now identical to retail; what is left is register allocation and one
  constant. Retail: pointer r30, counter r29, token r31, address temp r3, `li r4,0` before the
  destructor call. Measured here: pointer r30, counter r31, token r29, address temp r0,
  `li r4,-1`. The counter and the token are swapped between r29 and r31 and nothing in the
  source moved them: declaring the counter *before* the pointer put the pointer in r30
  (93.83%) and declaring it after (88.33%) put it in r31, so the pointer's register is not
  something declaration order chooses on its own. `li r4,0` vs `li r4,-1` in the dead argument
  register of the `~CToken` call is unexplained; `-1` is what the `fn_800436A8` forwarder passes
  and it is not in this function's source, so it looks like MWCC's dead-argument filler picking
  up a constant from the caller. **Left in at 93.83%** - strictly better than the 77.67% it
  started at, and the structure is right.
- **`fn_80041690` (76 B): 98.42%, one instruction.** Retail masks the search key **in place**
  (`clrlwi r0,r0,6`, so the key stays in r0 and the node key goes to r5); this compiler always
  moves it into the freed key-pointer register. Two spellings aimed at the in-place mask - `k
  &= mask` as a second statement, and `const uint k` - both leave it at 98.42% and both emit
  `clrlwi r5,r0,6`. It is the same code as `fn_800416DC` with the two registers swapped, and
  0x800416DC *is* at 100.00%, so whatever decides it is not the source text.
- **`fn_80041618` (120 B): 99.20%, one instruction.** Prologue, both calls, both stack slots and
  all four stores are byte-identical to retail. The only difference is the epilogue: retail
  restores the link register first (`lwz r0,52(r1)` then r31, r30, r29), this compiler restores
  it last. Every other function in this unit that MWCC gets right - including four landed this
  run and `fn_80043120` - puts the `lwz r0` first, so this is not a convention difference but a
  per-function scheduling decision. Tried and measured, all still 99.20%: a `uint*` out-parameter
  instead of a typed one, the two results as one 16-byte stack object (**worse, 92.53%**), and an
  explicit `return;`. Not attempted: register-variable or `volatile` escapes, which this repo
  does not use.
- **`fn_80038C5C` (0x80038C5C, 64 B): 75.00%, reverted.** `{ u16 id; CLight light; }`, the id
  copied with `lhz`/`sth` and the light by the out-of-line `CLight` copy constructor this file
  already defines - the code is right and the call target is right, and the only difference is
  that retail hoists `lhz r0,0(r4)` and `mr r4,r5` *above* `stw r31,12(r1)` in the prologue while
  this compiler sinks them below it. 12 of 16 instructions. **Reverted** rather than left in at a
  score that earns nothing.
- **`ShouldUpdatePatterned` (132 B): 99.85%, not touched, and this is a header question.** Its
  four non-matching instructions are one real mismatch and three relocations objdiff cannot
  resolve. The real one is the bitfield index: retail does
  `lbz r0,10572(r3) ; rlwinm r0,r0,30,31,31` and this file does
  `rlwinm r0,r0,27,31,31`. MWCC's rotate amount is `31 - bit`, so **retail reads bit 1 of the
  byte at 0x294C and `bool mCinematicPause : 1` sits on bit 4** - the header's own comment says
  "0x294c bit 26", and the layout puts it two bits above where retail has it. That is not a
  spelling: the flag block is shared with `KillSaveGameInterface` (bit 28) and
  `CScriptSpecialFunction::fn_80107458` (bit 26), both in other units, so moving the block to
  reach bit 1 would move their bits too and regress matched functions elsewhere. Not attempted:
  a private accessor that reads the byte and masks it by hand (`andi.`, not `rlwinm` - wrong
  instruction), or a `volatile` read. **Left alone deliberately.**
- The `SendScriptMsg` pair, the five `fn_800379xx`, `fn_800391E4`, `AllocateUniqueId`,
  `fn_80037784`, `DeferStateTransition`, `fn_8004380C` and the rest of the sub-100% list were
  **not touched**: the first three groups are walls an earlier run characterised with exact
  bytes, and the last two are the `@stringBase0` wall. Re-running their spellings would have
  measured nothing new. `fn_8004380C` is still 95.65%, unchanged.
- **The 0x8003AB38/AB58/AB80/ABC0 chain was left alone.** All four are trivial wrappers and all
  four are unreachable without `fn_8003ABF0` (248 B, four spellings at 21.74% best in the previous
  run), and calling an undefined symbol would put the port's undefined count up, which the gate
  fails on. The same goes for `fn_8003ACE8`, `fn_8003AD84` and the whole 0x80043888-0x80043A5C
  sub-chain behind `fn_8003ABF0`/`fn_8003ADC4`/`allocate__Q24rstl17rmemory_allocatorFi`.

## How the callees were chosen (so the next run does not have to redo it)

Every function written this run has **no out-of-unit callee that is not already defined**, so
`probe_sources.sh` cannot move: `CMemory::Free` is `src/Kyoto/Alloc/CMemory.cpp:71` (it is
`_ZN7CMemory4FreeEPKv` under the host compiler, so a `nm` cross-check against retail's MWCC
mangled names reports it as undefined and lies - the previous note's `Free__7CMemoryFPCv: +1` in
any callee table is that trap). `fn_800416DC`/`41690`/`41618`, `fn_8003EE44` and `fn_8003AD5C`
call nothing at all. Measured, not assumed: **288 undefined before and after**, against
`build/goal/judge/undef.base.count` (288).

The cheap way to find more of these: for each candidate, list its `bl` targets from
`powerpc-eabi-objdump -r build/G2ME01/obj/MetroidPrime/CStateManager.o`, then keep only the
functions whose targets are all in the same unit, or already defined by the port.

## Gates, all re-measured after the change

```
sha1sum build/G2ME01/main.dol   6ef9b491d0cc08bc81a124fdedb8bfaec34d0010   (unchanged)
./tools/probe_sources.sh        probe: 752 files, 0 failed, 0 errors;
                                link: LINKED (288 undefined, 0 duplicates)   (288 = baseline)
python3 tools/check_symbol_names.py   checked 525 units; 0 declared names are missing
python3 tools/check_decl_order.py     ok: 981 unit(s) checked, 29 permuted, all 29 accounted for
./tools/decomp_build.sh          All:  34.79% fuzzy, 28.34% matched, 12.90% linked
                                  (12312 / 28465 functions)
```

The one compiler warning in this unit ("return value expected", now at `CStateManager.cpp:1694`,
the closing brace of `GetEditorIdForUniqueId`) is pre-existing and not in the diff.

## Ranges claimed

None. This is a DOL unit: no `splits.txt` change, no `configure.py` change, no carve, no `asm`.

## Files

- `src/MetroidPrime/CStateManager.cpp` - the only file touched. Two edits:
  1. **rewritten** `fn_8004371C` and the `SRecord44` layout (`uchar x24` where it was `uchar*`),
     lines 211-256;
  2. **one new block**, lines 471-701, between the existing `fn_800437BC` and the `fn_800391B4`
     block. Nine functions, in this source order: `fn_8003AD5C` (483), `fn_8003AD28` (491),
     `fn_80043888` (500), `fn_80038E0C` (518), `fn_80039A7C` (547), `fn_800416DC` (607),
     `fn_80041690` (623), `fn_80041618` (650), `fn_8003EE44` (686) - plus the local types they
     walk (`SCounted`, `SFreeBlock`, `SFree20`, `SFree20Array`, `STreeNode`, `STree`, `STreeKey`,
     `STreeIter`, `STreeRange`, `SFixed44`), each stride or offset tied to the instruction that
     fixes it by a `CHECK_SIZEOF` where the size is load-bearing. No header, no `configure.py`,
     no `splits.txt`.
  The block is grouped by shape (release families, the 0x80039A7C walk, the tree searches, the
  flat copy) and so is **not** in retail-offset order within itself. `check_decl_order.py`
  reports the unit as one of the 29 already accounted for in `docs/research/decl_order.md`,
  which is pre-existing and only bites on a flip this unit is not close to. If this unit ever
  gets near a flip, this block has to be re-sorted descending by retail offset (0xEE44,
  0x43888, 0x416DC, 0x41690, 0x41618, 0x39A7C, 0x38E0C, 0x3AD5C, 0x3AD28) - and so does the
  0x800430D0-0x8004380C block above it, which the previous run left ascending.

## No NEW:

Nothing new is blocked. Everything still open is a spelling or an allocation question *inside*
the unit this item already targets, which the prompt says to put in this file rather than in a
`NEW:` line. The one thing that would be worth a separate item is the `CStateManager` flag block
at 0x294C: `mCinematicPause` is two bits above where retail reads it, and the block is shared
with two other units' matched code, so fixing it is a cross-unit header change rather than a
spelling - but it is one function of this unit, so it is a `progress` item on this same target,
not a new one.
