# progress-cgamestate-elem12-36byte-elem-copy

**Result: GATE PASS.** `fn_80142760` (0x80142760, 124 B) bodiless -> **100.00%**,
`MetroidPrime/Player/CGameState` 94 -> **95** matched functions (of 116), `build/report.json`
`matched` 9937 -> **9938**, `linked` 4896 unchanged, **no function anywhere worse**, no `asm`.
Not committed (the driver commits).

```
MP_GATE_DOCS_WRITE=1 ./tools/gate.sh <baseline>
  configure ok / ninja + build.sha1 ok / hashes vs config.yml ok / report ok
  per-function diff  matched  9937 -> 9938  linked 4896 -> 4896  (+1 functions at 100%, 0 units newly linked)
  module wiring ok / dol_read ok / docs claims ok / gs offsets ok / raw offsets ok
  decl order ok / files.cmake ok / module order ok / port probe ok / port link gap ok
  GATE PASS  a78fdb8+3 changed

sha1sum build/G2ME01/main.dol                       6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
python3 tools/check_symbol_names.py                 503 units, 0 declared names are missing
python3 tools/check_decl_order.py --unit MetroidPrime/Player/CGameState.cpp   ok, 0 out of order
./tools/probe_sources.sh                            749 files, 0 failed; LINKED (250 undefined, 0 duplicates)
./tools/unit_fit.sh MetroidPrime/Player/CGameState.cpp   98 extra functions, same as the baseline
```

Files touched: `src/MetroidPrime/Player/CGameState.cpp` (one function defined, one stale
forward declaration removed), `include/MetroidPrime/Player/CWorldState.hpp` (the `extern "C"`
declaration + one `friend`). No doc was edited - `docs/HANDOFF.md`'s state block is the judge's to
re-derive, and the plain `gate.sh` run above is the only thing that fails on it.

## The element type was already in a header; the item's premise was half wrong

The item said "no header types the 36-byte element". It does: **`CWorldState`**
(`include/MetroidPrime/Player/CWorldState.hpp`, `CHECK_SIZEOF(CWorldState, 0x24)` = 36), which is
what `CGameState::mWorldStates` holds. Measured, not inferred:

- `fn_8014260C` = `StateForWorld__10CGameStateFUi` (`config/G2ME01/symbols.txt:5355`) walks the
  block at `mulli r0,r5,36`, and at 0x80142694 calls **`__ct__11CWorldStateFUi`** to build the new
  element on its own stack - so the element is a `CWorldState`, and `GetWorldAssetId__11CWorldStateCFv`
  is what the loop at 0x80142638 tests it with.
- `fn_8000447C` is `CWorldState::~CWorldState(int)` (`fn_80004458` and `fn_801435D4` forward to it
  with `li r4,-1`), which is the matching destructor for the copy constructor below.

What was missing was the definition under retail's C-linkage name, not the type.

## Why it is the copy constructor and not the assignment

Retail `fn_80142760` has **no `mPtr != other.mPtr` test and no `ReleaseData` call**. That is
`rc_ptr(const rc_ptr&)`'s `mPtr(other.mPtr), mRefCount(other.mRefCount) { ++*mRefCount; }`
(`include/rstl/rc_ptr.hpp`) and not `rc_ptr::operator=`. `StateForWorld` builds a `CWorldState` on
its stack, hands it to `fn_801426E0` and then destroys it with `fn_8000447C(local, -1)` - push_back's
copy, not a reuse of a live element. Each of the three `ncrc_ptr` members' refcount word is
reloaded out of the **destination** after its two words are stored (0x80142788 / 0x801427AC /
0x801427C8), which is the copy-constructor form again: `operator=` would test and release first.

Measured spellings, all with the type right:

| spelling | score |
|---|---|
| `new (elem) CWorldState(*from)` | 15.10% - an out-of-line `bl __ct__11CWorldStateFRC11CWorldState` in a 16-byte frame |
| `*to = *from` (`operator=`) | 12.48% - calls `__as__11CWorldStateFRC11CWorldState` |
| member-wise, declared `void` | **99.19%** - four instructions, the last refcount reload |
| member-wise, returns `CWorldState*` | **100.00%** |

The compiler already generates this exact body, as the weak
`__ct__11CWorldStateFRC11CWorldState` that `rstl::vector< CWorldState >::push_back` needs. **It is
byte-for-byte identical to retail's `fn_80142760`** - verified with objdump against
`./tools/dis.sh 0x80142760 0x7C`. So the whole item is the naming and the register allocation, not
the body: `rstl::construct` / `uninitialized_copy` reach the same constructor and come out as a
call, because the unit's own `push_back` already wants an out-of-line weak copy of it.

## Returning the destination is what buys the last four instructions

This is the one non-obvious thing here and it is worth keeping. The member-wise body is 30 of
retail's 31 instructions in **every** spelling tried; the only difference is the final refcount
reload, and only its registers:

```
retail / matched:  lwz r5,32(r3)   lwz r4,0(r5)   addi r0,r4,1   stw r0,0(r5)
void return:       lwz r4,32(r3)   lwz r3,0(r4)   addi r0,r3,1   stw r0,0(r4)   -> 99.19%
```

Written `void`, the allocator has `r3` free by the last statement and picks the two lowest.
Returning the destination keeps `r3` live to the end - which is what a copy constructor returning
`this` does - and the reload becomes `r5`/`r4`. **Returning it is the semantics, not a trick**:
this is a copy constructor and it returns `this`. `CHintOptions.hpp:15-20` records the sibling case
(`fn_801447C4` returning `void*` for the same reason).

The members are private, so `fn_80142760` is befriended, following `CHintOptions.hpp:36` exactly:
declared with C linkage **before** the class (befriending first would give it C++ linkage and
mwcceppc would emit `fn_80142760__F...`, leaving retail's 124 bytes unclaimed), and the three
`ncrc_ptr` members are copied through `rstl::CRcPtrData` - the same-layout two-word view the port
already uses for this (`src/MetroidPrime/CIOWinManagerRemoveIOWin.cpp:74-79`).

**Things that do NOT work, so the next lane does not re-derive them** (MWCC 2.7, `-O4,p
-inline deferred,noauto -inline_max_size(125)`):

- `CWorldState(const CWorldState&) = default;` in the header - **the compiler does not accept
  `= default`**, `';' expected`. Spelling it out as an explicit member-initialiser list is accepted
  but changes nothing: the `new (elem) CWorldState(*from)` call site still does not inline it.
- `#pragma inline_max_size(400)` at the call site **does** force the inline, and the expanded body
  is still the 99.19% `r4`/`r3` shape - the pragma does not fix the register choice, and a
  pragma in the source is not worth a build-flag dependency.
- Wrapping the copy in a member function `void CopyFrom(const CWorldState&)` (an ordinary
  non-static member, `r3` reserved by the ABI) or in a `static inline` helper, or spelling the
  increment six ways (`++*p`, `*p += 1`, `p[0] += 1`, `p[0]++`, through a `int*` local, through a
  `CRcPtrData&` reference): **all 99.19%**, every one of them differing in exactly those four
  instructions. Only the return type moves it.
- A **local view struct** with the same members (`CHECK_SIZEOF(..., 0x24)`) plus the return type is
  byte-exact too, so the friend is not what makes it match - but the friend is the right spelling,
  because it is the real type and not a duplicate of it.
- `mwcceppc` rejects `friend extern "C" void f(...)` inside a class (reads `extern` as a storage
  class) and GCC rejects a friend whose linkage does not match the definition. `CResLoader.hpp:48-49`
  and `CWorldTransManagerView.hpp:66-68` record both; the declaration goes before the class instead.

## Not attempted, and why

- **`fn_801466F4` (172 B, 66.63%) and `fn_8014680C` (104 B, 92.69%)**, which the item says this
  gates. They are **not** blocked by `fn_80142760` - both are written against the `fn_80142718` /
  `fn_80142738` forwarders, which were already there and still are, so the item's "it also gates"
  is wrong. Their walls are the ones the previous item measured and they are unchanged: retail
  keeps `end` by address in `r29` and re-loads it at the bottom of the loop (0x80146848), and
  `fn_801466F4` builds its four-word range from a second read of `x0c_data`. Both are register
  allocation, not a missing function, and neither is reachable by anything done here.
- The remaining bodiless functions in the unit (`fn_801465EC` 264 B, `fn_80146338` 440 B,
  `LoadGameFileState` 488 B, the `CGMFrontEnd` ctor/dtor pair, `fn_80144818`, `fn_80142944`,
  `fn_80143CD4`, `fn_801422D4`, `fn_80142288`) and the five near-100% named methods - unreached,
  and the previous item's notes already carry the measured walls for `fn_80142944` (91.92% of
  eleven spellings) and `fn_8014601C` (99.05% of eight).

No `NEW:` line: the item's own target is delivered, and the two functions it named as gated are
blocked by register allocation that this change does not touch and does not unlock, so filing them
would spend a lane on a wall this run just re-measured.
