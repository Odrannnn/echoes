# progress-unit-canimcharacterset

`Kyoto/Animation/CAnimCharacterSet`: **2 / 16 functions matched -> 12 / 16**, matched code
196 -> 1072 bytes, unit fuzzy 12.50% -> 67.60%. Judge: `goal_check: PASS`. All 86 RELs and
`main.dol` still reproduce retail (`sha1 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`).

## What the unit actually is

Re-measured, and the item's `reason` was misleading in a way that matters: this is not a unit
whose functions are *unwritten*. All sixteen were already in the object, byte-for-byte, before
this change. `python3 tools/fnmap.py Kyoto/Animation/CAnimCharacterSet` on the clean tree
reported **IDENTICAL** for all sixteen, pairing each of retail's `fn_<address>` with a
*different* symbol of ours:

| retail | size | ours (already byte-identical, wrong name) |
| --- | --- | --- |
| `fn_8028E820` | 164 | `__ct<17CAnimCharacterSet>__16CFactoryFnReturnFP17CAnimCharacterSet` |
| `fn_8028E8C4` | 144 | `__dt__45TObjOwnerDerivedFromIObj<17CAnimCharacterSet>Fv` |
| `fn_8028E954` | 100 | `__dt__17CAnimCharacterSetFv` |
| `fn_8028E9B8` | 88 | `__dt__13CCharacterSetFv` |
| `fn_8028EA10` | 132 | `__dt__Q24rstl68vector<Q24rstl24pair<i,14CCharacterInfo>,...>Fv` |
| `fn_8028EA94` | 56 | `destroy<...pointer_iterator<pair<i,14CCharacterInfo>...>>` |
| `fn_8028EACC` | 80 | `destroy_impl<...>` |
| `fn_8028EB1C` | 32 | `destroy<pair<i,14CCharacterInfo>>*` |
| `fn_8028EB3C` | 36 | `destroy_impl<pair<i,14CCharacterInfo>>*` |
| `fn_8028EB60` | 88 | `__dt__Q24rstl24pair<i,14CCharacterInfo>Fv` |
| `fn_8028EBB8` | 152 | `__dt__13CAnimationSetFv` |
| `fn_8028EC50` | 44 | `GetIObjObjectFor<TToken<17CAnimCharacterSet>>` |
| `fn_8028EC7C` | 156 | `GetNewDerivedObject<45TObjOwnerDerivedFromIObj<17CAnimCharacterSet>>` |
| `fn_8028ED18` | 100 | `__dt__Q24rstl29auto_ptr<17CAnimCharacterSet>Fv` |

The fourteen unnamed ones are mwceppc output under the mangled names the compiler gives a class
destructor, a template instantiation or an inline member. objdiff pairs by name, so all fourteen
scored 0.00% while emitting exactly retail's bytes. **This is the same situation
`src/Kyoto/Animation/CAnimationSet.cpp` was written to solve, in the neighbouring file, and its
header comment is the reference**: no C++ declaration can rename a template instantiation, so the
only way to give objdiff a symbol to pair is to write the function out under an `extern "C"` name.
The Prime 1 donor named in the item's `reason`
(`prime-ref/src/Kyoto/Animation/CAnimCharacterSet.cpp`) is 11 lines and holds none of these
fourteen - it is a *smaller* class (0x78 vs our 0x7c, no `CAnimationSet` at all) and has no
`FAnimCharacterSet` factory. **The donor was not the way in for this item**; the retail object
was, and the fourteen bodies are all in it with their relocations resolved.

## What I did

Added ten `extern "C"` definitions to `src/Kyoto/Animation/CAnimCharacterSet.cpp`, in descending
retail offset, each documented with its own address, size and instruction trace read out of
`build/G2ME01/obj/Kyoto/Animation/CAnimCharacterSet.o`:

| function | before | after | what it is |
| --- | --- | --- | --- |
| `fn_8028ED18` | 0.00% | **100.00%** | `~auto_ptr<CAnimCharacterSet>` |
| `fn_8028EBB8` | 0.00% | **100.00%** | `CAnimationSet::~CAnimationSet()` |
| `fn_8028EB60` | 0.00% | **100.00%** | `~pair<int, CCharacterInfo>` |
| `fn_8028EB3C` | 0.00% | **100.00%** | `destroy_impl<pair<int, CCharacterInfo>*>` |
| `fn_8028EB1C` | 0.00% | **100.00%** | `destroy<pair<int, CCharacterInfo>*>` |
| `fn_8028EACC` | 0.00% | **100.00%** | `destroy_impl<pointer_iterator<pair<int, CCharacterInfo>>>` |
| `fn_8028EA94` | 0.00% | **100.00%** | `destroy<pointer_iterator<pair<int, CCharacterInfo>>>` |
| `fn_8028EA10` | 0.00% | **100.00%** | `~vector<pair<int, CCharacterInfo>>` |
| `fn_8028E9B8` | 0.00% | **100.00%** | `CCharacterSet::~CCharacterSet()` |
| `fn_8028E954` | 0.00% | **100.00%** | `CAnimCharacterSet::~CAnimCharacterSet()` |

A body calls its neighbour *by retail's name* (`fn_8028DA3C` and the three siblings, which
`CAnimationSet.cpp` already defines under those names) rather than inlining it, because a `bl` is
a `bl` in both objects and objdiff compares the instruction, not the symbol it relocates to.

### Two spellings that were measured, not guessed

1. **The `rc_ptr` member at `CAnimationSet+36`.** Written as `if (p) p->ReleaseData()`, the tail
   is `addic. r3,r30,36 / beq / bl` - two instructions short of retail's
   `addic. r0,r30,36 / beq / addi r3,r30,36 / bl`, and the function sat at **97.24%**. Spelled as
   the member's own destructor call (`p->~rc_ptr()`), mwceppc cannot see that the pointer is a
   member, so it recomputes the address into `r3` after the `addic.` has used it and the three
   instructions match. This is the file's **one** raw offset and it is now documented in
   `docs/research/raw_offsets.md` (166 sites in 70 files, measured).
2. **Accessor vs byte offset for the other seven members.** Reaching the rest through the class's
   own `const` accessors and `const_cast`ing them back costs no instruction and reproduces retail
   exactly, so only `+36` remains a raw offset. Both spellings were built and measured; the
   accessor form is in the file because it is the one that passes `tools/check_raw_offsets.py`
   without new debt.

## What is left, and why

Four functions remain at 0.00%: `fn_8028EC7C` (156 B), `fn_8028EC50` (44 B), `fn_8028E8C4`
(144 B), `fn_8028E820` (164 B). They are the `CFactoryFnReturn` / `TToken` /
`TObjOwnerDerivedFromIObj` chain - all templates, all reached only through
`CFactoryMgr.hpp`'s `CFactoryFnReturn(T* ptr) : obj(TToken<T>::GetIObjObjectFor(ptr).release())`.

I tried all four and **removed the attempts**, because none reached 100% and a half-correct
function in a diff is worse than a missing one. What was measured, so the next run does not repeat it:

| function | spelling tried | result |
| --- | --- | --- |
| `fn_8028E8C4` | `static_cast<TObjOwnerDerivedFromIObj<CAnimCharacterSet>*>(self)->~TObjOwnerDerivedFromIObj()` | **58.31%**. mwceppc folds the class's own `~IObj` chain and drops retail's three vtable stores (`stw vtable`, then `stw __vt__31CObjOwnerDerivedFromIObjUntyped`, then `stw __vt__4IObj`) because it treats the call as the base `~IObj`. The stores are the whole difference. |
| `fn_8028E8C4` | `->TObjOwner::~TObjOwner()` (fully qualified) | 58.31%, identical bytes. No help. |
| `fn_8028E820` | `*result = TToken<CAnimCharacterSet>::GetIObjObjectFor(obj).release()` | **20.10%**, then **62.15%** with `new (result) rstl::auto_ptr<IObj>(...)`. The frame is 48 bytes against retail's 32: our version keeps the `auto_ptr<IObj>` return slot in a callee-saved register and reloads `mItem` across the call, retail keeps it on the stack at `12(r1)`. |
| `fn_8028E820` | `new (out) CFactoryFnReturn(in)` | 29.98%. Placement new expands into call + null test + construct. |

**The blocker, stated once:** all four need a *vtable store* or a *stack-slot* the compiler will
not produce from a spelling I can find, and the vtable they need is retail's
`lbl_803B9240` (`config/G2ME01/symbols.txt:18459`, `type:object size:0x10`) - unnamed in retail's
own symbol table, so it is not reachable by any C++ declaration either. That is the same wall
`CAnimationSet.cpp` documents for its 12 unmatched functions.

## Verification

- `./tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS progress-unit-canimcharacterset`**, with
  `gate.sh` green (DOL sha1 + all 86 RELs + per-function report diff + wiring + docs claims + port probe),
  `counts: matched 11705 -> 11715`, `target rose: main/Kyoto/Animation/CAnimCharacterSet: 2 -> 12 / 16 functions`,
  `no asm added`, `check_symbol_names.py` clean.
- `python3 tools/check_decl_order.py --unit CAnimCharacterSet` -> `ok: 1 unit(s) checked, none emits its functions out of retail order`.
- `python3 tools/check_raw_offsets.py` -> `ok: 166 raw-offset site(s) in 70 file(s), all documented`.
- `tools/unit_fit.sh Kyoto/Animation/CAnimCharacterSet.cpp` reports 70 functions present in ours but
  not in the retail unit object, 6924 bytes. That is **pre-existing and not caused by this
  change**: the clean tree's object already carried 72 functions for retail's 16 (the whole
  `rstl` template and `CCharacterInfo` destructor tree the header pulls in), and the ten added
  functions are the ones retail *does* define. The unit still cannot flip for that reason plus
  the four above, which is why this is a `progress` item.

## NEW

None filed. The four remaining functions are a measured wall, not new work that can raise a
count: the spellings and their scores are the table above, and a `NEW:` line must name a target
whose success raises a count.
