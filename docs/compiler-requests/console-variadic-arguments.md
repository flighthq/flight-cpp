# Variadic console calls with non-string arguments

`console.log(prefix, data)` renders each argument with the engine's own formatting: objects are
inspected, and arguments are separated by spaces. That formatting is engine-specific; browsers and Node
differ. This runtime's console members are one-string sinks, so a call that passes an object, or more
than one argument, has no faithful target and is refused with `cpp-closed-key-external-member-arguments`
(through a finite key) or the binding's own call rules (direct).

## Minimal TypeScript

```ts
export function write(method: 'log' | 'warn', prefix: string, data: Record<string, unknown>): void {
  console[method](prefix, data);
}
```

## What a lowering needs

A declared console-formatting contract for this profile: how an object argument is rendered, for example
a stated JSON serialization with documented differences from engine inspection. Then a variadic member
binding that names it. Until that contract exists, the call stays refused instead of picking a format.

## Affected modules

`packages/log/src/log.ts` (`_writeConsoleCaptureEntry`: `console[method](prefix, data)` for non-string
log data). This is the next blocker for log.ts after the finite-key lane.
