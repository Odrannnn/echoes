# progress-unit-cmemorycarddriver

**Result: PASS.** `tools/goal_check.sh build/goal/item.json` in `wt-mp2-goal-L8` (branch
`goal/lane-8`, clean tree + this diff) printed:

```
goal_check: item progress-unit-cmemorycarddriver (progress) target=MetroidPrime/CMemoryCardDriver
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12446 -> 12451   linked 5863 -> 5863
  ok    check_symbol_names.py
  ok    All:  35.19% fuzzy, 28.94% matched, 12.90% linked (12451 / 28465 functions)
  ok    target rose: main/MetroidPrime/CMemoryCardDriver: 47 -> 52 / 53 functions
  ok    no asm added
goal_check: PASS progress-unit-cmemorycarddriver
```

**Files touched:** `src/MetroidPrime/CMemoryCardDriver.cpp` only. No header, no `configure.py`,
no `config/`, no `tools/`, no `build/goal/` (this notes file is the driver's copy, outside the
lane worktree, so the lane's `git status` stays `M src/MetroidPrime/CMemoryCardDriver.cpp`).

## Re-measured baseline (the item's `reason` numbers re-measured before acting)

`./tools/decomp_build.sh main/MetroidPrime/CMemoryCardDriver` on the clean tree:

```
main/MetroidPrime/CMemoryCardDriver: 89.29% fuzzy, 84.03% matched (47 / 53 functions)
   GetSaveSignature__Fv                                   1.94%  288 bytes
   fn_8017C2B4                                            0.00%  184 bytes
   fn_8017C27C                                            0.00%   56 bytes
   __ct__17CMemoryCardDriverFQ214CMemoryCardSys15EMemoryCardPortUiUiUib  61.88%  812 bytes
   fn_8017BED4                                            0.00%  124 bytes
   fn_8017BE84                                            0.00%   80 bytes
```

47/53 confirmed. All four `fn_*` were genuinely 0.00%, not a report artefact.

## What each function turned out to be

Every one of the four 0% functions is a **template instantiation that retail's symbol table
carries under an unnamed `fn_`-name**. That is the situation `include/rstl/reserved_vector.hpp`
already documents for `operator=` ("a template instantiation is emitted under its mangled name, so
objdiff never pairs it with the retail symbol, so it has to be written out by hand in a .cpp under
an `extern "C"` name"), and `src/MetroidPrime/CActorModelParticles.cpp:735-755` is the worked
example of the spelling. mwcceppc runs this unit `-inline deferred,noauto`, so these come out
out-of-line rather than inlined into their callers.

### `fn_8017C2B4` - 0x8017C2B4, 0xB8 bytes - `uninitialized_fill_n<rstl::reserved_vector<uchar,32>*, ...>`

```cpp
extern "C" void fn_8017C2B4(rstl::reserved_vector< uchar, 32 >* dest, int n,
                            const rstl::reserved_vector< uchar, 32 >* src) {
  for (int i = 0; i < n; ++i, ++dest) {
    rstl::construct(dest, *src);
  }
}
```

Retail reuses **one** source object for all `n` elements: `lwz r6,0(r5)` is hoisted out of the loop
and `addi r9,r5,4` is loop-invariant, so every element gets the same 32 bytes and the same length.
`cmplwi r3,0 / beq` is `rstl::construct`'s checked-pointer test, and `stw r6,0(r3)` ahead of the
copy is the copy constructor's `mCount(other.mCount)`.

*Before: 0.00%. Tried: (a) `for (i...) rstl::construct(&dest[i], *src)` -> 99.52%, the only
difference being the loop's two increments in the order `addi r3,r3,36` then `addi r10,r10,1`
where retail has `addi r10,r10,1` then `addi r3,r3,36`; (b) the comma-expression spelling
`for (int i = 0; i < n; ++i, ++dest)` (i.e. `uninitialized_fill_n`'s own loop) -> **100%.***

### `fn_8017C27C` - 0x8017C27C, 0x38 bytes - `rstl::reserved_vector<rstl::reserved_vector<uchar,32>,3>::reserved_vector(int, const T&)`

```cpp
extern "C" rstl::reserved_vector< rstl::reserved_vector< uchar, 32 >, 3 >*
fn_8017C27C(rstl::reserved_vector< rstl::reserved_vector< uchar, 32 >, 3 >* self, int count,
            const rstl::reserved_vector< uchar, 32 >& value) {
  self->mCount = count;
  fn_8017C2B4(self->data(), count, &value);
  return self;
}
```

Retail calls it with exactly `(this, 3, &seed)` (0x8017C0C4..0x8017C0D0), which is the
`(count, value)` overload, not the one-argument fill constructor the tree had been using.

*Before: 0.00%. Tried: (a) `void` return -> 70.29%, missing retail's `stw r31,12(r1)` /
`mr r31,r3` / `mr r3,r31`; (b) returning `self` -> **100%.** The `extsh`-free `stw r4,0(r3)` with
no null test on `self` confirms it is a constructor, not a free function.

### `fn_8017BED4` - 0x8017BED4, 0x7C bytes - `mFileSlots`'s `rstl::reserved_vector::destroy_elements()`

```cpp
extern "C" void fn_8017BED4(rstl::reserved_vector< rstl::auto_ptr< SGameFileSlot >, 3 >* self) {
  rstl::auto_ptr< SGameFileSlot >* ptr = self->data();
  for (int i = 0; i < self->mCount; ++i) {
    rstl::destroy(&ptr[i]);
  }
}
```

`r31` walks `self + 4` (`data()`) in 8-byte `rstl::auto_ptr` steps - `rstl::auto_ptr` is
`{ mutable bool mHas; T* mItem; }` (`include/rstl/auto_ptr.hpp`), 8 bytes with the padding, which
is what fixes the stride and `lbz 0(r31)` / `lwz 4(r31)`. `mCount` is re-read from `0(self)` on
every trip rather than hoisted, which is what the plain `for` gives.

*Before: 0.00%. After: **100%** on the first spelling tried.*

### `fn_8017BE84` - 0x8017BE84, 0x50 bytes - the same vector's `~reserved_vector()`

```cpp
extern "C" void* fn_8017BE84(rstl::reserved_vector< rstl::auto_ptr< SGameFileSlot >, 3 >* self,
                             short flags) {
  if (self) {
    fn_8017BED4(self);
    if (flags > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}
```

Identified because `CMemoryCardDriver::~CMemoryCardDriver` calls it at 0x8017BE4C with
`li r4,-1` - MW's destructor flag, member not deleted. `extsh. r0,r31 / ble` is the gate, so the
parameter must be a **signed 16-bit** type: declared `int`, mwcceppc emits `cmpwi r31,0`.
`mr r3,r30` in the epilogue is the `this` a destructor returns, so the return type is `void*`.

*Before: 0.00%. After: **100%** on the first spelling tried.*

## The constructor - 61.88% -> 100.00% (812 bytes)

Three things were missing, all now in the source.

**1. `mSaveIdx` was a hard-coded 0.** Retail loads it from the live game state: `lwz r3,-28360(r13)`
(gpGameState) then `lwz r4,124(r4)` then `stw r4,272(r30)` (0x8017C078..0x8017C08C). `124` = 0x7C
= `CGameState::mSystemOptions` (0x54) + `CPersistentOptions::mSaveIdx` (0x28), so
`gpGameState->SystemOptions().GetSaveIdx()`. Both offsets are this tree's own - the note in
`CMainResetGameState.cpp:309` about "0x28 lower than retail" is about the *slot*
`CGameGlobalObjects::gameState`, not about `CGameState`'s members; `+0x118` for
`mCompressedGameStates` (which the existing comment in `CGameState.hpp:214-221` already quotes) and
`+0x54` for `mSystemOptions` fall out of the same layout and both land where retail reads them.

**2. The three `.sdata` fill bytes were `uchar(0)`.** Retail reads three *distinct* one-byte
objects: `lbz r0,-30800(r13)` for `mSystemData`, `lbz r3,-30799(r13)` for the `mGameOptionsData`
seed and `lbz r0,-30798(r13)` for `mGlobalGameOptionsData` (0x8017BFB4 / 0x8017C094 / 0x8017C0E8)
= `.sdata:0x80418530`, `0x80418531`, `0x80418532` (`symbols.txt:19912-19914`), each
`size:0x1 data:byte`. Declared as `extern "C" unsigned char`, **not** `const`, for the reason the
existing `lbl_80418533` comment gives: `uchar(0)` puts a local byte in our own `.data` under a local
label, retail has `R_PPC_EMB_SDA21` against a named retail address.

**3. The serialisation tail did not exist.** The tree carried a TODO ("Read the selected save index
from persistent options and serialize the system options and default CGameOptions into their
respective bitstream buffers"); the port was writing nothing into any of the four option buffers.
Retail 0x8017C150..0x8017C258: `gpGameState->SystemOptions().PutTo()` through a `CBitStreamWriter`
over a `CMemoryStreamOut` on `mSystemData`, then **one** default-constructed `CGameOptions` put
into each of the three `mGameOptionsData` slots (loop at 0x8017C1AC, `r31 += 36`, `cmpwi r29,3`) and
into `mGlobalGameOptionsData`, then `~CGameOptions`. Each `CMemoryStreamOut`/`CBitStreamWriter` pair
is constructed and destroyed **inside** the loop (0x8017C1C4 / 0x8017C1E8), not one pair reused -
that is what puts the loop's temporaries at `r1+288`/`r1+28` and the tail's at `r1+156`/`r1+16`.
The `SGameOptionsMirror` + `__ct__12CGameOptionsFv` / `fn_80004D84` arrangement (already in the file
for `EraseFileSlot`) is what lets a `CGameOptions` local exist at all; it was moved above the
constructor, which now needs it too. Retail's frame is 576 bytes and ours is now 576 - it follows
from the same set of `CMemoryStreamOut` temporaries (0x84 each, last one at `r1+420`).

The `(count, value)` spelling of the `mGameOptionsData` member-initialiser matters here too:
`mGameOptionsData(3, rstl::reserved_vector< uchar, 32 >(lbl_80418531))` rather than the one-argument
fill constructor, because only the two-argument overload passes the count in `r4` and the value in
`r5`, as retail does (`li r4,3` / `addi r5,r1,52` / `bl`, 0x8017C0C4..0x8017C0D0). Measured:
one-argument form 65.01%, two-argument form **100.00%**.

*Measured ladder for the constructor:* 61.88% as found -> 65.01% after `mSaveIdx` + the three fill
bytes -> 99.48% after adding the serialisation tail (still the wrong `addi r4`/`addi r5` for the
seed argument) -> 100.00% after the `(count, value)` spelling.

## Final numbers (from `build/report.json`, `tools/fast_try.sh`)

```
main/MetroidPrime/CMemoryCardDriver: 97.08% fuzzy, 97.02% matched code, 52/53 functions
     1.94%     288 B  GetSaveSignature__Fv
```

`fn_8017BE84`, `fn_8017BED4`, `fn_8017C27C`, `fn_8017C2B4` and
`__ct__17CMemoryCardDriverFQ214CMemoryCardSys15EMemoryCardPortUiUiUib` are all
`fuzzy_match_percent: 100.0` in `build/report.json`. Whole-project `matched_functions`
12446 -> 12451; `linked` held at 5863; DOL sha1 and all 86 REL hashes verified by `gate.sh`.

`python3 tools/check_decl_order.py` -> `ok: 981 unit(s) checked, 28 permuted, all 28 accounted for`
(unchanged: the new functions are declared between `IsCardReading` (0x8017C36C) and the constructor
(0x8017BF50), and between the constructor and the destructor (0x8017BDE8), i.e. descending by
retail offset, which is where `check_decl_order.py` wants them).
`python3 tools/check_symbol_names.py` -> `checked 525 units; 0 declared names are missing`.

## `unit_fit.sh` - what a future flip of this unit still has to clear

`./tools/unit_fit.sh MetroidPrime/CMemoryCardDriver.cpp` (before and after this change):

| | before | after |
|---|---|---|
| `.text` claimed 9668 | ours 10360, **over by 692** | ours 11076, **over by 1408** |
| functions in ours but not in retail | 18, 1700 bytes | 18, **1688 bytes** |

The extra count did not change and the extra bytes went *down* 12. Diffing the two objects'
symbol tables: the only genuinely new symbol is
`__ct__Q24rstl50reserved_vector<Q24rstl22reserved_vector<Uc,32>,3>FiRCQ24rstl22reserved_vector<Uc,32>`
(56 bytes), and it **replaces** the one-argument
`...FRCQ24rstl22reserved_vector<Uc,32>` (68 bytes) that was already an extra - 68 - 56 = 12. It is
the compiler's own out-of-line copy of the `(count, value)` constructor the member-initialiser
calls, and it is the "COMDAT weak copy that both the retail linker and mwldeppc discard" case
`unit_fit.sh` describes. `fn_8017BE84/84/27C/2B4` are **not** extras: `build/G2ME01/obj`'s retail
object defines all four (`nm -S` on it lists them at 0x1f00/0x1f50/0x22f8/0x2330).

So the +716 in `.text` is the four new in-range retail functions (80+124+56+184 = 444) plus the
constructor growing 528 -> 812 (= retail's exact size), and **this unit still cannot flip**: the
other 17 extras are pre-existing (the `rstl::basic_string` `__ct__`/`__pl__`/`append` trio from
`InitializeFileInfo`, the `CGameStateEnvVarManager`/`rstl::map`/`red_black_tree` destructors pulled
in by `ExportPersistentOptions`, `__dt__15CMemoryInStreamFv`, `__dt__Q24rstl45single_ptr<...>Fv`,
`__dt__Q24rstl25auto_ptr<13SGameFileSlot>Fv`, `__dt__Q24rstl53reserved_vector<...>::destroy_elements`
and the 184-byte `uninitialized_fill_n<...>`), and the object also carries `.data` 100 / `.sdata` 2
/ `.sbss` 1 that `splits.txt` does not claim. Do not expect `flip_test.sh` to pass on this unit
until those are gone. (`python3 tools/check_decl_order.py --unit MetroidPrime/CMemoryCardDriver.cpp`
is a no-op - the tool takes no `--unit`; the unfiltered run above is the one that checks it.)

## What is left, and why

**`GetSaveSignature__Fv` - 0x8017C428, 0x120 bytes, still 1.94%.** Not attempted this run: it is
not a template instantiation and nothing in the tree decompiles it yet, so it needs four
externally-resolved globals plus a resource-manager traversal to land byte-exactly, and a
`progress` item is scored on exact matches, not on partial progress. What it does (for whoever
takes it next):

* Two `.sbss` statics: `0x80419244` (a `uint`, and `symbols.txt:20656` already names
  `lbl_80419248` as `.sbss:0x80419248 size:0x4 data:4byte`) and a one-byte "already computed" flag
  at `0x80419248`. `GetSaveSignature` writes `-1` into the first and `1` into the second the first
  time it is entered (0x8017C450..0x8017C45C), then reads both back.
* The sentinel test is **not** `cmpwi r3,-1`: it is `addis r0,r3,1 / cmplwi r0,65535 / beq`
  (0x8017C464), i.e. `((sig + 1) & 0xFFFF) == 0xFFFF`, `sig & 0xFFFF == 0xFFFE`. Whatever the source
  spelling, it has to lower to that pair.
* Then a loop over a global table pointer at `.sdata:0x8041F614` (`lwz r30,-28356(r13)`), iterating
  `lwz 0(r30)`-sized entries of 112 bytes (`mulli r0,r0,112` at 0x8017C514, `addi r29,r29,112` at
  0x8017C508) between `lwz 24(r30)` and `lwz 16(r30)`*112. Each entry's `+0x0C` word is built into
  a two-word `CAssetId` on the stack together with `"SAVW"`-shaped `0x53535657`, handed to a
  `CToken` ctor, resolved with `CToken::GetObj()`, and the loaded object's `+0x04` word is XORed
  into the signature through `fn_80182C84` (0x80182C84, 0x1F8 bytes - itself unmatched retail).
  A loader object is fetched from `.sdata:0x8041F628` (`lwz r4,-28376(r13)`) and called through
  its vtable slot at `+0x0C` with `bctrl`.
* Until that is done the existing `static uint GetSaveSignature() { return 0; }` stays as it is.
  It is honest - it carries a `TODO` naming the missing work - so it was left alone rather than
  replaced with something that looks right and is not.

NEW: none. Both remaining functions are inside the item's own target, and `GetSaveSignature` needs
`fn_80182C84` plus a resource table that no unit currently exposes, so it is not a separately
queueable unit.

## Codegen rules learned here (for `docs/` later, not filed as items)

* **A `for` loop's two increments are ordered as written.** `rstl::uninitialized_fill_n`'s own
  `for (int i = 0; i < n; ++i, ++cur)` emits `++i` before `++cur`; a loop that indexes
  `&dest[i]` emits the address bump first. Same body, 99.52% vs 100%.
* **MW destructors and constructors return `this`.** A `void` spelling of retail's
  `fn_8017C27C`/`fn_8017BE84` loses the `mr r3,r31` / `stw r31,12(r1)` pair; returning the object
  pointer puts them back.
* **A destructor flag is compared as a signed 16-bit value** (`extsh. r0,r3 / ble`), not an `int`.
* **The member-initialiser list cannot call retail's out-of-line constructor by name, and that does
  not matter.** With the `(count, value)` overload the emitted call goes to the compiler's own
  `__ct__Q24rstl50reserved_vector<...>FiRCQ...`, not to `fn_8017C27C`. The two objdiff commands
  disagree about that one instruction and it is worth knowing which is which:
  `objdiff-cli diff -1 <ours> -2 <retail>` reports the constructor at **99.95074%** with a single
  `DIFF_ARG_MISMATCH` on that `bl` (the relocation's `target_symbol` index, i.e. the symbol *name*),
  while `objdiff-cli report generate` - the one `build/report.json` and every gate read - reports
  **100.0**. The bytes and the call target are right; only the label differs. Do not contort the
  source to reach `fn_8017C27C` from the member-initialiser, and do not read the `diff` subcommand's
  99.95 as a regression - `report.json` is the measurement the item is judged on.