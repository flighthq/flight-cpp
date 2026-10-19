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

## G2 current-pin qualification: all three paint guards use this carrier

At compiler `806d257` / Flight `a627849`, source `sampleMorphShapePaintBinding` has three strict
inequality guards over the same flat command buffer:

- `commands[i] !== 'beginFill'`, emitted at `morph_shape_paint.hpp:77` in the color branch;
- `commands[i] !== 'lineStyle'`, emitted at line 85 in the line branch;
- `commands[i] !== binding.commandKey`, emitted at line 93 for the remaining texture/gradient branches.

The last right operand is a String-valued visit of the two remaining binding alternatives. It has the
same failure as the literals; repairing only the first literal would leave two equivalent comparisons.
`ShapeCommand.ts` defines the full source token domain: Matrix, all four Texture alternatives,
boolean, number, readonly number array, string and null. The emitted alias above retains those nine
present alternatives plus optional absence. A comparison must inspect the String alternative without
converting the other alternatives to strings, dropping them from the token carrier or selecting an
unchecked variant arm. A stored null token and every non-string token mismatch a string key; an equal
String matches and a different String mismatches. The original operands must be evaluated once in
their source order before deciding the comparison.

There is no missing external binding or runtime member to add. The existing equality operation in
`flight/presence.hpp` applies only to `variant<Value, Null, Undefined>` compared with its present Value.
This site's `optional<variant<...>>` is a different carrier and does not match it. Widening that
operator to arbitrary heterogeneous variants would introduce comparison policy outside its tested
three-state contract. Erasing the token to Any or coercing it would also discard the emitter's known
String test. No equality overload, token wrapper, source patch, emission repair or override is carried.

The source's owners matter after a matching guard. `commands` aliases the existing data command
array; sampling changes its color/alpha/thickness/focal-ratio cells. Texture sampling mutates the
existing command Matrix, and gradient sampling mutates the existing colors, alphas, ratios and
optional Matrix. Those handles must continue to address the same live objects. Gradient endpoint
arrays and matrices are snapshotted at construction where the source explicitly clones them, while
the texture itself remains shared. A repair must retain both those intentional snapshots and the
in-place sampling behavior, along with endpoint interpolation and binding order.

The five assigned headers remain failed in the product and explicitly link this existing request:

- `flight/shape/_internal_index.hpp`
- `flight/shape/contract.hpp`
- `flight/shape/morph_shape.hpp`
- `flight/shape/morph_shape_animation.hpp`
- `flight/shape/morph_shape_paint.hpp`

Qualification is **0/5 pass, 5 fail**, retained from G1 with the original diagnostics unchanged.
Only these five rows and their cause are reclassified to compiler lowering. No production C++ input
changed, so no generation, unchanged five-header compile or global sweep was run. No paint execution,
new native pass or upstream test coverage is claimed.

## Landed: strict comparison through the String alternative

`compiler-token-discriminant-comparison` lowers a strict (`===` / `!==`) comparison where one operand is
a union token carried as `optional<variant<...>>` (or a dual-sentinel variant) with exactly one String
alternative and the other operand is a String (a string-typed expression or a string literal):

```cpp
([&]() -> bool {
  const auto& token_discriminant = <token>;     // evaluated once, in source order
  const auto token_discriminant_key = <string>; // evaluated once, after the token when it is on the left
  return !(token_discriminant.has_value() &&
           std::holds_alternative<flight::String>(token_discriminant.value()) &&
           std::get<flight::String>(token_discriminant.value()) == token_discriminant_key);
}())
```

`===` emits the same predicate without the negation. Absence (a stored null token) and every
non-string alternative mismatch; an equal String matches; a different String mismatches. Nothing is
coerced, no alternative is converted or dropped, and the surrounding array/matrix aliases and explicit
clones are untouched. The operands use generated names, so a source operand called `key` cannot shadow
its own initializer.

Measured: six focused tests (the three guard shapes, reversed operand order, a single token evaluation,
and a non-token control); the emitted fixture compiles; the native consumer and the JavaScript oracle
agree on equal / different / null / number / boolean / array tokens and on no string coercion
(`5 !== '5'`). The three original guards — `!== 'beginFill'`, `!== 'lineStyle'` and
`!== binding.commandKey` — are the literal, literal and dynamic forms this lane covers.
