use azalea_core::{
    aabb::Aabb,
    direction::Direction,
    hit_result::{BlockHitResult, EntityHitResult, HitResult},
    position::Vec3,
};
use azalea_entity::{
    Attributes, Dead, LocalEntity, LookDirection, Physics, Position,
    dimensions::EntityDimensions,
    metadata::{
        AbstractArrow, AbstractBoat, AbstractLiving, AbstractMinecart, ArmorStand,
        ArmorStandMarker, EndCrystal, FallingBlock, InGround, Interaction, ShulkerBullet, Tnt,
    },
    view_vector,
};
use azalea_physics::{
    clip::{BlockShapeType, ClipContext, FluidPickType},
    collision::entity_collisions::{AabbQuery, get_entities},
    collision::OutlineContext,
};
use azalea_world::{Instance, InstanceContainer, InstanceName};
use bevy_ecs::prelude::*;
use derive_more::{Deref, DerefMut};

/// A component that contains the block or entity that the player is currently
/// looking at.
#[doc(alias("looking at", "looking at block", "crosshair"))]
#[derive(Clone, Component, Debug, Deref, DerefMut)]
pub struct HitResultComponent(HitResult);
impl From<HitResult> for HitResultComponent { fn from(value:HitResult)->Self{Self(value)} }

/// Optional native touch-camera ray. The origin offset is relative to the
/// authoritative eye; interaction reach is still measured from that eye.
#[derive(Clone, Copy, Component, Debug)]
pub struct NativePickRay {
    pub enabled: bool,
    pub offset: Vec3,
    pub look: LookDirection,
    pub revision: u64,
}
/// Written by the actual pick system, rather than by the event reader.
#[derive(Clone, Component, Debug)]
pub struct NativePickStamp {
    pub revision: u64,
    pub input_revision: u64,
    pub eye: Vec3,
    pub block_state: Option<u16>,
    pub attack_hit: Option<EntityHitResult>,
    pub outline_context: OutlineContext,
}

#[allow(clippy::type_complexity)]
pub fn update_hit_result_component(
    mut commands: Commands,
    mut query: Query<
        (
            Entity,
            Option<&mut HitResultComponent>,
            &Position,
            &EntityDimensions,
            &LookDirection,
            &InstanceName,
            &Physics,
            &Attributes,
            Option<&NativePickRay>,
            Option<&mut NativePickStamp>,
            Option<&azalea_entity::inventory::Inventory>,
            Option<&crate::local_player::LocalGameMode>,
        ),
        With<LocalEntity>,
    >,
    instance_container: Res<InstanceContainer>,
    aabb_query: AabbQuery,
    pickable_query: MaybePickableEntityQuery,
) {
    for (
        entity,
        hit_result_ref,
        position,
        dimensions,
        look_direction,
        world_name,
        physics,
        attributes,
        native_ray,
        stamp,
        inventory,
        game_mode,
    ) in &mut query
    {
        let block_pick_range = attributes.block_interaction_range.calculate();
        let entity_pick_range = attributes.entity_interaction_range.calculate();

        let eye_position = position.up(dimensions.eye_height.into());

        let Some(world_lock) = instance_container.get(world_name) else {
            continue;
        };
        let world = world_lock.read();

        let outline_context=inventory.map(|inv|OutlineContext::from_main(inv.held_item())).unwrap_or_default();
        let offset = native_ray.map_or(Vec3::ZERO, |r| r.offset);
        let ray_eye = eye_position + offset;
        let ray_bounds = physics.bounding_box.move_relative(offset);
        let ray_look = native_ray.map_or(*look_direction, |r| r.look);
        let hit_result = if native_ray.is_some_and(|r| !r.enabled) {
            HitResult::new_miss(eye_position, Direction::Down, eye_position.into())
        } else { pick(PickOpts {
            source_entity: entity,
            look_direction: ray_look,
            eye_position: ray_eye,
            aabb: &ray_bounds,
            world: &world,
            entity_pick_range: entity_pick_range + offset.length(),
            block_pick_range: block_pick_range + offset.length(),
            outline_context,
            aabb_query: &aabb_query,
            pickable_query: &pickable_query,
        }) };
        // A camera displaced by touch/third-person rendering must never extend
        // the server-authoritative player's interaction reach.
        let range = if matches!(&hit_result, HitResult::Entity(_)) { entity_pick_range } else { block_pick_range };
        let hit_result = filter_hit_result(hit_result, eye_position, range);
        // Weapon attack reach is a separate action target. Never replace the
        // ordinary HitResultComponent used by block/entity right-click actions.
        let attack_range = inventory.and_then(|inv| match inv.held_item() {
            azalea_inventory::ItemStack::Present(item) => item.get_component::<azalea_inventory::components::AttackRange>().map(|r| r.into_owned()), _=>None,
        });
        let attack_hit = attack_range.and_then(|range| {
            let creative=game_mode.is_some_and(|g|g.current==azalea_core::game_type::GameMode::Creative);
            let spectator=game_mode.is_some_and(|g|g.current==azalea_core::game_type::GameMode::Spectator);
            let min=if spectator{0.}else if creative{range.min_creative_reach}else{range.min_reach};
            let max=f64::from((if creative{range.max_creative_reach}else{range.max_reach})+range.hitbox_margin);
            let min=f64::from((min-range.hitbox_margin).max(0.));
            if !max.is_finite()||!min.is_finite()||max<min||max>65.||native_ray.is_some_and(|r|!r.enabled){return None;}
            let picked=pick(PickOpts {source_entity:entity,look_direction:ray_look,eye_position:ray_eye,aabb:&ray_bounds,world:&world,
                entity_pick_range:max+offset.length(),block_pick_range:block_pick_range+offset.length(),outline_context,aabb_query:&aabb_query,pickable_query:&pickable_query});
            if let HitResult::Entity(hit)=filter_hit_result(picked,eye_position,max) {
                // Match the action range's nearest-point-on-hitbox test;
                // right-click still uses the independent ordinary result.
                let bounds=aabb_query.get(hit.entity).ok()?.0;
                let nearest=Vec3::new(eye_position.x.clamp(bounds.min.x,bounds.max.x),eye_position.y.clamp(bounds.min.y,bounds.max.y),eye_position.z.clamp(bounds.min.z,bounds.max.z));
                let distance=nearest.distance_squared_to(eye_position);
                if distance>=min*min&&distance<=max*max {Some(hit)}else{None}
            }else{None}
        });
        let next = NativePickStamp {
            revision: stamp.as_ref().map_or(1, |s| s.revision.saturating_add(1)),
            input_revision: native_ray.map_or(0, |r| r.revision), eye: eye_position,
            attack_hit,outline_context,
            block_state: hit_result.as_block_hit_result_if_not_miss().and_then(|h|world.chunks.get_block_state(h.block_pos)).map(|s|s.id()),
        };
        if let Some(mut stamp) = stamp { *stamp = next; }
        else { commands.entity(entity).insert(next); }
        if let Some(mut hit_result_ref) = hit_result_ref {
            **hit_result_ref = hit_result;
        } else {
            commands
                .entity(entity)
                .insert(HitResultComponent(hit_result));
        }
    }
}

pub type MaybePickableEntityQuery<'world, 'state, 'a> = Query<
    'world,
    'state,
    (Option<&'a ArmorStandMarker>, Option<&'a InGround>),
    // search "isPickable" in the decompiled minecraft code
    (
        Or<(
            // TODO: ender dragon parts are pickable but the ender dragon itself isn't. this needs
            // more investigation.
            (With<Tnt>, Without<Dead>),
            (With<FallingBlock>, Without<Dead>),
            (With<AbstractMinecart>, Without<Dead>),
            (With<AbstractBoat>, Without<Dead>),
            With<ArmorStand>,
            With<EndCrystal>,
            With<Interaction>,
            With<ShulkerBullet>,
            (With<AbstractLiving>, Without<Dead>),
            With<AbstractArrow>,
        )>,
    ),
>;

pub struct PickOpts<'world, 'state, 'a, 'b, 'c> {
    source_entity: Entity,
    look_direction: LookDirection,
    eye_position: Vec3,
    aabb: &'a Aabb,
    world: &'a Instance,
    entity_pick_range: f64,
    block_pick_range: f64,
    outline_context: OutlineContext,
    aabb_query: &'a AabbQuery<'world, 'state, 'b>,
    pickable_query: &'a MaybePickableEntityQuery<'world, 'state, 'c>,
}

/// Get the block or entity that a player would be looking at if their eyes were
/// at the given direction and position.
///
/// If you need to get the block/entity the player is looking at right now, use
/// [`HitResultComponent`].
///
/// Also see [`pick_block`].
pub fn pick(opts: PickOpts<'_, '_, '_, '_, '_>) -> HitResult {
    // vanilla does extra math here to calculate the pick result in between ticks by
    // interpolating, but since clients can still only interact on exact ticks, that
    // isn't relevant for us.

    let mut max_range = opts.entity_pick_range.max(opts.block_pick_range);
    let mut max_range_squared = max_range.powi(2);

    let block_hit_result = pick_block_with_context(
        opts.look_direction,
        opts.eye_position,
        &opts.world.chunks,
        max_range,opts.outline_context,
    );
    let block_hit_result_dist_squared = block_hit_result
        .location
        .distance_squared_to(opts.eye_position);
    if !block_hit_result.miss {
        max_range_squared = block_hit_result_dist_squared;
        max_range = block_hit_result_dist_squared.sqrt();
    }

    let view_vector = view_vector(opts.look_direction);
    let end_position = opts.eye_position + (view_vector * max_range);
    let inflate_by = 1.;
    let pick_aabb = opts
        .aabb
        .expand_towards(view_vector * max_range)
        .inflate_all(inflate_by);

    let is_pickable = |entity: Entity| {
        if entity == opts.source_entity {
            return false;
        }

        // TODO: ender dragon has extra logic here. also, we shouldn't be able to pick
        // spectators.
        if let Ok((armor_stand_marker, arrow_in_ground)) = opts.pickable_query.get(entity) {
            !(armor_stand_marker == Some(&ArmorStandMarker(true))
                || arrow_in_ground == Some(&InGround(true)))
        } else {
            false
        }
    };
    let entity_hit_result = pick_entity(PickEntityOpts {
        source_entity: opts.source_entity,
        eye_position: opts.eye_position,
        end_position,
        world: opts.world,
        pick_range_squared: max_range_squared,
        predicate: &is_pickable,
        aabb: &pick_aabb,
        aabb_query: opts.aabb_query,
    });

    if let Some(entity_hit_result) = entity_hit_result
        && entity_hit_result
            .location
            .distance_squared_to(opts.eye_position)
            < block_hit_result_dist_squared
    {
        filter_hit_result(
            HitResult::Entity(entity_hit_result),
            opts.eye_position,
            opts.entity_pick_range,
        )
    } else {
        filter_hit_result(
            HitResult::Block(block_hit_result),
            opts.eye_position,
            opts.block_pick_range,
        )
    }
}

fn filter_hit_result(hit_result: HitResult, eye_position: Vec3, range: f64) -> HitResult {
    let location = hit_result.location();
    if !location.closer_than(eye_position, range) {
        let direction = Direction::nearest(location - eye_position);
        HitResult::new_miss(location, direction, location.into())
    } else {
        hit_result
    }
}

/// Get the block that a player would be looking at if their eyes were at the
/// given direction and position.
///
/// This does not consider entities.
///
/// Also see [`pick`].
pub fn pick_block(
    look_direction: LookDirection,
    eye_position: Vec3,
    chunks: &azalea_world::ChunkStorage,
    pick_range: f64,
) -> BlockHitResult {
    pick_block_with_context(look_direction,eye_position,chunks,pick_range,Default::default())
}
pub fn pick_block_with_context(look_direction:LookDirection,eye_position:Vec3,chunks:&azalea_world::ChunkStorage,pick_range:f64,outline_context:OutlineContext)->BlockHitResult{
    let view_vector = view_vector(look_direction);
    let end_position = eye_position + (view_vector * pick_range);

    azalea_physics::clip::clip_with_outline_context(
        chunks,
        ClipContext {
            from: eye_position,
            to: end_position,
            block_shape_type: BlockShapeType::Outline,
            fluid_pick_type: FluidPickType::None,
        },outline_context,
    )
}

struct PickEntityOpts<'world, 'state, 'a, 'b> {
    source_entity: Entity,
    eye_position: Vec3,
    end_position: Vec3,
    world: &'a azalea_world::Instance,
    pick_range_squared: f64,
    predicate: &'a dyn Fn(Entity) -> bool,
    aabb: &'a Aabb,
    aabb_query: &'a AabbQuery<'world, 'state, 'b>,
}

// port of getEntityHitResult
fn pick_entity(opts: PickEntityOpts) -> Option<EntityHitResult> {
    let mut picked_distance_squared = opts.pick_range_squared;
    let mut result = None;

    for (candidate, candidate_aabb) in get_entities(
        opts.world,
        Some(opts.source_entity),
        opts.aabb,
        opts.predicate,
        opts.aabb_query,
    ) {
        // TODO: if the entity is "REDIRECTABLE_PROJECTILE" then this should be 1.0.
        // azalea needs support for entity tags first for this to be possible. see
        // getPickRadius in decompiled minecraft source
        let candidate_pick_radius = 0.;
        let candidate_aabb = candidate_aabb.inflate_all(candidate_pick_radius);
        let clip_location = candidate_aabb.clip(opts.eye_position, opts.end_position);

        if candidate_aabb.contains(opts.eye_position) {
            if picked_distance_squared >= 0. {
                result = Some(EntityHitResult {
                    location: clip_location.unwrap_or(opts.eye_position),
                    entity: candidate,
                });
                picked_distance_squared = 0.;
            }
        } else if let Some(clip_location) = clip_location {
            let distance_squared = opts.eye_position.distance_squared_to(clip_location);
            if distance_squared < picked_distance_squared || picked_distance_squared == 0. {
                // TODO: don't pick the entity we're riding on
                // if candidate_root_vehicle == entity_root_vehicle {
                //     if picked_distance_squared == 0. {
                //         picked_entity = Some(candidate);
                //         picked_location = Some(clip_location);
                //     }
                // } else {
                result = Some(EntityHitResult {
                    location: clip_location,
                    entity: candidate,
                });
                picked_distance_squared = distance_squared;
            }
        }
    }

    result
}
