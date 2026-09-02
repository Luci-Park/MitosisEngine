# 0016 - Formatting is manual; clang-format is disabled

- **Status:** Accepted
- **Date:** 2026-09-01 (backfilled)
- **Deciders:** Sumin Park

## Context

`templates/.clang-format` sets `DisableFormat: true` and `SortIncludes: Never`.
An auto-formatter would rewrite hand-aligned blocks - the Vulkan struct
initialisers and the log macro tables are laid out to be read as tables - and
reordering includes breaks the group order that makes dependencies visible.

## Decision

No automatic formatting. Match the file you are editing: 4-space indent, Allman
braces, ~100 columns, include groups ordered by hand. The `.clang-format` file
exists so an editor with format-on-save does nothing.

## Consequences

- Hand-aligned code stays aligned and diffs contain only real changes.
- Style drifts unless review watches for it.
- No tool to lean on; [CONVENTIONS.md](../CONVENTIONS.md#formatting) is the whole
  specification.

## Revisit when

Whitespace comments become common in review. Then adopt a real `.clang-format`,
land the reformat as one isolated commit, and add it to `.git-blame-ignore-revs`.
