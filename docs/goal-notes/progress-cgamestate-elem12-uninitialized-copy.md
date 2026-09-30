# progress-cgamestate-elem12-uninitialized-copy

## Gate fix round

The judge failed the item on `decl-order` only. The other fourteen `gate.sh` steps were `ok` on
the first run, so nothing else was touched.

**What the defect was.** The new definition `fn_80142944` had been inserted between
`fn_80142A10` (0x80142A10) and `fn_801429AC` (0x801429AC). mwcceppc emits definitions in reverse
source order, so 0x80142944 landed ahead of 0x801429AC in `.text`: a one-position transposition.
Measured with `tools/check_decl_order.py`'s own comparison (its `--unit` mode prints only the
first eight functions, and those agreed):

```
len(got)=104 len(want)=104 mismatched positions: 2
  pos  28  ours=fn_801429AC   retail=fn_80142944
  pos  29  ours=fn_80142944   retail=fn_801429AC
```

That is the shape the `mainMid` entry in `docs/research/decl_order.md` warns about: **two
functions are out of place, not two hundred** - emission follows file order, so the displacement
stops at the next definition. It is also why `decl-order` is the only check that saw it:
objdiff pairs by name, `unit_fit.sh` compares sizes, and a permutation changes neither.

**What changed**

- `src/MetroidPrime/Player/CGameState.cpp` - the comment block and definition of `fn_80142944`
  moved whole from above `fn_801429AC` to below it, so the three are now declared
  0x80142A10, 0x801429AC, 0x80142944, 0x80142914 - descending by retail offset. No token inside
  the function changed; `git diff --stat` stays `38 insertions(+)` with zero deletions, which is
  the proof that this is a relocation and not an edit. It is safe because the unit already
  forward-declares `fn_80004BEC` and `fn_801429AC` at the top of the file, and nothing in the
  translation unit took the address of `fn_80142944`.
- Nothing in `docs/research/decl_order.md`. The log offered "add it with a reason", but this
  permutation was introduced by this very item and is one adjacent swap, so it is fixed rather
  than filed. Writing a work-list entry for a defect we had just created, in a file whose policy
  is "only worth doing for a unit that is otherwise ready to flip", would have been the wrong
  call: `CGameState` is 95 of 116 and is not a flip candidate, but the reorder is free here.

**Measured, not recalled**

```
./tools/gate.sh                         -> GATE PASS  a78fdb8+2 changed  (decl order ok)
./tools/decomp_build.sh                 -> All:  30.61% fuzzy, 22.73% matched, 11.74% linked (9938 / 28465 functions)
python3 tools/check_decl_order.py       -> ok: 957 unit(s) checked, 31 permuted, all 31 accounted for in decl_order.md
fn_80142944 in build/report.json        -> fuzzy_match_percent 100.0, size 104 (retail 0x68)
main/MetroidPrime/Player/CGameState     -> 95 / 116 matched, linked 0
sha1sum build/G2ME01/main.dol           -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
python3 tools/check_symbol_names.py     -> checked 503 units; 0 declared names are missing from their object
python3 tools/check_docs_claims.py      -> docs claims agree with the tree
./tools/probe_sources.sh                -> probe: 749 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)
python3 tools/link_gap.py --rebuild     -> ok: 246 MISSING symbol(s), all accounted for in port_link_gap_list.md
python3 tools/check_raw_offsets.py      -> ok: 152 raw-offset site(s) in 61 file(s), all documented in raw_offsets.md
python3 tools/check_files_cmake.py      -> every configured DOL object is either in files.cmake or excluded with a reason
python3 tools/gen_module_order.py --check-> rel_module_order: 86 modules, unchanged
```

`9938` matched and the `6ef9b491` DOL hash are the load-bearing numbers: the reorder is a pure
relocation inside one translation unit, so both must be unmoved, and they are. `raw-offsets`,
`files-cmake`, `docs` and `module-order` were never failing and were re-run only to confirm they
still are not.

**Not fixed, and deliberately so.** `docs/research/decl_order.md` still quotes `837 unit(s)
checked, 18 permuted` in its opening paragraph; the tool now prints `957` and `31`. That drift
predates this item and the paragraph already carries the correction ("Take the tool's number, not
this paragraph's"), so it is an annotated stale checkpoint rather than something to rewrite here.
