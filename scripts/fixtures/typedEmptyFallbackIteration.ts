// Native compiler boundary controls; the production particle body stays untouched.
export interface Item { size: number }
export interface Options { formats?: readonly Item[] }
export interface NullableOptions { formats?: readonly Item[] | null }
export function visit(options: Readonly<Options>, use: (item: Item) => void): void {
  for (const item of options.formats ?? []) use(item);
}
export function visitNullable(options: Readonly<NullableOptions>, use: (item: Item) => void): void {
  for (const item of options.formats ?? []) use(item);
}
export function visitOptional(options: Readonly<Options> | undefined, use: (item: Item) => void): void {
  for (const item of options?.formats ?? []) use(item);
}
export function visitGuarded(options: Readonly<Options> | undefined, use: (item: Item) => void): void {
  if (options !== undefined) for (const item of options.formats ?? []) use(item);
}
export function visitSource(read: () => readonly Item[] | undefined, use: (item: Item) => void): void {
  for (const item of read() ?? []) use(item);
}
export function control(options: Readonly<Options>, use: (item: Item) => void): void {
  for (const item of options.formats ?? []) {
    if (item.size === 1) continue;
    use(item);
    if (item.size === 3) break;
  }
}
