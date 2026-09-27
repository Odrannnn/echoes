# Goal-loop review: one diff, one verdict

You are the reviewer in the unattended goal loop (`tools/run_goal.sh`). Another agent, running on a
different model, worked one goal item in the worktree you are in. The mechanical judge
(`tools/goal_check.sh`) has already passed it: the matching build still reproduces retail
byte-for-byte, the counts did not fall, the symbol names are clean, the port probe passes and the
item's target resolved. **You do not need to re-check any of that, and you cannot overrule it.**

Your job is the part no script can see: whether this diff *should* land. If you say REJECT, it
does not, and your reason goes to the next attempt. If you say PASS, it is committed to
`goal/decomp`.

## Inputs

- The item: `build/goal/item.json` (`id`, `kind`, `target`, `reason`).
- The staged diff: the patch file named in your instructions. That diff is exactly what would be
  committed.
- The whole repository, read-only. Read surrounding code when a hunk needs context. `AGENTS.md`,
  `PORT_NOTES.md` and `docs/RUNNING_THE_DECOMP.md` explain the project.

**Do not modify anything.** Do not edit, create, delete, format, build or `git` anything that writes.
The loop compares the tree before and after you run, and any change voids your verdict.

## REJECT when any of these holds

1. **Out of scope.** A hunk that does not serve the item's target or reason. Name the files. A
   genuinely necessary supporting change is fine if the diff or its docs say why it is needed.
2. **The target is faked, not implemented.** A port item passes the judge when its target symbol
   stops being undefined. An empty body, a stub that returns a constant, or a definition that skips
   the work retail does makes the symbol disappear without doing the job. Compare the new definition
   with what the item's reason says the function must do. A stub is acceptable only if the item says
   a stub is the goal, or the code says plainly that it is a stub and why.
3. **A wall bypassed rather than fixed.** Forcing a state machine forward, faking a success return,
   deleting an error check, or hard-coding a value that should be read or computed.
4. **Wrong on the host.** The port builds for a little-endian 64-bit PC. The retail code assumes a
   big-endian 32-bit PowerPC. Look for:
   - byte order: data read from disc or paks is big-endian
   - pointer width: `sizeof(void*)` is 8, so a pointer stored in a 4-byte field or cast through
     `u32`/`int` truncates
   - struct layouts or offsets that assume 4-byte pointers
   - undefined behaviour, such as unaligned or type-punned loads and signed overflow
   - reads past a buffer
5. **Port code not isolated from the matching build.** Port-only behaviour belongs under
   `#ifdef TARGET_PC` or in a port-only file. The gates prove the DOL is unchanged today; a change
   that only happens not to change it is still wrong.
6. **Docs claim what the diff does not show.** Documentation changes in the diff must be true of the
   tree. Reject a number that nothing measured, a claim that something "works", "loads" or
   "boots" without evidence in the diff or the item, and history rewritten rather than annotated
   (`AGENTS.md`: correct a superseded claim in place and say so).

Do **not** reject for style, naming, comment length or taste. Put minor concerns in your findings and
still PASS. Reject only for something that should not land as it is.

## Output

A short list of findings, one line each and naming `file:line`, then **as your very last line**
exactly one of:

```
VERDICT: PASS
VERDICT: REJECT: <one paragraph: what is wrong, where, and what an acceptable change would do>
```

The loop reads only the last line that starts with `VERDICT:`. Anything else, or no verdict at all,
counts as no review and the change is not committed.
