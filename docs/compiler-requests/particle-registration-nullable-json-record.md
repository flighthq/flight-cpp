# Nullable JSON record in particle registration

Flight `a62784923f3be814463286c9fb28edfe6ab15789`, compiler
`806d257d2f4100de745cc97340e61c2f301b0f24`, composed compiler stack 111.
The stack-111 counterexample below is retained as historical evidence. Foreman8334abed now
approves both bounded compiler extensions described here. The declared
`compiler-nullable-json-and-indexed-string-trim` patch implements them on preceding stack112
without a public ABI change, runtime addition or source body rewrite.

The private `parseJsonObject` at original `registerBuiltInParticleFormats.ts:185` parses once,
returns the original object only after the null/object/nonarray guard, and returns null on any
caught failure. On historical stack111 the emitted native signature is `optional<Record<String,Any>>`, but its object
branch constructs that optional from `flight::named_properties(value)`. That value is the existing
owner-preserving `NamedProperties` view, which cannot construct an owning `Record`. Copying its
fields into a Record would replace the original owner and is not an acceptable conversion.

`scripts/fixtures/particleRegistrationJsonBoundary.ts` retains the exact original private helper
and adds only a boolean caller. The boolean caller avoids an unrelated optional-member-read
refusal reached by a first property-reading counterexample. The strict source passes nine root,
object and invalid-input cases. Its production-emitted native code fails with:

```text
error: no matching constructor for initialization of
  std::optional<flight::Record<flight::String, flight::Any>>
```

The argument is `NamedProperties`. `scripts/particleRegistrationBoundaryOracle.mjs` requires that
specific native failure rather than accepting an arbitrary failed build. It separately builds and
runs `particleRegistrationJsonViewControl.cpp`: existing JSON storage distinguishes every root
kind, a named read view retains the exact original object, aliases see writes, null and absent
properties differ, integer/ordinary keys and `__proto__` retain their spelling/order, and an optional
view owns the parsed container until the last view is released. Static controls reject constructing
the current optional Record from that view. These are storage controls, not a native implementation
of the private helper or an executed built-in registry.

The approved nullable extension selects `optional<NamedProperties>` only for a private exact
parse/guard/conditional-return/catch-null producer. Every call must be an exact null test or initialize
an immutable, uncaptured local used only for data reads, exact null tests or proven private readers.
Public results, returned owners, aliases, captures, identity comparisons, opaque calls, mutable locals,
nested writes and altered parse/guard/catch shapes keep the preceding lane. Original strict positive
null guards supply the selected local's presence proof; an unguarded or wrong-branch read refuses.
The emitter retains the original condition and parsed value, and the catch remains in place.

## The separate libGDX detector expression

The original descriptor at registration line 50 reads:

```ts
text.trimStart().split('\n')[0]?.trim() === 'Particle Effect'
```

Seven direct source cases pass. A freshly lowered minimal expression refuses with
`cpp-optional-property-call-missing-callable-result`. The retained earlier normal IR graph refuses
the descriptor at the same expression with `cpp-member-projection-without-present-storage`.
These are distinct measured contexts and rules; neither establishes current normal SDK output.
Builder2f2f23d97 confirms no overlap: its work covers IR `call.optional`, declared callable returns,
external binding `callResultType` and erased local evidence. This detector instead calls a member
through an optional chain on a builtin string receiver obtained by indexing. Its separately approved
scope recovers the existing builtin String method's known return evidence and preserves the
optional receiver guard, with each receiver/index evaluated once. No public function ABI or runtime
primitive changes are introduced. The absent receiver must remain undefined, and the present branch
must invoke the original trim exactly once; a proof may not assume an indexed array is nonempty
because this particular producer is split. Unknown/non-String callees must retain their established
refusal. The patch selects only a nonoptional indexed lookup of an exact bare-String array followed by
the original zero-argument optional trim member call. It also reconstructs the runtime-declared
String trimStart/split result evidence lost in a fresh lowering. Nullable element arrays, optional
array/call shapes, lookalike methods and other members receive no new proof. No detector rewrite
is included. Builder2 separately advises that generic lazy nonliteral `??` fallback work
is in flight; this request does not assume it has landed or covers the optional-member call.

The libGDX config/document assignments at registration lines 51/52 are a separate approved
follow-on, accepted by Foreman71091a29. Their assignment-only witness emits both existing-family
adapters and original-body readonly helpers. The full descriptor still requires its real detector;
this witness supplies no substitute detector or registry qualification. Normal Types remains the
sole builder2 shared run, followed by builder3's one particle run and all eight approved callback
controls. No SDK, generated headers, ledger counts or package/header promotions change here.

## Direct compiler qualification

`scripts/particleRegistrationCompilerOracle.mjs` retains the untouched original helper and detector
expression, strictly checks the source, and compiles/runs their production-emitted C++. The controls
cover nine JSON roots/errors, seven detector inputs, 24 guarded reads and seven indexed evaluation
cases, including receiver-before-index order and exact receiver/index exceptions. The native helper
returns the retained parsed object; native aliases observe a write to that same object, distinguish
null from an absent key and retain it until the last view is released.

The compiler patch carries focused source tests. The current qualification reports 36 source
assertions plus 29 preceding-stack parity cases; nullable/undefined String element and optional-array
controls are parity-only because their existing generic lane is outside the new bare-String selection. The
`node:assert` adapter is explicitly not Vitest. The artifact records exact inputs and results.
The earlier `particleRegistrationBoundaryOracle.mjs` remains a frozen-stack counterexample and
intentionally expects the old refusal/native diagnostic; use the new compiler oracle for this patch.

These are compiler prerequisite controls. No current normal SDK, full original registration,
installed callback/header, ledger or package promotion is implied. Builder2 remains the sole normal
Types writer; the dependent single particles run and all eight callbacks remain queued.
