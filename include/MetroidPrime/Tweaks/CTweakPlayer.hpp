#ifndef _CTWEAKPLAYER
#define _CTWEAKPLAYER

struct SLdrTweakPlayer;

/**
 * Retail's `CTweakPlayer` is **four bytes: one pointer**, and it is not a class
 * with members of its own. `REL_CreateTweakGlobals` (Tweaks.rel .text:0x508)
 * allocates it with `new[4]` and stores `&gpTweakContents->TweakPlayer` in word
 * 0; the destructor the DOL registers for the slot (0x800329AC, one address for
 * both player slots) is a free-the-pointer-then-free-the-cell pair, so word 0
 * is a pointer to free and not something with a destructor.
 * `docs/research/tweak_globals.md` has the store census.
 *
 * The proof that word 0 is used *as* `this` is retail's accessors, which are
 * three instructions each and dereference it:
 *
 *     802184cc <GetRightAnalogMax__12CTweakPlayerFv>:
 *     802184cc:  lwz   r3,0(r3)
 *     802184d0:  lfs   f1,428(r3)        ; 428 = 0x1AC
 *     802184d4:  blr
 *
 * so `this` is the 4-byte cell and the float is at a **retail** offset in
 * `SLdrTweakPlayer`. All five are named in `config/G2ME01/symbols.txt` and all
 * five are 12 bytes:
 *
 *   | accessor                          | retail   | reads                              |
 *   | --------------------------------- | -------- | ---------------------------------- |
 *   | `GetLeftAnalogMax`   0x802184D8   | +0x1A8   | `misc.leftAnalogMax`               |
 *   | `GetRightAnalogMax`  0x802184CC   | +0x1AC   | `misc.rightAnalogMax`              |
 *   | `GetVariaSuitDamageReduction` 0x80217D48 | +0x370 | `suitDamageReduction.varia`  |
 *   | `GetDarkSuitDamageReduction`  0x80217D3C   | +0x374 | `suitDamageReduction.dark`   |
 *   | `GetLightSuitDamageReduction` 0x80217D30   | +0x378 | `suitDamageReduction.light`  |
 *
 * **This tree's `SLdrTweakPlayer` already reproduces those offsets exactly** -
 * `sizeof` 0x37C and `suitDamageReduction` at +0x370, measured with mwcceppc
 * (32-bit), not with a host compiler. The accessors are therefore written
 * against *named members*, not offsets, and they are byte-correct in the
 * matching build. `docs/research/tweak_player.md` has the measurement, the
 * 64-bit-probe trap that produced a wrong answer first, and what is still wrong.
 *
 * The bodies are out of line - defining them in this header would inline them
 * into `MetroidPrime/main.cpp`, which is a `Matching` unit - and they live in
 * `src/MetroidPrime/PortGlobals.cpp`, which `configure.py` does not claim at
 * all, so nothing in the matching build moves.
 */
class CTweakPlayer {
public:
  /** Word 0 of retail's 4-byte cell: a `SLdrTweakPlayer*` used as `this`. */
  SLdrTweakPlayer* mTweak;

  float GetLeftAnalogMax();
  float GetRightAnalogMax();
  float GetVariaSuitDamageReduction();
  float GetDarkSuitDamageReduction();
  float GetLightSuitDamageReduction();
};

extern CTweakPlayer* gpTweakPlayerA;
extern CTweakPlayer* gpTweakPlayerB;

#endif // _CTWEAKPLAYER
