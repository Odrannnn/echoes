# match-sldractorparameters-dtor

**Kind:** `match` **Target:** `main/MetroidPrime/ScriptLoader/SLdrActorParameters` **Result: PASS.**

`tools/goal_check.sh build/goal/item.json` printed, verbatim:

```
goal_check: item match-sldractorparameters-dtor (match) target=main/MetroidPrime/ScriptLoader/SLdrActorParameters
goal_check: baseline .../wt-mp2-goal-L2/build/goal/judge/report.base.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13479 -> 13480   linked 6527 -> 6528
  ok    check_symbol_names.py
  ok    All:  37.55% fuzzy, 30.99% matched, 13.85% linked (13480 / 28465 functions)
  ok    flip_test main/MetroidPrime/ScriptLoader/SLdrActorParameters.cpp: PASS, Object(Matching) in configure.py
goal_check: PASS match-sldractorparameters-dtor
```

`gate.sh` in that run printed `GATE PASS`, and `build/gate-probe.log` reports
`probe: 921 files, 0 failed, 0 errors; link: LINKED (286 undefined, 0 duplicates)`.

## What I did

The carve, four files in one change, each entry in address order:

- **`src/MetroidPrime/ScriptLoader/SLdrActorParameters.cpp`** (new, C++, one function,
  `.text 0x8023F4C4..0x8023F534`, 0x70 = 112 bytes).
- **`configure.py:924`** -
  `Object(Matching, "main/MetroidPrime/ScriptLoader/SLdrActorParameters.cpp", source="MetroidPrime/ScriptLoader/SLdrActorParameters.cpp"),`
  between `Carve8023E5A8.c` (908) and `Carve80241C90.c` (925). One line; 15 lines of comment above it.
- **`config/G2ME01/splits.txt:1910-1911`** - `main/MetroidPrime/ScriptLoader/SLdrActorParameters.cpp:` /
  `.text start:0x8023F4C4 end:0x8023F534`, between `Carve8023E5A8.c` (1907-1908, `.text`
  0x8023E5A8..0x8023E5F4) and `Carve80241C90.c` (1913-1914, `.text` starts 0x80241C90). Nothing
  else claimed those 112 bytes and no claim spans a gap.
- **`files.cmake:849`** - `src/MetroidPrime/ScriptLoader/SLdrActorParameters.cpp`, after
  `Carve8023E5A8.c` (848) and before `Carve80241C90.c` (850).

`build/report.json` after: `total_functions` **28465** (unchanged), `matched_functions`
13479 -> **13480**, `linked` 6527 -> **6528**, `complete_units` +1. The unit is
`metadata.complete: true`, **1/1 function at 100.00%**, `.text` 112/112 bytes.
`build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (unchanged) and all 86 RELs
byte-identical, both from `gate.sh`.

## The unit is named `main/...` and carries `source=` - read this before "fixing" it

`goal_check.sh`'s `match` branch resolves the item's target by searching `configure.py` for
`Object(..., "<target>.cpp")` **verbatim**:

```sh
for u in ([t] if re.search(r'\.(cpp|cp|c)$', t) else [t + '.cpp', t + '.cp', t + '.c']); do
    if re.search(r'Object\(\s*(Matching|NonMatching|MatchingFor)\b[^,]*,\s*\"' + re.escape(u) + '"', s)
```

The target is `main/MetroidPrime/ScriptLoader/SLdrActorParameters`, which is this repo's *objdiff
report* unit-name form, not its `configure.py` form: the other 96 `match` items in the queue all
carry bare `configure.py` paths (`Kyoto/Graphics/CGX`, `MetroidPrime/CRagdoll`, ...) and none
carries a `main/` prefix. So naming the object `MetroidPrime/ScriptLoader/SLdrActorParameters.cpp`
fails the judge with `match target ... has no Object(...) entry in configure.py`, which is a hard
failure - the `PARTIAL` path only covers a flip that *printed* `FAIL`.

The prefix therefore has to be in the `Object` name, and `splits.txt`'s key must match it because
`tools/project.py:1128` looks the object up by the split's own name. `source=` keeps the file in
`src/MetroidPrime/ScriptLoader/` where its neighbours live, which is exactly what the
`MysteryFlyer/...` and `IngBoostBallGuardian/...` entries in this file already do with their module
prefix. Consequence: the objdiff unit name is `main/main/MetroidPrime/ScriptLoader/SLdrActorParameters`
(`project.py:1640` prefixes the module name onto the object name). `goal_check.sh`'s fallback
`target_rose` still finds it, because it matches with `u["name"].endswith("/" + t)`.

**If a future lane wants the natural name**, the change is one string in `configure.py` plus one in
`splits.txt` - but then the item's `target` has to change with it, and that is the driver's field.

## The function

`__dt__19SLdrActorParametersFv`, retail `.text:0x8023F4C4`, 0x70 = 112 bytes, 28 instructions,
`config/G2ME01/symbols.txt:10166`. It was inside `auto_03_8023E5F4_text` (0x8023E5F4..0x80241C90),
dtk's own object, so no unit of ours defined it - confirmed before writing anything:

```
$ grep -rn "SLdrActorParametersFv" src/ include/     # (no output)
$ grep -c . <(./tools/dis.sh 0x8023F4C4 0x70)       # renders, i.e. the bytes are there
```

Claiming it splits the auto unit into `auto_03_8023E5F4_text` (0x8023E5F4..0x8023F4C4, 7 functions)
and `auto_03_8023F534_text` (0x8023F534..0x80241C90, 53 functions); `gate.sh`'s per-function diff
scores that as a split, not a loss, and `total_functions` is still 28465.

**It is the class's own destructor, and the generated header is already retail's layout.** Read
off `__ct__19SLdrActorParametersFv` (0x8023F534): `bl fn_802405CC` on the receiver itself,
`addi r3,r31,64` for `scannable`, `stw r0,68(r31)..stw r0,80(r31)` for the four `CAssetId`s at
+0x44..+0x50, `stb r6,84(r31)` for `useGlobalRenderTime` at +0x54, `stfs` at +0x58/+0x5C for the
two floats, `addi r3,r31,96` for `visor`, and the last two words at +0x70/+0x74. So the three
offsets the destructor needs are the header's own - `lighting` +0, `scannable` +0x40, `visor`
+0x60 - which is why the source includes
`include/MetroidPrime/ScriptLoader/Structs/SLdrActorParameters.hpp` and passes `&self->visor`,
`&self->scannable`, `&self->lighting` rather than a stand-in struct. Order is reverse declaration
order over the only three members that have destructors, and that is the order in retail's bytes
(`addi r3,r30,96` / `addi r3,r30,64` / `mr r3,r30`).

**The three callees have to be called by retail's names, not as C++ member destructors.**
`symbols.txt` gives `SLdrVisorParameters`' and `SLdrLightParameters`' destructors the `fn_`
placeholders (`fn_8023F6C8` at 0x8023F6C8, `fn_80240590` at 0x80240590), so
`__dt__22SLdrVisorParametersFv` / `__dt__22SLdrLightParametersFv` do not exist in the binary and
`self->visor.~SLdrVisorParameters()` would emit a name mwldeppc cannot resolve. Only
`SLdrScannableParameters`' is named (`__dt__23SLdrScannableParametersFv`, 0x80241530). All three
are the same 60-byte deleting-destructor step that `src/MetroidPrime/ScriptLoader/Carve8023E5A8.c`
already reproduces as `__dt__5CMainFv`. `Free__7CMemoryFPCv` (0x802CE388) is declared, never
defined: `Kyoto/Alloc/CMemory.cpp` claims it in the DOL and `src/Kyoto/Alloc/PortMwccNew.cpp`
defines it for the host.

**Reproduced on the first spelling, byte for byte.** Verified before touching any config, with
the exact `Coin.cpp` flags:

```
wibo .../compilers/GC/2.7/mwcceppc.exe -O4,p -inline auto -inline deferred,noauto -lang=c++ ... -c SLdrActorParameters.cpp -o sldr.o
powerpc-eabi-size -A sldr.o   ->   .text 112, .comment 108   (no .data, no .bss, no extra sections)
```

and the 28 instructions are retail's 28, in retail's order, with only the four `bl` displacements
differing (they are `R_PPC_REL24` relocations against the same four symbol names). So no spelling
search was needed and there is no `WALL:` line for this item.

Two things in the spelling are load-bearing and are recorded in the source's own header:

- **`flag` is a `short`.** `extsh. r0,r31`, not `extsb.` - a `bool` or `int` flag changes the test.
- **the return type is `void*`.** The epilogue's `mr r3,r30` is what emits it; as `void` mwcceppc
  drops the move and the function is 4 bytes short of its claim. This is the same measurement
  `Carve80241C90.c` and `Carve8000447C.cpp` record for the identical body shape.
- **`if (flag > 0)` sits inside `if (self)`**, so the receiver's `beq` lands on the epilogue.

## Host side

`files.cmake` is mandatory for the unit (`tools/check_files_cmake.py` fails any configured object
that is neither listed nor in its `EXCLUDED` list, and that list is in `tools/`, which an agent may
not edit). Listing it put three new references into the port's link -
`fn_8023F6C8`, `fn_80240590`, `__dt__23SLdrScannableParametersFv` - and none of the three is in
`docs/research/port_link_gap_list.md` (measured: `grep` on that file returns nothing for all
three), so without definitions the gap would have grown by three. The file therefore carries three
`#ifndef __MWERKS__` empty stand-ins beside the one reference that asks for them, the arrangement
`src/MetroidPrime/Cameras/Carve801E7C14.c` uses for `__dt__17CCameraShakerDataFv`.

Measured outcome: `probe: 921 files, 0 failed, 0 errors; link: LINKED (286 undefined, 0
duplicates)`, and `link_check: duplicate definitions 0`. **The undefined count did not move** - the
three names were not in the gap list before, so defining them closes nothing and opens nothing.
The guard is `__MWERKS__` rather than `TARGET_PC` because in the matching build all three come
from dtk's own `auto_03_8023F534_text.o` and a second definition there would be a duplicate.

`SLdrStructMembers.cpp:105` keeps its own `SLdrActorParameters::~SLdrActorParameters() {}` for the
host, untouched: this unit defines the retail *mangled* name `__dt__19SLdrActorParametersFv` under
`extern "C"`, which is a different symbol from the host's `_ZN19SLdrActorParametersD1Ev`, so there
is no duplicate either way. That is the same arrangement `Carve8000447C.cpp` uses for
`__dt__11CWorldStateFv`.

## Checks run, individually

```
./tools/unit_fit.sh main/MetroidPrime/ScriptLoader/SLdrActorParameters.cpp
   .text  claimed 112  ours 112  retail 112  fits
   no extra functions: our object defines only what the retail unit object does
python3 tools/check_decl_order.py --unit main/main/MetroidPrime/ScriptLoader/SLdrActorParameters
   ok: 1 unit(s) checked, none emits its functions out of retail order
python3 tools/check_symbol_names.py
   checked 587 units; 0 declared names are missing from their object
./tools/flip_test.sh main/MetroidPrime/ScriptLoader/SLdrActorParameters.cpp
   PASS  -> kept as Matching
sha1sum build/G2ME01/main.dol  ->  6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
```

`gate.sh` also ran and passed: module wiring, dol_read, docs claims, gs offsets, raw offsets,
decl order, files.cmake, module order, port probe, port link gap, port link dups, reach stubs.
`docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` show as modified in `git status`: those are
`check_docs_claims.py --write` inside `gate.sh` rewriting the derived counts (matched 13480,
linked 6528, DOL units 11547, probe 921 files). They are machine-made and the driver discards them.

## What is left, measured

- **`__ct__19SLdrActorParametersFv`, 0x8023F534, 0xB4 = 180 bytes**, is the next contiguous function
  in the same unclaimed run and would extend this unit to 0x8023F4C4..0x8023F5E8. It is not
  written: it calls `fn_802405CC` (`SLdrLightParameters`' constructor, 0x802405CC),
  `__ct__23SLdrScannableParametersFv`, `fn_8023F704` (`SLdrVisorParameters`' constructor, 0x8023F704)
  and `__ct__6CColorFffff`, so it needs those four to have bodies or be declared the way this file
  declares them. Left deliberately: the item's reason names the destructor only.
- **The item's stated consumer is still open, and this change is what unblocks it.**
  `progress-scriptcoin-fn58-35d0-rodata-58.md:119-138` wanted `ScriptCoin`'s `fn_58_BC8`
  (0xBC8..0xC2C, 0x64) claimed as a step on the way to `fn_58_C2C`, and stopped because
  `__dt__19SLdrActorParametersFv` was undefined. That name is now defined by a `Matching` DOL unit,
  so `fn_58_BC8` is writable; it is not written here.

NEW: match-sldractorparameters-ctor | match | main/MetroidPrime/ScriptLoader/SLdrActorParametersCtor | __ct__19SLdrActorParametersFv, retail 0x8023F534, 0xB4 = 180 bytes, the function immediately after the destructor this item matched; it is contiguous with it, so the unit extends to .text 0x8023F4C4..0x8023F5E8 and only splits.txt needs a new end address - but it calls fn_802405CC, fn_8023F704, __ct__23SLdrScannableParametersFv and __ct__6CColorFffff, none of which any unit of ours defines, so each has to be declared by retail name the way SLdrActorParameters.cpp declares its three callees