import { execFileSync } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';

// Renames the C++ identifiers this tree carries to the spellings the next compiler pin emits.
//
// The compiler pin move 2b687eaf -> 806d257 changes the C++ identifier converter: `snakeCase` becomes
// `convertCppIdentifierToSnakeCase` (packages/compiler-backend-cpp/src/cppCompilerIdentity.ts). Every
// emitted name, and every module filename derived from a source path, changes with it. This script
// computes that mapping and rewrites the tree, and it is the engine the pin move itself runs.
//
// Three properties it exists to guarantee.
//
//   * The mapping is a PORT, checked against the real TypeScript. Both converters are declared here as
//     an ordered chain of string operations, in the same shape as the source, and `runConverterSelfCheck`
//     extracts the same chain out of the pinned checkout's source text and asserts the two are equal.
//     A port that drifts from the source then fails loudly instead of renaming the tree wrongly.
//
//   * Matching is WHOLE-IDENTIFIER only. A key matches where it appears as a maximal run of
//     [A-Za-z0-9_]; `foo2_data` is one token and is never touched by a key `foo2_d`. There is no
//     substring replacement anywhere: the text is rebuilt token by token.
//
//   * The run is RERUNNABLE. The map is keyed on the OLD spelling, so after an apply has rewritten a
//     token the old spelling is gone and a second run finds nothing to do. That makes the script safe
//     to run over a branch that already carries a rename, which is what lets any agent apply it to
//     in-flight work without redoing it.
//
// Applying is opt-in twice over. The default is a dry run that writes nothing at all, and the apply
// mode refuses unless `--in-place` names the repository as the deliberate target. See `parseArguments`.

export const IDENTITY_SOURCE_PATH = 'packages/compiler-backend-cpp/src/cppCompilerIdentity.ts';
export const OLD_PIN = '2b687eaf6b631e0fa4d7d6e18dad93b845f99633';
export const OLD_PIN_SHORT = '2b687eaf';
export const NEW_PIN = '806d257';

// The trees an apply rewrites. `source-patches/` is deliberately ABSENT: those are TypeScript patches
// against the pinned sibling and are not emitted C++, so nothing here reads or writes them.
export const SCOPE_TREES = ['generated', 'overrides', 'repairs', 'examples', 'tests', 'src', 'include', 'bindings', 'abi'];
export const EXCLUDED_TREES = ['source-patches'];

const FLIGHT_PACKAGES_SRC = ['.dependencies', 'flight', 'packages'];
const FLIGHT_COMPILER_SRC = ['.dependencies', 'flight-compiler'];
const TS_EXTENSIONS = new Set(['.ts', '.tsx', '.mts', '.cts']);
const TOKEN_RE = /[A-Za-z_][A-Za-z0-9_]*/gu;
const HASH_RE = /_[0-9a-fA-F]{16}$/u;
const INTERNAL_PREFIX = '_internal_';
const GUARD_PREFIX = 'FLIGHT_COMPILER_ANONYMOUS_';
const DECOMPOSITION_CAP = 4096;

// ------------------------------------------------------------------ the two converters, as data

// Old pin: `snakeCase` in packages/compiler-backend-cpp/src/cppCompilerIdentity.ts.
export const OLD_CHAIN = [
  { op: 'replace', pattern: '([a-z0-9])([A-Z])', flags: 'gu', replacement: '$1_$2' },
  { op: 'replace', pattern: '[^A-Za-z0-9]+', flags: 'gu', replacement: '_' },
  { op: 'replace', pattern: '^_+|_+$', flags: 'gu', replacement: '' },
  { op: 'toLowerCase' },
];

// New pin 806d257: `convertCppIdentifierToSnakeCase`, same file. The order is load-bearing: rule 1
// introduces the `_D` that rule 2 rewrites, and rule 2 introduces the `D`-before-Upper shape rule 3
// splits. They cannot be reordered or merged.
export const NEW_CHAIN = [
  { op: 'normalize', form: 'NFC' },
  { op: 'replace', pattern: '([a-z0-9])([A-Z])', flags: 'gu', replacement: '$1_$2' },
  { op: 'replace', pattern: '([A-Za-z])([0-9]+)_D(?=$|[^a-z])', flags: 'gu', replacement: '$1_$2D' },
  { op: 'replace', pattern: '([0-9]+D)([A-Z][a-z])', flags: 'gu', replacement: '$1_$2' },
  { op: 'replace', pattern: '[^A-Za-z0-9]+', flags: 'gu', replacement: '_' },
  { op: 'replace', pattern: '^_+|_+$', flags: 'gu', replacement: '' },
  { op: 'toLowerCase' },
];

export function applyChain(value, chain) {
  let out = value;
  for (const step of chain) {
    if (step.op === 'normalize') out = out.normalize(step.form);
    else if (step.op === 'toLowerCase') out = out.toLowerCase();
    else if (step.op === 'replace') out = out.replace(new RegExp(step.pattern, step.flags), step.replacement);
    else throw new Error(`unknown chain op: ${step.op}`);
  }
  return out;
}

export const oldSnakeCase = (value) => applyChain(value, OLD_CHAIN);
export const newSnakeCase = (value) => applyChain(value, NEW_CHAIN);

// The named examples the pin move is specified by, each with the source identifier that implies it.
export const NAMED_EXAMPLES = [
  { identifier: 'camera2D', oldCpp: 'camera2_d', newCpp: 'camera_2d' },
  { identifier: 'texImage2D', oldCpp: 'tex_image2_d', newCpp: 'tex_image_2d' },
  { identifier: 'scene3DResource', oldCpp: 'scene3_dresource', newCpp: 'scene_3d_resource' },
];

// ------------------------------------------------------------------ serialized chain self-check

const OLD_SIGNATURE = 'function snakeCase(value: string): string {';
const NEW_SIGNATURE = 'export function convertCppIdentifierToSnakeCase(value: string): string {';

function extractBody(source, signature, label) {
  const at = source.indexOf(signature);
  if (at < 0) throw new Error(`converter self-check: ${label} signature not found in source`);
  const open = source.indexOf('{', at);
  const close = source.indexOf('\n}', open);
  if (open < 0 || close < 0) throw new Error(`converter self-check: ${label} body not delimited`);
  return source.slice(open + 1, close);
}

// Parse the ordered operation chain out of a function body: every `.normalize(...)`, `.toLowerCase()`,
// and `.replace(/pat/flags, 'repl')` in source order.
export function parseChainFromBody(body) {
  const hits = [];
  const push = (index, step) => hits.push({ index, step });
  for (const m of body.matchAll(/\.normalize\(\s*'([^']*)'\s*\)/gu)) push(m.index, { op: 'normalize', form: m[1] });
  for (const m of body.matchAll(/\.toLowerCase\(\)/gu)) push(m.index, { op: 'toLowerCase' });
  // A regex literal with no `/` inside it, followed by flags ending in `gu`.
  for (const m of body.matchAll(/\.replace\(\s*\/((?:[^/\\]|\\.)*)\/([a-z]*gu)\s*,\s*'([^']*)'\s*\)/gu)) {
    push(m.index, { op: 'replace', pattern: m[1], flags: m[2], replacement: m[3] });
  }
  hits.sort((a, b) => a.index - b.index);
  if (hits.length === 0) throw new Error('converter self-check: parsed no operations');
  return hits.map((hit) => hit.step);
}

// Assert the ports against the REAL TypeScript of both pins. The old pin is read from the working tree
// of the pinned checkout and the new pin from `git show 806d257:...` in that same checkout, so the
// checkout is read, never moved. An absent checkout is reported, not failed: a fresh clone stays runnable.
export function runConverterSelfCheck(repoRoot) {
  const checkout = path.join(repoRoot, ...FLIGHT_COMPILER_SRC);
  if (!fs.existsSync(path.join(checkout, '.git'))) {
    return { status: 'skipped', reason: `no pinned flight-compiler checkout at ${checkout}` };
  }
  let newSource;
  try {
    newSource = execFileSync('git', ['-C', checkout, 'show', `${NEW_PIN}:${IDENTITY_SOURCE_PATH}`], {
      encoding: 'utf8',
      maxBuffer: 64 * 1024 * 1024,
    });
  } catch (error) {
    return { status: 'skipped', reason: `${NEW_PIN} is not in the pinned checkout: ${String(error.message).split('\n')[0]}` };
  }
  const oldSource = fs.readFileSync(path.join(checkout, IDENTITY_SOURCE_PATH), 'utf8');

  const failures = [];
  const results = [];
  const cases = [
    { label: 'old snakeCase', body: extractBody(oldSource, OLD_SIGNATURE, 'old'), declared: OLD_CHAIN },
    { label: 'new convertCppIdentifierToSnakeCase', body: extractBody(newSource, NEW_SIGNATURE, 'new'), declared: NEW_CHAIN },
  ];
  for (const item of cases) {
    const parsed = parseChainFromBody(item.body);
    const chainMatches = JSON.stringify(parsed) === JSON.stringify(item.declared);
    if (!chainMatches) {
      failures.push(
        `${item.label}: parsed chain does not equal the declared chain.\nparsed:   ${JSON.stringify(parsed)}\ndeclared: ${JSON.stringify(item.declared)}`,
      );
    }
    results.push({ label: item.label, chainMatches, steps: parsed.length });
  }

  const examples = NAMED_EXAMPLES.map((example) => {
    const gotOld = oldSnakeCase(example.identifier);
    const gotNew = newSnakeCase(example.identifier);
    const ok = gotOld === example.oldCpp && gotNew === example.newCpp;
    if (!ok) failures.push(`named example ${example.identifier}: expected (${example.oldCpp}, ${example.newCpp}) got (${gotOld}, ${gotNew})`);
    return { ...example, gotOld, gotNew, ok };
  });

  return { status: failures.length === 0 ? 'pass' : 'fail', checkout, results, examples, failures };
}

// ------------------------------------------------------------------ identifier harvest and mapping

function walkFiles(root) {
  const out = [];
  const stack = [root];
  while (stack.length > 0) {
    const dir = stack.pop();
    let entries;
    try {
      entries = fs.readdirSync(dir, { withFileTypes: true });
    } catch {
      continue;
    }
    // Sorted for determinism: readdir order is filesystem-dependent, and the report must be stable.
    entries.sort((a, b) => (a.name < b.name ? -1 : a.name > b.name ? 1 : 0));
    for (const entry of entries) {
      const full = path.join(dir, entry.name);
      if (entry.isDirectory()) stack.push(full);
      else if (entry.isFile()) out.push(full);
    }
  }
  out.sort();
  return out;
}

// The emitter converts a source path to a module name only for a module it actually emits
// (`convertSourcePathToCppFileName` in the pinned identity source). A test file, a spec file, a
// barrel and an internal module are not emitted, so neither their names nor the identifiers they
// mention can reach the generated tree. Harvesting them anyway invents names the emitter never
// produces: `Node2D.test` and `Node2DTest` both reduce to `node_2d_test` and look like a merge,
// when the first is a test file's stem that no module name can ever be.
export function isEmittedModuleFile(file) {
  const stem = path.basename(file, path.extname(file));
  return !(
    stem.toLowerCase() === 'index' ||
    stem.toLowerCase() === 'internal' ||
    /(?:^|[._-])(?:spec|test)(?:$|[._-])/iu.test(stem) ||
    /test(?:helper|util)/iu.test(stem)
  );
}

// Every identifier the pinned Flight TypeScript mentions, plus every emitted module's basename. The
// basename matters because the emitter derives a module filename from the source path and that name
// need not appear as a token anywhere in the file.
export function harvestIdentifiers(repoRoot) {
  const packagesSrc = path.join(repoRoot, ...FLIGHT_PACKAGES_SRC);
  if (!fs.existsSync(packagesSrc)) return { status: 'skipped', reason: `no pinned Flight packages at ${packagesSrc}` };

  const identifiers = new Set();
  let files = 0;
  let tokenCount = 0;
  let basenameCount = 0;
  for (const file of walkFiles(packagesSrc)) {
    if (!TS_EXTENSIONS.has(path.extname(file))) continue;
    if (!isEmittedModuleFile(file)) continue;
    let text;
    try {
      text = fs.readFileSync(file, 'utf8');
    } catch {
      continue;
    }
    files += 1;
    for (const m of text.matchAll(TOKEN_RE)) {
      identifiers.add(m[0]);
      tokenCount += 1;
    }
    const stem = path.basename(file, path.extname(file));
    if (stem.length > 0) {
      identifiers.add(stem);
      basenameCount += 1;
    }
  }
  return { status: 'ok', files, tokenCount, basenameCount, identifiers: [...identifiers].sort() };
}

// Build the map and the part index it is consulted through. Keyed on the OLD spelling throughout: that
// is what makes a second run find nothing.
export function buildRenameMap(identifiers) {
  const atomic = new Map();
  const conflicts = [];
  const byOld = new Map(); // oldForm -> Set of newForms, for part lookups

  for (const identifier of identifiers) {
    const oldForm = oldSnakeCase(identifier);
    if (oldForm.length === 0) continue;
    const newForm = newSnakeCase(identifier);

    if (!byOld.has(oldForm)) byOld.set(oldForm, new Set());
    byOld.get(oldForm).add(newForm);

    if (oldForm === newForm) continue;
    if (atomic.has(oldForm)) {
      if (atomic.get(oldForm) !== newForm) conflicts.push({ oldForm, known: atomic.get(oldForm), other: newForm, otherIdentifier: identifier });
      continue;
    }
    atomic.set(oldForm, newForm);
  }

  // Two DIFFERENT old spellings reaching one new spelling would silently merge two identifiers. Report
  // it here rather than discovering it as a compile error in a regenerated tree.
  const byNew = new Map();
  for (const [oldForm, newForm] of atomic) {
    if (!byNew.has(newForm)) byNew.set(newForm, []);
    byNew.get(newForm).push(oldForm);
  }
  const merges = [...byNew.entries()]
    .filter(([, olds]) => olds.length > 1)
    .map(([newForm, olds]) => ({ newForm, oldForms: [...olds].sort() }))
    .sort((a, b) => (a.newForm < b.newForm ? -1 : 1));

  return { atomic, conflicts, merges, byOld };
}

// A cheap necessary condition for a token to be a compound that renames: at least one of its parts
// must rename, and every renaming old form contains `[0-9]` followed by an optional `_` and a `d`
// (rule 2 either leaves the `_` rule 1 introduced, or removes it as it rewrites `_D` to `D`). Asserted
// in buildScope, so this stays a proof rather than an assumption.
const RENAME_PREFILTER_LOWER = /[0-9]_?d/u;
const RENAME_PREFILTER_UPPER = /[0-9]_?D/u;

// The rule-1 signature in its sharpest usable form: a digit, an underscore, then a LOWERCASE d. That is
// exactly the `2_d` / `3_d` shape rule 1 mints for a `<digit><Upper>` source, and it is what the old
// emitter leaves behind and the new one rewrites to `2d` / `_2d`. The lowercase `d` is what keeps this
// from lighting up on foreign constants and runtime type names -- `AWD2_BLOCK_CAMERA` (`2_B`),
// `uint32_t` (`2_t`), `scale9_shape` (`9_s`) are not rename candidates and are not reported.
const RESIDUAL_SIGNATURE = /[0-9]_d/u;

// ------------------------------------------------------------------ compound decomposition

// The emitter synthesizes compound identifiers by joining converted parts, and a whole-token map misses
// a compound whose PARTS rename -- exactly the "incomplete map" failure the pin move is meant to catch.
// Decompose a token into `_`-joined parts and rename the join.
//
// The old `_`-joined spelling is NOT injective: `clone_texture2_d` re-reads as `clone` + `texture2_d`
// (the emitted name of the union member type) or as `clone` + `texture2` + `d` (two incidental source
// bindings). The two disagree, so a decomposition alone cannot decide. Which one the emitter produced
// is stated by the emitter's own output, and two witnesses settle it:
//
//   * For an anonymous object shape the parts ARE the shape's property names, and the emitted struct
//     declares one member per property, in order -- `getCppAnonymousStructBaseName` is
//     `properties.map(snakeCase).join('_')` and the members are those same conversions. So the emitted
//     struct's member list, joined, reproduces the name and states the split exactly. That is
//     `verifyAgainstEmittedStruct` below, and it is a proof rather than a preference.
//
//   * Otherwise the fine-grained reading is the one that invents parts: `d`, `dby`, `texture2` are only
//     reachable if the emitter really had separate bindings with those names. A part is a whole source
//     name, so the emitted join is the COARSEST decomposition -- the one with the fewest, longest parts.
//     Where the emitted structs are available it agrees with them member for member.
//
// A tie at the coarsest depth is reported as unresolved rather than broken by a rule of thumb.
function decomposeCoarsest(body, index) {
  const segments = body.split('_');
  const n = segments.length;
  const dp = Array.from({ length: n + 1 }, () => new Map());
  dp[0].set('', { count: 0, parts: [] });
  let overflow = false;
  for (let i = 0; i < n; i += 1) {
    if (dp[i].size === 0) continue;
    for (let j = i; j < n; j += 1) {
      const group = segments.slice(i, j + 1).join('_');
      const news = index.get(group);
      if (!news) continue;
      for (const [prefix, info] of dp[i]) {
        for (const next of news) {
          const key = prefix === '' ? next : `${prefix}_${next}`;
          const candidate = { count: info.count + 1, parts: [...info.parts, { part: group, next }] };
          const existing = dp[j + 1].get(key);
          if (!existing || candidate.count < existing.count) dp[j + 1].set(key, candidate);
        }
      }
      if (dp[j + 1].size > DECOMPOSITION_CAP) overflow = true;
    }
  }
  if (overflow) return { kind: 'ambiguous', candidates: [] };
  const results = [...dp[n].entries()];
  if (results.length === 0) return { kind: 'none' };
  const fewest = Math.min(...results.map(([, info]) => info.count));
  const best = results.filter(([, info]) => info.count === fewest);
  if (best.length !== 1) return { kind: 'ambiguous', candidates: best.map(([result]) => result).sort() };
  const [result, info] = best[0];
  return { kind: 'resolved', result, parts: info.parts };
}

// The emitted anonymous struct's declared members, read from the tree the emitter wrote. Only used to
// CONFIRM a decomposition, so a hand-written or absent definition changes nothing.
export function emittedStructMembers(text, name) {
  const at = text.indexOf(`struct ${name} :`);
  if (at < 0) return undefined;
  const open = text.indexOf('{', at);
  const close = text.indexOf('\n};', open);
  if (open < 0 || close < 0) return undefined;
  const members = [];
  for (const line of text.slice(open + 1, close).split('\n')) {
    const match = /^\s+[A-Za-z_][A-Za-z0-9_:<>, ]*\s([A-Za-z_][A-Za-z0-9_]*);\s*$/u.exec(line);
    if (match) members.push(match[1]);
  }
  return members.length > 0 ? members : undefined;
}

// ------------------------------------------------------------------ the scope

// A scope turns a token into a rename, or into a reason it cannot be one. It is keyed on OLD spellings
// and memoized, so a plan is a pure function of the tree and the identifier set.
export function createScope(identifiers) {
  const { atomic, conflicts, merges, byOld } = buildRenameMap(identifiers);

  // The prefilter is asserted over the map it filters rather than assumed, so a converter change that
  // invalidates it fails here instead of silently dropping compounds.
  for (const oldForm of atomic.keys()) {
    if (!RENAME_PREFILTER_LOWER.test(oldForm)) {
      throw new Error(`rename prefilter unsound: renaming form lacks the [0-9]_?d signature: ${oldForm}`);
    }
  }

  const compounds = new Map();
  const guards = new Map();
  const ambiguous = new Map();
  const structMembers = new Map();
  const structNames = new Set();

  function classifyCompound(token) {
    const isUpper = /^[A-Z0-9_]+$/u.test(token);
    // An UPPERCASED compound is only ever the anonymous-struct include guard
    // (`getCppAnonymousStructGuard`: FLIGHT_COMPILER_ANONYMOUS_<PACKAGE>_<name.toUpperCase()>`).
    // Restricting to that prefix matters: an uppercase token elsewhere in the tree is a source
    // SYMBOL name recorded verbatim in a JSON report (e.g. `AWD2_DATA_INT8`) or a foreign constant,
    // and neither is a snake-cased emitted C++ identifier.
    if (isUpper && !token.startsWith(GUARD_PREFIX)) return null;
    if (!(isUpper ? RENAME_PREFILTER_UPPER : RENAME_PREFILTER_LOWER).test(token)) return null;

    let prefix = '';
    let body = token;
    if (body.startsWith(INTERNAL_PREFIX)) {
      prefix = INTERNAL_PREFIX;
      body = body.slice(INTERNAL_PREFIX.length);
    }
    let suffix = '';
    const hashMatch = HASH_RE.exec(body);
    if (hashMatch) {
      suffix = hashMatch[0];
      body = body.slice(0, body.length - suffix.length);
    }
    if (body.length === 0) return null;

    // An include guard is `FLIGHT_COMPILER_ANONYMOUS_<PACKAGE>_<name.toUpperCase()>`
    // (`getCppAnonymousStructGuard`), so it renames exactly as the name does. The name is not
    // separable from the package by any rule of its own, but it does not have to be: the tree states
    // it, as the struct this guard wraps, so the guard's tail is matched against the struct names the
    // scan met. An uppercase token with no such tail is a foreign constant, not a guard.
    if (isUpper) {
      for (let cut = body.length; cut >= 0; cut -= 1) {
        if (cut > 0 && body[cut - 1] !== '_') continue;
        const stem = body.slice(cut).toLowerCase();
        if (stem.length === 0 || !structNames.has(stem)) continue;
        const renamed = lookup(stem);
        if (!renamed || renamed.kind === 'ambiguous') return null;
        const guard = `${prefix}${body.slice(0, cut)}${renamed.to.toUpperCase()}${suffix}`;
        return guard === token ? null : { kind: 'compound', from: token, to: guard, basis: 'emitted-struct-guard' };
      }
      return null;
    }

    // An anonymous struct's parts are its emitted members, when the tree states them. The member list
    // CONFIRMS the split, so it is used only where it actually reproduces the name; a struct whose
    // members are not all emitted identifiers, or whose name this is not, falls through to the rule
    // that does not need them.
    const members = isUpper ? undefined : structMembers.get(body);
    if (members) {
      const parts = [];
      let reproduces = true;
      for (const member of members) {
        const news = byOld.get(member);
        if (!news) {
          reproduces = false;
          break;
        }
        if (news.size !== 1) return { kind: 'ambiguous', from: token, candidates: [...news].sort() };
        parts.push({ part: member, next: [...news][0] });
      }
      if (reproduces && parts.map((p) => p.part).join('_') === body) {
        const renamed = `${prefix}${parts.map((p) => p.next).join('_')}${suffix}`;
        if (renamed === token) return null;
        return { kind: 'compound', from: token, to: renamed, basis: 'emitted-struct-members' };
      }
    }

    // Only the lowercase spelling reaches here; an uppercase token was settled above or is a foreign constant.
    const decomposition = decomposeCoarsest(body, byOld);
    if (decomposition.kind === 'none') return null;
    if (decomposition.kind === 'ambiguous') return { kind: 'ambiguous', from: token, candidates: decomposition.candidates.map((r) => `${prefix}${r}${suffix}`) };
    const renamed = `${prefix}${decomposition.result}${suffix}`;
    if (renamed === token) return null;
    return { kind: 'compound', from: token, to: renamed, basis: 'coarsest-tiling' };
  }

  function lookup(token) {
    if (atomic.has(token)) return { kind: 'atomic', from: token, to: atomic.get(token) };
    if (compounds.has(token)) return compounds.get(token);
    if (guards.has(token)) return guards.get(token);
    if (ambiguous.has(token)) return ambiguous.get(token);
    const classified = classifyCompound(token);
    if (!classified) return null;
    if (classified.kind === 'ambiguous') ambiguous.set(token, classified);
    else if (classified.to !== classified.to.toLowerCase()) guards.set(token, classified);
    else compounds.set(token, classified);
    return classified;
  }

  return {
    atomic,
    conflicts,
    merges,
    lookup,
    // Record an emitted struct definition the scan met, so the next lookup of its name can be confirmed
    // against the members the emitter itself declared, and so an include guard wrapping it can find it.
    noteStructMembers(name, members) {
      // The emitted name carries its structural hash; the decomposition works on the stem, and a guard
      // on the stem uppercased. Both spellings are recorded so either lookup lands.
      const stem = name.replace(HASH_RE, '');
      structNames.add(name);
      structNames.add(stem);
      if (!members) return;
      for (const key of [name, stem]) {
        if (!structMembers.has(key)) structMembers.set(key, members);
      }
    },
    compounds,
    guards,
    ambiguous,
  };
}

// ------------------------------------------------------------------ planning

export function validateTrees(trees) {
  for (const tree of trees) {
    if (EXCLUDED_TREES.includes(tree)) throw new Error(`tree ${tree} is excluded from the rename and must not be selected`);
  }
  return trees;
}

// Scan the selected trees and report what an apply would change. Reads only; writes nothing.
export function planRename({ repoRoot, scope, trees = SCOPE_TREES, contents = true, filenames = true }) {
  validateTrees(trees);
  const perTree = [];
  const contentChanges = [];
  const fileRenames = [];
  const ambiguous = [];
  const residuals = new Set();
  const collisions = [];

  for (const tree of trees) {
    const root = path.join(repoRoot, tree);
    if (!fs.existsSync(root)) {
      perTree.push({ tree, present: false, filesScanned: 0, filesWouldChange: 0, occurrences: 0, fileRenames: 0 });
      continue;
    }
    const files = walkFiles(root);
    const treeRenames = [];
    const changed = new Set();
    let occurrences = 0;

    // Struct definitions first: a name's members must be known before any token is classified, and the
    // struct and its uses can sit in different files.
    const texts = new Map();
    for (const file of files) {
      let text;
      try {
        text = fs.readFileSync(file, 'utf8');
      } catch {
        continue;
      }
      if (text.includes('\u0000')) continue;
      texts.set(file, text);
      for (const m of text.matchAll(/struct ([A-Za-z0-9_]+) :/gu)) {
        scope.noteStructMembers(m[1], emittedStructMembers(text, m[1]));
      }
    }

    for (const [file, text] of texts) {
      const rel = path.relative(root, file);
      const extension = path.extname(file);

      if (filenames) {
        let newStem = null;
        for (const m of path.basename(file, extension).matchAll(TOKEN_RE)) {
          const hit = scope.lookup(m[0]);
          if (!hit || hit.kind === 'ambiguous') continue;
          newStem = newStem ?? path.basename(file, extension);
          newStem = newStem.split(m[0]).join(hit.to);
        }
        if (newStem !== null && newStem !== path.basename(file, extension)) {
          const renamed = {
            tree,
            from: rel,
            to: path.join(path.dirname(rel), `${newStem}${extension}`),
            fromPath: file,
            toPath: path.join(path.dirname(file), `${newStem}${extension}`),
          };
          treeRenames.push(renamed);
          fileRenames.push(renamed);
        }
      }

      if (contents) {
        let occurrencesInFile = 0;
        const seen = new Map(); // newToken -> oldToken, to catch a merge inside one file
        const rebuilt = text.replace(TOKEN_RE, (token) => {
          const hit = scope.lookup(token);
          if (!hit) {
            // A token carrying the rule-1 rename signature that no identifier explains is a CANDIDATE
            // for the diff stage to confirm against the regenerated tree. NECESSARY BUT NOT SUFFICIENT:
            // `get_vector2_distance` <- getVector2Distance and `texture_2_d` <- texture_2D both carry it
            // and are correctly unchanged.
            if (RESIDUAL_SIGNATURE.test(token)) residuals.add(token);
            return token;
          }
          if (hit.kind === 'ambiguous') {
            ambiguous.push({ tree, file: rel, token, candidates: hit.candidates });
            return token;
          }
          if (seen.has(hit.to) && seen.get(hit.to) !== token) {
            collisions.push({
              kind: 'content-merge',
              tree,
              file: rel,
              detail: `${seen.get(hit.to)} and ${token} both rename to ${hit.to}`,
            });
          }
          seen.set(hit.to, token);
          occurrencesInFile += 1;
          return hit.to;
        });
        if (occurrencesInFile > 0) {
          changed.add(rel);
          occurrences += occurrencesInFile;
          contentChanges.push({ tree, file, relative: rel, text: rebuilt, occurrences: occurrencesInFile });
        }
      }
    }

    // A filename rename must not land on a file that is staying put. Renaming onto a file that is itself
    // renamed away is fine, so the check is against the set that remains.
    if (filenames) {
      // A rename may land on a file that is itself renaming away; only an occupant that stays put blocks it.
      const vacated = new Set(treeRenames.map((rename) => rename.fromPath));
      for (const rename of treeRenames) {
        if (!fs.existsSync(rename.toPath)) continue;
        if (vacated.has(rename.toPath)) continue;
        collisions.push({
          kind: 'filename-exists',
          tree,
          file: rename.from,
          detail: `${rename.from} renames to ${rename.to}, which already exists in ${path.dirname(rename.from) || '.'}`,
        });
      }
    }

    for (const rename of treeRenames) changed.add(rename.from);
    perTree.push({
      tree,
      present: true,
      filesScanned: texts.size,
      filesWouldChange: changed.size,
      occurrences,
      fileRenames: treeRenames.length,
    });
  }

  // The same ambiguous token can appear in many files; report it once, with every candidate seen.
  const ambiguousByToken = new Map();
  for (const entry of ambiguous) {
    if (!ambiguousByToken.has(entry.token)) ambiguousByToken.set(entry.token, new Set());
    for (const candidate of entry.candidates) ambiguousByToken.get(entry.token).add(candidate);
  }

  return {
    repoRoot,
    trees,
    contents,
    filenames,
    perTree,
    contentChanges,
    fileRenames,
    ambiguous,
    ambiguousTokens: [...ambiguousByToken.entries()]
      .sort((a, b) => (a[0] < b[0] ? -1 : 1))
      .map(([token, candidates]) => ({ token, candidates: [...candidates].sort() })),
    residuals: [...residuals].sort(),
    collisions: [...collisions].sort((a, b) => (a.kind + a.file < b.kind + b.file ? -1 : 1)),
  };
}

// A plan is applicable only when it is collision-free. Refusing here is the whole point: last-write-wins
// would silently merge two identifiers, or overwrite a file, with nothing downstream to notice.
export function planRefusal(plan, scope) {
  const reasons = [];
  if (scope.conflicts.length > 0) {
    for (const conflict of scope.conflicts) reasons.push(`map: ${conflict.oldForm} maps to both ${conflict.known} and ${conflict.other}`);
  }
  if (scope.merges.length > 0) {
    for (const merge of scope.merges) reasons.push(`map: ${merge.oldForms.join(' and ')} both rename to ${merge.newForm}`);
  }
  for (const collision of plan.collisions) reasons.push(`${collision.kind}: ${collision.detail}`);
  return reasons;
}

// ------------------------------------------------------------------ applying

// Apply a plan to the working tree. This is the only function in the module that writes inside the
// repository, and every caller reaches it through `--apply --in-place`.
export function applyRename(plan, { contents = plan.contents, filenames = plan.filenames } = {}) {
  let filesWritten = 0;
  let filesRenamed = 0;
  if (contents) {
    for (const change of plan.contentChanges) {
      const before = fs.readFileSync(change.file, 'utf8');
      if (before === change.text) continue;
      fs.writeFileSync(change.file, change.text);
      filesWritten += 1;
    }
  }
  if (filenames) {
    // Through a unique intermediate name, so a chain (a -> b, b -> c) cannot clobber one of its own
    // members. A collision has already been refused, so each target is free once its occupant moves.
    const staged = plan.fileRenames.map((rename, index) => {
      const temporary = path.join(path.dirname(rename.toPath), `.rename-${process.pid}-${index}-${path.basename(rename.toPath)}`);
      fs.renameSync(rename.fromPath, temporary);
      return { rename, temporary };
    });
    for (const { rename, temporary } of staged) {
      fs.renameSync(temporary, rename.toPath);
      filesRenamed += 1;
    }
  }
  return { filesWritten, filesRenamed };
}

// ------------------------------------------------------------------ report

export function formatReport(plan, scope, { selfCheck, harvest } = {}) {
  const lines = [];
  const push = (line = '') => lines.push(line);
  push('flight-cpp identifier rename -- compiler pin 2b687eaf -> 806d257');
  push(plan.contents || plan.filenames ? 'DRY RUN. Nothing was written inside the repository.' : '');
  if (selfCheck) {
    push('');
    push(`converter self-check: ${selfCheck.status}`);
    if (selfCheck.status === 'skipped') push(`  ${selfCheck.reason}`);
    for (const result of selfCheck.results ?? []) push(`  ${result.label}: chain matches parsed TS source = ${result.chainMatches} (${result.steps} steps)`);
    for (const example of selfCheck.examples ?? []) push(`  ${example.identifier} -> old ${example.gotOld} / new ${example.gotNew}  ${example.ok ? 'MATCH' : 'MISMATCH'}`);
  }
  if (harvest) {
    push('');
    push(`identifiers harvested: ${harvest.status}`);
    if (harvest.status === 'skipped') push(`  ${harvest.reason}`);
    else push(`  source files ${harvest.files}, tokens seen ${harvest.tokenCount}, distinct identifiers ${harvest.identifiers.length}`);
  }
  push('');
  push('mapping');
  push(`  atomic old->new pairs      ${scope.atomic.size}`);
  push(`  synthesized compounds      ${[...scope.compounds.values()].length}`);
  push(`  include-guard compounds    ${[...scope.guards.values()].length}`);
  push(`  ambiguous, NOT applied     ${plan.ambiguousTokens.length}`);
  push(`  map conflicts              ${scope.conflicts.length}`);
  push(`  map merges                 ${scope.merges.length}`);
  push('');
  push('per tree            scanned  would change  occurrences  filename renames');
  for (const row of plan.perTree) {
    push(
      `  ${row.tree.padEnd(16)} ${row.present ? String(row.filesScanned).padStart(7) : '  absent'} ${String(row.filesWouldChange).padStart(13)} ${String(row.occurrences).padStart(12)} ${String(row.fileRenames).padStart(17)}`,
    );
  }
  push('');
  push(`out of scope, never read or written: ${EXCLUDED_TREES.join(', ')}`);
  push('');
  push(`collisions: ${plan.collisions.length}`);
  for (const collision of plan.collisions) push(`  ${collision.kind}: ${collision.detail}`);
  push('');
  push(`ambiguous compounds, resolved from the emitter's own synthesis and NOT applied: ${plan.ambiguousTokens.length}`);
  for (const entry of plan.ambiguousTokens) push(`  ${entry.token}`);
  push('');
  push(`residual tokens carrying the rename signature: ${plan.residuals.length}`);
  return `${lines.filter((line, index, all) => !(line === '' && all[index - 1] === '')).join('\n')}\n`;
}

// ------------------------------------------------------------------ arguments

export function parseArguments(argv) {
  const args = {
    repo: process.cwd(),
    out: undefined,
    trees: [],
    verbose: false,
    help: false,
    apply: false,
    inPlace: false,
    contents: true,
    filenames: true,
  };
  const valued = new Set(['--repo', '--out', '--tree']);
  for (let index = 0; index < argv.length; index += 1) {
    const argument = argv[index];
    const equals = argument.startsWith('--') ? argument.indexOf('=') : -1;
    const flag = equals < 0 ? argument : argument.slice(0, equals);
    const inline = equals < 0 ? undefined : argument.slice(equals + 1);
    const value = () => {
      if (inline !== undefined) return inline;
      index += 1;
      if (index >= argv.length) throw new Error(`${argument} needs a value`);
      return argv[index];
    };
    if (flag === '--repo') args.repo = value();
    else if (flag === '--out') args.out = value();
    else if (flag === '--tree') args.trees.push(value());
    else if (flag === '--apply') args.apply = true;
    else if (flag === '--in-place') args.inPlace = true;
    else if (flag === '--contents-only') args.filenames = false;
    else if (flag === '--filenames-only') args.contents = false;
    else if (flag === '--verbose') args.verbose = true;
    else if (flag === '--help' || flag === '-h') args.help = true;
    else if (!valued.has(flag)) throw new Error(`unknown argument: ${argument}`);
  }
  return args;
}

export const USAGE = `usage:
  node scripts/renameIdentifiers.mjs [--repo <path>] [--tree <name>]... [--verbose]
  node scripts/renameIdentifiers.mjs --apply --in-place [--tree <name>]... [--contents-only|--filenames-only]

  --repo <path>        repository to scan (default: the current directory)
  --tree <name>        restrict to one tree, repeatable (default: ${SCOPE_TREES.join(', ')})
  --apply              write the rename. Requires --in-place; without it the run is refused.
  --in-place           acknowledge that the write target is the repository working tree itself
  --contents-only      rename identifiers in file contents only
  --filenames-only     rename filenames only
  --out <path>         write the report to a directory outside the repository (optional; the
                       report is printed to stdout and nothing else is written by default)
  --verbose            list every filename rename, not just the first ten

  source-patches/ is never read or written.`;

function isInside(candidate, parent) {
  const relative = path.relative(parent, candidate);
  return relative === '' || (!relative.startsWith('..') && !path.isAbsolute(relative));
}

export function main(argv = process.argv.slice(2)) {
  const args = parseArguments(argv);
  if (args.help) {
    process.stdout.write(`${USAGE}\n`);
    return 0;
  }
  const repoRoot = path.resolve(args.repo);
  const trees = validateTrees(args.trees.length > 0 ? args.trees : SCOPE_TREES);

  // The one irreversible thing this script can do is write inside the repository, so it is guarded
  // twice: the apply flag is opt-in, and it must name the repository as the deliberate target.
  if (args.apply && !args.inPlace) {
    process.stderr.write('refusing to apply: pass --in-place to acknowledge that the write target is the repository working tree.\n');
    return 2;
  }
  if (!args.apply && (args.contents === false || args.filenames === false)) {
    process.stderr.write('--contents-only and --filenames-only select an apply path; pass --apply with them.\n');
    return 2;
  }
  if (args.out !== undefined && isInside(path.resolve(args.out), repoRoot)) {
    process.stderr.write(`refusing to write inside the repository: --out ${path.resolve(args.out)}\n`);
    return 2;
  }

  const selfCheck = runConverterSelfCheck(repoRoot);
  if (selfCheck.status === 'fail') {
    process.stderr.write(`converter self-check FAILED; refusing to compute a map from an unchecked port:\n${selfCheck.failures.join('\n')}\n`);
    return 1;
  }

  // No map can be computed without the pinned source, so this reports and skips in both modes rather
  // than failing: a fresh clone stays runnable, and nothing is ever applied from an unchecked map.
  const harvest = harvestIdentifiers(repoRoot);
  if (harvest.status === 'skipped') {
    process.stdout.write(`identifier harvest skipped: ${harvest.reason}\nRun \`npm run rehydrate\` to materialize the pinned checkouts.\n`);
    return 0;
  }

  const scope = createScope(harvest.identifiers);
  const plan = planRename({ repoRoot, scope, trees, contents: args.contents, filenames: args.filenames });

  const report = formatReport(plan, scope, { selfCheck, harvest });
  if (args.out !== undefined) {
    fs.mkdirSync(path.resolve(args.out), { recursive: true });
    fs.writeFileSync(path.join(path.resolve(args.out), 'rename-report.txt'), report);
  }

  const refusal = planRefusal(plan, scope);
  if (args.apply) {
    if (refusal.length > 0) {
      process.stderr.write(`refusing to apply: ${refusal.length} collision(s)\n`);
      for (const reason of refusal) process.stderr.write(`- ${reason}\n`);
      return 1;
    }
    const applied = applyRename(plan, { contents: args.contents, filenames: args.filenames });
    process.stdout.write(`applied: ${applied.filesWritten} file(s) rewritten, ${applied.filesRenamed} file(s) renamed\n`);
    process.stdout.write(report);
    return 0;
  }

  process.stdout.write(report);
  if (refusal.length > 0) {
    process.stderr.write(`\n${refusal.length} collision(s) would refuse an apply:\n`);
    for (const reason of refusal) process.stderr.write(`- ${reason}\n`);
  }
  return 0;
}

if (import.meta.url === `file://${process.argv[1]}`) {
  process.exitCode = main();
}
