# progress-twin-rel-sandboss

**Result: PASS.** `goal_check.sh build/goal/item.json` -> `PASS progress-twin-rel-sandboss`.
`module:SandBoss` matched functions **24 -> 32** (of 322); project `matched_functions`
**13098 -> 13106**, `linked` **6196 -> 6204**; the module's sha1 is unchanged
(`0a28cddbe1a04c8c4ca2bf6a6750d0467b9e805e`, `build/G2ME01/SandBoss/SandBoss.rel`), all 87
checksums OK, `ninja` exit 0.

## What landed

One new `Matching` unit, `MetroidPrime/ScriptObjects/CSandBossRelTail.cpp`, claiming **.text
0x10548..0x1073C** - eight functions, all 8/8 at 100.00% in `build/report.json`:

| retail | fn | size | what it is |
| --- | --- | --- | --- |
| 0x10548 | `fn_55_10548` | 0x40 | two-member copy; second member copy-constructed at +0x4 |
| 0x10588 | `fn_55_10588` | 0x3C | deleting destructor of a memberless object |
| 0x105C4 | `fn_55_105C4` | 0x40 | copy of the class whose only data is the flag byte at +0x30 |
| 0x10604 | `fn_55_10604` | 0x20 | forwarder to `fn_55_10624` |
| 0x10624 | `fn_55_10624` | 0x28 | null-guarded forwarder to `fn_55_106E0` |
| 0x1064C | `fn_55_1064C` | 0x3C | deleting destructor of a memberless object |
| 0x10688 | `fn_55_10688` | 0x58 | the 0x2C-byte payload's constructor |
| 0x106E0 | `fn_55_106E0` | 0x5C | the payload copy (member-for-member) |

Files: `src/MetroidPrime/ScriptObjects/CSandBossRelTail.cpp` (new),
`config/G2ME01/rels/SandBoss/splits.txt` (the claim), `configure.py` (one `Object(Matching, ...)`
with `mw_version="GC/2.7"`), `config/G2ME01/config.yml` (`force_active`), `files.cmake` (one line).

## Three things that had to be measured, in the order they blocked

1. **`mw_version` is the whole of the first two functions' diff.** Under the module's default
   **GC/1.3.2**, `fn_55_10548` is **62.5%** and `fn_55_105C4` is **77.5%**: retail schedules the
   argument setup (`lhz`/`lbz`) *in front of* the `stw r31,0xc(r1)` prologue save and 1.3.2 puts
   the save first. Six spellings of `fn_55_105C4` under 1.3.2 - `store`-then-`test`, `test`-then-
   `store`, a local flag, a `bool` member, `void` with no return, `return self` - all landed at
   36.125/45.25/56.25/75.0/77.5%, none at 100. **Under `mw_version="GC/2.7"` both are 100.00%
   unchanged, no spelling change at all.** This is the same finding `CLumiteRelTail.cpp` records
   for `fn_39_738`, so it is a property of this module family's tail, not of one function: the
   out-of-line instantiation blocks were compiled by the later compiler. **Try 2.7 before
   spending spellings on a REL tail that only differs in prologue scheduling.**
2. **The two destructors are dead-stripped.** With everything at 100.00% the module still linked
   **0x78 bytes short**: nothing in the module calls `fn_55_10588` or `fn_55_1064C`, so mwldeppc
   drops them and every function after them shifts. `config/G2ME01/config.yml`'s per-module
   `force_active:` list is the fix (dtk 1.8.4), exactly as `docs/RUNNING_THE_DECOMP.md` records
   for `ForgottenObject`; both names are listed there now. Verified by the sha1, not by objdiff -
   every function was 100.00% while the module was 120 bytes short.
3. **`check_files_cmake.py` fails a `Matching` unit that is neither in `files.cmake` nor in its
   `EXCLUDED` list**, and only a `RELMain`/`RELExit` unit is exempt (the SandBoss head is exempt
   for that reason; `tools/` is not editable from a goal item). The fix is the arrangement
   `CLumiteRelTail.cpp` uses: the bodies are inside `#ifdef __MWERKS__` with an empty host branch,
   so listing the file adds no undefined reference. Confirmed by the gate's own
   `check_files_cmake.py` run and by `goal_check`'s gate step.

## The twin list, judged

The 81 listed twins were a useful map but not a work list: the six that landed here
(`fn_55_10548`, `fn_55_10588`, `fn_55_10604`, `fn_55_10624`, `fn_55_1064C`, and `fn_55_106E0`'s
shape) are all one container instantiation. `fn_55_106E0`/`fn_55_10688` had **no** twin and were
written from the bytes; both needed the two adjacent words copied **in reverse source order**
(`x24` before `x20`), which is what retail's `lwz 0x24 / lwz 0x0 / stw 0x24 / stw 0x20` shows.

## Left for the next run (measured, not attempted)

- **0x1073C..0x107B0**: `fn_55_1073C` (0x24) and `fn_55_10760` (0x50). `fn_55_10760` reads
  `lbl_55_rodata_B8` (module rodata, still retail's) and `sRightVector__9CVector3f` (DOL), so the
  claim would carry a data relocation; the shape is otherwise the same float/word copy.
- **0x10AE8..0x10C78**: four contiguous destructors, `fn_55_10AE8` (0x60), `fn_55_10B48` (0x7C),
  `fn_55_10BC4` (0x54), `fn_55_10C18` (0x60), with twins `__dt__18CErrorOutputWindowFv`,
  `__dt__15CGameProjectileFv` and `__dt__Q24rstl36vector<i,...>Fv`. These tear down members
  (vtables, `rstl` containers) so they need the class layouts, not just a spelling.
- **0x108E4** (0x204, `CPlasmaProjectile`'s destructor, twin `__dt__17CPlasmaProjectileFv` in
  `src/MetroidPrime/TypesMatch.cpp`) is the biggest single item left in this block; its callees
  are module-local copies (`fn_55_10BC4`, `fn_55_10AE8`), so it wants those two first.

No `WALL:` line: nothing was left at a sub-100% score, and the one function that looked like a
scheduling wall (77.5% over six spellings) was the compiler version, not the source.

`NEW:` - none filed. The three findings above are lessons (notes), and the remaining work is the
same target (`module:SandBoss`) this item already names.

Note for the driver: `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` show as modified in
`git status` - that is `goal_check.sh`'s own `MP_GATE_DOCS_WRITE=1 ./tools/gate.sh` rewriting the
derived counts, not an edit of mine.

---

# Run 2 (2026-10-02, same day) — PASS, module 32 -> 39

**Result: PASS.** `./tools/goal_check.sh build/goal/item.json` -> `PASS progress-twin-rel-sandboss`.
`module:SandBoss` matched functions **32 -> 39** (of 322); project `matched_functions`
**13343 -> 13350**, `linked` **6391 -> 6398**; `build/G2ME01/SandBoss/SandBoss.rel` sha1
`0a28cddbe1a04c8c4ca2bf6a6750d0467b9e805e` == `config/G2ME01/config.yml` and `cmp` against
`orig/G2ME01/files/RelProd/SandBoss.rel` prints nothing. `All:` line
`37.40% fuzzy, 30.83% matched, 13.70% linked (13350 / 28465 functions)`.

Two new `Matching` units, 7 functions, all 100.00% in `build/report.json` (they were 0.00% and
unclaimed before - the runs were `auto_00_00000178_text` / `auto_00_0001073C_text`):

| unit (claim) | retail | fn | size | before | after | what it is |
| --- | --- | --- | --- | --- | --- | --- |
| `CSandBossRelTail2.cpp` (.text 0x4D78..0x4E94) | 0x4D78 | `fn_55_4D78` | 0x70 | 0.00% | 100.00% | `CCameraShakerData::~CCameraShakerData()` |
| | 0x4DE8 | `fn_55_4DE8` | 0x58 | 0.00% | 100.00% | `CMayaSpline::~CMayaSpline()` |
| | 0x4E40 | `fn_55_4E40` | 0x54 | 0.00% | 100.00% | `~vector<CMayaSplineKnot>` (the member at +8) |
| `CSandBossRelTail3.cpp` (.text 0x10AE8..0x10C78) | 0x10AE8 | `fn_55_10AE8` | 0x60 | 0.00% | 100.00% | `CBeamProjectile::~CBeamProjectile()` |
| | 0x10B48 | `fn_55_10B48` | 0x7C | 0.00% | 100.00% | `CGameProjectile::~CGameProjectile()` |
| | 0x10BC4 | `fn_55_10BC4` | 0x54 | 0.00% | 100.00% | `~vector<int>`, the +0x598 member |
| | 0x10C18 | `fn_55_10C18` | 0x60 | 0.00% | 100.00% | module-local class dtor (vtable `lbl_55_data_9AC`) |

## The twin list judged again: it is a map, not an order

The previous list's "left for the next run" named 0x1073C.., 0x10AE8.. and 0x108E4. The
**0x4D78..0x51BC** six-function run (rank 1 of the item's own list) was not in it and is what the
first unit took; 0x10AE8.. is the second. The two runs the item ranks first are exactly the two
that landed, so read the item's ordering, not just the previous notes.

## Four mechanical facts this run measured, in the order they blocked

1. **A body may be copied from the DOL's own free-function spelling, not just its twin's source.**
   All three functions of the first unit exist in the DOL as *free* functions over `void*`:
   `__dt__17CCameraShakerDataFv` (0x8009D174, 112 B), `__dt__11CMayaSplineFv` (0x800327FC, 88 B)
   and `fn_80032854` (84 B), the last two in `src/MetroidPrime/Factories/Carve80032774.cpp` and all
   100.00% in `build/report.json`. Copying those bodies verbatim, with the module's own names for
   the callees, is 100.00% on the first build - no spelling search at all. `CLumiteRelTail`'s
   `mw_version="GC/2.7"` was **not** needed here: the module's default GC/1.3.2 gives all three
   100.00%, so the 2.7 override is a property of the *tail* (0x10548..), not of the module.
2. **The auto objects carry relocations for module-internal calls, so `force_active:` is not
   needed for a function retail's own code calls** - `build/G2ME01/SandBoss/obj/auto_00_00000178_text.o`
   has `R_PPC_REL24 ... fn_55_4D78` at 0x4BC4 (`powerpc-eabi-readelf -r`). That is the whole
   difference from the previous run's `fn_55_10588`/`fn_55_1064C`: nothing in the module calls
   those two, so they were dead-stripped. None of the seven functions here needed a
   `config.yml force_active:` entry, and the ldscript's FORCEACTIVE list already had
   `fn_55_10C18` and `lbl_55_data_9AC` from the retail relocations.
3. **A module-local callee can be called by dtk's own name for it, and a DOL import the auto
   object lists as `U` resolves.** `nm -u` on the auto object first: `fn_55_480C` (0x480C, its own
   `T` definition, the module's copy of the +0x1F8 member's destructor), `fn_55_108E4`
   (0x108E4, still unclaimed and retail's), `lbl_55_data_9AC` (`.data:0x9AC`, 0x98 = the module's
   own vtable, `auto_04_00000000_data.o`), and the DOL's `__vt__15CBeamProjectile`,
   `__vt__15CGameProjectile`, `__dt__17CProjectileWeaponFv`, `__dt__7CWeaponFv`. `nm -u` on the
   new object is exactly those eight names, and the module hashes.
4. **A member-function spelling is the *wrong* spelling for these, and that is now measured, not
   assumed.** A scratch compile of the DOL's own `CGameProjectile::~CGameProjectile() {}`
   (`src/MetroidPrime/Weapons/CGameProjectile.cpp:302`) under the module's flags gives
   `__dt__15CGameProjectileFv` (0x7C, right size) **plus** an out-of-line weak
   `__dt__Q24rstl56optional_object<Q218CImpactVisorEffect15SParticleEffect>Fv` (0x5C), a local
   `destroy<...>` (0x48) and an 0x8C `__vt__15CGameProjectile` in `.data` (`nm --defined-only`).
   Retail's module has none of those extras: its +0x1F8 teardown is the module's own 0x480C and
   its vtable is the imported DOL one. The same argument covers the whole family - it is also why
   the item's "write it as a member of a declared class" hint did not apply to any of the seven.

## Gate/detail notes worth keeping

- A compile error on the first `CSandBossRelTail3.cpp` build: `fn_55_10B48(SFn55_10AE8*, int)`
  does not match `fn_55_10B48(SFn55_10B48*, short)`. Fixed with
  `reinterpret_cast< SFn55_10B48* >(self)`, the pointer types do not change the bytes.
- `docs/research/raw_offsets.md` needed a section per new file (the gate's `raw-offsets` step is
  fatal, not a warning): 3 sites for Tail2 (`+0x18`/`+0x5C`/`+0xA0`) and 2 for Tail3
  (`+0x1F8`/`+0x238`); the tool now prints `191 raw-offset site(s) in 82 file(s)` and the totals
  line in that file is re-derived to match. `+0xC`/`+8`/`+0` are *not* counted (the checker keys
  on two hex digits).
- `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` show as modified in `git status`: that is
  `goal_check.sh`'s own `MP_GATE_DOCS_WRITE=1 ./tools/gate.sh` rewriting the derived counts, not an
  edit of mine. Same as the previous run.

## Left for the next run (measured, not attempted)

- **0x4E94..0x51BC** (3 functions, immediately above Tail2, the rest of the item's rank-1 run):
  `fn_55_4E94` (0xAC, `CCameraShakerData` copy ctor), `fn_55_4F40` (0xAC, `CMayaSpline` copy ctor)
  and `fn_55_4FEC` (0x1D0, `vector<CMayaSplineKnot>` copy ctor, `mulli 0x1c` + the 4-element
  unrolled copy). All three call each other, so one claim of 0x4E94..0x51BC covers the run; the
  first two are mostly word/float copies in retail's 2-deep `lwz/lwz/stw/stw` pipeline (the
  previous run's `fn_55_106E0` note is the same problem), the third is the one to be careful with.
- **0x1073C..0x107B0**: `fn_55_1073C` (0x24, `MakeInvalid`) and `fn_55_10760` (0x50). Both are a
  0x2C-byte record's constructor; note it writes words at **+0x20 and +0x24 and a byte at +0x28**,
  so the module's class is *not* the repo's `CRayCastResult` (`CHECK_SIZEOF 0x30`, `mValid` at
  +0x24) - copy the offsets from the asm, not the header. `fn_55_10760` reads
  `lbl_55_rodata_B8` (module rodata, `auto_03_00000000_rodata.o`, `R lbl_55_rodata_B8`) and
  `sRightVector__9CVector3f` (DOL), both of which resolve.
- **0x108E4** (0x204, `CPlasmaProjectile::~CPlasmaProjectile()`, 516 B, the biggest single item
  left): its two module-local callees are now claimed in `CSandBossRelTail3.cpp`, so a claim of
  0x108E4..0x10C78 in one unit is possible and would absorb Tail3's four (one unit, one range);
  its body is fourteen member teardowns at +0x620..+0x69C whose shapes are each a typed
  `optional_object`/smart-pointer/`CToken` destructor, so it is a spelling job per member, not one
  body. `U __vt__17CPlasmaProjectile` and `U __dt__6CTokenFv` are already imports.
- The `0x2FB4..0x3044` run (3 functions with DOL twins: `reserved_vector<SDSPStreamVoice,4>::push_back`,
  `__sys_free`, `construct_impl<CPASAnimState>`) is the other small contiguous run in the
  unclaimed middle, if a cheaper target than the copy ctors is wanted.

No `WALL:` line: nothing was left at a sub-100% score - every function written this run matched on
its first build. No `STALE:`: the module's 39 were 32 at the baseline.

`NEW:` - none filed, same reasoning as the previous run: the remaining work is the same target
(`module:SandBoss`) this item already names, and the three findings above are lessons for the notes
file, not new queue items.
