# 0004 - Scaffolding scripts instead of hand-written boilerplate

- **Status:** Accepted
- **Date:** 2026-09-01 (backfilled)
- **Deciders:** Sumin Park

## Context

With one library per module ([0002](0002-module-per-library.md)), adding a file
means two files in mirrored trees, a Doxygen header, and a CMakeLists edit. By
hand that gets forgotten or done wrong, and surfaces as a link error much later.

## Decision

`tools/new_module.ps1` and `tools/new_file.ps1` stamp `templates/` into place and
edit the build files. The author comes from `-Author`, then `MITOSIS_AUTHOR`,
then `git config user.name`, then the OS user, so attribution needs no shared
edit. CMake edits go through `Add-SortedEntry`, which inserts into the sorted
block, treats a commented-out entry as present, and never overwrites a file.
Both are VS Code tasks.

## Consequences

- New files are consistent by construction.
- The templates are the one place to change the house file header.
- PowerShell, so the convenience is Windows-only; the manual path still works.
- A generated module will not configure until a file is added, and the script
  says so.

## Alternatives considered

- **File globbing in CMake** - CMake needs a reconfigure for a new file anyway,
  and globbing hides what a target contains.
- **Copy an existing file** - the current failure mode, and it carries the
  previous author along.
- **A cross-platform generator** - a new dependency; revisit when a non-Windows
  contributor needs it.
