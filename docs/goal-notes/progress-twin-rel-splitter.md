# progress-twin-rel-splitter

**Result: PASS.** `./tools/goal_check.sh build/goal/item.json` -> `PASS progress-twin-rel-splitter`:

```
ok  no judge-owned path touched
ok  gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
ok  counts: matched 13193 -> 13203   linked 6246 -> 6256
ok  check_symbol_names.py
ok  All:  37.22% fuzzy, 30.65% matched, 13.52% linked (13203 / 28465 functions)
ok  target rose: module:Splitter: 26 -> 36 / 307 functions
ok  no asm added
```

Independently of the judge, measured on this tree: `build/G2ME01/Splitter/Splitter.rel` is
`27f0d2cd85fc1ad0ed0682f6bdd5356693090863` - the value `config/G2ME01/config.yml:398` already
records, unchanged - and `cmp`-equal to `orig/G2ME01/files/RelProd/Splitter.rel`; all **86**
modules' sha1s equal config.yml (**0 mismatches**); `main.dol` is still
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`; `python3 tools/audit_rel_claim.py Splitter` prints
`0 claim(s) with a problem` and `preplf 307 text symbols, plf 307, 0 dropped by -strip_partial`;
`tools/check_files_cmake.py` and `tools/check_symbol_names.py` are clean and the probed port link
is unchanged at 850 files, 0 failures.

## What landed

One new `Matching` unit, `MetroidPrime/ScriptObjects/CSplitterRelTwins.cpp`, claiming **.text
0x3678..0x3A8C** - the whole run the item named, ten functions, all 10/10 at 100.00% in
`build/report.json` (`Splitter/MetroidPrime/ScriptObjects/CSplitterRelTwins`, `total=10
matched=10 fuzzy=100.0`). Module `matched_functions` **26 -> 36 / 307**; the module's auto ranges
are now `auto_00_000000FC_text` 45 fns (0xFC..0x3678), `auto_00_00003A8C_text` 86,
`auto_00_000082B0_text` 129, `auto_00_00010778_text` 8 and the three one-function units - the
carve splits the old `auto_00_000000FC_text` (141 fns) into two.

| retail | fn | size | what it is | twin the item gave |
| --- | --- | --- | --- | --- |
| 0x3678 | `fn_75_3678` | 0x28 | `rstl::string::operator==`'s shape | `CIOWinManager.cpp` `__eq__...` |
| 0x36A0 | `fn_75_36A0` | 0x7C | `rstl::string::compare` | `CCredits.cpp` `compare__...` |
| 0x371C | `fn_75_371C` | 0x114 | `internal_compare<const_linear_iterator<char,...>>` | `CCredits.cpp` |
| 0x3830 | `fn_75_3830` | 0x38 | `vector<CJointCollisionDescription>::push_back_unsafe` | `CCollisionActorManager.cpp` |
| 0x3868 | `fn_75_3868` | 0x20 | `rstl::construct<CJC>` forwarder | `main.cpp` `__sys_free` |
| 0x3888 | `fn_75_3888` | 0x28 | `rstl::construct_impl<CJC>` | `CAnimData.cpp` |
| 0x38B0 | *(renamed)* | 0xC0 | `CJointCollisionDescription`'s copy constructor | `CCollisionActorManager.cpp` |
| 0x3970 | `fn_75_3970` | 0x84 | `vector<CJointCollisionDescription>`'s deleting destructor | `CCollisionActorManager.cpp` |
| 0x39F4 | `fn_75_39F4` | 0x38 | `rstl::destroy<It>` forwarder | `main.cpp` |
| 0x3A2C | `fn_75_3A2C` | 0x60 | `rstl::destroy_impl<It>` loop | `CCollisionActorManager.cpp` `fn_801363D4` |

Files: `src/MetroidPrime/ScriptObjects/CSplitterRelTwins.cpp` (new),
`config/G2ME01/rels/Splitter/splits.txt` (the claim), `configure.py:1977` (the `Object(...)` line
inside the existing `Rel("Splitter", ...)` block, plus that block's comment),
`config/G2ME01/rels/Splitter/symbols.txt:67` (one rename), `files.cmake`.

Every instruction of all ten is byte-identical to retail with the relocation targets masked
(the retail run was extracted from `orig/G2ME01/files/RelProd/Splitter.rel` at file offset
`text+0xC4` and compared instruction by instruction with each branch target masked; 10/10). The
nine non-constructor functions are named `extern "C"` free
functions holding the statements of the template/member they came from, which is what makes the
names `symbols.txt` gives them exist for the rest of the module's retail code to call.

## Three things that had to be measured, in the order they blocked

1. **`mw_version="GC/2.7"` is the whole of the first diff, and it is measured, not inherited.**
   Under the module's default **GC/1.3.2** only **5 of the 10** reproduce (`fn_75_3678`,
   `fn_75_3830`, `fn_75_3868`, `fn_75_3888`, `fn_75_3970`); `fn_75_36A0`, `fn_75_371C`, the copy
   constructor, `fn_75_39F4` and `fn_75_3A2C` come out scheduled differently. Under **2.7 all ten
   are exact** with no spelling change. Third module in a row (after `CLumiteRelTail.cpp` and
   `CSandBossRelTail.cpp`) whose tail is 2.7 code - try 2.7 before spending spellings.
2. **The copy constructor has to be a constructor, and it needs an overlay class + one
   `symbols.txt` rename.** mwcceppc rejects an out-of-line definition of
   `CJointCollisionDescription`'s *implicitly declared* copy constructor
   (`object 'CJointCollisionDescription::CJointCollisionDescription(const ...&)' redefined`), so
   the body is written as `CJointCollisionOverlay`'s copy constructor, whose members are the
   class's own types in the class's order. That spelling is the only one that produces retail's
   register allocation (`this` in r30, the argument in r31, `mr r3,r30` returning it) and its
   member-by-member schedule. The emitted symbol is therefore
   `__ct__22CJointCollisionOverlayFRC22CJointCollisionOverlay`, and
   `config/G2ME01/rels/Splitter/symbols.txt:67` renames the module's address 0x38B0 from the dtk
   `fn_75_38B0` to it, exactly as `tools/wire_rel_setup.py` names a module's entry points after
   they are written. Without the rename the link fails: `Failed to find symbol fn_75_38B0 in any
   module` (measured).
3. **The member types decide the copy schedule.** With `char mPivotId/mNextId` (the first
   attempt) the copy is 192 bytes but a different function: the two bytes are read *before* the
   first float and the float copy runs `f0`/`f1`/`f2` three deep, where retail reads the bytes
   after the first float store and alternates `f1`/`f0` two deep. With the class's own `CSegId`
   members it is retail's 48 instructions exactly. That single type change was worth the whole
   function.

## Spellings measured and rejected for 0x38B0 (do not re-try)

- **Mirror class + `new (self) Mirror(*other)`** (1.3.2 and 2.7): the placement-new guard
  (`mr. r30,r3 / beq`) is not in retail, the wrapper came out 0x34 bytes plus an *outlined*
  `__ct__...` copy, and with `#pragma inline_max_size(400)` the ctor did inline but the schedule
  was still wrong.
- **Explicit `s.mX = o.mX` member assignments with placement new for the two class members**:
  49 instructions against retail's 48, two `addic.`/`beq` guards (one per placement new), and the
  roles of `self`/`other` swapped in r31/r30.
- **Free function returning `void*` with `return self;`** instead of a wrapper: same guard and
  schedule problems - the `mr r3,r30` is a constructor's, not a function's.

## What is still in this module (measured, not attempted)

After this change `tools/twin_scan.py --list` gives **49 twin rows in `Splitter/`, every one of
them with a source file** (the item's list was taken on the branch head; re-measured here), none
of them adjacent to the claimed range. The longest remaining contiguous
runs where every function has an exact matched twin are, longest first:
`0x1E38..0x1F08` (4 fns, 280 B), `0x204C..0x20D4` (4, 264 B), `0xEF24..0xEF84` (4, 128 B),
`0x46C..0x4CC` (3, 136 B), `0xA8FC..0xAA30` (3, 364 B) - each is one more carve of the same shape
as this one (`Rel("Splitter", ...)` entry, a `splits.txt` range, a `files.cmake` line), and the
first two are the same `rstl::optional_object`/`CImpactVisorEffect` cluster the twin list shows
elsewhere. Nothing was left at a sub-100% score, so there is no `WALL:` line.

## Caveats

- `tools/unit_fit.sh MetroidPrime/ScriptObjects/CSplitterRelTwins.cpp` reports `.text claimed
  1044 ours 1124 retail 1044 over by 80`, the extra function being one weak
  `__dt__Q24rstl66basic_string<...>Fv` copy that mwcceppc emits alongside a constructor with a
  class-typed member. Nothing references it, **mwldeppc discards it and the module still comes out
  0x11C64 against retail's 0x11C64** - measured by the hash and the `cmp`, not assumed. It is a
  latent hazard of this spelling rather than a live one: the gate re-checks the module sha1 on
  every run.
- `tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CSplitterRelTwins.cpp` prints
  `0 unit(s) checked` - the tool does not resolve a source path out of a `Rel(...)` block (the
  same limitation `progress-rel-head-shrieker.md` records for `flip_test.sh`). The ten
  definitions are written in descending retail order by hand, and the module's sha1 is the
  acceptance test that checks it.
- The two `destroy` functions are hand-written rather than left to `include/rstl/construct.hpp`:
  the header outlines a single 100-byte copy where retail has the 56-byte forwarder and the
  96-byte loop, the same reason `src/MetroidPrime/CCollisionActorManager.cpp` writes
  `fn_8013639C`/`fn_801363D4` by hand.

## NEW

None filed. The remaining twins are the same target (`module:Splitter`) this item already names,
and the runs above need no new unit, module or symbol.
