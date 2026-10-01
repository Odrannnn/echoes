# port-boot-stub-fn-80145acc-9f2d849 — `fn_80145ACC`

`kind: port`, target `fn_80145ACC`. **`tools/goal_check.sh build/goal/item.json` → PASS**
(full gate, `matched 12088 -> 12088`, `linked 5849 -> 5849`, `All:` unchanged, port link
290 undefined / 0 duplicates, DOL sha1 `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, 86 RELs
matching `config.yml`).

## What I did

`fn_80145ACC` (retail 0x80145ACC, `size:0xC4`) is the option map's set-if-absent, and
`fn_80145C98` calls it once per row for the eleven defaults. It was undefined in the port link
and was the only `reachstub` for it.

The decompilation already has a body -
`src/MetroidPrime/Player/CPersistentOptionsMapInsert.cpp` - but as the item's reason says it
cannot be listed: it is `NonMatching` at 71.37% and relocates against `fn_80146338` (the rbtree
node insert), which nothing implements, so listing it adds one undefined to close none.

So the port gets its own body in a new port-only file,
**`src/MetroidPrime/PortCPersistentOptionsMap.cpp`**, listed in `files.cmake`. It is not in
`configure.py`, so `main.dol` cannot move because of it.

## The finding that made it writable: the option map **is** `mVariables`

`CPersistentOptions` is `CGameStateEnvVarManager` + `mCinematicStates` (0x10) + `mSaveIdx` (4),
and `CHECK_SIZEOF(CPersistentOptions, 0x2c)` = 0x18 + 0x10 + 4. **0x2C leaves no room for a
second map.** Four independent measurements agree that the map retail's `fn_80145ACC` writes at
`self + 4` is the inherited `CGameStateEnvVarManager::mVariables`, the same one `AddVariable` and
`FindEnvironmentVariable` use:

- `fn_80145ACC` searches and inserts at `self + 4` (0x80145AF4/0x80145AFC);
- `fn_80146154` (`CPersistentOptionsCtor.cpp`, `Matching`) zeroes exactly `+0x08`, `+0x0C`,
  `+0x10`, `+0x14` and nothing else. For a map at `+0x04` that is count (`+0x08`) plus the
  header's three words (`+0x0C/+0x10/+0x14`). A map at `+0x00` would leave its count at `+0x04`
  unzeroed and overshoot by one word into `mCinematicStates`;
- `include/MetroidPrime/Player/CPersistentOptionsMap.hpp`'s `SMap` is `CHECK_SIZEOF(SMap, 0x18)`
  with `x14_unk` last, so it ends at `self + 0x1C` - the base class's end;
- the host mirror's size is asserted: `SEnvVarManagerMirror` is 0x28, which is
  `sizeof(CGameStateEnvVarManager)` measured with g++ under the port's own flags (4-byte scope
  word, then the 8-aligned 0x14 map).

The eleven rows therefore land in the environment-variable map, which is where retail puts them.
The two value types are the same twelve bytes with the same meaning - `SPersistentOptionsValue`
is `{lo, hi, value}` and `CEnvironmentVariable` is `{mMin, mMax, mValue}`, both three `int`s,
both `CHECK_SIZEOF` 0xC, and both clamping the third word into `[first, second]` (retail's clamp
is `fn_801461AC` from `fn_801462DC`). Every one of the eleven rows has `lo == 0` and a default
inside the range, so the clamp is a no-op for all of them.

`mVariables` is private and this file is not that class, so the two-member class is mirrored
locally rather than reopening the class in a header four `Matching` units include - the same
convention as `SFirst1C` in `CPersistentOptionsCtor.cpp`, `SGameStateVarTree` in `CGameState.cpp`
and `SMap` in `CPersistentOptionsMap.hpp`.

## The reach stub was retired, deliberately

`src/MetroidPrime/PortReachStubs.cpp` had `reachstub_548` for this symbol. That file is compiled
only under `-DMP_BOOT_STUBS=ON`, which only `tools/boot_probe.sh` passes, so a real definition
and the alias are a duplicate the boot probe cannot see and the gate's `port link dups` step does
not cover. Retired by hand, with the reason in the file, exactly as the file's own header
describes for `AllocateRenderer` on 2026-09-26. **It is not in
`docs/research/boot_path_reachable.tsv`**, so `tools/gen_link_stubs.py` will not put it back;
`tools/restub_reach.py` had appended it from a failed probe link.

## Measured

| | before | after |
| --- | --- | --- |
| port undefined | 291 | **290** |
| duplicates | 0 | 0 |
| probe | 747 files | 748 files, 0 failures, LINKED |
| `matched` / `linked` | 12088 / 5849 | 12088 / 5849 (unchanged) |

Set difference of the two undefined lists, both directions: resolved `fn_80145ACC` and nothing
else; newly undefined nothing. (`LoadAreaAttributes`, `mp_cswarmbasics` and
`mp_cswarmbasics_exit` appear in the fresh `link_undefined.txt` and are in
`docs/research/port_link_baseline.txt` at lines 219/295/296; they are absent from
`build/goal/judge/undef.base.txt` only because that list was taken before those were opened, and
the net count still fell 291 → 290.)

## Also changed, and why

- `docs/research/port_link_gap_list.md` - regenerated with `tools/link_gap.py --write-list`, which
  the gate requires: the tool fails on a listed symbol that is no longer missing. Two lines
  changed, the `fn_80145ACC` entry and its group's count 57 → 56.
- `docs/research/port_link_gap.md` - the per-group count row `unmangled: fn_/lbl_/globals`, 57 → 56,
  because `tools/check_docs_claims.py` derives it from the generated list.
- `docs/HANDOFF.md`, `docs/RUNNING_THE_DECOMP.md` - **machine-written**, by `gate.sh`'s
  `MP_GATE_DOCS_WRITE=1 --write`: the state block's percentages, the probe file count 747 → 748.

## What this does not do

`fn_80146338` (retail 0x80146338, `size:0x1B8`) - the rbtree node insert - is still unimplemented,
so `CPersistentOptionsMapInsert.cpp` is still unlistable and still `NonMatching` at 71.37%. That
is the decompilation's remaining work and it is unchanged by this item; the port simply no longer
needs the symbol.

`CGameStateEnvVarManager::LoadFields()` is also still undefined in the port link
(`build/goal/undef.base.txt:71`) and `SPersistentOptionsValue::SPersistentOptionsValue(int,int,int)`
(`:229`). Neither is this item's target, and neither is called by the body I added: it constructs
a `CEnvironmentVariable`, whose three-argument constructor is already defined in
`CGameState.cpp`. Both remain reachable for a later `port` item.

No `NEW:` line is filed. The wall here (`fn_80146338`) does not raise a count on its own - it is
what keeps `CPersistentOptionsMapInsert.cpp` from flipping, and `CPersistentOptionsMapLookup.cpp`
(one instruction of scheduling out in each of its two functions) is what keeps *that* from
flipping, so a `NEW:` for either would be a spelling wall rather than work whose success raises a
count.