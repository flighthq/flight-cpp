# A member use on an indexed element the loop proves in bounds must emit

`@flighthq/scene3d-formats`'s `collectMd5MeshFeatures` and `collectObjFeatures` scan text line by
line:

```ts
const lines = source.split('\n');
for (let i = 0; i < lines.length; i++) {
  const line = lines[i].trim();
  ...
}
```

`lines[i]` has the plain element type `string`, so the only absence the C++ carrier can hold is the
bounds check. The emitter's presence lane nevertheless refused the member call
(`cpp-member-projection-without-present-storage`: "member trim on optional C++ storage requires
proven present payload"), which took both functions — and the two requirement headers that retain
their calls — out of the shippable tree.

## Lowering

A loop of the shape `for (let i = 0; i < array.length; i++)` proves its own index in bounds; the
same proof holds for the `let i = 0; while (i < array.length)` form a lowering produces. The backend
records the exact indexed-read expressions inside that body (the loop's own condition is the
evidence, and a body that mutates or otherwise loses the index proof marks nothing), and the
member-receiver lane allows those reads. The read keeps the array's required accessor
(`lines.element(i)`), whose bounds check throws exactly where the source's own
`undefined.member` would throw.

No other indexed read acquires the proof: an unguarded literal read still refuses (the pinned
dense-array control), and a nullable element member still refuses without a presence proof (the
focused control).

## Disposition

Carried by `compiler-loop-bound-indexed-element-member`, with focused tests for the loop-bound and
nullable-refusal cases. It expires when the backend proves an indexed read in bounds from its
enclosing loop without this collected set.
