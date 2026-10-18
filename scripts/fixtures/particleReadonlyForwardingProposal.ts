export interface Options { size?: number; kind?: number }
export interface Sink { parse(text:string, options?:Readonly<Options>):number }
export let sink:Sink = { parse: (_text: string, options?:Readonly<Options>) => 5 };
export function dispatch(text:string, options?:Options):number { return sink.parse(text, options); }
export function consume(text:string, options?:Readonly<Options>):number { return dispatch(text, options); }
