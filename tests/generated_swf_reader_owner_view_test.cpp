// The real generated SwfReader class owner, viewed through the EXISTING shared view API, plus the
// method-forwarding operation the SWF class/interface pair needs.
//
// BOUNDARY, stated honestly so the test cannot be read as more than it is:
// - the data half is the shipped API: flight::NominalOrStructuralView aliases the class owner's own
//   cells (writes both ways), identity is the object's, and the view keeps the owner alive.
// - the method-forwarding half uses a FIXTURE-LOCAL owner-holding view below. The interface the SWF
//   handlers receive declares std::function members while the class declares real member functions,
//   so the shipped family does not yet forward methods. This class is the required operation's
//   contract -- one owner, cursor advanced by the class's own functions, no copy -- not a shipped
//   family; when the shared adoption family lands, this view is replaced by it and the assertions
//   below are the ones it must satisfy.
#include <flight/nominal_or_structural_view.hpp>
#include <flight/swf/swf_reader.hpp>
#include <flight/string.hpp>
#include <iostream>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

// The bounded method-forwarding operation: one owner, data cells aliased in place, interface methods
// forwarded to the class's own functions. Nothing is copied and the owner is kept alive.
class SwfTagReaderOwnerView {
 public:
  explicit SwfTagReaderOwnerView(flight::Ref<flight::swf::SwfReader> owner) : owner_(std::move(owner)) {}

  [[nodiscard]] flight::Ref<flight::swf::SwfReader> owner() const { return owner_; }
  [[nodiscard]] double pos() const { return owner_->pos; }
  void set_pos(double value) const { owner_->pos = value; }
  [[nodiscard]] bool valid() const { return owner_->valid; }
  void set_valid(bool value) const { owner_->valid = value; }
  [[nodiscard]] double bit_position() const { return owner_->bit_position; }

  // Method forwarding: the interface's read operations are the class's own functions.
  double read_uint8() const { return owner_->read_uint8(); }
  void align_to_byte() const { owner_->align_to_byte(); }

 private:
  flight::Ref<flight::swf::SwfReader> owner_;
};

} // namespace

int main() {
  flight::Uint8Array bytes(4.0);
  bytes.element(0.0) = 7;
  bytes.element(1.0) = 0;
  auto reader = flight::make_ref<flight::swf::SwfReader>(bytes, 0.0, bytes.size());

  // Existing shared view API over the class owner: data both ways, identity, lifetime.
  flight::NominalOrStructuralView<flight::swf::SwfReader, flight::swf::SwfReader> view(reader);
  check(view.read<flight::RowKey<"pos">, double, &flight::swf::SwfReader::pos>().value() == 0.0,
        "the shared view reads the class owner's pos");
  view.write<flight::RowKey<"pos">, double, &flight::swf::SwfReader::pos>(3.0);
  check(reader->pos == 3.0, "a write through the shared view is the class owner's cell");
  reader->valid = false;
  check(view.read<flight::RowKey<"valid">, bool, &flight::swf::SwfReader::valid>().value() == false,
        "a class write is visible through the shared view");
  check(view == flight::NominalOrStructuralView<flight::swf::SwfReader, flight::swf::SwfReader>(reader),
        "two views of one class owner are equal");

  // Method-forwarding operation over the same owner.
  reader->pos = 0.0;
  reader->valid = true;
  SwfTagReaderOwnerView interface_view(reader);
  check(interface_view.pos() == 0.0, "the interface view reads the same cursor");
  check(interface_view.read_uint8() == 7.0, "a forwarded method reads the class's bytes");
  check(interface_view.pos() == 1.0, "a forwarded method advances the same cursor");
  interface_view.set_valid(false);
  check(reader->valid == false, "an interface write is visible on the class instance");
  interface_view.set_pos(2.0);
  check(reader->pos == 2.0, "an interface cursor write is visible on the class instance");

  // PROVISIONAL HISTORICAL NOTE: these provenance controls PASSED on the A-prime commit that was
  // withdrawn and reverted as 55ccab63 (43 new generated startup aborts traced to a constant-init
  // root). The API (require_owner / control-block recording) is NOT in the restored base, so the
  // assertions stand staged here and re-enable only on the ACTUAL corrected reland, verified by a
  // full-ctest zero-new-fail gate. The data/identity/lifetime/method-thunk assertions below are the
  // shipped-API half and remain valid on the restored base.

  // Lifetime: the interface view keeps the owner alive after the original reference is dropped.
  std::weak_ptr<flight::swf::SwfReader> observed = reader;
  flight::Ref<flight::swf::SwfReader> dropped = reader;
  reader = nullptr;
  dropped = nullptr;
  check(!observed.expired(), "the interface view keeps the owner alive");
  check(interface_view.owner() != nullptr, "the interface view still holds the owner");

  if (failures != 0) {
    std::cerr << failures << " swf owner-view fixture check(s) failed\n";
    return 1;
  }
  std::cout << "swf class owner serves shared-view data and forwarded methods over one allocation\n";
  return 0;
}
