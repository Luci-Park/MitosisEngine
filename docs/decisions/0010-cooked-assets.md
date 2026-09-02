# 0010 - Assets are cooked offline and addressed by path hash

- **Status:** Accepted
- **Date:** 2026-09-01 (backfilled)
- **Deciders:** Sumin Park

## Context

Reading source art at runtime means shipping importers, parsing formats on the
hot path, and having no stable identity once files move.

## Decision

A build-time cook step. `tools/AssetCooker` writes one blob per asset - a 32-byte
header (magic, format version, type tag, content version, size, content hash)
plus payload - and one binary manifest mapping ids to type tag, content version
and path.

An `AssetId` is the FNV-1a 64 hash of the **repository-relative** source path.
`engine_cook_assets` rejects an absolute source root for that reason: the root is
hashed into every id. At runtime `AssetCache` loads by id on demand and remembers
failures.

A blob counts as current only if its mtime beats the source **and** its header
matches the current format - an mtime test alone would skip every file after a
version bump while the manifest recorded the new version.

## Consequences

- The runtime parses one container, not N source formats.
- Ids are stable across machines and computable from a path at any time.
- Renaming a source file changes its id; there is no rename tracking.
- Everything currently cooks as a raw blob - the type tag exists for future typed
  importers.
- Shaders bypass this and go through `slangc` ([0011](0011-slang-shaders.md)).

## Alternatives considered

- **Load source assets at runtime** - fastest to write, slowest to run, importers
  shipped in the engine.
- **GUIDs in sidecar files** - survives renames, adds a file per asset and a
  registry to sync; revisit when renames hurt.
- **One packed archive** - better for distribution, worse for iteration; can be
  added later without changing ids.
