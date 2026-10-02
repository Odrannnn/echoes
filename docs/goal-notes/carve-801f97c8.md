# carve-801f97c8 - `MetroidPrime/ScriptObjects/Carve801F97C8` is `Matching` and flipped

**Result: PASS.** `./tools/goal_check.sh build/goal/item.json` exits 0, last line
`goal_check: PASS carve-801f97c8`:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12577 -> 12580   linked 5940 -> 5943
  ok    check_symbol_names.py
  ok    All:  35.44% fuzzy, 29.25% matched, 12.97% linked (12580 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptObjects/Carve801F97C8.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-801f97c8
```

**+3 matched, +3 linked, `total_functions` still 28465.** `build/report.json` has
`main/MetroidPrime/ScriptObjects/Carve801F97C8` at `fuzzy_match_percent 100.0`,
`matched_functions 3 / total_functions 3`, `matched_code 128 / 128`, `complete_units 1`.

No `WALL:` - every function in the range matched on the first spelling tried. No `STALE:` - the
range was unclaimed at the start of this run (`symbols.txt:8190-8192` are `fn_` placeholders and
`config/G2ME01/splits.txt` had no entry for it).

## What the carve is

`.text 0x801F97C8..0x801F9848`, `0x80` = 128 bytes, **3 functions**, out of dtk's
`auto_03_801F9190_text` (`build/G2ME01/asm/auto_03_801F9190_text.s:461-505`):

```
fn_801F97C8  0x801F97C8  0x38   14 instructions
fn_801F9800  0x801F9800  0x20    8 instructions
fn_801F9820  0x801F9820  0x28   10 instructions
```

**A `rstl::vector<T>::push_back_unsafe` and the two halves of the `rstl::construct` it forwards
to.** `fn_801F97C8` loads the count at `+0x4` and the item pointer at `+0xC` of its `r3`, increments
the count, stores the new count, and calls `fn_801F9800` with `items + oldCount * 0x40`;
`fn_801F9800` forwards both arguments in a plain prologue/call/epilogue frame; `fn_801F9820` is
`if (dst != 0) fn_801F9848(dst, src)`.

**All three are byte-shape twins of functions retail names and this tree has matched**, and the twin
table is measured, not inferred from the sizes: disassembling both ranges out of
`build/G2ME01/main.elf` word by word leaves **exactly one differing word per pair, and it is the
`bl`** (masking it leaves no differing word at all, and the two ranges are the same length):

| this copy | twin | bytes | differing words | what it is |
| --- | --- | --- | --- | --- |
| `fn_801F97C8` | `push_back_unsafe__Q24rstl50vector<13CInt32POINode,Q24rstl17rmemory_allocator>FRC13CInt32POINode` 0x80299E64 (`src/Kyoto/Animation/CSequenceHelper.cpp`) | 0x38 / 0x38 | 1, `bl` at index 9 | `vector<T>::push_back_unsafe` |
| `fn_801F9800` | `__sys_free` 0x80008A28 (`src/MetroidPrime/main.cpp:396`) | 0x20 / 0x20 | 1, `bl` at index 3 | argument forwarder |
| `fn_801F9820` | `fn_80004D5C` 0x80004D5C (`src/MetroidPrime/Player/CGameStateBlockConstruct.cpp`) | 0x28 / 0x28 | 1, `bl` at index 5 | `rstl::construct`: null test + copy ctor call |

The bodies are therefore the twins' bodies with this copy's callee:

```c
void fn_801F9820(void* dst, const void* src) { if (dst != 0) fn_801F9848(dst, src); }
void fn_801F9800(void* dst, const void* src) { fn_801F9820(dst, src); }
void fn_801F97C8(struct SCarve801F97C8Vector* self, const void* src) {
  fn_801F9800(self->items + self->count++, src);
}
```

`fn_801F9848` (0x801F9848, `symbols.txt:8193`, size 0x88) is the element's copy constructor - the
`construct` target - and it is outside the item's range, so it is declared `extern` and dtk's own
`auto_03_801F9848_text.o` supplies it in the DOL. That is not a guess: `powerpc-eabi-nm
build/G2ME01/obj/auto_03_801F9848_text.o` lists `T fn_801F9848`, and the DOL flips.

The vector layout and the element stride are measured from the neighbours, not from the `slwi`:
`SetupColliders__20CCameraColliderGroupFfffif` (`symbols.txt:8189`, 0x801F9628, size 0x1A0, ending
exactly where this claim begins) calls this function with `r3 = itself + 0x4, r4 = &element`
(0x801F9728..0x801F9730) and then runs the inlined destructor on the temporary
(`addic. r0,r1,0x20 / beq / stw lbl_803B6564,0x20(r1)`); `fn_801FA1CC` (0x801FA1CC, size 0xC0), the
same vector's `reserve`, reads count at `+0x4`, capacity at `+0x8` and items at `+0xC`, and destroys
elements through the vtable with `r4 = -1` (0x801FA244..0x801FA258). So `+0x4`/`+0x8`/`+0xC` are the
`rstl::vector` fields and `T` is a 0x40-byte polymorphic class.

## Files (the carve is four, plus one port file the gate forced)

| file | change |
| --- | --- |
| `src/MetroidPrime/ScriptObjects/Carve801F97C8.c` | new; three definitions **descending by address**, plain C, header comment in the `Carve8038A7DC.c` style |
| `configure.py:739` | `Object(Matching, "MetroidPrime/ScriptObjects/Carve801F97C8.c"),` - one line, between `CUnknown90.cpp` and `Carve801FDB5C.c` |
| `config/G2ME01/splits.txt:1162-1163` | `MetroidPrime/ScriptObjects/Carve801F97C8.c:` / `\t.text       start:0x801F97C8 end:0x801F9848` |
| `files.cmake:561` | `src/MetroidPrime/ScriptObjects/Carve801F97C8.c` in the carve block, between `Carve801F7AC8.c` and `ScriptObjects/Carve801FDB5C.c` |
| `src/MetroidPrime/PortLinkStubs.cpp` | header counts `158 -> 159` supplied and `154 -> 155` functions, plus `stub_185` for `fn_801F9848` |

`stub_185` is the gate's own requirement, and it is the same trade `stub_178`/`stub_183`/`stub_184`
record. With the carve listed and no stub, `goal_check` fails as
`FAIL gate.sh / GATE FAIL: probe link-gap`, and `build/gate-link.log` says
`gap grew: fn_801F9848 is not in port_link_gap_list.md` with `287 MISSING` - the port has no
`auto_*` objects, and `fn_801F9820`'s body *is* that `bl`. With the stub the probe prints
`LINKED (291 undefined, 0 duplicates)`, i.e. the undefined count is back where it started, and the
host link is not a duplicate. The stub is an empty body and is not a claim that `fn_801F9848` is
decompiled; `docs/research/port_link_gap_list.md` is untouched.

**`docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` were rewritten by the judge's own gate run**
(`MP_GATE_DOCS_WRITE=1` in `goal_check.sh`, probe 773 -> 774) and reverted here with
`git checkout --`, per the item rules: the driver discards edits to those two files and re-derives
the counts itself.

## Verification (every number measured in this run)

```
./tools/goal_check.sh build/goal/item.json
  -> PASS (the block at the top of this note)

sha1sum build/G2ME01/main.dol
  -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010

tail -1 build/gate-probe.log
  -> probe: 774 files, 0 failed, 0 errors; link: LINKED (291 undefined, 0 duplicates)

./tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801F97C8.c
  -> TEST MetroidPrime/ScriptObjects/Carve801F97C8.c
     PASS  -> kept as Matching
     kept: 1 / 1   failed: 0   skipped: 0

./tools/unit_fit.sh MetroidPrime/ScriptObjects/Carve801F97C8.c
  -> .text claimed 128  ours 128  retail 128  fits
     no extra functions: our object defines only what the retail unit object does

python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/Carve801F97C8
  -> ok: 1 unit(s) checked, none emits its functions out of retail order

./tools/carve_diff.sh 801F97C8 80 build/G2ME01/src/MetroidPrime/ScriptObjects/Carve801F97C8.o
  -> retail: 32 instructions, 128 bytes / ours: 32 instructions, 128 bytes
     differing instructions: 3, all three `bl`s (indices 9, 17, 27) - 0x801F9800, 0x801F9820 and
     the extern 0x801F9848 - which is what an unlinked object against a linked ELF must show.
     flip_test is the decisive check that they resolve to retail's addresses.
```

## What is still there, for the next lane

- **`fn_801F9848` (0x801F9848..0x801F98D0, 0x88 = 34 instructions) is the natural next carve** from
  the same directory: it is the element's copy constructor, its `.text` is 34 straight-line
  instructions with **no `bl` at all** (so nothing new for `PortLinkStubs.cpp`), and this unit is
  the only thing that calls it. It is *not* a twin-copy job like this one - the body is an
  interleaved `lfs`/`stfs` copy plus a `lwz`/`stw` at `+0x38` and a vtable store, so it is a
  spelling job; no spelling was measured in this run, which is why it is not a `NEW:` line.
- The rest of the auto range is `CCameraColliderGroup` (fn_801F98D0 0x40, fn_801F9910 0x170,
  `UpdateColliders` 0x4E4, fn_801F9F64 0x198, `__ct__20CCameraColliderGroupFv` 0x74, fn_801FA170
  0x5C, fn_801FA1CC 0xC0, fn_801FA28C 0x68, ...). The class itself is still unnamed: the vtable is
  `lbl_803B6564` (`.data:0x803B6564`, size 0xC, `symbols.txt:18271`), as is the second one at
  `lbl_803B6570` (0x10).
- `SetupColliders__20CCameraColliderGroupFfffif` (0x801F9628..0x801F97C8) is the only caller of
  this unit and is itself unclaimed.

## Caveats

- The claim is exactly `0x801F97C8..0x801F9848` and nothing else; the two auto ranges that remain
  on either side are dtk's (`auto_03_801F9190_text` now ends at 0x801F97C8).
- The `.c` extension is required, not cosmetic: the three definitions must stay C so the `fn_`
  placeholders do not mangle (same reason as `Carve8038A7DC.c`).
- The names in the source (`SCarve801F97C8Vector`, `items`, `count`, `capacity`) are this file's
  spelling of offsets the bytes fix; nothing in the unit reads a field of `T`, only its size.
