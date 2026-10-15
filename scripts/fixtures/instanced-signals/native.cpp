#include <flight/runtime.hpp>
#include <flight/types/instanced_mesh.hpp>
#include <flight/types/instanced_mesh_signals.hpp>
#include <flight/entity/entity.hpp>
#include <flight/signals/signal.hpp>

#include <iostream>
#include <stdexcept>

namespace flight::scene3d {
int factory_calls = 0;
bool fail_factory = false;

// @INSTANCED_FUNCTIONS@
}

namespace {
int failures = 0;
void check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

auto mesh() {
  auto target = flight::make_ref<flight::types::InstancedMesh>();
  target->entity_runtime_key = flight::make_ref<flight::types::InstancedMeshRuntime>();
  return target;
}

auto read(const flight::Ref<flight::types::InstancedMesh>& target) {
  using Readonly = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::InstancedMesh>>>>;
  using Writable = flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<flight::types::InstancedMesh>>>>;
  return flight::scene3d::get_instanced_mesh_signals(flight::structural_ref_cast<Readonly>(Writable(target)));
}
}

int verify() {
  namespace scene = flight::scene3d;
  using Group = flight::Ref<flight::types::InstancedMeshSignals>;
  // Construct only the declared input carriers. These tests do not supply or certify
  // createInstancedMesh, which has independent generated construction failures.
  for (int sentinel = 0; sentinel != 3; ++sentinel) {
    auto target = mesh();
    auto runtime = target->entity_runtime_key.value();
    if (sentinel == 1) runtime->instanced_mesh_signals = flight::undefined;
    if (sentinel == 2) runtime->instanced_mesh_signals = flight::null;
    runtime->uid = flight::String("retained");
    scene::factory_calls = 0;
    check(!read(target).has_value(), "readonly get normalizes default, undefined and null");
    check(scene::factory_calls == 0, "readonly get never allocates");
    check(sentinel == 2 ? std::holds_alternative<flight::Null>(runtime->instanced_mesh_signals)
                        : std::holds_alternative<flight::Undefined>(runtime->instanced_mesh_signals),
          "readonly get preserves the stored sentinel");
    const auto group = scene::enable_instanced_mesh_signals(target);
    check(scene::factory_calls == 1, "each absent sentinel allocates exactly once");
    check(target->entity_runtime_key.value() == runtime, "selected runtime retains its exact owner");
    check(runtime->uid == std::optional<flight::String>{flight::String("retained")}, "unrelated runtime member is retained");
    const auto stored = std::get_if<Group>(&runtime->instanced_mesh_signals);
    check(stored && *stored == group, "returned and stored group are the same reference");
    check(group && group->on_cleared && group->on_instance_appended && group->on_instance_removed,
          "actual emitted factory initializes all three real Signal owners");
    const auto cleared = group->on_cleared;
    const auto appended = group->on_instance_appended;
    const auto removed = group->on_instance_removed;
    int calls = 0;
    group->on_cleared->emit = [&]() { ++calls; };
    for (int repeat = 0; repeat != 3; ++repeat) {
      const auto next = scene::enable_instanced_mesh_signals(target);
      check(next == group && read(target) == std::optional<Group>{group}, "enable and readonly get retain group identity");
      check(next->on_cleared == cleared && next->on_instance_appended == appended && next->on_instance_removed == removed,
            "all three signal references survive repeated enable");
      next->on_cleared->emit();
    }
    check(calls == 3 && scene::factory_calls == 1, "existing group short circuits factory and preserves its callback");
  }
  auto first = mesh();
  auto other = mesh();
  const auto other_runtime = other->entity_runtime_key.value();
  const auto first_group = scene::enable_instanced_mesh_signals(first);
  check(other->entity_runtime_key.value() == other_runtime && !read(other).has_value(), "another runtime is untouched");
  const auto other_group = scene::enable_instanced_mesh_signals(other);
  check(first_group != other_group && first_group->on_cleared != other_group->on_cleared &&
            first_group->on_instance_appended != other_group->on_instance_appended &&
            first_group->on_instance_removed != other_group->on_instance_removed,
        "different meshes receive independent groups and signals");
  for (bool null_sentinel : {false, true}) {
    auto target = mesh();
    auto runtime = target->entity_runtime_key.value();
    if (null_sentinel) runtime->instanced_mesh_signals = flight::null;
    scene::factory_calls = 0;
    scene::fail_factory = true;
    bool caught = false;
    try { (void)scene::enable_instanced_mesh_signals(target); }
    catch (const std::runtime_error& error) { caught = std::string_view(error.what()) == "factory-probe"; }
    scene::fail_factory = false;
    check(caught && scene::factory_calls == 1, "one factory exception propagates");
    check(null_sentinel ? std::holds_alternative<flight::Null>(runtime->instanced_mesh_signals)
                        : std::holds_alternative<flight::Undefined>(runtime->instanced_mesh_signals),
          "factory exception preserves the original sentinel");
  }
  if (failures == 0) std::cout << "Instanced emitted native sentinel, lazy allocation and exact owner identity PASS\n";
  return failures == 0 ? 0 : 1;
}

int main() {
  try { return verify(); }
  catch (const std::exception& error) {
    std::cerr << "FAIL: unexpected native exception: " << error.what() << '\n';
    return 1;
  }
}
