//! Native firework Use path: real queued action and packet dispatch, no invented
//! launch entity, rocket consumption or propulsion before authoritative packets.
use azalea_client::{test_utils::prelude::*,interact::{StartUseItemQueued,native_use::{NativeUseOptions,Context,item_outcome,PredictedNativeUse}},packet::game::SendGamePacketEvent};
use azalea_core::position::{ChunkPos,Vec3};
use azalea_entity::{Physics,Position,LookDirection,inventory::Inventory,metadata::{FallFlying,FireworkRocket}};
use azalea_inventory::ItemStack;
use azalea_protocol::packets::{ConnectionProtocol,game::{ServerboundGamePacket,ClientboundContainerSetSlot,s_interact::InteractionHand}};
use azalea_registry::builtin::ItemKind;
use bevy_ecs::prelude::*;
use std::sync::{Arc,Mutex};

#[test]
fn gliding_main_offhand_and_automatic_rocket_use_emit_correct_hand_without_local_consumption_or_entity(){
 for (hand,automatic) in [(InteractionHand::MainHand,false),(InteractionHand::OffHand,false),(InteractionHand::OffHand,true)]{
  let mut sim=Simulation::new(ConnectionProtocol::Game);sim.receive_packet(default_login_packet());sim.receive_packet(make_basic_empty_chunk(ChunkPos::new(0,0),24));sim.tick();
  sim.with_component_mut::<Position>(|p|**p=Vec3::new(8.,80.,8.));sim.with_component_mut::<Physics>(|p|{p.set_on_ground(false);p.velocity=Vec3::new(0.,0.,1.);});
  let rocket=ItemStack::new(ItemKind::FireworkRocket,3);let main=if hand==InteractionHand::MainHand{rocket.clone()}else{ItemStack::Empty};let off=if hand==InteractionHand::OffHand{rocket.clone()}else{ItemStack::Empty};
  sim.with_component_mut::<Inventory>(|inv|{*inv.inventory_menu.slot_mut(6).unwrap()=ItemStack::new(ItemKind::Elytra,1);*inv.inventory_menu.slot_mut(36).unwrap()=main.clone();*inv.inventory_menu.slot_mut(45).unwrap()=off.clone();});
  assert!(azalea_physics::elytra::can_start_fall_flying(sim.app.world(),sim.entity));
  sim.app.world_mut().entity_mut(sim.entity).insert((FallFlying(true),LookDirection::new(37.,-15.)));
  let inv=sim.component::<Inventory>();let context=Context{food:20,..Default::default()};let options=NativeUseOptions{automatic,main:item_outcome(&main,&inv,context),off:item_outcome(&off,&inv,context),main_stack:main.clone(),off_stack:off.clone()};
  let packets=Arc::new(Mutex::new(Vec::new()));let out=packets.clone();sim.app.world_mut().add_observer(move|event:On<SendGamePacketEvent>|{if let ServerboundGamePacket::UseItem(packet)=&event.packet{out.lock().unwrap().push(packet.clone());}});
  // Match the worker's direct queued component and normal GameTick consumer.
  sim.app.world_mut().entity_mut(sim.entity).insert((options,StartUseItemQueued{hand:if automatic{InteractionHand::MainHand}else{hand},force_block:None}));sim.tick();
  {let sent=packets.lock().unwrap();assert_eq!(sent.len(),1,"wrong packet count hand={hand:?} automatic={automatic}");assert_eq!(sent[0].hand,hand);assert_eq!(sent[0].y_rot,37.);assert_eq!(sent[0].x_rot,-15.);assert!(sent[0].seq>0);}
  let inv=sim.component::<Inventory>();assert_eq!(inv.inventory_menu.slot(36),Some(&main));assert_eq!(inv.inventory_menu.slot(45),Some(&off));assert!(inv.carried.is_empty());assert!(sim.app.world().get::<PredictedNativeUse>(sim.entity).is_none());assert!(sim.app.world().get::<StartUseItemQueued>(sim.entity).is_none());
  assert_eq!(sim.app.world_mut().query_filtered::<Entity,With<FireworkRocket>>().iter(sim.app.world()).count(),0,"UseItem cannot invent a server rocket");
  // Only a real server inventory packet consumes this stack in the fixture.
  let slot=if hand==InteractionHand::MainHand{36}else{45};let remaining=ItemStack::new(ItemKind::FireworkRocket,2);sim.receive_packet(ClientboundContainerSetSlot{container_id:0,state_id:17,slot,item_stack:remaining.clone()});sim.update();assert_eq!(sim.component::<Inventory>().inventory_menu.slot(slot as usize),Some(&remaining));
 }
}
