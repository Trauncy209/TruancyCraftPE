use azalea_client::test_utils::prelude::*;
use azalea_core::position::{Vec3,ChunkPos};
use azalea_entity::{Physics,Position,LookDirection,Pose,OnClimbable,ActiveEffects,inventory::Inventory,metadata::FallFlying,MobEffectData};
use azalea_inventory::{ItemStack,components::{EquipmentSlot,DataComponentUnion,Damage}};
use azalea_registry::builtin::{ItemKind,MobEffect,DataComponentKind};
use azalea_physics::elytra;
use azalea_protocol::packets::{ConnectionProtocol,game::{ClientboundSetPassengers,ClientboundRemoveEntities}};
use azalea_world::MinecraftEntityId;

fn simulation()->Simulation{
 let mut s=Simulation::new(ConnectionProtocol::Game);s.receive_packet(default_login_packet());s.receive_packet(make_basic_empty_chunk(ChunkPos::new(0,0),24));s.tick();
 s.with_component_mut::<Position>(|p|**p=Vec3::new(8.,80.,8.));s.with_component_mut::<Physics>(|p|{p.set_on_ground(false);p.velocity=Vec3::new(0.,0.,1.);});
 s.with_component_mut::<Inventory>(|inv|*inv.inventory_menu.slot_mut(6).unwrap()=ItemStack::new(ItemKind::Elytra,1));s.app.update();s
}
#[test]fn exact_level_glide_vector_and_real_loaded_world_travel_pose(){
 let velocity=elytra::flight_velocity(Vec3::new(0.,0.,1.),LookDirection::new(0.,0.),0.08);
 assert_eq!(velocity.x,0.);assert!((velocity.y-(-0.018*0.9800000190734863)).abs()<1e-14);assert!((velocity.z-(1.0018*0.9900000095367432)).abs()<1e-13);
 let mut s=simulation();assert!(elytra::can_start_fall_flying(s.app.world(),s.entity));s.app.world_mut().entity_mut(s.entity).insert(FallFlying(true));let before=s.component::<Position>();s.tick();let p=s.component::<Position>();let actual=s.component::<Physics>().velocity;
 assert!((p.y-(before.y+velocity.y)).abs()<1e-10);assert!((p.z-(before.z+velocity.z)).abs()<1e-10);assert!((actual.z-velocity.z).abs()<1e-10);assert_eq!(s.component::<Pose>(),Pose::FallFlying);
}
#[test]fn equipment_components_damage_and_removal_determine_eligibility(){
 let mut item=ItemStack::new(ItemKind::Elytra,1);assert!(elytra::can_glide_using(&item,EquipmentSlot::Chest));assert!(!elytra::can_glide_using(&item,EquipmentSlot::Mainhand));
 if let ItemStack::Present(i)=&mut item{unsafe{i.component_patch.unchecked_insert_component(DataComponentKind::Damage,Some(DataComponentUnion::from(Damage{amount:431})));}}
 assert!(!elytra::can_glide_using(&item,EquipmentSlot::Chest));
 if let ItemStack::Present(i)=&mut item{unsafe{i.component_patch.unchecked_insert_component(DataComponentKind::Glider,None);}}
 assert!(!elytra::can_glide_using(&item,EquipmentSlot::Chest));
}
#[test]fn real_passenger_packets_dismount_and_vehicle_removal_gate_start(){
 let mut s=simulation();let id=s.minecraft_entity_id();let vehicle=MinecraftEntityId(99);s.receive_packet(ClientboundSetPassengers{vehicle,passengers:vec![id]});s.update();assert!(!elytra::can_start_fall_flying(s.app.world(),s.entity));
 s.receive_packet(ClientboundSetPassengers{vehicle,passengers:vec![]});s.update();assert!(elytra::can_start_fall_flying(s.app.world(),s.entity));
 s.receive_packet(ClientboundSetPassengers{vehicle,passengers:vec![id]});s.update();s.receive_packet(ClientboundRemoveEntities{entity_ids:vec![vehicle]});s.update();assert!(elytra::can_start_fall_flying(s.app.world(),s.entity));
}
#[test]fn levitation_ground_climb_and_equipment_loss_stop_gliding_and_slowfall_is_bounded(){
 let mut s=simulation();s.with_component_mut::<ActiveEffects>(|e|e.insert(MobEffect::Levitation,MobEffectData::default()));assert!(!elytra::can_start_fall_flying(s.app.world(),s.entity));s.app.world_mut().entity_mut(s.entity).insert(FallFlying(true));s.tick();assert!(!s.component::<FallFlying>().0);
 s.with_component_mut::<ActiveEffects>(|e|{e.remove(MobEffect::Levitation);});s.with_component_mut::<OnClimbable>(|v|**v=true);assert!(!elytra::can_start_fall_flying(s.app.world(),s.entity));s.with_component_mut::<OnClimbable>(|v|**v=false);s.with_component_mut::<Physics>(|p|p.set_on_ground(true));assert!(!elytra::can_start_fall_flying(s.app.world(),s.entity));
 s.with_component_mut::<Physics>(|p|p.set_on_ground(false));s.app.world_mut().entity_mut(s.entity).insert(FallFlying(true));s.with_component_mut::<Inventory>(|i|*i.inventory_menu.slot_mut(6).unwrap()=ItemStack::Empty);s.tick();assert!(!s.component::<FallFlying>().0);
 let mut effects=ActiveEffects::default();effects.insert(MobEffect::SlowFalling,MobEffectData::default());assert_eq!(elytra::effective_gravity(0.08,-1.,Some(&effects)),0.01);assert_eq!(elytra::effective_gravity(0.005,-1.,Some(&effects)),0.005);assert_eq!(elytra::effective_gravity(0.08,1.,Some(&effects)),0.08);
}
