// A strict source type witness, not an alternate parser implementation.
interface StarlingOptions { textureSize?: number }
interface ParseOptions extends StarlingOptions { pixelsPerUnit?: number }
interface Config { scale: number }
interface Document { startSize: number }
interface Result { config: Config; diagnostics: string[]; document: Document }
interface Codec {
  parseToConfig(text: string, options?: Readonly<ParseOptions>): Config;
  parseToDocument(text: string, options?: Readonly<ParseOptions>): { config: Config; diagnostics: string[] };
}
declare function parse(text: string, options?: StarlingOptions): Config;
declare function parseDocument(text: string, options?: StarlingOptions): Result;
const codec: Codec = { parseToConfig: parse, parseToDocument: parseDocument };
const literal: Readonly<ParseOptions> = { textureSize: 2, pixelsPerUnit: 10 };
codec.parseToConfig('xml', literal);
codec.parseToConfig('xml', {});
codec.parseToConfig('xml');
const sameConfigFunction: boolean = codec.parseToConfig === parse;
const sameDocumentFunction: boolean = codec.parseToDocument === parseDocument;
const fullResult = codec.parseToDocument('xml', literal) as Result;
const retainedDocument: Document = fullResult.document;
void sameConfigFunction;
void sameDocumentFunction;
void retainedDocument;

export {};

// Required is the identity on this already-required result shape; writes remain permitted.
type PlainResult = { config: Config; diagnostics: string[] };
type ViewResult = Required<PlainResult>;
declare const plainResult: PlainResult;
declare const viewResult: ViewResult;
const asView: ViewResult = plainResult;
const asPlain: PlainResult = viewResult;
asView.config = asPlain.config;
asPlain.diagnostics = asView.diagnostics;
