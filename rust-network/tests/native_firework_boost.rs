use azalea_client::movement::native_fireworks::boost;
use azalea_entity::{HasClientLoaded,LocalEntity,LookDirection,Physics,metadata::{AttachedToTarget,FallFlying,FireworkRocket,Health}};
use azalea_physics::elytra::glide_view_vector as view_vector;
use azalea_world::{InstanceName,MinecraftEntityId};
use bevy_ecs::prelude::*;use bevy_ecs::system::RunSystemOnce;use azalea_core::position::Vec3;use azalea_entity::OptionalUnsignedInt;
 fn name(s:&str)->InstanceName{InstanceName(s.parse().unwrap())}
 #[test]fn actual_attached_rocket_boosts_only_live_gliding_target_and_stops_on_removal(){
  let mut world=World::new();let p=world.spawn((LocalEntity,HasClientLoaded,MinecraftEntityId(42),name("minecraft:overworld"),LookDirection::new(0.,0.),FallFlying(true),Health(20.),Physics::default())).id();
  world.entity_mut(p).get_mut::<Physics>().unwrap().velocity=Vec3::new(0.2,-0.1,0.3);
  let rocket=world.spawn((FireworkRocket,AttachedToTarget(OptionalUnsignedInt(Some(42))),name("minecraft:overworld"))).id();
  world.run_system_once(boost).unwrap();let v=world.get::<Physics>(p).unwrap().velocity;assert!((v.x-0.1).abs()<1e-7&&(v.y+0.05).abs()<1e-7&&(v.z-0.9999999999999999).abs()<1e-7);
  world.despawn(rocket);world.run_system_once(boost).unwrap();assert_eq!(world.get::<Physics>(p).unwrap().velocity,v);
  world.spawn((FireworkRocket,AttachedToTarget(OptionalUnsignedInt(Some(41))),name("minecraft:overworld")));world.spawn((FireworkRocket,AttachedToTarget(OptionalUnsignedInt(Some(42))),name("minecraft:the_nether")));world.run_system_once(boost).unwrap();assert_eq!(world.get::<Physics>(p).unwrap().velocity,v);
  world.spawn((FireworkRocket,AttachedToTarget(OptionalUnsignedInt(Some(42))),name("minecraft:overworld")));world.entity_mut(p).insert(FallFlying(false));world.run_system_once(boost).unwrap();assert_eq!(world.get::<Physics>(p).unwrap().velocity,v);
  world.entity_mut(p).insert((FallFlying(true),Health(0.)));world.run_system_once(boost).unwrap();assert_eq!(world.get::<Physics>(p).unwrap().velocity,v);
  world.entity_mut(p).insert(Health(20.));world.entity_mut(p).remove::<HasClientLoaded>();world.run_system_once(boost).unwrap();assert_eq!(world.get::<Physics>(p).unwrap().velocity,v);
 }
 #[test]fn simultaneous_rockets_apply_each_official_impulse_without_camera_mutation(){let mut world=World::new();let look=LookDirection::new(90.,-30.);let p=world.spawn((LocalEntity,HasClientLoaded,MinecraftEntityId(3),name("minecraft:overworld"),look,FallFlying(true),Health(20.),Physics::default())).id();for _ in 0..2{world.spawn((FireworkRocket,AttachedToTarget(OptionalUnsignedInt(Some(3))),name("minecraft:overworld")));}world.run_system_once(boost).unwrap();let expected=view_vector(look)*1.275;let v=world.get::<Physics>(p).unwrap().velocity;assert!((v.x-expected.x).abs()<1e-7&&(v.y-expected.y).abs()<1e-7&&(v.z-expected.z).abs()<1e-7);assert_eq!(*world.get::<LookDirection>(p).unwrap(),look);}
