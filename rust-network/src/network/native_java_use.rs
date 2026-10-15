use azalea_client::interact::native_use::{Context,NativeUseOptions,PredictedNativeUse,item_outcome};
use azalea_entity::inventory::Inventory;
use azalea_inventory::{ItemStack,components::{EquipmentSlot,Enchantments}};
use azalea_protocol::packets::game::s_interact::InteractionHand;
use bevy_ecs::{world::World,entity::Entity};
use azalea_registry::DataRegistry;
pub fn options(world:&World,local:Entity,automatic:bool)->Option<NativeUseOptions>{
 if world.get::<azalea_client::local_player::LocalGameMode>(local).is_some_and(|mode|mode.current==azalea_core::game_type::GameMode::Spectator){return None;}
 let inv=world.get::<Inventory>(local)?;let main=inv.held_item().clone();let off=inv.get_equipment(EquipmentSlot::Offhand).cloned().unwrap_or(ItemStack::Empty);
 let abilities=world.get::<azalea_entity::PlayerAbilities>(local);let food=world.get::<azalea_client::local_player::Hunger>(local).map(|h|h.food).unwrap_or(20);
 let wet=world.get::<azalea_entity::Physics>(local).is_some_and(|p|p.is_in_water());
 let context=|stack:&ItemStack|{let riptide=match stack{ItemStack::Present(item)=>item.get_component::<Enchantments>().is_some_and(|enchants|world.get::<azalea_client::local_player::InstanceHolder>(local).is_some_and(|holder|{let instance=holder.instance.read();enchants.levels.iter().any(|(enchantment,level)|*level>0&&instance.registries.enchantment.map.get_index(enchantment.protocol_id() as usize).is_some_and(|(name,_)|name.to_string()=="minecraft:riptide"))})),_=>false};Context{invulnerable:abilities.is_some_and(|a|a.invulnerable),creative:abilities.is_some_and(|a|a.instant_break),food,wet,riptide}};
 Some(NativeUseOptions{automatic,main:item_outcome(&main,inv,context(&main)),off:item_outcome(&off,inv,context(&off)),main_stack:main,off_stack:off})
}
pub fn tick(world:&mut World,local:Entity){
 let Some(predicted)=world.get::<PredictedNativeUse>(local).cloned()else{return;};
 if world.get::<azalea_client::local_player::LocalGameMode>(local).is_some_and(|mode|mode.current==azalea_core::game_type::GameMode::Spectator){authoritative(world,local);if let Some(mut flag)=world.get_mut::<azalea_entity::metadata::AbstractLivingUsingItem>(local){flag.0=false;}if let Some(mut flag)=world.get_mut::<azalea_entity::metadata::AbstractLivingUsingOffHand>(local){flag.0=false;}return;}
 // LivingEntity.updatingUsingItem uses ItemStack.isSameItem (kind only),
 // then refreshes its active stack. Count/components may change during charging.
 let current=world.get::<Inventory>(local).filter(|inv|inv.id==0).and_then(|inv|inv.get_equipment(if predicted.hand==InteractionHand::OffHand{EquipmentSlot::Offhand}else{EquipmentSlot::Mainhand})).cloned();
 let matches=matches!((&predicted.stack,&current),(ItemStack::Present(previous),Some(ItemStack::Present(current))) if previous.kind==current.kind);
 if !matches||predicted.ticks.saturating_add(1)>=predicted.duration {super::native_java::clear_native_item_use(world,local);return;}
 if let Some(mut p)=world.get_mut::<PredictedNativeUse>(local){p.ticks=p.ticks.saturating_add(1);if let Some(current)=current{p.stack=current;}}
}
pub fn target_max_health(world:&mut World,local:Entity,id:i32,value:Option<f32>){
 let Some(target)=world.get::<azalea_entity::indexing::EntityIdIndex>(local).and_then(|index|index.get_by_minecraft_entity(azalea_world::MinecraftEntityId(id)))else{return;};
 if !world.get::<azalea_entity::LoadedBy>(target).is_some_and(|loaded|loaded.contains(&local)){return;}
 if let Some(value)=value.filter(|v|v.is_finite()&&*v>0.){world.entity_mut(target).insert(azalea_client::interact::native_use::NativeTargetMaxHealth{owner:local,value});}else if world.get::<azalea_client::interact::native_use::NativeTargetMaxHealth>(target).is_some_and(|max|max.owner==local){world.entity_mut(target).remove::<azalea_client::interact::native_use::NativeTargetMaxHealth>();}
}
pub fn clear_target_max_health(world:&mut World,local:Entity){
 let targets=world.query::<(Entity,&azalea_client::interact::native_use::NativeTargetMaxHealth)>().iter(world).filter(|(_,max)|max.owner==local).map(|(e,_)|e).collect::<Vec<_>>();for target in targets{world.entity_mut(target).remove::<azalea_client::interact::native_use::NativeTargetMaxHealth>();}
}
pub fn busy(world:&World,local:Entity)->bool{
 world.get::<azalea_entity::metadata::AbstractLivingUsingItem>(local).is_some_and(|flag|flag.0)||world.get::<PredictedNativeUse>(local).is_some()||world.get::<azalea_client::interact::StartUseItemQueued>(local).is_some()
}
pub fn authoritative(world:&mut World,local:Entity){if let Ok(mut e)=world.get_entity_mut(local){e.remove::<PredictedNativeUse>();e.remove::<NativeUseOptions>();}}
#[cfg(test)] mod tests {
 use super::*;use azalea_registry::builtin::ItemKind;use azalea_client::interact::native_use::{Outcome,Context,item_outcome};
 fn stack(k:ItemKind)->ItemStack{ItemStack::new(k,1)}
 #[test]fn native_use_outcomes_match_official_conditions(){
  let mut inv=Inventory::default();let full=Context{food:20,..Default::default()};
  assert_eq!(item_outcome(&stack(ItemKind::DiamondSword),&inv,full),Outcome::Pass);
  assert_eq!(item_outcome(&stack(ItemKind::Shield),&inv,full),Outcome::Consume{duration:72000});
  assert_eq!(item_outcome(&stack(ItemKind::Apple),&inv,full),Outcome::Fail);
  assert_eq!(item_outcome(&stack(ItemKind::Apple),&inv,Context{food:19,..full}),Outcome::Consume{duration:32});
  assert_eq!(item_outcome(&stack(ItemKind::GoldenApple),&inv,full),Outcome::Consume{duration:32});
  assert_eq!(item_outcome(&stack(ItemKind::Potion),&inv,full),Outcome::Consume{duration:32});
  assert_eq!(item_outcome(&stack(ItemKind::Bow),&inv,full),Outcome::Fail);
  assert_eq!(item_outcome(&stack(ItemKind::Bow),&inv,Context{creative:true,..full}),Outcome::Consume{duration:72000});
  *inv.inventory_menu.slot_mut(9).unwrap()=stack(ItemKind::Arrow);assert_eq!(item_outcome(&stack(ItemKind::Bow),&inv,full),Outcome::Consume{duration:72000});
  *inv.inventory_menu.slot_mut(9).unwrap()=stack(ItemKind::FireworkRocket);assert_eq!(item_outcome(&stack(ItemKind::Crossbow),&inv,full),Outcome::Fail);
  *inv.inventory_menu.slot_mut(45).unwrap()=stack(ItemKind::FireworkRocket);assert_eq!(item_outcome(&stack(ItemKind::Crossbow),&inv,full),Outcome::Consume{duration:72000});
  assert_eq!(item_outcome(&stack(ItemKind::Trident),&inv,Context{riptide:true,..full}),Outcome::Fail);
  assert_eq!(item_outcome(&stack(ItemKind::Trident),&inv,Context{riptide:true,wet:true,..full}),Outcome::Consume{duration:72000});
  assert_eq!(item_outcome(&stack(ItemKind::IronSpear),&inv,full),Outcome::Consume{duration:72000});
  let loaded=stack(ItemKind::Crossbow).with_component(azalea_inventory::components::ChargedProjectiles{items:vec![stack(ItemKind::Arrow)]});assert_eq!(item_outcome(&loaded,&inv,full),Outcome::Consume{duration:0});
  let broken=stack(ItemKind::Trident).with_component(azalea_inventory::components::Damage{amount:249});assert_eq!(item_outcome(&broken,&inv,full),Outcome::Fail);
 }
 #[test]fn native_use_default_block_gate_preserves_containers_and_placement(){
  use azalea_client::interact::native_use::default_block_pass;
  assert!(default_block_pass("minecraft:stone",&stack(ItemKind::Shield)));
  assert!(default_block_pass("minecraft:oak_stairs",&stack(ItemKind::Bow)));
  for block in ["minecraft:chest","minecraft:crafting_table","minecraft:barrel","minecraft:anvil","minecraft:unknown"]{assert!(!default_block_pass(block,&stack(ItemKind::Shield)));}
  assert!(!default_block_pass("minecraft:stone",&stack(ItemKind::Cobblestone)));
 }
 #[test]fn native_use_actual_air_sword_then_offhand_shield_packets_and_prediction(){
  use azalea_protocol::packets::game::ServerboundGamePacket;
  use azalea_client::{interact::{StartUseItemQueued,BlockStatePredictionHandler,pick::HitResultComponent},packet::game::SendGamePacketEvent};
  use bevy_ecs::{prelude::*,system::RunSystemOnce};use std::sync::{Arc,Mutex};
  let mut world=World::new();let packets=Arc::new(Mutex::new(Vec::<ServerboundGamePacket>::new()));let captured=packets.clone();world.add_observer(move |e:On<SendGamePacketEvent>|{captured.lock().unwrap().push(e.packet.clone());});
  let mut inv=Inventory::default();*inv.inventory_menu.slot_mut(36).unwrap()=stack(ItemKind::DiamondSword);*inv.inventory_menu.slot_mut(45).unwrap()=stack(ItemKind::Shield);
  let hit=azalea_core::hit_result::HitResult::Block(azalea_core::hit_result::BlockHitResult{location:Default::default(),direction:azalea_core::direction::Direction::Up,block_pos:Default::default(),inside:false,world_border:false,miss:true});
  let e=world.spawn((inv,StartUseItemQueued{hand:InteractionHand::MainHand,force_block:None},BlockStatePredictionHandler::default(),HitResultComponent::from(hit),azalea_entity::LookDirection::default())).id();let opts=options(&world,e,true).unwrap();world.entity_mut(e).insert(opts);
  world.run_system_once(azalea_client::interact::handle_start_use_item_queued).unwrap();world.flush();let sent=packets.lock().unwrap();assert_eq!(sent.len(),2);for (index,hand) in [InteractionHand::MainHand,InteractionHand::OffHand].into_iter().enumerate(){match &sent[index]{ServerboundGamePacket::UseItem(p)=>{assert_eq!(p.hand,hand);assert!(p.seq>0);},_=>panic!("wrong actual packet")}}assert_eq!(world.get::<PredictedNativeUse>(e).unwrap().hand,InteractionHand::OffHand);assert!(world.get::<azalea_entity::metadata::AbstractLivingUsingItem>(e).unwrap().0);assert!(world.get::<azalea_entity::metadata::AbstractLivingUsingOffHand>(e).unwrap().0);
 }
 #[test]fn native_use_duration_stack_menu_and_authoritative_cleanup(){
  let mut world=World::new();let mut inv=Inventory::default();let item=stack(ItemKind::Apple);*inv.inventory_menu.slot_mut(36).unwrap()=item.clone();let e=world.spawn((inv,PredictedNativeUse{hand:InteractionHand::MainHand,stack:item.clone(),duration:2,ticks:0},azalea_entity::metadata::AbstractLivingUsingItem(true))).id();tick(&mut world,e);assert_eq!(world.get::<PredictedNativeUse>(e).unwrap().ticks,1);tick(&mut world,e);assert!(world.get::<PredictedNativeUse>(e).is_none());assert!(!world.get::<azalea_entity::metadata::AbstractLivingUsingItem>(e).unwrap().0);assert_eq!(world.get::<Inventory>(e).unwrap().held_item(),&item);
  world.entity_mut(e).insert(PredictedNativeUse{hand:InteractionHand::MainHand,stack:item.clone(),duration:32,ticks:0});*world.get_mut::<Inventory>(e).unwrap().inventory_menu.slot_mut(36).unwrap()=stack(ItemKind::GoldenApple);tick(&mut world,e);assert!(world.get::<PredictedNativeUse>(e).is_none());
  world.entity_mut(e).insert(PredictedNativeUse{hand:InteractionHand::MainHand,stack:stack(ItemKind::GoldenApple),duration:32,ticks:0});world.get_mut::<Inventory>(e).unwrap().id=2;tick(&mut world,e);assert!(world.get::<PredictedNativeUse>(e).is_none());
  world.entity_mut(e).insert((PredictedNativeUse{hand:InteractionHand::MainHand,stack:item,duration:32,ticks:0},azalea_entity::metadata::AbstractLivingUsingItem(true)));authoritative(&mut world,e);assert!(world.get::<PredictedNativeUse>(e).is_none());assert!(world.get::<azalea_entity::metadata::AbstractLivingUsingItem>(e).unwrap().0);
 }
 #[test]fn native_use_spectator_prediction_is_suppressed_without_removing_interaction_queue(){
  use azalea_client::{local_player::LocalGameMode,interact::StartUseItemQueued};
  let mut world=World::new();let mut inv=Inventory::default();*inv.inventory_menu.slot_mut(36).unwrap()=stack(ItemKind::Shield);let e=world.spawn((inv,LocalGameMode::from(azalea_core::game_type::GameMode::Spectator),StartUseItemQueued{hand:InteractionHand::MainHand,force_block:None})).id();assert!(options(&world,e,true).is_none());assert!(world.get::<StartUseItemQueued>(e).is_some());assert!(world.get::<PredictedNativeUse>(e).is_none());world.entity_mut(e).insert((PredictedNativeUse{hand:InteractionHand::MainHand,stack:stack(ItemKind::Shield),duration:72000,ticks:0},azalea_entity::metadata::AbstractLivingUsingItem(true)));tick(&mut world,e);assert!(world.get::<StartUseItemQueued>(e).is_some());assert!(world.get::<PredictedNativeUse>(e).is_none());assert!(!world.get::<azalea_entity::metadata::AbstractLivingUsingItem>(e).unwrap().0);world.get_mut::<LocalGameMode>(e).unwrap().current=azalea_core::game_type::GameMode::Survival;assert!(options(&world,e,true).is_some());
 }

}
/// Value effects are sent by the server's enchantment registry. Unsupported
/// random effects return unresolved instead of inventing a charge duration.
fn value_effect(effect:&azalea_core::registry_holder::value::ValueEffect,level:i32,input:f32)->Option<f32>{
 use azalea_core::registry_holder::value::ValueEffect;
 let result=match effect{
  ValueEffect::Set{value}=>value.calculate(level),
  ValueEffect::Add{value}=>input+value.calculate(level),
  ValueEffect::Multiply{factor}=>input*factor.calculate(level),
  ValueEffect::AllOf{effects}=>{let mut value=input;for e in effects{value=value_effect(e,level,value)?;}value},
  ValueEffect::Exponential{base,exponent}=>((input as f64)*(base.calculate(level) as f64).powf(exponent.calculate(level) as f64))as f32,
  ValueEffect::RemoveBinomial{..}=>return None,
 };result.is_finite().then_some(result)
}
fn charge_ticks(seconds:f32)->u32{if !seconds.is_finite(){return 0;}((seconds*20.).ceil() as i32).max(0) as u32}
pub fn crossbow_charge_duration(world:&World,local:Entity,stack:Option<&ItemStack>)->u32{
 use azalea_core::registry_holder::components::CrossbowChargeTime;
 let Some(ItemStack::Present(item))=stack else{return 0;};if item.kind!=azalea_registry::builtin::ItemKind::Crossbow{return 0;}
 let Some(enchantments)=item.get_component::<Enchantments>()else{return 25;};if enchantments.levels.is_empty(){return 25;}
 let Some(holder)=world.get::<azalea_client::local_player::InstanceHolder>(local)else{return 0;};let instance=holder.instance.read();let mut seconds=1.25f32;
 let mut entries=enchantments.levels.iter().collect::<Vec<_>>();entries.sort_by_key(|(enchantment,_)|enchantment.protocol_id());
 for (enchantment,level) in entries {if *level<=0{continue;}let Some((_,data))=instance.registries.enchantment.map.get_index(enchantment.protocol_id() as usize)else{return 0;};if let Some(effects)=data.get::<CrossbowChargeTime>(){for effect in effects{let Some(result)=value_effect(&effect.0,*level,seconds)else{return 0;};seconds=result;}}}
 charge_ticks(seconds)
}
#[cfg(test)]mod crossbow_tests{
 use super::*;use azalea_core::registry_holder::value::{ValueEffect,LevelBasedValue};
 #[test]fn native_crossbow_official_charge_ceiling_and_registry_value_effects(){
  let quick=ValueEffect::Add{value:LevelBasedValue::Linear{base:-0.25,per_level_above_first:-0.25}};
  for (level,ticks) in [(1,20),(2,15),(3,10),(4,5),(5,0)]{assert_eq!(charge_ticks(value_effect(&quick,level,1.25).unwrap()),ticks);}
  assert_eq!(charge_ticks(1.251),26);assert_eq!(charge_ticks(-1.),0);
  let effect=ValueEffect::AllOf{effects:vec![ValueEffect::Set{value:LevelBasedValue::Constant(0.7)},ValueEffect::Multiply{factor:LevelBasedValue::Constant(0.5)}]};assert_eq!(charge_ticks(value_effect(&effect,1,1.25).unwrap()),7);
  assert_eq!(charge_ticks(value_effect(&ValueEffect::Exponential{base:LevelBasedValue::Constant(2.),exponent:LevelBasedValue::Constant(2.)},1,1.25).unwrap()),100);
  assert!(value_effect(&ValueEffect::RemoveBinomial{chance:LevelBasedValue::Constant(0.2)},1,1.25).is_none());
  let world=World::new();assert_eq!(crossbow_charge_duration(&world,Entity::PLACEHOLDER,Some(&ItemStack::new(azalea_registry::builtin::ItemKind::Crossbow,1))),25);
 }
 #[test]fn native_crossbow_authoritative_charged_projectile_and_animation_snapshot(){
  use azalea_registry::builtin::ItemKind;use azalea_inventory::components::ChargedProjectiles;
  let base=ItemStack::new(ItemKind::Crossbow,1);for (projectiles,kind) in [(vec![],"none"),(vec![ItemStack::new(ItemKind::Arrow,1)],"arrow"),(vec![ItemStack::new(ItemKind::Arrow,1),ItemStack::new(ItemKind::FireworkRocket,1)],"rocket")]{let item=base.clone().with_component(ChargedProjectiles{items:projectiles});let snapshot=super::super::native_java_items::snapshot(&item).unwrap();assert_eq!(snapshot.charged_projectile,kind);assert_eq!(serde_json::to_value(snapshot).unwrap()["charged_projectile"],kind);}
 }

 #[test]fn native_crossbow_actual_server_registry_effect_resolves_protocol_id(){
  use azalea_buf::{AzaleaRead,AzaleaWrite};use azalea_registry::{identifier::Identifier,data::Enchantment};use std::{io::Cursor,sync::Arc,collections::HashMap};use azalea_inventory::components::Enchantments;
  fn text(bytes:&mut Vec<u8>,s:&str){bytes.extend_from_slice(&(s.len() as u16).to_be_bytes());bytes.extend_from_slice(s.as_bytes());}
  let mut bytes=Vec::new();Identifier::new("minecraft:enchantment").azalea_write(&mut bytes).unwrap();bytes.push(1);Identifier::new("test:actual_charge").azalea_write(&mut bytes).unwrap();bytes.push(1);bytes.push(10);bytes.push(10);text(&mut bytes,"effects");bytes.push(10);text(&mut bytes,"minecraft:crossbow_charge_time");bytes.push(8);text(&mut bytes,"type");text(&mut bytes,"minecraft:add");bytes.push(5);text(&mut bytes,"value");bytes.extend_from_slice(&(-0.25f32).to_be_bytes());bytes.extend_from_slice(&[0,0,0]);
  let packet=azalea_protocol::packets::config::ClientboundRegistryData::azalea_read(&mut Cursor::new(&bytes)).unwrap();let mut instance=azalea_world::Instance::from(azalea_world::ChunkStorage::default());instance.registries.append(packet.registry_id,packet.entries);assert_eq!(instance.registries.enchantment.map.len(),1);
  let mut world=World::new();let e=world.spawn(azalea_client::local_player::InstanceHolder{instance:Arc::new(parking_lot::RwLock::new(instance)),partial_instance:Arc::new(parking_lot::RwLock::new(azalea_world::PartialInstance::new(2,None)))}).id();let item=ItemStack::new(azalea_registry::builtin::ItemKind::Crossbow,1).with_component(Enchantments{levels:HashMap::from([(Enchantment::new_raw(0),3)])});assert_eq!(crossbow_charge_duration(&world,e,Some(&item)),20);
  let unresolved=ItemStack::new(azalea_registry::builtin::ItemKind::Crossbow,1).with_component(Enchantments{levels:HashMap::from([(Enchantment::new_raw(1),1)])});assert_eq!(crossbow_charge_duration(&world,e,Some(&unresolved)),0);
 }

 #[test]fn native_crossbow_and_bow_subclass_use_precede_injected_consumable(){
  use azalea_registry::builtin::ItemKind;use azalea_inventory::components::Consumable;use azalea_client::interact::native_use::{Context,Outcome,item_outcome};
  let food=ItemStack::new(ItemKind::Apple,1).get_component::<Consumable>().unwrap().into_owned();let inv=Inventory::default();for kind in [ItemKind::Bow,ItemKind::Crossbow]{let item=ItemStack::new(kind,1).with_component(food.clone());assert_eq!(item_outcome(&item,&inv,Context{food:20,..Default::default()}),Outcome::Fail);assert_eq!(item_outcome(&item,&inv,Context{food:20,creative:true,..Default::default()}),Outcome::Consume{duration:72000});}
 }

}
#[cfg(test)]mod entity_fallback_tests{
 use super::*;use azalea_registry::builtin::{ItemKind,EntityKind};
 use azalea_client::{interact::{EntityInteractEvent,BlockStatePredictionHandler,pick::HitResultComponent},packet::game::SendGamePacketEvent};
 use azalea_protocol::packets::game::{ServerboundGamePacket,s_interact::ActionType};use bevy_ecs::prelude::*;use std::sync::{Arc,Mutex};
 fn actual(kind:EntityKind,main:ItemKind)->(Vec<ServerboundGamePacket>,Option<InteractionHand>){actual_health(kind,main,None)}
 fn actual_health(kind:EntityKind,main:ItemKind,health:Option<(f32,Option<f32>,bool)>)->(Vec<ServerboundGamePacket>,Option<InteractionHand>){
  let mut world=World::new();let packets=Arc::new(Mutex::new(Vec::new()));let captured=packets.clone();world.add_observer(move |event:On<SendGamePacketEvent>|{captured.lock().unwrap().push(event.packet.clone());});world.add_observer(azalea_client::interact::handle_entity_interact);
  let target=world.spawn((azalea_entity::Position::new(azalea_core::position::Vec3::new(1.,0.,0.)),azalea_entity::EntityKindComponent(kind))).id();let mut index=azalea_entity::indexing::EntityIdIndex::default();index.insert(azalea_world::MinecraftEntityId(42),target);let mut inv=Inventory::default();*inv.inventory_menu.slot_mut(36).unwrap()=ItemStack::new(main,1);*inv.inventory_menu.slot_mut(45).unwrap()=ItemStack::new(ItemKind::Shield,1);
  let hit=azalea_core::hit_result::HitResult::Block(azalea_core::hit_result::BlockHitResult{location:Default::default(),direction:azalea_core::direction::Direction::Up,block_pos:Default::default(),inside:false,world_border:false,miss:true});let e=world.spawn((azalea_client::PhysicsState::default(),index,HitResultComponent::from(hit),azalea_entity::LookDirection::default(),BlockStatePredictionHandler::default(),inv)).id();if let Some((health,max,foreign))=health{world.entity_mut(target).insert(azalea_entity::metadata::Health(health));if let Some(value)=max{world.entity_mut(target).insert(azalea_client::interact::native_use::NativeTargetMaxHealth{owner:if foreign{target}else{e},value});}}let opts=options(&world,e,true).unwrap();world.entity_mut(e).insert(opts);world.trigger(EntityInteractEvent{client:e,target,hand:InteractionHand::MainHand,location:Some(azalea_core::position::Vec3::new(1.25,0.5,0.25))});world.flush();let result=packets.lock().unwrap().clone();let predicted=world.get::<PredictedNativeUse>(e).map(|p|p.hand);(result,predicted)
 }
 #[test]fn native_actual_default_entity_pass_attempts_both_hands_in_vanilla_order(){
  for kind in [EntityKind::Player,EntityKind::Zombie,EntityKind::Skeleton,EntityKind::Slime,EntityKind::Breeze]{let (packets,hand)=actual(kind,ItemKind::DiamondSword);assert_eq!(hand,Some(InteractionHand::OffHand));assert_eq!(packets.len(),6);for (index,h) in [InteractionHand::MainHand,InteractionHand::OffHand].into_iter().enumerate(){match &packets[index*3]{ServerboundGamePacket::Interact(p)=>{assert_eq!(p.entity_id.0,42);assert!(matches!(p.action,ActionType::InteractAt{hand,..} if hand==h));},_=>panic!("wronginteractAt")};match &packets[index*3+1]{ServerboundGamePacket::Interact(p)=>assert_eq!(p.action,ActionType::Interact{hand:h}),_=>panic!("wronginteract")};match &packets[index*3+2]{ServerboundGamePacket::UseItem(p)=>assert_eq!(p.hand,h),_=>panic!("wronguse")}}}
 }
 #[test]fn native_actual_iron_golem_non_ingot_pass_retains_item_consuming_exclusions(){
  let (packets,hand)=actual(EntityKind::IronGolem,ItemKind::DiamondSword);assert_eq!(packets.len(),6);assert_eq!(hand,Some(InteractionHand::OffHand));
  let (packets,hand)=actual(EntityKind::IronGolem,ItemKind::Shield);assert_eq!(packets.len(),3);assert_eq!(hand,Some(InteractionHand::MainHand));assert!(matches!(packets[2],ServerboundGamePacket::UseItem(_)));
  for item in [ItemKind::IronIngot,ItemKind::NameTag,ItemKind::IronGolemSpawnEgg,ItemKind::RedDye]{let (packets,hand)=actual(EntityKind::IronGolem,item);assert_eq!(packets.len(),2);assert!(hand.is_none());assert!(packets.iter().all(|p|matches!(p,ServerboundGamePacket::Interact(_))));}
 }
 #[test]fn native_actual_iron_golem_ingot_requires_known_current_max_health_for_pass(){
  for health in [0.,100.]{let (packets,hand)=actual_health(EntityKind::IronGolem,ItemKind::IronIngot,Some((health,Some(100.),false)));assert_eq!(packets.len(),6);assert_eq!(hand,Some(InteractionHand::OffHand));}
  for state in [None,Some((101.,Some(100.),false)),Some((99.,Some(100.),false)),Some((100.,None,false)),Some((100.,Some(f32::NAN),false)),Some((f32::NAN,Some(100.),false)),Some((100.,Some(0.),false)),Some((100.,Some(100.),true))]{let (packets,hand)=actual_health(EntityKind::IronGolem,ItemKind::IronIngot,state);assert_eq!(packets.len(),2);assert!(hand.is_none());assert!(packets.iter().all(|p|matches!(p,ServerboundGamePacket::Interact(_))));}
 }
 #[test]fn native_actual_consuming_entity_and_item_paths_do_not_blindly_use_items(){
  for kind in [EntityKind::Villager,EntityKind::Bee,EntityKind::Allay,EntityKind::ArmorStand]{let (packets,hand)=actual(kind,ItemKind::DiamondSword);assert_eq!(packets.len(),2);assert!(hand.is_none());assert!(packets.iter().all(|p|matches!(p,ServerboundGamePacket::Interact(_))));}
  for item in [ItemKind::NameTag,ItemKind::ZombieSpawnEgg,ItemKind::RedDye]{let (packets,hand)=actual(EntityKind::Zombie,item);assert_eq!(packets.len(),2);assert!(hand.is_none());}
 }
}
#[cfg(test)]mod crossbow_continuation_tests{
 use super::*;use azalea_registry::builtin::ItemKind;use azalea_inventory::components::ChargedProjectiles;
 use azalea_client::{interact::{StartUseItemQueued,BlockStatePredictionHandler,pick::HitResultComponent},packet::game::{GamePacketHandler,SendGamePacketEvent}};
 use azalea_protocol::packets::game::{ServerboundGamePacket,ClientboundContainerSetSlot};use bevy_ecs::{prelude::*,system::RunSystemOnce};use std::sync::{Arc,Mutex};use azalea_buf::{AzaleaRead,AzaleaWrite};
 #[test]fn native_crossbow_server_loaded_stack_stays_using_until_release_without_rapidfire(){
  for hand in [InteractionHand::MainHand,InteractionHand::OffHand]{
   let mut world=World::new();let packets=Arc::new(Mutex::new(Vec::<ServerboundGamePacket>::new()));let captured=packets.clone();world.add_observer(move |event:On<SendGamePacketEvent>|{captured.lock().unwrap().push(event.packet.clone());});
   let mut inv=Inventory::default();let slot=if hand==InteractionHand::OffHand{45}else{36};let crossbow=ItemStack::new(ItemKind::Crossbow,1);*inv.inventory_menu.slot_mut(slot).unwrap()=crossbow.clone();*inv.inventory_menu.slot_mut(9).unwrap()=ItemStack::new(ItemKind::Arrow,64);
   let hit=azalea_core::hit_result::HitResult::Block(azalea_core::hit_result::BlockHitResult{location:Default::default(),direction:azalea_core::direction::Direction::Up,block_pos:Default::default(),inside:false,world_border:false,miss:true});let e=world.spawn((inv,HitResultComponent::from(hit),azalea_entity::LookDirection::default(),BlockStatePredictionHandler::default())).id();let opts=options(&world,e,false).unwrap();world.entity_mut(e).insert((opts,StartUseItemQueued{hand,force_block:None}));world.run_system_once(azalea_client::interact::handle_start_use_item_queued).unwrap();world.flush();assert_eq!(packets.lock().unwrap().len(),1);assert!(busy(&world,e));
   for _ in 0..25{tick(&mut world,e);}
   let charged=crossbow.with_component(ChargedProjectiles{items:vec![ItemStack::new(ItemKind::Arrow,1)]});let authoritative=ClientboundContainerSetSlot{container_id:0,state_id:2,slot:slot as u16,item_stack:charged.clone()};let mut encoded=Vec::new();authoritative.azalea_write(&mut encoded).unwrap();let decoded=ClientboundContainerSetSlot::azalea_read(&mut std::io::Cursor::new(encoded.as_slice())).unwrap();GamePacketHandler{ecs:&mut world,player:e}.container_set_slot(&decoded);
   tick(&mut world,e);assert!(busy(&world,e));assert_eq!(world.get::<PredictedNativeUse>(e).unwrap().stack,charged);assert!(world.get::<azalea_entity::metadata::AbstractLivingUsingItem>(e).unwrap().0);
   for _ in 0..100{let opts=options(&world,e,false).unwrap();world.entity_mut(e).insert((opts,StartUseItemQueued{hand,force_block:None}));world.run_system_once(azalea_client::interact::handle_start_use_item_queued).unwrap();world.flush();tick(&mut world,e);assert!(busy(&world,e));}
   assert_eq!(packets.lock().unwrap().len(),1,"held Use must never shoot the server-loaded crossbow");assert_eq!(world.get::<Inventory>(e).unwrap().inventory_menu.slot(9),Some(&ItemStack::new(ItemKind::Arrow,64)),"client does not consume ammunition");
   super::super::native_java::clear_native_item_use(&mut world,e);assert!(!busy(&world,e));let opts=options(&world,e,false).unwrap();world.entity_mut(e).insert((opts,StartUseItemQueued{hand,force_block:None}));world.run_system_once(azalea_client::interact::handle_start_use_item_queued).unwrap();world.flush();assert_eq!(packets.lock().unwrap().len(),2,"new press may shoot the loaded crossbow exactly once");assert!(!busy(&world,e));
  }
 }
 #[test]fn native_use_pending_queue_blocks_duplicate_before_first_prediction_tick(){
  let mut world=World::new();let e=world.spawn(StartUseItemQueued{hand:InteractionHand::MainHand,force_block:None}).id();assert!(busy(&world,e));world.entity_mut(e).remove::<StartUseItemQueued>();assert!(!busy(&world,e));
 }
}

#[cfg(test)]mod target_health_lifecycle_tests{
 use super::*;use azalea_entity::{LoadedBy,indexing::EntityIdIndex};use azalea_client::interact::native_use::NativeTargetMaxHealth;
 #[test]fn native_target_health_scopes_invalidates_and_does_not_reuse_despawned_entity(){
  let mut world=World::new();let local=world.spawn_empty().id();let foreign=world.spawn_empty().id();let target=world.spawn(LoadedBy(Default::default())).id();let mut index=EntityIdIndex::default();index.insert(azalea_world::MinecraftEntityId(42),target);world.entity_mut(local).insert(index);
  target_max_health(&mut world,local,42,Some(100.));assert!(world.get::<NativeTargetMaxHealth>(target).is_none());world.get_mut::<LoadedBy>(target).unwrap().0.insert(local);
  target_max_health(&mut world,local,42,Some(100.));assert_eq!(world.get::<NativeTargetMaxHealth>(target).unwrap().value,100.);
  target_max_health(&mut world,local,42,Some(f32::NAN));assert!(world.get::<NativeTargetMaxHealth>(target).is_none());target_max_health(&mut world,local,42,Some(200.));clear_target_max_health(&mut world,foreign);assert!(world.get::<NativeTargetMaxHealth>(target).is_some());clear_target_max_health(&mut world,local);assert!(world.get::<NativeTargetMaxHealth>(target).is_none());
  target_max_health(&mut world,local,42,Some(200.));world.despawn(target);let replacement=world.spawn(LoadedBy(Default::default())).id();world.get_mut::<LoadedBy>(replacement).unwrap().0.insert(local);world.get_mut::<EntityIdIndex>(local).unwrap().insert(azalea_world::MinecraftEntityId(42),replacement);assert!(world.get::<NativeTargetMaxHealth>(replacement).is_none());target_max_health(&mut world,local,42,None);assert!(world.get::<NativeTargetMaxHealth>(replacement).is_none());
 }
}
