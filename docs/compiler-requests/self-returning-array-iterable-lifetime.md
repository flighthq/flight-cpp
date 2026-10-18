# A for-of over a self-returning array call must keep the receiver alive

`parseMd5MeshRequirements` and `parseObjRequirements` iterate a sorted spread:

```ts
for (const feature of [...features].sort()) { ... }
```

The emitter lowered that to `for (auto feature : (lambda).sort())`. `flight::Array::sort()` returns
`Array&` to its receiver (the JS in-place return), and a reference returned from a temporary receiver
dangles before the loop body runs — the loop silently iterated zero elements. Both requirement
builders consequently returned empty requirement sets even though the feature scans found features.

## Lowering

When a for-of iterates a call to a self-returning array method (`sort`, `reverse`, `fill`) whose
receiver expression is a fresh temporary (a spread/lambda result or an array literal), the emitter
binds the call's array value to a local first and iterates that local. A named receiver keeps the
direct in-place form, because it outlives the loop.

The source's temporary is not observable anywhere else, so iterating the bound value is the same
array in the same order; no owner identity a program could rely on is changed.

## Disposition

Carried by `compiler-self-returning-array-iterable-lifetime`, with focused tests for both shapes and
a native probe proving the dangling form yields zero elements while the bound form yields all. It
expires when the emitter keeps the temporary receiver alive for the loop by another mechanism.
