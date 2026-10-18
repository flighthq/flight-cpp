// Exact original detector expression; a source/refusal control, not a replacement descriptor.
export const detect = (text: string) => text.trimStart().split('\n')[0]?.trim() === 'Particle Effect';
