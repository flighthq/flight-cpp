# SDK-local re-exports in an SDK export lane

The workspace inventory reads every `export ... from` in an `@flighthq/sdk` export lane and requires
its specifier to name a `@flighthq/*` package. Flight `a627849` adds a module the SDK package owns
itself and re-exports it from the root lane, so inventory analysis throws before any package — even a
closure that never reaches `@flighthq/sdk` — is lowered.

## Minimal TypeScript

```ts
// packages/sdk/src/index.ts
export * from '@flighthq/math';
export * from './sdkHost.ts'; // the SDK package's own module
```

## Expected behavior

A relative specifier names one of the SDK package's own modules. It is not a package exposure and
contributes nothing to the lane-to-package exposure map; its declarations belong to the SDK package's
own sources like any other module there. Package-scoped specifiers are read exactly as today.

Observed at `2b687ea`:

```
CompilerInventoryError: SDK export lane @flighthq/sdk has unsupported external target: ./sdkHost.ts
  code: 'unsupported-package-specifier'
```

## Affected modules

`packages/sdk/src/index.ts` and `packages/sdk/src/contract.ts` (both `export * from './sdkHost.ts'`),
added upstream by the Host.compress / LZMA commits (`7f686d61f`, `1c6ad4b6b`, `35dc3c566`).

## Carrying workaround

[`compiler-sdk-lane-local-reexports.patch`](../../source-patches/compiler-sdk-lane-local-reexports.patch)
skips relative specifiers in `readSdkExposures` instead of throwing. It expires when upstream reads
SDK-local re-exports as SDK-owned modules.
