# Scoped emission must retain semantic context

An `emitPathPrefixes` filter currently shrinks the backend session as well as the files written. A
selected consumer then cannot resolve imported aliases and exact owners from lowered but unselected
dependencies.

## Minimal TypeScript

```ts
// provider.ts
export type Value = string | number;

// consumer.ts
import type { Value } from './provider.js';
export function retain(value: Value): Value { return value; }
```

Compile both modules while requesting output only for `consumer.ts`.

## Expected C++

The backend session receives both semantic modules, but writes only the selected header:

```text
session modules: Consumer, Provider
emitted files:   Consumer.hpp
report:          Consumer emitted; Provider skipped
```

`Consumer.hpp` must use the same represented `Provider::Value` carrier as an unscoped run.

## Affected modules

All bounded diagnostic runs, especially Scene3D GL and Canvas runs that need exact material, texture,
node, and callback owners from dependencies.

## Carrying workaround

[`compiler-scoped-emission-semantic-context.patch`](../../source-patches/compiler-scoped-emission-semantic-context.patch)
separates the dependency-complete session context from the output selection loop. It expires when
upstream scoped emission preserves the complete emittable semantic context.
