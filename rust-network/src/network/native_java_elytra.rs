//! Original TCPE jump-edge input mapped to Java's StartFallFlying command.
use bevy_ecs::prelude::*;
use azalea_entity::{Jumping,HasClientLoaded,metadata::FallFlying};
use azalea_world::MinecraftEntityId;
/// Called before the movement command updates Jumping. Holding jump cannot
/// repeatedly request gliding, and normal grounded jump is left to physics.
pub(super) fn jump_edge(world:&mut World,entity:Entity,jump:bool)->bool{
 if !jump||world.get::<Jumping>(entity).is_none_or(|v|v.0)||world.get::<HasClientLoaded>(entity).is_none()||!azalea_physics::elytra::can_start_fall_flying(world,entity){return false;}
 let Some(id)=world.get::<MinecraftEntityId>(entity).copied()else{return false;};
 world.entity_mut(entity).insert(FallFlying(true));
 world.trigger(azalea_client::packet::game::SendGamePacketEvent::new(entity,azalea_protocol::packets::game::ServerboundPlayerCommand{id,action:azalea_protocol::packets::game::s_player_command::Action::StartFallFlying,data:0}));true
}
#[cfg(test)]mod tests{
 use super::*;use azalea_client::{test_utils::prelude::*,packet::game::SendGamePacketEvent};use azalea_protocol::packets::{ConnectionProtocol,game::{ServerboundGamePacket,s_player_command::Action}};use azalea_core::position::{ChunkPos,Vec3};use azalea_entity::{Physics,Position,inventory::Inventory};use azalea_inventory::ItemStack;use azalea_registry::builtin::ItemKind;use std::sync::{Arc,Mutex};
 #[test]fn actual_walk_jump_release_jump_sends_one_start_and_predicts_only_valid_glide(){
  let mut sim=Simulation::new(ConnectionProtocol::Game);sim.receive_packet(default_login_packet());sim.receive_packet(make_basic_empty_chunk(ChunkPos::new(0,0),24));sim.tick();sim.app.world_mut().entity_mut(sim.entity).insert(HasClientLoaded);
  sim.with_component_mut::<Position>(|p|**p=Vec3::new(8.,80.,8.));sim.with_component_mut::<Inventory>(|i|*i.inventory_menu.slot_mut(6).unwrap()=ItemStack::new(ItemKind::Elytra,1));sim.with_component_mut::<Physics>(|p|p.set_on_ground(true));
  let captured=Arc::new(Mutex::new(Vec::new()));let out=captured.clone();sim.app.world_mut().add_observer(move|ev:On<SendGamePacketEvent>|{if let ServerboundGamePacket::PlayerCommand(p)=&ev.packet{if p.action==Action::StartFallFlying{out.lock().unwrap().push(p.clone());}}});
  assert!(super::super::apply_native_movement(sim.app.world_mut(),sim.entity,0.,0.,true,false));assert!(!sim.component::<FallFlying>().0);assert!(captured.lock().unwrap().is_empty());
  sim.with_component_mut::<Physics>(|p|p.set_on_ground(false));assert!(super::super::apply_native_movement(sim.app.world_mut(),sim.entity,0.,0.,true,false));assert!(captured.lock().unwrap().is_empty());
  assert!(super::super::apply_native_movement(sim.app.world_mut(),sim.entity,0.,0.,false,false));assert!(super::super::apply_native_movement(sim.app.world_mut(),sim.entity,0.,0.,true,false));assert!(sim.component::<FallFlying>().0);assert_eq!(captured.lock().unwrap().len(),1);
  assert!(super::super::apply_native_movement(sim.app.world_mut(),sim.entity,0.,0.,true,false));assert_eq!(captured.lock().unwrap().len(),1);
  sim.app.world_mut().entity_mut(sim.entity).insert(FallFlying(false));sim.with_component_mut::<Inventory>(|i|*i.inventory_menu.slot_mut(6).unwrap()=ItemStack::Empty);super::super::apply_native_movement(sim.app.world_mut(),sim.entity,0.,0.,false,false);super::super::apply_native_movement(sim.app.world_mut(),sim.entity,0.,0.,true,false);assert!(!sim.component::<FallFlying>().0);assert_eq!(captured.lock().unwrap().len(),1);
 }
}
