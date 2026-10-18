// The generated SwfReader class IS the SwfTagReader interface it is passed as: one allocation, one set
// of live data cells, and the interface's method members dispatch to the class's own implementations.
//
// Both directions are declared (class-interface-adoptions.json) and both are exercised here on the
// REAL emitted types:
// - forward: a flight::Ref<SwfReader> converts to flight::Ref<SwfTagReader> by the base adjustment,
//   sharing the control block, so the interface reference names the same object and the same cells;
// - reverse: require_owner<SwfReader> recovers that exact allocation, and a foreign interface
//   allocation keeps the honest NativeScalarEscapeError partial instead of a cast or an empty owner.
//
// It also pins what makes the shape sound: constructor initialization, the thunk binding (so a write
// or call through either spelling reaches one implementation), lifetime, and stable captures after the
// original class reference is dropped.
#include <flight/swf/swf_reader.hpp>
#include <flight/types/swf_tag_parse_state.hpp>

#include <iostream>
#include <memory>
#include <string>
#include <type_traits>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

} // namespace

int main() {
  static_assert(std::is_base_of_v<flight::types::SwfTagReader, flight::swf::SwfReader>,
                "the declared pair is one native hierarchy");
  static_assert(std::is_convertible_v<flight::Ref<flight::swf::SwfReader>, flight::Ref<flight::types::SwfTagReader>>,
                "a class reference converts to the interface it implements");

  flight::Uint8Array bytes(4.0);
  bytes.element(0.0) = 7;
  bytes.element(1.0) = 0;
  bytes.element(2.0) = 9;
  bytes.element(3.0) = 0;
  auto reader = flight::make_ref<flight::swf::SwfReader>(bytes, 0.0, bytes.size());

  // Constructor initialization: the class's field initializers ran on the inherited cells and the
  // interface method members were bound before any interface use.
  check(reader->bit_position == 0.0, "the constructor initialized the inherited bit cursor");
  check(reader->valid, "the constructor initialized the inherited validity flag");
  check(reader->pos == 0.0, "the constructor assigned the inherited cursor from its parameter");

  // Forward: one allocation and one control block.
  flight::Ref<flight::types::SwfTagReader> as_interface = reader;
  check(as_interface.get() == reader.get(), "the interface reference is the class allocation");
  check(!as_interface.owner_before(reader) && !reader.owner_before(as_interface), "the two references share one control block");
  check(as_interface.use_count() == reader.use_count(), "the reference counts are one count");

  // One physical data cell, written and read through both names.
  as_interface->pos = 2.0;
  check(reader->pos == 2.0, "an interface write is the class's own cursor cell");
  reader->bit_position = 5.0;
  check(as_interface->bit_position == 5.0, "a class write is the interface's own cell");
  as_interface->valid = false;
  check(!reader->valid, "the interface validity write is the class's cell");
  reader->valid = true;
  check(as_interface->valid, "the class validity write is the interface's cell");

  // Method thunks dispatch to the class's own implementations over the shared cursor.
  reader->pos = 0.0;
  reader->bit_position = 0.0;
  check(as_interface->read_uint8() == 7.0, "the interface method dispatches to the class reader");
  check(reader->pos == 1.0, "the dispatched method advanced the shared cursor");
  check(as_interface->read_string().length() == 0, "the forwarded string read terminates on the zero byte");
  check(reader->pos == 2.0, "the forwarded string read advanced the shared cursor");
  check(as_interface->read_uint16() == 9.0, "the forwarded word read is little-endian over the class bytes");
  check(reader->pos == 4.0, "the forwarded word read advanced the shared cursor");

  // No hiding collision: the class method and the inherited interface member are one implementation.
  reader->pos = 0.0;
  check(reader->read_uint8() == 7.0, "the class method reads the same bytes");
  check(as_interface->pos == 1.0, "a class-method call is visible through the interface cursor");

  // Reverse: the same owner is recovered from the interface reference.
  auto recovered = flight::require_owner<flight::swf::SwfReader>(as_interface);
  check(recovered.get() == reader.get(), "require_owner recovers the same allocation");
  check(!recovered.owner_before(reader) && !reader.owner_before(recovered), "the recovered owner shares the control block");

  // A foreign interface allocation is the honest partial boundary: exact throw, never a cast.
  auto foreign = flight::make_ref<flight::types::SwfTagReader>();
  bool threw = false;
  try {
    (void)flight::require_owner<flight::swf::SwfReader>(foreign);
  } catch (const flight::NativeScalarEscapeError&) {
    threw = true;
  }
  check(threw, "a foreign interface allocation throws the native escape");

  // Lifetime and stable captures: the interface reference keeps the owner alive, and its thunks still
  // dispatch to the same object after the original class reference is gone.
  std::weak_ptr<flight::swf::SwfReader> observed = reader;
  as_interface->pos = 1.0;
  reader = nullptr;
  check(!observed.expired(), "the interface reference keeps the owner alive");
  check(as_interface->read_uint8() == 0.0, "the thunk still dispatches after the class reference is dropped");
  check(as_interface->pos == 2.0, "the thunk still advances the same live cell");

  if (failures != 0) {
    std::cerr << failures << " swf class/interface heritage check(s) failed\n";
    return 1;
  }
  std::cout << "SwfReader is the SwfTagReader it is passed as: one allocation, cells and method dispatch shared\n";
  return 0;
}
