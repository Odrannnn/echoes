# progress-unit-chuddecoInterfaceScan

## Result

`MetroidPrime/HUD/CHudDecoInterfaceScan` went **5 -> 11 of 20 functions matched**; the DOL's
`All:` matched count went **11848 -> 11854**, and `goal_check.sh build/goal/item.json` printed
`PASS`. Unit stays `NonMatching` (15 functions left, the large ones). No `asm`, no judge-owned
path touched, nothing committed.

Files touched (both in this worktree only):

- `src/MetroidPrime/HUD/CHudDecoInterfaceScan.cpp`
- `include/MetroidPrime/HUD/CHudDecoInterfaceScan.hpp`

## Per function: before -> after, and what it took

| function | before | after | what was needed |
|---|---|---|---|
| `CheckHierarchyLoadComplete` | 94.34% | **100%** | two independent fixes, below |
| `__dt__` (destructor) | 92.33% | **100%** | `mScanDisplay` type change, below |
| `Draw` | 4.55% | **100%** | shared `sDrawParms` static + `mScanDisplay->Draw` |
| `ProcessControllerInput` | 11.11% | **100%** | `mScanDisplay->ProcessInput(input)` |
| `PrepareScanDisplay` | 9.09% | **100%** | null-check then `mScanDisplay->PrepareScanDisplay` |
| `GetMessageTextAlpha` | 9.33% | **100%** | operand order of two `rstl::min_val`/`max_val`, below |
| `__ct__` | 27.08% | 27.08% | untouched (needs the frame-loader/tweak read) |
| the other nine | <=0.91% | unchanged | untouched |

### 1. `CheckHierarchyLoadComplete` (94.34% -> 100%)

The existing body was structurally right. Two things differed, and each was one edit:

**The early-return polarity.** Retail branches *over* the work when the request is null
(`lwz r3,172(r3); cmplwi r3,0; beq <return true>`), i.e. `if (!ptr) { ... }` with the body
guarded positively and a single `return true` at the end - not `if (ptr == nullptr) return true;`
at the top. `rstl::auto_ptr` has no `operator bool` under mwcceppc, so the guard has to be
written `if (!mHierarchyRequest.null()) { ... }` with the trailing `return true;` outside the
block; that is what produces `beq` rather than `bne`.

**`CInputStream` vs `CMemoryInStream` for the local.** Retail constructs
`__ct__15CMemoryInStreamFPCvUl` (0x802FFF04) and tears down with
`stw <__vt__15CMemoryInStream>,24(r1); bl __dt__12CInputStreamFv` (0x8021C790). Declaring the
local `CMemoryInStream in(...)` (and including `Kyoto/Streams/CMemoryInStream.hpp`) reproduces
both: the vtable store before the base destructor call, and `li r4,0` instead of the `li r4,-1`
we were emitting for a plain `CInputStream`. The whole 12-byte tail is this one declaration -
nothing about the body changed.

### 2. `GetMessageTextAlpha` (9.33% -> 100%)

Retail (0x8021C978, 15 instructions) is:

```
f1 = mScanningTextAlpha            (this+0x58)
f0 = 1.0f;  f1 = min(f1, f0)       fcmpo f1,f0 / bge / b / fmr f1,f0
f2 = mScanDisplay->mBodyAlpha      (this+0x1c, then +0x300)
                                  fcmpo f1,f2 / bge / b / fmr f2,f1
return 1.0f - f2
```

The word at `CScanDisplay+0x300` is `mBodyAlpha` (measured: `mModelTransition` 0x2f8,
`mXAlpha` 0x2fc, `mBodyAlpha` 0x300 - I first reached for `mXAlpha` via a new accessor and that
gave 764 instead of 768; the accessor was reverted, `GetBodyAlpha()` already exists).

Getting the *register order* right took a search. `max_val(a, b)` compiles to
`fcmpo cr0, a_reg, b_reg; bge` only for one argument order; the other emits
`fcmpo cr0, b_reg, a_reg`. The form that matches is

```cpp
const float a = rstl::min_val(1.f, mScanningTextAlpha);
float b = mScanDisplay->GetBodyAlpha();
b = rstl::max_val(a, b);
return 1.f - b;
```

Note `min_val(1.f, x)`, **not** `min_val(x, 1.f)` - they are the same value but not the same
code. Spellings measured, all on the real unit:

| spelling | score |
|---|---|
| `max_val(body, min_val(alpha, 1.f))` | 97.67% |
| `max_val(body, min_val(1.f, alpha))` | 97.67% |
| `max_val(min(alpha,1), body)` | 36.33% |
| `const t = min(alpha,1); max(t, body)` | 97.67% |
| `t < 1.f ? t : 1.f` | 97.00% |
| `1.f < t ? 1.f : t` | 92.00% |
| `t >= 1.f ? 1.f : t` | 98.33% |
| `min(body, min(alpha,1))` | 98.33% |
| `max(body, alpha>1.f?1.f:alpha)` | 98.33% |
| if-statements (`if (t >= 1.f) t = 1.f;` etc.) | 47.67% / 89.67% |
| `CMath::Clamp(0.f, alpha, 1.f)` | 72.00% |
| **`min_val(1.f, alpha)` then `max_val(a, b)` with `b` reused** | **100%** |

The two 99.33% rows below 100% differ from retail in exactly one register pair: retail loads
`this+0x58` into `f1` and the `1.0f` constant into `f0`; ours loads them the other way round. The
winning spelling is the one that makes the constant land in `f0`.

### 3. `__dt__` (92.33% -> 100%) - a header type change

Retail's destructor destroys `mScanDisplay` **in reverse-declaration position**, between the two
`rstl::auto_ptr` members (`+0xA8` hierarchy request, `+0x9C` hierarchy buffer) and `mFlatFrame`
(`+0x0C`), with a non-virtual `lwz r3,28(r30); li r4,1; bl __dt__12CScanDisplayFv`. A raw pointer
cannot produce that - our destructor was missing those 5 instructions entirely (264 B vs 244 B).

`rstl::single_ptr<CScanDisplay>` reproduces it exactly: same `addic. r0,r30,28` / `beq` null-`this`
guard as the `auto_ptr` members, same `li r4,1`, same direct (non-virtual) destructor call, and it
is still 4 bytes so `CHECK_SIZEOF(CHudDecoInterfaceScan, 0xd0)` and every other member offset are
untouched. The destructor body itself becomes empty again. `PrepareScanDisplay`'s
`if (mScanDisplay != nullptr)` becomes `if (!mScanDisplay.null())`; the other call sites
(`->`, `operator*`) are unchanged, which is why `Draw` / `ProcessControllerInput` /
`PrepareScanDisplay` all reached 100% on the same change.

`rstl::auto_ptr<CScanDisplay>` was tried first and is **wrong**: it emits the extra
`lbz`/null-pair before the delete that retail does not have.

### 4. `Draw` / `ProcessControllerInput` / `PrepareScanDisplay`

Straight forwarders. `ProcessControllerInput` has no null check at all in retail (0x8021CF98:
`lwz r3,28(r3); bl ProcessInput__12CScanDisplayFRC11CFinalInput`), and `Draw` guards
`mLoadedFlatFrame` (`this+0x14`) but not `mScanDisplay`. `Draw`'s frame call passes
`lbl_80411024`, a shared `.bss` `CGuiWidgetDrawParms`, not a stack temporary - the same object
`CSamusHud::Draw` uses, so this follows the pattern already documented in
`docs/goal-notes/progress-prime1-csavegamescreen.md`.

## What is still open (not attempted, or attempted and stopped)

- **`__ct__`, 436 B at 27.08%** - the largest remaining win and untouched. Retail reads a name
  table at `lbl_803ACDDC` = `{ "FRME_ScanHudFlat", "FRME_ScanHudFlat2", "FRME_ScanHudFlat4" }`
  and calls `fn_80036B6C__13CStateManagerCFv` for the player index, then
  `__ct__15CMemoryInStreamFPC9CGuFrameLoader`-shaped construction via `new`
  (`__nw__FUlPCcPCc`) with the file/tag strings, then `GetScanSidesPositionStart__9CTweakGuiCFv`
  for `x50`. It needs `gpResourceFactory` / `gpSimplePool` / `gpTweakGui` wiring and the layout-
  specific loader; a partial guess would move bytes without matching.
- **`InitializeFlatFrame` (1668 B, 0.24%), `UpdateScanDisplay` (1424 B, 0.28%),
  `Update` (528 B, 0.76%), `StartHierarchyLoad` (440 B, 0.91%),
  `UpdateHierarchyProgress` (504 B, 0.79%), `BuildScanHistory` (656 B, 0.61%),
  `ReadHierarchy` (256 B, 1.56%), `fn_8021C29C` (148 B, 0.00%)** - all still stubs. These are
  the whole scan-display and logbook behaviour; each needs the shared interfaces to exist first.
- **`ReadHierarchy`** has a concrete and cheap-looking next step worth recording:
  `fn_80116704` is retail's out-of-line `SScanHierarchyNode(CInputStream&)` constructor
  (0x80116704, 140 B, **owned by `MetroidPrime/Player/CScanDisplay.cpp`'s split**, currently an
  unnamed unmatched function in that unit). Our `CScanHistory.hpp` declares the same constructor
  inline in the struct, so `ReadHierarchy` cannot emit the `bl` retail has. Making it out-of-line
  in the header, or giving the struct an explicit symbol, would unblock both this call site and
  `fn_80116704` itself.
- `ClearHierarchy`, `GetCurrScanInfo`, `GetMessageTextAlpha` and the three `fn_8021db*` helpers
  were already matched and were not touched.

## Verification

```
./tools/decomp_build.sh                     # All: 33.58% fuzzy, 26.70% matched, 12.64% linked (11854 / 28465)
./tools/goal_check.sh build/goal/item.json   # PASS
```

`goal_check.sh` reported, in order: no judge-owned path touched; `gate.sh` ok (DOL sha1, 86 RELs,
report diff, wiring, docs claims, port probe); counts matched 11848 -> 11854 with linked unchanged
at 5727; no asm added; target rose 5 -> 11 / 20. `CScanDisplay` (37/54) and `CSamusHud` (29/88)
did not regress. No `NEW:` items filed: the `fn_80116704` finding is a header change to a
different unit, not a lane-sized item of its own, and it is recorded above instead.