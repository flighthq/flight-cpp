# A Proxy returned as its nominal target must retain its traps

## Minimal reproduction

```ts
interface Runtime {
  binding: object | null;
}

export function guard(runtime: Runtime, report: () => void): Runtime {
  return new Proxy(runtime, {
    set(target, property, value) {
      if (property === 'binding') report();
      target[property as 'binding'] = value;
      return true;
    },
  });
}
```

The returned JavaScript value is the Proxy. A subsequent `guard(runtime, report).binding = null`
invokes the trap and then forwards the write.

## Current C++ shape

The compiler correctly creates a `StructuralRef` backed by `ProxyRowOwner`, but immediately casts it
back to `Ref<Runtime>` to satisfy the declared return type. Nominal recovery returns the original
native `Runtime` pointer. The `StructuralRef` and its proxy owner are discarded, so later member
writes cannot invoke the trap.

## Required C++ shape

The returned carrier must preserve Proxy identity and route represented writes through the proxy
owner. A structural writable return is sufficient for this source shape; a proxy-capable nominal
reference would also be valid. Recovering the unproxied target is not.

## Affected Flight module

- `@flighthq/entity/packages/entity/src/guards.ts#createGuardedEntityRuntime` at Flight
  `a62784923f3be814463286c9fb28edfe6ab15789`

The upstream guard suites assert both that a binding write reports `binding-slot` and that the write
still lands.

## Repository workaround and expiry

The focused native entity suite covers the structural entity-slot proxy, guard enable/disable,
reporter uninstall, forwarded mutation, and both diagnostic descriptions. It does not claim the
nominal binding-slot return as working. No source patch, override, or generated-header edit is
carried.

This request expires when `create_guarded_entity_runtime` returns a carrier whose direct `binding`
write both reports and reaches the original runtime.
