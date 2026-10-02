# progress-unit-geckoexceptionppc

`kind: progress`, target `Runtime/Gecko_ExceptionPPC`. Re-measured on the clean tree first:
`build/report.json` had **2 / 13** matched functions, `fuzzy_match_percent` 2.01.

## Result: 2 -> 5 / 13 matched. `goal_check.sh` PASS.

`matched_functions` for the unit rose 2 -> 5, no function anywhere got worse
(`tools/report_diff.py build/goal/judge/report.base.json build/report.json` -> "+3 functions at
100%, no regression"), DOL sha1 unchanged at `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.

## What I added

One file changed: `src/Runtime/Gecko_ExceptionPPC.cp` (+40 lines, nothing removed). Three
functions went 0.00% -> 100.00%:

| function | retail size | before | after | what it is |
| --- | --- | --- | --- | --- |
| `what__Q23std13bad_exceptionCFv` | 12 B | 0.00% | **100%** | `std::bad_exception::what() const`, returns `"bad_exception"` |
| `__dt__Q23std13bad_exceptionFv` | 92 B | 0.00% | **100%** | `std::bad_exception::~bad_exception()` |
| `__end__catch` | 68 B | 0.00% | **100%** | the EABI end-of-catch trampoline |

1. **`bad_exception`** (12 B, cheapest on the list). Retail's unit defines the whole class, but the
   source had no `std` declarations at all, so mwcceppc emitted neither function. Declaring
   `std::exception` and `std::bad_exception` in this file and defining
   `const char* bad_exception::what() const throw() { return "bad_exception"; }` reproduces retail's
   three instructions (`lis r3, str@ha / addi r3, str@l / blr`). The string lands in `.rodata` via
   the unit's existing `-str reuse,pool,readonly`, matching retail's pooled `lbl_803B0428`.
   Note `std::exception::what()` is deliberately *not* defined here - retail's `__vt__Q23std9exception`
   is an undefined reference in this object, so the base class stays declared-only.
2. **`__dt__Q23std13bad_exceptionFv`** (92 B). Defining `bad_exception::~bad_exception() throw() {}`
   made mwcceppc emit the vtable store, the base-class destructor call, the `r4` deleting flag test
   (`extsh. r0, r4`) and `__dl__FPv` exactly as retail does. **This needed
   `virtual ~exception() throw() {}` to be defined inline** rather than declared: with
   `virtual ~exception() throw();` the compiler emitted a 0x84-byte frame with a real `bl` to the
   base destructor and the function sat at 8.09%. Declaring the base destructor inline (empty body)
   is what lets the derived destructor inline the base call and reach retail's 92 bytes.
   The `-RTTI on -Cpp_exceptions on` extra_cflags used by `Runtime/NMWException.cp` were **not**
   needed here and made no difference (measured: 4 / 13 with and without); the file already carries
   `#pragma exceptions on`, so `configure.py` is untouched.
3. **`__end__catch`** (68 B). Retail offset 0x248. This one is emitted by mwcceppc itself, never by
   user source - I confirmed this with standalone compiles: a `throw` in a function produces only
   `U __throw`, and a `try`/`catch` produces `U __end__catch` as an undefined reference. So retail's
   definition is a *library* function that the MW runtime library contributes, and the only way to
   get it into this unit's object is to write it out by hand. It is plain code, not asm:
   `MWExceptionInfo { char* exception_record; char* current_exception; void (*cleanup)(void*, int); }`
   and, if `exception_record` and `cleanup` are both non-null, `cleanup(exception_record, -1)`.
   Compiled with `-Cpp_exceptions off` (the unit's own flags) that reproduces retail's 68 bytes
   exactly, including the `mtctr`/`bctrl` indirect call and the `-1` argument.

## Declaration order

mwcceppc emits definitions in reverse source order, so the new definitions are placed to descend by
retail offset: `what()` (0x142C) first, then the two fragment functions, then
`~bad_exception` (0x850), then `__end__catch` (0x248). Verified:
`python3 tools/check_decl_order.py --unit main/Runtime/Gecko_ExceptionPPC` -> "none emits its
functions out of retail order". The unit stays `NonMatching` and this is a `progress` item, so no
`flip_test.sh` was run.

`./tools/unit_fit.sh Runtime/Gecko_ExceptionPPC.cp` -> "no extra functions: our object defines only
what the retail unit object does" (`.text` 276 of 5176 bytes, `.rodata` 14 of 152, `.data` 16 of 232).

## What is still open

Eight functions remain at 0.00%, all the PowerPC EABI unwinder plus the two runtime entry points
that the compiler emits rather than user source:

- `ExPPC_LongJump` (260 B), `__throw` (324 B), `ExPPC_ThrowHandler` (1040 B), `__unexpected` (436 B),
  `ExPPC_UnwindStack` (1292 B), `ExPPC_PopStackFrame` (584 B), `ExPPC_NextAction` (448 B),
  `ExPPC_FindExceptionRecord` (516 B) - 4900 bytes of `.text`, all `local` in retail.

`__throw` and `__unexpected` call `ExPPC_ThrowHandler` and `unexpected__3stdFv`, and their bodies
walk the exception table, so they are one bounded but substantial slice: writing `__throw` by hand
means reproducing a 0x2C0-byte frame that saves f14-f31 and `mfcr`, and the `ExPPC_*` family is a
switch-per-action jumptable decoder over the exception-info records. That is a follow-on item on the
same unit, not a wall - the spellings are not exhausted. No `NEW:` filed: the same unit target is
already in the queue.

## Gates

`./tools/goal_check.sh build/goal/item.json` in the worktree, exit 0:

```
ok  no judge-owned path touched
ok  gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
ok  counts: matched 11851 -> 11854   linked 5727 -> 5727
ok  check_symbol_names.py
ok  target rose: main/Runtime/Gecko_ExceptionPPC: 2 -> 5 / 13 functions
ok  no asm added
goal_check: PASS progress-unit-geckoexceptionppc
```

`docs/HANDOFF.md`'s state block was rewritten by `gate.sh` itself (`MP_GATE_DOCS_WRITE=1`) - that is
the judge's own derived-count edit, not mine.
---

# Run 2 (lane 2, 2026-10-02)

Re-measured on the clean tree first: `build/report.json` had **5 / 13** matched functions for
`main/Runtime/Gecko_ExceptionPPC`, `fuzzy_match_percent` 5.332303, and all eight remaining
functions at 0.00%. The previous run's work was already in this tree.

## Result: 5 -> 6 / 13 matched. `goal_check.sh` PASS.

```
ok  no judge-owned path touched
ok  gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
ok  counts: matched 12348 -> 12349   linked 5863 -> 5863
ok  check_symbol_names.py
ok  All:  34.87% fuzzy, 28.51% matched, 12.90% linked (12349 / 28465 functions)
ok  target rose: main/Runtime/Gecko_ExceptionPPC: 5 -> 6 / 13 functions
ok  no asm added
goal_check: PASS progress-unit-geckoexceptionppc
```

Unit fuzzy 5.33% -> 13.99%. One file changed: `src/Runtime/Gecko_ExceptionPPC.cp`
(+131 lines, nothing removed). `docs/HANDOFF.md`'s state block was rewritten by `gate.sh`
itself (`MP_GATE_DOCS_WRITE=1`) - that is the judge's own derived-count edit, not mine.

| function | retail size | before | after |
| --- | --- | --- | --- |
| `ExPPC_NextAction__FP14ActionIterator` | 448 B | 0.00% | **100.00%** |

`python3 tools/check_decl_order.py --unit main/Runtime/Gecko_ExceptionPPC` -> "none emits its
functions out of retail order". `./tools/unit_fit.sh Runtime/Gecko_ExceptionPPC.cp` -> "no
extra functions: our object defines only what the retail unit object does" (`.text` 724 of
5176, `.rodata` 14 of 152, `.data` 84 of 232). No new undefined symbols: the only one the new
code adds is `ExPPC_FindExceptionRecord__FPcP15MWExceptionInfo`, which retail already
references - **so it has to be declared as a C++ function taking the class `MWExceptionInfo`,
not `extern "C"`**, or the symbol name will not match and the link will break.

## What `ExPPC_NextAction` turned out to be

Retail offset 0x1000. It is *not* assembly: a normal 16-byte frame, a normal
`lwz/mtlr/addi/blr` epilogue, one loop and a 17-case switch. The reconstruction is in the diff
(`class ActionIterator`, `class ActionTable`, `class ActionRecord`, the loop and the switch).
The layout, all measured from the loads:

```
ActionIterator (retail offsets)   ActionTable     unsigned short flags        (lhz at 0x00)
  0x00 table      (ActionTable*)                    the "five-bit field" is bits 3..7
  0x04 fragment_id (unused by retail's code)      of it; see the note below
  0x08 cur        (unsigned char*)
  0x0C/0x10/0x14  (unused by retail's code)
  0x18 node       (unsigned**)  read as *(unsigned**)it->node, written back as node
  0x1C action     (unsigned)    <- (flags>>4)&1 ? code : node
  0x20 code       (unsigned)    <- *(node - ((flags>>3)&0xF8) - 1)
```

Three things in retail's code are *provably dead* on a 16-bit index and are reproduced as
written, not "fixed": `cur[0] & 0x80` and `(index >> 4) & 1` and `(flags >> 4) & 1` all test
register bits a `lhz`-loaded value cannot set. That is a Metrowerks codegen quirk, not a typo
to correct - see the idiom table below.

## mwcceppc 2.7 codegen rules this unit needs (measured, not recalled)

Every one of these was established by compiling probe files with the unit's exact cflags
(`tools/fast_try.sh` loop + `build/binutils/powerpc-eabi-objdump`). They are the expensive part.

1. **`x & (1<<k)` on any type compiles to a mask of register bit `31-k`, not `k`.** So
   `byte & 0x80` -> `rlwinm. rd,rs,0,24,24`; `byte & 0x02` -> `rlwinm rd,rs,0,30,30`;
   `u16 & 8` -> `rlwinm. rd,rs,0,28,28`; `int & 0x02000000` -> `rlwinm. rd,rs,0,6,6`.
   Because the test is on a register bit, testing bit 7 of a byte is *always false*: retail's
   `ExPPC_NextAction` really does test `cur[0] & 0x80` and really does fall through.
2. **`x & 0x7F` -> `clrlwi rd,rs,25`**, and `(unsigned)x & 0x7F == 1` -> `clrlwi rd,rs,25` +
   **`cmplwi`** (declare the compared value `unsigned` or MWCC emits `cmpwi` and you are 4
   bytes off).
3. **Shift-then-mask gets a rotate, mask-only does not.** `(x >> 3) & 0x1F` -> `rlwinm. rd,rs,29,31,31`;
   `(x >> 4) & 1` -> `rlwinm. rd,rs,28,31,31`; and the one that cost this run its last
   instruction: `(x >> 3) & 0xF8` -> `rlwinm. rd,rs,29,24,28`, which is retail's exact word.
   `x & 0x1F` masks the *same* register bits (27..31) but is emitted as `clrlwi rd,rs,27`.
   Roughly 70 spellings were tried for that one instruction (`& C` over C = 0x01..0xFFFF,
   `(x & C) >> S` over a grid of both, `* 1..16`, `<< S`, `sizeof`, casts to `char`/`short`/
   `long`, `unsigned*` vs `char*` pointer arithmetic, `node[-(x&C)-1]`, `% 32`, `/ 1`); only
   the shift-then-mask family ever produced MWCC's rotate form, and only `>> 3` with a mask
   wide enough lands on `sh=29, mb=24, me=28`.
4. **`u16`-typed shifts are *not* folded to a compare, but `u16 < 2048` is**: `h < 2048` ->
   `cmplwi r,2048`; you need `(h >> 11) == 0` to get retail's `srawi. r0,rX,11`. And it must be
   an `int`: with `unsigned index` MWCC emits `srwi.`, 4 bytes off.
5. **MWCC rotates a `while (cond) body` loop** (the condition moves to the bottom). Retail's
   function has the condition at the *top* with the back edge at the bottom, which
   `for (;;) { if (cond) break; body; ... }` reproduces and `while (cond) { body }` does not.
6. **A `goto` out of a loop is what stops the switch from being re-materialised.** Retail's
   switch is reachable *only* from the loop condition, and the body exits to the tail, so
   `cur`/`op` stay live in r4/r5 from the condition into the switch dispatch. With the switch
   after the loop, MWCC reloads `it->cur[0]` (3 extra instructions); with the switch *inside*
   the loop it reorders the blocks and puts the switch first (83 wrong instructions). Only
   `switch` after the loop + `goto found` past it gives retail's block order and live ranges.
7. **The register allocator is sensitive to the order of the source statements even when the
   emitted instruction order is unchanged.** Retail puts `table`/`index` in r3 and the node
   pointer in r4. Writing `unsigned* node = ...` *before* `int index = ...` gets that; writing
   `index` first gets r4/r3 swapped (5 instructions wrong, everything else identical). This was
   the last structural blocker.
8. **MWCC merges identical switch case bodies only when the cases are adjacent in the source,
   and emits them in source order.** Retail's text order is 2,3,4,5,(6,7),8,9,10,11,12,16,13,15
   with the `std::terminate` body *last*: cases 6/7 share one body, 11 and 12 do **not** (two
   identical `it->cur += 12` bodies at 0x112c and 0x1138), and `case 16` sits between 11/12 and
   13. `default:` shares the terminate body with cases 0, 1 and 14, which is also where
   `cmplwi rX,16; bgt` lands.
9. **`&`-with-constant in pointer arithmetic gets the scale folded into the mask**
   (`node - (x & 0x1F) - 1` -> `rlwinm. rd,rs,2,25,29`, a 0x1F0 mask), so a byte-pointer
   spelling is needed to keep the mask unscaled.

## What is still open - and the blocker worth knowing before starting

Seven functions, 4892 bytes, all at 0.00%. They split into two groups, and the split is the
useful finding:

**Not C, and not worth retrying as C** - `ExPPC_LongJump` (260 B), `__throw` (324 B),
`__unexpected` (436 B). Their prologues and epilogues are hand-written assembly:
`ExPPC_LongJump` has no frame at all (`lmw r13,564(r3)`, `mtcr`, `mtlr r8` from the third
argument, then `blr`); `__throw` writes `old_sp` into its own context by reloading `0(r1)`
after the `stwu` and reads `*(old_sp + 4)` for the caller's LR; `__unexpected` ends with a
longjmp - `lwz r0,0(r1); lwz r1,48(r31); stw r0,0(r1); bl ...; lmw r26; lwz r10,0(r1);
mr r1,r10; mtlr r0; blr`. No C construct reaches the current stack pointer, so these need the
MW runtime source or inline asm, and asm is out of scope for this repo.

**C-shaped but large** - `ExPPC_FindExceptionRecord` (516 B), `ExPPC_NextAction`'s neighbours
`ExPPC_PopStackFrame` (584 B), `ExPPC_ThrowHandler` (1040 B), `ExPPC_UnwindStack` (1292 B).
All four have ordinary prologues and `addi r1,rN; blr` epilogues. `ExPPC_ThrowHandler` also
references `__throw_catch_compare`, `__throw`, `__end__catch`, `__vt__Q23std9exception` and
`__dt__Q23std13bad_exceptionFv`, i.e. it is a real `try`/`catch` body, so its EH tables have to
come out right too.

`ExPPC_FindExceptionRecord` is the cheapest of those and is fully decodable: walk the 16-byte
`__eti_init_info`-style records off `fragmentinfo->exception_info` until one covers `pc`, copy
8 words into the outgoing-argument area, then a division by 3 (`lis r5,10923; addi r5,r5,-21845;
mulhw` - MWCC's `/3`), a binary search over 12-byte entries, and two descriptor walks, one
stride 8 from `x+4` and one stride 6 from `x+2`. That is a whole item of its own, and the same
MWCC idiom table above applies to it.

Also derived, for whoever takes `__throw`: its `ThrowContext` is the same `jmp_buf` layout
`ExPPC_LongJump` reads - FP save area at +0xE0 (16-byte stride, `lfd` at 0xE0+16k, VR slot at
0xE8+16k, 18 slots for f14..f31/v14..v31), `stmw r13` at +0x234, CR at +0x280, link at +0x284
(zero-extended - `lwz`, not `lwz`/`extsh`), +0x288 unused, sp at +0x28C, +0x290 the caller's LR,
then object / cleanup / kind at +0x294/+0x298/+0x29C. `__throw` fills it from a frame whose
context pointer is `r1+24`, so the frame is exactly 0x2C0 bytes.

No `NEW:` filed: the same unit target is already in the queue, so the driver would queue a
duplicate.
