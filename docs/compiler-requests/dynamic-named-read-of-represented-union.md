# A dynamic named read of a represented union must preserve its active arm

`Object.keys` plus a computed property read emits through `flight::NamedProperties`. A member whose
C++ storage is a `std::variant` is currently reported as unrepresented even when every alternative is
an ordinary `flight::Any` language value.

The minimal source shape is:

```ts
interface Value {
  name: string | null;
}

export function copyName(out: Value, source: Readonly<Value>): void {
  const sourceFields = source as unknown as Record<string, unknown>;
  const outFields = out as unknown as Record<string, unknown>;
  for (const key of Object.keys(sourceFields)) outFields[key] = sourceFields[key];
}
```

At source revision `a627849`, `@flighthq/materials` exercises the same shape in `material.ts`.
`cloneMaterial` and `equalsMaterial` enumerate a material, then dynamically read every declared field.
The generated `Material.name` carrier is:

```cpp
std::variant<flight::String, flight::Null, flight::Undefined> name;
```

Every arm has an exact `flight::Any` representation, but `detail::any_from` rejects the outer variant.
The first dynamic read therefore terminates with:

```text
flight::NamedProperties has no erased reading of property 'name'
```

The inverse write has the same missing rule: an `Any` containing one exact alternative cannot be
assigned back into a variant-valued member, so a clone could not preserve the active sentinel even if
the read succeeded.

The carried runtime binding visits the active alternative for reads and, for writes, selects the first
alternative that accepts the `Any` without coercion. It preserves the variant's declared storage,
active sentinel, value, and object identity; if no arm accepts the value the checked write still
refuses. This is not a general opaque-variant approximation: a variant with an unrepresented active
arm remains unrepresented.

The compiler may instead emit a statically typed copy/equality path when the enumerated key set is
closed. It must not emit a dynamic access that deterministically throws for a source operation whose
complete union domain the runtime already represents.

The request expires when dynamic named reads and writes preserve represented union arms, or when the
compiler lowers this closed `Object.keys` loop without erased access. Native materials tests cover
String, Null, and Undefined reads/writes indirectly through clone/equality and a focused runtime test
pins direct variant conversion.
