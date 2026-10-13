# A structural view of an entity-allocated carrier is invisible when the carrier derives from the row's subject

`shared_object()` accepts an object only when the owner's recorded native type is EXACTLY the row's
subject type. A value the emitter produces through `createShapedRun()` is an instance of the anonymous
carrier struct, which **derives from** `ShapedRun`, so a structural row over `ShapedRun` resolves to
nothing: `->` dereferences null and the process panics. The check that causes this is deliberate — it
exists because two flattened TypeScript interfaces become two *sibling* C++ structs, where C++
inheritance cannot prove the relationship — but it also rejects the case where the relationship **is**
C++ inheritance.

At Flight `a62784923f3be814463286c9fb28edfe6ab15789`.

## Minimal TypeScript

`@flighthq/textshaper`'s `textShaperPool.ts`. The pool hands the released run to a caller-supplied guard:

```ts
export function setShapedRunReleaseGuard(guard: ((run: Readonly<ShapedRun>) => void) | null): void {
  _releaseGuard = guard;
}

export function releaseShapedRun(run: ShapedRun): void {
  if (_pooled.has(run)) {
    _releaseGuard?.(run);
    return;
  }
  ...
}
```

This is upstream's `setShapedRunReleaseGuard` fixture, which asserts the guard receives the run:
`setShapedRunReleaseGuard((released) => seen.push(released)); ... expect(seen).toEqual([run])`.

## Emitted C++

The guard's parameter is a row view over `ShapedRun`, and the call passes the run straight in:

```cpp
inline std::optional<std::function<void(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::ShapedRun>>>>)>> release_guard = std::nullopt;

inline void release_shaped_run(flight::Ref<flight::types::ShapedRun> run) {
  if (pooled.has(run)) {
    ([&]() { auto optional_chain_receiver = release_guard; if (!optional_chain_receiver.has_value()) return; optional_chain_receiver.value()(run); }());
    return;
  }
  ...
}
```

The run the pool hands out is built by `createShapedRun()`, whose carrier is the anonymous struct:

```cpp
struct advance_width_direction_font_glyph_count_glyphs_script_entity_runtime_key_69a845f27c0212e6
    : public flight::types::ShapedRun { ... };
```

and `acquire_shaped_run()` returns it as `Ref<ShapedRun>` — an upcast, so the pointer is valid. The
conversion to the guard's parameter then builds the row, and the row's read path refuses it:

```cpp
std::shared_ptr<object_type> shared_object() const noexcept {
  if (!owner_ || owner_->native_type() != typeid(object_type)) return {};
  ...
}
```

The owner records the **derived** carrier's type, `typeid(object_type)` is `ShapedRun`, they differ, and
the view answers with nothing.

## Observed

Driving the pool directly through its public API:

```cpp
auto pooled = flight::textshaper::acquire_shaped_run();   // pool empty -> createShapedRun()
RunView view_from_pool(pooled);
view_from_pool.shared_object() == nullptr                 // TRUE  -- the view is invisible

auto plain = flight::make_ref<flight::types::ShapedRun>(); // exact dynamic type
RunView view_from_plain(plain);
view_from_plain.shared_object() == nullptr                 // false -- the same build works
```

`pooled == nullptr` is false in both cases: the run is live. Only the view over it is empty. Reading
through it then panics, reached from upstream's own fixture:

```
thread 92712 panic: member access within null pointer of type 'object_type'
  (aka 'flight::types::ShapedRun')
  generated_textshaper_parity_test.cpp:240:78 in lambda(released) { ... released->script ... }
  generated/include/flight/textshaper/text_shaper_pool.hpp:42:110 in release_shaped_run
```

So `setShapedRunReleaseGuard`'s callback cannot read the run it is handed, for any run the pool itself
produced — which is every run the guard is ever called with.

## Impact

`@flighthq/textshaper`'s pool guard is opt-in and dev-only (`enableTextShaperGuards`), so the blast
radius is bounded: the double-release warning cannot fire, and instead of warning the process panics.
That is still worse than not installing a guard.

The shape is general, not one call site: any emitter-chosen row view over a base type, handed an object
the runtime allocated as a derived carrier, is invisible the same way.

## Expected

Either the emitted callback parameter should name the exact carrier the pool produces, or the runtime
should accept a native type that provably derives from the row's subject. The second must not weaken the
exact check's actual purpose: two flattened TypeScript interfaces are *sibling* C++ structs, and there
the relationship genuinely cannot be proven by inheritance. A `std::derived_from` (or a checked
`dynamic_cast`) test distinguishes the two cases rather than relaxing the check for both.

## Affected modules

`@flighthq/textshaper`'s `textShaperPool.ts` (`setShapedRunReleaseGuard` / `releaseShapedRun`).

## Carrying workaround

None carried, and none is possible from the TypeScript side: the defect is in how the emitted row view
relates to the carrier the pool allocates. The parity suite asserts the half that is observable without
reading through the view (that the guard fires exactly once) and records the read-through as blocked —
see `tests/generated_textshaper_parity_test.cpp`.
