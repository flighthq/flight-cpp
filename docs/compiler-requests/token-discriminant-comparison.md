# A union-token discriminant comparison is emitted against the whole optional variant

## Reproduction

`flight/shape/morph_shape_paint.hpp` at `806d257`, line 77, from
`packages/shape/src/morphShapePaint.ts`:

```cpp
flight::Array<flight::types::ShapeCommandToken> commands = flight::row_get<flight::RowKey<"commands">>(data);
if ((commands.element(i) != flight::String("beginFill"))) { return; }
```

`ShapeCommandToken` is `std::optional<std::variant<Array<double>, Ref<Texture2DArray>, Ref<Matrix>,
Ref<TextureCube>, Ref<Texture2D>, Ref<Texture3D>, bool, double, flight::String>>`, so `commands.element(i)`
is that optional variant and cannot be compared with a `String`:

```
error: invalid operands to binary expression
       ('value_type' (aka 'std::optional<std::variant<...>>') and 'flight::String')
```

The source compares a string discriminant of a union token, where `null !== 'beginFill'` is well defined.

## Required shape

Compare the named alternative and treat absence as not-equal, which is the source's own semantics:

```cpp
token.has_value() && std::holds_alternative<flight::String>(*token) &&
    std::get<flight::String>(*token) == flight::String("beginFill")
```

## Why this is not a runtime operator

An `operator==`/`operator!=` between `std::optional<std::variant<...>>` and `flight::String` would be a
templated operator over every such optional variant and every such alternative — a permissive global
coercion that would make unrelated comparisons compile by accident. The runtime should not decide what a
comparison between an arbitrary variant and a string means; the emitter knows the alternative it is
testing. No operator is carried here.

## Affected

5 headers in the G1 sweep, all in `shape` (`morph_shape_paint` and its importers). One cause, one request.

## What this repository carries meanwhile

Nothing: the cluster is open.
