# module_name

A sentence or two on what this module owns, and what it stays out of.

`mir::<module>` · depends `mir::core`, `thirdparty` (private) · API `modules/<module>/include/<module>/` · maintainer <name> · reviewed YYYY-MM-DD

## Model

What someone needs to know before they open a header.

Name the main abstraction, say what flows through it and what owns what, then
say why it ended up that shape.

Drop in the shortest snippet that shows the module in use. Copy it from a test
or a real call site, so it does not rot into something that no longer compiles.

## API

| Type | Header | Role |
|---|---|---|
| `Foo` | `<module>/Foo.h` | one line |

Only expand on the types that are not obvious from their name, and on how they
fit together. The header already says what each method does, so there is no need
to repeat a signature here.

## Rules

One block per rule: what the rule is, what goes wrong if someone breaks it, and
why it exists. If something in `src/` would surprise a reader, it goes here too.

This is usually the most useful part of the document — it is the stuff that
otherwise only lives in your head.

## State

*As of YYYY-MM-DD.* What works today. What is stubbed out or missing. Where the
tests live and what they cover, and anything you left untested on purpose.

Aim for enough detail that a reader can tell whether the thing they came looking
for actually exists yet.

## Backlog

What you want next, roughly in order.

If something is still undecided, put it here with a note on what would settle
it.

## Changed

Things that used to work differently, newest first, a few lines each. What the
rule was, what it is now, and what made you change it. Keep the last ten or so
and let the older ones go.

*YYYY-MM-DD* — **Short title.** Was … Now … Because …
