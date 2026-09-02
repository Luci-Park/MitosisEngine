# 0017 - Member variables are named mPascalCase

- **Status:** Accepted
- **Date:** 2026-09-01
- **Deciders:** Sumin Park

## Context

Three conventions were in the tree, roughly by the age of the code:

| Form | Where |
|---|---|
| `mPascalCase` | `core/ecs/*`, `assets/*` - newest and largest |
| `m_PascalCase` | `renderer/*` |
| `m_camelCase` | `app/*`, `window/*`, `*Desc` structs |

Everything else was already consistent, so this was the only open style question.
Left alone, a new contributor picks whichever file they opened first.

## Decision

`mPascalCase` for every member, engine-wide. It matches the largest and newest
body of code, so the least code moves.

- Members of a type with behaviour take the prefix, `*Desc` fields included:
  `AppDesc::mWidth`.
- Fields of a plain-data type describing a **layout** take no prefix and are not
  renamed: `AssetBlobHeader::magic`, `AssetManifestEntry::typeTag`, component
  fields. Those names are the format.
- `Entity::mIndex` and `mGeneration` already match.

## Consequences

- One convention, checkable in review at a glance.
- Three modules get a large mechanical diff, isolated into their own commits and
  ignored in blame.
- Any branch open across the rename conflicts on every touched line - a reason to
  land it while one person is working.

## Alternatives considered

- **`m_camelCase` everywhere** - more common in the wider C++ world, but renames
  `core/ecs` and `assets`, the largest bodies of code and the ones most likely to
  be read by someone learning the engine.
- **Leave it** - zero churn, permanent inconsistency, growing cost.
- **No prefix at all** - loses the local signal distinguishing a member from a
  parameter, which the Vulkan code leans on.

## Migration

Applied to the whole tree on 2026-09-01 - `renderer`, `app`, `window`, and the
one `main.cpp` call site - verified with a full build and `ctest` (67/67).
Currently an **uncommitted working-tree change**.

Commit it in two parts and no more: `renderer`, then `app` + `window` +
`main.cpp`. Those two cannot be split - `App.cpp` fills a `WindowDesc`, so the
fields and their call sites must move together for every commit to build.

Then list both full 40-character SHAs in `.git-blame-ignore-revs` at the
repository root. GitHub reads it automatically; each clone enables it once:

```
git config blame.ignoreRevsFile .git-blame-ignore-revs
```
