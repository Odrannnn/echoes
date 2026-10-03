// Carved out of an unclaimed dtk `auto_*` range by lane `5` (goal item `carve-8021293c`).
// Every number here is measured: the address and size come from `config/G2ME01/symbols.txt`,
// the instructions are the ones dtk itself emitted into
// `build/G2ME01/asm/auto_03_802126B8_text.s`, and the body below is the C those bytes are
// the compilation of.
//
// .text 0x8021293C..0x80212944, 0x8 = 8 bytes, 1 function:
//
//   fn_8021293C    0x8021293C  0x8    lwz r3, 0x68(r3) ; blr
//
// `fn_8021293C` is the *last* function of its dtk range: `auto_03_802126B8_text` spans
// 0x802126B8..0x80212944 and holds exactly two, `fn_802126B8` (0x802126B8, 0x284) and this one.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits
// function definitions in *reverse* source order and mwldeppc keeps the object `.text` order
// verbatim, so an ascending file is a permuted `.text` - 100.00% per function,
// `tools/unit_fit.sh` still saying "fits", the link still succeeding, and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  With one function the order is trivially right; the rule is
// recorded because the next run to extend this claim has two.
//
// Retail names none of these.  `symbols.txt:8560` carries the `fn_<addr>` placeholder
// (`fn_8021293C = .text:0x8021293C; // type:function size:0x8`) and this file reproduces that
// symbol verbatim, so the definition has to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>Pv` and objdiff would pair nothing.  That is also why the unit is a `.c`
// rather than a `.cpp`.
//
// **What the load is.**  Exactly one caller, in `fn_802114A4` (`auto_03_80210990_text`,
// 0x802116C8): it reloads the receiver from its own stack frame (`lwz r3, 0x2c(r1)` at
// 0x802116C4), calls, and moves the result straight into the second argument register
// (`mr r0, r3` at 0x802116CC, `lwz r3, 0x150c(r30)` at 0x802116D0, `mr r4, r0` at 0x802116D4)
// of `GetItemCapacity__12CPlayerStateCFQ212CPlayerState9EItemType`.  No arithmetic, no mask, no
// widening: the 32-bit word at `+0x68` is the return value verbatim.
//
// **The struct is by offset, not by a class.**  Retail gives no name for the receiver here, and
// the caller above is itself inside an unclaimed `auto_*` range, so nothing in this tree
// identifies it.  Only the one displacement the instruction uses is asserted, in a local struct
// of exactly that width - the same treatment `src/MetroidPrime/ScriptObjects/Carve80210978.c`
// gives its `CScanTree` head and `src/MetroidPrime/Tweaks/Carve80216D2C.c` gives `CTweakGame`.
// Nothing at or before `+0x64` is claimed and nothing else is read.
//
// **The padding is load-bearing, not decoration.**  A lone member *named* `x68` sits at struct
// offset 0, so the first spelling of this unit emitted `lwz r3, 0x0(r3)`: objdiff reported the
// unit `"complete": true`, `complete_code` 8/8 and 99.50% fuzzy, and the one wrong byte is
// visible only in `cmp` against the DOL - one byte, `0x68` to `0x00` at the instruction's
// displacement.  `tools/carve_diff.sh` and `tools/flip_test.sh` both reject it; **objdiff's own
// summary does not**, so neither `complete` nor `complete_code_percent` is a licence to flip.
//
// **The twin, and what it is evidence for.**  Same two instructions, differing only in the
// class: the matched `int CElementGen::GetEmitterTime() const { return mCurFrame; }` at
// `src/MetroidPrime/Player/CMorphBall.cpp:119` is retail 0x800CA558, `lwz r3, 104(r3)` /
// `blr`, in a `Matching` unit - and `mCurFrame` is also at `+0x68`
// (`include/Kyoto/Particles/CElementGen.hpp:198`).  Identical shape, identical displacement,
// different receiver; that says the two-instruction accessor is what retail emitted here.  It is
// evidence for the shape, not a result of this one, and nothing in this file claims the word at
// `+0x68` is anything in particular.
//
// **Port.**  Nothing in `src/` or `include/` names `fn_8021293C` by symbol: its one caller is
// inside the unclaimed dtk range `auto_03_80210990_text`, which has no `Object(...)` line in
// `configure.py`, so no `TARGET_PC` arm is needed and the body below is the DOL's, offset and
// all.  `grep -rn fn_8021293C src/ include/` on this tree is empty, so there is also no
// `PortLinkStubs.cpp` duplicate to delete (`docs/RUNNING_THE_DECOMP.md`, "The carve vein", 3).
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk
// `dol split` fails with "Cyclic dependency ... link order"), and because the function below it
// in the range is not part of the claim: `fn_802126B8` is 0x284 bytes and is still unwritten.
// Above, the 8-byte `fn_80212944` is already landed as
// `MetroidPrime/ScriptObjects/Carve80212944.c` (0x80212944..0x8021294C), so this claim abuts an
// existing unit's `.text` start rather than sitting inside one - the cycle case in the carve
// notes is a carve that *starts* where another unit ends, and this one does not.
//
// The directory is retail's own, taken from the nearest claimed range: the closest claim below
// 0x8021293C is `MetroidPrime/ScriptObjects/Carve802126B0.c` (0x802126B0..0x802126B8), and
// before this file that neighbourhood traces back to
// `MetroidPrime/ScriptObjects/CScanTreeInventory.cpp` (0x8020EBF0..0x8020EE18), 0x3D44 bytes
// below this address.  For an anonymous function that is the only evidence there is, and it
// beats a lane picking the directory it happened to own.
typedef struct {
  unsigned char pad_00[0x68]; /* 0x00..0x67 - unread, unnamed, only there to hold +0x68 */
  int word_68;                /* 0x68 - the word fn_8021293C loads and returns */
} SCarve8021293CHead;

int fn_8021293C(const void* self) { return ((const SCarve8021293CHead*)self)->word_68; }
