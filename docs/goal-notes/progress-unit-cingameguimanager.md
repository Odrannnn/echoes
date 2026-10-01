# progress-unit-cingameguimanager

`kind: progress`, target `MetroidPrime/CInGameGuiManager`. Re-measured on the clean tree first:
`build/report.json` gave `main/MetroidPrime/CInGameGuiManager` **7 / 45** functions matched,
12.00% fuzzy, 6.41% matched code. The unit stays `NonMatching`; `flip_test.sh` was not run to
decide anything (per `item.json`).

Result: **7 -> 8 / 45**, the unit's `matched_code` 6.41% -> 7.31%, fuzzy 12.00% -> 12.02%.
Repo-wide `matched` 12104 -> 12105, `linked` unchanged at 5860, `All:` unchanged at 34.22%.
`./tools/goal_check.sh build/goal/item.json` printed **PASS**.

## What landed: `TryReloadAreaTextures__17CInGameGuiManagerFv` 98.29% -> 100% (140 B)

`src/MetroidPrime/CInGameGuiManager.cpp:195-211`. The function body was already retail's real loop
(reload-and-erase, else mark incomplete and advance); the whole difference was the **return
conversion**. Retail ends `clrlwi r3,r30,24` where we emitted `mr r3,r30`.

The fix is a `const bool` copy before the return:

```cpp
const bool result = reloadedAll;
return result;
```

This is the same lever already documented for `CMain::AsyncIdle` in
`src/MetroidPrime/main.cpp:1646-1674` and `CMainAsyncIdle.cpp:126-153`: mwcceppc emits the byte
mask for a **one-byte value it cannot see through**, and it is *bare* (no `neg/or/srwi`
normalisation) because the copy is already `bool`. Range-analysing past the local is what made it
emit the plain `mr` before. The stale comment on the function claimed the accumulator "is an int
rather than the `bool` this function returns" while the declaration was in fact `bool` - that
hypothesis was wrong and is now removed.

### Spellings measured (tools/try_batch.py, differing-instruction count; 0 == byte-exact)

| spelling | differing instrs |
| --- | --- |
| **`const bool result = reloadedAll; return result;`** | **0 - MATCH** |
| `const bool result = static_cast<bool>(reloadedAll); return result;` | 0 - MATCH |
| `const bool& result = reloadedAll; return result;` | 0 - MATCH |
| `bool reloadedAll = true; ... return reloadedAll;` (the old code) | 1 (`mr` vs `clrlwi`) |
| `int reloadedAll = 1; ... reloadedAll = 0; return reloadedAll;` | 5 |
| `int reloadedAll = 1; ... return reloadedAll != 0;` | 5 |
| `int reloadedAll = 1; ... return !!reloadedAll;` | 5 |
| `int reloadedAll = 1; ... return static_cast<bool>(reloadedAll);` | 5 |
| `uint reloadedAll = 1; ... return reloadedAll;` | 5 |
| `uchar reloadedAll = 1; ... return reloadedAll;` | 5 |
| `unsigned char reloadedAll = 1; ... return reloadedAll;` | 5 |
| `bool reloadedAll = static_cast<bool>(true); ... = static_cast<bool>(false);` | 1 |
| `bool reloadedAll = true; ... return reloadedAll ? true : false;` | 5 |

Every accumulator-type spelling adds mwcceppc's `int`-to-`bool` normalisation
(`neg r0,rX ; or ; srwi`) that retail does not have. **Do not retype the accumulator on a later
run** - it moves away from retail every time. The bare `const bool` copy is the only shape that
reproduces the instruction.

## What is blocked, and why - do not spend a run on these without the class first

Measured from `build/G2ME01/main.elf` with `tools/dis.sh`. The unit's base is `0x80222EC4`
(`StopSounds` is offset 0), so `report.json`'s `address` is a unit offset, not a vaddr.

**Three cheap functions (`GetIsGameDraw` 16 B, `IsTransitionReady` 72 B, `EnsureStates` 112 B) are
all blocked on one missing class: `CPauseScreenBlur`.** It is forward-declared only
(`include/MetroidPrime/CInGameGuiManager.hpp:25`) - there is **no `CPauseScreenBlur.hpp`, no
`.cpp`, no unit in `configure.py`, and no entry in `config/G2ME01/splits.txt`**. Prime 1's
counterpart is at `prime-ref/include/MetroidPrime/CPauseScreenBlur.hpp`, but its own destructor
(`fn_800E0FAC`, called from retail's `fn_80222F18`) sits at `0x800E0FAC`, which is in the
**unclaimed** `splits.txt` gap between `CSimpleShadow` (ends `0x800DFA60`) and `CWorldShadow`
(starts `0x800E17E4`). So writing the class is a carve-adjacent job, not a header edit, and this
item may not do it.

The retail offsets they would need, measured:

- `GetIsGameDraw` (`0x80225BE8`): `lwz r3,56(r3)` = `mPauseScreenBlur` at **0x38**;
  `lbz r0,60(r3)` then `rlwinm r3,r0,26,31,31` = **`bool : 1` bit 6 of the byte at 0x3C**.
  16 bytes, so it is the cheapest function in the unit once the class exists.
- `IsTransitionReady` (`0x802241A8`): blur at 0x38, compares `lwz 16(r5)` with `lwz 20(r5)`
  (`mPrevState == mNextState`, i.e. Prime's `IsNotTransitioning()`), then `mAutoMapper` at **0x30**
  and two words at **0x200/0x204** of it compared with `cntlzw`/`srwi` - an equality on a pair.
- `EnsureStates` (`0x80223984`): `mDeferTransition` is **`bool : 1` bit 5 of byte 0x14C** in this
  unit (Prime's logic verbatim: `if (mDeferTransition && !GetIsGameDraw()) { DestroyAreaTextures;
  mDeferTransition = false; DoStateTransition; }`). Our header declares it as the third bit of
  three (`mLoaded`, `mPlayerAlive`, `mDeferTransition`, lines 109-111), so the **bit position is
  wrong** and would need fixing with the class.

**`StopSounds` (84 B, 4.76%) is a symbol/API wall, not a spelling problem.** Retail's body is
`fn_80222F18(this+60, 0)` then, if `*(this+44)` is non-null,
`StopSounds__9CSamusHudFRC13CStateManager(hud, r4)`. Two things block it: `mQuitScreen` must be
an `rstl::auto_ptr`-like smart pointer whose null-assignment calls retail's out-of-line
`fn_80222F18`, not a raw `CInGameQuitScreen*`; and retail **reads `r4`** (it forwards it to
`CSamusHud::StopSounds`) although `symbols.txt` mangles the method `Fv` - the parameter is real but
unnameable, which is what the existing TODO at line 211-213 already says. `CSamusHud::StopSounds`
itself takes `const CStateManager&` (`include/MetroidPrime/HUD/CSamusHud.hpp:74`), so the argument
has to exist.

**`TryCompleteStateTransition` (164 B) needs a third missing class, `CMessageScreen`.** Measured:
`0x80224104` tests `mNextState` (0xBC) against 3 then 4, deletes `mPauseScreen` (0x44) via
`__dt__12CPauseScreenFv`, then a `0 <= state <= 1` range test, deletes `mMessageScreen` (0x40) via
`fn_80150910` (an **unnamed** symbol in `symbols.txt`, 0x80150910, 128 B), calls
`TryReloadAreaTextures`, `EnableTextureTimeout__6CModelFv`, and stores 0xBC to 0xB8. Seven
statement shapes were measured with `tools/try_batch.py` (order 0 vs the `else`-form, reload before
vs after the message-screen delete, message-screen guard present or absent, `mPauseScreen` delete
present or absent, timeout present or absent, `mPrevState` store present or absent); best was **9
differing instructions** with the timeout call removed. Removing real work scored better, so none
of the seven is kept - the honest version needs `CMessageScreen` to exist so the delete resolves to
`fn_80150910`, and until then every variant either drops a call or mis-orders one.

Also absent and needed by `PreDraw` (52 B, needs `fn_8010B768` = `CSamusFaceReflection::PreDraw`),
`DrawDarkVisorMask` (128 B) and `fn_80225a30` (216 B): `CSamusFaceReflection`, `CTurretHud`,
`CPlayerVisor` (only a `.cpp`, no header), `CInGameQuitScreen`.

`__ct__` (1244 B, 52.16%) is the largest single win left but it constructs six of those missing
classes, so it is downstream of all of them.

## NEW

None filed. Every blocker above is "a class this repo has never had", which is one carve-and-header
job rather than a per-unit target, and naming `MetroidPrime/CInGameGuiManager` again would only
restate this item.

## Verified

`./tools/goal_check.sh build/goal/item.json` -> **PASS**, with `gate.sh` green (DOL sha1, all 86
RELs, report diff, module wiring, docs claims, port probe), `matched 12104 -> 12105`,
`linked 5860 -> 5860`, `check_symbol_names.py` 0 missing, target unit `7 -> 8`, no asm added. The
only files changed are `src/MetroidPrime/CInGameGuiManager.cpp` (the `TryReloadAreaTextures`
return) and the state block in `docs/HANDOFF.md`, which the judge rewrote itself. No commit made.
