/* `fn_80006954` and `fn_80008B60`: the two functions retail calls on the frame-time history,
 * written for the port because the frame loop cannot get past them without them.
 *
 * ## Why these are here and not in a unit
 *
 * Neither function is an unclaimed dtk range, so neither can be given a `splits.txt` claim of
 * its own without cutting someone else's - the same situation `src/MetroidPrime/Carve800069AC.c`
 * documents, and for the same reason the answer is `files.cmake` and not `configure.py`:
 *
 *   fn_80006954  0x80006954-0x800069AC   inside MetroidPrime/main.cpp's    .text 0x800053B8-0x80006B38
 *   fn_80008B60  0x80008B60-0x80008C28   inside MetroidPrime/mainTail.cpp's .text 0x80008680-0x80009880
 *
 * (`sed -n '40,41p;55,56p' config/G2ME01/splits.txt`, and `build/binutils/powerpc-eabi-nm
 * build/G2ME01/obj/MetroidPrime/mainTail.o | grep 80008B60` -> `000004e0 T fn_80008B60`.) Two
 * different units' claims, so a carve here is two claim cuts rather than one, and each cut is
 * another lane's file. **This file is therefore port-only**: it is in `files.cmake`, it is
 * compiled by the host build and checked by it, and it claims nothing in the DOL - so the DOL
 * build and `main.dol`'s sha1 cannot move because of it. A unit for `fn_80006954` wants the
 * `mainsplit` cut; a unit for `fn_80008B60` wants **mainTail's** claim cut. Whoever cuts either
 * can move a body from here into it without changing the body.
 *
 * Being C and not C++ is what lets the bodies be retail's: the loops and the `fdivs`/`fmuls`
 * tail below are what mwcceppc emits for these expressions at `-lang=c`, which is the language
 * `build.ninja` gives the `.c` rule, and measured with `tools/probe_cc.sh`'s flag list plus
 * `-lang=c` (`Carve800069AC.c`'s header calls that script `tools/probe_c.sh`; it is not in the
 * tree, so the flag was added by hand). Compiled as C++ the two symbols mangle to
 * `fn_80006954__FP15SFrameTimeTotalPC17SFrameTimeHistory` and `fn_80008B60__FPCfi` (measured), and
 * objdiff pairs by name, so a unit of this would score 0/0 with a right file. It also keeps the
 * two shapes local, the way `src/MetroidPrime/Carve800069AC.c` does.
 */

/* `CMain::SFrameTimeHistory`, duplicated locally. `tools/sizeprobe_cmain.cpp` measures it at
 * 0x14 = 20 bytes with the same compiler, and retail writes +0x00 (the count) and +0x04..+0x10
 * (the four floats). `include/MetroidPrime/CMain.hpp` carries the same struct, and the two are
 * kept apart on purpose: this file is C, it cannot include the C++ header, and the local copy
 * is what makes the body immune to anything that happens to `CMain.hpp` later. */
struct SFrameTimeHistory {
  int count;
  float values[4];
};

/* The 8-byte stack local retail passes in r3 - `addi r3,r1,32` at 0x8000610C and
 * `addi r3,r1,24` at 0x8000622C - and reads back as `lfs f0,32(r1)` / `lfs f0,24(r1)`
 * (0x80006118, 0x80006238). `fn_80006954` fills +0 with the float and +4 with the flag, and
 * `CMain::RsMain` stores only +0, to `CMain`+0x40 and `CMain`+0x44 respectively. */
struct SFrameTimeTotal {
  float value;
  unsigned char valid;
};

/* Retail 0x80008B60, 0xC8 = 200 bytes, **50 instructions, measured against retail's 50**.
 *
 * `r3 -> const float* values`, `r4 -> int count`, returns the **mean** in f1.
 *
 *   sum = values[0];  while (p < end) sum += *p;   // 0x80008B70-0x80008BF4
 *   return sum * (1.0f / count);                  // 0x80008BF8-0x80008C24
 *
 * The accumulation is the 8x-unrolled `srwi. r0,r3,3 / mtctr` loop with the `andi. r3,r3,7`
 * remainder and a 1x tail, which is what mwcc emits for a counted float sum written as a
 * pointer walk - the same sum as an index loop measures 58 instructions in **232** bytes
 * against retail's 50 in 200.
 *
 * The tail is a **mean, not a sum**, and that is worth writing down because a subtraction of
 * `200.0f` out of the `1.0f / count` is very easy to read into the disassembly and is wrong.
 * What is actually there is mwcc's integer-to-double bias, and every part of it is accounted
 * for: `xoris r3,r4,32768` + `lis r0,17200` build the **8-byte** value
 * `0x43300000_80000000 + count` in a stack slot, `lfd f0,8(r1)` reads it back, and
 * `lfd f1,-32664(r2)` is the matching bias `0x8041A428` = `0x43300000_80000000` =
 * 2^52 + 2^31 = 4503601774854144.0. `fsubs f0,f0,f1` (single precision, so no rounding)
 * subtracts one from the other and leaves exactly `count`. **0x8041A428 is not 200.0** -
 * reading `43300000 80000000` as an int-to-double encoding of a small integer instead of as
 * 2^52 + 2^31 produces a `count - 200.0f` body, which mwcceppc then compiles to *two*
 * subtractions of 200 (one `fsub` against the double and one against the float) and to 232
 * bytes. The other constant is `lfs f2,-32740(r2)` = 0x8041A3DC = `3F800000` = 1.0f, and
 * `_SDA2_BASE_` is 0x804223C0 (`tools/sda.py`); both addresses are confirmed by a second pair
 * in `fn_800597D8` (`-31336` -> 0x8041A958 = `3F4CCCCD` = 0.8f). So the tail is
 * `1.0f / count` in single precision (`fdivs f0,f2,f0`) and the result is the sum times it
 * (`fmuls f1,f3,f0`).
 *
 * `fn_800597D8` (0x80059928-0x8005993C) is the only consumer of the two floats this produces
 * - it does `lfs f2,64(r3) ; lfs f1,68(r3) ; fadds ; fcmpo` against that 0.8f - so `CMain`
 * +0x40 and +0x44 hold means. It still has no body in the tree; see
 * `build/goal/notes/port-boot-cmain-rsmain-384eef8.md` for the queued item.
 */
float fn_80008B60(const float* v, int count) {
  const float* end = v + count;
  const float* p = v + 1;
  float sum = v[0];

  while (p < end) {
    sum += *p;
    p++;
  }

  return sum * (1.0f / count);
}

/* Retail 0x80006954, 0x58 = 88 bytes: **22 instructions and 88 bytes, against retail's 22 and
 * 88**. Three of them sit at a different offset - the `stfs`/`li`/`stb` shuffle below - and the
 * fourth difference is the `bl`'s displacement, which is a relocation in an unlinked object.
 *
 *   r3 -> SFrameTimeTotal* out,  r4 -> const SFrameTimeHistory* h
 *
 *   if (h->count == 0) { out->valid = 0; return; }   // 0x8000696C-0x8000697C
 *   out->value = fn_80008B60(h->values, h->count);   // 0x80006980-0x80006994
 *   out->valid = 1;                                  // 0x8000698C-0x80006990
 *
 * The `cmpwi r0,0 / bne` is `count != 0` and the `b` at 0x8000697C is the early return, so
 * **the count==0 path does not call `fn_80008B60` and does not write `out->value`** - which is
 * why the caller reads an 8-byte local that retail itself leaves half-written on that path,
 * and why this body must not "helpfully" clear it.
 *
 * **The two assignments are in that order and not the other way round.** Retail stores the
 * value at +0, then `li r0,1`, then the flag at +4; this body with the flag first produces the
 * same 22 instructions in the same 88 bytes but five of them at a different offset instead of
 * three, because mwcc hoists `li r0,1` above the call. Nothing semantic hangs on it - this is
 * not a `Matching` unit - but the order that is retail's is the one written here.
 */
void fn_80006954(struct SFrameTimeTotal* out, const struct SFrameTimeHistory* h) {
  if (h->count == 0) {
    out->valid = 0;
    return;
  }

  out->value = fn_80008B60(h->values, h->count);
  out->valid = 1;
}
