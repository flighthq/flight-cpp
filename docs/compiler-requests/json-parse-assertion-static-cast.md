# A TypeScript `as` on `JSON.parse` is emitted as a `static_cast` between unrelated types

## Reproduction

`flight/scene2d_formats/lottie_document.hpp` at `806d257`, line 324, from
`packages/scene2d-formats/src/lottieDocument.ts`:

```cpp
static_cast<flight::Ref<flight::types::LottieDocument>>(flight::Json::parse(std::get<1>(source)))
```

`flight::Json::parse` yields a `JsonValue`; `flight::Ref<LottieDocument>` is a reference to a declared
row type. The two are unrelated, so the cast does not compile:

```
error: no matching conversion for static_cast from 'JsonValue' to
       'flight::Ref<flight::types::LottieDocument>'
```

The source writes `JSON.parse(text) as LottieDocument`, a TypeScript assertion over a value that came from
`JSON.parse`, and the emitter lowers the assertion as a C++ cast.

## Required shape

An assertion between unrelated types is not a cast. Either materialize the document from the parsed value
through the declared owner the source asserts (the same path the format importers already take), or refuse
the assertion with the rule that names it. Emitting `static_cast` is the one answer that is neither.

## Affected

15 headers in the G1 sweep, all in `scene2d-formats` and the `scene2d-resources` importers that include it.
Once the `SequenceView::filter` runtime gap was closed this became the first diagnostic for all of them,
which is why it is filed separately from `local-reexport-using-before-declaration.md`.

## What this repository carries meanwhile

Nothing: the cluster is open. `SequenceView::filter` was added to the runtime because emitted code calls it
and the runtime genuinely lacked it; this one is an emitter decision and is left to the request.
