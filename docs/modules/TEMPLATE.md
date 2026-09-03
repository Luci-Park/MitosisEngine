# <module>

- **Maintainer:**
- **Depends on:** `mts::` modules and third-party libraries
- **Public API:** `modules/<module>/include/<module>/`
- **Last reviewed:** YYYY-MM-DD

## Purpose

What this module is responsible for, and what it deliberately does not do.

## Mental model

The paragraph a reader needs before opening a header: the central abstraction,
what flows through it, what owns what.

## Key types

| Type | Header | Role |
|---|---|---|
| `Foo` | `<module>/Foo.h` | one line |

Expand only on the types that are not self-explanatory, and on how they relate -
the header says what each method does.

## Usage

The shortest representative example. Copy it from a test so it stays compilable.

```cpp
```

## Invariants

Rules a caller must not break, each with the consequence of breaking it. The most
valuable section - this is what otherwise lives only in the maintainer's head.

## Implementation notes

Anything surprising in `src/`: a layout chosen for a reason, an ordering that
matters, a workaround. Why, not what.

## Current state

*As of YYYY-MM-DD.* What works, what is stubbed, what is broken - specific enough
that a reader can tell whether what they need exists.

## Tests

Where they are, what they cover, what is deliberately untested and why.

## Open questions

Undecided things, and what would settle them.
