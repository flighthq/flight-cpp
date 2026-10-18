# finishEntity must specialize from its construction, not from the expected return type

`@flighthq/loader`'s `createResourceLoader` allocates a `ResourceLoaderInternal` construction,
initializes it, and finishes it as the public `ResourceLoader` it extends. The emitted factory
specialized the ambient call with the DECLARED RETURN type:

```cpp
return flight::entity::finish_entity<flight::Ref<ResourceLoader>>(
    flight::structural_ref_cast<EntityConstruction<Ref<ResourceLoader>>>(out));
```

The row's recorded native owner is the `ResourceLoaderInternal` allocation, so
`structural_ref_cast`'s exact-owner recovery (`RowOwner::native_type() == typeid(Object)`) rejects
the public schema and throws `std::bad_cast`. The factory is public API, so the runtime abort blocks
the whole loader consumer even though every header compiles.

## Lowering

A call written without a type argument specializes from the ARGUMENT's construction owner:

```cpp
return flight::entity::finish_entity<flight::Ref<ResourceLoaderInternal>>(out);
```

The same construction row and the same allocation are finished; the returned shared pointer is then
the identical object widened derived-to-base along the heritage the module already emits
(`struct ResourceLoaderInternal : public ResourceLoader`). Nothing is copied, materialized, or
reinterpreted, and the exact-owner check is not relaxed. An explicitly written type argument still
wins, so the compiler never overrides an assertion the author stated.

## Disposition

Carried by `compiler-finish-entity-construction-owner`, with a focused test covering: the derived
owner specialization, the public-return non-specialization, no replacement owner, one construction
evaluation per factory, and the explicit-argument control. It expires when upstream infers the
finish type argument from the argument in every target.
