// Actual public slot and original parsers; no local parser/detector/public declaration.
import type { ParticleFormatCodec } from '../../types/src/ParticleFormatCodec.ts';
import { parsePixiParticleDocument } from './pixiParse.ts';
import { parseSpineParticleDocument } from './spineParse.ts';
export const namedPixiDocument: Pick<ParticleFormatCodec, 'parseToDocument'> = { parseToDocument: parsePixiParticleDocument };
export const namedSpineDocument: Pick<ParticleFormatCodec, 'parseToDocument'> = { parseToDocument: parseSpineParticleDocument };
export function originalNamedIdentity(): boolean {
  return namedPixiDocument.parseToDocument === parsePixiParticleDocument && namedSpineDocument.parseToDocument === parseSpineParticleDocument;
}
