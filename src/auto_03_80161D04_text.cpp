/**
 * `rstl::sort`, `__insertion_sort` and `__sort3` for `rstl::vector< rstl::pair< uint, uint > >`
 * - retail's `fn_80161D04`, `fn_80161F40` and `fn_80161EC8`.
 *
 * `.text 0x80161D04..0x80161FBC`, 0x2B8 = 696 bytes, three functions:
 *
 * ```
 * 80161D04  fn_80161D04  0x1C4  452  rstl::sort<It, Cmp>
 * 80161EC8  fn_80161EC8  0x78   120  rstl::__sort3<pair<Ui,Ui>, Cmp>
 * 80161F40  fn_80161F40  0x7C   124  rstl::__insertion_sort<It, Cmp>
 * ```
 *
 * The bodies are the three `include/rstl/algorithm.hpp` templates with the template parameters
 * written out, because `symbols.txt` gives the instantiations the placeholder names above and
 * **other units reference those placeholders**: the retail object of
 * `MetroidPrime/Player/CGameOptions.cpp` (`fn_80161D04` is its `ResetControllerAssets`) and
 * `MetroidPrime/CMemoryCard.cpp` (`InitializePump`) both carry `U fn_80161D04`. A definition of
 * `rstl::sort<...>` mangles to `sort<...>`, leaves `fn_80161D04` undefined, and the link fails with
 * `undefined: 'fn_80161D04'` - the same trap `rstl/rstl_string_l.cpp` documents for
 * `fn_802FF3AC`, solved from C++ by spelling the parameters concretely instead of dropping to a
 * `.c` file, which was not available here because the parameters are class template
 * instantiations. `tools/check_symbol_names.py` skips `fn_`/`lbl_` placeholders, so nothing here is
 * a `symbols.txt` rename.
 *
 * Before this claim the range was unclaimed, so `dtk` supplied retail's own bytes from
 * `main/auto_03_80161D04_text` and both callers linked against that. `tools/range_owner.py` says
 * `.text 0x80161D04..0x80161FBC -> UNCLAIMED`, and both ends are retail function boundaries and
 * no other unit's, so this is a whole-unit carve of that gap rather than a cut out of a named
 * neighbour: `MetroidPrime/Player/CGameOptions.cpp` ends at 0x80161D04 and
 * `MetroidPrime/CEnvFxManager.cpp` starts at 0x80161FBC.
 *
 * **The file is named after the `auto_03_80161D04_text` unit it replaces, not after a class.** Two
 * reasons, both practical: `build/report.json` names a DOL unit after its object path, so keeping
 * the name keeps `main/auto_03_80161D04_text` resolvable for the goal item that targets this range
 * (`progress-fn80161d04-sort`), and the name records that this file is exactly that auto unit's
 * range and nothing else.
 *
 * Source order is **descending by retail address** and that is load-bearing: mwcceppc emits
 * definitions in reverse source order and mwldeppc keeps the object's `.text` order verbatim, so
 * an ascending file is a permuted `.text` - 100.00% per function and a broken DOL hash. Only
 * `tools/flip_test.sh` catches that; `python3 tools/check_decl_order.py --unit auto_03_80161D04_text`
 * checks it without a build.
 *
 * What the bytes are, in one place, because none of it is guessable from the types:
 *
 * - The element is **8 bytes and the key is its first word only**. `cmp` is
 *   `pair_sorter_finder<pair<Ui,Ui>, less<Ui>>`, whose `operator()(a, b)` is `a.first < b.first`;
 *   every compare in all three functions is a single `cmplw`/`cmplw.` on word 0. `less<uint>` is
 *   empty, so the whole comparator is **one byte**, passed through `r5` as a pointer to the
 *   caller's outgoing argument slot, and every `cmp(...)` is inlined into the compare above.
 * - `count` is the **element** count, and the three instructions that make it are MWCC's own
 *   expansion of `T* - T*`: `subf r0,r3,r5` is the byte difference, `srawi r0,r0,3` is the divide
 *   by `sizeof(T)`, and `addze r4,r0` rounds towards zero. The threshold is `cmpwi r4,0x14` = 20
 *   **elements**; `srwi r0,r4,31 / add r4,r0,r4 / srawi r4,r4,1 / slwi r4,r4,3` is `count / 2`
 *   times `sizeof(T)`, i.e. `mid = first + count / 2` with pointer arithmetic. `n <= 1` returns
 *   and `n <= 20` tail-calls `fn_80161F40`.
 * - The partition is **two independent scans against `*mid`**, not one pivot variable copied out
 *   of the array: `pivot` is `*mid` read once into `r7` (`lwz r7,0(r28)`) before the first scan and
 *   `r7` lives across both. The inner loop is
 *   `while (*it < pivot) ++it;` then `while (pivot < *end) --end;`, then `if (it >= end) break;`,
 *   and the swap is an eight-byte exchange done through `r5`/`r6`/`r8`/`r9` with `*t2` written
 *   from the saved pair halves - `iter_swap`, not `swap`.
 * - `__sort3` compares **only word 0** of all three elements and swaps whole 8-byte elements;
 *   that is `pair_sorter_finder`'s `a.first < b.first` on `pair<Ui,Ui>`, not `pair::operator<`
 *   (which falls through to `second` when `first` ties).
 * - `__insertion_sort` counts its outer loop with `mtctr`/`bdnz` - the counted form - which is why
 *   the trip count is computed once, before the loop, instead of by re-comparing `next < last` on
 *   every pass: `addi r5,r5,8` makes `r5` the second element, `addi r0,r4,7` makes `r0`
 *   `last + sizeof(T) - 1`, and `subf r0,r5,r0 / srwi r0,r0,3` is `(n - 1)` elements, one
 *   iteration for each element after the first - which is `for (++next; next < last; ++next)`.
 */

#include "rstl/algorithm.hpp"

#include "rstl/functional.hpp"
#include "rstl/pair.hpp"
#include "rstl/pointer_iterator.hpp"
#include "rstl/vector.hpp"

namespace {
/**
 * The instantiation retail made, written out. `rstl::vector<T>`'s default allocator is
 * `rstl::rmemory_allocator`, and `rstl::vector<T, Alloc>::iterator` is
 * `rstl::pointer_iterator<T, rstl::vector<T, Alloc>, Alloc>`, so these are the exact template
 * arguments of retail's `sort<...>`.
 */
typedef rstl::pair< uint, uint > TPair;
typedef rstl::vector< TPair > TPairVector;
typedef TPairVector::iterator TIt;
typedef rstl::pair_sorter_finder< TPair, rstl::less< uint > > TCmp;
} // namespace

/**
 * `fn_80161F40`, 0x80161F40..0x80161FBC - `rstl::__insertion_sort<It, Cmp>`. The loop is
 * `for (++next; next < last; ++next)`, which mwcceppc turns into the counted form retail has.
 */
extern "C" void fn_80161F40(TIt first, TIt last, TCmp cmp) {
  TIt next = first;
  for (++next; next < last; ++next) {
    TPair value = *next;

    TIt t1 = next - 1;
    TIt t2 = next;
    while (first < t2 && cmp(value, *t1)) {
      *t2 = *t1;
      --t2;
      --t1;
    }
    *t2 = value;
  }
}

/**
 * `fn_80161EC8`, 0x80161EC8..0x80161F40 - `rstl::__sort3<pair<Ui,Ui>, Cmp>`. The three elements
 * arrive by reference in `r3`, `r4`, `r5`; the comparator is the fourth parameter and is one
 * byte wide, passed by value, and never reloaded from memory because every `comp(...)` is the
 * inlined `cmplw` on word 0.
 */
extern "C" void fn_80161EC8(TPair& a, TPair& b, TPair& c, const TCmp comp) {
  if (comp(b, a)) {
    rstl::swap(a, b);
  }
  if (comp(c, b)) {
    TPair tmp(c);
    c = b;
    if (comp(tmp, a)) {
      b = a;
      a = tmp;
    } else {
      b = tmp;
    }
  }
}

/**
 * `fn_80161D04`, 0x80161D04..0x80161EC8 - `rstl::sort<It, Cmp>`. Recursive: the two tail calls at
 * 0x80161E7C and 0x80161EA4 are to itself, which is why this definition cannot be an inline
 * forwarder to the template.
 */
extern "C" void fn_80161D04(TIt first, TIt last, TCmp cmp) {
  const long count = last - first;
  if (count <= 1) {
    return;
  }
  if (count <= 20) {
    fn_80161F40(first, last, cmp);
    return;
  }

  TIt mid = first + count / 2;
  TIt end = last - 1;
  fn_80161EC8(*first, *mid, *end, cmp);

  TPair pivot = *mid;
  TIt it = first + 1;
  --end;

  while (true) {
    while (cmp(*it, pivot)) {
      ++it;
    }
    while (cmp(pivot, *end)) {
      --end;
    }
    if (it >= end) {
      break;
    }
    rstl::iter_swap(it, end);
    ++it;
    --end;
  }

  fn_80161D04(first, it, cmp);
  fn_80161D04(it, last, cmp);
}