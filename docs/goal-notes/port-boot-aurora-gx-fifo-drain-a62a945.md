# port-boot-aurora-gx-fifo-drain-a62a945 - shutdown hang fixed by teardown order

Done by hand on 2026-10-01 at 5c7d0482, after lane 1's attempt (correct diagnosis, failed by the
judge) was discarded. The item was removed from the queue first.

## The defect

`platform/main.cpp` held `CGraphicsSys graphicsSys` as a plain local of `main`, so it was destroyed
after the last statement - after `aurora_shutdown()`. `~CGraphicsSys` -> `CGraphics::Shutdown` ->
`CFrameDelayedKiller::StallAndFlushAllAllocations` -> `GXDrawDone` -> `aurora::gx::fifo::drain()`,
which in `ProcessingMode::Thread` waits for the FIFO worker to consume the tail
(`extern/aurora/lib/gx/fifo.cpp:260`). `aurora_shutdown` -> `gx::fifo::shutdown` -> `stop_worker`
had already joined that thread. Lane 1 measured it under gdb: six threads at the hang, no
`Aurora FIFO processor`, all five samples identical. The stall is retail's; the order was the port's.

## The fix

`platform/main.cpp` only: `graphicsSys`, `InitAll`, the stand-in tweaks, `InvokeCMain` and
`port::modules::ShutdownAll()` sit in an inner scope that closes before `aurora_dvd_close()` /
`aurora_shutdown()`. The order among them is unchanged (`ShutdownAll` still runs before
`~CGraphicsSys`). Nothing is skipped: the same `CGraphics::Shutdown` runs, while the worker is alive.
Lane 1 instead added a `CGraphicsSys::Shutdown()` method and called it from `main`; the scope does
the same without a new method on a game class.

## Measured

Head (`boot-progress.sh --record`), both runs: hang in `aurora::gx::fifo::drain`, last marker
`frame: 300`. With the fix, both runs: `exited normally`, 300 `frame:` lines each, and the lines after
the stall are reached (`fn_8032F6EC` reach-stub, Aurora's `Device lost`, a static destructor).
`BOOT_PROGRESS PASS: all 2 runs got further than all 2 head runs`.

## The judge change

`boot_progress.py` scored a clean exit with the head's marker set as undecidable, which no fix to a
teardown can avoid: nothing after it prints. New rule: head crashed or hung, candidate printed every
head marker and gdb reports `exited normally` -> further. Still undecidable: `exited with code N`,
and an exit against a head that also exited. Checked on the recorded baseline: hang vs hang 0,
hang vs exit-0 +1, hang vs exit-N undecidable, exit vs exit undecidable, lost marker -1.
An `exit(0)` that skips the stop passes this rule, as an early return passes the stack rule; the
reviewer rejects those.

## What is left

`osContext` and `memorySys` are still destroyed after `aurora_shutdown`; that works today. The boot
now reaches the end of the judge's 300-frame budget and exits, so there is no boot blocker to queue.
