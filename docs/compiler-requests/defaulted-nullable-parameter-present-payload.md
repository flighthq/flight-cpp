# Guarded defaulted nullable parameters need both storage projections

The source `owner: Readonly<Owner> | null = null` distinguishes caller omission/undefined from
an explicit nullable value. C++ retains those states as `optional<optional<ReadonlyOwner>>`.
Default initialization consumes caller absence while keeping its outer storage engaged.
A later null guard proves the inner payload present; passing it to a readonly helper must read
`owner.value().value()`. The pinned backend emits only `owner.value()`.

The original `applySvgElementAppearance` combines an author transform with a defaulted nullable
`geometryTransform` through `multiplySvgMatrices`. Its incorrect argument at `svg_document.hpp:70`
blocks five original SVG consumers. No source caller or readonly contract needs changing.

[`compiler-defaulted-nullable-present-storage.patch`](../../source-patches/compiler-defaulted-nullable-present-storage.patch)
uses the emitter's existing initialized-binding read before projecting the declared nullable union.
Shared captured defaults already project initialization in their binding read and keep that rule.
An optional nullable parameter has the same separate caller-presence layer after both source
sentinels are excluded. Its sentinel tests inspect the original caller-presence carrier even when a
preceding strict guard narrowed the value; projecting that value first would test the wrong layer. Ordinary/module bindings retain their original identifier-reference path.

Ten compiler regression groups cover same/imported readonly owners, conditional/early guards,
primitive payloads, captured values and mutable shared cells, nonnull defaults, union arms,
required/optional controls and reachable absence. The old backend fails the new behavioral guards.
The source oracle and emitted native fixture preserve defaults/null/undefined, original row/native
owner and shared control block, live cells, one producer evaluation, exact thrown reference owners,
signed zero/NaN and final owner release. Required, optional nonnullable and nullable-return functions
are byte-identical to baseline. Raw806d257 and the ordered84-patch backend use the same frontend IR
and both fail native compilation at the actual helper argument; the candidate compiles and runs.
This independently transpiles the raw backend and does not claim a full raw compiler build.

The fresh ordered85-patch compiler archive applies and builds successfully, and all19 focused
checks pass. A private reversal of the68 touched source paths restores the raw pinned bytes.
The supplemental source/native optional-nullable and shared-default checks pass. Across503 relevant
existing backend tests, baseline and candidate both have469 passes and the same34 failures, with
1071 unrelated tests unselected and no new failures. The full static check retains exactly the
baseline's four failing gates:36 formatting paths, four lint diagnostics,12 import-order diagnostics
and five foreign fixture type errors. The new test passes its own formatting and lint checks.
Repository source-patch, emission-repair and package-label automation passes33/33.

Compiling the complete original document also instantiates its explicit
`add_node_child<Ref<Node2DTraits>>` call. The partial hierarchy override already has a native
Node2D overload; the explicit argument bypasses it and instantiates incompatible generic owner
recovery and NodeAny signal code. An exact Node2DTraits specialization delegates to that existing
overload. The pinned source's `addNodeChild` likewise delegates to `addNodeChildAt` at the current
child count. The specialization supplies no new graph implementation or owner conversion, and the
override retains its explicit unsupported signal-owner limitation and partial status.

The registered original full-header `applySvgElementAppearance` consumer covers omitted/undefined/null,
no-transform retention, author/geometry composition and order, live readonly Matrix cells, original
target/style behavior and final geometry lifetime. Its execution and normal scoped SDK generation
remain pending serialized package-writer coordination. No package/tier or full SVG import completion
claim follows from the compiler and synthetic native checks.
