#pragma once

// The first template argument of a class-template instantiation.
//
// This exists for one narrow reason, and it is worth stating so nobody reaches for it as a general
// metaprogramming utility. TypeScript infers a call's type argument from the type being ASSIGNED TO:
//
//   out.onChildAdded = createSignal();   // T comes from the declared type of onChildAdded
//
// C++ has no contextual typing, so the emitted `create_signal()` has nothing to deduce T from and the
// call does not compile. The repair that answers it computes T from the assignment target instead --
// the same rule TypeScript applies, applied mechanically -- and this trait is the one piece it needs:
// given `std::shared_ptr<Signal<T>>::element_type`, name the `T`.
//
// It is deliberately generic over any class template and names nothing the compiler emits, because the
// runtime must not depend on generated types. `template_argument` is intentionally left UNDEFINED for a
// type that is not an instantiation, so a wrong use is a compile error naming this trait rather than a
// silent fallback.
//
// First argument only: a trait that accepted an index would invite use beyond the case it was written
// for, and every type it is applied to -- flight::types::Signal<T> -- has exactly one.

namespace flight {

template <typename Instantiated>
struct template_argument;

template <template <typename...> class Template, typename First, typename... Rest>
struct template_argument<Template<First, Rest...>> {
  using type = First;
};

template <typename Instantiated>
using template_argument_t = typename template_argument<Instantiated>::type;

} // namespace flight
