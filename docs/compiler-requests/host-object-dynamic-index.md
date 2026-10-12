# Dynamic index on a bound host object must refuse

A host object the profile binds member by member (`console`, `performance`) has no subscript. An open
key into it, whether a `string`, an `any` cast, or a `Record` cast of the host, used to fall through to
the generic element lowering. That emitted an index on the host carrier, which does not compile and
selects no member:

```cpp
static_cast<flight::Any>(host::console)[static_cast<size_t>(name)](line);
```

## Minimal TypeScript

```ts
export function f(name: string, line: string): void {
  (console as any)[name](line);
}
```

## Expected

A structured refusal, `cpp-host-object-dynamic-index`, naming the open key. A finite key proven to name
bound members is a separate, supported case
([closed-key-bound-member-call](closed-key-bound-member-call.md)).

## Carrying workaround

[`compiler-host-object-dynamic-index-refusal.patch`](../../source-patches/compiler-host-object-dynamic-index-refusal.patch)
refuses at the start of element emission whenever the receiver, after stripping casts, is a bound host
object. Finite-key member calls are dispatched before the callee element is emitted, so they are
unaffected.
