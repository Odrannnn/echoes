// CIngBoostBallGuardian1056C.cpp - IngBoostBallGuardian's (module 30) whole-class copy
// assignment, `.text` 0x1056C..0x10694: one function, 0x128 = 296 bytes.
//
//   0x1056C fn_30_1056C 0x128  a call at +0x00, a call at +0x29C, three triple assignments,
//                                one byte, `mr r3,r30`
//
// **It is the function `fn_30_10694` (the unit behind it in the module) is the callee of**, which
// is what makes it claimable: retail emits an `R_PPC_REL24` to `fn_30_10694` at object offset
// 0x32A8 of `build/G2ME01/IngBoostBallGuardian/obj/auto_00_0000D2E0_text.o` (the object that
// begins at module `.text` 0xD2E0, so object offset + 0xD2E0 is the module offset), and
// `fn_30_10694` is a unit of its own now - `CIngBoostBallGuardian10694.cpp`, which in turn is the
// caller of the 10B90 and 108D4 units. The second call is to `fn_30_AD4`, at object offset 0x32B4,
// which stays an `extern "C"` declaration: it is at `.text` 0xAD4 with size 0x5C
// (`config/G2ME01/rels/IngBoostBallGuardian/symbols.txt:27`) and dtk's own objects already name it,
// so the module still links with it undefined in ours.
//
// **The class is larger than the sub-object `fn_30_10694` copies, and that is what this function
// shows.** It runs the base copy at +0, then the record copy at +0x29C, then copies its own tail -
// so it is the whole class's copy assignment and `fn_30_10694` is its base sub-object's, which is
// why both are spelled `self`/`other` with a pointer return.
//
// **The tail's layout is retail's and it is load-bearing.** Twenty-eight loads off r31 (`other`)
// and twenty-eight stores into r30 (`self`) from +0x2CC to +0x320, one member per instruction pair
// and nothing narrower than a byte, and the offsets and the load/store types agree one for one. The
// per-member type sequence is
//
//   W F F F F H H H B | W F F F F H H H B | W F F F F H H H B B
//
// at 0x2CC, 0x2D0, ... , 0x320, i.e. **three elements of a 0x1C stride** - `int`, four `float`s,
// three `unsigned short`s and an `unsigned char`, 0x1C with the 27 bytes of members padded to the
// element's 4-byte alignment - **plus one trailing byte at +0x320**, which is where the third
// repeat's would-be `int` is a `lbz`/`stb` instead. So the tail is a `RelTriple28 t[3]` and a
// `unsigned char`, 0x55 bytes, not 28 loose members. Modelling the stride is what removes the
// question of whether +0x320 needs a pad member.
//
// **The body is one assignment per triple**, and this is where `CIngBoostBallGuardian108D4.cpp`'s
// lesson applies and where its scope matters: at 87 members the flat member-by-member spelling was
// 13 bytes of 700 wrong, while one assignment per 12-byte triple was byte-exact. At nine members
// per element and three elements both spellings work (measured - see the note on this function in
// `docs/goal-notes/progress-rel-ingboostballguardian-1056c.md`), because the fix in 108D4 was the
// nesting, not the count, and here the count is small either way. The array spelling is the one
// kept, because the 0x1C stride it encodes is the thing the disassembly shows.
//
// **The nine members inside a triple must not be array members.** `float f[4]` and
// `unsigned short h[3]` inside the element compile to a *block* copy of the member - four `lwz`/
// `stw` for the floats and one `lwz`/`stw` plus one `lhz`/`sth` for the halfwords - and the object
// comes out 272 bytes against retail's 296. The element therefore spells the four floats and three
// halfwords out one at a time; only the three *elements* are an array. The same trap closes the
// other way: in the flat 28-member spelling the two adjacent bytes at +0x31E and +0x320 pack into
// +0x31E and +0x31F unless an explicit pad is declared between them.
//
// **The base copy is spelled as a call and passes no address arithmetic.** `&self->base` is at
// offset 0, so `fn_30_10694(&self->base, other.base)` compiles to the bare `bl` retail emits at
// 0x32A8 with no `addi` in front of it (measured). The 0x29C record is not at offset 0, and retail
// does emit `addi r3,r30,0x29C` / `addi r4,r31,0x29C` for that call, so `fn_30_AD4` gets the
// explicit address of the member.
//
// **`fn_30_AD4`'s record is only sized, not laid out here**, and that is deliberate: the two
// `addi`s and the call are the only bytes it contributes, so the 0x30-byte record is declared with
// the layout `fn_30_AD4`'s own disassembly shows (0x18 copied by its first callee, two inline
// words, 0x10 copied by its second) to get its size and its 4-byte alignment, and no more.
//
// **`fn_30_10694`'s record types are repeated here identically to
// `CIngBoostBallGuardian10694.cpp`**, and that is checked by measurement rather than by
// inspection: `&self->base` has to land on the same offset for both, and both objects are
// byte-identical to retail, so the two agree by construction.
//
// **mw_version is load-bearing**, with the same per-object override as the three units it calls:
// at the module default GC/1.3.2 this source is still 296 bytes and 74 instructions but **54 of the
// instructions are not retail's** (plain load/store pairs, one live register, source order). At
// GC/2.7 it is byte-identical (GC/2.0, 2.5 and 2.6 agree; GC/3.0a5 differs in 69). Setting the
// version on the `Rel(...)` block instead would recompile the other module-30 units.
//
// **No dead-strip hazard, and that is measured.** `fn_30_1056C` is not in
// `build/G2ME01/IngBoostBallGuardian/ldscript.lcf`'s FORCEACTIVE list, but an `R_PPC_REL24` names
// it at module 0xECEC, inside `fn_30_EC6C`, in both `auto_00_00000000_text.o` and
// `auto_00_0000D2E0_text.o` - one instruction named twice, because dtk's auto units overlap, so
// the two relocation records are one call site and not two. dtk's own objects therefore hold the
// reference and this unit's `.text` survives the link. No `force_active:` entry, no
// `config/G2ME01/config.yml` change and no `symbols.txt` rename: the split claims `.text` only, and
// keeping the `fn_30_*` name matters because dtk's objects name it that way.
//
// **The claim is 0x1056C..0x10694 and not the 0x10530..0x10694 run around it.** `fn_30_10530` in
// front of it is 0x3C bytes and ends exactly at 0x1056C, so the claim spans no unclaimed gap, and
// `fn_30_10694` behind it is already its own unit. `total_functions` stays 28465 because retail's
// function boundaries do not move.
//
// **It returns `self`.** The `mr r3,r30` at 0x32BC puts the destination back in r3 - hoisted to
// just after the first load of the tail, ahead of both calls' clobber of r3 - which is what a copy
// assignment returns, so the return type is the pointer and not `void`.
//
// The member names say what each word is *made of*, not what the class is called; a copy assignment
// says what it is made of and nothing else, and these 296 bytes occur nowhere else in the DOL to
// anchor a name.
//
// In `files.cmake` with an empty host branch, as `CIngBoostBallGuardianBits.cpp` explains.
// Definitions are in descending retail text order (one function).

extern "C" {

#ifdef __MWERKS__

// Retail's 0x48-byte record at +0xF0 and its 0x15C-byte record at +0x140, the two types
// `fn_30_10694` copies. Same layouts as in `CIngBoostBallGuardian10B90.cpp` and
// `CIngBoostBallGuardian108D4.cpp`, repeated here because `RelRecord29C` below embeds them.
struct RelRecord48 {
  int w00;
  int w04;
  int w08;
  int w0C;
  int w10;
  int w14;
  float f18;
  float f1C;
  float f20;
  float f24;
  float f28;
  float f2C;
  float f30;
  float f34;
  float f38;
  int w3C;
  int w40;
  int w44;
};

struct RelTriple {
  float f;
  int w;
  unsigned char b;
};

struct RelRecord15C {
  RelTriple t[29];
};

// The 0x29C-byte sub-object `fn_30_10694` copies, at member offset +0x00 of this class. Same layout
// as in `CIngBoostBallGuardian10694.cpp`; the header comment above is load-bearing on why it is
// repeated rather than shared.
struct RelRecord29C {
  int w00;
  float f04;
  float f08;
  float f0C;
  float f10;
  float f14;
  float f18;
  int w1C;
  int w20;
  int w24;
  int w28;
  int w2C;
  int w30;
  int w34;
  int w38;
  int w3C;
  int w40;
  int w44;
  int w48;
  float f4C;
  int w50;
  float f54;
  float f58;
  float f5C;
  int w60;
  int w64;
  int w68;
  int w6C;
  int w70;
  float f74;
  float f78;
  float f7C;
  float f80;
  float f84;
  int w88;
  float f8C;
  float f90;
  float f94;
  int w98;
  float f9C;
  float fA0;
  float fA4;
  float fA8;
  float fAC;
  int wB0;
  float fB4;
  float fB8;
  float fBC;
  float fC0;
  int wC4;
  int wC8;
  float fCC;
  float fD0;
  float fD4;
  int wD8;
  int wDC;
  int wE0;
  float fE4;
  float fE8;
  float fEC;
  RelRecord48 rec0F0;
  int w138;
  float f13C;
  RelRecord15C rec140;
};

// Retail's 0x30-byte record at +0x29C, the type `fn_30_AD4` copies: 0x18 bytes its own first callee
// copies, two words inline, and 0x10 bytes its second callee copies. Only the size and the 4-byte
// alignment are load-bearing here - this function contributes the two `addi`s and nothing else.
struct RelRecord30 {
  unsigned char sub00[0x18];
  int w18;
  int w1C;
  unsigned char sub20[0x10];
};

// The two callees, by the names dtk's own object gives them. `fn_30_10694` is the base sub-object's
// copy assignment and is declared as such; `fn_30_AD4` is the 0x30-byte record's, it is not claimed
// by this item and stays an external declaration the module already resolves.
void fn_30_10694(RelRecord29C* self, const RelRecord29C& other);
void fn_30_AD4(RelRecord30* self, const RelRecord30& other);

// Retail's 0x1C-byte tail element at +0x2CC: `int`, four `float`s, three `unsigned short`s and one
// `unsigned char` - 27 bytes of members, padded to the element's 4-byte alignment. The nine members
// are spelled out one at a time because an array member compiles to a block copy of it, and only
// the elements themselves are an array; the header comment above is load-bearing on both.
struct RelTriple28 {
  int w;
  float f0;
  float f1;
  float f2;
  float f3;
  unsigned short h0;
  unsigned short h1;
  unsigned short h2;
  unsigned char b;
};

// Retail's 0x55-byte tail at +0x2CC: three of the element above and the one byte at +0x320, which is
// a `lbz`/`stb` where the stride would put the next element's `int`.
struct RelTail55 {
  RelTriple28 t[3];
  unsigned char flag;
};

// Retail's class: the 0x29C-byte base sub-object at +0x00, the 0x30-byte record at +0x29C and the
// 0x55-byte tail at +0x2CC - 0x321 bytes, the last store of this function at +0x320.
struct RelRecord321 {
  RelRecord29C base;
  RelRecord30 rec29C;
  RelTail55 tail;
};

// .text 0x1056C, 0x128 bytes. `self` is the destination in r3 and `other` the source in r4. The two
// records are the calls retail makes - the base one at +0 and the 0x30-byte one at +0x29C - and the
// tail is three element assignments and one byte, which is what `GC/2.7` compiles to retail's
// two-deep schedule, each load issued two instructions ahead of its store. Returns `self`.
RelRecord321* fn_30_1056C(RelRecord321* self, const RelRecord321& other) {
  fn_30_10694(&self->base, other.base);
  fn_30_AD4(&self->rec29C, other.rec29C);
  self->tail.t[0] = other.tail.t[0];
  self->tail.t[1] = other.tail.t[1];
  self->tail.t[2] = other.tail.t[2];
  self->tail.flag = other.tail.flag;
  return self;
}

#endif
}
