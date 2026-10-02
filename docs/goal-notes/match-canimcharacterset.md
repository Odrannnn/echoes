---

# `fn_8028E8C4` matched: **15 / 16 -> 16 / 16**, unit 90.82% -> **100.00%** fuzzy

Re-measured first, as always: on the clean tree `report.json` read
`main/Kyoto/Animation/CAnimCharacterSet: 90.82% fuzzy (15 / 16 functions)` with
`fn_8028E8C4  0.00%  144 bytes` the only one left, and `All: 34.78% fuzzy, 28.33% matched,
12.90% linked (12305 / 28465 functions)`. The previous run's work
(`docs/goal-notes/progress-unit-canimcharacterset.md`) is in the tree; none of it was redone.

## The one thing this run found, and it is a correction of that note

The note records `fn_8028E8C4` as **blocked**, and gives the reason as a naming wall: three vptr
stores mwceppc emits only inside a real destructor, whose tables "could not be named", so
"the stores could be written by hand if the tables could be named". **Both halves of that are
wrong, and together they are the whole of this item.**

Measured, in this tree:

1. **The stores can be named and written by hand.** `lbl_803B9240` *is* in
   `config/G2ME01/symbols.txt` - line 18458, `lbl_803B9240 = .data:0x803B9240; // type:object
   size:0x10` - and it is an ordinary DOL data object (`build/binutils/powerpc-eabi-nm
   build/G2ME01/main.elf` -> `803b9240 D lbl_803B9240`), so `extern const char lbl_803B9240[];`
   resolves. Declaring it costs no `.data` bytes because mwldeppc emits no vtable of our own.
   `src/MetroidPrime/CConsoleOutputWindowCtor.cpp` is the existing precedent for exactly this
   idiom and for the three words it produces.
2. **Which** table is stored **does not matter.** `lis r3,X@ha / addi r0,r3,X@l / stw r0,0(r30)`
   is the same three instruction words for every `X`; the address lives in the relocation, and
   `configure.py`'s `progress_report_args` sets `functionRelocDiffs=none` (the default). That is
   the same measured fact the note already records for `fn_8028EC7C` (`@stringBase0` vs
   `lbl_803AED58` at 100.00%), applied one function further.

There was a much simpler observation available and the note did not make it: **our object already
contained the function, word for word, at retail's own offset.** mwceppc emits the deleting
destructor of `TObjOwnerDerivedFromIObj< CAnimCharacterSet >` at object offset `0x108` as
`__dt__45TObjOwnerDerivedFromIObj<17CAnimCharacterSet>Fv`, 36 instructions, and retail's object
has `fn_8028E8C4` at `0x108`, 36 instructions. Dumped both with `objdump -dr` and compared the
mnemonics: **identical, all 36.** The function scored 0.00% purely because objdiff pairs
functions by *symbol name* and no C++ declaration can rename a template instantiation's
destructor. So the work was never to make the compiler produce the code - it was to produce the
same 36 instructions under `fn_8028E8C4`.

## `fn_8028E8C4` - the spellings, so the next run does not repeat them

It is `TObjOwnerDerivedFromIObj< CAnimCharacterSet >::~TObjOwnerDerivedFromIObj()`, the deleting
destructor. All scores are objdiff's fuzzy match for that function, measured with
`./tools/decomp_build.sh Kyoto/Animation/CAnimCharacterSet`.

| spelling | score | what it shows |
| --- | --- | --- |
| `static_cast<...>(self)->~TObjOwnerDerivedFromIObj()` / `->TObjOwner::~TObjOwner()` (from the previous note, re-measured logic unchanged) | 58.31% | mwceppc inlines the body and drops all three vptr stores - a direct destructor call needs no fixups |
| `delete static_cast<...>(self)` (previous note) | 53.19% | the delete stays **virtual**: `lwz r12,0(r3) / lwz r12,8(r12) / mtctr / bctrl`, where retail calls `fn_8028E954` directly on `m_objPtr` |
| two `void**` vptr stores plus `if (self != nullptr)` guards on each | **97.08%** | 37 instructions to retail's 36; the only difference is a redundant `cmplwi r30,0` at `+0x5c` |
| same, but the vptr stores go through a local 2-word struct instead of `void**` | 97.08% | identical output - mwceppc does not care that the store cannot alias the tested pointer |
| same, guards written `if (self)` / `if (owner != nullptr)` / nested | 97.08% | all identical; mwceppc does **not** CSE two source-level tests of the same variable |
| `if (self != nullptr) { store base } else { store iobj }` | 98.19% | 36 instructions, right count, wrong shape - retail has the branch *between* the two stores |
| **outer `if (self != nullptr)` wrapping the base store, inner `if (self == nullptr) { } else { store iobj }`** | **100.00%** | the winning spelling |

The winning spelling, and the rule it turns on:

```cpp
extern "C" void* fn_8028E8C4(void* self, int flag) {
  if (self != nullptr) {
    void** vtable = reinterpret_cast< void** >(self);
    TObjOwnerDerivedFromIObj< CAnimCharacterSet >* owner =
      static_cast< TObjOwnerDerivedFromIObj< CAnimCharacterSet >* >(self);
    *vtable = const_cast< char* >(lbl_803B9240);
    if (owner->GetContents() != nullptr) {
      fn_8028E954(owner->GetContents(), 1);
    }
    if (self != nullptr) {
      *vtable = const_cast< char* >(__vt__31CObjOwnerDerivedFromIObjUntyped);
      if (self == nullptr) {
      } else {
        *vtable = const_cast< char* >(__vt__4IObj);
      }
    }
    if (static_cast< short >(flag) > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}
```

> **mwceppc reuses a `cmplwi`'s CR0 bit for a later branch when the two branches share a target
> block, and does not when they do not.** Retail's dead `beq` at `+0x5c` is exactly this: its own
> destructor codegen CSE'd the second `cmplwi r30,0` of the base-destructor sequence but kept the
> branch, and both branches target `0x16c`. Spelling the second test as an *empty* `if` with an
> `else` gives both branches the same target and mwceppc emits retail's shape exactly. Spelled as
> two ordinary `if`s the two branches target different blocks and the compare is re-emitted.

Two smaller points the body records:

- `m_objPtr` is `protected` in `include/Kyoto/IObj.hpp`, so it is reached through the class's own
  public `GetContents()`, which inlines to the `lwz r3,4(r30)` retail has. No header change was
  needed (the earlier run's `private:` -> `public:` on the two constructors stands, and is still
  needed by `fn_8028EC7C`).
- `flag` is compared as a `short`, exactly as in the other five deleting destructors in this file,
  because retail sign-extends from 16 bits inside the function.

## Why the unit still does not flip - measured, and pre-existing

`./tools/flip_test.sh Kyoto/Animation/CAnimCharacterSet.cpp` -> **FAIL**, `kept: 0 / 1`. The
failure is **not** a link error: with the unit flipped the link succeeds and
`build/G2ME01/main.dol`'s sha1 is wrong, because our object is **6924 bytes longer than the
range `config/G2ME01/splits.txt` claims for this unit** and nothing dead-strips it. Measured on
the flipped build: `fn_8028E8C4` lands at `0x8028F244` instead of `0x8028E8C4`, and
`__vt__4IObj` at `0x803B1F1C` instead of `0x803B16DC`.

`./tools/unit_fit.sh Kyoto/Animation/CAnimCharacterSet.cpp`:

```
.text      claimed   1568   ours   8492   retail   1568   over by 6924
70 function(s) present in ours but not in the retail unit object, 6924 bytes total
```

**Both numbers are unchanged by this run** (the previous run measured 70 / 6924 too), because
`fn_8028E8C4` is a function retail's object *has*, so adding it removes nothing from the extras
list. The extras are the whole `CAnimationSet` / `CCharacterInfo` / `CPASAnimState` template tree
the headers drag in (`__dt__45TObjOwnerDerivedFromIObj<17CAnimCharacterSet>Fv`,
`__dt__13CAnimationSetFv`, `destroy_impl<...>` x many, ...). The earlier run also recorded that
this object carried the 70 extras, so the flip was already out of reach before this item and
nothing here made it worse.

I also measured the clean tree's flip so the claim is not recalled: on `git checkout` of the source
with the unit marked `Matching`, `flip_test.sh` fails with
`### mwldeppc.exe Linker Error: # undefined: 'fn_8028E8C4'` - the link cannot even complete
without this function. So the two failures are different failures, and this run moved the unit
from "will not link" to "links but the bytes are displaced".

## Verification

- `./tools/goal_check.sh build/goal/item.json` ->
  **`goal_check: PARTIAL match-canimcharacterset - flip_test ...: FAIL, but the target rose; commit it and keep the item`**
  - `ok  no judge-owned path touched`
  - `ok  gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)`
  - `ok  counts: matched 12305 -> 12306   linked 5863 -> 5863` - `linked` unmoved, so no unit got
    worse and nothing was added to the link
  - `ok  target rose: main/Kyoto/Animation/CAnimCharacterSet: 15 -> 16 / 16 functions`
  - `ok  no asm added`, `ok  check_symbol_names.py`
- `sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (unit left
  `NonMatching`; `configure.py` is unchanged from the clean tree).
- `python3 tools/check_decl_order.py --unit CAnimCharacterSet` -> `ok: 1 unit(s) checked, none
  emits its functions out of retail order`. The new definition sits between `fn_8028E954`
  (0x8028E954) and `fn_8028E820` (0x8028E820), i.e. descending by retail offset, which is what the
  earlier run's trap note warns about.
- `python3 tools/check_raw_offsets.py` -> `ok: 167 raw-offset site(s) in 71 file(s), all
  documented` - **167 before this change too**, so it adds no raw offset: the vptr is `+0` of a
  known layout reached through a named accessor and the member through `GetContents()`.
- `python3 tools/check_docs_claims.py` -> `docs claims agree with the tree`.
- The generated body was diffed instruction by instruction against
  `build/G2ME01/obj/Kyoto/Animation/CAnimCharacterSet.o` at `0x108`: 36 instructions, identical,
  including the dead `beq` at `+0x5c` and both `beq`s targeting `+0x64`.

## NEW

None filed. The flip is blocked by the 70 pre-existing extra functions in this unit, which is a
measured wall (`unit_fit.sh`: 70 / 6924 bytes, unchanged by this run) rather than work whose
success raises a count on its own: killing those extras means cutting the `CAnimationSet`
template tree out of what this translation unit drags in, which is a different unit's problem and
would need `CAnimationSet.cpp` / `CCharacterInfo.cpp` decided first.