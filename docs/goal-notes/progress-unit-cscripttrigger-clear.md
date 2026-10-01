# progress-unit-cscripttrigger-clear

`kind: progress`, target `MetroidPrime/ScriptObjects/CScriptTrigger`. The unit stays `NonMatching`;
`flip_test.sh` is not the acceptance test here. The judge is `build/report.json`'s per-unit
`matched_functions`.

## Result

**21 -> 22 of 44 matched functions.** `./tools/goal_check.sh build/goal/item.json` prints
`PASS progress-unit-cscripttrigger-clear`; the whole-project matched count went 11953 -> 11954 with
`linked` unchanged at 5728 and the `All:` line moving 33.77% -> 33.78% fuzzy.
`sha1sum build/G2ME01/main.dol` is `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.

| function | before | after |
| --- | --- | --- |
| `ClearInhabitants__14CScriptTriggerFR13CStateManager` | 1.79% | **100%** |

Unit fuzzy 34.46% -> 36.84%, matched code 28.40% -> 30.82%.

**Every one of the 63 instructions now matches retail byte for byte.** The last 0.09% is the
`bl` target *name* alone: ours is `clear__Q24rstl67list<Q214CScriptTrigger14CObjectTracker,...>`,
retail's is `fn_80072278`, and our `clear` is byte-identical to retail's `fn_80072278` (16
instructions, same frame, same `mEnd`/`mStart` loads into `r1+8`/`r1+12`, same call).
objdiff's only remaining complaint on the function is `DIFF_ARG_MISMATCH` on that one `bl`; the
SDA21 sites match. This is the same COMDAT-naming dead end the earlier runs measured for
`do_erase`/`fn_8007334C`.

## The previous run's blocker was a misread of the register, and it was not a blocker

The previous run left `ClearInhabitants` blocked on "naming the undeclared `.sbss` global
`lbl_80419110` (0x80419110)", on the grounds that adding a `.sbss` global to `PortGlobals.cpp`
shifts every `.sbss` symbol after it. **Both halves of that are wrong.**

The instruction is `lwz r30,-27760(r2)` - base register **r2**, not r13. `_SDA_BASE_` is 0x8041FD80
and `_SDA2_BASE_` is 0x804223C0 (`tools/sda.py`), so:

| base | -27760 lands on | what it is |
| --- | --- | --- |
| `r13` (`_SDA_BASE_`) | 0x80419110 | `lbl_80419110`, `.sbss` - a CDecalManager static |
| `r2` (`_SDA2_BASE_`) | **0x8041B750** | `lbl_8041B750`, **`.sdata2`**, value -1 |

The retail object's own disassembly settles it without arithmetic: the load carries
`R_PPC_EMB_SDA21`, which is the r2 relocation.

```
00001164:	83 c0 00 00 	lwz     r30,0(0)
		1164: R_PPC_EMB_SDA21	lbl_8041B750
```

And 0x8041B750 is not a decal manager value at all. It is read eleven times in the whole image
(`objdump -d --section=.text build/G2ME01/main.elf | grep -- -27760(r2)`) and **written zero times**,
so it is a constant, and `CScriptTrigger::Touch` fixes its meaning: it seeds a player index with
the value at 0x80072d14, and at 0x80073008/0x80073068 passes either the index it found or a reload
of the constant instead, before calling `AddInhabitant(mgr, bool, int)`. It is the "no player"
player-index sentinel. Declared as `const int kInvalidPlayerIndex = -1;` in `PortGlobals.cpp` -
which is not a configure.py unit and so is not in the link, so no `.sbss` address moves.

Generalise: **`-NNNN(r2)` and `-NNNN(r13)` in a retail disassembly are different addresses.**
`objdump` prints the register from the instruction, so read it, and do not resolve an SDA
displacement by arithmetic when the object's relocation table is right there.

## Files

- `src/MetroidPrime/ScriptObjects/CScriptTrigger.cpp:9` - `#include "MetroidPrime/Player/CPlayer.hpp"`.
- `src/MetroidPrime/ScriptObjects/CScriptTrigger.cpp:89-107` - the body.
- `include/MetroidPrime/TGameTypes.hpp:18-24` - `extern const int kInvalidPlayerIndex;` beside the
  other three sentinels, with the evidence.
- `src/MetroidPrime/PortGlobals.cpp:146-163` - the definition, with the eleven read sites.
- `include/MetroidPrime/ScriptObjects/CScriptTrigger.hpp:30-34` - `CObjectTracker::GetObjectId()`
  now returns `const TUniqueId&`.

No `configure.py`, `splits.txt` or `files.cmake` change: nothing was carved and no symbol was
claimed, so the four-file carve rule does not apply. `python3 tools/check_decl_order.py --unit
MetroidPrime/ScriptObjects/CScriptTrigger` says `ok: 1 unit(s) checked, none emits its functions out
of retail order`. `python3 tools/check_symbol_names.py` says `checked 516 units; 0 declared names
are missing`. `tools/probe_sources.sh` says `746 files, 0 failed, 0 errors; link: LINKED (324
undefined, 0 duplicates)`.

`GetObjectId()` is used only inside `CScriptTrigger.cpp` (`HasInhabitant`, `ReplaceInhabitant`,
`RemoveInhabitant`, `RemoveInhabitantIfOutside`, `ClearInhabitants`), so returning by reference moves
nothing else. `docs/HANDOFF.md` was not edited by hand; the gate rewrote its two derived counts.

## The body, and what each spelling cost

`./tools/fast_try.sh MetroidPrime/ScriptObjects/CScriptTrigger` after rebuilding just that object,
one change at a time from the 1.79% stub:

| body | score |
| --- | --- |
| as landed below | **100%** |
| `int playerIndex`, `int i` | 96.05% |
| `uint playerIndex`, `uint i` | 93.52% |
| `uint playerIndex`, `int i`, `static_cast<uint>(GetNumPlayers())` | 89.18% |
| `uint playerIndex`, `int i`, plain bound | 97.12% |
| `uint playerIndex`, `uint i`, but `GetObjectId()` still by value | 98.20% |

The three things that had to be right, all of them type choices rather than logic:

- **`uint playerIndex`, not `int`.** The loop-bound compare is `cmplwi r0,0` and the
  found-index compare is `cmplw r30,r0`, both unsigned. `int` gives `cmpwi`/`cmpw`.
- **The inner loop must index `mgr.m_players[i]` directly, not through `mgr.GetPlayer(i)`, and `i`
  must be `uint`.** Retail strength-reduces the array into `r4 = mgr; lwz r3,5372(r4); addi r4,r4,4`
  (`mr r4,r28` once, then a 4-byte walk). `GetPlayer(int)` forces a conversion on a `uint i` and the
  compiler switches to `lwzx r4,r28,r4`, which costs 4-8 points; `static_cast<uint>` on the bound
  throws away the counter form entirely. `uint i` + `mgr.m_players[i]` reproduces the walk exactly.
- **`GetObjectId()` returns `const TUniqueId&`.** Retail's frame has one `TUniqueId` temp
  (`r1+8`) and passes `addi r4,r1,8`. With a by-value getter, mwcceppc builds the getter's return
  temp *and* the by-value `TUniqueId` argument temp of `CStateManager::ObjectById` - two stores and
  `addi r4,r1,12`. **This is the same rule as the `SetObjectId` fix that landed `ReplaceInhabitant`:**
  a class-type by-value parameter costs a caller-frame temporary, and if retail's frame has one
  fewer slot than yours that is the first thing to look at.

Note `ObjectById` (non-const) is the right call here - the retail relocation is
`R_PPC_REL24 ObjectById__13CStateManagerF9TUniqueId`, not `GetObjectById`, unlike
`RemoveInhabitant`/`RemoveInhabitantIfOutside`/`ReplaceInhabitant` which use the const overload.

## What retail's `ClearInhabitants` actually does

Read out of `build/G2ME01/obj/MetroidPrime/ScriptObjects/CScriptTrigger.o` at 0x113c, 0xE0 bytes,
63 instructions (retail address 0x80072198):

- `for (it = mInhabitants.begin(); it != mInhabitants.end(); ++it)` - the same walk and shape
  `ReplaceInhabitant` uses, already known-good spelling.
- Inside, an inner `for (i = 0; i < mgr.GetNumPlayers(); ++i)` over `mgr.m_players` with a countdown
  (`mtctr`/`bdnz`), comparing `lhz 8(player)` (a `CEntity::m_uid`) against `lhz 8(node)` (the
  tracker's `mId`) and remembering the matching index in `r30`. Seeded with the sentinel.
- `mgr.ObjectById(id)` + `TCastToPtr<CActor>`, then `if (actor)`, and inside that
  `if (playerIndex != sentinel) SetPlayerInside(mgr, false, playerIndex);` followed
  unconditionally by `NotifyInhabitantExited(*actor, mgr)`. The sentinel reload at 0x8007221c is
  because the two calls may have changed it.
- `addi r3,r27,344 ; bl clear` at the end. **`mInhabitants.clear()` was right and needed no
  change**: the previous run's open question about it inlining the walk instead of calling
  `rstl::list::erase(begin, end)` out of line is answered - mwcceppc emits `clear` as an out-of-line
  COMDAT that calls an out-of-line `erase`, and both are byte-identical to retail's `fn_80072278`
  and `fn_800722B8`. The COMDAT is weak (`W`) and pairs with retail's `fn_*` by content; only the
  name is different, which is the known objdiff limit.

The other four `kInvalidPlayerIndex` sites in this unit are in `AddInhabitant` (0x800726b8) and
`Touch` (0x80072d14, 0x80073008, 0x80073068), and five more are in one other unit
(fn_80118E28, fn_80118F38 x2, fn_801191F0, fn_801193A8), each comparing a player index against it.

## Codegen rules learned (for the next lane, not for the queue)

- **Read the base register before resolving an SDA displacement.** `-27760(r2)` is 0x8041B750;
  `-27760(r13)` is 0x80419110. The retail object's relocation table names the target outright.
- `CScriptTrigger::ClearInhabitants` and `CScriptTrigger::Touch` both pass a player index to
  `AddInhabitant(mgr, bool, int)` and use `kInvalidPlayerIndex` for "no player"; a lane taking
  `Touch` (1044 B, 0.38%) or `AddInhabitant` (944 B, 0.42%) already has the sentinel declared.
- mwcceppc strength-reduces `array[i]` into a pointer walk only when `i` and the subscript need no
  conversion. Wrapping the array in an accessor whose parameter type differs from the loop
  variable's type switches it to `lwzx`, and that alone is worth several percent.

## Still open in this unit (22 functions unmatched, measured)

`UpdateInhabitants` 1520 B at 0.26%, `Touch` 1044 B at 0.38%, `AddInhabitant` 944 B at 0.42%,
`UpdateCameraInhabitant` 664 B at 0.60%, `SetPlayerInside` 240 B at 1.67%, the constructor at
95.71% (measured wall, see `progress-unit-cscripttrigger-csctor`), and 13 unnamed `fn_*` at 0.00%.

`Touch` is the cheapest big one and now has both its sentinel and its shape partly known:
`TCastToPtr<CPlayer>` gates the player path, `MaskUIdNumPlayers` is called, `HasInhabitant` guards
the whole tail, and the two `AddInhabitant` call sites read as
`AddInhabitant(mgr, player != nullptr && flag, playerIndex or kInvalidPlayerIndex)`.

NEW: progress-unit-cscripttrigger-touch | progress | MetroidPrime/ScriptObjects/CScriptTrigger |
`Touch` (1044 B, 0.38%) is the cheapest remaining stub; its callee list, the `TCastToPtr<CPlayer>`
gate, the `kInvalidPlayerIndex` seed at 0x80072d14 and the two `AddInhabitant` call sites are
recorded above, and `HasInhabitant` is already matched so the guard's spelling is known.