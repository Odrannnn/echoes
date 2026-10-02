# progress-dup-cplayer (lane 12, wt-mp2-goal-L12)

`kind: progress`, target `main/MetroidPrime/Player/CPlayer`. Two files changed, no `tools/`,
no `build/goal/` (other than this note), no `docs/HANDOFF.md`/`RUNNING_THE_DECOMP.md` edit.

## Result, measured

`build/report.json` clean-tree baseline (**`build/goal/judge/report.base.json`**, 77/228) to after:

| function (retail address) | bytes | before | after | what produced it |
| --- | --- | --- | --- | --- |
| `__ct__20CDamageVulnerabilityFRC20CDamageVulnerability` (0x8001C634) | 92 | 82.61% | **100%** | `include/MetroidPrime/CDamageVulnerability.hpp:95`, `__attribute__((aligned(4)))` on `mChargedBeamIndices` |
| `__ct__Q24rstl61vector<24CWeaponTypeVulnerability,Q24rstl17rmemory_allocator>FRC…` (0x8001C6E4, was `fn_8001C6E4`) | 304 | 0.00%, unpaired | **100%** | `config/G2ME01/symbols.txt:512` rename to the name MWCC already emits |

- `main/MetroidPrime/Player/CPlayer`: **77 -> 79 / 228**.
- Project `matched_functions`: **13123 -> 13125**; linked 6215 -> 6215.
- `python3 tools/report_diff.py <baseline> build/report.json` -> `no regression`
  (the rename is reported as `RENAMED`, same unit, same size, 0.00% -> 100.00%).
- `./tools/goal_check.sh build/goal/item.json` -> **PASS** (gate.sh, counts, symbol names,
  `All: 37.11% fuzzy, 30.55% matched, 13.48% linked (13125 / 28465)`, `target rose: 77 -> 79`,
  `no asm added`). `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.

## 1. `fn_80015B84`'s shape (`__ct__20CDamageVulnerabilityFRC20CDamageVulnerability`)

`fn_80015B84` (0x80015B84, 92 B) and `__ct__20CDamageVulnerabilityFRC20CDamageVulnerability`
(0x8001C634, 92 B) are **the same function, byte for byte** - I diffed the 0x5C words: only the two
`bl` targets differ (`__copy` at 0x80344e38 both; then 0x80015BE0 vs 0x8001C6E4). So the listed
"92 B copy" is retail's *local* duplicate of the copy constructor our compiler already emits under
its weak global name, and matching the named twin is the reachable half. 0x80015B84 itself stays
unpaired (objdiff pairs by name and our object emits one copy; giving both addresses one name buys
nothing).

The 82.61% was a **layout** defect, not a wording one. Measured:

- Retail 0x8001C634 copies 21 bytes with one `__copy`, then `lwz`/`stw` at +0x18 and +0x1C, then
  the vector copy at +0x20 - nothing at +0x15..+0x17.
- Retail `CDamageVulnerability(const CWeaponTypeVulnerability&)` (0x800DBD10) byte-stores
  +0x00..+0x14 and then +0x18..+0x1F - **never +0x15..+0x17**. Those three bytes are padding.
- Our header had `uchar x15[3]` there, so the compiler copied them too: 27 instructions / 108 bytes
  against retail's 23 / 92. `objdump -dr` on our `CPlayer.o` showed the extra
  `__copy(dst+21, src+21, 3)`, exactly the 4 instructions.

The fix keeps the member and every offset and only makes the three bytes padding:
`signed char mChargedBeamIndices[4] __attribute__((aligned(4)))`. `x15` had no other use in
`src/` or `include/` (grep), no initialisation was dropped, `CHECK_SIZEOF` is still 0x30, and the
whole-object byte-store pattern (0x800DBD10) is untouched. Same idiom as
`include/rstl/construction_deferred.hpp:37`.

## 2. `fn_8001C6E4` (item's target #2) - renamed, per `RUNNING_THE_DECOMP.md`

`fn_8001C6E4` is `rstl::vector<CWeaponTypeVulnerability>::vector(const vector&)`. Our `CPlayer.o`
**already emits it byte-exactly** (`nm`: `00002814 00000130 W
__ct__Q24rstl61vector<24CWeaponTypeVulnerability,Q24rstl17rmemory_allocator>FRC…`; disassembly
differs from retail only in the `R_PPC_REL24 allocate__Q24rstl17rmemory_allocatorFi` call).
Retail's symbol table has no name for it, so dtk called it `fn_8001C6E4` and objdiff, which pairs by
name, scored it 0%. Fixed with the documented rename ("Pairing a function the retail symbol table
has no name for", `docs/RUNNING_THE_DECOMP.md`), replacing the `fn_` line - never beside it - with
the exact name `powerpc-eabi-nm` prints. `check_symbol_names.py` passes.

## What still blocks the other four listed targets (measured, not recalled)

Every remaining one is emitted only by a large `CPlayer` function that is still a structure-pass
stub. Caller map from a `bl`-scan of retail `.text` (scripts in the run, addresses from
`symbols.txt`):

| listed target | size | emitted by (retail caller) | caller now |
| --- | --- | --- | --- |
| `__ct__10CModelDataFRC10CModelData` 0x80018FBC | 316 B | `CPlayer::Update` 0x800182C8 | 2532 B, 0.16% |
| `fn_8001ACC4` 0x8001ACC4 | 148 B | `__dt__7CPlayerFv` 0x8001A500 | 1056 B, 67.86% |
| `__ct__14CRayCastResultFRC14CRayCastResult` 0x80015CAC | 92 B | `CPlayer::AcceptScriptMsg` 0x80014EB8 | 3276 B, 0.12% |
| `__as__Q24rstl61vector<24CWeaponTypeVulnerability,…>FRC…` 0x80015BE0 | 192 B | `fn_80015B84`, 3 calls from `AcceptScriptMsg` | same |

`AcceptScriptMsg` is the one that would unlock four of the six in a single item (`fn_80015B84`,
`fn_80015CA0` and `fn_80015D08` are its local copies too); `fn_8001CDC8` (700 B) is the only caller
of `__ct__14CRayCastResultFQ214CRayCastResult8EInvalid` (0x8001D084). I did not file these as
`NEW:` - they are the same unit and the same reason this item already carries.

The first listed target has one more trap beyond `CPlayer::Update`: `CModelData`'s copy constructor
is declared out of line (`CModelData(const CModelData& other);`) and defined only in
`src/MetroidPrime/CModelDataCopyCtor.cpp`, which is in `files.cmake` but **not** in `configure.py`,
so no matching-build TU emits the symbol today; making it inline and emitting it needs the flags
byte back as a struct (its header note measures the 12 extra instructions) plus a use in a real
`CPlayer` function.

## Files

- `include/MetroidPrime/CDamageVulnerability.hpp:88-95` (comment + aligned member; was
  `uchar x15[3];` at 88).
- `config/G2ME01/symbols.txt:512` (`fn_8001C6E4` -> the vector copy constructor's mangled name).
