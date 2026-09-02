# 0014 - Logging and asserts are macros over a private spdlog

- **Status:** Accepted
- **Date:** 2026-09-01 (backfilled)
- **Deciders:** Sumin Park

## Context

Engine code logs everywhere, including per-frame paths, and needs assertions with
useful messages. A logging library in public headers becomes every module's
dependency, and log calls left in Release cost time on the hot path.

## Decision

`core/log/Log.h` exposes `MTS_LOG_TRACE` through `MTS_LOG_CRITICAL` over
`std::format`, plus `InitLog` and `FlushLog`. spdlog links `PRIVATE` to
`engine_core` and appears in no header. `MTS_ACTIVE_LOG_LEVEL` is a compile-time
floor - Trace in Debug, Info in Release - and anything below it becomes
`((void)0)`. Call sites come from `std::source_location`.

`Assert.h` adds `MTS_ASSERT` and `MTS_VARIFY` for programmer error (out in
Release, `MTS_VARIFY` still evaluating its condition) and `MTS_CHECK` for what
must abort anyway. All log and break with a per-compiler intrinsic; `MTS_CHECK`
flushes first. Logging is initialised in `main`, outside `App`, so early
construction failures are visible.

## Consequences

- Modules log without depending on spdlog; swapping it touches one `.cpp`.
- Trace and Debug cost nothing in Release, so they can stay in hot code.
- Macros, with the usual macro problems.
- No exceptions: failures return `bool`, `nullptr` or `optional` and log once.

## Alternatives considered

- **Expose spdlog directly** - fewer layers, but every module inherits it.
- **A logger interface with virtual calls** - runtime-swappable, at an indirect
  call per log and no compile-time stripping.
- **`assert()`** - no message formatting, no logging, no break control.
