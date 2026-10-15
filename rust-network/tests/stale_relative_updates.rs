use azalea_client::{
    local_player::InstanceHolder,
    packet::game::{handle_stale_relative_update, process_packet},
    test_utils::prelude::*,
};
use azalea_core::position::{ChunkPos, Vec3};
use azalea_entity::{indexing::EntityIdIndex, LookDirection, Position, RelativeEntityUpdate};
use azalea_protocol::packets::{
    game::{ClientboundMoveEntityRot, ClientboundRemoveEntities},
    ConnectionProtocol, Packet,
};
use azalea_registry::builtin::EntityKind;
use azalea_world::MinecraftEntityId;
use bevy_ecs::{prelude::*, world::CommandQueue};
use std::sync::{
    atomic::{AtomicUsize, Ordering},
    Arc,
};
fn simulation() -> Simulation {
    let mut sim = Simulation::new(ConnectionProtocol::Game);
    sim.receive_packet(default_login_packet());
    sim.receive_packet(make_basic_empty_chunk(ChunkPos::new(0, 0), (384 + 64) / 16));
    sim.tick();
    sim
}
#[test]
fn queued_old_generation_update_does_not_mutate_replacement() {
    let mut sim = simulation();
    let partial = sim.component::<InstanceHolder>().partial_instance;
    let world = sim.app.world_mut();
    let old = world
        .spawn((MinecraftEntityId(123), Position::new(Vec3::ZERO)))
        .id();
    let ran = Arc::new(AtomicUsize::new(0));
    let captured = ran.clone();
    let mut queue = CommandQueue::default();
    {
        let mut commands = Commands::new(&mut queue, world);
        commands.entity(old).queue_handled(
            RelativeEntityUpdate::new(partial, move |entity| {
                captured.fetch_add(1, Ordering::SeqCst);
                entity.insert(Position::new(Vec3::new(50., 64., 50.)));
            }),
            handle_stale_relative_update,
        );
    }
    assert!(world.despawn(old));
    let replacement = world
        .spawn((MinecraftEntityId(123), Position::new(Vec3::new(1., 2., 3.))))
        .id();
    assert_eq!(replacement.index(), old.index());
    assert_ne!(replacement, old);
    queue.apply(world);
    assert_eq!(ran.load(Ordering::SeqCst), 0);
    assert_eq!(
        **world.get::<Position>(replacement).unwrap(),
        Vec3::new(1., 2., 3.)
    );
}
#[test]
fn production_rotation_packet_with_retired_index_does_not_panic() {
    let mut sim = simulation();
    let world = sim.app.world_mut();
    let old = world
        .spawn((MinecraftEntityId(123), LookDirection::default()))
        .id();
    world
        .get_mut::<EntityIdIndex>(sim.entity)
        .unwrap()
        .insert(MinecraftEntityId(123), old);
    assert!(world.despawn(old));
    let replacement = world
        .spawn((MinecraftEntityId(123), LookDirection::default()))
        .id();
    assert_eq!(old.index(), replacement.index());
    let packet = ClientboundMoveEntityRot {
        entity_id: MinecraftEntityId(123),
        y_rot: 100,
        x_rot: 50,
        on_ground: false,
    }
    .into_variant();
    process_packet(world, sim.entity, &packet);
    assert_eq!(world.get::<LookDirection>(replacement).unwrap().y_rot(), 0.);
}
#[test]
fn packets_move_remove_and_world_transition_preserve_runner() {
    let mut sim = simulation();
    sim.receive_packet(make_basic_add_entity(EntityKind::Cow, 123, (0.5, 64., 0.5)));
    sim.tick();
    sim.receive_packet(ClientboundMoveEntityRot {
        entity_id: MinecraftEntityId(123),
        y_rot: 100,
        x_rot: 50,
        on_ground: false,
    });
    sim.receive_packet(ClientboundRemoveEntities {
        entity_ids: vec![MinecraftEntityId(123)],
    });
    let login = default_login_packet();
    sim.receive_packet(make_basic_respawn_packet(
        login.common.dimension_type,
        login.common.dimension,
    ));
    sim.tick();
    sim.tick();
    assert!(sim.has_component::<InstanceHolder>());
    sim.receive_packet(make_basic_empty_chunk(ChunkPos::new(0, 0), (384 + 64) / 16));
    sim.receive_packet(make_basic_add_entity(EntityKind::Cow, 123, (0.5, 64., 0.5)));
    sim.tick();
    let mut query = sim.app.world_mut().query::<&MinecraftEntityId>();
    assert!(query.iter(sim.app.world()).any(|id| id.0 == 123));
}
#[test]
#[should_panic]
fn errors_other_than_retired_generation_are_not_suppressed() {
    use bevy_ecs::{error::ErrorContext, world::error::EntityMutableFetchError};
    handle_stale_relative_update(
        EntityMutableFetchError::AliasedMutability(Entity::PLACEHOLDER).into(),
        ErrorContext::Command {
            name: "unexpected alias error".into(),
        },
    );
}
