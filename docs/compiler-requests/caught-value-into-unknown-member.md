# A caught value written into an `unknown` member needs an `Any` that can hold it

The refusal `cpp-contextual-union-missing-expression-type:optionalSingle` on `@flighthq/tray` `destroyTrayRuntime`
is not missing evidence. It is the emitter declining to spell a value the runtime cannot represent.

Measured at flight-compiler `2b687ea` with every carried patch, against flight `a627849`.

## Minimal TypeScript

```ts
export interface F { readonly error?: unknown; readonly step: 'native-resource' }
export function c(): F {
  try { throw 1; } catch (error) { return { error, step: 'native-resource' }; }
}
```

Refused with `contextual optionalSingle construction requires expression type evidence`. The same refusal occurs for:
- `{ error: error }`;
- any object literal that writes the catch binding straight into an optional `unknown` member (`{ e: error }`);
- the Tray shape inside `destroyTrayRuntime`:

```ts
} catch (error) {
  result = { failures: [{ error, step: 'native-resource' as const }], outcome: 'tray-destroy-failed' as const };
}
```

Writing the same member from a parameter `e: unknown` emits normally.

## Why the refusal is right

Every catch binding is emitted as `catch (const std::exception& error)`. The member `error?: unknown` is
`std::optional<flight::Any>`, and `flight::Any` has no form for a caught exception. There is no constructor from
`std::exception` and no `std::exception_ptr` kind:

```cpp
catch (const std::exception& error) { F{.error = std::optional<flight::Any>{error}}; }
// error: no matching constructor for initialization of 'std::optional<flight::Any>'
```

So supplying "evidence" for the binding would turn a refusal into a header that does not compile. Writing it
through a typed local (`const e: unknown = error`) does emit, as `auto e = error; std::optional<flight::Any>{e}`, and
fails the same way in C++.

[catch-recorded-slot-carrier.md](catch-recorded-slot-carrier.md) is the same gap for a local. There the fix is to
give the slot the exception carrier (`std::exception_ptr`). That is not available here, for two reasons:
- the destination is a declared member of a `@flighthq/types` shape, emitted as `std::optional<flight::Any>`;
- providers also write arbitrary non-exception values into that member.

## Required runtime capability

`flight::Any` needs a caught-value kind that owns the thrown object (a `std::exception_ptr`) and preserves what node
preserves:
- **identity:** the same caught object, compared `===` to itself, and a rethrow of the stored value rethrows the
  very object;
- **`typeof`:** `'object'` for an Error;
- **the checked Error view:** `instanceof Error` and `.message`, through the existing `AnyErrorAccessor`.

The compiler would then lower a catch binding that flows into `unknown` storage as that kind. This changes `Any`,
the public erased value every emitted header uses, so it is a runtime capability with its own semantic tests. It is
not a Tray-local patch, and none is carried.

## The next refusal behind it

`destroyTrayRuntime` has a second, independent blocker. After `if (result.outcome === 'destroyed') return result;`
the narrowed `result` is spelled as a fresh single-arm struct (`outcome_4c14…`). But `TrayDestroyCapabilityResult`
stores `destroyed` coalesced with the other discriminant-only arms in `outcome_bd96…`. So the return into
`TrayDestroyResult` is refused with `contextual union value type flight::Ref<outcome_4c14…> is not a represented
runtime domain`. It reproduces with:

```ts
export function n(result: DC): DR { if (result.outcome === 'destroyed') { return result; } return result; }
```

## Affected modules

`@flighthq/tray` `destroyTrayRuntime`, and through it `destroyTrayIcon`, whose emitted body calls the omitted
`destroy_tray_runtime`.

## Carrying workaround

None. A source rewrite that drops `error` or stringifies it would change what the caller observes.
