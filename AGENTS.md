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
