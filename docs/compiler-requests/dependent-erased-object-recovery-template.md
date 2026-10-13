# Dependent erased-object recovery needs a template disambiguator

A checked assertion from `unknown` to an object reference lowers through `flight::Any::object_if<T>()`.
When that assertion appears inside a generic function whose surrounding expressions retain a dependent
owner, Clang parses the emitted `<` as an operator unless the member template is explicitly
disambiguated:

```ts
interface Payload { readonly id: string }
interface Box { readonly value: unknown }

export function recover<T extends Box>(box: T): Payload {
  return box.value as Payload;
}
```

The generated shape currently contains:

```cpp
erased_value.object_if<Payload>()
```

and an instantiated materials clone reports:

```text
error: use 'template' keyword to treat 'object_if' as a dependent template name
```

The required spelling is `erased_value.template object_if<Payload>()`. C++ permits that explicit
disambiguator for the same member-template call in non-dependent contexts as well, but the carried
compiler change adds it only while a dependent type parameter is active, keeping unrelated output
byte-identical.

This is syntax only: the same erased value performs the same exact-type lookup and returns the same
shared owner or throws on mismatch. It does not add a cast, select a different owner, copy an object,
or change failure behavior.

At Flight revision `a627849`, generic `copyMaterialFields<T>` reaches this path when preserving the
`standard` property block of an extended PBR material. The request expires when checked erased-object
recovery emits a valid dependent member-template call. A backend fixture pins the dependent spelling,
and the native materials clone/copy cases instantiate the resulting header.
