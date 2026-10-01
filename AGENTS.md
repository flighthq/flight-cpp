# flight-cpp contributor contract

`flight-cpp` is an independently buildable C++20 runtime. It was incubated inside `flight-compiler` and carries that history; it is now its own repository. Do not couple the runtime to the compiler's implementation modules, its npm workspace, or any path outside this one.

The public boundary is the installed `flight/` header tree and the `Flight::Cpp` CMake target. Preserve TypeScript observable semantics over superficial resemblance to STL APIs. Reference identity, absence, equality, ordering, exceptions, and task settlement must be deliberate and directly tested.

The compatibility aliases in `flight/runtime.hpp` serve emitted code during migration. New runtime APIs belong in namespace `flight`; do not add further global names unless the compiler currently emits them.

Keep the runtime dependency-free. Build and test it from this directory with:

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Do not claim a compiler capability or runtime ABI version in `contract.hpp` until its semantic tests exist. Document known gaps in `docs/` rather than hiding them behind permissive fallback behavior.

## Repository automation

`scripts/` is repository automation, not runtime source. The rules above constrain what ships in `include/` and `src/`; they do not govern these scripts, which are plain ESM run directly by Node with no dependencies and no build step. Keep it that way: a contributor who only builds the runtime must never need `npm install`.

`npm run check` runs every gate even after an earlier one fails, because the gates are independent and stopping at the first failure hides the rest. Gates read; nothing here rewrites a committed baseline except `rehydrate:update`, which re-pins the lock deliberately.

## Pinned siblings

The runtime and the compiler evolve against each other, so each repository pins the other instead of sharing a tree. `dependencies.lock.json` names the exact commit of `flight` and `flight-compiler` this checkout is verified against, and `npm run rehydrate` materializes them under the gitignored `.dependencies/`.

Nothing in `.dependencies/` is committed and no gate may treat it as a source of truth. It is a disposable build input; the lock is the only thing that decides which revision is read. A gate that needs a checkout reports and skips when it is absent rather than failing, so a fresh clone stays runnable. `flight` supplies the input for the committed SDK inventory; `npm run sdk:check` must reproduce both its emitted headers and refusal ledger.

Move a pin deliberately, in its own commit, with the gate result that motivated it. Regenerate the SDK inventory when
either the `flight` or `flight-compiler` pin moves and commit the changed output with that pin update.

## Shipping a package before the compiler can emit it

`flight-compiler` refuses rather than mislowers, and that is correct. It also means a package can be
blocked by one expression. Three declared mechanisms let a package ship anyway without the runtime
quietly forking from the generator. Each one is a debt instrument, and each one has an expiry check,
because a workaround that outlives its defect is indistinguishable from a fork nobody chose.

`source-patches/` rewrites an expression in a pinned sibling checkout into an equivalent the compiler
does lower. A patch is applied only after the pin integrity check and reverted when the run ends, so a
checkout is rewritten for the length of one run; `baseCommit` pins it to the revision it was written
against, and a pin move that invalidates it fails loudly. Every patch states the equivalence that makes
it safe, and generation reports a patch whose module is still refused for the reason the patch names.

`repairs/emission-repairs.json` inserts a C++ declaration the emitter omitted. It may only add text with
no behavior — a forward declaration — and `sdk:check` fails when a repair matches no generated header.

`deferred-packages.json` lets a package's headers fail the compile gate without failing the build. The
headers are still emitted, still compiled and always reported, and a deferral is refused when a
non-deferred header still includes the deferred package.

None of the three may change absence, reference identity, equality, ordering, exception shape, or task
settlement. Those are the semantics this runtime exists to preserve, and a workaround that alters one is
a claim our own tests would then certify as true. A gap of that kind is a runtime capability to build or
a compiler request to file, never a patch.

Prefer the fixes that leave no debt, in this order: extend the runtime so emitted code compiles; declare
the external binding the compiler was missing; then, only for what neither can reach, patch or defer.
