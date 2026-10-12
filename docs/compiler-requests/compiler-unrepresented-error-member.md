# An Error member the runtime does not carry must be refused, not emitted

An Error read of a member other than `name` or `message` has no representation on either carrier, and
the emitter was not refusing it — it emitted the member access verbatim, which on an erased value is a
member access on `flight::Any` and cannot compile.

## Minimal TypeScript

```ts
export function stack(cause: unknown): unknown {
  if (cause instanceof Error) return cause.stack;   // erased carrier: no member to read
  return null;
}

export function typedStack(error: Error): unknown {
  return error.stack;                               // typed carrier: the runtime declares no such member
}
```

## Expected behavior

`flight::Error` declares `name` and `message`. A `stack`, a `cause`, or anything else the source
language allows on an Error is outside what this profile represents, and neither carrier can answer it:
the erased `flight::Any` has no members at all, and the typed `flight::Error` declares none of them. The
read is therefore outside the represented domain and belongs in a structured refusal — the same
treatment every other unrepresented construct gets — rather than in emitted code that only fails later,
in a compile gate, pointing at a header the author never wrote.

There is no faithful lowering to reach for here: lowering would require the runtime to carry the
member, and inventing a `stack` or `cause` field to make the read type-check would fabricate data the
source never had. A runtime that grows the member earns the lowering; until then the gap is named.

## Observed at `2b687ea` / `a627849`

Both reads emitted, with no diagnostic:

```cpp
inline flight::Any f(flight::Any cause) {
  if ((cause instanceof flight::Error)) { return cause.stack; }   // member access on flight::Any
  return nullptr;
}
```

The erased case is worse than the typed one because the guard itself also leaked (`instanceof`), which
[the erased-read request](compiler-erased-error-view-and-instanceof-guard.md) covers.

## Affected modules

`packages/log/src/log.ts` (`serializeLogError` reads `value.stack` and `serializeLogError(value.cause)`).

## Carrying workaround

[`compiler-unrepresented-error-member.patch`](../../source-patches/compiler-unrepresented-error-member.patch)
refuses the read with `cpp-error-member-unrepresented`, for an erased or a typed receiver, and leaves
`name` and `message` to the lowering above. It expires when the runtime or a declared external binding
represents the member and the emitter lowers it.

This refusal is deliberately distinct from the absence-channel refusals (`nullable-optional-presence`,
the optional-storage present-payload class): those are about storage that may hold absence, and this is
about a member that does not exist. Both stay named separately so neither gets absorbed into the other.

## Update: `stack` and `cause` are now represented

The runtime now declares two of these members under a stated profile contract
([runtime-semantics.md](../runtime-semantics.md)): `Error::stack()` is always absent, because this
runtime captures no call stack, and `Error::cause()` is the exact value given to ES2022
`ErrorOptions`. Following this request's own rule, a runtime that grows the member earns the lowering,
so [`compiler-error-stack-cause-presence.patch`](../../source-patches/compiler-error-stack-cause-presence.patch)
lowers those two reads and their presence tests. It is applied after this request's refusal patch and
narrows the refusal to skip exactly `stack` and `cause`. Every other member the runtime does not
declare is still refused with `cpp-error-member-unrepresented`. The upstream lowering is requested in
[compiler-error-stack-cause-presence](compiler-error-stack-cause-presence.md).
