# A null guard on a member of an erased external type does not narrow the member

`if (pass.encoder !== null) { pass.encoder.end(); }`, where `encoder: GPURenderPassEncoder | null` names an
external host type, is refused with `cpp-member-projection-without-present-storage`. The lowering's program
does not resolve the external type, so the checker reports the member as `any` and never narrows it, and the
read inside the guard is lowered as if it could be absent.

## Minimal TypeScript

```ts
export interface Pass { encoder: GPURenderPassEncoder | null }
export function end(pass: Pass): void {
  if (pass.encoder !== null) { pass.encoder.end(); pass.encoder = null; }
}
```

## Expected C++

The guarded read is the present payload, and the write of `null` clears the storage:

```cpp
if (pass->encoder.has_value()) { pass->encoder.value().end(); pass->encoder = std::nullopt; }
```

## Affected modules

`@flighthq/render-wgpu` render-pass lifecycle (`wgpu_render_pass.hpp`).

## Carrying workaround

`source-patches/compiler-erased-external-member-presence.patch` extends the lowering's existing syntactic
presence recovery for erased bindings to `binding.member` paths, including its invalidation by an assignment
to the member or its receiver. It expires when the program resolves the external types or upstream extends
the recovery to member paths.
