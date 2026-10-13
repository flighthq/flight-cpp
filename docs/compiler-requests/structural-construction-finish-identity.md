# Finishing a structural construction bag must preserve object identity

## Minimal reproduction

```ts
interface Entity {
  value?: number;
}

type EntityConstruction<Type extends Entity> = Omit<Type, never>;

export function allocate<Type extends Entity>(): EntityConstruction<Type> {
  return {} as EntityConstruction<Type>;
}

export function finish<Type extends Entity>(out: EntityConstruction<Type>): Type {
  return out as Type;
}
```

`finish(allocate())` returns the exact JavaScript object supplied to `finish`; the assertion neither
allocates nor copies.

## Current C++ shape

The allocation is represented by a `StructuralRef` over a generated empty anonymous owner. Finishing
casts that row to `Type`. The runtime's materializing structural cast allocates a new nominal object
when the empty anonymous owner and destination owner differ, so the construction row and result have
different `RowOwner` identities.

## Required C++ shape

The construction carrier must retain one identity across its asserted finish. This could be an owner
that gains the finished nominal projection without replacement, or another representation in which
the returned nominal reference delegates to the same storage. Copying the bag into a new object is
not equivalent for identity or aliases retained during construction.

## Affected Flight module

- `@flighthq/entity/packages/entity/src/entity.ts#finishEntity` at Flight
  `a62784923f3be814463286c9fb28edfe6ab15789`

The upstream test explicitly asserts `finishEntity(out) === out`.

## Repository workaround and expiry

The focused native entity test covers allocation state and the finished entity's usable state, but
does not relabel the replacement owner as identity parity. No source patch, override, or generated
header edit is carried.

This request expires when a native assertion can compare the construction row and finished entity as
the same reference identity.
