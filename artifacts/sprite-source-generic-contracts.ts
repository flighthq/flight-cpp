// Diagnostic-only callers of unchanged frozen declarations. Never a replacement Sprite/node body.
// Run via scripts/spriteSourceGenericContractProof.mjs; expected refusals are intentionally unsuppressed.
import { EntityRuntimeKey } from '@flighthq/types/contract';
import type { Entity, EntityConstruction, Node2D, Node2DData, Node2DRuntime, NodeInitialization, PartialNode, NodeDataFactory, Sprite, SpriteData, SpriteRuntime, BoundsNode, Node2DTraits, Rectangle, MethodsOf } from '@flighthq/types/contract';
import { finishEntity } from '@flighthq/entity/contract';
interface OtherData extends Node2DData {
    label: string;
}
type Model = Node2D;
type Derived = Sprite;
type BoundsRuntime = Pick<Node2DRuntime, 'computeLocalBoundsRectangle'>;
interface SpriteRuntimeRunner {
    computeLocalBoundsRectangle(out: Rectangle, source: Readonly<Sprite>): void;
    isLocalBoundsRectangleValid(source: Readonly<Sprite>): boolean;
}
export namespace nullable_forward {
    declare function initialize<D extends Node2DData>(obj?: Readonly<NodeInitialization<D>>): void;
    export function forward(obj?: Readonly<PartialNode<Node2D>>): void { initialize<Node2DData>(obj); }
}
export namespace independent_N_D {
    declare function initialize<D extends Node2DData>(obj?: Readonly<NodeInitialization<D>>): void;
    export function forward<N extends Node2D, D extends Node2DData>(obj?: Readonly<PartialNode<N>>): void { initialize<D>(obj); }
}
export namespace present_same_family {
    declare function initialize<D extends Node2DData>(obj?: Readonly<NodeInitialization<D>>): void;
    export function forward(obj?: Readonly<PartialNode<Sprite>>): void { initialize<SpriteData>(obj); }
}
export namespace admitted_mismatched_families {
    declare function factory<N extends Node2D, D extends Node2DData>(obj: Readonly<PartialNode<N>>, createData: NodeDataFactory<D>): N;
    declare const obj: Readonly<PartialNode<Sprite>>, other: NodeDataFactory<OtherData>;
    export function admitted(): Sprite { return factory<Sprite, OtherData>(obj, other); }
}
export namespace null_input_admitted {
    declare function accepts(obj?: Readonly<PartialNode<Node2D>>): void;
    export function admitted(): void { accepts({ data: null }); }
}
export namespace null_factory_input_rejected {
    declare const factory: NodeDataFactory<SpriteData>;
    export function rejected(): SpriteData { return factory(null); }
}
export namespace sprite_callback_slot {
    declare function install(methods: Readonly<Partial<MethodsOf<BoundsRuntime>>>): void;
    declare const runner: SpriteRuntimeRunner;
    export function register(): void { install(runner); }
}
export namespace source_signature_wide_control {
    declare function install(methods: Readonly<Partial<MethodsOf<BoundsRuntime>>>): void;
    export function wide(_out: Rectangle, _source: Readonly<BoundsNode<Node2DTraits>>): void { }
    export function register(): void { install({ computeLocalBoundsRectangle: wide }); }
}
export namespace method_syntax_still_rejected_with_full_runtime {
    interface MethodSlot {
        computeLocalBoundsRectangle(out: Rectangle, source: Readonly<BoundsNode<Node2DTraits>>): void;
    }
    declare function install(methods: Readonly<Partial<MethodSlot>>): void;
    declare const runner: SpriteRuntimeRunner;
    export function register(): void { install(runner); }
}
export namespace generic_finish {
    export function finishGeneric<N extends Model>(out: EntityConstruction<N>): N { return finishEntity(out); }
}
export namespace generic_finish_explicit_control {
    export function finishGeneric<N extends Model>(out: EntityConstruction<N>): N { return finishEntity<N>(out); }
}
export namespace derived_finish_widening_control {
    export function finishBase(out: EntityConstruction<Derived>): Model { return finishEntity(out); }
}
export namespace sprite_to_bounds_full_runtime {
    declare const sprite: Readonly<Sprite>;
    export const bounds: Readonly<BoundsNode<Node2DTraits>> = sprite;
}
export namespace bounds_to_sprite_nullable_data {
    declare const bounds: Readonly<BoundsNode<Node2DTraits>>;
    export const sprite: Readonly<Sprite> = bounds;
}
export namespace sprite_to_bounds_data_only_control {
    declare const sprite: Readonly<Sprite>;
    export const bounds: Readonly<Pick<BoundsNode<Node2DTraits>, 'data'>> = sprite;
}
export namespace sprite_to_bounds_without_runtime_control {
    declare const sprite: Readonly<Sprite>;
    export const bounds: Readonly<Omit<BoundsNode<Node2DTraits>, typeof EntityRuntimeKey>> = sprite;
}
export namespace parameterizing_methods_by_R_is_insufficient {
    declare function install<R extends Node2DRuntime>(methods: Readonly<Partial<MethodsOf<R>>>): void;
    declare const runner: SpriteRuntimeRunner;
    export function register(): void { install<SpriteRuntime>(runner); }
}
export namespace no_factory_cannot_establish_sprite_data {
    declare function factory<N extends Node2D, D extends Node2DData>(obj?: Readonly<PartialNode<N>>, createData?: NodeDataFactory<D>): N;
    export function admitted(): Sprite { return factory<Sprite, SpriteData>(); }
}
