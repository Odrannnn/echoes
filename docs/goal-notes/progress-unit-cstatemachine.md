# progress-unit-cstatemachine (kind: progress, target `main/MetroidPrime/Enemies/CStateMachine`)

Unit stays `NonMatching`. **Matched functions 2 -> 5 / 30**, measured in `build/report.json` against
`build/goal/judge/report.base.json`; the judge printed
`target rose: main/MetroidPrime/Enemies/CStateMachine: 2 -> 5 / 30 functions`.
Project matched 11664 -> 11667 / 28465; linked 5625 -> 5625 (unchanged - the unit does not flip).
`./tools/goal_check.sh build/goal/item.json` -> **PASS**, gate clean end to end (DOL sha1, 86 RELs,
per-function diff, wiring, docs claims, port probe 743 files 0 failed, link 324 undefined).

Three functions matched, all three from retail's own bytes rather than a guess about the source.

## Per function

| function | before | after | what it took |
|---|---|---|---|
| `__ct__6CStateFPCc` (140 B) | 99.83% | **100%** | `strlen(mName) > 1` -> `strlen(mName) >= 2`. Retail emits `cmplwi r3,2 / blt`, i.e. "length at least 2"; `> 1` makes mwcceppc emit `cmplwi r3,1 / ble`. Same predicate, different spelling, and it was the only differing instruction. |
| `Setup__8CTriggerFPCcbfP8CTrigger` (184 B) | 96.52% | **100%** | `"Default"` -> `lbl_803AA230 + 7`. |
| `Setup__8CTriggerFPCcbfP6CState` (184 B) | 96.52% | **100%** | same. |

## The string-pool trick, and why a literal costs a `.rodata` section

Both `Setup` overloads compare the copied name against retail's `"Default"`. Retail addresses it as

```
801957e4:  lis     r4,0x803B
801957ec:  addi    r4,r4,-24016      ; r4 = 0x803AA230 = lbl_803AA230
801957f4:  addi    r4,r4,7            ; +7
801957f8:  li      r5,7
801957fc:  bl      strncmp
```

- `config/G2ME01/symbols.txt:17242`: `lbl_803AA230 = .rodata:0x803AA230; size:0x10`.
- `tools/dol_read.py 0x803AA230 0x10`: `3f 3f 28 3f 3f 29 00 44 65 66 61 75 6c 74 00 00`, i.e.
  `"??(??)\0Default\0\0"` - retail's linker merged the pool entry with the tail of a longer literal,
  so `+7` is retail's own arithmetic and lands on the NUL-terminated `"Default"`.
- The `R_PPC_ADDR16_HA`/`LO` pair in `build/G2ME01/obj/.../CStateMachine.o` (offsets 0x135a/0x1362
  and 0x1412/0x141a) names `lbl_803AA230`, not a per-object `@stringBase0`. That is the whole
  difference from our 96.52%.

Writing `strncmp(mName, lbl_803AA230 + 7, 7)` reproduces the three instructions exactly. A `"Default"`
literal of our own goes through mwcceppc's per-translation-unit pool: measured, our object grew a
7-byte `.rodata` section and emitted `lis r4,0 / addi r4,r4,0` - two instructions, no `+7` - which
is both the wrong code and a section `config/G2ME01/splits.txt` does not claim for this unit. This
is the same trap that keeps `MetroidPrime/Factories/CStateMachineFactory.cpp` (which is 9/9 at 100%
and still will not flip) from using its own `CMEMORY_NEW_FILE` literal; that file's header comment
already documents it.

`lbl_803AA230` is **declared, never defined**, in `CStateMachine.cpp`. It is **defined** in
`src/MetroidPrime/PortGlobals.cpp` for the PC link only - that file is deliberately not a
`configure.py` unit, so a definition there cannot collide with a retail object at DOL link time.
Measured: without the PortGlobals definition the port's undefined count went 324 -> **325** and
`goal_check.sh` **FAILED** on `probe link-gap`; with it, 324 unchanged. The bytes are retail's own,
so the definition is the same text (`"??(??)\0Default"`, 16 bytes = `symbols.txt`'s `size:0x10`).

## `lbl_8041CB18`: the unit's unclaimed `.sdata2`

`tools/unit_fit.sh MetroidPrime/Enemies/CStateMachine.cpp` reported, before this change:

```
   .sdata2    claimed      -   ours      4   <- NOT CLAIMED BY splits.txt; the bytes live in a neighbour
```

That is `CTrigger`'s default constructor's `mArg(0.f)`. Retail instead reads its pooled zero:

```
80194670:  lfs     f31,-22696(r2)      ; r2 = 0x804223C0 -> 0x8041CB18
8019486c:  lfs     f0,-22696(r2)
```

`config/G2ME01/symbols.txt:23633`: `lbl_8041CB18 = .sdata2:0x8041CB18; size:0x8 data:float`, and
`tools/dol_read.py 0x8041CB10 0x10` gives `ff ff ff ff 00 00 00 00`, so the four bytes are
`00 00 00 00` = 0.0f. The relocations in our object (`R_PPC_EMB_SDA21 lbl_8041CB18` at 0x1f0 and
0x3ec) now match retail's, and **`unit_fit.sh` no longer reports a `.sdata2`** - the section is gone
from `readelf -SW` entirely. That does not move the constructor's percentage (see below) but it removes
a section the unit has no claim to, which is a real flip blocker removed.

Defined for the port in `PortGlobals.cpp` next to `lbl_8041C398`, which is in the same unclaimed
`.sdata2` gap (splits.txt claims 0x8041CB00-0x8041CB08 then 0x8041CB20-0x8041CB30).

## What did not work, measured

`__ct__13CStateMachineFR12CInputStream` (1472 B) stayed at **97.16%** and is the item's remaining
work. Three spellings, all measured:

| spelling tried | score |
|---|---|
| as committed (`const bool lnot`, `const char* triggerName`) | **97.16%** |
| `const int lnot` instead of `const bool` | 95.00% |
| `trigger->Setup(lnot ? name + 1 : name, ...)` inline, no `triggerName` local | 91.58% |

An instruction-by-instruction diff of the two objects (368 instructions each, same count, so nothing
is structurally missing) puts the remaining 202 differing instructions into three buckets:

- **76 register-allocation only** - identical mnemonic and operand shape, different register. Ours
  holds `this` in r30 and the stream in r31, retail in r27 and r28; our loop counters are r18/r19/r21
  where retail's are r16/r17/r30/r31.
- **116 branch/call targets**, of which the only non-relative ones are `bl fn_8019595C` /
  `bl fn_80195AA0` (retail calls its own `reserve`-shaped helpers out of line; ours inlines COMDAT
  weak copies of `reserve__Q24rstl42vector<CState,...>Fi` etc., which `unit_fit.sh` lists as
  "extra") and `bl memset` / `bl ReadFloat__12CInputStreamFv`, whose targets only look different
  because objdump prints a placeholder for the unresolved `R_PPC_REL24`.
- **10 immediate/offset**, and the two `lfs` that the `lbl_8041CB18` change addressed.

So what is left is register allocation and the placement of the out-of-line `reserve`/`memset`
calls - not a missing store or a wrong predicate. I stopped rather than churn further spellings:
this is exactly the wall the brief describes, and the item already passes on the three functions
above.

`fn_80195234` / `fn_80195228` (12 B each, `li r0,0 / stw r0,4(r3) / blr`) I did **not** touch. They
are called from `fn_801951C0` with `this+4` and `this+20`, so they are methods of a class this unit's
header does not declare, and the honest spelling would mean inventing that class. Declaring them
`extern "C"` with a made-up body would match the bytes and mean nothing.

WALL: __ct__13CStateMachineFR12CInputStream 97.16% - remaining diff is register allocation (76 insns) plus inlined-vs-out-of-line reserve/memset calls; three spellings measured, none better.

## The rest of the unit (unchanged, for the next run)

The other 25 functions belong to **classes this unit's header does not declare at all** -
`fn_80194A4C`, `fn_80194B04`, `fn_80194BF4`..`fn_80194E7C`, `fn_80194F10`, `fn_80194F24`,
`fn_80194F38`, `fn_8019507C`, `fn_801950E0`, `fn_801951C0`, `fn_80195240`, `fn_801956BC`,
`fn_80195914`, `fn_8019595C`, `fn_80195A14`, `fn_80195AA0`, `fn_80195B58`, `fn_80195BEC`,
`fn_80195CA0`. Measured evidence:

- `fn_80194A4C` reads `this+36`, `this+32`, `this+16` and dispatches through `__ptmf_test` /
  `__ptmf_scall` - a **pointer-to-member-function callback**, which is how Echoes replaced Prime 1's
  `CAiTrigger::CallFunc`. Prime 1's donor has no such type, so its `CStateMachineState` cannot be
  adapted directly.
- `fn_801956BC` (92 B) is a constructor: it stores two `.data` pointers (`lbl_803B5D64` at +0 then
  `lbl_803B5D28` at +0 again - a vtable pair, `symbols.txt:18255-18256`), zeroes +8..+40, three
  floats at +44/+48/+52 from `-22696(r2)`, a bitfield at +60. `fn_80195914` (72 B) is its destructor
  (`Free__7CMemoryFPCv` when a signed `extsh` argument is non-positive). That is a **polymorphic**
  class of size 0x40 with two vtables - nothing in `CStateMachine.hpp` is polymorphic.
- `fn_80195240` (1140 B) is the other half: it walks the stream and calls
  `fn_80195BEC`/`fn_80195CA0` with `machine->mStates` / `machine->mTriggers` (offsets 4 and 20 of the
  argument), which is `CStateMachineState::Setup(const CStateMachine*)` from Prime 1 with the
  out-of-line helpers Echoes emitted instead of inlining `reserve`.
- `fn_80194F10` (20 B) is `lwz r3,40(r3) / neg / or / srwi` = "is `mState` non-null", and
  `fn_80194F24` (20 B) is "return `mState`", both on the same object - Prime 1's
  `CStateMachineState::GetName` guard pair.

So the next slice needs a **new class declared in this unit's header**, sized from retail's own
offsets (0x40, `mState` at +40, `mTime` at +44, `mMachine` at +36), with a `CStateMachineState`-shaped
member-function-pointer callback type. That is a real design step, not a spelling, so it is filed as
a separate item rather than done here.

## Verifications

```
./tools/decomp_build.sh MetroidPrime/Enemies/CStateMachine
  All:  33.28% fuzzy, 26.21% matched, 12.24% linked (11667 / 28465 functions)
  main/MetroidPrime/Enemies/CStateMachine: 30.74% fuzzy, 8.20% matched (5 / 30 functions)
     __ct__13CStateMachineFR12CInputStream                 97.16%  1472 bytes   <- the only one left
./tools/unit_fit.sh MetroidPrime/Enemies/CStateMachine.cpp
  .text claimed 6344 ours 2912 retail 6344 SHORT by 3432; no .sdata2 / .rodata line (was 4-byte .sdata2)
python3 tools/check_symbol_names.py        -> 0 declared names missing
python3 tools/check_docs_claims.py         -> docs claims agree with the tree
./tools/link_check.sh                      -> 324 undefined, unchanged from baseline
./tools/probe_sources.sh                   -> 743 files, 0 failed, 0 errors
./tools/goal_check.sh build/goal/item.json -> goal_check: PASS progress-unit-cstatemachine
```

No `asm`, no `configure.py` / `config/` / `files.cmake` change, no carve, no new `.s`. Not committed.

## Files

- `src/MetroidPrime/Enemies/CStateMachine.cpp` - `extern "C" const char lbl_803AA230[];` plus the
  two `Setup` bodies and the `strlen >= 2`.
- `include/MetroidPrime/Enemies/CStateMachine.hpp` - `extern "C" const float lbl_8041CB18;` and
  `CTrigger()`'s `mArg(lbl_8041CB18)`.
- `src/MetroidPrime/PortGlobals.cpp` - the two PC-side definitions (`lbl_803AA230`, `lbl_8041CB18`),
  which is what keeps the port's undefined count at 324.
- `docs/HANDOFF.md` - the state block, rewritten by `gate.sh` (`MP_GATE_DOCS_WRITE=1`) from the tree.

NEW: progress-unit-cstatemachine-state-class | progress | MetroidPrime/Enemies/CStateMachine | 25 of the unit's functions are members of classes its header does not declare - a polymorphic 0x40-byte one (fn_801956BC/fn_80195914, vtable pair lbl_803B5D64/lbl_803B5D28, mState at +40, mTime at +44, mMachine at +36) and a ptmf-callback state machine (fn_80194A4C/fn_80194F38 dispatch through __ptmf_test/__ptmf_scall, which Prime 1's CAi donor has no type for); declaring them from retail's own offsets is the next slice.