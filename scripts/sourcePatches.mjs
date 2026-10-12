import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { copyFileSync, existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
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

// A patch the pin has outgrown is declared, not skipped. When a pin move breaks a patch, the patch keeps
// its manifest entry and its original `baseCommit`, and gains `pendingRebase: { since, lane, reason }`:
// the pin it is pending at, the lane that owns it, and why it cannot be applied there -- usually a rewrite
// the pin broke, sometimes a rewrite that applies but must not be activated yet. A pending
// patch is never applied, and every generation names the pending set, so the debt stays visible while
// the rest of the pin generates. This is what lets one broken patch stop blocking every package.
export function loadSourcePatches(root) {
  return readSourcePatches(root).filter((patch) => patch.pendingRebase === undefined);
}

export function loadPendingSourcePatches(root) {
  return readSourcePatches(root).filter((patch) => patch.pendingRebase !== undefined);
}

function readSourcePatches(root) {
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
    if (patch.pendingRebase !== undefined) {
      const pending = patch.pendingRebase;
      if (
        typeof pending !== 'object' ||
        pending === null ||
        !/^[0-9a-f]{40}$/u.test(pending.since ?? '') ||
        typeof pending.lane !== 'string' ||
        pending.lane.length === 0 ||
        typeof pending.reason !== 'string' ||
        pending.reason.length === 0
      ) {
        throw new Error(`Source patch ${patch.id} has a pendingRebase without since, lane and reason`);
      }
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
    // The exact bytes applied are kept beside the ownership marker, and every later revert uses them.
    // A patch file can change while a run is in flight -- a fast-forwarded working tree rewrites it --
    // and reverting the NEW file against the OLD application fails, leaving the checkout dirty.
    const file = snapshotPatch(dependency, patch);
    const check = git(dependency.directory, ['apply', '--check', '--verbose', file]);
    if (check.status !== 0) {
      throw new Error(
        `Source patch ${patch.id} no longer applies to ${dependency.name} at ${dependency.commit.slice(0, 7)}:\n` +
          `${check.stderr}`,
      );
    }
    const result = git(dependency.directory, ['apply', file]);
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

// Where the applied bytes of each patch are kept for the life of the patched state.
function snapshotDirectory(dependency) {
  return `${ownershipMarker(dependency)}.applied`;
}

function snapshotPatch(dependency, patch) {
  const directory = snapshotDirectory(dependency);
  mkdirSync(directory, { recursive: true });
  const file = path.join(directory, `${patch.id}.patch`);
  copyFileSync(patch.file, file);
  return file;
}

// The file to reverse for a patch: the bytes that were applied when a snapshot exists, otherwise the
// declared file (a checkout patched before snapshots existed).
function appliedFile(dependency, patch) {
  const file = path.join(snapshotDirectory(dependency), `${patch.id}.patch`);
  return existsSync(file) ? file : patch.file;
}

function releaseOwnership(dependency) {
  rmSync(ownershipMarker(dependency), { force: true });
  rmSync(snapshotDirectory(dependency), { force: true, recursive: true });
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
// ending the process gets a say in. SIGKILL is not one of them, so a killed run can leave patches applied,
// and the NEXT run would otherwise refuse with "dependency has uncommitted changes" and no hint why.
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
//   3. The marker names every patch this run applied. They are unwound in REVERSE application order,
//      because a later patch may intentionally overlap an earlier one's context. Each reverse is checked
//      before mutation; an already-unwound patch must instead forward-check cleanly. If neither proof
//      holds, every reversal made during this recovery is reapplied and the checkout is left untouched.
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
    releaseOwnership(dependency);
    return { recovered: [] };
  }
  const ownedIds = Array.isArray(owner.patches) ? owner.patches : mine.map((patch) => patch.id);
  // A patch the owner applied may since have been renamed or retired in the manifest; its snapshot is
  // still the exact bytes to unwind. Without one, only a patch the manifest still declares is usable.
  const declaredById = new Map(mine.map((patch) => [patch.id, patch]));
  const owned = ownedIds
    .map((id) => {
      const snapshot = path.join(snapshotDirectory(dependency), `${id}.patch`);
      if (existsSync(snapshot)) return { file: snapshot, id };
      return declaredById.get(id);
    })
    .filter((patch) => patch !== undefined);
  const declared = new Set(owned.flatMap((patch) => patchedPaths(patch)));
  if (dirty.some((filename) => !declared.has(filename))) return { recovered: [] };
  const reverted = [];
  for (const patch of [...owned].reverse()) {
    if (git(dependency.directory, ['apply', '--reverse', '--check', patch.file]).status === 0) {
      if (git(dependency.directory, ['apply', '--reverse', '--quiet', patch.file]).status !== 0) {
        restoreRevertedPatches(dependency, reverted);
        return { recovered: [] };
      }
      reverted.push(patch);
      continue;
    }
    // A normal cleanup may have unwound some patches before being interrupted. A clean forward check is
    // byte-level evidence that this patch is absent at precisely this point in the reverse-order stack.
    if (git(dependency.directory, ['apply', '--check', patch.file]).status === 0) continue;
    restoreRevertedPatches(dependency, reverted);
    return { recovered: [] };
  }
  const after = git(dependency.directory, ['status', '--porcelain']);
  if (after.status !== 0 || after.stdout.length > 0) {
    restoreRevertedPatches(dependency, reverted);
    return { recovered: [] };
  }
  releaseOwnership(dependency);
  return { recovered: reverted.map((patch) => patch.id).reverse() };
}

function restoreRevertedPatches(dependency, reverted) {
  for (const patch of [...reverted].reverse()) git(dependency.directory, ['apply', '--quiet', patch.file]);
}

// The repository-relative paths a patch file touches, read from its own `+++ b/...` headers.
function patchedPaths(patch) {
  const text = readFileSync(patch.file, 'utf8');
  return [...text.matchAll(/^\+\+\+ b\/(.+)$/gmu)].map((found) => found[1].trim());
}

export function revertSourcePatches(patches, dependency) {
  const mine = patches.filter((patch) => patch.dependency === dependency.name);
  let clean = true;
  for (const patch of [...mine].reverse()) {
    const file = appliedFile(dependency, patch);
    if (git(dependency.directory, ['apply', '--reverse', '--check', file]).status === 0) {
      if (git(dependency.directory, ['apply', '--reverse', '--quiet', file]).status !== 0) clean = false;
      continue;
    }
    // Safe when cleanup is called before this dependency was patched or after a partial unwind.
    if (git(dependency.directory, ['apply', '--check', file]).status !== 0) clean = false;
  }
  // Releases ownership. Removed after the reverts so a process killed mid-revert still leaves a marker
  // naming it, which is what lets the next run recover rather than refuse.
  const status = git(dependency.directory, ['status', '--porcelain']);
  if (mine.length > 0 && clean && status.status === 0 && status.stdout.length === 0) {
    releaseOwnership(dependency);
  }
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
