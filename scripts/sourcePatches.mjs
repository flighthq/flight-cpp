import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { existsSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import path from 'node:path';

// Patches applied to a pinned sibling checkout before generation, so a module the compiler refuses to
// lower today can still ship.
//
// Patching the INPUT rather than the output is the cheaper half of the same idea: the refusals that
// matter are usually one expression the emitter cannot project, and rewriting that expression into an
// equivalent it does project is a smaller, far more reviewable change than hand-authoring the header
// the compiler declined to write. What makes it safe is that the rewrite must be an equivalence, and
// the manifest makes each author state the one that applies.
//
// Three rules hold the mechanism together:
//
//   Integrity first. Patches are applied only AFTER the caller has checked the checkout is clean and
//   at its pinned revision, so `baseCommit` describes real code and a patch can never paper over a
//   checkout that was already modified.
//
//   The checkout is dirty for exactly one run. `revert` restores the touched paths, and the caller
//   runs it in a `finally`, so an interrupted run does not leave the pinned source rewritten.
//
//   A patch that no longer applies is a hard failure, never a skip. A pin move that invalidates a
//   patch has to be seen; applying a patch to code it was not written against, or quietly carrying a
//   patch the pin has outgrown, is how a pinned build turns into an unacknowledged fork.

const SCHEMA = 'flight-cpp-source-patches/1';
// What the patch's `refusal` field names. A patch can answer a compiler REFUSAL, in which case
// generation can check the refusal is gone, or a C++ DIAGNOSTIC in emitted code, which only the header
// compile gate can see. The distinction exists so the effectiveness check does not quietly pass a
// patch it cannot actually evaluate.
const ANSWERS = new Set(['refusal', 'diagnostic']);
const REQUIRED = [
  'id',
  'dependency',
  'patch',
  'baseCommit',
  'module',
  'package',
  'refusal',
  'rewrite',
  'equivalence',
  'whyNotUpstreamFirst',
  'unblocks',
  'expires',
  'appliedOn',
];

export function loadSourcePatches(root) {
  const directory = path.join(root, 'source-patches');
  const file = path.join(directory, 'manifest.json');
  let parsed;
  try {
    parsed = JSON.parse(readFileSync(file, 'utf8'));
  } catch (error) {
    if (error.code === 'ENOENT') return [];
    throw new Error(`Source patch manifest ${portable(file)} is not readable JSON: ${error.message}`);
  }
  if (parsed.schema !== SCHEMA) {
    throw new Error(`Source patch manifest ${portable(file)} has schema ${parsed.schema}, expected ${SCHEMA}`);
  }
  if (!Array.isArray(parsed.patches)) throw new Error(`Source patch manifest ${portable(file)} has no patches array`);
  return parsed.patches.map((patch) => {
    for (const field of REQUIRED) {
      if (typeof patch[field] !== 'string' || patch[field].length === 0) {
        throw new Error(`Source patch ${patch.id ?? '<unnamed>'} is missing required field ${field}`);
      }
    }
    patch.answers ??= 'refusal';
    if (!ANSWERS.has(patch.answers)) {
      throw new Error(`Source patch ${patch.id} has unknown answers ${patch.answers}`);
    }
    const patchFile = path.join(directory, patch.patch);
    let contents;
    try {
      contents = readFileSync(patchFile, 'utf8');
    } catch {
      throw new Error(`Source patch ${patch.id} names ${portable(patchFile)}, which is not readable`);
    }
    return { ...patch, contents, digest: createHash('sha256').update(contents).digest('hex'), file: patchFile };
  });
}

// Applies every patch for `dependency`, which the caller has already verified is clean and at its pin.
// Returns the records to record in the manifest. Throws on the first patch that does not apply.
export function applySourcePatches(patches, dependency) {
  const applied = [];
  for (const patch of patches) {
    if (patch.dependency !== dependency.name) continue;
    if (patch.baseCommit !== dependency.commit) {
      throw new Error(
        `Source patch ${patch.id} was written against ${patch.baseCommit.slice(0, 7)} but ${dependency.name} is ` +
          `pinned at ${dependency.commit.slice(0, 7)}. Rewrite the patch against the new pin, or drop it if the ` +
          'refusal it answers is gone.',
      );
    }
    const check = git(dependency.directory, ['apply', '--check', '--verbose', patch.file]);
    if (check.status !== 0) {
      throw new Error(
        `Source patch ${patch.id} no longer applies to ${dependency.name} at ${dependency.commit.slice(0, 7)}:\n` +
          `${check.stderr}`,
      );
    }
    const result = git(dependency.directory, ['apply', patch.file]);
    if (result.status !== 0) throw new Error(`Source patch ${patch.id} failed to apply: ${result.stderr}`);
    applied.push({
      dependency: patch.dependency,
      digest: patch.digest,
      id: patch.id,
      module: patch.module,
      // Carried so a consumer of the manifest can attribute the patch to its PACKAGE. Without it a
      // package that only works because of a patch reads as though it needed no help, which defeats
      // the provenance the mechanism exists to keep.
      package: patch.package,
    });
  }
  // Records which process owns the patched state, so a later run can tell a dead run's leftovers from a
  // live run's working tree. Written only when something was actually applied; see recoverStalePatches.
  if (applied.length > 0) {
    writeFileSync(
      ownershipMarker(dependency),
      `${JSON.stringify({ patches: applied.map((record) => record.id), pid: process.pid, startedAt: new Date().toISOString() }, undefined, 2)}\n`,
    );
  }
  return applied;
}

// Restores every path the applied patches touched. Safe to call when nothing was applied, and safe to
// call twice, so it belongs in a `finally`.
// Who currently owns the patched state of a checkout. Written beside the checkout rather than inside it,
// so recording ownership does not itself dirty the tree the integrity check reads.
function ownershipMarker(dependency) {
  return path.join(path.dirname(dependency.directory), `.${path.basename(dependency.directory)}-source-patches.json`);
}

function readOwner(dependency) {
  const file = ownershipMarker(dependency);
  if (!existsSync(file)) return undefined;
  try {
    const parsed = JSON.parse(readFileSync(file, 'utf8'));
    return typeof parsed.pid === 'number' ? parsed : undefined;
  } catch {
    return undefined;
  }
}

function ownerIsAlive(pid) {
  if (pid === process.pid) return true;
  try {
    // Signal 0 tests for existence without delivering anything.
    process.kill(pid, 0);
    return true;
  } catch (error) {
    // EPERM means the process exists but belongs to someone else, which still counts as alive.
    return error.code === 'EPERM';
  }
}

// Recovers a checkout left patched by a run that could not clean up after itself.
//
// `revertSourcePatches` runs in a `finally` and from a SIGINT/SIGTERM/SIGHUP handler, which covers every
// ending the process gets a say in. SIGKILL is not one of them, and the generation hang has to be killed
// that way -- it ignores SIGTERM, which is what proves it is a synchronous spin. So a killed run leaves
// all three patches applied, and the NEXT run refuses with "flight has uncommitted changes" and no hint
// about why. That is a confusing failure and it cost a measurement here.
//
// The hazard in fixing it is the opposite mistake. A reverting recovery that only asks "is this dirt
// shaped like our patches?" cannot tell a DEAD run's leftovers from a LIVE run's working state, and would
// happily pull the patches out from under a generation in progress -- which is not hypothetical, it is
// what the first version of this function did to a running 73-package run. So ownership is recorded
// explicitly: `applySourcePatches` writes a marker naming its pid, `revertSourcePatches` removes it, and
// recovery proceeds only when the owner is GONE.
//
// Three conditions, all required:
//
//   1. A marker exists and its pid is not alive. No marker means we cannot show the dirt is ours, so the
//      integrity check's refusal stands; a live pid means another run owns it and is reported as such.
//   2. Every modified path in the checkout is a path some declared patch touches, so a file nobody
//      declared a patch against is never discarded.
//   3. Every patch whose paths are dirty reverse-applies CLEANLY. `git apply --reverse --check` succeeds
//      only if the patch's exact added lines are present, which is a byte-level identification rather
//      than a guess from filenames.
//
// Returns `{ recovered, heldBy }`: the ids reverted, or the live pid that owns the checkout.
export function recoverStalePatches(patches, dependency) {
  const mine = patches.filter((patch) => patch.dependency === dependency.name);
  if (mine.length === 0) return { recovered: [] };
  const owner = readOwner(dependency);
  if (owner === undefined) return { recovered: [] };
  if (ownerIsAlive(owner.pid)) return { heldBy: owner.pid, recovered: [] };
  const status = git(dependency.directory, ['status', '--porcelain']);
  if (status.status !== 0) return { recovered: [] };
  const dirty = status.stdout
    .split('\n')
    .map((line) => line.slice(3).trim())
    .filter((line) => line.length > 0);
  if (dirty.length === 0) {
    // The owner died after reverting but before removing its marker. Nothing to undo; clear the marker.
    rmSync(ownershipMarker(dependency), { force: true });
    return { recovered: [] };
  }
  const declared = new Set(mine.flatMap((patch) => patchedPaths(patch)));
  if (dirty.some((filename) => !declared.has(filename))) return { recovered: [] };
  const dirtySet = new Set(dirty);
  const applied = mine.filter((patch) => patchedPaths(patch).some((filename) => dirtySet.has(filename)));
  for (const patch of applied) {
    if (git(dependency.directory, ['apply', '--reverse', '--check', patch.file]).status !== 0) {
      return { recovered: [] };
    }
  }
  for (const patch of applied) git(dependency.directory, ['apply', '--reverse', '--quiet', patch.file]);
  rmSync(ownershipMarker(dependency), { force: true });
  return { recovered: applied.map((patch) => patch.id) };
}

// The repository-relative paths a patch file touches, read from its own `+++ b/...` headers.
function patchedPaths(patch) {
  const text = readFileSync(patch.file, 'utf8');
  return [...text.matchAll(/^\+\+\+ b\/(.+)$/gmu)].map((found) => found[1].trim());
}

export function revertSourcePatches(patches, dependency) {
  let reverted = false;
  for (const patch of patches) {
    if (patch.dependency !== dependency.name) continue;
    git(dependency.directory, ['apply', '--reverse', '--quiet', patch.file]);
    reverted = true;
  }
  // Releases ownership. Removed after the reverts so a process killed mid-revert still leaves a marker
  // naming it, which is what lets the next run recover rather than refuse.
  if (reverted) rmSync(ownershipMarker(dependency), { force: true });
}

// A patch earns its place by removing a refusal. When the refusal it names is still there, the patch is
// not doing what it claims and the caller should say so rather than carry it.
//
// The check looks at the whole PACKAGE, not just the patched module. A patch often edits one file to
// clear a rule that fires in several -- narrowing an assertion in node.ts to clear six consumers of it,
// say -- and a module-only check passes such a patch the moment its own file happens to be refused for
// some other reason. That is exactly how an ineffective patch survived a run here: node.ts was refused
// under `cpp-reference-assertion-without-heritage` while the rule the patch targeted still fired, in
// six other modules, unreported.
export function ineffectivePatches(applied, patchesById, refusals) {
  const ineffective = [];
  for (const record of applied) {
    const patch = patchesById.get(record.id);
    if (!patch) continue;
    // A patch answering a C++ diagnostic leaves no trace in the refusal ledger; the header compile
    // gate is what judges it, so claiming anything here would be guessing.
    if (patch.answers !== 'refusal') continue;
    const still = refusals.some(
      (refusal) =>
        refusal.reason.includes(patch.refusal) &&
        (refusal.module === patch.module || refusal.package === patch.package),
    );
    if (still) ineffective.push(record.id);
  }
  return ineffective;
}

function git(directory, args) {
  return spawnSync('git', args, { cwd: directory, encoding: 'utf8' });
}

function portable(filename) {
  return filename.split(path.sep).join('/');
}
