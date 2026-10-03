# carve-80335a0c — DONE

`kind: match`, target `Kyoto/Math/Carve80335A0C`. Carved `fn_80335A0C`
(`.text 0x80335A0C..0x80335A14`, 0x8 = 8 bytes, 1 function) out of dtk's unclaimed
`auto_03_80335A0C_text` as a new `Matching` unit.

## What the function is

```
80335a0c <fn_80335A0C>:
80335a0c:  c0 22 c9 28   lfs     f1,-14040(r2)   ; lfs f1, lbl_8041ECE8@sda21(r0)
80335a10:  4e 80 00 20   blr
```

It is the byte-shape twin of the matched `CParticleGen::GetGeneratorRate()` in
`src/MetroidPrime/CExplosion.cpp` (`.text 0x800534BC`: `lfs f1, lbl_8041A920@sda21(r0)` /
`blr`) - load one `.sdata2` float into the return register and return. The two differ only
in which literal they load.

## The one non-obvious thing: the constant is `extern`, and the port needs it defined

The twin can write `return 1.f;` because its literal is inside its own claim:
`splits.txt` gives `MetroidPrime/CExplosion.cpp` `.sdata2 start:0x8041A900 end:0x8041A928`,
and the compiler's R_PPC_EMB_SDA21 relocation at `GetGeneratorRate` names `lbl_8041A920`
inside it (`objdump -d -r build/G2ME01/obj/MetroidPrime/CExplosion.o`).

`lbl_8041ECE8` has **no owner**. Measured three ways:

- `config/G2ME01/symbols.txt:25423` types it `.sdata2:0x8041ECE8; size:0x4 data:float`.
- **No unit in `splits.txt` claims that address.** Scanned every section entry in
  `splits.txt`: the nearest claims are `Kyoto/Math/CMayaSpline.cpp`
  `.sdata2 0x8041EBC0..0x8041EC28` and `Kyoto/Animation/CSoundPOINode.cpp`
  `.sdata2 0x8041EE08..0x8041EE10`. 0x8041ECE8 sits in the 0x1E0-byte unclaimed gap
  between them.
- That gap is dtk's own object: `build/G2ME01/asm/auto_11_8041EC28_sdata2.s` covers
  `0x8041EC28..0x8041EE08`, and `powerpc-eabi-nm build/G2ME01/main.elf` reports
  `8041ec28 D lbl_8041EC28` — a data symbol, defined, from dtk's gap object.

So `return 0.f;` is wrong twice over: it gives this translation unit a `.sdata2` section
that no claim places, and it returns the wrong value. The source declares the word
`extern`, which reproduces retail's relocation exactly and claims nothing extra. Same
arrangement already in `Kyoto/Graphics/Carve802C2534.cpp` (`extern float lbl_8041E508;`,
also an unclaimed `auto_11_*` word).

The value, for the record, is 0.0f: `auto_11_8041EC28_sdata2.s:185` reads
`.obj lbl_8041ECE8, global` / `.float 0`, and `objdump -s -j .sdata2` gives `00000000`.
Its only other reader is `fn_803367F8` (`build/G2ME01/asm/auto_03_80335B5C_text.s`), which
loads it as the default before a switch - so `return 0.f;` would have meant the same
thing, had it been possible.

### The port-link consequence, which the first build failed on

Declaring the word `extern` in a game source adds an **undefined symbol to the port build**,
and the `port link gap` step of `tools/gate.sh` fails on a gap that grew and is not in
`docs/research/port_link_gap_list.md`:

```
link gap not accounted for:
  gap grew: lbl_8041ECE8 is not in port_link_gap_list.md
```

Fixed the way `lbl_8041A920` and `lbl_8041C500..508` are handled: **define it on the port
side only**, in `src/MetroidPrime/PortGlobals.cpp` next to `lbl_8041A920`, as
`extern "C" const float lbl_8041ECE8 = 0.0f;`. `const` on the definition and non-`const`
on the reader is deliberate and is recorded at the entry: with a `const` *declaration*
mwcceppc materialises the value inline instead of reloading it through `r13`.

That file is not in the DOL link, so it cannot move a DOL byte - and the gate confirms it
did not (sha1 unchanged below). After the fix the port link **improved**: 287 -> 286
undefined, 0 duplicates.

## Claimed range

`Kyoto/Math/Carve80335A0C.c` claims **exactly** `.text start:0x80335A0C end:0x80335A14` and
nothing else - no `.sdata2`, no `.data`, no `.sbss`. Both neighbours are already claimed
units (`Carve803359F4.c` ends exactly at 0x80335A0C, `Carve80335A14.c` starts at
0x80335A14), and proximity to another carve is fine per the carve-vein notes.

Four files, each entry in address order:

- `src/Kyoto/Math/Carve80335A0C.c` (new)
- `configure.py` - `Object(Matching, "Kyoto/Math/Carve80335A0C.c")`, one line, between
  `Carve803359F4.c` and `Carve80335A14.c`
- `config/G2ME01/splits.txt` - between `Carve803359F4.c` and `Carve80335A14.c`
- `files.cmake` - same position

Fifth file, the port-side definition above (required by the gate; `PortGlobals.cpp` is not
in the DOL link). No `PortLinkStubs.cpp` duplicate existed for this symbol
(`grep -rn fn_80335A src/ include/` returned nothing before the carve), and none was created.

Single function, so the descending-source-order rule is met trivially; plain C so the
`fn_` symbol does not mangle.

## Measured

| check | result |
| --- | --- |
| `sha1sum build/G2ME01/main.dol` | `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` - unchanged |
| `total_functions` | 28465 - unchanged |
| `matched_functions` | 13598 -> **13599** |
| `linked` | 6646 -> **6647** |
| `complete_units` | 1024 -> 1025 |
| `main/Kyoto/Math/Carve80335A0C` | `fuzzy 100.00%, matched 1 / 1, complete 1 / 1` |
| `flip_test.sh Kyoto/Math/Carve80335A0C.c` | **PASS -> kept as Matching** (1/1 kept, 0 failed) |
| `./tools/probe_sources.sh` | `1000 files, 0 failed, 0 errors; link: LINKED (286 undefined, 0 duplicates)` |
| `python3 tools/check_symbol_names.py` | `checked 609 units; 0 declared names are missing` |
| `./tools/goal_check.sh build/goal/item.json` | **PASS** (gate, counts, symbol names, flip) |
| 86 RELs | `cmp`-equal, sha1s match `config.yml` (gate.sh step) |

`tools/carve_diff.sh 0x80335A0C 0x8` prints **NOT byte-exact**, and that is expected rather
than a failure: it diffs the *relocatable* `.o`, where the SDA21 relocation is still
unresolved (`lfs f1,0(0)`), against the linked `main.elf`. The linked image at that address
is `c0 22 c9 28 / 4e 80 00 20`, byte-identical to `build/G2ME01/asm/auto_03_80335A0C_text.s`,
and `flip_test.sh` - which the item brief names as the only decider - passes. Recorded so
the next run does not read that line as a wall.

## Notes for the next run

- A carve that loads a `.sdata2` word will fail `goal_check`'s gate step on `link-gap` every
  time, because declaring the word `extern` grows the port's undefined list. The fix is one
  `extern "C" const float` in `src/MetroidPrime/PortGlobals.cpp`; it never touches the DOL.
  Budget for the rebuild, since it is a full port build inside the gate.
- `carve_diff.sh`'s "NOT byte-exact" on any unit with an SDA21 or `bl` relocation is a
  property of the tool reading an unlinked object, not a mismatch.

Nothing was committed, per the brief. No `NEW:` line - the item's work landed.