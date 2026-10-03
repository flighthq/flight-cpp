import { readFileSync } from 'node:fs';
import path from 'node:path';

// Mechanical repairs applied to compiler output, so a package can ship while an emitter defect is
// still open upstream. This is deliberately the narrowest mechanism that does the job: a repair
// inserts a C++ declaration into generated text and nothing else. It cannot change behavior, because
// a forward declaration has none.
//
// The reason the mechanism is safe to have at all is the expiry rule. `applyEmissionRepairs` reports
// which repairs matched nothing, and `sdk:check` treats a repair that matches nothing as a failure.
// A repair therefore cannot quietly outlive the bug it answers, which is the failure mode that turns
// a patched build into a silent fork of the generator.

const SCHEMA = 'flight-cpp-emission-repairs/1';
const KINDS = new Set(['insert-forward-declaration', 'insert-using-declaration', 'respell-reference-alias', 'name-in-place-alternative', 'name-defaulted-template-argument',
  'deduce-call-argument-from-assignment',
  'respell-flattened-union',
]);
// How `flight::Ref<Symbol<...>>` expands for one named template. `shared-pointer` for a struct that
// derives from flight::ReferenceEnabled, `value` for anything else (an alias to a StructuralRef or a
// variant). The two are not interchangeable and the wrong one is a type error, so each repair states
// which it is and `referenceAliasIdentityProof` turns that statement into a compiled assertion.
const EXPANSIONS = new Set(['shared-pointer', 'value']);
// When a repair is expected to match. A module that only emits once external bindings are applied
// cannot need its repair in the unbound inventory, so claiming the repair is obsolete there would be
// wrong -- `withBindings` says to judge expiry only on a profiled run.
const EXPECTATIONS = new Set(['always', 'withBindings']);

// What each kind must declare, beyond the fields every repair shares. A table rather than a ternary
// chain: there are seven kinds now, and the chain had already grown past the point where a reader could
// tell which branch a kind fell into.
//
// `insert-using-declaration` is the one kind with two shapes, so its required list holds only the shared
// fields and the rest is checked below: either a single `symbol` with the `declaration` to insert, or a
// `symbols` list with the `namespace` they come from.
const SHARED_FIELDS = ['id', 'kind', 'appliesTo', 'diagnostic', 'defect', 'expires'];
const REQUIRED_FIELDS = {
  'deduce-call-argument-from-assignment': [...SHARED_FIELDS, 'symbol', 'sourceDeclaration'],
  default: [...SHARED_FIELDS, 'symbol', 'declaration'],
  'insert-using-declaration': SHARED_FIELDS,
  'name-defaulted-template-argument': [...SHARED_FIELDS, 'symbol', 'defaultArgument', 'sourceDeclaration'],
  'name-in-place-alternative': [...SHARED_FIELDS, 'symbol'],
  'respell-flattened-union': [...SHARED_FIELDS, 'symbol', 'replacement', 'requiredSuffix', 'sourceDeclaration'],
  'respell-reference-alias': [...SHARED_FIELDS, 'symbol', 'expansion', 'witness', 'witnessInclude'],
};

export function loadEmissionRepairs(root) {
  const file = path.join(root, 'repairs', 'emission-repairs.json');
  let parsed;
  try {
    parsed = JSON.parse(readFileSync(file, 'utf8'));
  } catch (error) {
    if (error.code === 'ENOENT') return [];
    throw new Error(`Emission repair declaration ${portable(file)} is not readable JSON: ${error.message}`);
  }
  if (parsed.schema !== SCHEMA) {
    throw new Error(`Emission repair declaration ${portable(file)} has schema ${parsed.schema}, expected ${SCHEMA}`);
  }
  if (!Array.isArray(parsed.repairs)) {
    throw new Error(`Emission repair declaration ${portable(file)} has no repairs array`);
  }
  for (const repair of parsed.repairs) {
    const required = REQUIRED_FIELDS[repair.kind] ?? REQUIRED_FIELDS.default;
    for (const field of required) {
      if (typeof repair[field] !== 'string' || repair[field].length === 0) {
        throw new Error(`Emission repair ${repair.id ?? '<unnamed>'} is missing required field ${field}`);
      }
    }
    if (!KINDS.has(repair.kind)) {
      throw new Error(`Emission repair ${repair.id} has unknown kind ${repair.kind}`);
    }
    // One name or a list, never both and never neither. This defect -- a name owned by another package's
    // namespace, written unqualified -- produces names by the DOZEN per package: 26 in @flighthq/texture
    // alone, 56 across three packages. A row per name would put 56 entries in the expiry ledger for one
    // defect, each repeating the same `defect` text, which makes the ledger unreadable and its signal
    // worthless. The list keeps one row per (package, namespace) without wildcarding anything: every name
    // is still written down, and each carries its OWN defining header.
    if (repair.kind === 'insert-using-declaration') {
      const listed = repair.symbols !== undefined;
      if (listed === (repair.symbol !== undefined)) {
        throw new Error(`Emission repair ${repair.id} must carry exactly one of symbol or symbols.`);
      }
      if (listed) {
        if (!Array.isArray(repair.symbols) || repair.symbols.length === 0) {
          throw new Error(`Emission repair ${repair.id} has an empty or non-array symbols list.`);
        }
        if (typeof repair.namespace !== 'string' || repair.namespace.length === 0) {
          throw new Error(`Emission repair ${repair.id} uses symbols and must name their namespace.`);
        }
        for (const entry of repair.symbols) {
          if (typeof entry?.name !== 'string' || typeof entry?.include !== 'string') {
            throw new Error(
              `Emission repair ${repair.id} has a symbols entry without both a name and an include.`,
            );
          }
        }
      } else if (typeof repair.declaration !== 'string' || repair.declaration.length === 0) {
        throw new Error(`Emission repair ${repair.id} is missing required field declaration`);
      }
    }
    if (repair.kind === 'respell-reference-alias' && !EXPANSIONS.has(repair.expansion)) {
      throw new Error(`Emission repair ${repair.id} has unknown expansion ${repair.expansion}`);
    }
    repair.expectedWhen ??= 'always';
    if (!EXPECTATIONS.has(repair.expectedWhen)) {
      throw new Error(`Emission repair ${repair.id} has unknown expectedWhen ${repair.expectedWhen}`);
    }
  }
  return parsed.repairs;
}

// Rewrites `files` in place where a repair applies. Returns one record per repair naming the files it
// touched, so the caller can record them in the manifest and fail when a repair matched nothing.
export function applyEmissionRepairs(repairs, files) {
  const applied = repairs.map((repair) => ({
    appliesTo: repair.appliesTo,
    expectedWhen: repair.expectedWhen,
    files: [],
    id: repair.id,
  }));
  if (repairs.length === 0) return applied;
  // To a FIXED POINT, because one repair can create the condition another answers: the bare-Node repair
  // writes a type argument, and if that argument needed a using-declaration the declaration repair had
  // already run and found nothing. A single ordered pass makes the outcome depend on the order entries
  // happen to sit in the JSON, which is not a property anyone would think to preserve while editing it.
  // Iterating until nothing changes removes the ordering from the contract altogether. The bound is a
  // guard against a pair of repairs that undo each other, which would otherwise spin forever; it has
  // never been reached, and reaching it is a declaration bug rather than a condition to tolerate.
  const rounds = 8;
  for (let round = 0; round < rounds; round += 1) {
    const changed = applyOneRound(repairs, files, applied);
    if (!changed) return applied;
  }
  throw new Error(
    `Emission repairs did not reach a fixed point in ${String(rounds)} rounds, which means two repairs are ` +
      'rewriting each other. Check the most recently added entries in repairs/emission-repairs.json.',
  );
}

// One pass over every file. Returns whether anything changed, and records each file a repair touched.
function applyOneRound(repairs, files, applied) {
  let changed = false;
  for (const file of files) {
    for (const [index, repair] of repairs.entries()) {
      if (!file.path.startsWith(repair.appliesTo)) continue;
      const contents = typeof file.contents === 'string' ? file.contents : String(file.contents);
      const repaired =
        repair.kind === 'respell-flattened-union'
          ? respellFlattenedUnion(contents, repair)
          : repair.kind === 'deduce-call-argument-from-assignment'
            ? deduceCallArgumentFromAssignment(contents, repair)
            : repair.kind === 'name-defaulted-template-argument'
              ? nameDefaultedTemplateArgument(contents, repair)
              : repair.kind === 'name-in-place-alternative'
                ? nameInPlaceAlternative(contents, repair)
          : repair.kind === 'respell-reference-alias'
            ? respellReferenceAlias(contents, repair)
          : repair.kind === 'insert-using-declaration'
            ? insertUsingDeclaration(contents, repair)
            : insertForwardDeclaration(contents, repair);
      if (repaired === undefined) continue;
      file.contents = repaired;
      changed = true;
      // A repair can legitimately touch one file across two rounds; the record is a set of files, not a
      // count of rewrites, so the same path must not appear twice.
      if (!applied[index].files.includes(file.path)) applied[index].files.push(file.path);
    }
  }
  return changed;
}

// Writes a union back as the alias it is declared as, where the emitter flattened it.
//
// `logOnce(key, level, data: LogData | (() => LogData))` has a parameter the emitter lowers as
// `std::variant<Ref<LogDataProvider>, Ref<LogData>>`, which collapses to
// `variant<function<LogData()>, LogData>`. At the CALL sites it spells the same union flattened --
// `variant<function<LogData()>, Record<String, Any>, String>` -- pulling LogData's own two alternatives
// up into the outer variant. Those are different types, so the argument does not convert:
//
//   could not convert ... from 'variant<function<...>, Record, String>'
//                       to   'variant<function<...>, std::variant<Record, String>>'
//
// Rewriting the argument's type to `flight::types::LogData` makes it the alias, which converts to the
// parameter's second alternative exactly. The brace structure is untouched -- the same `in_place_type`
// alternative holds the same Record -- so only the spelling of the surrounding union changes, and
// `LogData` IS `variant<Record, String>` by its own declaration, which is what makes that a respelling
// rather than a different value.
//
// `requiredSuffix` is what keeps this honest. The flattened spelling is rewritten only where the text
// immediately after it proves a CONSTRUCTION, never a declaration whose type must stay as written. All
// five occurrences in the tree are followed by `{std::in_place_type<flight::Re`, and a declaration
// would not be.
function respellFlattenedUnion(contents, repair) {
  const marker = repair.symbol + repair.requiredSuffix;
  if (!contents.includes(marker)) return undefined;
  return contents.split(marker).join(repair.replacement + repair.requiredSuffix);
}

// Supplies the type argument TypeScript took from the assignment target.
//
// TypeScript infers a call's type argument from the type being assigned to:
//
//   out.onChildAdded = createSignal();      // T is read off the declared type of onChildAdded
//
// C++ has no contextual typing, so the emitted `create_signal()` has nothing to deduce T from:
// "no matching function for call to 'create_signal()'". Twelve of @flighthq/node's 21 headers and
// @flighthq/scene2d's fail on this and on nothing else.
//
// The rewrite applies the SAME RULE mechanically: it reads T off the assignment target, which is sitting
// right there in the text. `(out->on_child_added = create_signal());` becomes
//
//   (out->on_child_added = create_signal<flight::template_argument_t<
//       typename std::remove_cvref_t<decltype(out->on_child_added)>::element_type>>());
//
// -- the target's `shared_ptr<Signal<T>>`, through `element_type`, through the trait, is `T`.
//
// Why this is still a repair and not a rewrite of meaning: the argument is not CHOSEN here, it is
// RECOVERED. TypeScript's own rule says T is the target's parameter, so there is exactly one answer and
// this computes it. It is also self-checking in a way the other repairs are not -- if the expression
// named the wrong type the assignment would not compile, so a wrong rewrite cannot reach a built header.
//
// Only the assignment form is matched. The one non-assignment call in the tree
// (flight/render/render_cache.hpp, inside a nullish-assignment lambda) has no target to read and is
// deliberately left for its own repair or an override rather than guessed at.
function deduceCallArgumentFromAssignment(contents, repair) {
  const pattern = new RegExp(`\\(([A-Za-z_][\\w]*(?:(?:->|\\.)[A-Za-z_]\\w*)+) = ${repair.symbol}\\(\\)\\)`, 'gu');
  if (!pattern.test(contents)) return undefined;
  return contents.replace(
    pattern,
    (_match, target) =>
      `(${target} = ${repair.symbol}<flight::template_argument_t<` +
      `typename std::remove_cvref_t<decltype(${target})>::element_type>>())`,
  );
}

// Writes the default template argument the TypeScript declares and the emitter dropped.
//
// `export interface Node<Traits extends object = NodeTraits>` has a DEFAULT, so TypeScript's bare `Node`
// means `Node<NodeTraits>`. The emitter writes `template <typename Traits> struct Node` with no default
// and then writes the bare name at the use sites, which in C++ is not a type:
//
//   inline std::optional<std::function<void(flight::Ref<Node>, flight::Ref<Node>)>> reparent_node_guard;
//   error: type/value mismatch at argument 1 ... note: expected a type, got 'Node'
//
// Restoring the default on the template would not be enough on its own -- C++ still spells a defaulted
// instantiation `Node<>`, never a bare `Node` -- so the use sites have to be written either way, and
// writing them is the whole repair.
//
// The rewrite only fires where the bare name sits in a TEMPLATE-ARGUMENT position: immediately after a
// `<` or `,` and immediately before a `,` or `>`. That restriction is what makes it safe rather than a
// search-and-replace on a common identifier. In such a position a class-template name with no arguments
// of its own is NEVER valid C++, so there is no reading of the text under which the rewrite could be
// changing a correct program -- and the definition site (`struct Node : public ...`), the parameterised
// uses (`Node<Traits>`), and every ordinary mention are all outside it and left alone.
//
// `sourceDeclaration` records the TypeScript the default is read from, because that citation IS the
// equivalence argument here: nothing in the C++ tree can prove what the dropped default was.
function nameDefaultedTemplateArgument(contents, repair) {
  const pattern = new RegExp(`(?<=[<,]\\s*)${repair.symbol}(?=\\s*[,>])`, 'gu');
  if (!pattern.test(contents)) return undefined;
  return contents.replace(pattern, `${repair.symbol}<${repair.defaultArgument}>`);
}

// Names the variant alternative the emitter left for deduction to find.
//
// The emitter writes `std::variant<A, B, C>{std::in_place_type<B>, {k1, v1, k2, v2}}`. There is no such
// constructor: `variant(in_place_type_t<T>, Args&&...)` has to DEDUCE Args, and a brace-enclosed
// initializer list is a non-deduced context, so the call fails with
// "no matching function for call to 'std::variant<...>::variant(<brace-enclosed initializer list>)'".
// The initializer-list overload cannot rescue it either, because `U` in
// `variant(in_place_type_t<T>, initializer_list<U>, Args&&...)` is equally undeducible from `{k, v, k, v}`.
//
// Writing the type once more -- `std::in_place_type<B>, B{k1, v1, k2, v2}` -- selects
// `variant(in_place_type_t<T>, T&&)`, which direct-initializes the B alternative from a B built out of
// the identical initializer. The name inserted is the one already written inside `in_place_type<...>`,
// so the repair introduces nothing the emitter had not already decided; it only spells the alternative
// where the language cannot infer it. The alternative holds the same value, reached through one move of
// a value type. That is the narrowest edit that compiles, and it is why this is a repair and not an
// override: no declaration, no body, no type changes.
function nameInPlaceAlternative(contents, repair) {
  const marker = `std::in_place_type<${repair.symbol}>, {`;
  if (!contents.includes(marker)) return undefined;
  return contents.split(marker).join(`std::in_place_type<${repair.symbol}>, ${repair.symbol}{`);
}

// Writes the type `flight::Ref<X>` already IS, in place of the alias, so a function template can
// deduce through it.
//
// `flight::Ref<Value>` is not a transparent alias -- it resolves through
// `detail::reference_shape<Value, decltype(detail::complete_probe<Value>(0))::value>::type`, a
// computation. A template parameter that appears only inside it is therefore in a NON-DEDUCED CONTEXT,
// and every generated signal and node helper is declared that way:
//
//   template <typename T>
//   void emit_signal(flight::Ref<flight::types::Signal<T>> signal, ArgsPack&&... args);
//
// so `emit_signal(signals->on_log_entry, entry)` fails with "couldn't deduce template parameter 'T'".
// The declaration is well-formed and the file compiles alone; every CALL fails. That is why packages
// that are 100% emitted still have failing headers, and it is the largest single cause measured in the
// corpus.
//
// The repair writes the alias's own result: `std::shared_ptr<Signal<T>>` for a struct deriving from
// flight::ReferenceEnabled, and `X` itself otherwise. Those are the SAME TYPE -- `reference_shape` has
// exactly those two answers, and `make_ref` already static_asserts the correspondence -- so this is a
// spelling change and not a behavior change, which keeps it inside the rule repairs live by. The
// difference is only that the result is deducible and the alias is not.
//
// Each repair names its `expansion` and a concrete `witness` type; `referenceAliasIdentityProof` emits
// `static_assert(std::same_as<...>)` per repair so the claim is compiled rather than argued. A wrong
// expansion fails the build instead of silently changing a signature.
function respellReferenceAlias(contents, repair) {
  const pattern = new RegExp(`(?<![A-Za-z0-9_:])(?:flight::)?Ref<\\s*((?:[A-Za-z_][A-Za-z0-9_]*::)*${repair.symbol})<`, 'gu');
  let out = contents;
  let changed = false;
  for (;;) {
    pattern.lastIndex = 0;
    const found = pattern.exec(out);
    if (found === null) break;
    // The argument list of the inner template, then the `>` that closes `Ref<`.
    const innerOpen = found.index + found[0].length - 1;
    const innerClose = matchingAngle(out, innerOpen);
    if (innerClose === undefined) break;
    const outerClose = out.indexOf('>', innerClose + 1);
    if (outerClose === -1) break;
    // Nothing but whitespace may sit between the two closers, or this is not the shape we parsed.
    if (out.slice(innerClose + 1, outerClose).trim() !== '') break;
    const inner = `${found[1]}<${out.slice(innerOpen + 1, innerClose)}>`;
    const respelt = repair.expansion === 'shared-pointer' ? `std::shared_ptr<${inner}>` : inner;
    out = out.slice(0, found.index) + respelt + out.slice(outerClose + 1);
    changed = true;
  }
  return changed ? out : undefined;
}

// Index of the `>` closing the `<` at `open`, counting nesting. `>>` is plain text here -- the emitter
// never writes a shift operator inside a type -- so no token-level handling is needed.
function matchingAngle(text, open) {
  let depth = 0;
  for (let at = open; at < text.length; at += 1) {
    if (text[at] === '<') depth += 1;
    else if (text[at] === '>') {
      depth -= 1;
      if (depth === 0) return at;
    }
  }
  return undefined;
}

// The compiled proof for every respell repair that was applied. Each line asserts that the alias and
// the replacement name the same type at a concrete witness, which is the whole claim the repair makes.
// Written into the generated tree so it is compiled by the same gate that compiles the headers.
export function referenceAliasIdentityProof(repairs) {
  const respells = repairs.filter((repair) => repair.kind === 'respell-reference-alias');
  if (respells.length === 0) return undefined;
  const lines = respells.map((repair) => {
    const replacement =
      repair.expansion === 'shared-pointer' ? `std::shared_ptr<${repair.witness}>` : repair.witness;
    return `// ${repair.id}\nstatic_assert(std::same_as<flight::Ref<${repair.witness}>, ${replacement}>,\n              "respell-reference-alias ${repair.id} claims the wrong expansion for ${repair.symbol}");`;
  });
  return [
    '// Generated by scripts/emissionRepairs.mjs. Do not edit.',
    '//',
    '// One assertion per respell-reference-alias repair. The repair rewrites `flight::Ref<X>` to the type',
    '// that alias already resolves to, so that a function template can deduce through it; these lines are',
    '// that claim, compiled. If a repair names the wrong expansion the build fails here rather than',
    '// changing a signature quietly.',
    '#pragma once',
    '#include <concepts>',
    '#include <memory>',
    '#include <flight/runtime.hpp>',
    '',
    ...flightIncludesFor(respells),
    '',
    ...lines,
    '',
  ].join('\n');
}

function flightIncludesFor(respells) {
  return [...new Set(respells.map((repair) => `#include <${repair.witnessInclude}>`))].sort();
}

// Brings a name from flight::types into the package's own namespace, for a file that refers to it
// UNQUALIFIED. `flight/log/log.hpp` says `LogSink` inside `namespace flight::log`, where the type is
// `flight::types::LogSink`; gcc even names the fix in its diagnostic. A forward declaration cannot help
// here, because these are type ALIASES and an alias has no forward declaration, and an include cannot
// help either, because the name is already visible under a different qualification. A using-declaration
// introduces the name and nothing else, so it stays inside the rule that a repair adds no behavior.
// The list form: one entry, many names, one source namespace. Each name carries ITS OWN defining header
// and is tested and introduced INDIVIDUALLY, so a file gets a using-declaration only for the names it
// actually uses unqualified, and only the headers those names need.
//
// The per-name header is not a detail. A first version of this carried one `include` per entry, taken
// from the first name's header, and @flighthq/scene2d then got `flight/types/animation_interpolation.hpp`
// for a group that also contained EntityConstruction -- so the using-declaration named something that was
// not visible yet and a header that had been PASSING began to fail with "'EntityConstruction' has not
// been declared in 'flight::types'". Measured on a scratch tree before declaring, which is the only
// reason it never reached repairs/.
//
// Deliberately not a `using namespace flight::types;` directive, which would be shorter still: a
// directive also pulls in every name the package did not ask for, so any future collision between a
// package's own name and a types-owned one becomes an ambiguity error in generated code nobody edited.
// This is a shorter declaration, not a broader edit.
function insertUsingDeclarations(contents, repair) {
  const qualifier = repair.namespace.split('::').pop();
  const needed = repair.symbols.filter((entry) =>
    new RegExp(`(?<!${qualifier}::)\\b${entry.name}\\b`, 'u').test(contents),
  );
  if (needed.length === 0) return undefined;
  const declarations = needed
    .map((entry) => `using ${repair.namespace}::${entry.name};`)
    .filter((declaration) => !contents.includes(declaration));
  if (declarations.length === 0) return undefined;
  const headers = [...new Set(needed.map((entry) => entry.include))]
    .filter((header) => !contents.includes(`#include <${header}>`))
    .sort();
  let withIncludes = contents;
  for (const header of headers) {
    const hoisted = hoistInclude(withIncludes, header);
    if (hoisted === undefined) return undefined;
    withIncludes = hoisted;
  }
  const anchor = /^namespace flight::[a-z0-9_]+ \{\n/mu.exec(withIncludes);
  if (anchor === null) return undefined;
  const at = anchor.index + anchor[0].length;
  return `${withIncludes.slice(0, at)}\n${declarations.join('\n')}\n${withIncludes.slice(at)}`;
}

function insertUsingDeclaration(contents, repair) {
  if (repair.symbols !== undefined) return insertUsingDeclarations(contents, repair);
  const { symbol } = repair;
  // Unqualified use only. A file that always writes types::X is already correct.
  if (!new RegExp(`(?<!types::)\\b${symbol}\\b`, 'u').test(contents)) return undefined;
  if (contents.includes(repair.declaration)) return undefined;
  // The declaration needs the name to EXIST, and the emitter does not always include the header that
  // defines it -- flight/log/log.hpp names four flight::types aliases and includes none of them, so a
  // bare using-declaration fails with "'flight::types' has not been declared". A repair that introduces
  // a name has to bring its definition with it.
  const withInclude =
    repair.include === undefined || contents.includes(`#include <${repair.include}>`)
      ? contents
      : hoistInclude(contents, repair.include);
  if (withInclude === undefined) return undefined;
  // Inside the package namespace, which is where the unqualified name is looked up.
  const anchor = /^namespace flight::[a-z0-9_]+ \{\n/mu.exec(withInclude);
  if (anchor === null) return undefined;
  const at = anchor.index + anchor[0].length;
  return `${withInclude.slice(0, at)}\n${repair.declaration}\n${withInclude.slice(at)}`;
}

// At file scope, on the emitter's own boundary between the include prologue and the declarations. It
// cannot go inside the namespace block: an include parsed in there resolves every name it declares as
// flight::<package>::flight::…, which is the mistake that cost three iterations on the struct alias.
function hoistInclude(contents, header) {
  const anchor = /^static_assert\(flight::runtime_contract\.cpp_abi == \d+,[^\n]*\n/mu.exec(contents);
  if (anchor === null) return undefined;
  const at = anchor.index + anchor[0].length;
  return `${contents.slice(0, at)}\n#include <${header}>\n${contents.slice(at)}`;
}

// Returns the repaired text, or undefined when this file needs no repair. A file needs the
// declaration when it names the symbol as a qualified member of the namespace but neither declares
// nor defines it. The file that actually defines the symbol is left alone, as is one that already
// carries its own forward declaration.
function insertForwardDeclaration(contents, repair) {
  const { symbol } = repair;
  // The emitter writes the name both qualified and bare -- a header inside namespace flight::types
  // says `Ref<Node<Any>>`, a sibling says `flight::types::Node<...>` -- so the reference test is
  // unqualified. Word boundaries keep it off longer names: \bNode\b does not match Node2D or NodeData.
  const names = new RegExp(`\\b${symbol}\\b`).test(contents);
  if (!names) return undefined;
  const declares = new RegExp(`\\bstruct ${symbol}\\s*[;:{]`).test(contents);
  if (declares) return undefined;
  if (contents.includes(repair.declaration)) return undefined;
  // The ABI assertion block is the emitter's own boundary between the include prologue and the
  // declarations, which is exactly where its forward declarations already go.
  const anchor = /^static_assert\(flight::runtime_contract\.cpp_abi == \d+,[^\n]*\n/m.exec(contents);
  if (anchor === null) return undefined;
  const at = anchor.index + anchor[0].length;
  return `${contents.slice(0, at)}\n${repair.declaration}\n${contents.slice(at)}`;
}

// The expiry check. A repair that matched nothing is either fixed upstream or no longer reachable;
// either way carrying it is how a patched build drifts into a fork.
export function obsoleteRepairs(applied, profiles, fullRun) {
  const bound = profiles.length > 0;
  // Expiry is judged ONLY on a full run, the same way the structural-row-key guard is. On a subset run
  // every repair outside the named packages matches nothing, and calling those obsolete is an instruction
  // to delete live repairs because of what the run did not ask for: a `--package=@flighthq/math` run
  // named five node repairs, the geometry one, and four tree-wide ones whose constructs math simply does
  // not contain. Scoping by `appliesTo` fixes the first group and cannot fix the last, because a repair
  // declared over `flight/` is in scope for every run and still only fires where its construct appears.
  // A full run is the only run where "matched nothing" and "no longer needed" are the same statement.
  if (!fullRun) return [];
  return applied
    .filter((record) => record.files.length === 0)
    .filter((record) => record.expectedWhen !== 'withBindings' || bound)
    .map((record) => record.id);
}

function portable(filename) {
  return filename.split(path.sep).join('/');
}

// ---------------------------------------------------------------------------------------------------
// The duplicate structural struct repair.
//
// An anonymous object shape in the source -- `{ x: number; y: number }` -- is emitted as a struct named
// by its members and a structural hash, and it is emitted ONCE PER PACKAGE that mentions it, each behind
// its own include guard. `x_y_8365950bd60f783f` is defined five times at the current pin, under
// flight::collision, flight::mesh, flight::screen, flight::shape and flight::types, with byte-identical
// bodies. C++ makes those five unrelated types, so a value produced by one package and consumed by
// another does not convert, and the emitted header does not compile:
//
//   no match for call to '(const std::function<shared_ptr<flight::types::x_y_8365950bd60f783f>(…)>)
//                        (flight::Ref<flight::screen::x_y_8365950bd60f783f>&)'
//
// This repair makes the duplicates name one type: the definition in flight::types is kept and each
// other package's copy becomes an alias to it. An alias introduces no operation and no storage -- it
// makes two spellings denote the same type -- so it stays inside the rule that a repair may only add
// text with no behavior of its own.
//
// The precondition is strict and checked, not assumed: the bodies must be BYTE-IDENTICAL. Two shapes
// that merely hash alike would be a different claim entirely, and this repair does not make it.
//
// Only `flight::types` is used as the canonical home, because it is the package every other one already
// depends on; aliasing toward any other package could introduce an include it does not have.

const STRUCT_DEFINITION = /^(#ifndef (FLIGHT_COMPILER_ANONYMOUS__[A-Z0-9_]+)\n#define \2\n)(struct ([a-z0-9_]+_[0-9a-f]{16}) : public flight::ReferenceEnabled \{\n(?:[^}]*?)\n\};\n)(#endif \/\/ \2\n)/gmu;

// Indexes every anonymous structural struct definition by name, recording the body text per package.
function indexStructuralDefinitions(files) {
  const index = new Map();
  for (const file of files) {
    const package_ = packageOf(file.path);
    if (package_ === undefined) continue;
    const contents = typeof file.contents === 'string' ? file.contents : String(file.contents);
    for (const match of contents.matchAll(STRUCT_DEFINITION)) {
      const [, , , body, name] = match;
      if (!index.has(name)) index.set(name, new Map());
      // The defining file is recorded too: an alias is only usable if the header that defines the
      // canonical struct is included, and only this index knows which header that is.
      index.get(name).set(package_, { body, path: file.path });
    }
  }
  return index;
}

// Rewrites each non-canonical copy of a duplicated struct into an alias to the flight::types one.
// Returns the files touched, for the manifest and for the expiry report.
export function aliasDuplicateStructuralStructs(files) {
  const index = indexStructuralDefinitions(files);
  const canonical = new Map();
  for (const [name, byPackage] of index) {
    if (byPackage.size < 2) continue;
    const types = byPackage.get('types');
    if (types === undefined) continue;
    // Every other copy must be byte-identical to the canonical one, or this struct is left alone
    // entirely -- a partial alias would be worse than none.
    if (![...byPackage].every(([, copy]) => copy.body === types.body)) continue;
    canonical.set(name, types);
  }
  if (canonical.size === 0) return { files: [], structs: [] };
  const touched = [];
  const aliased = new Set();
  for (const file of files) {
    const package_ = packageOf(file.path);
    if (package_ === undefined || package_ === 'types') continue;
    const contents = typeof file.contents === 'string' ? file.contents : String(file.contents);
    let changed = false;
    const needed = new Set();
    const repaired = contents.replaceAll(STRUCT_DEFINITION, (whole, open, _guard, body, name, close) => {
      const target = canonical.get(name);
      if (target === undefined || target.body !== body) return whole;
      changed = true;
      aliased.add(name);
      // The include CANNOT go here. These guard blocks sit inside the package's `namespace flight::x`,
      // and an include placed in one is parsed inside that namespace -- which makes every name in the
      // included header resolve as flight::x::flight::… and nothing compiles. The include is collected
      // and hoisted to file scope below; only the alias stays here.
      needed.add(target.path);
      return `${open}using ${name} = flight::types::${name};\n${close}`;
    });
    if (!changed) continue;
    const hoisted = hoistIncludes(repaired, needed);
    if (hoisted === undefined) continue;
    file.contents = hoisted;
    touched.push(file.path);
  }
  return { files: touched, structs: [...aliased].sort() };
}

// Puts the canonical headers at file scope, on the emitter's own boundary between the include prologue
// and the declarations. Returns undefined when that boundary is not found, so a file whose shape this
// repair does not recognize is left exactly as emitted.
function hoistIncludes(contents, needed) {
  if (needed.size === 0) return contents;
  const anchor = /^static_assert\(flight::runtime_contract\.cpp_abi == \d+,[^\n]*\n/mu.exec(contents);
  if (anchor === null) return undefined;
  const absent = [...needed].filter((header) => !contents.includes(`#include <${header}>`)).sort();
  if (absent.length === 0) return contents;
  const at = anchor.index + anchor[0].length;
  const block = `\n${absent.map((header) => `#include <${header}>`).join('\n')}\n`;
  return `${contents.slice(0, at)}${block}${contents.slice(at)}`;
}

function packageOf(filePath) {
  const parts = filePath.split('/');
  return parts.length >= 3 && parts[0] === 'flight' ? parts[1] : undefined;
}
