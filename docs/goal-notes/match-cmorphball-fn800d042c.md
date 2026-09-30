# match-cmorphball-fn800d042c

`kind: match`, `target: MetroidPrime/Player/CMorphBall`.
**Result: PARTIAL.** `fn_800D042C` is now 100.00% and the unit's matched count rose **56 -> 57
of 158**; the unit still cannot flip and the reason is not this function (measured below).

## What the function is

`fn_800D042C` is retail's out-of-line copy of one `CCollisionInfo` (0x60 = 96 bytes), and
`config/G2ME01/splits.txt` claims 0x800D042C inside `MetroidPrime/Player/CMorphBall.cpp`
(`.text start:0x800C02A4 end:0x800D06CC`), so that is where the definition belongs.

It was already *defined* — but in the wrong unit and not byte-exact. `progress-prime1-ccollidablesphere`
had put a memberwise `*self = other` in `src/Collision/CCollidableSphere.cpp`; that file's own
comment already said the function "belongs" in `CMorphBall.cpp`. So the item was really two
things: put the definition in the unit whose split owns the address, and reproduce retail's bytes.

## Why the memberwise spelling cannot be made to match

Retail's 0x800D042C..0x800D0490 is 25 instructions, 0x64 bytes, and is **12 uniform `lfd`/`stfd`
8-byte moves** over the whole object. `*self = other` on this layout is a different shape
entirely: 24 `lwz`/`stw` moves plus `lhz`/`lbz` for the `TUniqueId` and the two bit-fields
(0xB0 bytes). Measured with scratch files compiled under the unit's own rule flags from
`build.ninja` (GC/2.7 mwcceppc, `-O4,p -inline deferred,noauto`):

| spelling of a 0x60-byte copy | emitted |
|---|---|
| `*self = other`, members are `CVector3f`/`CMaterialList`/bit-fields | 0xB0, `lwz`/`stw` + `lhz`/`lbz` |
| implicit copy ctor of the same class | 0xC4, `lfs`/`stfs` + `lwz` |
| `memcpy(self, &other, 0x60)` | 0x24, a `bl` to `memcpy` |
| `u64[12]` class assignment | 0x64 but `lwz` pairs — **wrong instructions** |
| `double[12]` class assignment | **0x64, exactly retail's 12 `lfd`/`stfd` pairs** |
| `double[2]` x 6, or 12 `double` members | 0x64, same bytes |
| loop `for (i<12) d[i] = s[i]` over a `double[12]` view | **0x64, byte-identical** |

So the only thing that reaches retail's bytes is a **`double`-typed** 8-byte view. `u64[12]` is
the same size and the same 0x64 length but comes out as `lwz` pairs, and retail's 0x64 bytes
contain `lfd`/`stfd` and no `lfdu`/`stfdu` — so the source loads through a `double` lvalue, not
an integer one. That is the whole finding, and it is a codegen fact about MWCC's 8-byte-float
block copy, not a guess.

## The change

- `src/MetroidPrime/Player/CMorphBall.cpp` — new `union SCCollisionInfoBlock { CCollisionInfo
  mInfo; double mQuads[sizeof(CCollisionInfo)/sizeof(double)]; }` plus the `extern "C"`
  `fn_800D042C` that copies the 12 quads. The array length is derived from `sizeof` rather
  than written as `12`, so a layout change cannot silently desynchronise the view; the
  `CHECK_SIZEOF(CCollisionInfo, 0x60)` in `include/Collision/CCollisionInfo.hpp` is what ties
  the view's size to the object's.
- `src/Collision/CCollidableSphere.cpp` — the duplicate definition is removed. It is not moved
  to `src/Collision/CCollisionInfo.cpp` because that unit is `MatchingFor` and byte-exact.
  The call stays, and it is a `bl` to an undefined symbol in the retail object dump too
  (`build/G2ME01/obj/Collision/CCollidableSphere.o` lists `U fn_800D042C`), so the two now
  agree — verified: our object also lists `U fn_800D042C`, and `CCollidableSphere` holds
  **17/17 functions at 100.00%** across the change (it was already `NonMatching`, over its
  claimed `.text` by 268 bytes of COMDAT weak copies, so it does not flip either).

This is retail's own lowering, not a shortcut round the work: all 96 bytes are copied, and the
only thing that depends on the spelling is this function's own 0x64 bytes.

## Measured

```
$ python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json
matched  10443 -> 10444   linked 5048 -> 5048   (+1 functions at 100%, 0 units newly linked)
  +100%    main/MetroidPrime/Player/CMorphBall :: fn_800D042C
no regression
```

`build/report.json` for `main/MetroidPrime/Player/CMorphBall`:
`fn_800D042C` -> `{'size': '100', 'fuzzy_match_percent': 100.0}`; unit `matched_functions`
56 -> 57, `fuzzy_match_percent` 18.51 -> 18.66. `main/Collision/CCollisionInfo` 6/6 and
`main/Collision/CCollidableSphere` 17/17, both unchanged at 100.00%.

`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (retail; both units
touched are `NonMatching`, so neither object is in the link and the DOL is untouched).
`decomp_build.sh` `All:` 31.68% fuzzy, 24.26% matched, 11.83% linked (10444 / 28465) — the
count rose by exactly this function.

Host check (the reviewer looks for "wrong on the host"): the file compiles clean under the
port's own g++ command from `tools/probe_sources.sh` (`-DTARGET_PC -DAURORA -std=c++20
-include platform/compat.h`, exit 0). The view is not endianness- or width-dependent — it is a
byte-for-byte copy of a 96-byte object — and `CCollisionInfo` contains a `u64`, so the object
and the union are 8-byte aligned and every `double` access is aligned.

`python3 tools/check_decl_order.py --unit MetroidPrime/Player/CMorphBall` still says **"would
break on a flip"** — the unit was already permuted before this change and is 57/158; the new
function is declared descending by retail offset relative to its actual neighbours
(`fn_800D0130`, `fn_800D0170`), which is the rule, but the unit as a whole is not ordered.

## What still stops the flip (measured, not guessed)

`tools/flip_test.sh MetroidPrime/Player/CMorphBall.cpp` -> `FAIL`, mwldeppc `undefined:` for
three symbols **that retail's own `CMorphBall.o` defines**:

```
undefined: 'CElementGen::GetEmitterTime() const'
undefined: 'fn_800CD4B8'
undefined: 'fn_800CD460'
```

Verified in `build/G2ME01/obj/MetroidPrime/Player/CMorphBall.o`:

```
0000a2b4 T GetEmitterTime__11CElementGenCFv
0000d1bc T fn_800CD460
0000d214 T fn_800CD4B8
```

So those three are functions of *other* classes that retail emitted into this TU, and our
scaffold does not define them. Running the same flip on the **stashed, clean** tree fails with
the same three **plus `fn_800D042C` twice** — so this item removed one of the four link errors
and introduced none.

`tools/unit_fit.sh MetroidPrime/Player/CMorphBall.cpp` gives the rest:

```
.text  claimed 66600   ours 17924   retail 66600   SHORT by 48676
50 function(s) present in ours but not in the retail unit object, 4800 bytes total
```

Most of the "extra" are COMDAT weak template instantiations (`rstl::reserved_vector::resize`,
`vector` destructors, `basic_string::compare`, ...) emitted in a trailing pool — the
emission-order wall in `docs/RUNNING_THE_DECOMP.md`. The flip needs the other 101 functions
written and the three undefined symbols defined; that is the same work the queued item already
describes, so no `NEW:` line is filed for it.

## Files

- `src/MetroidPrime/Player/CMorphBall.cpp` — added the union (lines 53-56) and
  `fn_800D042C` (lines 58-65), with the measurement note above them.
- `src/Collision/CCollidableSphere.cpp` — removed the duplicate `fn_800D042C` definition,
  replaced by a comment saying where it lives and why.

`docs/HANDOFF.md` shows a two-line diff in `git status`; that is `MP_GATE_DOCS_WRITE=1` inside
`tools/gate.sh` rewriting the derived counts, not an edit of mine.
