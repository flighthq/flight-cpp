# Upstream TypeScript test parity prototype

The pinned `@flighthq/flight` math and geometry test corpus has a useful mechanically lowerable
floor, but it is not uniform. At Flight revision
`7e2fc7df5378313ceaf21ba8d6ef39cc18a6f4ad`, the prototype classifies **410 of 1,539 tests
(26.6%)** as direct-lowering candidates:

| Package | Test files | Candidate tests | Total tests | Candidate fraction |
| --- | ---: | ---: | ---: | ---: |
| `@flighthq/math` | 15 | 218 | 325 | 67.1% |
| `@flighthq/geometry` | 28 | 192 | 1,214 | 15.8% |
| Total | 43 | 410 | 1,539 | 26.6% |

This is a conservative, reproducible lower bound rather than a claim that all 410 candidates already
compile. A candidate contains only `const` declarations and non-negated `expect` statements, uses one
of the native harness's implemented matchers, and stays inside the prototype's expression subset.
The checked-in pilot proves the full path for 18 of those candidates: all 17 tests in
`math/src/clamp.test.ts` and the geometry typed-array zero-fill test. The generated native executable
passes 18/18.

## What the prototype does

[`scripts/upstreamTestParityPrototype.mjs`](../scripts/upstreamTestParityPrototype.mjs) reads only the
two package-scoped `*.test.ts` corpora from the pinned checkout. Its dependency-free tokenizer:

1. finds `describe`, `it`, and `it.each` callbacks without confusing strings or comments for code;
2. preserves suite/test names and upstream source lines;
3. recognizes safe `const` declarations and `expect` calls;
4. maps imported camel-case symbols to generated `flight::<package>::snake_case` names;
5. lowers scalar expressions, `Math` constants/functions, `{ x, y }` vector literals, and the three
   typed-array constructors used by geometry;
6. lowers `toBe`, `toBeCloseTo`, range comparisons, `toBeNaN`, and sequence `toEqual` onto the small
   native harness; and
7. writes a C++ test plus a revision- and SHA-256-pinned census artifact.

[`tests/upstream_test_harness.hpp`](../tests/upstream_test_harness.hpp) owns the native
`describe`/`it` registration, failure isolation, summary, and `expect` matchers. The generated C++ is
ordinary C++20 linked only to `Flight::SdkPreview`; running it does not invoke Node or the upstream
test runner.

The evidence in [`artifacts/upstream-test-parity-prototype.json`](../artifacts/upstream-test-parity-prototype.json)
is regenerated from the pinned checkout. The generated pilot carries the exact source path, line,
and file digest beside every test, so a pin move cannot silently keep testing an older body.

## Where the remaining tests stop

The classifier records the first unsupported construct per test:

| Boundary | Tests | What an expansion would need |
| --- | ---: | --- |
| Imperative/setup body | 823 | `let`, assignment, mutation, ordinary call statements, and `beforeEach`/`afterEach` expansion |
| Unsupported expression | 217 | General property/index access, richer constructors/literals, nullish values, callbacks, and ABI-aware type translation |
| Negated expectation | 40 | Matcher negation with correct reference/identity semantics |
| Other matchers | 33 | Throws, strict equality, object matching, lengths, definedness, and instance checks |
| `it.each` | 16 | Deterministic table expansion and parameter substitution |

The denominator is the actual 43-file math/geometry corpus at the pinned revision; no whole-SDK
generation or compile was used. The classification is intentionally syntactic. In particular, it
does not promise that every imported symbol has a simple snake-case ABI name or that every generated
package header is independently compile-ready. Those checks belong in the next tier: emit candidate
tests package by package and record compile acceptance separately from syntax acceptance.

The largest next gain is setup/body lowering, not more matchers. It accounts for 823 tests (53.5% of
the corpus), especially geometry's stateful vector, matrix, and pool tests. Hooks should be expanded
into each `it` body before C++ emission so test isolation remains the same as Vitest. Identity matchers
must not be approximated with value equality; unsupported identity cases should remain refused until
the native representation can answer them exactly.

## Reproduction

Materialize the pinned sources, regenerate/check the evidence, then build only the scoped pilot:

```sh
npm run rehydrate
npm run parity:generate
npm run parity:check
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --target flight_cpp_upstream_math_geometry_parity_tests -j2
ctest --test-dir build -R '^flight_cpp\.upstream_math_geometry_parity$' --output-on-failure
```

`npm run check` includes the drift check. Like the repository's other pinned-sibling gates, it reports
and skips when Flight has not been rehydrated, so a fresh runtime-only clone remains buildable.
