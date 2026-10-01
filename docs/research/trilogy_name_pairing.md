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

**The pairing tried** (`tools/trilogy_pair_names.py`). A REL refers to the DOL by address and its RSO
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

**Not tried.** Aligning per function instead of per module (split both sides at function
boundaries, pair functions by order and size within the module, then align calls inside each pair)
should recover most of the 690; and pairing the two DOLs directly needs a compiler-independent
function similarity, which nothing here provides yet.
