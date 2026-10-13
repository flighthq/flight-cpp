# An array written into a readonly-row member needs the row projection

A construction member declared as a readonly array of readonly rows is assigned an array of the values
themselves. TypeScript stores the same objects; C++ refuses the write because the member's element type is a
`StructuralRef<RowReadonly<RowOf<Ref<T>>>>` and the value's is `Ref<T>`, and the emitted assignment has no
conversion between them.

## Minimal TypeScript

```ts
interface Entry { readonly name: string }
interface Box { readonly entries: readonly Readonly<Entry>[] }

export function createBox(entries: Entry[]): Box {
  const out = allocateEntity<Box>();
  out.entries = entries;
  return finishEntity(out);
}
```

## Observed

```cpp
static assertion failed: structural row write has an incompatible value type --
Array<flight::Ref<types::Entry>> written where Array<StructuralRef<RowReadonly<RowOf<Ref<Entry>>>>> is declared
```

The two element types describe the same subject: a row over `Ref<Entry>` is a projection of that reference, not a
different object. Only the container has to be rebuilt, and the projection itself reinterprets nothing.

## Affected

Five construction sites at this pin, all with the same shape: `@flighthq/requirement` `requirementSet.ts`
(`requirements`), `@flighthq/registry-codegen` `registryCodegen.ts` (`entries`), `@flighthq/requirement-codegen`
`requirementCodegen.ts` (`entries`), `@flighthq/animation` `animationStateMachine.ts` (`channels`) and
`animationLayerStack.ts` (`channels`).

## Carried by repairs

`repairs/emission-repairs.json`, kind `project-array-at-row-write`, five entries -- one per site, each naming the
member it applies to and carrying the identity precondition that makes the rebuild safe: the written expression is a
LOCAL or a temporary that no caller retains, and its final use is that write, so its array handle is unobservable
while `flight::array_of` preserves every element's identity. That precondition is why the repair is declared per
site rather than derived: in the general case the array handle at a row write IS observable, and rebuilding it
would be a different program.

Expiry: when the emitter projects an array into the element type its member declares. A repair that stops matching a
generated header fails `sdk:check`, so these cannot outlive the defect silently.
