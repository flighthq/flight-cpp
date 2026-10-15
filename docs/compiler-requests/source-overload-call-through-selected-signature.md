# A call to an overloaded function must lower through the overload TypeScript selected

A source module declares overload signatures over a single implementation. TypeScript resolves each call
to one overload and types the arguments and result by it. The emitter lowers the call through the
implementation signature instead, so the arguments meet the implementation's wider union parameters and
the result is the implementation's wider return type.

## Minimal TypeScript

```ts
export function create<T extends object>(manager: Manager, target: T): Handle<T>;
export function create<T extends object>(target: T): Handle<T>;
export function create<T extends object>(managerOrTarget: Manager | T, target?: T): Handle<T> {
  /* ... */
}

export function timer(manager: Manager): Handle<object> {
  return create(manager, {}); // selects the first overload
}
```

## Required C++ shape

Either one C++ function per overload, each forwarding to the implementation with the arguments it
selected, or a call lowered with the selected overload's parameter and result types. Then `{}` is checked
against `T`, not against `Manager | T`.

## Affected modules

- `createTweenTimer` in `@flighthq/tween` (`timer.ts`): refused, "contextual union value type
  `flight::Ref<anonymous>` is not a represented runtime domain". Measured: calling `makeTween` and
  `addTweenToManager` directly (exactly createTween's manager branch) removes the refusal, but the empty
  `{}` target then fails as an anonymous struct, or as `auto target = {}` when bound as `object`. So no
  patch is carried; the `tween-timer-empty-object-target` override supplies it, passing a fresh member-less
  object for each `{}` through the manager overload.
- `createTween` itself (`tween.ts`), emitted but uncompilable: the brand test `isTweenManager(managerOrTarget)`
  is handed the implementation's `std::variant<Ref<TweenManager>, T>` rather than the value it holds, and the
  manager branch reads `propsOrOptions` without its optional layer. The
  `tween-erased-target-and-overload-dispatch` override unwraps the variant to its value before the brand test
  and reads each branch's alternative through its presence.
- `createTexture` from `@flighthq/swf` (`swfImageTexture.ts`, `swfVideoHandler.ts`): carried by
  [`swf-image-texture-2d-constructor`](../../source-patches/swf-image-texture-2d-constructor.patch) for the
  image path and [`swf-video-texture-2d-constructor`](../../source-patches/swf-video-texture-2d-constructor.patch)
  for the video path. Both sites then meet the texture-lane override signature.
