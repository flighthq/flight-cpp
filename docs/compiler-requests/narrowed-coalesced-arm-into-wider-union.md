# A binding narrowed to one coalesced arm must enter a wider union as the slot it is stored in

Measured at flight-compiler `2b687ea` with every carried patch, against flight `a627849`.

## Minimal TypeScript

```ts
export interface F { readonly error?: unknown; readonly step: 'native-resource' }
export type DC = { readonly outcome: 'destroyed' } | { readonly failures: readonly F[]; readonly outcome: 'tray-destroy-failed' };
export type DR = DC | { readonly outcome: 'already-destroyed' };

export function n(result: DC): DR {
  if (result.outcome === 'destroyed') { return result; }
  return result;
}
```

## Refusal

```
contextual union value type flight::Ref<outcome_4c14b6b0d4cf2d84> is not a represented runtime domain: the value
leaves a member the destination requires optional or absent, or gives it an incompatible type ...
```

## Cause

`DC` stores `{ outcome: 'destroyed' }` in the slot `Ref<outcome_bd96…>`, a struct with one `outcome` member. `DR`
stores both `destroyed` and `already-destroyed` in that same slot, because the two arms differ only in a string
discriminant. Inside the `if`, `result` is narrowed to the `destroyed` arm:
- reading it emits `std::get<1>(result)`, which is a `Ref<outcome_bd96…>`;
- matching it to a destination slot spells its type afresh, from that one arm, as `outcome_4c14…`. That name is
  given to no stored value, so it matches no slot and the return is refused.

The value and the destination slot are the same C++ type. Only the name used to compare them differs.

## Required behavior

When the value is a binding narrowed to one member of its own union, and its storage holds that member in a slot,
the value is that slot's carrier. If the destination has a slot of exactly that carrier, the value enters it
unchanged: same object, same identity, no conversion. In every other case, the existing refusal stands:
- a value whose own declared type is the single-arm struct;
- a member held in more than one slot;
- a destination with no slot of that carrier.

## Affected modules

`@flighthq/tray` `destroyTrayRuntime` (`return result` after `result.outcome === 'destroyed'`). That function is
independently blocked by [caught-value-into-unknown-member.md](caught-value-into-unknown-member.md), so this
alone does not let it emit.
