use azalea_client::{test_utils::prelude::*,native_firework_visuals as visuals};
use azalea_core::{position::{ChunkPos,Vec3},delta::LpVec3};
use azalea_entity::{EntityMetadataItems,EntityDataItem,EntityDataValue,OptionalUnsignedInt,metadata::PlayerMainHand,HumanoidArm,inventory::Inventory,Position,LookDirection};
use azalea_inventory::{ItemStack,components::{Fireworks,FireworkExplosion,FireworkExplosionShape}};
use azalea_protocol::packets::{ConnectionProtocol,game::*};
use azalea_registry::builtin::{EntityKind,ItemKind};
use azalea_world::MinecraftEntityId;
fn simulation()->Simulation{let mut s=Simulation::new(ConnectionProtocol::Game);s.receive_packet(default_login_packet());s.receive_packet(make_basic_empty_chunk(ChunkPos::new(0,0),24));s.tick();s}
fn explosion()->FireworkExplosion{FireworkExplosion{shape:FireworkExplosionShape::Star,colors:vec![0x123456,0xff0000],fade_colors:vec![0x00ff00],has_trail:true,has_twinkle:true}}
fn add(s:&mut Simulation,id:i32){s.receive_packet(ClientboundAddEntity{id:MinecraftEntityId(id),uuid:uuid::Uuid::new_v4(),entity_type:EntityKind::FireworkRocket,position:Vec3::new(8.,80.,8.),movement:LpVec3::from(Vec3::new(0.1,0.2,0.3)),x_rot:0,y_rot:0,y_head_rot:0,data:0});s.receive_packet(ClientboundSetEntityData{id:MinecraftEntityId(id),packed_items:EntityMetadataItems(vec![EntityDataItem{index:8,value:EntityDataValue::ItemStack(ItemStack::new(ItemKind::FireworkRocket,1).with_component(Fireworks{flight_duration:1,explosions:vec![explosion()]}))}])});s.update();}
fn status(s:&mut Simulation,id:i32){s.receive_packet(ClientboundEntityEvent{entity_id:MinecraftEntityId(id),event_id:17});}
#[test]fn actual_status17_then_same_batch_remove_captures_authored_effect_once_and_reused_id_is_new_generation(){
 let mut s=simulation();add(&mut s,99);status(&mut s,99);status(&mut s,99);s.receive_packet(ClientboundRemoveEntities{entity_ids:vec![MinecraftEntityId(99)]});s.update();
 let event=visuals::drain(s.app.world_mut(),s.entity);assert_eq!(event.len(),1);assert_eq!(event[0].position,Vec3::new(8.,80.,8.));assert_eq!(event[0].explosions,vec![explosion()]);assert!(visuals::trails(s.app.world_mut(),s.entity).is_empty());assert!(visuals::drain(s.app.world_mut(),s.entity).is_empty());
 add(&mut s,99);status(&mut s,99);s.update();assert_eq!(visuals::drain(s.app.world_mut(),s.entity).len(),1);
}
#[test]fn actual_respawn_clears_queued_effects_and_wrong_status_or_empty_payload_never_invents_explosions(){
 let mut s=simulation();add(&mut s,99);s.receive_packet(ClientboundEntityEvent{entity_id:MinecraftEntityId(99),event_id:3});s.update();assert!(visuals::drain(s.app.world_mut(),s.entity).is_empty());
 status(&mut s,99);s.update();s.receive_packet(ClientboundRespawn{common:default_login_packet().common,data_to_keep:0});s.update();assert!(visuals::drain(s.app.world_mut(),s.entity).is_empty());
 add(&mut s,100);s.receive_packet(ClientboundSetEntityData{id:MinecraftEntityId(100),packed_items:EntityMetadataItems(vec![EntityDataItem{index:8,value:EntityDataValue::ItemStack(ItemStack::Empty)}])});status(&mut s,100);s.update();assert!(visuals::drain(s.app.world_mut(),s.entity).is_empty());
}
#[test]fn attached_local_trail_tracks_actual_equipment_hand_arm_position_and_camera(){
 let mut s=simulation();add(&mut s,99);let id=s.minecraft_entity_id();s.with_component_mut::<Position>(|p|**p=Vec3::new(8.,80.,8.));s.with_component_mut::<LookDirection>(|look|*look=LookDirection::new(0.,0.));
 s.with_component_mut::<Inventory>(|inv|{*inv.inventory_menu.slot_mut(36).unwrap()=ItemStack::new(ItemKind::FireworkRocket,1);});
 s.receive_packet(ClientboundSetEntityData{id:MinecraftEntityId(99),packed_items:EntityMetadataItems(vec![EntityDataItem{index:9,value:EntityDataValue::OptionalUnsignedInt(OptionalUnsignedInt(Some(id.0 as u32)))}])});s.update();
 let main=visuals::trails(s.app.world_mut(),s.entity);assert_eq!(main.len(),1);assert!(main[0].0.x<8.&&main[0].0.z>8.);assert_eq!(main[0].0.y,80.);
 s.with_component_mut::<Inventory>(|inv|{*inv.inventory_menu.slot_mut(36).unwrap()=ItemStack::Empty;*inv.inventory_menu.slot_mut(45).unwrap()=ItemStack::new(ItemKind::FireworkRocket,1);});let off=visuals::trails(s.app.world_mut(),s.entity);assert!(off[0].0.x>8.);s.app.world_mut().entity_mut(s.entity).insert(PlayerMainHand(HumanoidArm::Left));let left=visuals::trails(s.app.world_mut(),s.entity);assert_eq!(left[0].0,main[0].0);assert_eq!(s.component::<LookDirection>(),LookDirection::new(0.,0.));
}
#[test]fn remote_attached_trail_uses_actual_equipment_packets_and_world_scope(){
 let mut s=simulation();add(&mut s,99);let owner=MinecraftEntityId(88);
 s.receive_packet(ClientboundAddEntity{id:owner,uuid:uuid::Uuid::new_v4(),entity_type:EntityKind::Player,position:Vec3::new(12.,80.,8.),movement:LpVec3::from(Vec3::ZERO),x_rot:0,y_rot:0,y_head_rot:0,data:0});
 s.receive_packet(ClientboundSetEquipment{entity_id:owner,slots:c_set_equipment::EquipmentSlots{slots:vec![(azalea_inventory::components::EquipmentSlot::Offhand,ItemStack::new(ItemKind::FireworkRocket,1))]}});
 s.receive_packet(ClientboundSetEntityData{id:MinecraftEntityId(99),packed_items:EntityMetadataItems(vec![EntityDataItem{index:9,value:EntityDataValue::OptionalUnsignedInt(OptionalUnsignedInt(Some(88)))}])});s.update();
 let off=visuals::trails(s.app.world_mut(),s.entity);assert_eq!(off.len(),1);assert!(off[0].0.x>12.);
 s.receive_packet(ClientboundSetEquipment{entity_id:owner,slots:c_set_equipment::EquipmentSlots{slots:vec![(azalea_inventory::components::EquipmentSlot::Mainhand,ItemStack::new(ItemKind::FireworkRocket,1))]}});s.update();let both=visuals::trails(s.app.world_mut(),s.entity);assert!(both[0].0.x<12.);
 let rocket=s.app.world().get::<azalea_entity::indexing::EntityIdIndex>(s.entity).unwrap().get_by_minecraft_entity(MinecraftEntityId(99)).unwrap();s.app.world_mut().entity_mut(rocket).insert(azalea_world::InstanceName("minecraft:the_nether".parse().unwrap()));assert!(visuals::trails(s.app.world_mut(),s.entity).is_empty());status(&mut s,99);s.update();assert!(visuals::drain(s.app.world_mut(),s.entity).is_empty());
}
