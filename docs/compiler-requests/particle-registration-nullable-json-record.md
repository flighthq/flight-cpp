# Nullable JSON record in particle registration

Flight `a62784923f3be814463286c9fb28edfe6ab15789`, compiler
`806d257d2f4100de745cc97340e61c2f301b0f24`, composed compiler stack 111.
Foreman675731df authorizes a minimal counterexample, direct controls and scope proposal here.
This request supplies no production compiler/runtime change or new ABI.

The private `parseJsonObject` at original `registerBuiltInParticleFormats.ts:185` parses once,
returns the original object only after the null/object/nonarray guard, and returns null on any
caught failure. The emitted native signature is `optional<Record<String,Any>>`, but its object
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

The bounded proposal is to investigate a nullable extension of the existing private JSON
producer/read-view analysis. A success path must retain the original parsed owner; a null path must
remain null; original parse/catch timing must remain intact. Any selection must prove that the
private result stays inside known readonly consumers and cannot escape, mutate or expose a public
Record ABI. This requires an explicit scope decision before implementation. The runtime control
shows what existing storage can represent; it does not choose the compiler's carrier or authorize
a Record conversion. Later optional-presence and nested JSON member boundaries remain separate.

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
through an optional chain on a builtin string receiver obtained by indexing. Its separate proposed
scope is to recover the existing builtin String method's known return evidence and preserve the
optional receiver guard, with each receiver/index evaluated once. No public function ABI or runtime
primitive changes are proposed. The absent receiver must remain undefined, and the present branch
must invoke the original trim exactly once; a proof may not assume an indexed array is nonempty
because this particular producer is split. Unknown/non-String callees must retain their established
refusal. This also requires a scope decision before implementation. No detector rewrite or new
compiler fix is included. Builder2 separately advises that generic lazy nonliteral `??` fallback work
is in flight; this request does not assume it has landed or covers the optional-member call.

The libGDX config/document assignments at registration lines 51/52 are a separate approved
follow-on, accepted by Foreman71091a29. Their assignment-only witness emits both existing-family
adapters and original-body readonly helpers. The full descriptor still requires its real detector;
this witness supplies no substitute detector or registry qualification. Normal Types remains the
sole builder2 shared run, followed by builder3's one particle run and all eight approved callback
controls. No SDK, generated headers, ledger counts or package/header promotions change here.
