function easeInCubic(t: number): number {
  return t * t * t;
}

function easeInElastic(t: number): number {
  if (t === 0 || t === 1) return t;
  const period = 0.4;
  const shift = period / 4;
  t -= 1;
  return -(Math.pow(2, 10 * t) * Math.sin(((t - shift) * (2 * Math.PI)) / period));
}

function easeInExponential(t: number): number {
  return t === 0 ? 0 : Math.pow(2, 10 * t - 10);
}

function easeInOutCubic(t: number): number {
  return t < 0.5 ? 4 * t * t * t : 1 - Math.pow(-2 * t + 2, 3) / 2;
}

function easeInOutExponential(t: number): number {
  if (t === 0 || t === 1) return t;
  return t < 0.5 ? Math.pow(2, 20 * t - 10) / 2 : (2 - Math.pow(2, -20 * t + 10)) / 2;
}

function easeInOutQuadratic(t: number): number {
  return t < 0.5 ? 2 * t * t : 1 - Math.pow(-2 * t + 2, 2) / 2;
}

function easeInOutSine(t: number): number {
  return -(Math.cos(Math.PI * t) - 1) / 2;
}

function easeInQuadratic(t: number): number {
  return t * t;
}

function easeInSine(t: number): number {
  return 1 - Math.cos((t * Math.PI) / 2);
}

function easeOutBounce(t: number): number {
  if (t < 1 / 2.75) return 7.5625 * t * t;
  if (t < 2 / 2.75) {
    t -= 1.5 / 2.75;
    return 7.5625 * t * t + 0.75;
  }
  if (t < 2.5 / 2.75) {
    t -= 2.25 / 2.75;
    return 7.5625 * t * t + 0.9375;
  }
  t -= 2.625 / 2.75;
  return 7.5625 * t * t + 0.984375;
}

function easeOutCubic(t: number): number {
  return 1 - Math.pow(1 - t, 3);
}

function easeOutElastic(t: number): number {
  if (t === 0 || t === 1) return t;
  const period = 0.4;
  const shift = period / 4;
  return Math.pow(2, -10 * t) * Math.sin(((t - shift) * (2 * Math.PI)) / period) + 1;
}

function easeOutExponential(t: number): number {
  return t === 1 ? 1 : 1 - Math.pow(2, -10 * t);
}

function easeOutQuadratic(t: number): number {
  return t * (2 - t);
}

function easeOutSine(t: number): number {
  return Math.sin((t * Math.PI) / 2);
}

export function sampleTweenCurves(t: number): number[] {
  return [
    easeInQuadratic(t),
    easeOutQuadratic(t),
    easeInOutQuadratic(t),
    easeInCubic(t),
    easeOutCubic(t),
    easeInOutCubic(t),
    easeInSine(t),
    easeOutSine(t),
    easeInOutSine(t),
    easeInExponential(t),
    easeOutExponential(t),
    easeInOutExponential(t),
    easeInElastic(t),
    easeOutElastic(t),
    easeOutBounce(t),
  ];
}
