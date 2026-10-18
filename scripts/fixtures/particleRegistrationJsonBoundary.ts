// Minimal private-helper counterexample, not a registry or parser replacement.
function parseJsonObject(text: string): Record<string, unknown> | null {
  try {
    const value: unknown = JSON.parse(text);
    return value !== null && typeof value === 'object' && !Array.isArray(value)
      ? (value as Record<string, unknown>)
      : null;
  } catch {
    return null;
  }
}

export function acceptsObject(text: string): boolean {
  return parseJsonObject(text) !== null;
}
