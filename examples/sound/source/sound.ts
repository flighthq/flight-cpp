// These are the procedural PCM calculations from Flight's sound example. The browser entry point
// wraps each result in AudioResource; the native entry point gives the same samples to host-sdl.

export function generateToneSamples(
  frequency: number,
  duration: number,
  decay: number,
  sampleRate: number,
): number[] {
  const length = Math.floor(sampleRate * duration);
  const samples: number[] = [];
  for (let i = 0; i < length; i++) {
    const t = i / sampleRate;
    const envelope = Math.exp(-decay * t);
    samples.push(Math.sin(2 * Math.PI * frequency * t) * envelope);
  }
  return samples;
}

export function generateSweepSamples(
  startFrequency: number,
  endFrequency: number,
  duration: number,
  decay: number,
  sampleRate: number,
): number[] {
  const length = Math.floor(sampleRate * duration);
  const samples: number[] = [];
  let phase = 0;
  for (let i = 0; i < length; i++) {
    const t = i / sampleRate;
    const frequency = startFrequency + (endFrequency - startFrequency) * (t / duration);
    const envelope = Math.exp(-decay * t);
    samples.push(Math.sin(phase) * envelope);
    phase += (2 * Math.PI * frequency) / sampleRate;
  }
  return samples;
}
