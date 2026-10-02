# progress-prime1-cscriptplayerhint

Target `MetroidPrime/CGameHint`. The `reason` talks about CScriptPlayerHint fields (for
`CPlayer::SetAreaPlayerHint`); the target unit, though, is CGameHint, whose only unmatched function was
`AcceptScriptMsg__9CGameHintFR13CStateManagerRC10CScriptMsg` (77.55% at start, 2/3 functions).

**Result: `AcceptScriptMsg` 77.55% -> 100.00%; unit 3/3 functions, 100% fuzzy; `tools/goal_check.sh` PASS**
(matched 12571 -> 12572). Unit stays `NonMatching`.

## What changed (src/MetroidPrime/CGameHint.cpp only)
1. `if (mgr.IsMultiplayer()) { ... } else { CActor::AcceptScriptMsg(mgr, msg); }` instead of early return
   (retail puts the multiplayer body first and the plain call after: 89.0%).
2. In the camera case: `EScriptObjectState state = msg.GetState(); TUniqueId uid = camera->Player(mgr).GetUniqueId();`
   then `CScriptMsg(msg.GetUnk(), msg.GetId(), TUniqueId(uid), msg.GetMessage(), state)`.
   Retail reads state, calls Player(), then reads the message (stmw r28 not r27, `lwz r5,8(r31)` after the call).

Spelling scores (all with the CGameHint unit only):
- orig single expression 77.55; switch on `forwarded.GetMessage()` 77.55; local msg enum 77.55
- local `state` only 77.55; local `uid` only 88.42 (early-return form)
- if/else + plain `uid` local 99.90; `const TUniqueId& uid = ...GetUniqueIdRef()` 98.4;
  `const TUniqueId& uid = ...GetUniqueId()` 99.93; `CPlayer& player` local 94.94
- **if/else + `uid` local + `TUniqueId(uid)` copy at the call: 100.00** (the extra copy supplies retail's second
  stack copy of the uid)

## Flip
`tools/flip_test.sh MetroidPrime/CGameHint.cpp` FAILS (build/main.dol sha check). `unit_fit.sh` says sections are
92 bytes over the claimed range; `compare_unit.sh` shows the object also defines weak
`CModelDataNull__10CModelDataFv` and `__dt__16CActorParametersFv` (COMDAT copies from the ctor) and data
(`@313..@316`, `SolidMaterial`) the retail object lacks. Not investigated further.

## Declaration order
Source order must stay ctor, dtor, AcceptScriptMsg. I first reordered it "descending" per the prompt and
`gate.sh` failed on decl-order; here the .o comes out Accept, dtor, ctor from that source order, which is retail's
(Accept 0x8022D1EC, dtor 0x8022D34C, ctor after). Reverted.

NEW: none. (`CScriptPlayerHint` fields for `SetAreaPlayerHint` are still not modelled; that was this item's
`reason` but not its target unit.)
