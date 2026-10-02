# progress-twin-rel-ingblobswarm (module:IngBlobSwarm, `progress`)

## Result

**module matched 10 -> 14 functions** (`build/report.json`, summed over `IngBlobSwarm/*`), four
functions in two new `Matching` units, both claimed ranges byte-exact:

| unit | claim | functions | before | after |
|---|---|---|---|---|
| `MetroidPrime/ScriptObjects/CIngBlobSwarmVecTail.cpp` | `.text 0x2350..0x23D0` | `fn_31_2350`, `fn_31_2388`, `fn_31_23A8` | unclaimed (auto unit) | 3/3 at 100% |
| `MetroidPrime/ScriptObjects/CIngBlobSwarmFree.cpp` | `.text 0x1A60..0x1A80` | `fn_31_1A60` | unclaimed (auto unit) | 1/1 at 100% |

The module's unit list after the carve (`build/report.json`):

```
IngBlobSwarm/CScriptIngBlobSwarmRel              5/5
IngBlobSwarm/CIngBlobSwarmFree                   1/1
IngBlobSwarm/CIngBlobSwarmVecTail                3/3
IngBlobSwarm/REL/REL_Setup                       5/5
IngBlobSwarm/auto_00_000000D8_text              None/22
IngBlobSwarm/auto_00_00001A80_text              None/10
IngBlobSwarm/auto_00_000023D0_text              None/17
```

## What was verified, and with what

- `./tools/flip_test.sh MetroidPrime/ScriptObjects/CIngBlobSwarmFree.cpp MetroidPrime/ScriptObjects/CIngBlobSwarmVecTail.cpp`
  -> `kept: 2 / 2   failed: 0   skipped: 0`, both `PASS  -> kept as Matching`.
- `./tools/unit_fit.sh` on both: `.text claimed 32 ours 32 retail 32 fits` and
  `.text claimed 128 ours 128 retail 128 fits`, `no extra functions`.
- `python3 tools/check_decl_order.py --unit CIngBlobSwarmFree` / `--unit CIngBlobSwarmVecTail`:
  `ok: 1 unit(s) checked, none emits its functions out of retail order`.
- `ninja` -> `87 files OK`; all 86 REL `.rel` files `cmp`-equal to `orig/G2ME01/files/RelProd/`
  (also checked directly against `config/G2ME01/config.yml`: `rel hashes differing: 0`).
- `./tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS`**, with
  `counts: matched 13335 -> 13339   linked 6383 -> 6387` and
  `target rose: module:IngBlobSwarm: 10 -> 14 / 63 functions`.

## The one finding worth more than the four functions

**A claimed range's call to a module import must use the import name that *that call site*
records, which is not always the name the same DOL function has elsewhere in the module.**
`fn_31_1A60` (`.text 0x1A60`, the module's own `__sys_free`) is 32 bytes and calls
`CMemory::Free`. Written the obvious way - declare `extern "C" void Free__7CMemoryFPCv(const void*)`
and call it, which is what `CFrontEndDataNetworkRel.cpp` does - it compiles, links, produces a
**byte-identical `.text`**, and the module's sha1 still fails:

```
build/G2ME01/IngBlobSwarm/IngBlobSwarm.rel: FAILED
86 files OK
WARNING: 1 computed checksum(s) did NOT match
```

`.text` diff over the whole 11992-byte section: **0 bytes**. 662 differing bytes, all in the import
table, and the header field at file offset 0x48 (the import table's own offset) reads `0x32a0`
against retail's `0x32a8` - the table is 8 bytes short. Counting records: retail has 42
`CMemory::Free` call fixups (`kind 0x0a04`, address `0x802ce388`), the wrong build has 43, and
retail's `fn_80_81C8` entry is the one that goes missing.

`build/G2ME01/IngBlobSwarm/asm/*.s` is where the answer is: this module spells the same DOL
function **two ways** -

```
0x1A6C  bl fn_80_81C8            <- fn_31_1A60's only call
0x2558  bl Free__7CMemoryFPCv    (also 0x25AC, 0x2610, 0x2654, 0x2664, 0x26BC, 0x2870)
```

Calling it `fn_80_81C8` instead of `Free__7CMemoryFPCv` makes the module hash. So **dtk records a
fixup per import *name*, not per call site**: renaming one call site to the other name for the same
address is an extra record, and a claim containing any import call has to carry the name dtk
printed for it. Read the name off the module's own disassembly, not off the DOL's symbol.

## Other things measured, so the next lane does not repeat them

- **`force_active:` was not needed here, and I did not leave one in `config.yml`.** `fn_31_2350`,
  `fn_31_2388` and `fn_31_23A8` are *not* in the generated
  `build/G2ME01/IngBlobSwarm/ldscript.lcf` FORCEACTIVE block, and are reachable only from each
  other and `fn_31_23D0`. `AIMannedTurret`'s `fn_1_48C0` is the same shape and does need a
  `force_active:` entry. Measured: with all three added to a `force_active:` list, `ninja` printed
  `87 files OK`; with the list removed again, `87 files OK` again. A claim that is *not* a lone
  orphan is kept by mwldeppc even when nothing in data reaches it. (`fn_31_1A60` needs nothing for
  a second reason: it *is* in FORCEACTIVE, because data stores its address.)
- **`fn_31_2350` only matches as a struct member access, not as raw offsets.** Spelled over raw
  offsets (`int& count = *(int*)(self + 4); ... count * 0x4C`), 29 of 32 instructions in the range
  differ - all from one allocation decision: the count lands in `r7` where retail puts it in `r5`,
  and the `mulli` destination follows (`1c a7 00 4c` against `1c 05 00 4c`). Spelled as members of
  a `SIngBlobVec { int; int mCount; int; SIngBlobElem* mItems; }` with
  `self->mItems + self->mCount++` - the `mCount++` **inside** the index expression, which is what
  puts the `stw` between the `mulli` and the `add` - it is byte-exact with no register pressure
  change at all. This is the item reason's "a member function only matches written as a member of a
  declared class" in a raw-pointer costume: the register allocation is part of the match.
- **`fn_31_23A8` (`construct_impl`) is the early-return spelling** and needed no adjustment:
  `if (dest == 0) { return; }` gives retail's `stwu / mflr / cmplwi / stw r0 / beq / bl / epilogue`,
  the same order as `fn_39_798` in `CLumiteRelTail.cpp`.
- This module's block compiles at the default **GC/1.3.2**, where **`nullptr` is undefined**
  (`Error: undefined identifier 'nullptr'`) - write `0`.
- `dtk dol split` **rewrites `config/G2ME01/rels/IngBlobSwarm/splits.txt` in address order** on the
  next build; the file in the diff is dtk's ordering, not mine.
- MWCC emits `bl` as `48 00 00 01` in the relocatable object, so a byte-compare against the retail
  `.rel` shows the three `bl`s as differences before linking. They are not.

## What is left in this module (49 functions unmatched)

The item's other runs, unchanged and unclaimed:

- `0x25CC..0x28B8`, 7 adjacent twins, 748 bytes - the largest run and the obvious next target:
  `__dt__rstl::auto_ptr<CScannableObjectInfo>`, `__dt__rstl::single_ptr<...>` x2,
  `__ct__CActorParameters(CActorParameters const&)`, `__ct__CLightParameters(CLightParameters const&)`,
  `rstl::vector<CWorldState>::reserve(int)` and a second `__sys_free`. **It is claimable**: every
  call name it needs is already printed in `build/G2ME01/IngBlobSwarm/asm/auto_00_000023D0_text.s`,
  and the ones absent from `config/G2ME01/rels/IngBlobSwarm/symbols.txt` are DOL *imports* dtk has
  already resolved - call them by exactly the printed name:
  `__dt__6CTokenFv` (0x2548/0x259C), `Free__7CMemoryFPCv` (0x2558/0x25AC/0x2610/0x2654/0x2664/0x26BC),
  `__dt__9CAnimDataFv` (0x2600), `__dt__10CModelDataFv` (0x26AC),
  `allocate__Q24rstl17rmemory_allocatorFi` (0x281C), and the module's own `fn_31_2770`, `fn_31_2898`,
  `fn_31_2948`, `fn_31_28B8`. All seven must be byte-exact at once or the claim breaks the hash.
  Both copy constructors' layouts are already modelled in `src/MetroidPrime/CActorParameters.cpp`.
- `0x1A60`'s neighbours are `__dt__` groups at 0x1B44/0x1BC8/0x1D34/0x1DB8/0x250C (five isolated
  single functions, each with a matched twin in the DOL) and `0x10CC` (`__dt__80004B9C`).
- 22 functions remain in `auto_00_000000D8_text` (0xD8..0x1A60) and 17 in
  `auto_00_000023D0_text` (0x23D0..0x2D34); these are the behavioural `CIngBlobSwarm` code and need
  the `CActor`/`CPatterned` hierarchy, which is why `configure.py`'s comment above still leaves
  them to dtk.

NEW: progress-twin-rel-ingblobswarm-2 | progress | module:IngBlobSwarm | .text 0x25CC..0x28B8, 7 adjacent twins (748 B), every call name already printed in the module's asm, both copy-constructor layouts in src/MetroidPrime/CActorParameters.cpp - all seven must be byte-exact at once or the claim breaks the hash