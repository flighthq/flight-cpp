// A real consumer of the original package contract. No local replacement codec or parser types.
import type {
  ParseParticleConfigOptions, ParticleConfigParseResult, ParticleEmitterConfig,
  ParticleFormatDescriptor, ParticleImportOptions,
} from '@flighthq/types/contract';
import {
  applyParticleImportOptions, getParticleFormatCodec, parseParticleConfig, parseParticleConfigDocument,
  parseRegisteredParticleFormat, libgdxParticleFormat, starlingPexParticleFormat,
  particleDesignerParticleFormat, unityParticleFormat,
  parseLibgdxParticle, parseLibgdxParticleDocument, parseStarlingPex, parseStarlingPexDocument,
  parseParticleDesignerPlist, parseParticleDesignerPlistDocument, parseUnityParticle, parseUnityParticleDocument,
} from '@flighthq/particles-formats/contract';

export function namedCallbacksRetained(): boolean {
  return libgdxParticleFormat.codec.parseToConfig === parseLibgdxParticle &&
    libgdxParticleFormat.codec.parseToDocument === parseLibgdxParticleDocument &&
    starlingPexParticleFormat.codec.parseToConfig === parseStarlingPex &&
    starlingPexParticleFormat.codec.parseToDocument === parseStarlingPexDocument &&
    particleDesignerParticleFormat.codec.parseToConfig === parseParticleDesignerPlist &&
    particleDesignerParticleFormat.codec.parseToDocument === parseParticleDesignerPlistDocument &&
    unityParticleFormat.codec.parseToConfig === parseUnityParticle &&
    unityParticleFormat.codec.parseToDocument === parseUnityParticleDocument;
}
export function publicConfig(text: string, options?: Readonly<ParseParticleConfigOptions>): ParticleEmitterConfig {
  return parseParticleConfig(text, options);
}
export function publicDocument(text: string, options?: Readonly<ParseParticleConfigOptions>): ParticleConfigParseResult {
  return parseParticleConfigDocument(text, options);
}
export function registeredDocument(text: string, kind: string, options?: Readonly<ParseParticleConfigOptions>): ParticleConfigParseResult {
  return parseRegisteredParticleFormat(text, kind, options);
}
export function install(formats: readonly Readonly<ParticleFormatDescriptor>[] | undefined): void {
  applyParticleImportOptions({ formats });
}
export function installOptions(options: Readonly<ParticleImportOptions>): void {
  applyParticleImportOptions(options);
}
export function writeCallbackResult(text: string, kind: string, replacement: ParticleEmitterConfig): boolean {
  const codec = getParticleFormatCodec(kind);
  if (codec === null) return false;
  const original = codec.parseToDocument(text);
  const alias = original;
  alias.config = replacement;
  alias.diagnostics = [];
  return original === alias && original.config === replacement;
}
