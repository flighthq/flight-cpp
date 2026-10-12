# A slot a catch records into needs the exception carrier, whatever its initializer

An `unknown` local that a catch body records the caught value into is emitted two different ways depending
on its initializer alone. Without one it becomes `std::exception_ptr`, and the record and rethrow lower
correctly. With `= null` it becomes the erased `flight::Any`, which cannot hold a caught exception, and the
recorded assignment does not compile.

Measured at flight-compiler `2b687ea` / flight `a627849`, regenerating `@flighthq/power` with
[`compiler-nullish-assignment-statement-in-catch.patch`](../../source-patches/compiler-nullish-assignment-statement-in-catch.patch)
applied (that patch is what makes the record emit at all; see below).

## The two spellings, and what each emits

`packages/power/src/power.ts:138`, `destroyPowerKeepAwake`:

```ts
  let failure: unknown = null;
  for (const provider of pending) {
    try {
      if (provider.destroy !== undefined) assertSyncVoid(provider.destroy());
    } catch (error) {
      failure ??= error;
      continue;
    }
    _destroyedKeepAwake.add(provider);
  }
  if (failure !== null) throw failure;
```

```cpp
  flight::Any failure = nullptr;
  ...
    catch (const std::exception& error) {
      ([&]() { auto&& assignment_target = failure; if (assignment_target.is_nullish()) assignment_target = error; return assignment_target; }());
```

```
generated/include/flight/power/power.hpp:159: error: no viable overloaded '='
generated/include/flight/power/power.hpp:292: error: no viable overloaded '='
```

`packages/mediasession/src/mediasession.ts:55`, `destroyMediaSession` -- the same problem shape, one
identifier, and no initializer on the slot:

```ts
  let firstError: unknown;
  let hasError = false;
  ...
    } catch (error) {
      if (!hasError) firstError = error;
      hasError = true;
    }
  }
  if (hasError) throw firstError;
```

```cpp
  std::exception_ptr first_error;
  ...
    catch (const std::exception& error) {
      if (!has_error) {
        first_error = std::current_exception();
      }
      (has_error = true);
    }
  }
  if (has_error) {
    std::rethrow_exception(first_error);
  }
```

That body compiles and is faithful: the caught value is recorded as an exception pointer, and throwing the
slot rethrows the original. Every piece of the lowering this request needs **already exists** in the
emitter -- the carrier, the record, and the rethrow -- and is selected for one spelling of the same
source. The initializer is the whole difference.

## Required C++

A local that a catch body assigns the caught value into is an exception slot: carry it as
`std::exception_ptr` even when the source initializes it to `null`, record with
`std::current_exception()`, and lower `throw` of that slot to `std::rethrow_exception`. A source that
initializes such a slot to `null` is saying "no failure yet", which an empty `std::exception_ptr` already
means; the `Any` carrier says something else, because `flight::Any` cannot hold an exception at all (its
alternatives are the JavaScript value domains) and the caught value is not one of them.

## This is the blocker that has to move with the record

With [`nullish-assignment-dropped-in-catch.md`](nullish-assignment-dropped-in-catch.md) carried by patch
and this carrier unfixed, `@flighthq/power`'s module goes from emitting a bare `failure;` -- which
compiles, silently drops the write, and never rethrows -- to emitting the guarded assignment above, which
does not compile. Neither state is shippable, so the two must land together: the patch is the lowering the
record needs, and this request is the carrier it needs. Measured: regenerating the package with the patch
applied leaves `flight/power/power.hpp` failing with `no viable overloaded '='` at both record sites
(lines 159 and 292), and the package's slice was therefore left unregenerated rather than committed in
that state.

## Distinguish from the requests already filed

[`nullish-assignment-dropped-in-catch.md`](nullish-assignment-dropped-in-catch.md) is the write itself,
now carried by the patch named above. [`catch-handler-breadth-attempt-all.md`](catch-handler-breadth-attempt-all.md)
is which thrown values reach the handler at all; this request changes nothing about the handler, and the
handler stays as narrow as it is today. This one is the slot the handler's value is stored in.

## Carrying workaround

None carried. The source could drop the `= null` initializer and test the slot against `undefined`
instead, which is a source change in a sibling package and a semantics-preserving one at that -- the
repository's preference is to lower what the source already says, and the lowering it needs is already in
the emitter for the other spelling.

## Carried by a compiler patch

[`compiler-catch-recorded-slot-carrier.patch`](../../source-patches/compiler-catch-recorded-slot-carrier.patch)
is the companion of
[`compiler-nullish-assignment-statement-in-catch.patch`](../../source-patches/compiler-nullish-assignment-statement-in-catch.patch)
and the two land together. It admits a nullish-literal initializer as the "nothing recorded yet" state,
counts a `??=` from a catch binding as a recording assignment, and counts the slot's own nullish presence
test as a permitted reference; the record then lowers to `if (!slot) slot = std::current_exception();` --
first failure kept -- and the presence test to pointer emptiness, for either sentinel. Its test pins those
and three controls that must NOT change carrier: an ordinary `unknown` slot holding a plain value, a slot
recorded but never rethrown, and a slot read for its value as well as its presence.

Measured after both patches: `@flighthq/power` regenerates package-scoped to

```cpp
  std::exception_ptr failure = nullptr;
  ...
    catch (const std::exception& error) {
      ([&]() { if (!failure) failure = std::current_exception(); return failure; }());
    }
  ...
  if (([&]() { const auto& presence_operand = failure; return presence_operand; }())) {
    std::rethrow_exception(failure);
  }
```

at both call sites, compiles 3/3, and `tests/generated_power_test.cpp` asserts that the caller receives the
ORIGINAL exception -- including, with two failing providers, the first one and not the last. Catch breadth
is untouched and still open.
