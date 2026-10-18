// The two approved source relations; no backend implementation or SDK substitute.
const EntityRuntimeKey = Symbol.for('EntityRuntime');
type Policy = 'refresh' | 'invalidate';
interface RenderRegistries { nodeRenderers: Map<string, number>; callback?: () => void }
interface GlRenderRegistries extends RenderRegistries { glTag: number }
interface RenderStateRuntime { frame: number; registries: RenderRegistries }
interface GlRenderStateRuntime extends RenderStateRuntime { registries: GlRenderRegistries; glFrame: number }
interface RenderState { [EntityRuntimeKey]: RenderStateRuntime | undefined; policy: Policy }
interface GlRenderState extends RenderState { [EntityRuntimeKey]: GlRenderStateRuntime | undefined; gl: number }

export function runSourceContract() {
  const out: Record<string, boolean> = {};
  const firstRegistries: GlRenderRegistries = { nodeRenderers: new Map([['first', 1]]), glTag: 7 };
  const secondRegistries: GlRenderRegistries = { nodeRenderers: new Map([['second', 2]]), glTag: 11 };
  const firstRuntime: GlRenderStateRuntime = { frame: 3, registries: firstRegistries, glFrame: 5 };
  const secondRuntime: GlRenderStateRuntime = { frame: 13, registries: secondRegistries, glFrame: 17 };
  const state: GlRenderState = { [EntityRuntimeKey]: firstRuntime, policy: 'refresh', gl: 19 };
  const base: RenderState = state;
  out.stateOwnerIdentity = base === state;
  out.baseReadsPresentRuntime = base[EntityRuntimeKey] !== undefined;
  out.baseReadsExactRuntime = base[EntityRuntimeKey] === firstRuntime;
  base[EntityRuntimeKey] = secondRuntime;
  out.baseReplacementVisibleToDerived = state[EntityRuntimeKey] === secondRuntime;
  state[EntityRuntimeKey] = firstRuntime;
  out.derivedReplacementVisibleToBase = base[EntityRuntimeKey] === firstRuntime;
  base[EntityRuntimeKey] = undefined;
  out.baseClearVisibleToDerived = state[EntityRuntimeKey] === undefined;
  out.clearKeepsPropertyPresent = EntityRuntimeKey in state;
  state[EntityRuntimeKey] = secondRuntime;
  out.replacementAfterClear = base[EntityRuntimeKey] === secondRuntime;
  out.internedSymbolIdentity = EntityRuntimeKey === Symbol.for('EntityRuntime');
  out.sameDescriptionSymbolDistinct = !(Symbol('EntityRuntime') in state);

  const missing: Partial<GlRenderState> = {};
  out.absentIsNotPresent = !(EntityRuntimeKey in missing);
  missing[EntityRuntimeKey] = undefined;
  out.writtenUndefinedIsPresent = EntityRuntimeKey in missing;
  delete missing[EntityRuntimeKey];
  out.deletedIsNotPresent = !(EntityRuntimeKey in missing);

  const baseRuntime: RenderStateRuntime = firstRuntime;
  out.runtimeOwnerIdentity = baseRuntime === firstRuntime;
  out.baseReadsExactRegistries = baseRuntime.registries === firstRegistries;
  baseRuntime.registries = secondRegistries;
  out.baseRegistryReplacementVisible = firstRuntime.registries === secondRegistries;
  firstRuntime.registries = firstRegistries;
  out.derivedRegistryReplacementVisible = baseRuntime.registries === firstRegistries;
  baseRuntime.registries.nodeRenderers.set('live', 23);
  out.registryMutationLive = firstRuntime.registries.nodeRenderers.get('live') === 23;
  let calls = 0;
  const callback = () => { ++calls; };
  baseRuntime.registries.callback = callback;
  out.callableIdentityPreserved = firstRuntime.registries.callback === callback;
  firstRuntime.registries.callback!();
  out.callableEffectOnce = calls === 1;

  // Legal base writes can invalidate the derived annotation. Value reads remain lazy. A future
  // typed native escape is partial and must raise the shared dedicated out-of-profile error.
  const baseOnlyRegistries: RenderRegistries = { nodeRenderers: new Map([['base', 29]]) };
  baseRuntime.registries = baseOnlyRegistries;
  out.baseOnlyRegistryWriteChangesPhysicalCell = firstRuntime.registries === baseOnlyRegistries;
  out.baseOnlyRegistryRawReadIsLazy = typeof firstRuntime.registries === 'object';
  out.baseOnlyRegistryDerivedMemberIsUndefined = typeof firstRuntime.registries.glTag === 'undefined';
  const baseOnlyRuntime: RenderStateRuntime = { frame: 31, registries: baseOnlyRegistries };
  base[EntityRuntimeKey] = baseOnlyRuntime;
  out.baseOnlyRuntimeWriteChangesPhysicalCell = state[EntityRuntimeKey] === baseOnlyRuntime;
  out.baseOnlyRuntimeRawReadIsLazy = typeof state[EntityRuntimeKey] === 'object';
  out.baseOnlyRuntimeDerivedMemberIsUndefined = typeof state[EntityRuntimeKey]!.glFrame === 'undefined';
  return out;
}
