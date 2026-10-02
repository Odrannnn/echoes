# carve-80046db8 — `MetroidPrime/Carve80046DB8` (kind: `match`)

**Result: `Matching`, 4/4 functions, and the judge passed.**

```
goal_check: item carve-80046db8 (match) target=MetroidPrime/Carve80046DB8
goal_check: baseline .../wt-mp2-goal-L4/build/goal/judge/report.base.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12582 -> 12586   linked 5944 -> 5948
  ok    check_symbol_names.py
  ok    All:  35.45% fuzzy, 29.27% matched, 12.98% linked (12586 / 28465 functions)
  ok    flip_test MetroidPrime/Carve80046DB8.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-80046db8
```

## The claim

`.text 0x80046DB8..0x80046F88`, 0x1D0 = 464 bytes, four functions of 0x74 (29 instructions each),
carved out of dtk's `main/auto_03_80045CDC_text` (which ran 0x80045CDC..0x800476A8). The four
`fn_<addr>` placeholders are `config/G2ME01/symbols.txt:1322-1325`; the instructions were read from
`build/G2ME01/asm/auto_03_80045CDC_text.s:1269-1409` **before** the claim split that unit.

Claimed exactly this range, nothing either side. Below: `do_erase__Q24rstl43list<9TUniqueId,...>` at
0x80046D44 — a *named* symbol, so it needs its own mangling and a `.cpp`, out of scope for an `fn_`
carve. Above: `fn_80046F88` (0x8C), which walks nodes whose element has an out-of-line destructor,
a shape these four do not cover.

## What the four are

Three `rstl::list<T>::~list()` copies and one `do_erase(node*)`. `include/rstl/list.hpp:251` and
`:263` are the source all four are instantiations of. Both bodyshapes were already matched
byte-for-byte elsewhere in this tree, which is why this reproduces their logic rather than inventing
one: `src/MetroidPrime/CWorld.cpp` at 0x80052788/0x800527FC (the seed's twin; both
`fuzzy_match_percent` 100.0 in `report.json`), and four more copies in
`src/MetroidPrime/ScriptObjects/Carve801E3864.c` — a carve of the identical 4x`0x74` shape that
matches. That file's header layout, struct definitions and `Free__7CMemoryFPCv` declaration were
the template here.

The offsets the bytes pin down, all three read by the `do_erase` body: `mStart` at +4, `mEnd` at
+8, `mCount` at +0x14, `node { mPrev, mNext }` at +0/+4. No copy emits an element destructor call,
so every element type here is trivially destructible.

**One correction to the seed, worth carrying to the next lane:** the seed listed all four as byte-shape
twins of the `CWorld` `~list`. That is right for three of them. **`fn_80046EA0` is a different body** —
it carries the `mCount` decrement `subi r0,r4,0x1 ; stw r0,0x14(r30)`, which the other three never
emit, and its result travels back in `r31` rather than `r29`. It is a `do_erase`, not a fourth
`~list`. It still lands in the same unit (the claim is one contiguous run, and
`RUNNING_THE_DECOMP.md` rule 1 is explicit: a contiguous run is one unit — do not split it).

## Who owns them (from the five `bl` edges in the DOL, measured)

- **`fn_80046F14`** — one caller. `__dt__13CStateManagerFv` at 0x80042B24 passes `+0x1608`, and
  `include/MetroidPrime/CStateManager.hpp:348` names that member
  `rstl::list< rstl::reserved_vector< CEntity*, 32 > > mGraveyard`. The **only** one of the four
  whose element type is known, and consistent with the empty `~T()` the bytes show — a
  `reserved_vector<CEntity*, 32>` is a pointer array with no destructor of its own.
- **`fn_80046DB8`** — five callers across four units: `__dt__13CStateManagerFv` (`+0x8D4`,
  0x80042B58), `CGameArea`'s dtor (`+0x1C4`, 0x8005DFF4), and `CScriptTrigger` three times (a stack
  local at 0x800726B4; `node->mItem` at 0x800733A8; `node+0xC` at 0x80073428). A list shared that
  widely has an element type no symbol names, so the address is the best name available.
- **`fn_80046E2C`** — one caller, 0x80229BF8 in `auto_03_80229BC4_text.s`: a deleting destructor that
  stores `lbl_803B86A8` as its vtable (`symbols.txt:18397`, `type:object size:0x10`) and then
  destroys the list at `self+4`.
- **`fn_80046EA0`** — the only one called as `do_erase` rather than as a destructor. Its single
  caller `fn_800418B8` (0x800418B8, 0x64) walks a range with its return value —
  `bl fn_80046EA0 ; mr r0,r3 ; cmplw r0,r31 ; bne` — the `erase(first, last)` shape.

## The one `bl` target

All four `bl`s resolve to **0x802CE388**, retail's `CMemory::Free(void const*)` — checked by decoding
the displacements out of `build/G2ME01/main.elf` at 0x80046DEC, 0x80046E60, 0x80046EE8 and 0x80046F48,
all four landing on 0x802CE388. Declared `extern void Free__7CMemoryFPCv(const void*);`, never
defined here. No `PortLinkStubs.cpp` duplicate: `grep -rln fn_80046DB8 src/` is empty, and the gate's
`port link gap` / `port link dups` steps pass.

## The four files, each in address order

- `config/G2ME01/splits.txt:129-130` — new `MetroidPrime/Carve80046DB8.c` block between
  `Carve80045CD4.c` (ends 0x80045CDC) and `CEntity.cpp` (0x800476A8).
- `configure.py:662` — `Object(Matching, "MetroidPrime/Carve80046DB8.c"),` on **one line**.
- `files.cmake:421` — `src/MetroidPrime/Carve80046DB8.c`.
- `src/MetroidPrime/Carve80046DB8.c` — the source, new file.

Plain C, so the `fn_` names do not mangle. Definitions **descending by address**: fn_80046F14,
fn_80046EA0, fn_80046E2C, fn_80046DB8. `python3 tools/check_decl_order.py --unit
src/MetroidPrime/Carve80046DB8.c` → `ok: 0 unit(s) checked, none emits its functions out of retail
order`, and the gate's own `decl order` step reads `ok`. No `asm` in the diff (the only two `asm`
strings in the new file are comment prose naming dtk's own `build/G2ME01/asm/...` file).

## What I measured

```
sha1sum build/G2ME01/main.dol               6ef9b491d0cc08bc81a124fdedb8bfaec34d0010  (retail)
./tools/decomp_build.sh                     All: 35.45% fuzzy, 29.27% matched, 12.98% linked
                                            (12586 / 28465 functions)
./tools/flip_test.sh MetroidPrime/Carve80046DB8.c
  PASS  -> kept as Matching
  kept: 1 / 1   failed: 0   skipped: 0
python3 tools/check_symbol_names.py         checked 528 units; 0 declared names are missing
```

`build/report.json` for the new unit:

```
main/MetroidPrime/Carve80046DB8   total_functions 4   matched_functions 4
  matched_code 464 / 464   fuzzy 100.0   complete_units 1
    fn_80046F14 116 100.0    fn_80046EA0 116 100.0
    fn_80046E2C 116 100.0    fn_80046DB8 116 100.0
```

DOL function total is still **28465** after the `splits.txt` edit. The gate's `per-function diff`
step reads `SPLIT main/auto_03_80045CDC_text: 13 function(s) moved into
main/MetroidPrime/Carve80046DB8, main/auto_03_80046F88_text (exact count match - a split, not a
loss)` — the expected carve signature, not a regression. `counts: matched 12582 -> 12586, linked
5944 -> 5948`: +4 each, this item and nothing else.

## No blockers, no `NEW:` lines, no commit

The carve vein worked first try because the shape had already been matched twice in this tree. One
thing for the next lane, filed here rather than as a `NEW:` item because it is a header fix and its
success does not obviously raise a count on its own:

**NOTE:** `include/MetroidPrime/CStateManager.hpp:333` still declares `char x8d4_[0x18];`, but the
measured `__dt__13CStateManagerFv` edge at 0x80042B58 (`addi r3,r28,0x8d4 ; li r4,-1 ; bl
fn_80046DB8`) shows an `rstl::list` at exactly that offset — 0x8D4..0x8EC, and the 0x18 the
placeholder reserves is precisely the list's own size (`mAllocator` at +0 through `mCount` at
+0x14). Naming that member would let a later `CStateManager` unit refer to the list instead of
passing an address. Not touched here: the element type is not recoverable from the bytes (five call
sites, four units, no symbol names it), and the driver discards edits outside the four carve files.

The `docs/HANDOFF.md` / `docs/RUNNING_THE_DECOMP.md` lines the judge rewrote during `goal_check.sh`
were reverted, so the diff is exactly the four files of the carve.
