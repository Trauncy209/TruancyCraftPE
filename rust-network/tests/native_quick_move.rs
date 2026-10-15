use azalea_client::{test_utils::prelude::*,inventory::ContainerClickEvent,packet::game::SendGamePacketEvent};
use azalea_entity::inventory::Inventory;
use azalea_inventory::{ItemStack,operations::{ClickOperation,ClickType,QuickMoveClick}};
use azalea_protocol::packets::{ConnectionProtocol,game::{ClientboundContainerSetContent,ServerboundGamePacket}};
use azalea_registry::builtin::ItemKind;
use bevy_ecs::prelude::*;
use std::sync::{Arc,Mutex};
#[test]
fn native_quick_move_actual_server_packet_and_prediction_preserve_whole_stack(){
 for right in [false,true]{
  let mut sim=Simulation::new(ConnectionProtocol::Game);sim.receive_packet(default_login_packet());sim.update();
  let mut slots=vec![ItemStack::Empty;46];slots[9]=ItemStack::new(ItemKind::Stone,8);
  sim.receive_packet(ClientboundContainerSetContent{container_id:0,state_id:7,items:slots,carried_item:ItemStack::Empty});sim.update();
  let packets=Arc::new(Mutex::new(Vec::new()));let out=packets.clone();sim.app.world_mut().add_observer(move |event:On<SendGamePacketEvent>|{if let ServerboundGamePacket::ContainerClick(packet)=&event.packet{out.lock().unwrap().push(packet.clone());}});
  sim.app.world_mut().trigger(ContainerClickEvent{entity:sim.entity,window_id:0,operation:ClickOperation::QuickMove(if right{QuickMoveClick::Right{slot:9}}else{QuickMoveClick::Left{slot:9}})});sim.app.world_mut().flush();sim.update();
  let packets=packets.lock().unwrap();assert_eq!(packets.len(),1);let packet=&packets[0];assert_eq!(packet.container_id,0);assert_eq!(packet.state_id,7);assert_eq!(packet.slot_num,9);assert_eq!(packet.button_num,u8::from(right));assert_eq!(packet.click_type,ClickType::QuickMove);assert!(packet.carried_item.0.is_none());assert!(packet.changed_slots[&9].0.is_none());let moved=packet.changed_slots[&36].0.as_ref().unwrap();assert_eq!(moved.kind,ItemKind::Stone);assert_eq!(moved.count,8);
  let inv=sim.component::<Inventory>();assert!(inv.inventory_menu.slot(9).unwrap().is_empty());assert_eq!(inv.inventory_menu.slot(36),Some(&ItemStack::new(ItemKind::Stone,8)));assert!(inv.carried.is_empty());
 }
}
#[test]
fn actual_full_contents_scopes_state_and_carried_to_each_menu(){
 use azalea_protocol::packets::game::{ClientboundOpenScreen,ClientboundContainerClose};use azalea_registry::builtin::MenuKind;
 let mut sim=Simulation::new(ConnectionProtocol::Game);sim.receive_packet(default_login_packet());sim.update();
 sim.receive_packet(ClientboundContainerSetContent{container_id:0,state_id:7,items:vec![ItemStack::Empty;46],carried_item:ItemStack::new(ItemKind::Diamond,3)});sim.update();assert_eq!(sim.component::<Inventory>().state_id,7);assert_eq!(sim.component::<Inventory>().carried,ItemStack::new(ItemKind::Diamond,3));
 sim.receive_packet(ClientboundOpenScreen{container_id:5,menu_type:MenuKind::Generic9x3,title:azalea_chat::FormattedText::from("Fixture")});sim.update();assert_eq!(sim.component::<Inventory>().state_id,0);assert!(sim.component::<Inventory>().carried.is_empty());
 sim.receive_packet(ClientboundContainerSetContent{container_id:5,state_id:9,items:vec![ItemStack::Empty;63],carried_item:ItemStack::new(ItemKind::Coal,4)});sim.update();assert_eq!(sim.component::<Inventory>().state_id,9);assert_eq!(sim.component::<Inventory>().carried,ItemStack::new(ItemKind::Coal,4));
 sim.receive_packet(ClientboundContainerSetContent{container_id:0,state_id:11,items:vec![ItemStack::Empty;46],carried_item:ItemStack::new(ItemKind::IronIngot,2)});sim.update();assert_eq!(sim.component::<Inventory>().state_id,9);assert_eq!(sim.component::<Inventory>().carried,ItemStack::new(ItemKind::Coal,4));
 sim.receive_packet(ClientboundContainerSetContent{container_id:6,state_id:99,items:vec![ItemStack::Empty;63],carried_item:ItemStack::new(ItemKind::GoldIngot,9)});sim.update();assert_eq!(sim.component::<Inventory>().state_id,9);assert_eq!(sim.component::<Inventory>().carried,ItemStack::new(ItemKind::Coal,4));
 sim.receive_packet(ClientboundContainerClose{container_id:5});sim.update();let inv=sim.component::<Inventory>();assert_eq!(inv.id,0);assert_eq!(inv.state_id,11);assert_eq!(inv.carried,ItemStack::new(ItemKind::IronIngot,2));
}
