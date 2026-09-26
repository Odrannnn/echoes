/**
 * `InitMetroTRK_BBA`, retail .text 0x803765C4..0x803765C8, 0x4 = 4 bytes:
 *
 *     803765c4  blr
 *
 * METROTRK's BBA (bulk-boot adapter) init hook, an empty `void` - the same four bytes as
 * `EnableMetroTRKInterrupts` in `src/Runtime/MetroTRKConsoleStubs.c`, and for the same
 * reason: a `void` with no body is a bare `blr`, because `r3` is caller-saved and the frame
 * is empty.
 *
 * A separate unit only because it is not contiguous with that one: 0x80003858 and 0x803765C4
 * are 0x3F2D6C bytes apart, and a unit may not claim two discontiguous `.text` ranges. Both
 * sat in `main`'s unclaimed `.text` at 0.00%, in `auto_03_80003840_text` and
 * `auto_03_803765C4_text`.
 *
 * A `.c` file so the name stays unmangled - `symbols.txt` has `InitMetroTRK_BBA` verbatim.
 */
void InitMetroTRK_BBA() {}
