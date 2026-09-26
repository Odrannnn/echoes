/*
 * MetroTRK's console and init stubs, retail .text 0x80003840..0x80003858 (0x18 = 24 bytes,
 * four functions), carved out of dtk's `auto_03_80003840_text`.
 *
 *     80003840  blr              EnableMetroTRKInterrupts
 *     80003844  blr              InitMetroTRK
 *     80003848  li   r3,0        __read_console
 *     8000384c  blr
 *     80003850  li   r3,0        __TRK_write_console
 *     80003854  blr
 *
 * All four are the no-op builds of METROTRK's trace hooks: two empty `void`s and two
 * `return 0`. `__read_console` and `__TRK_write_console` are the C library's console
 * read/write, and the signatures are the ones `src/Runtime/ansi_files.c` and
 * `src/Runtime/uart_console_io.c` already declare - `int f(__file_handle, unsigned char*,
 * size_t*, __idle_proc)`, the types coming from `libc/file_struc.h`.
 *
 * **One unit, not four.** They are contiguous, and the rule is one *discontiguous* range per
 * unit per section, not one range, so a single 0x18-byte claim covers all four. That is also
 * what `linked` wants: it counts *functions* in `Matching` units, not units, so one range is
 * worth exactly as much as four and costs one `configure.py` entry.
 *
 * **Declared in descending retail offset.** mwcceppc emits function definitions in reverse
 * source order and mwldeppc keeps the object's `.text` order verbatim, so a unit's functions
 * must be written highest-address-first or the module's bytes come out permuted while objdiff
 * still reads 100% and the link still succeeds. `tools/check_decl_order.py --unit` is the test.
 *
 * **A `.cpp`, not a `.c`, and that is forced.** The names must stay unmangled -
 * `symbols.txt` has `__read_console`, not a mangled form - so both compilers need
 * `extern "C"`, which a C translation unit cannot spell. A `.c` file was tried first and
 * mwcceppc's `-lang=c` then rejected the definitions outright: it does not know `size_t`
 * without a header, and `-i libc` is on the matching build's command line but `libc/` is not
 * on the port's, so there is no single header both compilers can be given. Repeating the
 * `size_t` typedef locally is not an option either - the host force-includes
 * `platform/compat.h`, which reaches `<cmath>` and with it the real 64-bit `size_t`, and a
 * second `typedef unsigned int size_t` is a conflicting declaration. `unsigned int` in the
 * signature is the one spelling that is correct on the 32-bit target and legal on the host;
 * nothing in the port calls these, so the width difference is never exercised.
 */
#include <stddef.h>

extern "C" {

typedef unsigned long __file_handle;
typedef void (*__idle_proc)(void);

int __TRK_write_console(__file_handle handle, unsigned char* buffer, size_t* count,
                        __idle_proc idle_proc) {
  return 0;
}

int __read_console(__file_handle file, unsigned char* buff, size_t* count,
                   __idle_proc idle_proc) {
  return 0;
}

void InitMetroTRK() {}

void EnableMetroTRKInterrupts() {}

} // extern "C"
