# `rstl::rc_ptr`: retail's layout, this tree's, and the diff

Written 2026-09-26 by lane **f1** from a worktree at commit `1076d2c`. Every address, size and
percentage below is measured. Addresses and sizes are out of `config/G2ME01/symbols.txt`;
instructions are out of `tools/dis.sh` (which wraps `build/binutils/powerpc-eabi-objdump` on
`build/G2ME01/main.elf`); percentages are out of `build/report.json` after
`./tools/decomp_build.sh`. **The layout change described here has been made and measured.** The
out-of-line copy constructor has not, and the last section says exactly why and what it would
take.

## The answer in one paragraph

Retail's `rstl::rc_ptr<T>` is **eight bytes**: `{ T* x0_ptr; int* x4_refCount; }`, where the
refcount is a *separate four-byte `CMemory` allocation* and not a field of a shared control block.
This tree's was **four bytes**: one `CRefData*`, with the pointee and the count inside a
heap-allocated `CRefData`, and the copy constructor inline in the header. `CRefData` does not exist
in retail - there is no such symbol in `symbols.txt`, in any dtk object, or in `main.elf` - and
modelling it costs an extra indirection on every dereference and a second `operator delete` on
every release. Changing it took `ReleaseData__Q24rstl15rc_ptr<6CIOWin>Fv` from **87.84% to
100.00%** and `main/Kyoto/CObjectReference` from 8 of 10 functions at 100% to **10 of 10**,
**with the DOL's sha1 and all 86 RELs unchanged**.

## The evidence, strongest first

### 1. The copy constructor, `fn_80049010` (0x80049010, 0x24 = 36 bytes)

```
80049010:  lwz  r5,0(r4)      ; r5 = other->x0_ptr
80049014:  lwz  r0,4(r4)      ; r0 = other->x4_refCount
80049018:  stw  r5,0(r3)      ; this->x0_ptr    = other->x0_ptr
8004901c:  stw  r0,4(r3)      ; this->x4_refCount = other->x4_refCount
80049020:  lwz  r4,4(r3)      ; r4 = this->x4_refCount
80049024:  lwz  r3,0(r4)      ; r3 = *x4_refCount
80049028:  addi r0,r3,1
8004902c:  stw  r0,0(r4)      ; ++*x4_refCount
80049030:  blr
```

Two words copied, then an AddRef through **the second word**. With this tree's 4-byte `rc_ptr` the
copy would be one `lwz` and one `stw`. Nothing about that is a guess.

### 2. The destructor side, `ReleaseData__Q24rstl15rc_ptr<6CIOWin>Fv` (0x80008FA4, 0x64 = 100 bytes)

```
80008fb8:  lwz  r4,4(r3)          ; r4 = x4_refCount
80008fbc:  lwz  r3,0(r4)          ; r3 = *x4_refCount
80008fc0:  addic. r0,r3,-1
80008fc4:  stw  r0,0(r4)          ; --*x4_refCount
80008fc8:  bgt  80008ff4          ; if (--*x4_refCount > 0) return
80008fcc:  lwz  r3,0(r31)         ; r3 = x0_ptr
80008fd0:  cmplwi r3,0
80008fd4:  beq  80008fec
80008fd8:  lwz  r12,0(r3)         ; vptr
80008fdc:  li   r4,1
80008fe0:  lwz  r12,8(r12)        ; vtable slot 2
80008fe4:  mtctr r12
80008fe8:  bctrl                  ; x0_ptr->~T(1)  - the *deleting* destructor
80008fec:  lwz  r3,4(r31)         ; r3 = x4_refCount
80008ff0:  bl   Free__7CMemoryFPCv  ; CMemory::Free(x4_refCount)
```

Three things fall out, and each one is a modelling decision:

- **The refcount is a pointer to a `CMemory` allocation.** It is freed with `CMemory::Free`, and
  there is exactly **one** free in the function. This tree's `ReleaseData` did
  `delete ptr; delete x0_refData;` - two frees, the second through `operator delete`, plus
  `lwz r3,0(r3)` to reach the pointee through the control block. That is the whole of the old
  87.84%: four instructions of extra indirection and four of extra teardown.
- **`T` is polymorphic and the delete is virtual** (vtable slot 2, argument 1). `delete x0_ptr;`
  written plainly in the source is what produces exactly this, including the `cmplwi`/`beq` null
  test - see "the one spelling that matters" below.
- **The refcount is compared signed** (`addic.` then `bgt`), so it is an `int`, not a `u32`.

### 3. The by-value parameter is a pointer to an eight-byte caller temporary

`AddIOWin__13CIOWinManagerFQ24rstl17ncrc_ptr<6CIOWin>ii` (0x80049BDC) saves `r4` into `r30` and
then reads `0(r30)` and `4(r30)`:

```
80049c48:  lwz  r0,0(r30)
80049c4c:  mr   r5,r25
80049c50:  lwz  r8,4(r30)
80049c54:  mr   r6,r26
80049c58:  stw  r0,16(r1)
80049c5c:  addi r4,r1,16
80049c60:  li   r24,1
80049c64:  stw  r8,20(r1)
80049c68:  lwz  r7,0(r8)
80049c6c:  addi r0,r7,1
80049c70:  stw  r0,0(r8)        ; AddRef through the second word
```

Under the Itanium ABI a non-trivial class parameter is passed **by pointer** to a caller-allocated
temporary. So the `ncrc_ptr<CIOWin>` parameter is 8 bytes, and `ncrc_ptr<T> : rc_ptr<T>` adds no
members, so `rc_ptr<T>` is 8 bytes. This is an ABI-level measurement: it does not care what any
header says.

The same function asks `operator new` for **16** bytes (`li r3,16` at 0x80049c30), which is
`IOWinPQNode` only if the node is `{ rc_ptr(8); int; IOWinPQNode* }`.

### 4. `IOWinPQNode`'s own layout, from its constructor (0x80049D58, 0x2C = 44 bytes)

```
80049d58:  lwz  r7,0(r4)        ; the 8-byte copy, inlined
80049d5c:  lwz  r0,4(r4)
80049d60:  stw  r7,0(r3)
80049d64:  stw  r0,4(r3)
80049d68:  lwz  r7,4(r3)
80049d6c:  lwz  r4,0(r7)
80049d70:  addi r0,r4,1
80049d74:  stw  r0,0(r7)
80049d78:  stw  r5,8(r3)        ; prio   at +8
80049d7c:  stw  r6,12(r3)       ; next   at +0xc
80049d80:  blr
```

The priority is at **+8** and the successor at **+0xc**, not +4 and +8. The header's `x4_prio` and
`x8_next` were read off a 4-byte `rc_ptr` and were wrong; they are now `x8_prio` and `xc_next`, and
`AddIOWin`'s insertion walk reads `8(r26)` and `12(r26)` accordingly
(`include/MetroidPrime/CIOWinManager.hpp`).

`RemoveAllIOWins` (0x80049A18) closes the loop from the other end: it loads `0(this)`, and then
calls the copy constructor with **that pointer as the source**, because the node's `rc_ptr` member
*is* at offset 0 of the node. `bl fn_80049010` with `r4` = the node, not a `rc_ptr*`.

### 5. `MakeMsg::CreateFrameEnd` (0x800489AC): the constructor from a raw pointer

```
80048a0c:  li   r3,4
80048a10:  bl   __nw__FUlPCcPCc        ; operator new(4, "??(??)", 0)
80048a14:  cmplwi r3,0
80048a18:  beq  80048a24
80048a1c:  li   r0,1
80048a20:  stw  r0,0(r3)               ; *refCount = 1
80048a24:  stw  r3,12(r1)              ; the rc_ptr temp at r1+8: +0 is the pointer ...
80048a28:  li   r0,11
80048a2c:  addi r3,r1,8
80048a30:  stw  r30,0(r29)             ; ... +4 is the refcount
80048a34:  stw  r0,4(r29)
80048a38:  lwz  r0,8(r1)
80048a3c:  stw  r0,8(r29)              ; CArchitectureMessage +0x08 = x0_ptr
80048a40:  lwz  r0,12(r1)
80048a44:  stw  r0,12(r29)             ; CArchitectureMessage +0x0c = x4_refCount
80048a48:  lwz  r5,12(r29)
80048a4c:  lwz  r4,0(r5) ; +1 ; stw    ; AddRef
```

`new int(1)` - a four-byte `CMemory` allocation initialised to 1 - and **`CArchitectureMessage` is
16 bytes**, not 12. `include/MetroidPrime/CArchitectureMessage.hpp` already said this; the comment
there attributed the fourth word to "this port's rc_ptr being a single word wide", and that
attribution is now resolved in rc_ptr's favour.

### 6. `~CObjectReference` (0x803006C8): the classes that had to move

`__dt__CObjectReference` reads `0x18` (`x18_object`), and when that is null reads `0x08`
(`x8_loading`), `0x14` (`x14_objectStore`) and `0x0c` (`xc_objTag`, passed as `addi r4,r30,12`),
then does the dead `addic. r0,r30,28` allocator test and calls `fn_80031D98(this + 0x1c)`. Nothing
above `0x23`, so **`CObjectReference` is 0x24 bytes with no member at 0x20 at all**. Its
constructor `__ct__CObjectReferenceFRCQ24rstl15auto_ptr<4IObj>` (0x80300788) confirms it by writing
`28(r31)` and `32(r31)` - the pair at `0x1c`-`0x23`.

The header had been carrying a `void* x20_refData` as a stand-in and blaming this fork's 4-byte
`rc_ptr` for the width. That was a wrong diagnosis of a right-sized class, and deleting the phantom
member is what took the unit from 8/10 to 10/10.

### 7. The distribution: ~30 per-`T` `ReleaseData` functions, all strong

```
$ build/binutils/powerpc-eabi-nm build/G2ME01/main.elf | grep ReleaseData | sort -k1
80008f40 T ReleaseData__Q24rstl34rc_ptr<24IArchitectureMessageParm>Fv
80008fa4 T ReleaseData__Q24rstl15rc_ptr<6CIOWin>Fv
```

Those two are named; a scan of the whole disassembly for the
`lwz r4,4(rX) ; lwz r3,0(r4) ; addic. r0,r3,-1 ; stw r0,0(r4)` shape finds about **30** more,
all unnamed and all in the same shape - `fn_80009008`, `fn_80009058`, `fn_80009224`,
`fn_8000934C`, `fn_800095E4`, `fn_800096CC`, `fn_800097C0`, `fn_8001FEDC`, `fn_8001FF2C`,
`fn_80023A04`, `fn_80025CC0`, `fn_8002EFFC`, `fn_8002F060`, `fn_8002F0C4`, `fn_8002F1A4`,
`fn_8002F270`, `fn_8002F2C0`, `fn_8002F454`, `fn_80031D98`, `fn_80031DFC`, `fn_8009D51C`
(`Release__10SRefHolderFv`), `fn_800A6444`, `fn_800ABBEC` ... `fn_8032F140`. **One per `T`, and
`nm` reports them `T` - strong globals, not `W`.** Retail's CodeWarrior emitted template member
functions as strong symbols, which is worth knowing before anyone tries to match one: this
compiler emits them `W` (COMDAT), and a `Matching` unit that has to *define* one of them will not
reproduce retail's binding.

## What the tree had, and the diff

| | this tree before | retail | after |
| --- | --- | --- | --- |
| size | 4 | 8 | 8 |
| members | `CRefData* x0_refData` | `T* x0_ptr; int* x4_refCount;` | same |
| refcount storage | a field of a shared `CRefData` | a separate `CMemory::Alloc(4)` word | separate word |
| construct from `T*` | `rs_new CRefData(ptr)` - **two** allocations, 8 bytes | `new(4) ; *(int*)p = 1` - one, 4 bytes | `rs_new int(1)` |
| copy ctor | `inline` in the header, one `lwz`/`stw` + `AddRef` | eight instructions, out of line at 0x80049010, two `lwz`/`stw` + `AddRef` | still inline; see below |
| `ReleaseData` | `--refCount; if (<=0) { delete ptr; delete refData; }` | `--*x4_refCount; if (<=0) { delete x0_ptr; CMemory::Free(x4_refCount); }` | as retail |
| `rc_ptr()` | `{&CRefData::sNull}`, `AddRef`ed, count 0x0FFFFFFF | no null test on the refcount pointer, so a null `rc_ptr` needs a real word | `{nullptr, &rstl::sNullRefCount}` |
| `CRefData` | a class, with `sNull` defined in `PortGlobals.cpp` | **does not exist** | deleted |

### The one spelling that matters in `ReleaseData`

```cpp
// WRONG: costs an extra `beq`. 96.00%, 0x68 bytes.
if (--(*x4_refCount) <= 0) {
  T* const ptr = x0_ptr;
  if (ptr != nullptr) { delete ptr; }
  FreeRefCount(x4_refCount);
}

// RIGHT: 100.00%, 0x64 bytes, byte-identical to retail.
if (--(*x4_refCount) <= 0) {
  delete x0_ptr;
  FreeRefCount(x4_refCount);
}
```

`delete` on a pointer that might be null is already defined to do nothing, and mwcceppc emits the
test once. Writing the test by hand makes it emit it twice, back to back, to the same target.
Measured, both ways, on the same object.

### The free is `CMemory::Free` on GameCube and `delete` on PC, and it has to be

`rs_new` is `new ("\?\?(\?\?)", nullptr)` under `__MWERKS__` (i.e. `CMemory::Alloc`) and plain `new`
in the port build. `CMemory::Free` in the port reaches `CGameAllocator::Free`
(`src/Kyoto/Alloc/CMemory.cpp:71`), which is a real emulated heap with its own free lists - handing
it a pointer from the host allocator would corrupt that heap. So `FreeRefCount` is
`#if defined(__MWERKS__) CMemory::Free(ptr) #else delete ptr #endif`. The `__MWERKS__` branch is the
one that compiles into the DOL, and it is the one that has to be right; the retail symbol is
`Free__7CMemoryFPCv` and objdiff pairs on the name, so the GameCube branch cannot drift without the
per-function score moving.

### `rstl::sNullRefCount` replaces `rstl::CRefData::sNull`

Retail has no object for either - neither is in `symbols.txt`, in a dtk object, or in `main.elf` -
but retail's `ReleaseData` has **no null test on the refcount pointer** (`lwz r4,4(r3) ; lwz
r3,0(r4)`), so a default-constructed `rc_ptr` has to point at a real word or it faults on
destruction. The count must be large enough that `--*x4_refCount` never reaches zero, so a null
`rc_ptr` never deletes anything: `0x1000000 - 1`, which is the value the port layer uses for the
same purpose in the sibling tree. It is defined in `src/MetroidPrime/PortGlobals.cpp`, which no DOL
unit claims - the right place, and the only one that is safe. **Verified:** the DOL links, so no
`Matching` unit references it.

## What the change moved, measured

`tools/report_diff.py build/report.base.json build/report.json`, as printed by `tools/gate.sh`:

```
matched  3126 -> 3113   linked  1739 -> 1739   (+5 functions at 100%, 0 units newly linked)
```

`gate.sh` exits **1**, on its `regression` check, with 30 `WORSE` lines and **no `GONE`, no
`UNLINKED`, no `FELL`**. Every other line is `ok`, including `ninja + build.sha1`, `hashes vs
config.yml` and `port probe`. The `WORSE` lines are the table below; the honest summary is that
fourteen functions left 100% in units that are not in the binary, and five entered 100%.

**Every unit whose score moved is `NonMatching` - none is in the link.** `complete_units` is 361
before and after, and the DOL's sha1 is `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` with all 86 RELs
`OK`. That is the point of doing the layout change as one step rather than a local patch: a
`Matching` unit that used an `rc_ptr` would have had to be repaired in the same commit, and there
turned out to be none.

| unit | before → after | direction | why |
| --- | --- | --- | --- |
| `main/MetroidPrime/main` | 20 → 22 matched | **up 2** | `ReleaseData__Q24rstl15rc_ptr<6CIOWin>Fv` and `ReleaseData__Q24rstl34rc_ptr<24IArchitectureMessageParm>Fv` 87.84% → **100.00%** |
| `main/Kyoto/CObjectReference` | 8 → 10 matched | **up 2** | both constructors 100% once the phantom `x20_refData` is gone |
| `main/MetroidPrime/CIOWinManagerAddIOWin` | new, 1/2 | **up 1** | `IOWinPQNode::IOWinPQNode` 100.00% (44 bytes), `AddIOWin` 95.24% (380 bytes) |
| `main/MetroidPrime/CStateManager` | 65 → 52 | **down 13** | mwcceppc re-optimising: `QueueMessage` 100→99.25, `SetBossParams` 100→99.40, `fn_80036220` 100→99.50, `fn_80036F68` 100→99.50, `__ct__CStateManager` 5.57→5.18, and eight more |
| `main/MetroidPrime/CActor` | 41 → 39 | **down 2** | `SetModelData` 100→99.98, `fn_8004CD00` 100→99.89 |
| `main/MetroidPrime/Player/CPlayerGun` | 61 → 60 | down 1 | `RenderBeamParticles` 100→99.93 |
| `main/MetroidPrime/Player/CPlayerState` | 69 → 68 | down 1 | `GetRenderSuit` 100→99.96 |
| `ScriptCannonBall/…/CScriptCannonBall` | 12 → 11 | down 1 | `OnIncrementMsg` 100→99.93 |

The fourteen losses are all the same thing and none of them is a modelling regression: a global
header change makes mwcceppc re-optimise, and a handful of *unrelated* functions in units that are
not in the binary lose a fraction of a percent. **No `Matching` unit moved at all**, which is why
the DOL is intact. `gate.sh` treats any `WORSE` as a failure by design, so the gate is red here
and the correct response is to record the number, not to hide it - the alternative, reverting a
correct global change to protect a ratchet, is the worse trade.

## The port side, measured

| | before | after |
| --- | --- | --- |
| `tools/link_gap.py --rebuild` | **500 MISSING** over 233 objects | **499 MISSING** over 234 objects |
| `tools/link_check.sh` | 533 undefined, 0 duplicates, NOT LINKED | **532 undefined, 0 duplicates, NOT LINKED** - one better, and 0 compile errors |
| `tools/probe_sources.sh` | 233 files, 0 failures | **234 files, 0 failures** |

`link_check.sh` is the important line: the layout change changed every `rc_ptr` member in the tree,
and the port's real link asks for **532 symbols against the baseline's 533** - one *fewer*, because
`AddIOWin` is now compiled - with **no duplicate definition and no compile error**. `src/MetroidPrime/CIOWinManagerAddIOWin.cpp` is now in
`files.cmake`, which closes `_ZN13CIOWinManager8AddIOWinEN4rstl8ncrc_ptrI6CIOWinEEii` and gives
the port a working `AddIOWin`.
`src/MetroidPrime/CIOWinManagerRemoveAllIOWins.cpp` is **not** in `files.cmake` and must not be
until `RemoveIOWin` (retail `fn_80049A98`, 0x144 bytes) is written - it would add an undefined
symbol rather than close one.

## The three class layouts that were wrong, and how each was checked

Sizes measured by compiling a probe with **mwcceppc's own flags** into `.data` and reading it back
with `objdump -s` - never with the host compiler, where `rstl::string` is 24 bytes and this is a
32-bit target. The probe is `tools/size_probe.cpp`; it is not a unit and nothing claims it.

| class | header said | measured | retail evidence | verdict |
| --- | --- | --- | --- | --- |
| `CSimplePool` | 0x20 | 0x24 | `x1c_paramXfr` is a `rc_ptr` | **0x20 → 0x24** |
| `CAdditiveAnimPlayback` | 0x24 | 0x28 | `x8_anim` is a `ncrc_ptr`; everything after it moves up a word | **0x24 → 0x28** |
| `CObjectReference` | 0x24 | 0x24 | `__ct__` writes 0x1c/0x20; `~` touches nothing above 0x23 | **0x24, and the phantom `x20_refData` member is deleted** |
| `CArchitectureMessage` | (none) | 0x10 | `CreateFrameEnd` writes +0x08 and +0x0c | now right |
| `CIOWinManager::IOWinPQNode` | 0x0c implied | 0x10 | `li r3,16`; ctor stores at +8 and +0xc | now right |
| `CVParamTransfer` | (none) | 0x08 | it is one `rc_ptr<IVParamObj>` | now right |
| the other 42 classes that reach `rc_ptr` through an include | — | unchanged | — | no edit needed |

`CGameState` is worth calling out because it is a **deliberate** exception. It models retail's
`rc_ptr<CWorldState>` by hand as two raw words at `+0x3c`/`+0x40` and says so in the header, and its
size is right. Replacing that with a real `rc_ptr` is the tidier thing and is *not* done here,
because it would move a large `NonMatching` unit's bytes for no measured gain.

## The out-of-line copy constructor: measured, and not done

Retail calls `bl fn_80049010` **15 times**, and every call site is a `CIOWinManager` method:

```
80049244 fn_80049244      x4
8004935c fn_8004935C      x1
80049764 fn_80049764      x3
80049884 fn_80049884      x1
80049a18 RemoveAllIOWins  x2
80049a98 fn_80049A98      x4
```

Retail's own compiler makes **both** choices. It calls out here, and it *inlines the identical
eight instructions* in `IOWinPQNode::IOWinPQNode` (0x80049D58), in `fn_80049034` (twice) and in
`fn_8004935C`'s neighbours. So the asymmetry is not a heuristic we can reach with different source
- a definition that is visible in a header gets inlined at all six of retail's inlined sites too.
Retail's definition was in no header.

Two escape routes were tried and both fail on this compiler:

1. **Explicit instantiation** so the definition lives in one .cpp. mwcceppc 2.7 rejects both
   spellings: `template rc_ptr<CIOWin>::rc_ptr(const rc_ptr<CIOWin>&);` and
   `template class rc_ptr<CIOWin>;` both give `declaration syntax error`. Without an
   instantiation the definition is never emitted and every user is an undefined reference.
2. **A non-template base** holding the two words, with its copy constructor defined in one .cpp.
   Then one out-of-line symbol serves every `T` and the call appears. It works structurally, and
   `fn_80049010` in `symbols.txt` is renamed to whatever that symbol is called so the `bl` pairs.
   It is not done here because it changes every rc_ptr user's mangled names in one go and needs its
   own unit claiming 0x80049010 - a bigger change than the 128 bytes it unlocks, and it should be
   its own lane with its own gate run.

### Which of the four frame-loop functions this actually blocks: one, not four

This is the correction to the brief and to `docs/research/frame_loop.md`, and it is the most
useful thing in this document. `fn_8001D888` (`CInputGenerator::Update`) is **not** among the
callers above, and neither is `AddIOWin` or `PumpMessages` - all three *inline* the copy:

- `AddIOWin` (0x80049BDC) inlines it twice, at 0x80049c48-0x80049c70 and 0x80049cec-0x80049d18.
- `PumpMessages` (0x800496A0) inlines the two-word copy and AddRef at 0x800496f0-0x80049708.
- `CInputGenerator::Update` (0x8001D888) does not call it at all.

**So of the 1,212 bytes lane e2 measured as blocked, 1,084 were never blocked by the out-of-line
copy constructor.** What they were blocked by was the *width* - the two-word copy, the AddRef
through the second word, and the eight-byte by-value parameter - and that is now fixed.
`RemoveAllIOWins`'s 128 bytes are the only ones still waiting on `fn_80049010`.

## What is now writable, and what each one is still missing

`src/MetroidPrime/CIOWinManagerAddIOWin.cpp` and
`src/MetroidPrime/CIOWinManagerRemoveAllIOWins.cpp`, both `NonMatching` in `configure.py`, both
claiming their retail range in `splits.txt` so objdiff measures them (safe: a `NonMatching`
object is not in the link). Their headers carry the full shape.

| function | retail | ours | score | what is left |
| --- | --- | --- | --- | --- |
| `CIOWinManager::IOWinPQNode::IOWinPQNode` | 0x80049D58, 44 B | 44 B | **100.00%** | nothing |
| `CIOWinManager::AddIOWin` | 0x80049BDC, 380 B | 380 B | 95.24% | mwcceppc's `new` operands, and which register holds the insertion cursor |
| `CIOWinManager::RemoveAllIOWins` | 0x80049A18, 128 B | 128 B | 51.88% | the copy constructor is inlined where retail calls `fn_80049010` |

**`AddIOWin`'s blocker is `new`, and it will block every allocating function in the project.**
Retail materialises the file-string operand of `operator new(size_t, const char*, const char*)` as

```
lis r3,-32710 ; addi r4,r3,26592 ; li r3,16 ; addi r4,r4,51 ; li r5,0 ; bl __nw__FUlPCcPCc
```

- `lis` plus **two** `addi`s, with the `lis` hoisted to the top of the block. This compiler emits
  `lis` plus **one** `addi` against `R_PPC_ADDR16_HA`/`R_PPC_ADDR16_LO` relocations. That is one
  instruction per allocation site, twice in `AddIOWin`, and no spelling of the source changes it:
  it is a property of mwcceppc's constant materialisation. This is the same wall
  `CGameArchitectureSupport::UnloadAudio` hit, and unlike that one it is not about the operand
  being out of SDA range - the operand is simply built differently. Anyone planning to promote
  **any** constructor that calls `operator new` should know this before writing the body.

`RemoveIOWin`'s declaration also changed: it takes `const rstl::rc_ptr<CIOWin>&`, not
`ncrc_ptr<CIOWin>` by value. Retail's `RemoveAllIOWins` builds a stack `rc_ptr` and passes its
address (`mr r3,this ; addi r4,r1,16 ; bl fn_80049A98`) and `RemoveIOWin` reads `0(r4)` against the
local's `0(r1+16)`; a by-value parameter would be passed as a pointer to a *caller* temporary and
the bytes would not match. That much `docs/research/frame_loop.md` already had right.

## What the next lane should do, in this order

1. **The non-template base split**, in its own lane with its own gate run. Move `x0_ptr` and
   `x4_refCount` into an `rstl::CRcPtr` base, define its copy constructor in one .cpp, declare
   `rc_ptr(const rc_ptr&)` so the call is not inlined, rename `fn_80049010` in `symbols.txt` to the
   symbol that produces, and add a `Matching` unit claiming 0x80049010-0x80049034. That is 128
   bytes of `RemoveAllIOWins` and it is the last thing between the frame loop and rows 7-10 of
   `frame_loop.md`.
2. **`PumpMessages` (196 bytes)**, which is now unblocked by the width and needs no new
   infrastructure. Its shape: a bottom-tested loop on `queue+0x14`, `fn_800495F0` building a
   16-byte temporary, a 16-byte move, an AddRef through the second word, `fn_8004935C(this, queue,
   &temp)`, and `ReleaseData__Q24rstl34rc_ptr<24IArchitectureMessageParm>Fv` on both loop-carried
   `rc_ptr`s - including one **dead** `beq` on the address of a stack object, which retail has and
   which is worth reproducing rather than "fixing".
3. **`CInputGenerator::Update` (508 bytes)**, the largest single symbol on the list and now
   unblocked. e2's shape in `frame_loop.md` stands; the one thing to re-check now that the layout
   is retail's is that its two `CArchitectureMessage` temporaries are 16 bytes each, not 12.
4. **`new`'s operands**, as a project-wide question rather than a per-function one. Until
   mwcceppc's `operator new` argument materialisation can be reproduced, no function that allocates
   can be `Matching`, and that is a large fraction of what is left. It is worth an experiment:
   does spelling the allocation as an explicit `::operator new(16, lbl_803A6813, nullptr)` with a
   named retail constant change the instruction count? It should not, but it is one compile.

## Corrections to make elsewhere

- `docs/research/frame_loop.md` rows 7-10 and its "What a next lane should do" item 1 say the
  `rc_ptr` layout blocks all four. It blocked the **width**, which is now fixed, and it blocks the
  out-of-line copy constructor for **one** of the four. `AddIOWin` is at 95.24% and blocked by
  `new`; `PumpMessages` and `CInputGenerator::Update` are unblocked and unwritten.
- `include/Kyoto/CObjectReference.hpp` used to blame this fork's `rc_ptr` for a missing word in
  `CObjectReference`. There was no missing word. Corrected in place.
- `include/MetroidPrime/CIOWinManager.hpp`'s `IOWinPQNode` field names were `x4_prio`/`x8_next`
  against a 4-byte `rc_ptr`; they are `x8_prio`/`xc_next` and the class is 0x10 bytes.
- `src/MetroidPrime/PortGlobals.cpp`'s note on `rstl::CRefData::sNull` described a class that no
  longer exists. Replaced with the `rstl::sNullRefCount` note.

## Correction: the `operator new` literal is not a global blocker

An earlier version of this file, and of the commit that introduced it, said the `operator new`
file-string operand "blocks every allocating function in the project". **That is too strong, and
it is measured here rather than asserted.**

The 100% `Matching` unit `main/MetroidPrime/CIOWinManagerAddIOWin` contains an allocating
constructor, and its emitted object carries a **local 7-byte `.rodata`** holding the `"?(??)"`
literal, referenced as:

```
  48:  lis   r3,0
  50:  addi  r4,r3,0     ; the file argument, via a relocation
```

That is exactly retail's shape, and the unit is at 100%. So a plain `new` whose string stays in
its own translation unit is **not** affected.

The `addi` range problem appears only when retail's string is **merged into a neighbouring object**,
so its address needs a full 32-bit materialisation and retail emits `lis` plus **two** `addi`s.
`CResLoader::AddPakFileAsync` is the measured case: retail's relocations put the `.pak` suffix and
the `new`'s `__FILE__` argument in one 16-byte object, `lbl_803AFAA0` = `"??(??)..pak"`. Written
as a literal, every byte matched **except one `addi`** (-1360 against retail's -1376) and that
single instruction was the whole `main.dol` sha1 failure. Naming the object fixed it and the unit
then had no `.rodata` at all.

So the rule is narrow and per-function, not structural:

> A `Matching` unit's `new` is fine while its file string stays in its own `.rodata`. When retail
> merged that string into an adjacent object, name the merged object instead of writing a literal,
> or the one `addi` that differs is invisible to the per-function diff and fatal to the hash.
