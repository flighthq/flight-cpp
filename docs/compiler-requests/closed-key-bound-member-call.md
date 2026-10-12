# Finite-key calls on a bound host object

`console[method](message)` where `method` is a closed union of console method names is a call through a
finite key on a host object the profile binds member by member. The emitter treated `console` as an
unevidenced receiver and refused with `cpp-closed-key-access-without-object-storage`, because it looked
for represented object storage that a host binding does not have.

## Minimal TypeScript

```ts
type Method = 'debug' | 'error' | 'info' | 'log' | 'warn';
declare function methodFor(level: number): Method;
export function write(level: number, line: string): void {
  console[methodFor(level)](line);
}
```

## Expected C++ (flight-cpp profile, console members bound)

```cpp
([&]() -> void {
  const auto member_key = method_for(level);
  const auto member_argument = line;
  if (member_key == flight::String("debug")) { flight::host::console_debug(member_argument); return; }
  // ... one branch per proven key ...
  throw flight::TypeError(flight::String("console[key] is not a function"));
}());
```

The key's type must be proven a subset of the bound members, the result void, and the argument one
string.

## Carrying workaround

[`compiler-closed-key-external-member-call.patch`](../../source-patches/compiler-closed-key-external-member-call.patch),
with the runtime's `console_log/info/warn/error` beside `console_debug` and all five bound as `console`
members (`bindings/headless.json`). The one-string argument rule is the contract those runtime sinks
declare. The binding schema cannot yet state primitive member parameters (member `parameters` are
object-construction contracts), so a future void member with a different arity would fail at the header
compile gate rather than be mis-lowered.

## Not covered: variadic console arguments

See [console-variadic-arguments](console-variadic-arguments.md).
