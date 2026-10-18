// Assignment-only source witness for the two original libGDX functions.
// Virtual source location: packages/particles-formats/src/particleCodecLibgdxCallbacks.ts.
// The original detector and full registration remain outside this witness.
import type{ParticleFormatCodec}from '@flighthq/types/contract';
import{parseLibgdxParticle,parseLibgdxParticleDocument}from './libgdxParse.ts';
export const callbacks:Pick<ParticleFormatCodec,'parseToConfig'|'parseToDocument'>={parseToConfig:parseLibgdxParticle,parseToDocument:parseLibgdxParticleDocument};
export function originalConfig():boolean{return callbacks.parseToConfig===parseLibgdxParticle}export function originalDocument():boolean{return callbacks.parseToDocument===parseLibgdxParticleDocument}
