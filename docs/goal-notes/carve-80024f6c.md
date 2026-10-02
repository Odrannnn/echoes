# carve-80024f6c - `MetroidPrime/Carve80024F6C` is `Matching` and flipped

**Result: PASS.** `./tools/goal_check.sh build/goal/item.json` exits 0, last line
`goal_check: PASS carve-80024f6c`:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12664 -> 12666   linked 6023 -> 6025
  ok    check_symbol_names.py
  ok    All:  35.52% fuzzy, 29.37% matched, 13.04% linked (12666 / 28465 functions)
  ok    flip_test MetroidPrime/Carve80024F6C.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-80024f6c
```

`total_functions` is still **28465** after the `splits.txt` edit, and `build/report.json` has the
unit at `main/MetroidPrime/Carve80024F6C`: `fuzzy_match_percent 100.0`, `matched_functions 2 /
total_functions 2`, `matched_code 68 / total_code 68`, `complete_units 1 / total_units 1`, `.text`
virtual_address 2147635052 (= 0x80024F6C).

## The carve

`.text 0x80024F6C..0x80024FB0`, `0x44` = 68 bytes, **2 functions**, carved out of dtk's
`auto_03_80024C18_text` (`build/G2ME01/asm/auto_03_80024C18_text.s:276-300`; `symbols.txt:646-648`):

```
fn_80024F6C  0x80024F6C  0x20   8 instructions
fn_80024F8C  0x80024F8C  0x24   9 instructions
```

**What they are: `rstl::destroy< T >` / `rstl::destroy_impl< T >` for `T =
CTextRenderBuffer::SFontPalette`.** The two unclaimed functions at the lower addresses are the
measurement, not a guess:

- `fn_80024F0C` (0x80024F0C, 0x60, `asm/auto_03_80024C18_text.s:246-274`) counts from the word at
  `+0x00`, walks from `+0x04` in **strides of 0x2c** and calls `fn_80024F6C` on each element -
  `rstl::reserved_vector<SFontPalette, 64>::destroy_elements()`
  (`include/rstl/reserved_vector.hpp:97-104`). The 0x2c stride names the element: it is
  `NESTED_CHECK_SIZEOF(CTextRenderBuffer, SFontPalette, 0x2c)`
  (`include/Kyoto/Text/CTextRenderBuffer.hpp:101`), and `CTextRenderBuffer::mPalettes` is
  `rstl::reserved_vector< SFontPalette, 64 >` (same header, line 93).
- `fn_80024EBC` (0x80024EBC, 0x50, lines 221-244) is that vector's deleting destructor - receiver
  guard, the call to `fn_80024F0C`, the `extsh.` flag test, `Free__7CMemoryFPCv(self)`.

Both twins, disassembled beside retail's range as the item said (identical apart from the two
address-relative `bl` displacements):

| function | twin | what it is |
| --- | --- | --- |
| `fn_80024F6C` | `fn_80004D3C` 0x80004D3C (`src/MetroidPrime/Player/Carve80004C4C.c`, `Matching`) | frame + one `bl fn_80024F8C`, nothing else |
| `fn_80024F8C` | `fn_80004C6C` 0x80004C6C (same file) | frame + `li r4,-1` + `bl __dt__Q217CTextRenderBuffer12SFontPaletteFv` |

`li r4,-1` is MWCC's "destroy, do not free me afterwards" flag, which is why the callee is the
*deleting* destructor form; retail's own `extsh. r0,r31` at 0x80025050 truncates it to 16 bits, so
the declaration is `(void*, short)` - `li r4,-1` is one instruction for `short` and for `int`, so
this choice moves no byte (the twin's note records the same measurement).

## The four files

| file | change |
| --- | --- |
| `src/MetroidPrime/Carve80024F6C.c` | new, 99 lines: header comment in the `src/Dolphin/Carve8038A7DC.c` style, one `extern`, the two definitions **descending** (`fn_80024F8C` first) |
| `configure.py:664` | `Object(Matching, "MetroidPrime/Carve80024F6C.c")`, one line, after `Carve80024B70.c` (address order) |
| `config/G2ME01/splits.txt:113-114` | `MetroidPrime/Carve80024F6C.c:` / `.text start:0x80024F6C end:0x80024FB0`, after `Carve80024B70.c` |
| `files.cmake:414` | `src/MetroidPrime/Carve80024F6C.c`, after `src/MetroidPrime/Carve80024B70.c` |

The claim is exactly the item's range: the callee
`__dt__Q217CTextRenderBuffer12SFontPaletteFv` (`symbols.txt:648`, 0x80024FB0, 0xCC) is a real-named
function, so it needs its own `.cpp` and is **not** part of this claim, and it is exactly where the
claim stops. `src/MetroidPrime/PortLinkStubs.cpp` defines neither symbol (checked), so no
duplicate-hand deletion was needed.

## The fifth file, and why it is not optional

`src/Kyoto/Alloc/PortMwccNew.cpp` gains an `extern "C"` definition of
`__dt__Q217CTextRenderBuffer12SFontPaletteFv` (plus the `Kyoto/Text/CTextRenderBuffer.hpp`
include), beside the `__dt__6CTokenFv` shim that commit `744bc7de` added for the neighbouring
carve. That file is in `files.cmake` but **not** in `configure.py`, so mwcceppc never sees it and it
cannot affect `main.dol`. **The port's undefined count is what forces it, and that is measured, not
assumed**: with the definition deleted and nothing else changed,
`MP_TOOLCHAIN=.../MetroidPrimePort/build/review-tools ./tools/link_check.sh` printed

```
link_check: unique undefined symbols 292
link_check: duplicate definitions   0
```

and `nm build-port-link/CMakeFiles/mp_game.dir/src/MetroidPrime/Carve80024F6C.c.o` showed
`U __dt__Q217CTextRenderBuffer12SFontPaletteFv` with no definition anywhere. Restored, the same
command printed

```
link_check: unique undefined symbols 291
link_check: duplicate definitions   0
link_check: unchanged from baseline (291 undefined, 0 duplicates)
```

The shim calls the real `CTextRenderBuffer::SFontPalette::~SFontPalette()`, so the four
`rstl::auto_ptr< CGraphicsPalette >` members are really released; `CGraphicsPalette`'s own
destructor is already defined for the host by `src/Kyoto/Graphics/CGraphicsPalettePortStub.cpp`, so
no further undefined symbol appears. Note the same count in `build/gate-probe.log`:
`probe: 800 files, 0 failed, 0 errors; link: LINKED (291 undefined, 0 duplicates)`.

## Everything measured

```
./tools/decomp_build.sh MetroidPrime/Carve80024F6C
  main/MetroidPrime/Carve80024F6C: 100.00% fuzzy, 100.00% matched (2 / 2 functions)
./tools/flip_test.sh MetroidPrime/Carve80024F6C.c
  PASS  -> kept as Matching          kept: 1 / 1   failed: 0   skipped: 0
./tools/unit_fit.sh MetroidPrime/Carve80024F6C.c
  .text  claimed 68  ours 68  retail 68  fits;  no extra functions
python3 tools/check_decl_order.py --unit MetroidPrime/Carve80024F6C
  ok: 1 unit(s) checked, none emits its functions out of retail order
./tools/carve_diff.sh 80024F6C 44 build/G2ME01/src/MetroidPrime/Carve80024F6C.c.o
  retail 17 instructions / 68 bytes, ours 17 / 68; the two "differing" instructions are the two
  `bl` displacements, which are relocations in the relocatable object
sha1sum build/G2ME01/main.dol
  6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
python3 tools/link_gap.py --rebuild
  15 c++ runtime, 43 libc/libm, 188 aurora source, 1 aurora header only, 285 MISSING - all
  accounted for in port_link_gap_list.md
```

## Two things the next lane should know

1. **`tools/check_decl_order.py --unit <name>` needs the path prefix.** `--unit
   Carve80024F6C` printed `ok: 0 unit(s) checked, none emits its functions out of retail order` -
   which reads as a pass and measures nothing. With `MetroidPrime/Carve80024F6C` it checked 1 unit.
2. **`tools/unit_fit.sh` takes the unit path, not the `src/` path**: `src/MetroidPrime/
   Carve80024F6C.c` answers "not declared in any splits.txt" and exits without checking anything.
3. **Commit `744bc7de` (`carve-80024b70`) claimed less than its item's range.** Its item named
   three functions and a range ending at 0x80024CD4; the commit's `splits.txt` entry and its
   source are `0x80024B70..0x80024C18` - one function. `fn_80024C18` (0x80024C18, 0x84) and
   `fn_80024C9C` (0x80024C9C, 0x38) are therefore **still unsourced**, and that item's own reason
   text carries their byte-shape twins (`__dt__Q24rstl79vector<Q218CWorldSaveGameInfo20
   SEnvironmentVariable,Q24rstl17rmemory_allocator>Fv` in `src/MetroidPrime/CMemoryCard.cpp`, and
   `destroy<rstl::pointer_iterator<CTweakValue, rstl::vector<CTweakValue,...>>>` in
   `src/MetroidPrime/main.cpp`). I did not touch them - that is a different range and a different
   lane's item - but the driver may want them queued, and note that claiming `fn_80024C18` alone
   would add an undefined symbol, so the range should run to 0x80024CD4 to take its callee
   `fn_80024CD4` with it.

## Follow-on

The unclaimed run immediately below this carve is self-contained: `0x80024EBC..0x80024F6C` =
`fn_80024EBC` (0x50, the `reserved_vector<SFontPalette, 64>` deleting destructor - receiver guard,
`bl fn_80024F0C`, `extsh.` flag test, `Free__7CMemoryFPCv(self)`) plus `fn_80024F0C` (0x60,
`destroy_elements()` - `i` from 0 against the count at `+0x00`, cursor from `+0x04`, stride 0x2c,
`cmpw`/`blt` so the loop is signed). The only callee either of them has is `fn_80024F6C`, which
this item's unit now defines, so the range introduces no undefined symbol of its own. **I did not
identify a byte-shape twin for either**, so I am not claiming one: the tree has no matched
`reserved_vector` deleting destructor to point at (`__dt__Q24rstl26reserved_vector<6CPlane,6>Fv`
and `__dt__Q24rstl49reserved_vector<...>Fv` appear only in comments in `CMainResetGameState.cpp:170`
and `Player/CGameState.cpp:912`, never as a definition). The next lane should mine one, the way
this item's twins were mined.

NEW: carve-80024ebc | match | MetroidPrime/Carve80024EBC | two more unsourced fn_ in auto_03_80024C18_text, self-contained 0x80024EBC..0x80024F6C, call graph mapped but no twin identified yet
---

# Second run (lane 4, `goal/lane-4` at `1e022368`) - same carve, re-applied. PASS.

**Why this item ran twice: the driver could not apply the judged commit, not a failed change.**
`build/goal/run.log:4952-4988` (this worktree's log) says it in full:

```
[2026-10-02 13:19:52Z] agent transcript: .../agent/carve-80024f6c-L4-10-20261002T130508.jsonl (272 lines)
goal_check: PASS carve-80024f6c
[2026-10-02 13:20:14Z] judge PASS carve-80024f6c - not reviewed (match items are outside MP_GOAL_REVIEW_KINDS='port progress')
[2026-10-02 13:20:14Z] goal/decomp moved (55d7c4c -> 1e02236) during carve-80024f6c - carrying the judged change onto it
[2026-10-02 13:20:28Z] carve-80024f6c does not apply on 1e02236 - releasing it for a fresh attempt
goal_queue: released carve-80024f6c
```

The base moved under the item (`carve-80024d24` landed on `goal/lane-4` as `1e022368` and the
rebase did not apply), so the release was mechanical. Nothing in the first run's analysis needed
re-testing; this run re-measured everything on `1e022368` and re-landed the same carve.

**Re-measured, not recalled.** On this base the neighbourhood is one unit larger than the first run
saw - `MetroidPrime/Carve80024D24.c` (0x80024D24..0x80024D68, commit `1e022368`) now sits below
this range - and `src/Kyoto/Alloc/PortMwccNew.cpp` already carries the `__dt__13CFontImageDefFv`
shim from that commit. Every other number in the first run's notes still holds, checked again here:

- `symbols.txt:644-648` - `fn_80024EBC` 0x80024EBC 0x50, `fn_80024F0C` 0x80024F0C 0x60,
  `fn_80024F6C` 0x80024F6C 0x20, `fn_80024F8C` 0x80024F8C 0x24,
  `__dt__Q217CTextRenderBuffer12SFontPaletteFv` 0x80024FB0 0xCC `scope:weak`.
- The instructions have moved **file**: the first run read them out of
  `build/G2ME01/asm/auto_03_80024C18_text.s:276-300`; after `carve-80024d24`'s split they are
  `auto_03_80024D68_text.s:169-192` (and the two unclaimed callers below, `fn_80024EBC`
  `:114-137`, `fn_80024F0C` `:139-167`). Same addresses, same bytes.
- `fn_80024F0C`'s stride is still `addi r31,r31,0x2c` and `CTextRenderBuffer.hpp:101` still pins
  `NESTED_CHECK_SIZEOF(CTextRenderBuffer, SFontPalette, 0x2c)`, with `mPalettes` a
  `rstl::reserved_vector< SFontPalette, 64 >` at line 93.
- `__dt__Q217CTextRenderBuffer12SFontPaletteFv` itself (0x80024FB0..0x8002507C, read again at
  `auto_03_80024D68_text.s:194-278`) checks the four `rstl::auto_ptr` members at `+0x24`, `+0x1c`,
  `+0x14`, `+0x0c` - `addic. r0,r30,off` / `beq` / `lbz r0,off(r30)` / `cmplwi r0,0` /
  `lwz r3,off+4(r30)` / `bl __dt__16CGraphicsPaletteFv` - which is exactly
  `rstl::auto_ptr`'s `{ bool mHas; T* mItem; }` (`include/rstl/auto_ptr.hpp:11-12`), and closes
  with `extsh. r0,r31` / `ble` / `Free__7CMemoryFPCv`. That is the work the port shim below does.
- `rstl::destroy`/`destroy_impl` are at `include/rstl/construct.hpp:92-95` / `85-90` and
  `reserved_vector::destroy_elements()` at `include/rstl/reserved_vector.hpp:97-105`.

## The four files, again, on `1e022368`

| file | change |
| --- | --- |
| `src/MetroidPrime/Carve80024F6C.c` | new, 98 lines, same body as the first run (header comment in the `src/Dolphin/Carve8038A7DC.c` style, one `extern`, the two definitions **descending**: `fn_80024F8C` then `fn_80024F6C`) |
| `configure.py:666` | `Object(Matching, "MetroidPrime/Carve80024F6C.c")`, one line, after `Carve80024D24.c` |
| `config/G2ME01/splits.txt:119-120` | `MetroidPrime/Carve80024F6C.c:` / `.text start:0x80024F6C end:0x80024FB0`, after `Carve80024D24.c` |
| `files.cmake:415` | `src/MetroidPrime/Carve80024F6C.c`, after `src/MetroidPrime/Carve80024D24.c` |

and the port-only fifth file, `src/Kyoto/Alloc/PortMwccNew.cpp:67-82` (+ the
`Kyoto/Text/CTextRenderBuffer.hpp` include on line 23): an `extern "C"` definition of
`__dt__Q217CTextRenderBuffer12SFontPaletteFv` forwarding to
`CTextRenderBuffer::SFontPalette::~SFontPalette()`, beside the `__dt__13CFontImageDefFv` shim.
`src/MetroidPrime/PortLinkStubs.cpp` still defines neither symbol, so no duplicate-hand deletion was
needed. `PortMwccNew.cpp` is in `files.cmake` and not in `configure.py`, so mwcceppc never sees it.

The claim is exactly the item's range: the callee at 0x80024FB0 is real-named and unclaimed, and
0x80024EBC..0x80024F6C below is still dtk's, so the two functions are the whole writable run.

## Everything measured on this tree

```
./tools/decomp_build.sh MetroidPrime/Carve80024F6C
  main/MetroidPrime/Carve80024F6C: 100.00% fuzzy, 100.00% matched (2 / 2 functions)
./tools/flip_test.sh MetroidPrime/Carve80024F6C.c
  PASS  -> kept as Matching          kept: 1 / 1   failed: 0   skipped: 0
./tools/unit_fit.sh MetroidPrime/Carve80024F6C.c
  .text  claimed 68  ours 68  retail 68  fits;  no extra functions
python3 tools/check_decl_order.py --unit MetroidPrime/Carve80024F6C
  ok: 1 unit(s) checked, none emits its functions out of retail order
./tools/carve_diff.sh 80024F6C 44 build/G2ME01/src/MetroidPrime/Carve80024F6C.o
  retail 17 instructions / 68 bytes, ours 17 / 68; differing instructions: 2 - the two `bl`
  displacements, which are relocations in the relocatable object
sha1sum build/G2ME01/main.dol
  6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
MP_TOOLCHAIN=$MP_TOOLCHAIN_DIR/build/review-tools ./tools/link_check.sh
  compile errors 0; unique undefined symbols 291; duplicate definitions 0
  link_check: unchanged from baseline (291 undefined, 0 duplicates)
build/gate-probe.log
  probe: 804 files, 0 failed, 0 errors; link: LINKED (291 undefined, 0 duplicates)
./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12672 -> 12674   linked 6031 -> 6033
  ok    check_symbol_names.py
  ok    All:  35.53% fuzzy, 29.37% matched, 13.05% linked (12674 / 28465 functions)
  ok    flip_test MetroidPrime/Carve80024F6C.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-80024f6c
```

`total_functions` is still **28465** (`report.json` `measures`), and the unit is
`main/MetroidPrime/Carve80024F6C`: `fuzzy_match_percent 100.0`, `matched_functions 2 / 2`,
`matched_code 68 / 68`, `complete_units 1 / 1`, `.text` `virtual_address 2147635052`
(= 0x80024F6C), `metadata.complete true`.

## Two corrections to the first run's write-up, and one trap for the next lane

1. **`tools/carve_diff.sh` wants `build/G2ME01/src/MetroidPrime/<unit>.o`, not
   `build/G2ME01/src/MetroidPrime/<unit>.c.o`.** The first run's command line is in the section
   above; it printed `ours: 0 instructions, 0 bytes` and `NOT byte-exact` and measures nothing. With
   the `.o` path it prints `retail 17 / 68, ours 17 / 68`.
2. `tools/flip_test.sh` leaves the unit's object at `build/G2ME01/src/MetroidPrime/<unit>.o` (no
   `.c`); `decomp_build.sh` writes the same path. Neither is at the `src/...c.o` path.
3. **`tools/gate.sh` rewrites `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` itself, and
   `git status` then shows both as modified with a ~191 000-line diff.** That is not an agent edit:
   the two files already contain the same line ("byte-identical to `orig/G2ME01/files/RelProd/`...",
   "probe 803 files 0 failures") **47 902 times each** at HEAD, and the gate's derived-count rewrite
   changes every copy (`probe 803 files` -> `probe 804 files`), so the whole file reads as changed.
   Measured on this tree: `git show HEAD:docs/HANDOFF.md | grep -c byte-identical` = 47902 =
   `grep -c` in the worktree file, and `cmp` reports the first difference at byte 315, line 10.
   Worth knowing before a lane panics at its own diff or "fixes" it.

Nothing is left open on this item, and the `NEW:` line the first run filed
(`carve-80024ebc`) still stands - `0x80024EBC..0x80024F6C` is still unsourced, and the twin search
this run did not repeat is the only thing missing for it. This run's `carve-80024f6c.c` header
comment names both of its callees' surroundings, which is most of what that item needs:
`fn_80024F0C`'s 0x2c stride, the signed `cmpw`/`blt` loop, and `fn_80024EBC`'s
`extsh.`-gated `Free__7CMemoryFPCv`.
