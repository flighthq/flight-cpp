# String conversion of an erased value needs a runtime contract

TypeScript's `String(value)` accepts an `unknown` value. The C++ emitter currently lowers that call to
`flight::to_string(Any)`, but the runtime deliberately provides no such overload, so the generated
header fails native compilation instead of refusing during emission:

```text
no matching function for call to 'to_string'
```

## Minimal TypeScript

```ts
export function failureMessage(cause: unknown): string {
  if (cause instanceof Error) return cause.message;
  return String(cause);
}
```

## Expected behavior

The compiler must either lower the call through a runtime operation that implements the represented
ECMAScript string-conversion domain, or refuse it before writing C++. Primitive, null, and undefined
values have defined spellings; symbols throw; objects may require source-visible conversion hooks that
`flight::Any` does not currently retain. Silently choosing a generic object string would lose custom
conversion behavior and is not an equivalent fallback.

## Affected module

`@flighthq/image` `imageResourceReference.ts`, in the non-`Error` arm of
`createImageResourceFailure`. The checked `instanceof Error` arm is already handled by the separate
erased-Error projection work; this request concerns only conversion of the remaining erased value.

## Carrying workaround

None. Replacing the value with a fixed message would discard a diagnostic the source deliberately
retains, and the current runtime cannot reproduce arbitrary object conversion faithfully. The retained
declarations remain a native compile failure until the compiler refuses the call or a runtime contract
is defined.
