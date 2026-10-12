# A callable's parameter pack needs a typed buffer

A generic connector that buffers a slot's arguments to replay later is refused: the source buffers
`...args` as `any[]`, and the emitter will forward a dependent callable pack only as a terminal call
spread. Storing it would require naming the pack as storage, and erasing it into `flight::Any` would
copy every argument through a carrier that loses its type.

## Minimal TypeScript

```ts
export function connectLater<T extends (...args: any[]) => void>(slot: T): T {
  let lastArgs: any[] | null = null;
  const handler = ((...args: any[]) => {
    lastArgs = args;
    queueMicrotask(() => {
      if (lastArgs !== null) slot(...lastArgs);
    });
  }) as T;
  return handler;
}
```

## Expected C++

`T` lowers to `std::function<void(Args...)>`, so the buffer has an exact native type: the slot's own
parameter tuple, replayed with `std::apply`.

```cpp
template <typename... Args>
std::function<void(Args...)> connect_later(std::function<void(Args...)> slot) {
  auto last_args = std::make_shared<std::optional<std::tuple<Args...>>>();
  return [=](Args... args) {
    last_args->emplace(args...);
    flight::queue_microtask([=]() { if (last_args->has_value()) std::apply(slot, last_args->value()); });
  };
}
```

The tuple holds exactly the values the slot received, without erasure or extra copies through `Any`.

## Affected modules

`connectSignalDebounced` and `connectSignalThrottled` in `@flighthq/signals` (`throttle.ts`) are
refused with `cpp-dependent-parameter-pack-nonterminal-use`.

## Carrying workaround

Override [`signals-throttle-typed-argument-buffers`](../../overrides/manifest.json) supplies both
connectors with `std::optional<std::tuple<Args...>>` buffers. It closes when the emitter stores a
dependent callable pack in a typed tuple.
