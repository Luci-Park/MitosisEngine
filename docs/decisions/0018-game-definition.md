# 0018 - A game is data: config, assets and Lua scripts

- **Status:** Accepted
- **Date:** 2026-09-01
- **Deciders:** Sumin Park

## Context

`games/` is reserved for games, but nothing said what a game is. The goal is low
friction: making a game, or changing one, should not mean touching engine code,
waiting on a C++ build, or knowing how the renderer is wired.

## Decision

A game is **data**, not a build target. One shared runtime executable loads a
game directory; games contain no C++.

```
games/<name>/
  game.config           boot parameters: title, resolution, entry script
  assets/               source assets, cooked with SOURCE_ROOTS games/<name>/assets/
  scripts/              Lua - components, systems, scenes, gameplay
```

1. **One runtime executable for every game.** It takes a game directory (or its
   cooked output) and runs it. Adding a game adds no build target.
2. **Lua is the gameplay language.** Scripts spawn entities, read and write
   components, and register systems into the existing phases.
3. **Engine systems stay C++.** Rendering, transforms and anything per-frame over
   every entity are native; Lua drives gameplay and glue. The boundary is crossed
   per system, not per entity, wherever that is possible.
4. **Component types are registered in C++**, in a name to `TypeId` registry.
   Lua sees the registered types by name; it cannot invent a new POD layout.
5. **Config and scripts are cooked assets** like everything else, with their own
   type tags, so a shipped game is one cooked tree.

## Consequences

- Making or changing a game needs no rebuild, and no C++ knowledge. That is the
  whole point.
- Hot reload becomes reachable: reloading a script is not relinking a binary.
- Games are trivially separable - a directory, with no build wiring to untangle.
- The C++/Lua boundary costs performance, and misplacing work across it is the
  easy way to make the engine slow. Rule 3 exists to bound that.
- POD components ([0006](0006-pod-components.md)) now earn their constraint
  twice: serialization and script marshalling both want a flat layout.
- Debugging spans two languages, and a script error has to surface as something
  better than a Lua stack trace in the log.
- A game that genuinely needs a new component type still needs a C++ change,
  until dynamic components exist.

## What this needs before it works

None of it exists yet. In dependency order:

1. `lua` and a binding layer (`sol2` is the obvious candidate, both in vcpkg) as
   a new `script` module.
2. A component registry: name to `TypeId`, plus per-type get and set, keyed off
   the already-stable `TypeId::hash`.
3. Query access from Lua, so a scripted system can iterate.
4. A config asset type, and a script asset type, in the cooker.
5. The runtime executable, replacing `main.cpp` and `HelloWorld`.

## Alternatives considered

- **One executable per game, gameplay in C++** - no scripting layer to build and
  no boundary cost, but every game is a build target, every change is a rebuild,
  and the engine ends up knowing about games. Rejected: friction is the thing
  being optimised away.
- **Data-only, no scripting** - config and scenes but no behaviour language.
  Cannot express gameplay without inventing a worse language in data.
- **C# or a custom VM** - heavier runtime, or a project of its own. Lua is small,
  embeddable, and standard for this job.
