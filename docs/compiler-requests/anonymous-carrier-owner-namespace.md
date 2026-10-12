# An anonymous carrier is referenced through a namespace it is not declared in

`@flighthq/notification` defines its anonymous outcome carriers in its own namespace and then spells them
as `flight::types::…`, so every reference is unresolvable even though the definition is right there in the
same header.

Measured at flight-compiler `2b687ea` / flight `a627849` against emitted `@flighthq/notification`.

## Observed

The definition, inside `namespace flight::notification` (opened at line 94):

```cpp
// notification.hpp:341
template <typename TSubscription, typename TArgs>
struct attach_failed_reason_release_failed_b36c59d257622846 : public flight::ReferenceEnabled {
  bool attach_failed;
  flight::String reason;
  bool release_failed;
};
```

and six uses of it under a different namespace:

```cpp
co_return std::variant<flight::Ref<flight::types::attach_failed_reason_release_failed_b36c59d257622846<TSubscription, TArgs>>, …
```

```
notification.hpp:353:55: error: no member named 'attach_failed_reason_release_failed_b36c59d257622846'
                          in namespace 'flight::types'
```

The same shape appears for the sibling carriers the module defines and references: `reason_562c02e4452c65e0`,
`attachment_reason_4801a2ab642e6e7f`, `reason_release_failed_9494ccb3317a62d7`,
`attach_failed_reason_release_failed_1c465b7fe63c730b`.

## Why it happens

The module's anonymous shapes are named after the *union* they came from, which is declared in
`@flighthq/types`; the carrier is then emitted into whichever module first needs it — here
`flight::notification` — while the references keep the declaring package's namespace. For the
non-generic carriers in the same file the emitter gets this right, which is why the defect is visible
only on the two-parameter instantiations.

## What would close this

Qualify a reference to an anonymous carrier by the namespace the carrier is actually emitted into, not by
the namespace of the type alias it came from.

## Distinguish from the requests already filed

[`partial-module-placeholder-qualification.md`](partial-module-placeholder-qualification.md) is about a
*partially* emitted module losing the `using` declarations its retained declarations need. This one is in
a declaration the emitter believes it emitted whole, and the name is not merely unqualified — it is
qualified with the wrong namespace.

## Carrying workaround

None, for the reason given in
[`attach-declaration-truncated-parameter.md`](attach-declaration-truncated-parameter.md): the module's
supply predates this emission and is retained while the emitted form is unusable.
