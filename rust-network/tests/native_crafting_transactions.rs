//! Headless real packet/ECS crafting transactions; no invented local recipes.
use azalea_client::{test_utils::prelude::*,inventory::ContainerClickEvent,packet::game::SendGamePacketEvent};
use azalea_entity::inventory::Inventory;
use azalea_inventory::{ItemStack,operations::{ClickOperation,ClickType,PickupClick,QuickMoveClick}};
use azalea_protocol::packets::{ConnectionProtocol,game::*};
use azalea_registry::builtin::{ItemKind,MenuKind};
use bevy_ecs::prelude::*;
use std::sync::{Arc,Mutex};

#[test]
fn actual_two_and_three_grid_result_clicks_use_current_server_state_and_authoritative_cursor(){
 for table in [false,true]{
  let mut sim=Simulation::new(ConnectionProtocol::Game);sim.receive_packet(default_login_packet());sim.update();let id=if table{7}else{0};
  if table{sim.receive_packet(ClientboundOpenScreen{container_id:id,menu_type:MenuKind::Crafting,title:azalea_chat::FormattedText::from("Crafting fixture")});sim.update();}
  let mut slots=vec![ItemStack::Empty;46];slots[0]=ItemStack::new(ItemKind::CraftingTable,1);for index in [1,2,if table{4}else{3},if table{5}else{4}]{slots[index]=ItemStack::new(ItemKind::OakPlanks,2);}
  sim.receive_packet(ClientboundContainerSetContent{container_id:id,state_id:37,items:slots,carried_item:ItemStack::Empty});sim.update();
  let captured=Arc::new(Mutex::new(Vec::new()));let out=captured.clone();sim.app.world_mut().add_observer(move|event:On<SendGamePacketEvent>|{if let ServerboundGamePacket::ContainerClick(p)=&event.packet{out.lock().unwrap().push(p.clone());}});
  sim.app.world_mut().trigger(ContainerClickEvent{entity:sim.entity,window_id:id,operation:ClickOperation::Pickup(PickupClick::Left{slot:Some(0)})});sim.app.world_mut().flush();sim.update();
  {let packets=captured.lock().unwrap();assert_eq!(packets.len(),1);assert_eq!(packets[0].container_id,id);assert_eq!(packets[0].state_id,37);assert_eq!(packets[0].slot_num,0);assert_eq!(packets[0].click_type,ClickType::Pickup);assert_eq!(packets[0].carried_item.0.as_ref().unwrap().kind,ItemKind::CraftingTable);}
  // The server, not a client recipe simulator, determines consumption/result.
  let mut authoritative=vec![ItemStack::Empty;46];authoritative[1]=ItemStack::new(ItemKind::OakPlanks,1);
  sim.receive_packet(ClientboundContainerSetContent{container_id:id,state_id:38,items:authoritative,carried_item:ItemStack::new(ItemKind::CraftingTable,1)});sim.update();
  let inv=sim.component::<Inventory>();assert_eq!(inv.state_id,38);assert_eq!(inv.carried,ItemStack::new(ItemKind::CraftingTable,1));assert_eq!(inv.menu().slot(1),Some(&ItemStack::new(ItemKind::OakPlanks,1)));assert!(inv.menu().slot(0).unwrap().is_empty());
 }
}

#[test]
fn actual_all_result_quick_move_preserves_authoritative_cursor_and_server_rejection_resynchronizes(){
 for table in [false,true]{
  let mut sim=Simulation::new(ConnectionProtocol::Game);sim.receive_packet(default_login_packet());sim.update();let id=if table{7}else{0};if table{sim.receive_packet(ClientboundOpenScreen{container_id:id,menu_type:MenuKind::Crafting,title:azalea_chat::FormattedText::from("Crafting fixture")});sim.update();}
  let mut slots=vec![ItemStack::Empty;46];slots[0]=ItemStack::new(ItemKind::CraftingTable,1);slots[1]=ItemStack::new(ItemKind::OakPlanks,8);sim.receive_packet(ClientboundContainerSetContent{container_id:id,state_id:51,items:slots.clone(),carried_item:ItemStack::Empty});sim.update();
  let captured=Arc::new(Mutex::new(Vec::new()));let out=captured.clone();sim.app.world_mut().add_observer(move|event:On<SendGamePacketEvent>|{if let ServerboundGamePacket::ContainerClick(p)=&event.packet{out.lock().unwrap().push(p.clone());}});
  sim.app.world_mut().trigger(ContainerClickEvent{entity:sim.entity,window_id:id,operation:ClickOperation::QuickMove(QuickMoveClick::Left{slot:0})});sim.app.world_mut().flush();sim.update();
  {let packets=captured.lock().unwrap();assert_eq!(packets.len(),1);assert_eq!(packets[0].state_id,51);assert_eq!(packets[0].slot_num,0);assert_eq!(packets[0].click_type,ClickType::QuickMove);assert!(packets[0].carried_item.0.is_none());}
  sim.receive_packet(ClientboundContainerSetContent{container_id:id,state_id:52,items:slots,carried_item:ItemStack::Empty});sim.update();let inv=sim.component::<Inventory>();assert_eq!(inv.state_id,52);assert_eq!(inv.menu().slot(0),Some(&ItemStack::new(ItemKind::CraftingTable,1)));assert_eq!(inv.menu().slot(1),Some(&ItemStack::new(ItemKind::OakPlanks,8)));assert!(inv.carried.is_empty());
 }
}
