# A nullish-coalesce fallback of `null` is wrapped into an *engaged* optional, so its truthiness test dereferences null

A local whose type is `T | null` is stored as `std::optional<flight::Ref<T>>`, but the `??` that
initializes it returns a nullable `Ref<T>` whose "no value" case is a null pointer, and that pointer is
then wrapped into the optional unconditionally. The optional is therefore **always engaged**, so the JS
truthiness test `local && ...` lowers to `optional::has_value()` — always true — and the member access
below it dereferences a null `Ref`. This is a mislowering that *compiles*, so no header-compile gate
sees it; it is a null-pointer panic at run time.

At Flight `a62784923f3be814463286c9fb28edfe6ab15789`.

## Minimal TypeScript

Reduced from `@flighthq/textlayout`'s `textLayout.ts` wrapping path. The initializer is upstream's
verbatim; the surrounding function is reduced so the null branch is reachable:

```ts
interface Group {
  positions: number[];
  lineIndex: number;
  width: number;
  endIndex: number;
}

export function trimTrailingSpace(
  activeGroup: Group | null,
  groups: Group[],
  lineIndex: number,
): void {
  const trimTarget = activeGroup ?? (groups.length > 0 ? groups[groups.length - 1] : null);
  if (trimTarget && trimTarget.positions.length > 0 && trimTarget.lineIndex === lineIndex) {
    const trailingW = trimTarget.positions[trimTarget.positions.length - 1];
    trimTarget.width -= trailingW;
    trimTarget.endIndex--;
  }
}
```

When `activeGroup` is `null` and `groups` is empty, `trimTarget` is `null`. JavaScript's `&&` then
short-circuits on `null` and the body is skipped.

## Emitted C++

```cpp
std::optional<flight::Ref<flight::types::TextLayoutGroup>> trim_target =
    std::optional<flight::Ref<flight::types::TextLayoutGroup>>{([&]() -> flight::Ref<flight::types::TextLayoutGroup> {
      auto nullish_coalesce_left = active_group_capture.read_binding();
      if (nullish_coalesce_left.has_value()) return nullish_coalesce_left.value();
      return ((static_cast<double>(groups.size()) > 0.0)
                  ? groups.element((static_cast<double>(groups.size()) - 1.0))
                  : nullptr);
    }())};
if (flight::to_boolean(((trim_target && (static_cast<double>(trim_target.value()->positions.size()) > 0.0)) &&
                       (trim_target.value()->line_index == line_index_capture.read_binding())))) {
```

The lambda returns `flight::Ref<T>`, which is `nullptr` on the `null` branch. Wrapping that value in
`std::optional<...>` produces an engaged optional holding a null pointer, so `trim_target &&` is
`has_value()` — true — and `trim_target.value()` is a null `Ref` whose `->positions` panics.

`flight::to_boolean` cannot help: the `&&` is evaluated as C++ operator `&&` *before* the conversion,
and it is the left operand's contextual bool that selects the branch.

## Observed

Driving the public API through `@flighthq/textlayout`'s own upstream fixture
(`textLayout.test.ts`, "wraps at word boundary when line exceeds width" — `text: 'hello world'`,
`width: 50`, `wordWrap: true`, `multiline: true`, fixed measure of 10px per character):

```
thread 88235 panic: member access within null pointer of type 'element_type'
  (aka 'flight::types::TextLayoutGroup')
generated/include/flight/textlayout/text_layout.hpp:446:93 in build_groups
  ... text_layout.hpp:716:3 in compute_text_layout
```

Reached whenever `wordWrap` is on and the **first** segment (the first word plus its trailing space)
already exceeds the wrap width, so the wrap takes the trim path before any group has been placed — i.e.
exactly the case where `activeGroup` is absent and `groups` is empty. Measured directly against the
emitted tree (fixed measure, multiline on, no maxLines):

| text | width | wrap | result |
| --- | --- | --- | --- |
| `"a b"` | 13 | on | **panic** |
| `"ab cd"` | 15 | on | **panic** |
| `"hello world"` | 50 | on | **panic** |
| `"ab cd"` | 25 | on | 2 lines |
| `"hello world"` | 1000 | on | 1 line |
| `"hello world"` | 50 | off | 1 line |

Every non-panicking input above produces the line count upstream expects, so the wrapping logic itself
is correct; only this guard path is not.

## Expected

Either the `null` branch of `??` must materialize an **empty** optional (leaving `trim_target &&` as
the right absence test), or a `T | null` local whose `T` is a reference must be stored as a nullable
`Ref<T>` with its truthiness lowered as `!= nullptr`. In both cases `trimTarget` must read as absent
when the coalesce fell through to `null`, and the guarded member access must be skipped rather than
evaluated.

## Affected modules

`@flighthq/textlayout`'s `textLayout.ts` (`buildGroups`). Any package reaching that path with
`wordWrap` on and a narrow container is affected — `@flighthq/scene2d-canvas` calls
`computeTextLayout` for text labels, so a wrapped label in a narrow field takes this path.

The pattern is general: every `const local = maybe ?? fallback` where the fallback is `null` and `local`
is then tested for truthiness has the same shape, so this is not one call site.

## Carrying workaround

None carried. A behaviour-preserving source rewrite exists — replacing the truthiness test with an
explicit `!== null` comparison, which is equivalent for an object-or-null — but this repository
carries no patch for it: `textlayout` compiles and its non-wrapping paths work, so nothing forces the
package to ship ahead of the fix, and a patch would rewrite a pinned sibling's source for a defect the
compiler owns.
