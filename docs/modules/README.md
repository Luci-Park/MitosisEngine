# Module documentation

One document per module, `docs/modules/<module>.md`, kept current by whoever
works on it. [ARCHITECTURE.md](../ARCHITECTURE.md) says how the modules fit
together; these say how one of them works.

## Index

| Module | Document | Maintainer |
|---|---|---|
| core | [core.md](core.md) | Sumin Park |
| assets | - | - |
| window | [window.md](window.md) | Sumin Park |
| renderer | - | - |
| app | [app.md](app.md) | Sumin Park |

Add your row when you start a document.

## Writing one

Copy [TEMPLATE.md](TEMPLATE.md) to `<module>.md`, delete the sections that do not
apply rather than leaving them empty, and add yourself to the index.
[core.md](core.md) is the worked example.

**Belongs here:** the mental model, what the module refuses to do, the key types
and how they relate, invariants a caller must not break, anything surprising in
the implementation and why, current limits, what the tests cover.

**Does not:** API reference copied from headers (it goes stale in a month),
cross-module reasoning (that goes in [ARCHITECTURE.md](../ARCHITECTURE.md)),
task instructions (those are in [EXTENDING.md](../EXTENDING.md)).

## Keeping it honest

A stale module document is worse than none, because it is believed. Change it in
the same commit as the code, and date the "Current state" section so a reader can
judge its age.
