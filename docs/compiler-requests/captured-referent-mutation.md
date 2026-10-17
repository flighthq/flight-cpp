# A captured referent mutated after suspension needs a shared reference representation

A closure that captures an object it also mutates — after an await or a `finally` — is refused: the
emitter has no shared C++ representation to hang the mutation on, and it will not quietly copy the
referent, which would leave the source's other holders looking at a different object.

Measured at `2b687ea`, flight `a627849`.

## Minimal TypeScript

`@flighthq/midi`, `packages/midi/src/midiPort.ts:70`:

```ts
export function disposeMidiPort(port: MidiPort): Promise<MidiPortDisposeOutcome> {
  const state = getMidiPortResourceState(port);
  if (state === undefined || state.disposeCompleted) return Promise.resolve({ reason: 'already-disposed' });
  if (state.disposePending !== null) return state.disposePending;
  state.disposed = true;
  const pending = disposeOwnedMidiPort(port);
  state.disposePending = pending;
  void pending.finally(() => {
    if (state.disposePending === pending) state.disposePending = null;   // <- the capture, mutated
  });
  return pending;
}
```

```
cpp emission failed for @flighthq/midi/packages/midi/src/midiPort.ts: captured referent mutation of
state requires a shared C++ reference representation
```

`state` is the port's resource record, reached through a module-level map keyed by the port. The
closure is invoked from the settlement of `pending`, after the enclosing function has returned, and it
writes a field of that same record.

## Expected behavior

The captured `state` is the one object (`flight::Ref<…>`), and the write goes through it, exactly as
the enclosing function's own `state.disposed = true` already lowers. The refusal is about the *capture*
carrying a mutable referent across the closure boundary, so the lowering needs the closure to hold the
same reference rather than a copy or a by-value capture of the record.

## Why a copy is not the answer, and this is not a source problem

Copying the record into the closure would compile and would be wrong: the comparison
`state.disposePending === pending` and the write would then act on a copy, and the next
`disposeMidiPort` call — which reads the real map entry — would still see `disposePending` set. The
source's aliasing is the behavior. So this is a representation gap to close in the emitter (or a
runtime capability to design deliberately), not an expression to rewrite.

## Affected modules

`@flighthq/midi` — `midiPort.ts`, which refuses the whole module and cascades to `contract.ts`,
`index.ts` and `midiAccess.ts`. No other package is known to hit it today; the shape is ordinary
"mutate captured state in a completion callback", so others plausibly will.

Also refused at a627849: `createColorTween` in `@flighthq/tween` (`colorTween.ts`). Its update closure
writes `target[property]` on the caller's `Record<string, number>`; without a shared reference
representation the write would land on a copy. `flight::Record` already is that shared reference -- its
copies share their entries -- so the `tween-color-tween-shared-record-target` override supplies the
function with the closure capturing the record by value and writing the caller's own entries
(`generated_tween_test`: "the caller's record reaches the target colour"). The refusal stands for the
general case: a captured local of an unshared type still has no shared representation.

## Carrying workaround

None. `@flighthq/midi` is `partial` with this and two other defects named against it; no patch or
override re-homes the state, because re-homing it would change the identity the module is built on.

## G2 WGPU capture timer initial storage

At Flight `a627849` / compiler `806d257`, `wgpuScreenCapture.ts:132` map helper refuses
`cpp-shared-mutable-capture-missing-initial-storage`: `let timer: ReturnType<typeof setTimeout> | undefined`
is assigned inside a Promise executor and read in the enclosing finally. Preserve one mutable cell
whose initial value is undefined, updated to the exact host timer handle, then consumed by clearTimeout.
Its union must retain the bound timer handle, not an invented numeric id or copied callback local.

The source starts one GPU map, waits indefinitely when timeoutMs<=0, otherwise adopts a late map
rejection and races it against a named timeout, clearing the same timer in finally. Timeout does not
cancel or unmap a later resolution; retrying the same capture buffer is not a supplied recovery path.
The readback unmaps only after normal pixel copying; do not infer a readback-wide finally from this
timer-only finally. Default10000ms, exact errors/settlement and original buffer ownership stay intact.

This is an inspected refusal, not executed mapping/timer proof. Original1 remains0/1 at missing acquire;
an exact three-declaration unused-header candidate fails native [mapped-range access](webgpu-buffer-mapped-readback.md)
before link. No source initialization patch, timer/Task/host implementation, repair or generation is carried.
