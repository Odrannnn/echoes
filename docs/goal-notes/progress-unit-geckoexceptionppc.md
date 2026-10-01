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