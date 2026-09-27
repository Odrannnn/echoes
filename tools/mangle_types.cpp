// Which mangled letter does mwcceppc give each one-byte type? Read the answer out of the object
// rather than out of anyone's memory of the Itanium ABI - CodeWarrior does not use it.
//
//   tools/probe_cc.sh tools/mangle_types.cpp /tmp/mangle_types.o
//   build/binutils/powerpc-eabi-nm /tmp/mangle_types.o | awk '{print $3}' | sort
//
// Measured 2026-09-27 with the `MetroidPrime/CMainAsyncIdle` unit's own flags (GC/2.7):
//
//   void  A(unsigned int, bool)            ->  A__4CFooFUib     bool           -> b
//   void  B(unsigned int, unsigned char)   ->  B__4CFooFUiUc    unsigned char  -> c
//   void  C(unsigned int, char)            ->  C__4CFooFUic     char           -> c
//   void  D(unsigned int, signed char)     ->  D__4CFooFUiSc    signed char    -> Sc
//   void  E(unsigned int, short)           ->  E__4CFooFUis     short          -> s
//   void  F(unsigned int, unsigned short)  ->  F__4CFooFUiUs    unsigned short -> Us
//   bool  I()                              ->  I__4CFooFv       bool           -> b
//   unsigned char J()                      ->  J__4CFooFv       unsigned char  -> c
//
// **`unsigned char` is `c`, not `h`.** That is the load-bearing line: it is why
// `config/G2ME01/symbols.txt`'s `AsyncIdle__11CResFactoryFUib` says the second parameter is
// `bool` and cannot be read as `unsigned char`, and why renaming that symbol to `...FUiUc` to
// match a `clrlwi r5,r30,24` would be asserting something the retail map contradicts. See
// `src/MetroidPrime/CMainAsyncIdle.cpp`.
//
// `char` and `unsigned char` both give `c`, so the two are indistinguishable by mangled name -
// worth knowing before a `char` parameter is diagnosed from a name.
//
// The definitions are out of line on purpose: with only declarations the unit emits no symbol
// table at all and `nm` prints nothing, which reads as a broken probe rather than an empty one.
struct CFoo {
  void A(unsigned int, bool);
  void B(unsigned int, unsigned char);
  void C(unsigned int, char);
  void D(unsigned int, signed char);
  void E(unsigned int, short);
  void F(unsigned int, unsigned short);
  void G(bool);
  void H(unsigned char);
  bool I();
  unsigned char J();
};

void CFoo::A(unsigned int a, bool b) { (void)a; (void)b; }
void CFoo::B(unsigned int a, unsigned char b) { (void)a; (void)b; }
void CFoo::C(unsigned int a, char b) { (void)a; (void)b; }
void CFoo::D(unsigned int a, signed char b) { (void)a; (void)b; }
void CFoo::E(unsigned int a, short b) { (void)a; (void)b; }
void CFoo::F(unsigned int a, unsigned short b) { (void)a; (void)b; }
void CFoo::G(bool b) { (void)b; }
void CFoo::H(unsigned char b) { (void)b; }
bool CFoo::I() { return true; }
unsigned char CFoo::J() { return 1; }
