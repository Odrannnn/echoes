# match-canimationset — `Kyoto/Animation/CAnimationSet`, 66/67 -> **67/67**

**Result: PARTIAL.** The judge returns
`PARTIAL match-canimationset - flip_test ... FAIL, but the target rose; commit it and keep the item`.
Matched functions rose 11943 -> 11944 overall, and the target rose
`main/Kyoto/Animation/CAnimationSet: 66 -> 67 / 67 functions`. The unit is still `NonMatching`:
the flip is out of reach, for a measured reason given in full at the bottom. **Not `STALE`** - the
tree started at 66/67 and this run took the last function.

## What I did

Two independent changes, both required.

### 1. `fn_8028D524` 94.64% -> 100.00%: the blocker named in the comment is gone

The function's own comment said it could not be finished because
`src/Kyoto/Animation/CHalfTransition.cpp` did not exist, so `in.Get< CHalfTransition >()`'s
COMDAT referenced a `CHalfTransition` stream constructor no object in the tree defined. **That
unit has since been carved** (`ca2822ef`, `docs/goal-notes/match-fn8032250c-halftransition-stream-ctor.md`,
`Matching`), so the obvious spelling now links:

```c++
for (int i = 0; i < count; ++i) {
  self->push_back_unsafe(in.Get< CHalfTransition >());
}
```

replacing a `reinterpret_cast` 12-byte slot standing in for the temporary. The real
`CHalfTransition` in that slot gets retail's `cmplwi r30,0 / beq / addi r3,r30,4 / bl ReleaseData`
destructor instead of the cast's extra `addic. r0,r1,12 / beq` null test. Those three
instructions were the whole remaining diff. This is the general lesson the old note got wrong:
the obstacle was a missing symbol in the tree, **not a codegen wall**, and it evaporated the
moment another lane supplied the symbol.

`fn_8028D604`'s third parameter lost its `= TType< CHalfTransition >()` default at the same time -
it existed only so the old `fn_8028D524` could call it with two arguments, and with that call gone
nothing defaults it. Measured with it removed: the unit stays 100.00% (67/67).

### 2. Declaration order: the unit was permuted, and nothing reported it

`python3 tools/check_decl_order.py --unit Kyoto/Animation/CAnimationSet` said
`would break on a flip`, and the full order showed **seven** misplaced definitions. mwcceppc emits
in reverse source order, so each of these landed at the wrong address:

| function | retail | was declared | belongs above |
|---|---|---|---|
| `fn_8028D950` | 0x8028D950 | after `fn_8028CCC8` | `fn_8028D9D4` |
| `fn_8028DB68` | 0x8028DB68 | beside `fn_8028D440` | `fn_8028DB8C` |
| `fn_8028DB8C` | 0x8028DB8C | after `fn_8028DB48` | `fn_8028DB68` |
| `fn_8028DD1C` | 0x8028DD1C | after `fn_8028DC10` | `fn_8028DC60` |
| `fn_8028DC60` | 0x8028DC60 | after `fn_8028DC10` | `fn_8028DC10` |
| `fn_8028DF98` | 0x8028DF98 | after `fn_8028DF40` | `fn_8028DE98` |
| `fn_8028D524` | 0x8028D524 | after `fn_8028D4BC` | `fn_8028D604` |

`fn_8028D950` is the instructive one: **its comment block was already sitting in exactly the right
place** - between `fn_8028D9D4` and `CAnimationSet::CAnimationSet`, orphaned, with the definition
left behind with its siblings. Moving the definition up to its own comment is the whole fix.

Each moved definition carries a note saying which sibling it now sits above and why, so the next
lane does not re-derive it. The unit now reports
`ok: 1 unit(s) checked, none emits its functions out of retail order`.

**This corrects `docs/research/decl_order.md`,** whose entry said 51/67 and claimed
`fn_8028D950` "lands between `fn_8028CCA8` and `fn_8028CCC8` whatever its source position,
measured by declaring it before and after `fn_8028D9D4` (same result both times)". **That
measurement was wrong.** Declaring it before and after `fn_8028D9D4` is the same experiment - both
positions are on the correct side of the `fn_8028CCC8` block - so it could not have distinguished
anything. The correct position was never in that block at all. I replaced the entry with a
removed-entry record in the file's existing convention. The gate requires this: it fails on a
listed unit that is no longer permuted.

## What is measured

```
main/Kyoto/Animation/CAnimationSet: 100.00% fuzzy, 100.00% matched (67 / 67 functions)
All:  33.75% fuzzy, 26.93% matched, 12.64% linked (11944 / 28465 functions)
sha1sum build/G2ME01/main.dol   6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
tools/probe_sources.sh          746 files, 0 failed, 0 errors; link: LINKED (324 undefined, 0 duplicates)
check_symbol_names.py           checked 516 units; 0 declared names are missing from their object
check_decl_order.py             ok: 979 unit(s) checked, 30 permuted, all 30 accounted for
tools/goal_check.sh             PARTIAL ... but the target rose; commit it and keep the item
                                  ok  target rose: 66 -> 67 / 67 functions
                                  ok  no asm added
```

## What stops the flip (measured, not a codegen wall)

**The object's `.text` is 7884 bytes larger than the 7220-byte claim, and the excess is placed
after the claim, so every symbol above it moves.**

```
tools/unit_fit.sh Kyoto/Animation/CAnimationSet.cpp
   .text      claimed   7220   ours  15104   retail   7220   over by 7884
   83 function(s) present in ours but not in the retail unit object, 7884 bytes total
```

These are COMDAT weak copies and template instantiations mwldeppc keeps because they are called -
`reserve__Q24rstl52vector<15CHalfTransition,...>`, the five
`__ct__Q24rstl4xvector<...>FR12CInputStream` stream constructors, `uninitialized_copy`,
`destroy_impl`, and so on. `-strip_partial` cannot drop them.

The consequence is a clean 664-byte shift, and it is the only thing wrong:

```
FAnimCharacterSet__FRC10SObjectTagR12CInputStreamRC15CVParamTransfer
   retail 0x8028E7BC  (exactly the claim's end)
   ours   0x8028EA54  ->  +0x298 = +664 bytes
```

Diffing the claim between the two builds gives **932 differing instructions out of 1805, and 784
of them differ in the mnemonic, not just the operand** - the instruction stream is displaced, not
merely retargeted, because everything downstream of the claim moved up by 664. The diff is
dominated by `bl` targets landing 664 bytes off and `addi rX,rY,-4792` where retail has
`-5432` (the same 640-byte `addi` immediate delta, which is 664 rounded into a 16-bit field's
sign/alignment).

**This is pre-existing and not caused by this item.** Measured directly: I restored the pristine
`CAnimationSet.cpp` from `.tmp/CAnimationSet.cpp.bak`, flipped to `Matching`, and rebuilt.
`main.dol` = `10c22a53ed2d4dc538f4410d02a511d12557a89d`, still not retail's
`6ef9b491...`. The pristine unit also overran its claim, and by slightly more
(`.text` 0x3b08 = 15112 bytes vs my 0x3b00 = 15104 - the 8-byte difference is the
`fn_8028D604` default argument I removed). **The overflow got 8 bytes smaller; it did not appear.**

**Why it cannot be fixed from inside this unit.** Retail emitted these template instantiations in
*other* translation units - `src/MetroidPrime/Carve80049244.cpp`'s header is the full account, and
`src/Kyoto/Animation/CHalfTransition.cpp` cites it for the identical `rc_ptr` problem. Ours cannot
be moved out of this one. C++98 has no `extern template`, and mwceppc has no
`-fno-implicit-templates`. Getting to 7220 bytes means either a compiler flag (a global change,
not an item) or restructuring the unit to stop instantiating the templates, which is a bigger job
than a carve. `Kyoto/Animation/CTransition.cpp` carries the same 156 bytes past its own claim and
does flip, so the threshold is not zero - it is that *its* excess happens to fit. This unit's 7884
does not.

`tools/flip_test.sh` is the only thing that catches this, exactly as `AGENTS.md` says: objdiff
paired all 67 functions at 100.00% before and after the reorder, `unit_fit.sh` reported the same
sizes, and the link succeeded.

## For the next lane

The flip needs the `.text` excess down from 7884 to 0. The obvious lever is a compiler or linker
flag applied to this unit's compile line (`-fno-implicit-templates` or an equivalent), which is a
cross-cutting change and belongs in its own item with the whole tree re-gated. Check first with
`./tools/unit_fit.sh Kyoto/Animation/CAnimationSet.cpp` - the `.text` "over by" number is the
thing that has to reach 0, and it is currently 7884.

## Files

- `src/Kyoto/Animation/CAnimationSet.cpp` - `fn_8028D524` body; seven definitions moved; the
  `fn_8028D604` default argument dropped; the trailing "what is left" note rewritten.
- `docs/research/decl_order.md` - the `CAnimationSet` entry replaced with a removed-entry record
  (the gate fails on a listed unit that is no longer permuted).

No `asm`, no new stubs, no `configure.py` change (the unit stays `NonMatching` - the flip did not
hold, so it must not be marked `Matching`).
