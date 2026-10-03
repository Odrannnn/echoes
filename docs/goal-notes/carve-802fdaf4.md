# carve-802fdaf4

**DONE.** `rstl/Carve802FDAF4` is a new `Matching` DOL unit, **2 / 2 functions at 100.00%**, held by
`tools/flip_test.sh`. `tools/goal_check.sh build/goal/item.json` → `PASS carve-802fdaf4`.

## What this is

Two anonymous functions in the gap `rstl/rstl_misc.cpp` (ends 0x802FDAF4) .. `rstl/rstl_strings.cpp`
(starts 0x802FDD90), claimed as their own unit.

| symbol | addr | size | twin |
| --- | --- | --- | --- |
| `fn_802FDAF4` | 0x802FDAF4 | 0x40 (64 B) | `internal_dereference__Q24rstl66basic_string<w,...>` (`src/rstl/rstl_strings.cpp:150`, asm `rstl/rstl_strings.s:133` @ 0x802FDF38) |
| `fn_802FDB34` | 0x802FDB34 | 0x60 (96 B) | `internal_allocate__Q24rstl66basic_string<w,...>` (`src/rstl/rstl_strings.cpp:141`, asm `rstl/rstl_strings.s:155` @ 0x802FDF78) |

They are the **`char`** instantiations of the two private members; retail did not name them
(`symbols.txt:13825-13826` carries only `fn_<addr>`). `rstl/rstl_strings.cpp` claims the named
`wchar_t` and `char` copies at 0x802FDD90..0x802FF3AC, so these two fall outside it.

### The one difference that mattered, and it is the whole `char` story

```
twin  0x802FDF94  slwi r3, r4, 1      ; sizeof(wchar_t) == 2
twin  0x802FDF98  addi r3, r3, 0x8
ours  0x802FDB50  addi r3, r31, 0x8   ; sizeof(char) == 1, multiply already folded
```

Everything else is instruction-for-instruction identical, including `fn_802FDAF4` (its only
difference is the `bl` target in the object, not in the text). So the source is the twins' source
with `char` substituted, the two words of `rstl::basic_string<char>::control` named from what the
bytes read (`+0` mCapacity, `+4` mRefCount — `include/rstl/string.hpp:110-113`), and
`sizeof(control)` left as the compiler's `sizeof`.

**Neither callee needed a name a C identifier cannot hold.** Both mangled names are pure
identifier characters: `Free__7CMemoryFPCv` (0x802CE388, unclaimed — dtk's `auto_*` object supplies
the bytes) and `allocate__Q24rstl17rmemory_allocatorFi` (0x802FDAB8, **claimed** by
`rstl/rstl_misc.cpp`, so the `bl` binds there). `<` / `>` appear only in *template* names, which is
what blocks `Carve80281310.c`'s two functions — a difference worth naming, because it is why this
item was writable and that one was not.

`rmemory_allocator::allocate(int)` is `static` (`include/rstl/rmemory_allocator.hpp:16`), so
`addi r3,r31,0x8 / bl` passes the byte count in `r3` with no receiver — that is what makes the
declared prototype `extern void* allocate__Q24rstl17rmemory_allocatorFi(int size);` correct.

## Claim boundary

`0x802FDAF4..0x802FDB94` exactly, as seeded. **`fn_802FDB94` (0x802FDB94, 0xB8) is left to retail**
and is the reason: it is the case-insensitive scan (`fn_8016BED0`'s twin — two ranges on the
sign-extended byte, folded with `subi 0x20` / `subi 0x60`) and it reads `lbl_80419AEC@sda21`, a
`.sdata2` datum. Its caller `fn_802FDC4C` (0x802FDC4C, 0x144) follows and is also unclaimed. Those
are separate carve items and nothing above needs them.

**No link-order cycle.** This claim starts exactly where `rstl/rstl_misc.cpp`'s `.text` ends, which
is the documented trigger for `dtk dol split`'s `Cyclic dependency` — it does not happen here
(`flip_test.sh` re-split and linked). So proximity to a pre-existing `Matching` unit's end is a risk,
not a certainty; here the neighbouring unit is a 0x3C-byte `static` helper and the carve linked.

## Files (four, as the vein requires)

- `src/rstl/Carve802FDAF4.c` — new, 90 lines of header comment + the two bodies. **Descending by
  address** (`fn_802FDB34` then `fn_802FDAF4`).
- `config/G2ME01/splits.txt:2797` — `rstl/Carve802FDAF4.c: .text start:0x802FDAF4 end:0x802FDB94`,
  between `rstl_misc.cpp` and `rstl_strings.cpp` (address order).
- `configure.py:1157` — `Object(Matching, "rstl/Carve802FDAF4.c")`, one line, after
  `Object(MatchingFor("G2ME01"), "rstl/rstl_misc.cpp")`.
- `files.cmake:1260` — `src/rstl/Carve802FDAF4.c`, after `src/rstl/rstl_misc.cpp`.

`src/MetroidPrime/PortLinkStubs.cpp` needed no edit: neither `fn_802FDAF4` nor `fn_802FDB34` was in it
(grep over `src/` and `include/` returns nothing), and the gate's `port link dups` step is clean at
287 undefined / 0 duplicates.

## Measured

```
./tools/decomp_build.sh main/rstl/Carve802FDAF4
  All:  37.57% fuzzy, 31.01% matched, 13.88% linked (13501 / 28465 functions)
  main/rstl/Carve802FDAF4: 100.00% fuzzy, 100.00% matched (2 / 2 functions)

./tools/unit_fit.sh rstl/Carve802FDAF4.c
   .text   claimed 160   ours 160   retail 160   fits
   no extra functions: our object defines only what the retail unit object does

python3 tools/check_decl_order.py --unit rstl/Carve802FDAF4.c
  ok: 0 unit(s) checked, none emits its functions out of retail order

./tools/flip_test.sh rstl/Carve802FDAF4.c
  PASS  -> kept as Matching      kept: 1 / 1   failed: 0   skipped: 0

./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13499 -> 13501   linked 6547 -> 6549
  ok    All:  37.57% fuzzy, 31.01% matched, 13.88% linked (13501 / 28465 functions)
  ok    flip_test rstl/Carve802FDAF4.c: PASS, Object(Matching) in configure.py
  goal_check: PASS carve-802fdaf4
```

`build/report.json`, unit `main/rstl/Carve802FDAF4`: `total_functions 2`, `matched_functions 2`,
`matched_code_percent 100.0`, `complete_units 1`, `complete_code_percent 100.0`. The unit's `.text` is
160 bytes and the claim is 160 bytes, so **no new `auto_*` gap and no `PortLinkStubs.cpp` growth**.
`total_functions` is still 28465.

**One spelling was tried and it worked** — worth recording so nobody re-derives it: writing the
bodies as the twins' source with the fields named (`self->mCow && --self->mCow->mRefCount == 0` /
`self->mCow = allocate(8 + size); self->mPtr = self->mCow + 1; self->mCow->mCapacity = size;
self->mCow->mRefCount = 1;`) matched on the first build. The store sites matter: the twins **reload**
`mCow` from the receiver for each store (`lwz r3,0x4(r30)` before every one), so caching the pointer
in a local would not have produced the same text. Mirroring the twin's `self->mCow->` spelling
rather than introducing a temporary is what got it.

`tools/carve_diff.sh 802FDAF4 A0 build/G2ME01/src/rstl/Carve802FDAF4.o` reports 40 instructions and
160 bytes on **both** sides with 2 differing instructions — the two `bl`s, which are relocations in
a relocatable object and resolve to `Free__7CMemoryFPCv` and
`allocate__Q24rstl17rmemory_allocatorFi` in the link. That script's "NOT byte-exact" line is expected
for any carve with calls; `flip_test.sh` is the verdict and it passed.

## Follow-ups

- `fn_802FDB94` (0x802FDB94, 0xB8) and `fn_802FDC4C` (0x802FDC4C, 0x144) remain unclaimed in
  `auto_03_802FDAF4_text`. `fn_802FDB94` is the twin of the matched `case_insensitive_char_traits`
  lower/fold at `include/rstl/string.hpp:84-92`; its only obstacle measured here is the
  `lbl_80419AEC@sda21` read. Left for the seeder, not filed as `NEW:` — a `.sdata2` symbol plus a
  0xB8 body is a real job but this run did not measure it, and the queue already has carve items.