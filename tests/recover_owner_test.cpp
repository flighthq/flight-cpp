// flight::recover_owner: the checked runtime recovery behind a base-to-derived interface assertion.
//
// A record emitted with native interface heritage names its primary base as `flight_native_base`, and
// `make_ref` registers it by shared ownership. Recovery must return the SAME allocation (identity, live
// cells, shared lifetime) when the object really is the target or derives from it, and must answer empty
// -- never cast -- for a base-only allocation, a sibling, an object not made by make_ref, or an expired one.
#include <flight/reference.hpp>

#include <cstdio>
#include <memory>

namespace {

struct Target : flight::ReferenceEnabled {
  double width{0.0};
};
struct Texture : Target {
  using flight_native_base = Target;
  double view{0.0};
};
struct Screen : Target {
  using flight_native_base = Target;
  double swap{0.0};
};
struct CustomTexture : Texture {
  using flight_native_base = Texture;
  double extra{0.0};
};

int failures = 0;
void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL %s\n", what);
    ++failures;
  }
}

}  // namespace

int main(int argc, char**) {
  auto texture = flight::make_ref<Texture>(Texture{});
  texture->view = 7.0;
  flight::Ref<Target> base = texture;
  auto recovered = flight::recover_owner<Texture>(base);
  check(recovered.get() == texture.get(), "an exact allocation recovers as the same owner");
  check(!recovered.owner_before(texture) && !texture.owner_before(recovered), "the recovery shares ownership");
  recovered->view = 9.0;
  check(texture->view == 9.0, "a write through the recovered owner reaches the live cell");

  auto custom = flight::make_ref<CustomTexture>(CustomTexture{});
  flight::Ref<Target> custom_base = custom;
  check(flight::recover_owner<Texture>(custom_base).get() == static_cast<Texture*>(custom.get()),
        "a subclass recovers as its declared native base");
  check(flight::recover_owner<CustomTexture>(custom_base).get() == custom.get(), "and as itself");

  auto screen = flight::make_ref<Screen>(Screen{});
  check(!flight::recover_owner<Texture>(flight::Ref<Target>(screen)), "a sibling does not recover");
  check(!flight::recover_owner<Texture>(flight::make_ref<Target>(Target{})), "a base-only allocation does not recover");
  check(!flight::recover_owner<Texture>(std::make_shared<Texture>()), "an object not made by make_ref does not recover");
  check(!flight::recover_owner<Texture>(flight::Ref<Target>{}), "an empty reference does not recover");

  std::weak_ptr<Texture> weak;
  {
    auto temporary = flight::make_ref<Texture>(Texture{});
    weak = temporary;
  }
  check(weak.expired(), "the registry does not keep an owner alive");

  if (argc > 1) check(flight::recover_owner<Screen>(base).get() == nullptr ? false : true, "mutant: demand a sibling recovery");
  std::printf("%s (%d failures)\n", failures ? "FAIL" : "recover_owner ok", failures);
  return failures ? 1 : 0;
}
