// The runtime node hierarchy side table must never answer for a dead node through a reused address.
//
// It is keyed by node identity, which for a native node is its address. When a child dies, its storage may
// be handed to the next allocation of the same size; an entry keyed by the bare address would then report
// the dead child's parent for an unrelated new node. Each entry keeps a weak witness of the node it
// describes and answers only for that same allocation. This test forces the reuse rather than hoping for it.
#include <flight/runtime.hpp>
#include <flight/reference.hpp>

#include <cstdio>
#include <memory>
#include <vector>

namespace {

struct TestNode : flight::ReferenceEnabled {
  double tag{0.0};
};

} // namespace

int main() {
  int failures = 0;
  auto check = [&](bool ok, const char* what) {
    if (!ok) {
      std::printf("FAIL %s\n", what);
      ++failures;
    }
  };
  const auto parent = flight::make_ref<TestNode>(TestNode{.tag = 1.0});
  auto child = flight::make_ref<TestNode>(TestNode{.tag = 2.0});
  flight::node::add_node_child(parent, child);
  const auto parent_of_child = flight::node::get_node_parent(child);
  check(parent_of_child.has_value() && *parent_of_child == parent, "a live child reports its parent");

  const void* dead_address = child.get();
  child.reset();
  // Allocate until a new node lands on the dead child's storage. Kept alive so each try is a new address.
  std::vector<flight::Ref<TestNode>> tries;
  flight::Ref<TestNode> reused;
  for (int i = 0; i < 4096 && !reused; ++i) {
    tries.push_back(flight::make_ref<TestNode>(TestNode{.tag = 3.0}));
    if (tries.back().get() == dead_address) reused = tries.back();
  }
  if (reused) {
    check(!flight::node::get_node_parent(reused).has_value(),
          "a new node at a dead child's address does not inherit that child's parent");
  } else {
    std::printf("NOTE the allocator never reused the dead child's address in 4096 tries; reuse not exercised\n");
  }
  std::printf("%s (%d failures)%s\n", failures ? "FAIL" : "runtime hierarchy reuse ok", failures,
              reused ? "" : " [reuse not exercised]");
  return failures;
}
