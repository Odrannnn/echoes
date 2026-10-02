# Pairing Trilogy (R3ME01) names with G2ME01 addresses

Measured 2026-10-02 with the Trilogy USA disc's MP2 files extracted to `orig/R3ME01/files/`
(`rs5mp2_p.dol` and `MP2/RSO/Production/`; both sha1s equal `config/R3ME01/config.yml`).

**What the disc has.** `MP2/RSO/Production/selfile.sel` exports 1,975 real mangled names for the
Trilogy MP2 executable. The 85 RSO modules import 1,879 of them. 84 modules have a GameCube REL of
the same name (`IngSwarm` and `WallCrawlerSwarm` exist only on GameCube, `rhbm` only on Wii).

**What we already had.** 1,189 of those 1,879 names are spelled identically in
`config/G2ME01/symbols.txt`. About 690 are not placed, which is the whole prize.

**What does not work.** Trilogy's code is compiled by a Wii compiler, so nothing byte-matches and the
disc cannot confirm or supply a match. It is a naming and signature source only.

**The first pairing, per module** (replaced in the tool by the per-function one below). A REL refers to the DOL by address and its RSO
twin by name, so the two relocation streams of a module pair were aligned with the names both sides
share as anchors:

- `R_PPC_ADDR32` (vtables, pointer tables) aligns well, ratio about 0.9 per module.
- `R_PPC_REL24` (calls) aligns badly, mean ratio 0.19: the two compilers inline differently and the
  call streams drift apart.

Result: 80 proposals, 69 unanimous and unique, 35 with two or more votes; 39 would name a `fn_`
and 30 disagree with a name we have. The disagreements are mixed. Some look right and matter for
signatures (`LdrToEntityInfo__FR11CEntityInfo...` non-const, `LoadDolphinSpareTexture...PCv`,
`SetEnergyBarActorInfo__13CStateManagerF9TUniqueIdfi`, `GetLookAtPosition__6CActorCF...` with 92
votes). Some are plainly wrong (`White__6CColorFv` -> a `CExplosion` constructor, on one vote).
**Treat single-vote proposals as noise and check every rename against retail's registers.**

**Per function (supersedes the "not tried" note that stood here, same day).** The tool now splits
both modules into functions, turns each into the list of DOL symbols it relocates against, and pairs
functions by how many they share. Two things were learned on the way:

- **Function order is not preserved** between the GameCube REL and the Wii RSO, and the Wii modules
  have fewer functions. An order-preserving alignment found 389 pairs; unordered greedy matching on
  shared symbols (similarity >= 0.5, at least three relocations, ties skipped) finds 2,570.
- The RSO has no function symbols and dtk will not load it as a module, so boundaries come from a
  heuristic (internal relocation targets, and a `blr`/`b` past the furthest forward branch).

Inside a pair, a position where the REL has an address and the RSO a name is a vote. A name is
accepted with two or more votes, 90% agreement, one address only, and not already used elsewhere in
`symbols.txt`; accepted names feed the next round (181 + 24 + 3).

Result: **208 accepted** (the module-level version gave 35 at the same two-vote bar), listed in
`trilogy_name_proposals.tsv`: 108 name a `fn_`/`lbl_`, 100 rename something. Left out of the list:
17 whose name we already have at another address, 115 on one vote or split votes.

Reading the list:

- 47 of the renames are `TCastToPtr<T>__FP7CEntity_P<T>`, the Trilogy spelling of a name we have.
- Many are plain improvements (`LoadTypedef*`, `LdrTo*`, `IsNormalizable__9CVector3fCFv` on 123
  votes) or const/reference corrections that change the signature we should write.
- Some are **Trilogy-only and wrong for GameCube**: `__dt__13CRSOFileTokenFv` for our
  `CRELFileToken`, the `CHUDMemoParms` constructor with a `CTrilogyAudioManager` argument, extra
  enum arguments on `CCollisionActorManager::Add/RemoveMaterialList`. Take the name, not the
  parameter list, unless retail's registers agree.
- At least one is plainly wrong: `__nw__FUlPCcPCc` -> `LoadPirateRagDoll...` (4 of 4 votes; a
  mis-split Wii function).

**So: candidates, not facts.** Check each against retail's argument registers before renaming, as
with any other name source.

**Verified and applied (2026-10-02).** `tools/verify_trilogy_names.py` parses each proposed mangling
into the registers it needs and compares them with the registers retail reads before writing them
(it can only refute a name, never prove one). Of the 108 unnamed rows: 73 agree outright; 25 differ
and every one was read by hand - all are explained by a hidden `sret` pointer in r3 (the name omits
the return type), a static method (no `this`), arguments past r10 on the stack, or a scan that stops
at the first call. **None refuted.** 94 `fn_` names were applied to `config/G2ME01/symbols.txt`
behind the full gate. 14 were held back, with the reason per row in `trilogy_name_hints.tsv`: four
`fn_` are declared in our sources (`fn_800B5FF0`, `fn_8023ACFC`, `fn_800EA17C`, `fn_80257A14` - rename
source and symbol together), `DisableControls` carries a Trilogy-only enum, and nine are data labels
(`lbl_8041AAB8` is `extern "C"` in about twenty REL sources). Lesson: a name from this source says
nothing about the return type, so a register check cannot see `sret`; the signature still has to be
written from retail.

**Still not tried.** Pairing the two DOLs directly (most of the remaining ~480 names live in
DOL-to-DOL calls that no module imports on the GameCube side) needs a compiler-independent function
similarity, which nothing here provides yet.

## 2026-10-02 - leftover renames applied in groups

Each group was followed by `./tools/gate.sh`. A symbol rename is safe only when no compiled unit
defines or calls the old name; otherwise the per-function diff shows GONE entries or the link fails
with undefined / multiply-defined symbols.

- **A-functions: 3 applied, 1 held.** `fn_8023ACFC`, `fn_800B5FF0` and `fn_800EA17C` renamed together
  with their `extern "C"` declarations and the `PortLinkStubs.cpp` asm name. `fn_80257A14` held: the
  new name collides with the inline `GetTriangle` already defined in `CCollisionSurface.o`
  (multiply-defined). `DisableControls` left alone (Trilogy-only enum).
- **A-data: 7 applied, 2 held** (the two unconfirmed labels), with ~40 `ScriptObjects` sources plus
  `CWorldShadow.cpp`, `mainHead.cpp` and `CPowerBeam.cpp` renamed in step.
- **B: 17 applied, 83 held.** Only renames whose old name is not emitted by any built object went
  through (the Ldr/Load loaders, vulnerability helpers, `gpTweakPlayerControlsA` ->
  `gpTweakPlayerControlExpert`). The rest are held because the old mangling is produced by our
  source (the `TCastToPtr` instantiations, member signatures), because the name is Trilogy-only, or
  because it needs a wide source rename (`gpTweakContents`). The held rows and reasons are in
  `trilogy_name_hints.tsv`; the per-row reasons there are grouped, not individually re-verified.
- `port_link_gap_list.md` was regenerated and the counts in `port_link_gap.md` updated for the renamed loaders.
