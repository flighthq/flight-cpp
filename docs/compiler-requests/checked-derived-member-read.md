# A member read through a base-to-derived interface assertion has no lowering

`(target as WgpuTextureRenderTarget).view` reads a member of an object asserted from a native interface base
to a derived owner. TypeScript checks nothing at runtime, and JavaScript reads the member of whatever object
it has. The pinned backend refuses the assertion with `cpp-reference-assertion-without-heritage`, or would
need an unchecked cast, which reads garbage when the object is not the derived type.

## Minimal TypeScript

```ts
export interface Target { context: NativeContext | null; width: number }
export interface Texture extends Target { readonly context: null; view: NativeView; label?: string }
export function f(t: Target): NativeView { return (t as Texture).view; }
```

## Expected C++

The receiver is evaluated once. The member is read from the SAME live owner when the allocation is the derived
type or a subclass; otherwise from the member cell of another owner that declares it; otherwise it is the
member's absent state (for a declared opaque host-handle absence carrier, the empty handle). It never throws:

```cpp
const auto asserted_receiver = t;
if (const auto recovered_owner = flight::recover_owner<Texture>(asserted_receiver)) return recovered_owner->view;
if (const auto structural_member = flight::StructuralMemberView<Target>(asserted_receiver)...) ...;
return flight::absent_host_handle<host::View>();
```

A member type with no absent state still refuses, and a bare value assertion keeps its existing refusal.

## Affected modules

`@flighthq/render-wgpu` `bindWgpuRenderPassTarget` (the texture-target arm).

## Carrying workaround

`source-patches/compiler-checked-derived-member-read.patch`. `flight::recover_owner` recovers the same
allocation by identity only where `make_ref` recorded a `flight_native_base` record, never by a cast. Where a
narrowed value escapes into derived-typed slots rather than being read once, see
[narrowed-value-escape.md](narrowed-value-escape.md).
