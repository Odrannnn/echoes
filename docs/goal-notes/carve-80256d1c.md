# carve-80256d1c - `WorldFormat/Carve80256D1C.c`, 2 functions, `Matching`

## Result

`./tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS carve-80256d1c`** (exit 0), all
six of its checks green:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13468 -> 13470   linked 6516 -> 6518
  ok    check_symbol_names.py
  ok    All:  37.54% fuzzy, 30.98% matched, 13.84% linked (13470 / 28465 functions)
  ok    flip_test WorldFormat/Carve80256D1C.c: PASS, Object(Matching) in configure.py
```

The unit flips: `build/report.json` has `main/WorldFormat/Carve80256D1C` at
`matched_functions 2 / total_functions 2`, `fuzzy_match_percent 100.0`, `metadata.complete` true,
`fn_80256D3C` (40 B) at offset 32 and `fn_80256D1C` (32 B) at offset 0.

## What I did

The claim the driver asked for, written in C from retail's own bytes. The four carve files are all
in the change:

| file | edit |
| --- | --- |
| `src/WorldFormat/Carve80256D1C.c` | new, 0x48 = 72 bytes, `fn_80256D3C` then `fn_80256D1C` (descending) |
| `configure.py:432` | `Object(Matching, "WorldFormat/Carve80256D1C.c")`, one line, after `Carve80255A0C.c` |
| `config/G2ME01/splits.txt:1966-1967` | `WorldFormat/Carve80256D1C.c:` / `.text start:0x80256D1C end:0x80256D64`, between `Carve80255A0C.c` (ends 0x80255B28) and `CCollisionPrimitiveData.cpp` (0x80257A14) |
| `files.cmake:849` | `src/WorldFormat/Carve80256D1C.c`, after `src/WorldFormat/Carve80255A0C.c` |
| `src/MetroidPrime/PortLinkStubs.cpp` (tail, +21) | announced stand-in `stub_carve80256d1c_0` for the unclaimed callee `fn_80256D64` |

Both bodies are one call each. `fn_80256D3C` is `if (self != 0) { fn_80256D64(self, src); }` - the
`cmplwi r3,0` lands *before* the `stw r0,0x14(r1)` in retail and in the twin, and that order is
reproduced by this spelling. `fn_80256D1C` is `fn_80256D3C(self, src);` and nothing else.

## What I measured

- **Retail bytes**: `build/binutils/powerpc-eabi-objdump -d --start-address=0x80256D1C
  --stop-address=0x80256D64 build/G2ME01/main.elf` - 18 instructions, 72 bytes; `fn_80256D1C` 0x20 =
  8 instructions, `fn_80256D3C` 0x28 = 10. Sizes agree with `config/G2ME01/symbols.txt:10528-10529`.
- **Twins are exact**, as the brief said: `fn_80256D1C` is `fn_80004438`
  (`src/MetroidPrime/Carve80004438.c`, `Matching`) word for word with a different `bl`; `fn_80256D3C`
  is `fn_80004D5C` (`src/MetroidPrime/Player/CGameStateBlockConstruct.cpp`, `Matching`) word for word
  with `bl fn_80004AA0` in place of `bl fn_80256D64`.
- **`(dst, src)` is measured from all three call sites**, not assumed: `grep -n 'bl fn_80256D1C'
  build/G2ME01/asm/` gives 0x80256CE4 (`fn_80256CB4`), 0x80256E94 (`fn_80256E70`), 0x80257008
  (`fn_80256FD8`), and each loads two registers and `addi`s both by 0x20 per iteration. That is why
  `src` is in the signature of `fn_80256D1C` even though retail spends no instruction forwarding it -
  mwcceppc never emits `mr r4,r4`.
- **The 0x20 stride is retail's**: `fn_80256C30` (0x80256C30) allocates `slwi r3,r0,5` (count * 32)
  and calls `fn_80256CB4` to fill the array.
- **`fn_80256D64`** (0x80256D64, 0x4C, `symbols.txt:10530`) is above the claim, unclaimed, and is
  19 instructions copying a 0x1E = 30-byte aggregate (six `float`s at +0x00..+0x14, three `short`s
  at +0x18/+0x1A/+0x1C) from r4 to r3, ending in `blr` with no frame. Its class is not guessed -
  retail names it nothing, so `void*` is only what the bytes read.
- **`tools/carve_diff.sh 0x80256D1C 0x48 build/G2ME01/src/WorldFormat/Carve80256D1C.o`**: 18/18
  instructions, 72/72 bytes, and the only two reported differences are the `bl` targets, which are
  relocations in an unlinked object (`bl c <fn_80256D1C+0xc>`, `bl 34 <fn_80256D3C+0x14>`).
  `tools/unit_fit.sh WorldFormat/Carve80256D1C.c`: `claimed 72 / ours 72 / retail 72 / fits` and
  `no extra functions`.
- **Gates after the change**: `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`;
  tree `total_functions` unmoved at 28465; the two neighbouring `auto_*` objects report 37 and 21
  functions and the gate's per-function diff reads `SPLIT main/auto_03_80255B28_text: 23
  function(s) moved into main/WorldFormat/Carve80256D1C, main/auto_03_80256D64_text (exact count
  match - a split, not a loss)`, which is the expected shape for a carve out of an `auto_*` range.

## The one thing that had to be added, and why

The first `goal_check.sh` failed on exactly one check, and it was the expected one:

```
port link gap                   280  MISSING
link gap not accounted for:
  gap grew: fn_80256D64 is not in port_link_gap_list.md
```

`files.cmake` is the port link's source list, so listing the new unit made the port ask for the
callee, which no unit defines. The fix is the trade the tree already makes for this case: an
**announced empty-body stand-in**, `stub_carve80256d1c_0() asm("fn_80256D64")`, appended to
`src/MetroidPrime/PortLinkStubs.cpp` with a comment saying what retail's 19 instructions do and
that it claims nothing about 0x80256D64 being decompiled. It is named for the unit that asks for
it rather than `stub_NNN`, following `stub_carve8000447c_0` and `stub_80004438_0`, because a
numbered name is what another lane's carve takes between the judge and the rebase. Measured after:
`python3 tools/link_gap.py --rebuild` -> `279 MISSING symbol(s), all accounted for in
port_link_gap_list.md`, which is the count the gate's baseline carries.

## Notes for the next lane

- `tools/check_decl_order.py --unit WorldFormat/Carve80256D1C.c` printed `0 unit(s) checked, none
  emits its functions out of retail order` - it did not pick this unit up. The order is right
  anyway and it is checked three ways here: the file declares `fn_80256D3C` (0x80256D3C) before
  `fn_80256D1C` (0x80256D1C), objdiff places them at offsets 32 and 0 respectively, and
  `flip_test.sh` PASSed with `main.dol` byte-exact. Worth knowing that the tool reports 0 rather
  than 1 for a `.c` carve.
- `goal_check.sh` runs `gate.sh` with `MP_GATE_DOCS_WRITE=1`, so it rewrote `docs/HANDOFF.md` and
  `docs/RUNNING_THE_DECOMP.md` itself. That is the judge's own derived-count rewrite, not an edit of
  mine, and the driver discards those files before judging.