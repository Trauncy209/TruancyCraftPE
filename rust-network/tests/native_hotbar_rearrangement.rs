//! Real player-menu click packets, cursor prediction and authoritative resync.
use azalea_client::{test_utils::prelude::*, inventory::ContainerClickEvent,packet::game::SendGamePacketEvent};
use azalea_entity::inventory::Inventory;
use azalea_inventory::{ItemStack,operations::{ClickOperation,ClickType,PickupClick}};
use azalea_protocol::packets::{ConnectionProtocol,game::*};
use azalea_registry::builtin::ItemKind;
use bevy_ecs::prelude::*;
use std::sync::{Arc,Mutex};
#[test]
fn hotbar_pickup_empty_place_existing_swap_and_server_correction_use_actual_player_slots(){
 let mut sim=Simulation::new(ConnectionProtocol::Game);sim.receive_packet(default_login_packet());sim.update();
 let stone=ItemStack::new(ItemKind::Stone,8);let sword=ItemStack::new(ItemKind::IronSword,1);let mut slots=vec![ItemStack::Empty;46];slots[36]=stone.clone();slots[38]=sword.clone();
 sim.receive_packet(ClientboundContainerSetContent{container_id:0,state_id:71,items:slots.clone(),carried_item:ItemStack::Empty});sim.update();
 let captured=Arc::new(Mutex::new(Vec::new()));let out=captured.clone();sim.app.world_mut().add_observer(move|e:On<SendGamePacketEvent>|{if let ServerboundGamePacket::ContainerClick(p)=&e.packet{out.lock().unwrap().push(p.clone());}});
 // Pick slot36, put in empty37, pick again, swap with sword38, put sword in bag9.
 for slot in [36,37,37,38,9]{sim.app.world_mut().trigger(ContainerClickEvent{entity:sim.entity,window_id:0,operation:ClickOperation::Pickup(PickupClick::Left{slot:Some(slot)})});sim.app.world_mut().flush();sim.update();}
 {let packets=captured.lock().unwrap();assert_eq!(packets.len(),5);for(p,slot)in packets.iter().zip([36,37,37,38,9]){assert_eq!(p.slot_num,slot);assert_eq!(p.state_id,71);assert_eq!(p.container_id,0);assert_eq!(p.click_type,ClickType::Pickup);}assert_eq!(packets[0].carried_item.0.as_ref().unwrap().kind,ItemKind::Stone);assert!(packets[1].carried_item.0.is_none());assert_eq!(packets[3].carried_item.0.as_ref().unwrap().kind,ItemKind::IronSword);assert!(packets[4].carried_item.0.is_none());}
 let inv=sim.component::<Inventory>();assert!(inv.menu().slot(36).unwrap().is_empty());assert!(inv.menu().slot(37).unwrap().is_empty());assert_eq!(inv.menu().slot(38),Some(&stone));assert_eq!(inv.menu().slot(9),Some(&sword));assert!(inv.carried.is_empty());
 // A server rejection/correction fully replaces the prediction and cursor.
 sim.receive_packet(ClientboundContainerSetContent{container_id:0,state_id:72,items:slots,carried_item:ItemStack::Empty});sim.update();let inv=sim.component::<Inventory>();assert_eq!(inv.state_id,72);assert_eq!(inv.menu().slot(36),Some(&stone));assert_eq!(inv.menu().slot(38),Some(&sword));assert!(inv.menu().slot(9).unwrap().is_empty());assert!(inv.carried.is_empty());
}
