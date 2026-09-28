/**
 * `CEchoEmitter::CreateEmitter` - retail `.text 0x801ECD8C..0x801ECDCC`, `size:0x40` = 64 bytes,
 * 16-byte frame, one function, the last in its own vtable slot before 0x801ECDCC's deleting
 * destructor.
 *
 * ```
 * 801ecd9c:  lbz     r0,92(r3)          ; 0x5C, the flag byte
 * 801ecda0:  rlwimi  r0,r5(1),7,24,24   ; set bit 0
 * 801ecda8:  stb     r0,92(r3)
 * 801ecdac:  lbz     r0,92(r3)          ; **re-read**, not carried over
 * 801ecdb0:  rlwimi  r0,r5(0),6,25,25   ; clear bit 1
 * 801ecdb4:  stb     r0,92(r3)
 * 801ecdb8:  bl      fn_801ECE14         ; (this, mgr)
 * ```
 *
 * The two masks are calibrated against `CGameOptions`' constructor (0x80161B9C), whose six
 * `bool : 1` members produce the same encodings for a byte at the same alignment: `rlwimi ..,7,24,24`
 * is bit 0 and `..,6,25,25` is bit 1. In `include/MetroidPrime/CEchoEmitter.hpp` the only two
 * `bool : 1` members are `mActive` then `mPendingDeletion`, so bit 0 is `mActive` and bit 1 is
 * `mPendingDeletion` - and the function is named for what it does: arm the emitter, mark it not
 * pending deletion, hand over to the worker-spawning helper.
 *
 * **The re-read between the two stores is not redundant and is what the source says.** `mActive`
 * and `mPendingDeletion` share the byte at 0x5C, so `mPendingDeletion = false` is a read-modify-write
 * of that byte and must load it again; writing both as one expression would drop the second
 * `lbz`.
 *
 * `fn_801ECE14` is 0x801ECE14, outside this unit's claim, so it is a plain `bl` to an unclaimed
 * `.text` address - which a carve is allowed to do; dtk resolves `R_PPC_REL24` against the base
 * object. `CEchoEmitter` is port-compiled through `src/MetroidPrime/CActor.cpp` and
 * `src/MetroidPrime/ScriptObjects/CScriptActor.cpp`, and neither needs this body, so this file is
 * **not** in `files.cmake` - see the note at the bottom.
 *
 * Upstream's `config/G2ME01/splits.txt` left 0x801ECD8C..0x801ECDCC as a gap between
 * `MetroidPrime/ScriptObjects/Carve801E8AEC.c` (which ends at 0x801E8AF4) and
 * `MetroidPrime/CGameGlobalObjectsTailCtor.cpp` (which starts at 0x801F0A44); this unit fills it
 * and nothing overlaps.
 *
 * The pre-merge version of this file was named for a local `CField25` shape reinterpreted from a
 * `CActor*`, on the belief that this was an enemy-side field. `config/G2ME01/symbols.txt` names the
 * address `CreateEmitter__12CEchoEmitterFR13CStateManager`, the header declares that method and
 * defines nothing, and the offsets above are `CEchoEmitter`'s own members, so the local shape and
 * the `CActor`/`CStateManager` signature are both gone. The file name is kept because renaming it
 * would churn a path that `configure.py`, `config/G2ME01/splits.txt` and `files.cmake` all carry.
 */
#include "MetroidPrime/CEchoEmitter.hpp"
#include "MetroidPrime/CStateManager.hpp"

extern "C" void fn_801ECE14(CEchoEmitter* self, CStateManager& mgr);

void CEchoEmitter::CreateEmitter(CStateManager& mgr) {
  mActive = true;
  mPendingDeletion = false;
  fn_801ECE14(this, mgr);
}
