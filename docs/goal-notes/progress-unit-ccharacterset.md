# progress-unit-ccharacterset

`kind: progress`, target `Kyoto/Animation/CCharacterSet`. **The unit's matched_functions went 0 -> 9 of 12**
(`build/report.json`, re-measured after the change; `tools/goal_check.sh` prints
`target rose: main/Kyoto/Animation/CCharacterSet: 0 -> 9 / 12 functions`). Unit stays `NonMatching`.
Only `src/Kyoto/Animation/CCharacterSet.cpp` changed; no header, no config, no `tools/`.

## What the item's `reason` said, and what was true

`reason` gave 0/12 and listed `__ct__13CCharacterSetFR12CInputStream` at 72.9% plus ten `fn_80293*`
at 0.0%. Re-measured on the clean tree with `tools/fast_try.sh Kyoto/Animation/CCharacterSet`:
`0/12 functions, 14.90% fuzzy`. True.

**objdiff pairs functions by name** (`tools/report_diff.py`'s docstring says so, and it is
measurable: the target functions are named from `config/G2ME01/symbols.txt`, and an unmatched name
reports `fuzzy_match_percent: null`, not 0). Every one of those ten is a retail *unnamed* function,
so its target name is the `fn_<addr>` placeholder. Reaching 100% on one therefore requires our
object to contain a symbol with **that exact spelling** - the compiler-emitted
`push_back_unsafe__Q24rstl68vector<...>` / `construct<Q24rstl24pair<...>>__4rstl` names pair with
nothing even when the bytes are identical. Verified: before this change, `push_back_unsafe` at
`.text+0xC0` was byte-identical to retail's 0x80293D58 (dumped both objects' `.text`) and still
read `0.00%`.

That is why the eleven bodies are written out as `extern "C"` functions with the placeholder names.
It is the repo's own convention, not a new device: `src/MetroidPrime/CAnimData.cpp:976`
(`extern "C" void fn_80025E08() {}`) and its `fn_80026EE0`/`fn_80026F00`/`fn_80026F28`/`fn_80026F68`
`rstl::construct` chain (the comment block at `CAnimData.cpp:700` explains it at length). Each body
here is the template it stands for; nothing is stubbed, and nothing is a do-nothing forwarder -
every one either runs the loop/copy it is named for or forwards to the routine retail's own
forwarder forwards to.

## Per function (from `build/report.json` after the change)

| addr | size | before | after | what |
|---|---|---|---|---|
| `__ct__13CCharacterSetFR12CInputStream` 0x80293C98 | 192 B | 72.94% | **100%** | `push_back_unsafe(rstl::pair<int,CCharacterInfo>(in))` instead of `push_back(in.Get<pair<...>>())` |
| 0x80293D58 `push_back_unsafe` | 56 B | 0% | **100%** | `fn_80293D90(&self->data()[self->mCount++], in)` |
| 0x80293D90 `rstl::construct<T>` | 32 B | 0% | **100%** | forwarder to `fn_80293DB0` |
| 0x80293DB0 `rstl::construct_impl<T>` | 40 B | 0% | **100%** | `if (dest != nullptr) fn_80293DD8(dest, src);` |
| 0x80293DD8 `pair::pair(const pair&)` | 64 B | 0% | 60.56% | see below |
| 0x80293E18 `pair::pair(CInputStream&)` | 108 B | 0% | 32.07% | see below |
| 0x80293E84 `CCharacterInfo(CInputStream&)` | 32 B | 0% | 75.00% | see below |
| 0x80293EA4 `rstl::destroy<T>` | 32 B | 0% | **100%** | `rstl::destroy(in)` |
| 0x80293EC4 `vector::reserve` | 172 B | 0% | **100%** | `vector::reserve` body spelled in place |
| 0x80293F70 `rstl::destroy<T>(f,l)` | 32 B | 0% | **100%** | forwarder to `fn_80293F90` |
| 0x80293F90 `rstl::destroy_impl<T*>` | 76 B | 0% | **100%** | loop spelled out |
| 0x80293FDC `rstl::uninitialized_copy` | 104 B | 0% | **100%** | loop spelled out, iterators by value |

### The constructor

Prime 1's donor (`prime-ref/src/Kyoto/Animation/CCharacterSet.cpp`) is the same shape but writes
`mCharacters.push_back(in)`; Echoes' is a fork and retail here is neither. Retail's loop body is

```
mr r4, in ; addi r3, r1, 8 ; bl fn_80293E18      ; build the pair in a temporary
addi r3, this+4 ; addi r4, r1, 8 ; bl fn_80293D58 ; push_back_unsafe(&tmp)
addi r3, r1, 8 ; li r4, -1 ; bl fn_8028EB60        ; destroy the temporary
```

i.e. the entry is **constructed in a stack temporary** and its address is pushed, and the push is
`push_back_unsafe` (no `mCount >= mCapacity` branch - 56 B, not the 120 B `push_back`). With
`in.Get<rstl::pair<int, CCharacterInfo>>()` the return slot moved the temporary to `sp+0xc`, mwcc
emitted `push_back` instead, and the prologue needed a fifth callee-saved register (`stmw r27`,
`r27/r28` instead of `r28/r29`) - hence 72.94%. `push_back_unsafe(rstl::pair<int, CCharacterInfo>(in))`
reproduces all 192 bytes. Measured before: 72.94%; after: 100%.

### Two bodies have to be spelled as loops

`fn_80293F90` and `fn_80293FDC` are exactly the bodies mwcceppc already emits out-of-line in this
object as `destroy_impl__4rstl...` (0x1e40, 76 B) and `uninitialized_copy__4rstl...` (0x1e8c,
104 B) - both byte-identical to retail. But the deferred inliner leaves them **outlined at an
`extern "C"` call site**, so `rstl::destroy_impl(first, last)` and
`rstl::uninitialized_copy(begin, end, out)` each produced a 32-byte thunk (41.79% and 35.00%).
Spelled as loops they match, and the register allocation falls out right too: `destroy_impl` keeps
the induction variable in r31 and the limit in r30 (a loop written as `for (; first != last;
++first)` puts them the other way round), and `uninitialized_copy` reloads the limit from the
caller's iterator each iteration (`lwz r0, 0x0(r29)`) rather than holding it in a register - which
is why `fn_80293FDC` takes the two iterators by value, since mwcceppc passes those by address.
`fn_80293EC4` likewise had to be `vector::reserve` written in place: as
`extern "C" void fn_80293EC4(TCharacterList*, int) { self->reserve(size); }` it was a 32-byte
thunk (18.40%); written out it is 172 bytes and byte-exact, including the four `begin()`/`end()`
iterator temporaries on the stack.

## What is still short, and why

All three remaining bodies want a `bl` to a constructor **whose body the compiler does not have**.
mwcceppc 2.7 expands `new (dest) T(src)` into "call `operator new`, test the result against null,
then construct", and that test survives inlining - documented at
`include/Collision/CCollisionInfo.hpp:60`, with the measured consequence
(`Sphere_Sphere` 93.33% / `Sphere_AABox` 95.54%, each short by exactly the outlined call plus an
`addic.`/`beq`).

- `fn_80293E84` (75.00%, 40 B vs 32 B) and `fn_80293DD8` (60.56%, 15 instructions vs 16) each end
  in the extra `cmplwi r3,0`/`addic. r5,r3,4` + `beq`. Retail's are `bl __ct__14CCharacterInfoFR12CInputStream`
  and `bl __ct__14CCharacterInfoFRC14CCharacterInfo`. Fixing them means **declaring
  `CCharacterInfo`'s constructors out of line in `include/Kyoto/Animation/CCharacterInfo.hpp`** so
  mwccceppc has no body for them. That header is shared by every unit that copies a
  `CCharacterInfo`, and `MetroidPrime/CAnimData.cpp` already matches 101 functions through
  `__ct__14CCharacterInfoFRC14CCharacterInfo` - so this is a header item, not this unit's.
  Spelling tried first and rejected: `new (out + 4) CCharacterInfo(src.second)`, hoping the
  provably-non-null address would fold the test - it does not, mwccceppc keeps the test on the
  *result* of `operator new`, which is `out + 4` itself.
- `fn_80293E18` (32.07%, 40 B vs 108 B) additionally wants the `bool` member retail's `TType<T>`
  carries - `lbz r0, lbl_80419860@sda21(r0)` / `stb r0, 0x8(r1)` builds a `TType<CCharacterInfo>`
  temporary at `sp+8` before the `Get`. Ours is `template <typename T> struct TType {};`
  (`include/Kyoto/Streams/CInputStream.hpp:35`), and retail's `lbl_80419860` is a `.sdata2`
  constant, so the flag comes from a global, not a literal. Same header-cost objection as above:
  `TType` is on every `CInputStream::Get` path in the tree.

## Gates

`./tools/goal_check.sh build/goal/item.json` in the worktree: **PASS**, every check ok -
`gate.sh` (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe),
`counts: matched 11741 -> 11750   linked 5727 -> 5727`, `check_symbol_names.py`,
`All:  33.39% fuzzy, 26.39% matched, 12.64% linked (11750 / 28465 functions)`,
`target rose: main/Kyoto/Animation/CCharacterSet: 0 -> 9 / 12 functions`, `no asm added`.
`python3 tools/check_decl_order.py --unit Kyoto/Animation/CCharacterSet`:
`ok: 1 unit(s) checked, none emits its functions out of retail order` (the eleven are declared in
reverse retail offset, so mwccceppc's reverse-source-order emission reproduces `.text`).

The unit cannot flip: it emits ~90 weak `rstl`/`CCharacterInfo` copies that retail's object does not
define (`__dt__14CCharacterInfoFv` and the whole destructor chain, 0x180..0x1a54), so
`tools/unit_fit.sh` would fail, and 9 of 12 functions still differ.