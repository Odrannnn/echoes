# progress-prime1-cguiwidget — main/GuiSys/CGuiWidget, 16 -> 21 of 25 functions

`kind: progress`, target `GuiSys/CGuiWidget`. Only `src/GuiSys/CGuiWidget.cpp` changed (+15/-6).
The unit stays `NonMatching`; `flip_test.sh` was not run and is not the verdict here.

## Measured

| | before | after |
|---|---|---|
| unit `matched_functions` | 16 / 25 | **21 / 25** |
| unit `fuzzy_match_percent` | 91.75 | 94.14 |
| unit `matched_code` | 1032 / 2612 (39.51%) | 1620 / 2612 (62.02%) |
| `All:` | 9797 / 28465 matched (22.04%) | **9802 / 28465 matched (22.05%)** |

Five functions reached 100%: `SetColor`, `RecalcWidgetColor`, `SetVisibility`, `SetIsActive`,
`GetWorkerWidget`. `python3 tools/report_diff.py .tmp/opencode/base-report.json build/report.json`
printed `+100%` for exactly those five and `no regression`.

## Per function: Prime 1's source (`prime-ref/src/GuiSys/CGuiWidget.cpp`) vs what the tree had

| function | before | after | what it took |
|---|---|---|---|
| `SetColor` | 73.50% (44 B vs retail 56 B) | **100%** | **Prime 1 is wrong for Echoes.** Prime 1 assigns unconditionally; Echoes compares the packed `CColor` word first and skips both the store and the `RecalcWidgetColor` call (`beq .L_8027DAFC` over both, retail asm lines 280-291). Wrote `if (!(mColor == color)) { mColor = color; RecalcWidgetColor(kTM_Children); }` - this repo's `CColor` has `operator==` but no `operator!=`, so the negation is spelled out. |
| `RecalcWidgetColor` | 90.08% | **100%** | Prime 1's `if (parent) {...} else {...}` matched **unchanged**. The tree had a ternary (`parent != nullptr ? CColor::Modulate(...) : mColor`) that makes MWCC materialise the *address* of the selected source (`addi r3, r1, 0x8; lwz r0, 0x0(r3)`); retail loads the value directly (`lwz r0, 0x8(r1); stw r0, 0xac(r30)`). |
| `SetVisibility` | 98.78% (1 instruction) | **100%** | One instruction: retail `clrlwi r4, r31, 24` before the final `SetIsVisible`, ours `mr r4, r31`. Prime 1 spells the parameter `const bool visible`; the tree had dropped the `const`. Restored it. |
| `SetIsActive` | 98.06% (1 instruction) | **100%** | Same one instruction: retail's `rlwimi r5, r0, 6, 25, 25` stores the *normalised* param, ours stores the raw one (`rlwimi r6, r4, ...`). Prime 1 spells it `const bool active`; the tree had dropped the `const`. Restored it. |
| `GetWorkerWidget` | 65.83% | **100%** | Echoes-only (not in Prime 1). Retail is `while (w) { if (w->GetWorkerId() == id) break; w = next; }` - rotated to `b cond; body; cond: bne body`. The tree's `while (w != nullptr && w->GetWorkerId() != workerId)` tests in the other order. |
| `ReadWidgetHeader` | 57.13% | 57.13% (unchanged) | wall, below |
| `ParseBaseInfo` | 93.35% | 93.35% | wall (string pool), below |
| `Create`, `CreateGroup` | 99.98% | 99.98% | wall (string pool), below |

**Codegen rule worth keeping (it is not a NEW):** on this target, a `const` on a *by-value* `bool`
parameter makes MWCC reuse the normalised (`clrlwi ... 24`) copy instead of the raw argument
register. That single word took both `SetIsActive` and `SetVisibility` from one bad instruction to
100%. Top-level `const` is not mangled by mwcceppc, so this does not disturb the symbol (checked: the
`CGuiWidgetParms` ctor stays 100% with `const` on its six `bool` parameters). Use it wherever a
`bool` value parameter is one instruction away from matching.

## Walls, with the evidence

**1. `Create`, `CreateGroup`, `ParseBaseInfo`: the string-pool addend cannot be matched from source.**
Each is a *single* differing instruction, the `rs_new`/printf string relocation:

```
retail  lis r3, lbl_803AE7F8@ha ; addi r3, r3, lbl_803AE7F8@l ; addi r3, r3, 0x132   (printf, 306)
ours    lis r3, @stringBase0@ha ; addi r3, r3, @stringBase0@l ; addi r3, r3, 0x0     (printf, 0)
retail  addi r4, r4, 0x132   (rs_new "??(??)", 306)   vs   ours  addi r4, r4, 0x49 (73)
```

The *relative* offset is already right (306 - 233 = 73 = 0x49, the two literals' distance in our
`.rodata`), but objdiff compares the addends literally. The reason is in the retail **object**:

```
$ build/binutils/powerpc-eabi-objdump -h build/G2ME01/obj/GuiSys/CGuiWidget.o   # retail
  0 .text  0xa34   1 .data  0x40   2 .comment   3 .note.split        <- no .rodata, no ADDR16 relocs
$ build/binutils/powerpc-eabi-objdump -h build/G2ME01/obj/MetroidPrime/CCredits.o
  0 .text  0x4080  1 .ctors  2 .rodata 0x1c0  3 .data  ...          <- has .rodata + ADDR16 @stringBase0
```

`CGuiWidget`'s retail object has no string data at all and its relocations are already resolved, so
its addend is an absolute offset into `main.dol`'s `.rodata` (233 / 306). `CCredits` - a unit that
*is* `Matching` with 129 string relocations - kept its `.rodata`, so its addends pair up. Nothing in
`src/` can put 233 bytes of *other objects'* literals in front of ours, so these three are walled at
99.98/99.98/93.35. `ParseBaseInfo` additionally has a register-allocation diff in its prologue
(retail `r27=in, r28=version, r31=this`; ours `r30=in, r27=version, r29=this`) - 7 same-cost
spellings did not move it.

**2. `ReadWidgetHeader` 57.13%: register allocation plus one extra canonicalisation.** Retail
normalises each stream byte to a bool with `neg/or/srwi` *interleaved* with the next read and keeps
the result in a callee-saved register across the `CColor(in)` call (r24-r26 free, six values live).
Ours loads all three bytes first, then normalises, and inserts a redundant `clrlwi rX, rY, 24` (mask
to 0/1) *before* each `neg/or/srwi` - MWCC has pushed one extra `bool` conversion down into the
load. Structurally the two agree: 4 stream advances, 3 byte loads, same ctor arguments in the same
registers. The id locals are a real difference too: ours emits `extsh` for `short selfId/parentId`
into the ctor, retail emits plain `mr`. Scores measured (objdiff raw `match_percent`, all reverted):

| spelling | score |
|---|---|
| baseline (`const short` ids, non-`const` bool locals) | 57.13% |
| `in.Get< bool >()` for the three reads (Prime 1's spelling) | 57.13% |
| named unused `useAnimController` for the first read | 60.81% |
| `const bool` locals | 60.81% |
| `const bool` locals + `const` on the ctor's six `bool` params (header too) | 57.13% |
| named return temp `const CGuiWidgetParms parms(...); return parms;` | 62.69% (324 B, worse) |
| `const short&` ids (Prime 1) | 57.13% |
| `const int` ids | 62.45% |
| `short` ids (drop `const`) | **63.36%** (best) |
| `short` ids + `const bool` locals | 58.75% |
| `short` ids + `const bool` + `Get< bool >()` | 58.75% |

Every one of them is the same 4-byte-larger object with the same three `clrlwi`. I left the file at
the 57.13% baseline rather than ship a `const`-stripping cosmetic change that buys no matched
function.

## Also measured, not changed

`./tools/unit_fit.sh GuiSys/CGuiWidget.cpp` - the unit is 220 bytes over its claimed `.text` range and
emits 7 functions retail's object does not define: the `CGuiWidgetParms` copy ctor (92), the
`rstl::string` destructor (80) and five inline virtuals (`GetIsActive`, `GetIsVisible`,
`GetWidgetTypeID`, `GetWidgetUsageFlags`, `Initialize`, 48). It also emits an 80-byte `.rodata`
`splits.txt` does not claim. That is why this is a `progress` item and not a `match` one: the flip
needs those 220 bytes accounted for, and no source edit of mine changed that number.

## Gates (all re-run on the final tree)

```
./tools/goal_check.sh build/goal/item.json
  ok  no judge-owned path touched
  ok  gate.sh (DOL sha1, 86 RELs, report diff, module wiring, docs claims, port probe)
  ok  counts: matched 9797 -> 9802   linked 4895 -> 4895
  ok  check_symbol_names.py
  ok  All:  30.19% fuzzy, 22.05% matched, 11.74% linked (9802 / 28465 functions)
  ok  target rose: main/GuiSys/CGuiWidget: 16 -> 21 / 25 functions
  ok  no asm added
  goal_check: PASS progress-prime1-cguiwidget
python3 tools/check_decl_order.py --unit main/GuiSys/CGuiWidget   # none emits out of retail order
python3 tools/report_diff.py <base> build/report.json              # no regression
```

(`gate.sh` rewrites the `docs/HANDOFF.md` state block as a side effect; I reverted that file - the
driver re-derives it.)

No `NEW:` line: both remaining walls are measured spelling lists, and the only thing that would
raise this unit's count further is work the seeder already re-queues from the four functions still
below 100%.
