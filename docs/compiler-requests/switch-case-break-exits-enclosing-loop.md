# A `break` nested inside a `case` leaves the enclosing loop instead of the `switch`

A `switch` over strings is lowered to an `if`/`else if` chain. The `break` that ends each case at its top
level is dropped, which is correct. A `break` nested in a statement inside the case, such as an early exit
`if (x) break;`, is copied through as a C++ `break`. The chain is no longer a `switch`, so that `break` binds
to the nearest enclosing loop and ends the whole iteration. The skipped work includes the loop's own
increment. Nothing is refused and the result compiles, so the code is silently wrong.

Measured at flight-compiler `2b687ea` with all 44 compiler source patches applied: 1 module emitted, 0
refused.

## Minimal TypeScript (measured)

```ts
export interface Step {
  kind: 'add' | 'skip';
  value: number;
}

export function total(steps: ReadonlyArray<Step>): number {
  let sum = 0;
  for (let i = 0; i < steps.length; i++) {
    const step = steps[i];
    switch (step.kind) {
      case 'add': {
        if (step.value === 0) break;
        sum += step.value;
        break;
      }
      case 'skip':
        break;
    }
  }
  return sum;
}
```

## Observed

```cpp
    while ((i < static_cast<double>(steps.size()))) {
      ...
          if (switch_value == flight::String("add")) {
            {
              if ((step->value == 0.0)) {
                break;            // leaves the while, not the case
              }
              (sum += step->value);
            }
          }
      ...
      (i += 1.0);
    }
```

Run natively over `[{ kind: 'add', value: 0 }, { kind: 'add', value: 5 }]`, `total` returns `0`.
JavaScript returns `5`. The same function written with `if (step.value !== 0) { ... }` and no nested
`break` returns `5`.

## Expected behavior

A `break` that targets the `switch` must end only the current case. When the switch is lowered to an
`if` chain, the nested `break` has to become a jump to the end of the chain, for example by wrapping the
chain in a `do { ... } while (false)` block, or with a label and `goto`. It must never be the C++ `break`
of an enclosing loop. A labelled `break` that does target the loop is unaffected.

## Affected modules

`@flighthq/particles` `applyParticleForces.ts`, in `accumulateForces`. `AttractorForce` and `VortexForce`
each exit early with `if (dist <= 1e-6) break;` and `if (mag === 0) break;`. A force outside its radius, or
one sitting on the particle, therefore discards every force after it in the list for that particle. Under
`linear` falloff, a force at exactly its radius discards them too. None of upstream's tests combine an
early-exiting force with a later one, so they pass either way.

## Carrying workaround

The `particles-forces-case-early-exit-guards` source patch rewrites the four early exits as guards around
the rest of each case: `if (dist > 1e-6) { ... if (mag !== 0) { ... } }`. The case then ends only through
its top-level `break`, which the lowering already handles. The rewrite is equivalent in JavaScript, because
everything after each `break` in the case is exactly what the guard now encloses.
