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
sentinels are excluded. Ordinary/module bindings retain their original identifier-reference path.

Nine compiler regression groups cover same/imported readonly owners, conditional/early guards,
primitive payloads, captured values and mutable shared cells, nonnull defaults, union arms,
required/optional controls and reachable absence. The old backend fails the new behavioral guards.
The source oracle and emitted native fixture preserve defaults/null/undefined, original row/native
owner and shared control block, live cells, one producer evaluation, exact thrown reference owners,
signed zero/NaN and final owner release. Required, optional nonnullable and nullable-return functions
are byte-identical to baseline. Raw806d257 and the ordered84-patch backend use the same frontend IR
and both fail native compilation at the actual helper argument; the candidate compiles and runs.
This independently transpiles the raw backend and does not claim a full raw compiler build.

The current compiler production build passes. Final adjacent regression checks, repository automation,
normal scoped SDK generation and the original SVG public-consumer check remain pending. This work
has no package/tier or full SVG import completion claim.
