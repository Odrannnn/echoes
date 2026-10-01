# progress-unit-ccharacterinfo — `Kyoto/Animation/CCharacterInfo`

`src/Kyoto/Animation/CCharacterInfo.cpp` only, no config, no carve, no asm.
`./tools/goal_check.sh build/goal/item.json` → **`goal_check: PASS`**,
`target rose: main/Kyoto/Animation/CCharacterInfo: 4 -> 14 / 42 functions`,
gate green, matched `11589 -> 11599`, linked `5625 -> 5625` (unchanged — the unit stays
`NonMatching`, which is what a `progress` item wants).

## What the unit actually is

`config/G2ME01/symbols.txt` names 6 of the 42 functions in this range. The other 36 are
`fn_<addr>`, i.e. **retail's map never named them**, and objdiff had **no partner** for any of
them: they scored 0.00% because our object emitted no symbol at all, not because the bytes
differ. `MetroidPrime/CAnimData.cpp` and `MetroidPrime/CTargetReticles.cpp` already solve this
for their ranges by spelling each body out under retail's own `fn_` name with `extern "C"`; the
same trick applies here and is what this change is.

Disassembling the range (`.text 0x802925C4`–`0x80293C98`) shows the 36 unnamed functions are
the `rstl` container helpers behind the class's members, not new game logic: `uninitialized_copy`
and `destroy` loops over `TEffectList`'s and `vector<pair<uint,CAABox>>`'s elements, `reserve`,
`operator=`, `push_back_unsafe`, and five bare forwarders. Prime 1's decomp
(`prime-ref/src/Kyoto/Animation/CCharacterInfo.cpp`) has only the two constructors and
`GetAnimationIndex` — Echoes added the four table-versioned members — so it was no help here;
`tools/who_calls.py` plus the disassembly identified each body.

## 14 functions at 100.00%, measured before → after

| function | addr / size | before | after |
|---|---|---|---|
| `fn_80293C30` `uninitialized_copy` over `TEffectList` | 0x80293C30 / 104 B | 0.00% | **100.00%** |
| `fn_80293BE4` `rstl::destroy` loop over `TEffectList` | 0x80293BE4 / 76 B | 0.00% | **100.00%** |
| `fn_80293BC4` forwarder to it | 0x80293BC4 / 32 B | 0.00% | **100.00%** |
| `fn_80293A44` `vector<pair<uint,CAABox>>::reserve` | 0x80293A44 / 212 B | 0.00% | **100.00%** |
| `fn_80293248` forwarder | 0x80293248 / 32 B | 0.00% | **100.00%** |
| `fn_80292D94` `TEffectList::push_back_unsafe` | 0x80292D94 / 56 B | 0.00% | **100.00%** |
| `fn_80292C94` `TEffectList` destroy-then-zero-count | 0x80292C94 / 96 B | 0.00% | **100.00%** |
| `fn_80292C30` `uninitialized_copy` over element pointers | 0x80292C30 / 100 B | 0.00% | **100.00%** |
| `fn_80292B00` forwarder | 0x80292B00 / 32 B | 0.00% | **100.00%** |
| `fn_80292A1C` `mCount = 0` | 0x80292A1C / 12 B | 0.00% | **100.00%** |

Unit fuzzy `19.68% -> 37.43%`, `matched_functions` `4 -> 14`.

## Spellings that had to be found (measured, so the next run does not repeat them)

- **Callee-saved register allocation decides the loop's shape.** `fn_80292C30` came out 98.20%
  with `for (; first != last; ++dst, ++first)` because `mr r30,r3` / `mr r29,r4` were the other
  way round from retail's. Introducing a named cursor for the *destination* (`TEffectEntry* out`
  before `const TEffectEntry* it`) and keeping `++it` before `++out` is what lands
  `mr r30,r3` / `mr r29,r4` / `addi r30,r30,0x20` in retail's order. `fn_80293BE4` needed the
  same trick for `r31`.
- **`rstl::uninitialized_copy` will not inline.** `fn_80293A44` measured **80.40%** written as
  `rstl::uninitialized_copy(vec->begin(), vec->end(), newData)` — mwccceppc emits the
  out-of-line instantiation because this unit has a second caller for it. Putting the loop in a
  file-local `static inline` helper that takes the two `rstl::pointer_iterator`s **by value**
  gets the loop inline *and* materialises the four words at 8/12/16/20(r1) that a by-value
  aggregate argument occupies, which retail's body has: **100.00%**. (`*out = *first`, not
  `rstl::construct` — the latter emits a call and scores 0 extra.)
- **`rstl::destroy` must be reached through `vec.begin(), vec.end()`**, not a raw pointer range:
  that is what makes mwccceppc emit the out-of-line `destroy<pointer_iterator<...>>` at
  0x802925C4 rather than inlining the loop.
- **`push_back_unsafe` has to be the post-increment subscript.** Retail increments the count
  *before* forming the address (`addi r5,r6,1` / `slwi r0,r6,5` / `stw r5,4(r3)` /
  `add r3,r7,r0`), so `rstl::construct(&vec->mItems[vec->mCount++], in)` is the spelling;
  `vec.push_back(in)` is not.

## Not finished, and what stops it

- **`fn_8029293C` (`vector<pair<uint,CAABox>>::operator=`, 0x8029293C / 224 B) — 93.93%.**
  Instruction-for-instruction except two things: the self-assignment guard is `cmplw` + `bne`
  into the body in retail (branch *into* the copy) against our `beq` past it, and the copy loop
  swaps `r4`/`r6`. 6 spellings tried, all measured: `if (dst != &src) { ...; return dst; }`
  (93.93%), the same with an early `return dst` in the empty branch (93.12%), the body in an
  `else` (93.93%), a counted `for (int i = src.mCount; i != 0; --i)` loop (121 instructions —
  mwccceppc fully unrolls it, 121 vs 57), and two orders of the `in`/`out` cursor declarations.
  The remaining diff is only register allocation and branch polarity, not scheduling.
- **`fn_80293268` / `fn_80292B00`'s target `fn_80292B20` (`pair<...,CAABox>` stream
  constructors, 0x80293268 and 0x80292B20) — 35.59% / 16.43%.** Retail copies the `CAABox` out
  of its temporary **six floats at a time** (`lfs`/`stfs`); ours copies it as three words. The
  fix is the commented-out `CAABox::operator=` in `include/Kyoto/Math/CAABox.hpp` (lines 41-49),
  which does `min = other.min; max = other.max;` member-wise — but `min`/`max` are **private**,
  so nothing outside `CAABox` can spell that assignment, and uncommenting the operator would
  change every other unit that copies a `CAABox`. `SetX`/`SetY`/`SetZ` through the accessors
  compiles to the same three-word copy. **This is a header-visibility problem, not a spelling
  problem**: it needs a `CAABox` accessor or the operator un-commented, which is a change to a
  shared header and therefore out of scope for a `progress` item on one unit. Every copy site in
  this unit that moves a `CAABox` by words (`fn_80293A44`, `fn_8029293C`, `fn_80292A28`) is
  *word-wise in retail too* and does match, so the fix is local to the two stream constructors.
- **`__ct__14CCharacterInfoFR12CInputStream` — 98.77%, unchanged by this item.** Every
  difference is a `bl` to a named `rstl` instantiation where retail calls an unnamed helper
  (`bl __ct__Q24rstl220vector<pair<int,pair<string,string>>>...` against `bl fn_80293324`, and
  eight more). Landing it means writing out the `mAnimInfo` / `mAabbs` / `mEffects` /
  `mAnimIdxs` stream-assignment helpers (`fn_80293324`, `fn_80293128`, `fn_80292CF4`,
  `fn_80292A28`, `fn_80293304`) the same way. That is the obvious next slice of this unit.
- **`__ct__Q214CCharacterInfo16CParticleResDataFR12CInputStreamUs` — 55.83%, unchanged.** Its
  helpers (`fn_80293638`, `fn_802936EC`, `fn_802937A4`, `fn_80293808`, `fn_802938E8`,
  `fn_802939A4`, `fn_80293B18`) are still unwritten; the ctor itself then follows.

WALL: fn_8029293C 93.93% - regalloc + branch polarity on the self-assign guard; 6 spellings tried, nothing reached 100%
WALL: fn_80293268 35.59% and fn_80292B20 16.43% - retail copies CAABox member-wise as floats, and `CAABox::min`/`max` are private in this tree
---

# Run 8 (2026-10-01) — the wall was in the map, not in the code

`./tools/goal_check.sh build/goal/item.json` → **`goal_check: PASS`**, every check `ok`.

| | before | after |
|---|---|---|
| **unit matched functions** | **14 / 42** | **31 / 42** |
| unit fuzzy | 37.38% | 72.43% |
| unit matched code | 16.56% | 52.91% |
| **All: matched functions** | **12129 / 28465** | **12146 / 28465** |
| All: fuzzy | 34.28% | 34.31% |
| All: linked | 5860 | 5860 (unchanged, as it must be: no unit flipped) |

`sha1sum build/G2ME01/main.dol` → `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`; `./tools/probe_sources.sh`
→ `747 files, 0 failed, 0 errors; link: LINKED (290 undefined, 0 duplicates)`;
`python3 tools/check_symbol_names.py` → `checked 525 units; 0 declared names are missing from their object`.

## What the previous run's two walls actually were

Both `WALL:` lines in the run above are **superseded**. Neither was a spelling problem:

- "`CAABox::min`/`max` are private, so nothing outside `CAABox` can spell that assignment" — **wrong**.
  `rstl::pair<string, CAABox>::pair(CInputStream&, const Alloc&)` does the six `lfs`/`stfs` on its
  own, because it *constructs* into a `CAABox` temporary rather than assigning member-wise, so the
  private members are never touched. Our object has produced retail's bytes for retail's
  `0x80293268` (35.59%) and `0x80292B20` (16.43%) **all along**; the previous run's hand-written
  `extern "C"` bodies were what scored badly, and objdiff never even looked at the good copy
  because our object emits the real one under a mangled name that retail's map does not use.
- "fn_8029293C regalloc + branch polarity, 6 spellings" — still true (92.41%), and still not a
  rename candidate: our `__as__vector<pair<uint, CAABox>>` is 152 bytes where retail's is 224.

## The real finding: 19 of the 36 `fn_`s are byte-identical to what we already compile

Measured, not reasoned. `objcopy -O binary --only-section=.text` on the retail-derived object and
on ours, then a byte compare of every retail function range against every symbol range in our
object. **Nineteen retail functions are byte-for-byte identical to a weak instantiation mwccceppc
emits in this unit** — they scored 0.00% purely because `config/G2ME01/symbols.txt` called them
`fn_<addr>` while our object emitted the mangled name, so objdiff had no partner. Nothing in `src/`
was wrong about them.

`config/G2ME01/symbols.txt` now names those nineteen retail addresses after the symbols our object
actually emits, applied with `tools/apply_rename.py` (`renamed 19/19`):

```
fn_80293B18 -> reserve__Q24rstl189vector<Q24rstl144pair<string, vector<CEffectComponent>>, rmemory_allocator>Fi
fn_802938E8 -> reserve__Q24rstl133vector<Q24rstl89pair<string, CAABox>, rmemory_allocator>Fi
fn_802937A4 -> uninitialized_copy<pointer_iterator<CEffectComponent, vector<CEffectComponent>>, CEffectComponent*>
fn_802936EC -> reserve__Q24rstl53vector<16CEffectComponent, rmemory_allocator>Fi
fn_80293268 -> __ct__Q24rstl89pair<string, 6CAABox>FR12CInputStream
fn_80293248 -> Get<Q24rstl89pair<string, 6CAABox>>__12CInputStreamFRC105TType<...>
fn_80293128 -> __ct__Q24rstl133vector<pair<string, CAABox>, rmemory_allocator>FR12CInputStreamRCQ24rstl17rmemory_allocator
fn_802930C8 -> clear__Q24rstl133vector<pair<string, CAABox>, rmemory_allocator>Fv
fn_80292F94 -> __as__Q24rstl133vector<pair<string, CAABox>, rmemory_allocator>FRC<same>
fn_80292F3C -> push_back_unsafe__Q24rstl53vector<16CEffectComponent, rmemory_allocator>FRC16CEffectComponent
fn_80292EA0 -> __ct__Q24rstl53vector<16CEffectComponent, rmemory_allocator>FR12CInputStreamRCQ24rstl17rmemory_allocator
fn_80292E7C -> Get<Q24rstl53vector<16CEffectComponent, rmemory_allocator>>__12CInputStreamFRC69TType<...>
fn_80292DEC -> __ct__Q24rstl144pair<string, vector<CEffectComponent>>FR12CInputStream
fn_80292DCC -> Get<Q24rstl144pair<string, vector<CEffectComponent>>>__12CInputStreamFRC161TType<...>
fn_80292CF4 -> __ct__Q24rstl189vector<pair<string, vector<CEffectComponent>>, rmemory_allocator>FR12CInputStreamRCQ24rstl17rmemory_allocator
fn_80292B98 -> __as__Q24rstl189vector<pair<string, vector<CEffectComponent>>, rmemory_allocator>FRC<same>
fn_80292B20 -> __ct__Q24rstl16pair<Ui, 6CAABox>FR12CInputStream
fn_80292B00 -> Get<Q24rstl16pair<Ui, 6CAABox>>__12CInputStreamFRC32TType<Q24rstl16pair<Ui, 6CAABox>>
fn_80293304 -> Get<12CPASDatabase>__12CInputStreamFRC21TType<12CPASDatabase>
```

### Why byte identity is enough, and how the four ambiguous 32-byte thunks were settled

Byte identity cannot be coincidence here: every `bl` to an *undefined* symbol carries the same
placeholder in both objects, so an exact match on 96-224 bytes is decisive. It is not decisive on
its own for the 32-byte thunks, because four of them are byte-identical **to each other**. Those
were settled by `objdump -r` on our object — each is a single `bl`, and the callee is the renamed
function:

| retail thunk | our callee (relocation) | retail's callee (disassembly) |
|---|---|---|
| 0x80292DCC | `__ct__Q24rstl144pair<string, vector<CEffectComponent>>FR12CInputStream` | `fn_80292DEC` |
| 0x80292E7C | `__ct__Q24rstl53vector<16CEffectComponent,...>FR12CInputStreamRCQ24rstl17rmemory_allocator` | `fn_80292EA0` |
| 0x80293248 | `__ct__Q24rstl89pair<string, 6CAABox>FR12CInputStream` | `fn_80293268` |
| 0x80292B00 | `__ct__Q24rstl16pair<Ui, 6CAABox>FR12CInputStream` | `fn_80292B20` |
| 0x80293304 | `__ct__12CPASDatabaseFR12CInputStream` | `0x80298E18` = `__ct__12CPASDatabaseFR12CInputStream` |

Two more needed a second measurement because their callees are themselves byte-identical:
`fn_802936EC` is `reserve<vector<CEffectComponent>>` and not `reserve<vector<pair<uint,CAABox>>>`
(the two are byte-identical) because our `__ct__vector<CEffectComponent>(CInputStream&, ...)` -
itself byte-identical to retail's `fn_80292EA0` - has its `reserve` `bl` at **+0x48** and its
`push_back_unsafe` `bl` at **+0x68**, the same offsets as retail's. Likewise `fn_80292CF4`'s
`reserve` `bl` is at +0x3c, `Get<pair<...>>` at +0x60, `push_back_unsafe` at +0x6c - retail's
offsets exactly.

## What changed in `src/` (required: a `progress` item must touch `src/` or `include/`)

The four renamed `fn_`s that the previous run had spelled out by hand as `extern "C"` bodies -
`fn_80293268`, `fn_80293248`, `fn_80292B20`, `fn_80292B00` - are **deleted**. Keeping them would
have left four symbols in our object that retail's map no longer has at those names
(`tools/unit_fit.sh` counts those), and two of them were the wrong code to begin with. The unused
`TStringAabbEntry` typedef went with them. `include/Kyoto/Animation/CCharacterInfo.hpp` gained the
19-row table with sizes and the proof above (comment only, so `goal_check`'s `CODE_CHANGED` is real
rather than padding).

A `symbols.txt` rename needs `python3 configure.py` to re-split the retail-derived objects before
objdiff will see it: `./build/tools/objdiff-cli report generate` alone keeps the old names. The
gate runs configure itself; for a fast loop use `./tools/decomp_build.sh -r`.

## What is left, and where the next run starts (all measured this run)

11 functions below 100%, 31/42 matched:

| retail | bytes | ours | gap |
|---|---|---|---|
| 0x802939A4 | 160 | - | no partner; `vector<pair<int,pair<string,string>>>`'s `operator=`-range helper |
| 0x80293808 | 224 | 172 | our `reserve<that vector>`; retail has an extra null-checked destroy loop |
| 0x80293638 | 180 | 96 | ours is `uninitialized_copy_n<CEffectComponent*,...>`; retail is a range-assign that takes *pointers to the two range endpoints* (`r4`/`r5` dereferenced once) and allocates via `allocate(int)`. Called 3x by the `CParticleResData` ctor, whose 55.83% is entirely this. |
| 0x80293324 | 204 | 248 | our `vector<pair<int,pair<string,string>>>(CInputStream&,...)`; retail's is 44 bytes shorter |
| 0x802933F0 | 100 | 156 | our `pair<int,pair<string,string>>::pair(CInputStream&)`; 56 bytes longer |
| 0x802931C4 | 132 | - | `vector<pair<string,CAABox>>::push_back_unsafe`; we emit none |
| 0x8029302C | 156 | 152 | `vector<pair<string,CAABox>>`'s copy range loop; retail's moves the `CAABox` as **six floats**, ours as three words. **Same `CAABox` header wall as before, but on the copy path, not the stream path** - and this one is real: un-commenting `CAABox::operator=` in `include/Kyoto/Math/CAABox.hpp` would change every unit that copies a `CAABox`. |
| 0x80292A28 | 216 | 148 | `vector<pair<uint,CAABox>>::vector(CInputStream&,...)`; retail inlines that vector's `push_back_unsafe` (7 word stores, no `bl`) and mwccceppc outlines ours. No lever found. |
| 0x8029293C | 224 | 92.41% | unchanged from the previous run; the 6 spellings it lists still stand |
| `__ct__14CCharacterInfoFR12CInputStream` | 672 | 98.77% | unchanged; 168 instructions, identical except the SDA `lbz` reloc and `addi r5,r1,24` being hoisted above `mr r4,r31`/`addi r3,r1,100` around the `Get<CPASDatabase>` call |
| `__ct__CParticleResData(CInputStream&, ushort)` | 484 | 55.83% | unchanged; every difference is a `bl fn_80293638` where ours `bl`s the shared `__as__vector<uint>` |

`tools/unit_fit.sh` reports 80 extra COMDAT weak copies in this object (template instantiations
both linkers drop), which is why the object is 13296 bytes over a 5844-byte range; that is
pre-existing and not what stops a flip.
