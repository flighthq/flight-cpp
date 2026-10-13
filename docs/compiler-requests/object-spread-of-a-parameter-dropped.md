# An object spread of a parameter is emitted as if it were not there

`{ type: 'normal', enabled: true, ...template }` emits the two spelled defaults and **never reads
`template`**. The module compiles, the function returns an object, and every field the caller supplied is
silently gone — defaults filling is exactly the behavior the source documents, so the failure is not only
a lost argument but a lost contract. Nothing in the header or the ledger says so: the declaration is
emitted, not refused.

Measured at flight-compiler `2b687ea` / flight `a627849` against emitted `@flighthq/menu`.

## Observed

Source, `packages/menu/src/menu.ts:57-71`:

```ts
export function createMenuItemTemplate(
  template?: Readonly<Partial<MenuItemTemplate>>,
): NonEntityCreateResult<MenuItemTemplate, 'descriptor'> {
  const item: MenuItemTemplate = {
    type: 'normal',
    enabled: true,
    ...template,
  };
  if (item.submenu !== undefined) {
    item.submenu = item.submenu.map((child) => createMenuItemTemplate(child));
  }
  return item;
}
```

Emission, `generated/include/flight/menu/menu.hpp:66-73`:

```cpp
inline flight::Ref<flight::types::MenuItemTemplate> create_menu_item_template(std::optional<flight::StructuralRef<flight::RowReadonly<flight::RowPartial<flight::RowOf<flight::Ref<flight::types::MenuItemTemplate>>>>>> template_ = std::nullopt) {
  flight::Ref<flight::types::MenuItemTemplate> item = flight::make_ref<flight::types::MenuItemTemplate>(flight::types::MenuItemTemplate{.type = std::optional<flight::String>{flight::String("normal")}, .enabled = std::optional<bool>{true}});
  if (item->submenu.has_value()) {
    (item->submenu = std::optional<flight::Array<flight::Ref<flight::types::MenuItemTemplate>>>{item->submenu.value().map([=](flight::Ref<flight::types::MenuItemTemplate> child) { return create_menu_item_template(flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<flight::RowPartial<flight::RowOf<flight::Ref<flight::types::MenuItemTemplate>>>>>>(flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<flight::types::MenuItemTemplate>>>>(child))); })});
  }
  return item;
}
```

`template_` appears in the signature and nowhere else. Two consequences follow from the same omission:

- the caller's fields are dropped (`createMenuItemTemplate({ label: 'Save', type: 'checkbox' })` returns
  an item whose `label` is absent and whose `type` is `'normal'`);
- the recursive normalization never runs, because the `item->submenu` it guards is always unset on a fresh
  object — so a submenu passed by the caller is not normalized *and* is not carried.

The sibling `clone_menu_template` in the same module, which builds its clone and then reads
`row_get<RowKey<"submenu">>(template_)`, lowers correctly — so this is the **spread** lowering, not row
access in general.

## Where it fires

| site | source | effect |
| --- | --- | --- |
| `createMenuItemTemplate` | `menu.ts:57` | the package's public template factory loses its argument and its recursive fill |

`@flighthq/menu` is `assisted` with this hidden inside it: `menu.ts` is supplied by the
`menu-recursive-template-validation` override only because of a *different* refusal, and the override's
copy is the emitted text for every declaration except the one it supplies — so the dropped spread is in
the emitted tree and in the supply alike.

## Reduction

The measurement is a single call, and the test that found it is checked in:
`tests/generated_menu_test.cpp` asserts the no-argument default (which holds) and records why the
argument case is not asserted. Removing the parameter from the source makes the emission correct, which
locates the omission in the spread rather than in `createMenuItemTemplate`'s body.

## What would close this

Read the spread source through the same row access the emitted functions already use — the parameter is a
`RowReadonly<RowPartial<…>>`, whose every member reads as an optional, so each spelled default becomes
"caller value if present, else the default" and each member the caller did not name stays as emitted.
`named-interface-partial-shape.md` is the request that made this parameter's *shape* representable; this
one is that the shape is not consumed.

## Distinguish from the requests already filed

[`object-assign-onto-structural-record.md`](object-assign-onto-structural-record.md) is about
`Object.assign` onto a Record target. [`named-interface-partial-shape.md`](named-interface-partial-shape.md)
is about the `Partial<T>` row shape itself. [`corpus-context-structural-row-record-spread.md`](corpus-context-structural-row-record-spread.md)
is about spreading a structural row into a record elsewhere. This one is the *silent* case: no refusal, no
diagnostic, an emitted body that ignores its parameter.

## Carrying workaround

None. The package's own override would have to re-write the emitted body of a declaration the emitter
believes it emitted, which is the kind of edit the override mechanism exists to make visible; it is not
made here because the same file is already supplied for a different refusal and mixing the two would hide
which one the copy is answering.

## A second shape: spreads of literal and conditional operands (`@flighthq/assets`)

The drop is not specific to parameters. Measured in a scratch compiler at `2b687ea` with all current
compiler source patches:

```ts
export interface D { id: string; groups?: string[] }
export function copy(d: Readonly<D>): D {
  const groups = d.groups;
  return { id: d.id, ...(groups === undefined ? {} : { groups: [...groups] }) };
}
```

This emits `return flight::make_ref<D>(D{.id = flight::row_get<flight::RowKey<"id">>(d)});`, with no
`groups` at all. A plain literal spread, `{ id: d.id, ...{ groups: ['x'] } }`, drops the same way. The
control, an explicit conditional assignment (`if (groups !== undefined) out.groups = [...groups]`), emits
correctly.

`@flighthq/assets` `assetLibrary.ts` (a627849) `copyAssetDescriptor` uses exactly this spread, so every
registered descriptor was stored without its groups and every group index stayed empty. Upstream's
`getAssetGroupIds` and `registerAssetDescriptor` group cases fail natively against the generated form.

The `assets-reject-task-template-parameter` override carries a corrected `copy_asset_descriptor`, and
`tests/generated_assets_test.cpp` asserts it, including a deduplication case whose expected value is upstream's
own `copyAssetDescriptor` run under node.
