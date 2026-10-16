#include <flight/scene3d/scene_node.hpp>
#include <flight/scene3d/mesh.hpp>

#include <iostream>

int main() {
  if (&flight::scene3d::node_3d_kind != &flight::types::node_3d_kind) {
    std::cerr << "Node3DKind re-export has different storage\n";
    return 1;
  }
  if (&flight::scene3d::mesh_kind != &flight::types::mesh_kind) {
    std::cerr << "MeshKind re-export has different storage\n";
    return 1;
  }
}
