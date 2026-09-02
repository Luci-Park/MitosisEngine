# 0015 - Catch2 and CTest, per module

- **Status:** Accepted
- **Date:** 2026-09-01 (backfilled)
- **Deciders:** Sumin Park

## Context

The ECS, the asset container and the path helpers fail quietly and are painful to
debug through the running application. The renderer and window need a device and
a display and cannot be unit tested the same way.

## Decision

Catch2 v3 from vcpkg, one test executable per module, built by
`engine_add_module_tests` from `modules/<name>/tests/` under the `BUILD_TESTING`
option. It links the module plus `Catch2::Catch2WithMain` and calls
`catch_discover_tests`, so every `TEST_CASE` is its own CTest case and a failure
names the behaviour. `core` and `assets` are tested; `renderer` and `window` are
verified by running.

## Consequences

- `ctest --test-dir builds/windows-msvc-debug --output-on-failure` is the whole
  test story.
- A module's tests link only that module, so a test needing another module signals
  a wrong boundary.
- Test executables multiply with modules - the price of isolation.
- No coverage measurement and no CI; tests are run by hand.

## Alternatives considered

- **One test executable** - fewer targets, but it links everything, so a test can
  quietly depend on a module it should not know about.
- **GoogleTest** - equivalent; Catch2 needs less boilerplate.
- **No tests** - what the renderer does today, a known weakness rather than a
  policy.
