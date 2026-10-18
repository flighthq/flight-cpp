// Synthetic result-owner controls; actual original module emission is qualified separately.
export interface Config { size: number }
export interface Diagnostic { code: number }
export interface Document { size: number }
export interface WideResult { config: Config; diagnostics: Diagnostic[]; document: Document }
export interface SmallResult { config: Config; diagnostics: Diagnostic[] }
export interface Options { size?: number; width?: number }
export interface Codec {
  parse(text: string, options?: Readonly<Options>): Required<{ config: Config; diagnostics: Diagnostic[] }>;
}
export let wideProvider: (text: string) => WideResult = text => ({ config: { size: 1 }, diagnostics: [], document: { size: 2 } });
export let smallProvider: (text: string) => SmallResult = text => ({ config: { size: 3 }, diagnostics: [] });
export function parseWide(text: string): WideResult { return wideProvider(text); }
export function parseSmall(text: string): SmallResult { return smallProvider(text); }
export const wideCodec: Codec = { parse: parseWide };
export const smallCodec: Codec = { parse: parseSmall };
export function sameWide(): boolean { return wideCodec.parse === parseWide; }
export function distinct(): boolean { return wideCodec.parse === parseSmall; }
export function writeConfig(text: string, config: Config): void {
  const result = wideCodec.parse(text);
  result.config = config;
  result.diagnostics = [];
}
