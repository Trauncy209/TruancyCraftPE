//! Mirror Java's shared player Inventory behind the independent native menus.
use azalea_entity::inventory::Inventory;
use azalea_inventory::ItemStack;
use bevy_ecs::prelude::*;

/// Java raw Inventory indices differ from InventoryMenu slot indices.
pub fn raw_player_slot(raw:u32)->Option<usize>{match raw{0..=8=>Some(raw as usize+36),9..=35=>Some(raw as usize),36..=39=>Some(44-raw as usize),40=>Some(45),_=>None}}

/// BODY and SADDLE are real equipment, but have no InventoryMenu GUI slots.
#[derive(Component,Clone,Default)]
pub struct ExtraPlayerEquipment{pub body:ItemStack,pub saddle:ItemStack}

pub fn set_player_slot(inv:&mut Inventory,index:usize,item:&ItemStack)->bool{
 let range=inv.inventory_menu.player_slots_range();
 let Some(slot)=inv.inventory_menu.slot_mut(index)else{return false;};*slot=item.clone();
 if range.contains(&index){if let Some(menu)=inv.container_menu.as_mut(){if let Some(target)=menu.player_slots_range().nth(index-*range.start()){if let Some(slot)=menu.slot_mut(target){*slot=item.clone();}}}}
 true
}
pub fn set_active_slot(inv:&mut Inventory,index:usize,item:&ItemStack)->bool{
 if inv.id==0{return set_player_slot(inv,index,item);}
 let Some(menu)=inv.container_menu.as_mut()else{return false;};let range=menu.player_slots_range();
 let Some(slot)=menu.slot_mut(index)else{return false;};*slot=item.clone();
 if range.contains(&index){if let Some(target)=inv.inventory_menu.player_slots_range().nth(index-*range.start()){if let Some(slot)=inv.inventory_menu.slot_mut(target){*slot=item.clone();}}}
 true
}
pub fn player_to_active(inv:&mut Inventory){
 let source=inv.inventory_menu.slots();let range=inv.inventory_menu.player_slots_range();
 let Some(items)=source.get(range)else{return;};let items=items.to_vec();
 if let Some(menu)=inv.container_menu.as_mut(){for (index,item) in menu.player_slots_range().zip(items){if let Some(slot)=menu.slot_mut(index){*slot=item;}}}
}
pub fn active_to_player(inv:&mut Inventory){
 let Some(menu)=inv.container_menu.as_ref()else{return;};let source=menu.slots();let range=menu.player_slots_range();
 let Some(items)=source.get(range)else{return;};let items=items.to_vec();
 for(index,item)in inv.inventory_menu.player_slots_range().zip(items){if let Some(slot)=inv.inventory_menu.slot_mut(index){*slot=item;}}
}
