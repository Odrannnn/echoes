# carve-800534bc - `MetroidPrime/Carve800534BC` is `Matching` and flipped

**Result: PASS.** `tools/goal_check.sh build/goal/item.json` exits 0. `matched_functions`
12482 -> 12483, `linked` 5863 -> 5864, DOL sha1 unchanged at
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, all 86 RELs still byte-identical.

## What the carve is

`fn_800534BC`, `.text 0x800534BC..0x800534C4`, 8 bytes, 1 function, out of dtk's
`auto_03_800534B4_text` (0x800534B4..0x80053594):

```
800534bc:  c0 22 85 60   lfs f1,lbl_8041A920@sda21(r0)
800534c0:  4e 80 00 20   blr
```

`lbl_8041A920` is `.sdata2:0x8041A920`, `size:0x8`, `data:float` (`symbols.txt:21860`), and
`objdump -s -j .sdata2 build/G2ME01/main.elf` gives its four bytes as `3f800000` = **1.0f**
(the object's other four bytes are a `0.f` pad, which is why it is 0x8 bytes and the carve's
own read is 4). So the body is `float fn_800534BC(void) { return lbl_8041A920; }`.

The byte-shape twin the item named, `GetIngSnatchingModelOverlapSize__10CPatternedCFv`
(0x80074AF8, also 0x8 bytes, also `lfs f1,<.sdata2 float> ; blr`), is
`src/MetroidPrime/Enemies/CPatterned.cpp:470` `return 0.f;` and loads `lbl_8041AAC0`, which
`auto_11_8041AA90_sdata2.s` types `.float 0`. **The twin fixes the instruction sequence, not
the constant** - reading the twin as `return 0.f;` here would have compiled, linked and scored
100% per function while producing the wrong answer. The neighbours agree on what the shape is:
`0x800534B4` is `SetDrawFlags__12CParticleGenFUi` and `0x800534C4` is
`GetDrawFlags__12CParticleGenCFv`, and `include/Kyoto/Particles/CParticleGen.hpp:41` has
`virtual float GetGeneratorRate() const { return 1.f; }` between that pair. Retail carries no
name for `fn_800534BC`, so the identification is recorded, not relied on.

## Files (the carve is four, plus the one the port needed)

| file | change |
| --- | --- |
| `src/MetroidPrime/Carve800534BC.c` | new; `.c` because a C++ definition of `fn_800534BC` mangles and objdiff would pair nothing |
| `configure.py:659` | `Object(Matching, "MetroidPrime/Carve800534BC.c"),` on one line, in address order between `Carve800534B0.c` and `Carve80053594.c` |
| `config/G2ME01/splits.txt:175-176` | `.text start:0x800534BC end:0x800534C4`, in address order; `total_functions` still 28465 |
| `files.cmake:410` | `src/MetroidPrime/Carve800534BC.c` |
| `src/MetroidPrime/PortGlobals.cpp:974-981` | `extern "C" const float lbl_8041A920 = 1.0f;` - see below |

No `PortLinkStubs.cpp` duplicate existed (`grep -rn fn_800534BC src/` had no other hit), so
nothing had to be deleted there.

## The port definition is part of the carve, not an extra

**A carve that declares retail data is not finished until the port link has that data.** The
first `goal_check` run failed with `gate.sh` -> `probe link-gap`:

```
link_check: STRICT FAIL - regression gate: 292 undefined against a baseline of 291 (GREW)
  NEW  lbl_8041A920
gap grew: lbl_8041A920 is not in port_link_gap_list.md
```

That is the rule from `docs/RUNNING_THE_DECOMP.md`'s carve section working in the port's
direction: the carve's `.text` reaches retail's `.sdata2` word, the port has no retail objects,
and a PC link has no definition to bind. `src/MetroidPrime/PortGlobals.cpp` is exactly where
that belongs - its header states the rule ("for the DOL every symbol ... is defined by a
*retail* object ... a PC link has no retail objects, so each one needs a real definition
somewhere, holding the value the retail binary holds") and it is deliberately not a
`configure.py` unit, so it cannot perturb `main.dol`. It already carries the identical entry
for `lbl_8041C398` and the three `lbl_8041C5xx` floats.

**And the carve cannot write `return 1.f;` to avoid the extern.** A literal of its own gives
that translation unit a `.sdata2` section `config/G2ME01/splits.txt` does not claim for it -
the `lbl_8041C398` failure recorded in that file's own comment, and the reason a `Matching`
object may not own a `.rodata`/`.sdata2`/`.data` byte splits.txt has not given it. So the
carve declares retail's word and `PortGlobals.cpp` supplies it. Declaring `extern const float`
(not `const char[]` etc.) is also what makes the object's single relocation
`R_PPC_EMB_SDA21 lbl_8041A920` land on retail's own address.

## Measurements

```
tools/decomp_build.sh -r MetroidPrime/Carve800534BC
  All: 35.30% fuzzy, 29.11% matched, 12.90% linked (12483 / 28465 functions)
  main/MetroidPrime/Carve800534BC: 100.00% fuzzy, 100.00% matched (1 / 1 functions)

tools/flip_test.sh MetroidPrime/Carve800534BC.c
  PASS  -> kept as Matching      kept: 1 / 1  failed: 0  skipped: 0

tools/unit_fit.sh MetroidPrime/Carve800534BC.c
  .text  claimed 8  ours 8  retail 8  fits
  no extra functions: our object defines only what the retail unit object does

python3 tools/check_decl_order.py
  ok: 982 unit(s) checked, 28 permuted, all 28 accounted for in decl_order.md
  (982 not 981 - the new unit is in the sweep and is not permuted; one function, so the
   descending rule is met trivially)

python3 tools/check_symbol_names.py
  checked 525 units; 0 declared names are missing from their object

sha1sum build/G2ME01/main.dol -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
build/report.json total_functions -> 28465 (unchanged by the splits.txt edit)

tools/goal_check.sh build/goal/item.json
  ok  no judge-owned path touched
  ok  gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok  counts: matched 12482 -> 12483   linked 5863 -> 5864
  ok  check_symbol_names.py
  ok  All:  35.30% fuzzy, 29.11% matched, 12.90% linked (12483 / 28465 functions)
  ok  flip_test MetroidPrime/Carve800534BC.c: PASS, Object(Matching) in configure.py
  goal_check: PASS carve-800534bc
```

The linked DOL holds retail's bytes at the carve: `dol_read 0x800534BC 8` on
`build/G2ME01/main.dol` gives `c0 22 85 60 4e 80 00 20`, and `main.elf` at the same address
disassembles as `lfs f1,-31392(r2) ; blr` - `-31392(r2)` with `_SDA2_BASE_ = 0x804223C0` is
`0x8041A920`, so the relocation resolved to retail's own word.

**`tools/carve_diff.sh 0x800534BC 0x8` prints `NOT byte-exact`, and that is a tool artefact,
not a mismatch.** It diffs the *object*, where the SDA21 field is still zero
(`objdump -r` shows `R_PPC_EMB_SDA21 lbl_8041A920`) against `main.elf`, where it is filled.
The acceptance test is `flip_test`, which passed, and the gate's DOL hash is unchanged. Worth
knowing before a future carve's author reads `carve_diff` as a verdict on a carve that loads
data.

## For the next one

- **`tools/carve_diff.sh` cannot judge a carve that loads `.sdata2`.** Compare the linked
  `main.elf`/`main.dol`, not the object, and let `flip_test` decide.
- **Adding a carve that reads retail data costs one port-link symbol.** Budget it: the extern
  in the carve plus the definition in `PortGlobals.cpp`, or `goal_check` fails on
  `probe link-gap` while every other check is green. The same applies to a carve that loads a
  `.data` vtable or calls an unnamed `fn_`; `src/MetaRender/Carve80272958.c` is the worked
  example for the call case.
- **A byte-shape twin tells you the instruction sequence and nothing about the constant.**
  Read the `.sdata2` word out of `main.elf` before writing the body. This one is 1.0f where
  the twin is 0.0f, and a copy of the twin's source would have been a wrong answer at 100%.

## Blocked

Nothing. No `NEW:` item: this one flipped, and there is no adjacent unsourced run in the same
`auto_*` unit that this run measured a wall against. (`SetDrawFlags__12CParticleGenFUi` at
0x800534B4 and `GetDrawFlags__12CParticleGenCFv` at 0x800534C4 are real-named
`CParticleGen` members of `Kyoto/Particles/CParticleGen.cpp`, whose own claimed range is
0x802E80B4..0x802E8198 - a different unit entirely, not a carve candidate. Not filed.)
