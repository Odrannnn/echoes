# carve-80335b7c - Kyoto/Math/Carve80335B7C.c (kind: match)

**Result: DONE.** The unit is `Matching`, `tools/flip_test.sh` PASSes, and
`tools/goal_check.sh build/goal/item.json` returns `goal_check: PASS carve-80335b7c`
with `matched 13602 -> 13603` and `linked 6650 -> 6651`.

## What I did

Carved `.text 0x80335B7C..0x80335B84` (0x8 = 8 bytes, 1 function) out of dtk's
`auto_03_80335B5C_text` as its own unit `src/Kyoto/Math/Carve80335B7C.c`. The four files:

- `src/Kyoto/Math/Carve80335B7C.c` (new) - `fn_80335B7C`, a one-line struct load.
- `config/G2ME01/splits.txt:3374-3375` - new
  `Kyoto/Math/Carve80335B7C.c: .text start:0x80335B7C end:0x80335B84`, entered between
  `Carve80335B58.c` (ends 0x80335B5C) and `Carve80337198.c` (starts 0x80337198), i.e. in
  address order.
- `configure.py:1083` - `Object(Matching, "Kyoto/Math/Carve80335B7C.c"),` one line, between
  the same two neighbours.
- `files.cmake:1121` - `src/Kyoto/Math/Carve80335B7C.c`, same position.

No `PortLinkStubs.cpp` duplicate had to go: `grep -rn "fn_80335B7C" src/ include/ tools/
config/` returns only `config/G2ME01/symbols.txt:15115` (the new file aside), so nothing
else in either build defines the symbol.

## The instructions, as measured

`build/G2ME01/asm/auto_03_80335B5C_text.s` lines 33-36 (dtk's own listing, still on disk
because the claim did not exist when this lane started):

```
fn_80335B7C  0x80335B7C  0x8   lwz r3, 0x1c(r3) ; blr
```

`build/binutils/powerpc-eabi-objdump -d --start-address=0x80335b7c
--stop-address=0x80335b84 build/G2ME01/main.elf` confirms retail's own bytes are
`80 63 00 1c / 4e 80 00 20`.

The twin the item names is real and was read, not assumed.
`symbols.txt:1559` puts `IGetAreaCount__6CWorldCFv` at `.text:0x8005066C`; objdump there
gives `lwz r3,28(r3) ; blr`, byte-identical. Its source is
`src/MetroidPrime/CWorld.cpp:405`, `int CWorld::IGetAreaCount() const { return mAreas.size(); }`
- a getter returning a member by value in `r3`. So the shape here is a getter, and the
body below is written as one. The function calls nothing and names no data address, so the
unit declares no `extern` and no constant.

## What I measured

    ./tools/decomp_build.sh Kyoto/Math/Carve80335B7C.c
      -> All: 37.69% fuzzy, 31.12% matched, 13.99% linked (13603 / 28465 functions)

`total_functions` is still **28465** and `total_units` 2403 -> 2405, so the `splits.txt`
edit moved no claim and added one unit. `sha1sum build/G2ME01/main.dol` is
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.

    ./tools/carve_diff.sh 0x80335B7C 0x8 build/G2ME01/obj/Kyoto/Math/Carve80335B7C.o
      retail: 2 instructions, 8 bytes
      ours  : 2 instructions, 8 bytes
      differing instructions: 0
      BYTE-EXACT

    ./tools/unit_fit.sh Kyoto/Math/Carve80335B7C.c
      .text  claimed 8  ours 8  retail 8  fits
      no extra functions: our object defines only what the retail unit object does

    python3 tools/check_decl_order.py --unit Kyoto/Math/Carve80335B7C.c
      ok: 0 unit(s) checked, none emits its functions out of retail order
      (the tool counts 0 because the unit is a plain-C carve; order is confirmed directly
       by build/G2ME01/asm/Kyoto/Math/Carve80335B7C.s, which puts fn_80335B7C at .text+0x0,
       the only function there is.)

    ./tools/flip_test.sh Kyoto/Math/Carve80335B7C.c
      PASS  -> kept as Matching
      kept: 1 / 1   failed: 0   skipped: 0

    ./tools/goal_check.sh build/goal/item.json
      goal_check: PASS carve-80335b7c
      ok  no judge-owned path touched
      ok  gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
      ok  counts: matched 13602 -> 13603   linked 6650 -> 6651
      ok  check_symbol_names.py
      ok  All:  37.69% fuzzy, 31.12% matched, 13.99% linked (13603 / 28465 functions)
      ok  flip_test Kyoto/Math/Carve80335B7C.c: PASS, Object(Matching) in configure.py

`build/report.json` is the source of truth for the counts above.

## Notes a next run can use

- **The function is virtual, which the item does not say.** `auto_07_803BBB68_data.s` lists
  `fn_80335B7C` as a vtable entry of `lbl_803BBB68` (`symbols.txt:18732`, `size:0x1F0`) at
  line 47 and again at line 199, and `auto_07_803BB288_data.s` repeats it at lines 229, 422
  and 509 - four vtables in total. It is reached through a slot, never called by name, so
  there is no caller to read a signature off and the two instructions are the whole of the
  evidence.
- **The whole 0x80335B5C..0x80337198 auto range is one anonymous class.** The vtable that
  holds this slot also holds, consecutively, `fn_80335B38`, `fn_80335B40`, `fn_80335B48`,
  `fn_80335B50`, `fn_803371F4`, `fn_80335B58`, `fn_80335B5C`, `fn_80335B64`, `fn_80335B6C`,
  and eleven `fn_803377xx`/`fn_803376xx` accessors. The `0x80335Bxx` ones are the accessors
  that have already been carved (`Carve80335B38.c`, `Carve80335B48.c`, `Carve80335B58.c` and
  this one); the rest of the range is unclaimed. That is why the offsets recur across the
  carve: `fn_80335B50` (`stw r4, 0x1c(r3)`) and `fn_80335B7C` (`lwz r3, 0x1c(r3)`) look like
  a setter/getter pair for one word, which is the only coherent reading of the evidence.
  A future run that carves more of this range should keep using one local struct per unit
  until the owning class is identified.
- **The struct is declared by offset, deliberately.** Retail names no class for the slot and
  nothing in `include/` puts an int at +0x1C for the class the vtable belongs to, so the word
  lives in a local `SCarve80335B7C` whose only claim is the offset the instruction names -
  the same treatment `src/Kyoto/Math/Carve80335B48.c` gives the pair below it and
  `src/MetroidPrime/Tweaks/Carve80216D2C.c` gives `CTweakGame`. Nothing between +0x0 and
  +0x1B is asserted and nothing reads it. Replacing it with the real header later needs no
  change to the body.
- **The twin's C is not the twin's class.** `fn_80335B7C` shares bytes with a `CWorld`
  getter, but `CWorld`'s `mAreas.size()` is a call that retail inlined to a member load at
  +0x1C by a different route; writing the body against `CWorld` would have produced the right
  two instructions by accident and asserted a wrong layout everywhere else.
- **No `TARGET_PC` arm is needed and none was written.** `grep` finds the symbol only in
  `config/G2ME01/symbols.txt`, and the only tables that reference it are the unclaimed
  `auto_07_*` `.data` objects that retail's DOL supplies and the host link does not carry.
- **This claim is not gap-filling and does not need to be.** The claim starts at 0x80335B7C,
  0x20 bytes above where `Carve80335B58.c` ends, and ends 0x1614 bytes below where
  `Carve80337198.c` starts, so 0x80335B5C..0x80335B7C and 0x80335B84..0x80337198 stay
  unclaimed. That is fine: the carve vein's ordering trap is about entries being in address
  order, not about adjacency, and a claim never spans an unclaimed gap.
- **Still unclaimed in that auto range, if a next item wants it:** `fn_80335B5C`
  (`stw r4, 0x68(r3)`), `fn_80335B64` (`stw r4, 0x3c(r3)`), `fn_80335B6C` (`stw r4, 0x40(r3)`),
  `fn_80335B74` (`lfs f1, 0x4(r3)`) at 0x80335B5C..0x80335B7C, plus the whole 0x163C-byte
  run 0x80335B84..0x80337198 which holds `fn_80335B84` (a 0x30C-byte function) and is not
  trivial. The four accessors above are byte-shape twins of ordinary setters/getters and
  should carve as readily as this one did.
